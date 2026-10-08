#include "CoverCombatObserver.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverCombatScoring.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverObserve, Log, All);

namespace CoverCombatObserverPrivate
{
	constexpr int32 StateCount = static_cast<int32>(ECoverObservedState::Count);
	constexpr float SampleLogIntervalSeconds = 10.0f;
	// Slower than this, a soldier counts as standing still.
	constexpr float StationarySpeed = 30.0f;
	constexpr int32 PlayerOwnedTeam = 1;
	// Heights above the ground: a rifle at the shoulder, the chest of a standing man, and of a crouching one.
	constexpr float MuzzleHeight = 150.0f;
	constexpr float StandingChestHeight = 120.0f;
	constexpr float CrouchedChestHeight = 70.0f;
	// The soldier's own capsule ends the trace; a hit this close to him is him, not cover.
	constexpr float HitOnTargetDistance = 60.0f;

	const TCHAR* GetStateName(const ECoverObservedState State)
	{
		switch (State)
		{
		case ECoverObservedState::CoverProtected:
			return TEXT("cover_protected");
		case ECoverObservedState::CoverExposed:
			return TEXT("cover_exposed");
		case ECoverObservedState::WalkingToCover:
			return TEXT("walking_to_cover");
		case ECoverObservedState::OpenStationary:
			return TEXT("open_stationary");
		case ECoverObservedState::OpenMoving:
			return TEXT("open_moving");
		default:
			return TEXT("unknown");
		}
	}

	bool GetIsInCover(const ECoverObservedState State)
	{
		return State == ECoverObservedState::CoverProtected || State == ECoverObservedState::CoverExposed;
	}

	FString DescribePerState(const float Values[StateCount])
	{
		FString Description;
		for (int32 StateIndex = 0; StateIndex < StateCount; ++StateIndex)
		{
			Description += FString::Printf(
				TEXT("%s=%.0f "),
				GetStateName(static_cast<ECoverObservedState>(StateIndex)),
				Values[StateIndex]);
		}
		return Description.TrimEnd();
	}
}

void FCoverCombatObserver::Reset()
{
	M_Units.Reset();
	M_SideTotals.Reset();
	M_NextSampleLogSeconds = 0.0f;
}

void FCoverCombatObserver::GatherAttackers(
	UWorld& World,
	TMap<const ASquadUnit*, TArray<FVector>>& OutAttackerLocations) const
{
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* Attacker = *UnitIterator;
		AInfantryWeaponMaster* AttackerWeapon = IsValid(Attacker) && Attacker->IsUnitAlive()
			? Attacker->GetInfantryWeapon()
			: nullptr;
		if (not IsValid(AttackerWeapon) || not AttackerWeapon->GetIsCurrentTargetInRange())
		{
			continue;
		}
		const ASquadUnit* TargetUnit = Cast<ASquadUnit>(AttackerWeapon->GetCurrentTargetActor());
		if (IsValid(TargetUnit))
		{
			OutAttackerLocations.FindOrAdd(TargetUnit).Add(Attacker->GetActorLocation());
		}
	}
}

ECoverObservedState FCoverCombatObserver::ClassifyUnit(const ASquadUnit& SquadUnit) const
{
	const FSquadUnitCoverRuntimeState& CoverState = SquadUnit.GetCoverRuntimeState();
	if (SquadUnit.GetIsOccupyingCover())
	{
		return CoverState.State == ESquadUnitCoverState::Exposed
			? ECoverObservedState::CoverExposed
			: ECoverObservedState::CoverProtected;
	}
	if (SquadUnit.GetHasCoverAssignment())
	{
		return ECoverObservedState::WalkingToCover;
	}
	return SquadUnit.GetVelocity().SizeSquared2D() > FMath::Square(CoverCombatObserverPrivate::StationarySpeed)
		? ECoverObservedState::OpenMoving
		: ECoverObservedState::OpenStationary;
}

void FCoverCombatObserver::Tick(
	UWorld& World,
	const URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const float DeltaTime,
	const float ElapsedSeconds)
{
	TMap<const ASquadUnit*, TArray<FVector>> AttackerLocations;
	GatherAttackers(World, AttackerLocations);
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		if (IsValid(SquadUnit) && SquadUnit->IsUnitAlive())
		{
			ObserveUnit(*SquadUnit, AttackerLocations.Find(SquadUnit), CoverSubsystem, DeltaTime, ElapsedSeconds);
		}
	}
	RecordDeaths();
	if (ElapsedSeconds >= M_NextSampleLogSeconds)
	{
		M_NextSampleLogSeconds = ElapsedSeconds + CoverCombatObserverPrivate::SampleLogIntervalSeconds;
		LogSample(ElapsedSeconds);
	}
}

void FCoverCombatObserver::ObserveUnit(
	ASquadUnit& SquadUnit,
	const TArray<FVector>* AttackerLocations,
	const URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const float DeltaTime,
	const float ElapsedSeconds)
{
	using namespace CoverCombatObserverPrivate;
	const UHealthComponent* UnitHealth = SquadUnit.GetHealthComponent();
	const float CurrentHealth = IsValid(UnitHealth) ? UnitHealth->GetCurrentHealth() : 0.0f;
	const ECoverObservedState State = ClassifyUnit(SquadUnit);
	const int32 StateIndex = static_cast<int32>(State);
	const bool bIsUnderFire = AttackerLocations != nullptr && not AttackerLocations->IsEmpty();
	FCoverObservedUnit* KnownUnit = M_Units.Find(&SquadUnit);
	if (KnownUnit == nullptr)
	{
		FCoverObservedUnit& NewUnit = M_Units.Add(&SquadUnit);
		NewUnit.LastHealth = CurrentHealth;
		NewUnit.OwningPlayer = SquadUnit.GetOwningPlayer();
		NewUnit.LastState = State;
		NewUnit.bWasUnderFire = bIsUnderFire;
		return;
	}
	FCoverObservedSideTotals& SideTotals = M_SideTotals.FindOrAdd(KnownUnit->OwningPlayer);
	(bIsUnderFire ? SideTotals.UnderFireSeconds : SideTotals.NotUnderFireSeconds)[StateIndex] += DeltaTime;
	// Damage is charged to the state the soldier was in when it arrived.
	SideTotals.DamageTaken[static_cast<int32>(KnownUnit->LastState)] += FMath::Max(0.0f, KnownUnit->LastHealth - CurrentHealth);

	const bool bIsInOpen = not GetIsInCover(State);
	if (bIsUnderFire)
	{
		AccumulateLinesOfFire(SquadUnit, State, *AttackerLocations, DeltaTime, SideTotals);
	}
	if (bIsUnderFire && GetIsInCover(State))
	{
		AccumulateShielding(SquadUnit, *AttackerLocations, CoverSubsystem, DeltaTime, SideTotals);
	}
	if (bIsUnderFire && State == ECoverObservedState::OpenStationary)
	{
		const bool bHasUsableCover = GetHasUsableCoverNearby(SquadUnit, (*AttackerLocations)[0], CoverSubsystem);
		(bHasUsableCover ? SideTotals.OpenWithUsableCoverNearbySeconds : SideTotals.OpenWithoutUsableCoverSeconds) += DeltaTime;
	}
	if (bIsUnderFire && bIsInOpen && KnownUnit->UnderFireInOpenSinceSeconds < 0.0f)
	{
		KnownUnit->UnderFireInOpenSinceSeconds = ElapsedSeconds;
	}
	const bool bEnteredCover = GetIsInCover(State) && not GetIsInCover(KnownUnit->LastState);
	const bool bLeftCover = not GetIsInCover(State) && GetIsInCover(KnownUnit->LastState);
	SideTotals.CoverEnteredCount += bEnteredCover ? 1 : 0;
	SideTotals.CoverLeftCount += bLeftCover ? 1 : 0;
	if (bEnteredCover && KnownUnit->UnderFireInOpenSinceSeconds >= 0.0f)
	{
		SideTotals.SecondsToCoverSum += ElapsedSeconds - KnownUnit->UnderFireInOpenSinceSeconds;
		++SideTotals.SecondsToCoverSamples;
	}
	if (GetIsInCover(State) || not bIsUnderFire)
	{
		KnownUnit->UnderFireInOpenSinceSeconds = -1.0f;
	}
	if (State != KnownUnit->LastState)
	{
		UE_LOG(
			LogRTSCoverObserve,
			Verbose,
			TEXT("RTS_COVER_OBSERVE transition t=%.1f unit=%s owner=%d %s -> %s under_fire=%d attackers=%d command=%s health=%.0f"),
			ElapsedSeconds,
			*SquadUnit.GetName(),
			KnownUnit->OwningPlayer,
			GetStateName(KnownUnit->LastState),
			GetStateName(State),
			bIsUnderFire ? 1 : 0,
			AttackerLocations != nullptr ? AttackerLocations->Num() : 0,
			*UEnum::GetValueAsString(SquadUnit.GetActiveCommand()),
			CurrentHealth);
	}
	KnownUnit->LastHealth = CurrentHealth;
	KnownUnit->LastState = State;
	KnownUnit->bWasUnderFire = bIsUnderFire;
}

void FCoverCombatObserver::AccumulateShielding(
	const ASquadUnit& SquadUnit,
	const TArray<FVector>& AttackerLocations,
	const URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const float DeltaTime,
	FCoverObservedSideTotals& InOutTotals) const
{
	FRTSCombatCoverThreats Threats;
	Threats.PrimaryTargetLocation = AttackerLocations[0];
	Threats.ThreatLocations.Append(AttackerLocations);
	const float ShieldedFraction = FRTSCombatCoverScoring::GetProtectedThreatFraction(
		SquadUnit.GetCoverRuntimeState().AssignedCoverPoint,
		Threats,
		CoverSubsystem.BuildCombatCoverSettings());
	if (ShieldedFraction >= 1.0f)
	{
		InOutTotals.ShieldedFromAllSeconds += DeltaTime;
		return;
	}
	(ShieldedFraction > 0.0f ? InOutTotals.ShieldedFromSomeSeconds : InOutTotals.ShieldedFromNoneSeconds) += DeltaTime;
}

void FCoverCombatObserver::AccumulateLinesOfFire(
	const ASquadUnit& SquadUnit,
	const ECoverObservedState State,
	const TArray<FVector>& AttackerLocations,
	const float DeltaTime,
	FCoverObservedSideTotals& InOutTotals) const
{
	using namespace CoverCombatObserverPrivate;
	const UWorld* World = SquadUnit.GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// The channel the attackers' weapons trace on: the one this soldier's side blocks.
	const ECollisionChannel WeaponChannel = SquadUnit.GetOwningPlayer() == PlayerOwnedTeam
		? COLLISION_TRACE_PLAYER
		: COLLISION_TRACE_ENEMY;
	const float HalfHeight = SquadUnit.GetSimpleCollisionHalfHeight();
	const float ChestHeight = State == ECoverObservedState::CoverProtected ? CrouchedChestHeight : StandingChestHeight;
	const FVector TargetLocation = SquadUnit.GetActorLocation() + FVector::UpVector * (ChestHeight - HalfHeight);
	const int32 StateIndex = static_cast<int32>(State);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTSCoverObserveLineOfFire), true);
	for (const FVector& AttackerLocation : AttackerLocations)
	{
		const FVector MuzzleLocation = AttackerLocation + FVector::UpVector * (MuzzleHeight - HalfHeight);
		FHitResult Hit;
		const bool bHitSomething = World->LineTraceSingleByChannel(Hit, MuzzleLocation, TargetLocation, WeaponChannel, QueryParams);
		const bool bBlockedBeforeTarget = bHitSomething && Hit.GetActor() != &SquadUnit &&
			FVector::Dist(Hit.ImpactPoint, TargetLocation) > HitOnTargetDistance;
		(bBlockedBeforeTarget ? InOutTotals.LineOfFireBlockedSeconds : InOutTotals.LineOfFireOpenSeconds)[StateIndex] += DeltaTime;
	}
}

bool FCoverCombatObserver::GetHasUsableCoverNearby(
	const ASquadUnit& SquadUnit,
	const FVector& AttackerLocation,
	const URTSCoverFinderWorldSubsystem& CoverSubsystem) const
{
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	if (not IsValid(CoverSettings))
	{
		return false;
	}
	const FRTSCombatCoverSettings CombatSettings = CoverSubsystem.BuildCombatCoverSettings();
	FRTSCombatCoverThreats Threats;
	Threats.PrimaryTargetLocation = AttackerLocation;
	Threats.ThreatLocations.Add(AttackerLocation);
	const TArray<FRTSCoverPoint> NearbyPoints = CoverSubsystem.FindCoverPointsInRadius(
		SquadUnit.GetActorLocation(),
		CoverSettings->M_AutomaticCoverSearchRadius);
	for (const FRTSCoverPoint& CoverPoint : NearbyPoints)
	{
		const ASquadUnit* ReservingUnit = CoverSubsystem.GetCoverReservationOwner(CoverPoint.PointId);
		const bool bIsFree = not IsValid(ReservingUnit) || ReservingUnit == &SquadUnit;
		if (bIsFree && FRTSCombatCoverScoring::GetProtectedThreatFraction(CoverPoint, Threats, CombatSettings) > 0.0f)
		{
			return true;
		}
	}
	return false;
}

void FCoverCombatObserver::RecordDeaths()
{
	for (auto UnitIterator = M_Units.CreateIterator(); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = UnitIterator.Key().Get();
		if (IsValid(SquadUnit) && SquadUnit->IsUnitAlive())
		{
			continue;
		}
		FCoverObservedSideTotals& SideTotals = M_SideTotals.FindOrAdd(UnitIterator.Value().OwningPlayer);
		++SideTotals.Deaths[static_cast<int32>(UnitIterator.Value().LastState)];
		SideTotals.DamageTaken[static_cast<int32>(UnitIterator.Value().LastState)] += UnitIterator.Value().LastHealth;
		UnitIterator.RemoveCurrent();
	}
}

void FCoverCombatObserver::LogSample(const float ElapsedSeconds) const
{
	TMap<int32, TArray<int32>> UnitsPerState;
	TMap<int32, int32> UnderFirePerSide;
	for (const TPair<TWeakObjectPtr<ASquadUnit>, FCoverObservedUnit>& ObservedUnit : M_Units)
	{
		TArray<int32>& StateCounts = UnitsPerState.FindOrAdd(ObservedUnit.Value.OwningPlayer);
		StateCounts.SetNumZeroed(CoverCombatObserverPrivate::StateCount);
		++StateCounts[static_cast<int32>(ObservedUnit.Value.LastState)];
		UnderFirePerSide.FindOrAdd(ObservedUnit.Value.OwningPlayer) += ObservedUnit.Value.bWasUnderFire ? 1 : 0;
	}
	for (const TPair<int32, TArray<int32>>& SideStates : UnitsPerState)
	{
		FString Description;
		for (int32 StateIndex = 0; StateIndex < SideStates.Value.Num(); ++StateIndex)
		{
			Description += FString::Printf(
				TEXT("%s=%d "),
				CoverCombatObserverPrivate::GetStateName(static_cast<ECoverObservedState>(StateIndex)),
				SideStates.Value[StateIndex]);
		}
		UE_LOG(
			LogRTSCoverObserve,
			Display,
			TEXT("RTS_COVER_OBSERVE sample t=%.0f owner=%d under_fire=%d %s"),
			ElapsedSeconds,
			SideStates.Key,
			UnderFirePerSide.FindRef(SideStates.Key),
			*Description);
	}
}

void FCoverCombatObserver::LogSummary() const
{
	using namespace CoverCombatObserverPrivate;
	for (const TPair<int32, FCoverObservedSideTotals>& Side : M_SideTotals)
	{
		const FCoverObservedSideTotals& Totals = Side.Value;
		float DeathsAsFloat[StateCount];
		for (int32 StateIndex = 0; StateIndex < StateCount; ++StateIndex)
		{
			DeathsAsFloat[StateIndex] = static_cast<float>(Totals.Deaths[StateIndex]);
		}
		UE_LOG(LogRTSCoverObserve, Display, TEXT("RTS_COVER_OBSERVE summary owner=%d under_fire_seconds %s"),
			Side.Key, *DescribePerState(Totals.UnderFireSeconds));
		UE_LOG(LogRTSCoverObserve, Display, TEXT("RTS_COVER_OBSERVE summary owner=%d not_under_fire_seconds %s"),
			Side.Key, *DescribePerState(Totals.NotUnderFireSeconds));
		UE_LOG(LogRTSCoverObserve, Display, TEXT("RTS_COVER_OBSERVE summary owner=%d damage_taken %s"),
			Side.Key, *DescribePerState(Totals.DamageTaken));
		UE_LOG(LogRTSCoverObserve, Display, TEXT("RTS_COVER_OBSERVE summary owner=%d deaths %s"),
			Side.Key, *DescribePerState(DeathsAsFloat));
		UE_LOG(LogRTSCoverObserve, Display, TEXT("RTS_COVER_OBSERVE summary owner=%d line_of_fire_blocked_seconds %s"),
			Side.Key, *DescribePerState(Totals.LineOfFireBlockedSeconds));
		UE_LOG(LogRTSCoverObserve, Display, TEXT("RTS_COVER_OBSERVE summary owner=%d line_of_fire_open_seconds %s"),
			Side.Key, *DescribePerState(Totals.LineOfFireOpenSeconds));
		UE_LOG(
			LogRTSCoverObserve,
			Display,
			TEXT("RTS_COVER_OBSERVE summary owner=%d in_cover_under_fire_seconds shielded_from_all=%.0f shielded_from_some=%.0f shielded_from_none=%.0f | open_stationary_under_fire_seconds usable_cover_nearby=%.0f no_usable_cover=%.0f | cover_entered=%d cover_left=%d average_seconds_to_cover=%.1f (%d samples)"),
			Side.Key,
			Totals.ShieldedFromAllSeconds,
			Totals.ShieldedFromSomeSeconds,
			Totals.ShieldedFromNoneSeconds,
			Totals.OpenWithUsableCoverNearbySeconds,
			Totals.OpenWithoutUsableCoverSeconds,
			Totals.CoverEnteredCount,
			Totals.CoverLeftCount,
			Totals.SecondsToCoverSamples > 0 ? Totals.SecondsToCoverSum / static_cast<float>(Totals.SecondsToCoverSamples) : 0.0f,
			Totals.SecondsToCoverSamples);
	}
}
