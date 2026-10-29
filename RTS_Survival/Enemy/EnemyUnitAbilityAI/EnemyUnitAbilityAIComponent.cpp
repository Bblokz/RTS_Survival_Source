// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyUnitAbilityAIComponent.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "EnemyAbilityUsageProfiles.h"
#include "EnemyUnitAbilityAISettings.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyRetreatController/EnemyRetreatController.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeComponent.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

namespace EnemyUnitAbilityAIConstants
{
	constexpr uint8 HumanPlayerIndex = 1;
	constexpr uint8 EnemyPlayerIndex = 2;
	// The aim point is moved to the middle of the soldiers within this many blast radii of the target.
	constexpr float InfantryClusterSearchRadiusMultiplier = 2.f;
	constexpr float DebugTextDuration = 3.f;
	constexpr float DebugTextHeight = 350.f;
}

UEnemyUnitAbilityAIComponent::UEnemyUnitAbilityAIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyUnitAbilityAIComponent::BeginPlay()
{
	Super::BeginPlay();
	// Bound at runtime: a constructor-time reference on a placed controller would point at the class default object.
	M_EnemyController = Cast<AEnemyController>(GetOwner());
	BeginPlay_StartEvaluationTimer();
}

void UEnemyUnitAbilityAIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_EvaluationTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UEnemyUnitAbilityAIComponent::BeginPlay_StartEvaluationTimer()
{
	const UEnemyUnitAbilityAISettings* Settings = GetDefault<UEnemyUnitAbilityAISettings>();
	UWorld* World = GetWorld();
	if (not Settings || not Settings->bEnabled || not IsValid(World))
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		M_EvaluationTimerHandle,
		this,
		&UEnemyUnitAbilityAIComponent::EvaluationTick,
		Settings->EvaluationInterval,
		true);
}

bool UEnemyUnitAbilityAIComponent::GetIsValidEnemyController() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_EnemyController",
		"UEnemyUnitAbilityAIComponent::GetIsValidEnemyController",
		this);
	return false;
}

void UEnemyUnitAbilityAIComponent::EvaluationTick()
{
	TRACE_CPUPROFILER_EVENT_SCOPE("EnemyUnitAbilityAI_Tick")
	const UEnemyUnitAbilityAISettings* Settings = GetDefault<UEnemyUnitAbilityAISettings>();
	UWorld* World = GetWorld();
	if (not Settings || not Settings->bEnabled || not IsValid(World))
	{
		return;
	}
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	if (not TryGetMissionDifficulty(Difficulty))
	{
		return;
	}
	const bool bSquadsAllowed = GetIsDifficultyAllowed(*Settings, true, Difficulty);
	const bool bTanksAllowed = GetIsDifficultyAllowed(*Settings, false, Difficulty);
	if (not bSquadsAllowed && not bTanksAllowed)
	{
		return;
	}
	const float NowSeconds = World->GetTimeSeconds();
	PrepareTick(NowSeconds);
	if (NowSeconds >= M_NextRosterRefreshSeconds)
	{
		M_NextRosterRefreshSeconds = NowSeconds + Settings->RosterRefreshInterval;
		RefreshRoster();
	}
	const int32 UnitsToEvaluate = FMath::Min(Settings->UnitsEvaluatedPerTick, M_Roster.Num());
	for (int32 EvaluatedCount = 0; EvaluatedCount < UnitsToEvaluate; ++EvaluatedCount)
	{
		M_NextRosterIndex = M_NextRosterIndex % M_Roster.Num();
		EvaluateUnit(M_Roster[M_NextRosterIndex], *Settings, Difficulty, NowSeconds);
		++M_NextRosterIndex;
	}
}

bool UEnemyUnitAbilityAIComponent::TryGetMissionDifficulty(ERTSGameDifficulty& OutDifficulty)
{
	if (not M_MissionManager.IsValid())
	{
		UWorld* World = GetWorld();
		const bool bIsRosterRefreshDue = IsValid(World) && World->GetTimeSeconds() >= M_NextRosterRefreshSeconds;
		if (not bIsRosterRefreshDue)
		{
			// Searching the level is costly; a map without a mission manager is retried at the roster cadence.
			return false;
		}
		M_MissionManager = Cast<AMissionManager>(UGameplayStatics::GetActorOfClass(this, AMissionManager::StaticClass()));
		if (not M_MissionManager.IsValid())
		{
			M_NextRosterRefreshSeconds = World->GetTimeSeconds() + GetDefault<UEnemyUnitAbilityAISettings>()->
				RosterRefreshInterval;
			return false;
		}
	}
	if (not M_MissionManager->GetIsGameDifficultyInitialized())
	{
		return false;
	}
	OutDifficulty = M_MissionManager->GetCurrentGameDifficulty().DifficultyLevel;
	return true;
}

bool UEnemyUnitAbilityAIComponent::GetIsDifficultyAllowed(const UEnemyUnitAbilityAISettings& Settings,
                                                          const bool bIsSquad, const ERTSGameDifficulty Difficulty)
{
	const ERTSGameDifficulty MinimumDifficulty = bIsSquad
		                                             ? Settings.MinDifficultySquadsUseAbilities
		                                             : Settings.MinDifficultyTanksUseAbilities;
	return Difficulty >= MinimumDifficulty;
}

void UEnemyUnitAbilityAIComponent::PrepareTick(const float NowSeconds)
{
	bM_HasGatheredPlayerSoldiersThisTick = false;
	RemoveExpiredGrenadeClaims(NowSeconds);
	M_DirectControlRetreatUnits.Reset();
	if (not GetIsValidEnemyController())
	{
		return;
	}
	if (const UEnemyDirectControlComponent* DirectControl = M_EnemyController->GetEnemyDirectControlComponent())
	{
		M_DirectControlRetreatUnits = DirectControl->GetRetreatGroupUnitsToExclude();
	}
}

void UEnemyUnitAbilityAIComponent::RefreshRoster()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// Command cards fill in after the units spawn, so units are re-checked on every refresh instead of once.
	TArray<FEnemyAbilityAIUnit> NewRoster;
	for (TActorIterator<ASquadController> SquadIterator(World); SquadIterator; ++SquadIterator)
	{
		AddEnemyUnitToRoster(*SquadIterator, true, NewRoster);
	}
	for (TActorIterator<ATankMaster> TankIterator(World); TankIterator; ++TankIterator)
	{
		AddEnemyUnitToRoster(*TankIterator, false, NewRoster);
	}
	// Keep the per-unit spacing of units that were already known.
	for (FEnemyAbilityAIUnit& NewUnit : NewRoster)
	{
		const FEnemyAbilityAIUnit* OldUnit = M_Roster.FindByPredicate([&NewUnit](const FEnemyAbilityAIUnit& Unit)
		{
			return Unit.UnitActor == NewUnit.UnitActor;
		});
		if (OldUnit)
		{
			NewUnit.M_LastAbilityUseSeconds = OldUnit->M_LastAbilityUseSeconds;
			NewUnit.bM_HasUsedAbility = OldUnit->bM_HasUsedAbility;
		}
	}
	M_Roster = MoveTemp(NewRoster);
}

void UEnemyUnitAbilityAIComponent::AddEnemyUnitToRoster(AActor* UnitActor, const bool bIsSquad,
                                                        TArray<FEnemyAbilityAIUnit>& InOutNewRoster) const
{
	if (not RTSFunctionLibrary::RTSIsValid(UnitActor))
	{
		return;
	}
	const URTSComponent* RTSComponent = UnitActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent) || RTSComponent->GetOwningPlayer() != EnemyUnitAbilityAIConstants::EnemyPlayerIndex)
	{
		return;
	}
	ICommands* Commands = Cast<ICommands>(UnitActor);
	if (not GetHasUsableAbilityOnCard(Commands))
	{
		return;
	}
	FEnemyAbilityAIUnit NewUnit;
	NewUnit.UnitActor = UnitActor;
	NewUnit.bM_IsSquad = bIsSquad;
	InOutNewRoster.Add(NewUnit);
}

bool UEnemyUnitAbilityAIComponent::GetHasUsableAbilityOnCard(ICommands* Commands)
{
	if (not Commands)
	{
		return false;
	}
	for (const FUnitAbilityEntry& Entry : Commands->GetUnitAbilityEntries())
	{
		if (Entry.AbilityId == EAbilityID::IdApplyBehaviour || Entry.AbilityId == EAbilityID::IdThrowGrenade)
		{
			return true;
		}
	}
	return false;
}

void UEnemyUnitAbilityAIComponent::EvaluateUnit(FEnemyAbilityAIUnit& Unit,
                                                const UEnemyUnitAbilityAISettings& Settings,
                                                const ERTSGameDifficulty Difficulty, const float NowSeconds)
{
	if (not GetIsDifficultyAllowed(Settings, Unit.bM_IsSquad, Difficulty))
	{
		return;
	}
	const bool bIsSpacedOut = not Unit.bM_HasUsedAbility
		|| NowSeconds - Unit.M_LastAbilityUseSeconds >= Settings.MinSecondsBetweenAbilityUsesPerUnit;
	if (not bIsSpacedOut)
	{
		return;
	}
	FEnemyAbilityUnitContext Context;
	if (not TryBuildUnitContext(Unit, Context))
	{
		return;
	}
	const TArray<FUnitAbilityEntry> Card = Context.Commands->GetUnitAbilityEntries();
	if (TryUseBehaviourAbility(Context, Card, Settings, true))
	{
		OnBehaviourUsed(Unit, Context, NowSeconds);
		return;
	}
	if (Context.bIsSquad && TryThrowGrenade(Context, Card, Settings, NowSeconds))
	{
		Unit.M_LastAbilityUseSeconds = NowSeconds;
		Unit.bM_HasUsedAbility = true;
		return;
	}
	if (TryUseBehaviourAbility(Context, Card, Settings, false))
	{
		OnBehaviourUsed(Unit, Context, NowSeconds);
	}
}

bool UEnemyUnitAbilityAIComponent::TryBuildUnitContext(const FEnemyAbilityAIUnit& Unit,
                                                       FEnemyAbilityUnitContext& OutContext) const
{
	AActor* UnitActor = Unit.UnitActor.Get();
	if (not RTSFunctionLibrary::RTSIsValid(UnitActor) || not GetIsValidEnemyController())
	{
		return false;
	}
	ICommands* Commands = Cast<ICommands>(UnitActor);
	UCommandData* CommandData = Commands ? Commands->GetIsValidCommandData() : nullptr;
	if (not CommandData)
	{
		return false;
	}
	OutContext.Commands = Commands;
	OutContext.UnitActor = UnitActor;
	OutContext.bIsSquad = Unit.bM_IsSquad;
	OutContext.ActiveCommand = Commands->GetActiveCommandID();
	if (GetIsUnitRetreating(UnitActor, OutContext.ActiveCommand))
	{
		return false;
	}
	OutContext.bIsInCombat = Commands->GetIsUnitInCombat();
	OutContext.bIsMoving = CommandData->GetIsCurrentCommandAMovementCommand();
	const bool bIsIdleOrAttacking = OutContext.ActiveCommand == EAbilityID::IdIdle
		|| OutContext.ActiveCommand == EAbilityID::IdAttack;
	// Moving units only get behaviours, and only in a fight; anything else (a running ability, construction) is
	// left alone.
	if (not bIsIdleOrAttacking && not (OutContext.bIsMoving && OutContext.bIsInCombat))
	{
		return false;
	}
	const UEnemyFormationController* FormationController = M_EnemyController->GetEnemyFormationController();
	const bool bIsWalkingToWaypoint = IsValid(FormationController)
		&& FormationController->GetIsUnitMovingToFormationWaypoint(UnitActor);
	OutContext.bIsInFormation = IsValid(FormationController) && FormationController->GetIsUnitInActiveFormation(UnitActor);
	OutContext.bMayUseQueuedAbilities = bIsIdleOrAttacking && not bIsWalkingToWaypoint;
	OutContext.bIsOnHoldPosition = GetIsOnHoldPosition(UnitActor, Unit.bM_IsSquad);
	AActor* WeaponTarget = FindWeaponTarget(UnitActor, Unit.bM_IsSquad);
	OutContext.bHasWeaponTarget = IsValid(WeaponTarget);
	AActor* AttackTarget = OutContext.ActiveCommand == EAbilityID::IdAttack
		                       ? Commands->GetActiveCommandTargetActor()
		                       : nullptr;
	OutContext.CombatTarget = IsValid(AttackTarget) ? AttackTarget : WeaponTarget;
	return true;
}

bool UEnemyUnitAbilityAIComponent::GetIsUnitRetreating(const AActor* UnitActor, const EAbilityID ActiveCommand) const
{
	if (ActiveCommand == EAbilityID::IdRetreat || ActiveCommand == EAbilityID::IdReverseMove)
	{
		return true;
	}
	if (M_DirectControlRetreatUnits.Contains(UnitActor))
	{
		return true;
	}
	const UEnemyRetreatController* RetreatController = M_EnemyController->GetEnemyRetreatController();
	return IsValid(RetreatController) && RetreatController->GetIsUnitRetreating(UnitActor);
}

AActor* UEnemyUnitAbilityAIComponent::FindWeaponTarget(AActor* UnitActor, const bool bIsSquad)
{
	if (bIsSquad)
	{
		ASquadController* Squad = Cast<ASquadController>(UnitActor);
		if (not IsValid(Squad))
		{
			return nullptr;
		}
		for (const ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			const AInfantryWeaponMaster* Weapon = Soldier->GetInfantryWeapon();
			AActor* Target = IsValid(Weapon) ? Weapon->GetCurrentTargetActor() : nullptr;
			if (IsValid(Target))
			{
				return Target;
			}
		}
		return nullptr;
	}
	const ATankMaster* Tank = Cast<ATankMaster>(UnitActor);
	if (not IsValid(Tank))
	{
		return nullptr;
	}
	for (const ACPPTurretsMaster* Turret : Tank->GetTurrets())
	{
		AActor* Target = IsValid(Turret) ? Turret->GetCurrentTargetActor() : nullptr;
		if (IsValid(Target))
		{
			return Target;
		}
	}
	for (const UHullWeaponComponent* HullWeapon : Tank->GetHullWeapons())
	{
		AActor* Target = IsValid(HullWeapon) ? HullWeapon->GetCurrentTargetActor() : nullptr;
		if (IsValid(Target))
		{
			return Target;
		}
	}
	return nullptr;
}

bool UEnemyUnitAbilityAIComponent::GetIsOnHoldPosition(AActor* UnitActor, const bool bIsSquad)
{
	if (bIsSquad)
	{
		const ASquadController* Squad = Cast<ASquadController>(UnitActor);
		return IsValid(Squad) && Squad->GetEngagementStance() == ERTSAggroBehaviour::Stance_HoldPosition;
	}
	const ATankMaster* Tank = Cast<ATankMaster>(UnitActor);
	return IsValid(Tank) && Tank->GetEngagementStance() == ERTSAggroBehaviour::Stance_HoldPosition;
}

bool UEnemyUnitAbilityAIComponent::TryUseBehaviourAbility(const FEnemyAbilityUnitContext& Context,
                                                          const TArray<FUnitAbilityEntry>& Card,
                                                          const UEnemyUnitAbilityAISettings& Settings,
                                                          const bool bRepairOnly)
{
	for (const FUnitAbilityEntry& Entry : Card)
	{
		if (Entry.AbilityId != EAbilityID::IdApplyBehaviour || Entry.CooldownRemaining > 0)
		{
			continue;
		}
		const EBehaviourAbilityType BehaviourAbility = static_cast<EBehaviourAbilityType>(Entry.CustomType);
		const bool bIsRepair = FEnemyAbilityUsageProfiles::GetBehaviourUsage(BehaviourAbility) ==
			EEnemyBehaviourUsage::Repair;
		if (bIsRepair != bRepairOnly || not GetShouldUseBehaviour(Context, BehaviourAbility, Settings))
		{
			continue;
		}
		// Immediate activation leaves the command queue untouched.
		if (Context.Commands->ActivateBehaviourAbility(BehaviourAbility, true) == ECommandQueueError::NoError)
		{
			DebugAbilityUse(Context, EAbilityID::IdApplyBehaviour, UEnum::GetValueAsString(BehaviourAbility));
			return true;
		}
	}
	return false;
}

bool UEnemyUnitAbilityAIComponent::GetShouldUseBehaviour(const FEnemyAbilityUnitContext& Context,
                                                         const EBehaviourAbilityType BehaviourAbility,
                                                         const UEnemyUnitAbilityAISettings& Settings) const
{
	const EEnemyBehaviourUsage Usage = FEnemyAbilityUsageProfiles::GetBehaviourUsage(BehaviourAbility);
	if (Usage == EEnemyBehaviourUsage::Repair)
	{
		return GetActorHealthRatio(Context.UnitActor, Context.bIsSquad) < Settings.RepairBelowHealthRatio;
	}
	// Every other behaviour is wasted outside a fight.
	if (not Context.bIsInCombat)
	{
		return false;
	}
	const EEnemyAbilityTargetKind TargetKind = GetTargetKind(Context.CombatTarget.Get());
	switch (Usage)
	{
	case EEnemyBehaviourUsage::CombatAnyTarget:
		return Context.bHasWeaponTarget;
	case EEnemyBehaviourUsage::CombatVsArmor:
		return Context.bHasWeaponTarget && TargetKind == EEnemyAbilityTargetKind::Armor;
	case EEnemyBehaviourUsage::CombatVsInfantry:
		return Context.bHasWeaponTarget && TargetKind == EEnemyAbilityTargetKind::Infantry;
	case EEnemyBehaviourUsage::CombatCloseRange:
		return Context.bHasWeaponTarget && GetIsTargetInCloseRange(Context, Settings);
	case EEnemyBehaviourUsage::CombatStationary:
		// Locks the vehicle in place: never for units that move or have to follow a formation.
		return Context.bHasWeaponTarget && not Context.bIsMoving && not Context.bIsInFormation;
	case EEnemyBehaviourUsage::Mobility:
		return Context.bIsMoving || (Context.ActiveCommand == EAbilityID::IdAttack && not Context.bHasWeaponTarget);
	default:
		return false;
	}
}

bool UEnemyUnitAbilityAIComponent::GetIsTargetInCloseRange(const FEnemyAbilityUnitContext& Context,
                                                           const UEnemyUnitAbilityAISettings& Settings) const
{
	const AActor* Target = Context.CombatTarget.Get();
	const float MaxWeaponRange = GetMaxWeaponRange(Context.UnitActor, Context.bIsSquad);
	if (not IsValid(Target) || MaxWeaponRange <= 0.f)
	{
		return false;
	}
	const float Distance = FVector::Dist(Context.UnitActor->GetActorLocation(), Target->GetActorLocation());
	return Distance <= MaxWeaponRange * Settings.CloseRangeFraction;
}

bool UEnemyUnitAbilityAIComponent::TryThrowGrenade(const FEnemyAbilityUnitContext& Context,
                                                   const TArray<FUnitAbilityEntry>& Card,
                                                   const UEnemyUnitAbilityAISettings& Settings,
                                                   const float NowSeconds)
{
	const bool bHasReadyGrenade = Card.ContainsByPredicate([](const FUnitAbilityEntry& Entry)
	{
		return Entry.AbilityId == EAbilityID::IdThrowGrenade && Entry.CooldownRemaining <= 0;
	});
	if (not bHasReadyGrenade)
	{
		return false;
	}
	EEnemyGrenadeRejection Rejection = EEnemyGrenadeRejection::WrongTargetKind;
	if (not GetIsSquadReadyToThrow(Context, Settings, Rejection))
	{
		++M_Stats.GrenadeRejections[static_cast<int32>(Rejection)];
		return false;
	}
	for (const FUnitAbilityEntry& Entry : Card)
	{
		if (Entry.AbilityId != EAbilityID::IdThrowGrenade || Entry.CooldownRemaining > 0)
		{
			continue;
		}
		const UGrenadeComponent* GrenadeComponent = FindGrenadeComponentOfType(Context.UnitActor, Entry.CustomType);
		if (not IsValid(GrenadeComponent))
		{
			continue;
		}
		const EGrenadeAbilityType GrenadeAbility = static_cast<EGrenadeAbilityType>(Entry.CustomType);
		const EEnemyGrenadeUsage Usage = FEnemyAbilityUsageProfiles::GetGrenadeUsage(GrenadeAbility);
		FVector AimLocation;
		if (not TryFindGrenadeAimLocation(Context, *GrenadeComponent, Usage, Settings, AimLocation, Rejection))
		{
			continue;
		}
		if (Context.Commands->ThrowGrenadeBeforeActiveCommand(AimLocation, GrenadeAbility) != ECommandQueueError::NoError)
		{
			Rejection = EEnemyGrenadeRejection::OrderRefused;
			continue;
		}
		RegisterGrenadeThrow(Context, *GrenadeComponent, AimLocation, Settings, NowSeconds);
		DebugAbilityUse(Context, EAbilityID::IdThrowGrenade, UEnum::GetValueAsString(GrenadeAbility));
		return true;
	}
	++M_Stats.GrenadeRejections[static_cast<int32>(Rejection)];
	return false;
}

bool UEnemyUnitAbilityAIComponent::GetIsSquadReadyToThrow(const FEnemyAbilityUnitContext& Context,
                                                          const UEnemyUnitAbilityAISettings& Settings,
                                                          EEnemyGrenadeRejection& OutRejection) const
{
	if (not Context.bMayUseQueuedAbilities)
	{
		OutRejection = EEnemyGrenadeRejection::NotIdleOrAttacking;
		return false;
	}
	if (not Context.bIsInCombat)
	{
		OutRejection = EEnemyGrenadeRejection::NotInCombat;
		return false;
	}
	if (not Context.CombatTarget.IsValid())
	{
		OutRejection = EEnemyGrenadeRejection::NoTarget;
		return false;
	}
	if (GetAliveSoldierCount(Context.UnitActor) < Settings.MinAliveSoldiersToThrow)
	{
		OutRejection = EEnemyGrenadeRejection::TooFewSoldiers;
		return false;
	}
	return true;
}

void UEnemyUnitAbilityAIComponent::RegisterGrenadeThrow(const FEnemyAbilityUnitContext& Context,
                                                        const UGrenadeComponent& GrenadeComponent,
                                                        const FVector& AimLocation,
                                                        const UEnemyUnitAbilityAISettings& Settings,
                                                        const float NowSeconds)
{
	FEnemyGrenadeTargetClaim Claim;
	Claim.Location = AimLocation;
	Claim.Radius = GrenadeComponent.GetGrenadeAoeRange();
	Claim.ExpireSeconds = NowSeconds + Settings.TargetClaimSeconds;
	M_GrenadeClaims.Add(Claim);
	++M_Stats.GrenadesThrown;
#if !UE_BUILD_SHIPPING || RTS_WITH_MAP_TESTS
	OnEnemyUnitAbilityUsedForTest.Broadcast(Context.UnitActor, EAbilityID::IdThrowGrenade, AimLocation);
#endif
}

bool UEnemyUnitAbilityAIComponent::TryFindGrenadeAimLocation(const FEnemyAbilityUnitContext& Context,
                                                             const UGrenadeComponent& GrenadeComponent,
                                                             const EEnemyGrenadeUsage GrenadeUsage,
                                                             const UEnemyUnitAbilityAISettings& Settings,
                                                             FVector& OutAimLocation,
                                                             EEnemyGrenadeRejection& OutRejection)
{
	AActor* Target = Context.CombatTarget.Get();
	const EEnemyAbilityTargetKind TargetKind = GetTargetKind(Target);
	const bool bIsHardTarget = TargetKind == EEnemyAbilityTargetKind::Armor
		|| (TargetKind == EEnemyAbilityTargetKind::Structure && GrenadeUsage == EEnemyGrenadeUsage::AnyTarget);
	if (GrenadeUsage != EEnemyGrenadeUsage::AntiInfantry && bIsHardTarget)
	{
		OutAimLocation = Target->GetActorLocation();
		OutRejection = EEnemyGrenadeRejection::TargetNearlyDead;
		if (not GetIsTargetHealthWorthAbility(Target, Settings))
		{
			return false;
		}
	}
	else if (GrenadeUsage != EEnemyGrenadeUsage::AntiArmor && TargetKind == EEnemyAbilityTargetKind::Infantry)
	{
		OutRejection = EEnemyGrenadeRejection::TooFewInBlast;
		if (not TryFindInfantryAimLocation(Target->GetActorLocation(), GrenadeComponent.GetGrenadeAoeRange(), Settings,
		                                   OutAimLocation))
		{
			return false;
		}
	}
	else
	{
		OutRejection = EEnemyGrenadeRejection::WrongTargetKind;
		return false;
	}
	OutRejection = EEnemyGrenadeRejection::Claimed;
	if (GetIsGrenadeTargetClaimed(OutAimLocation))
	{
		return false;
	}
	// On hold position the squad may not walk; otherwise a short walk is fine but a long one lets the target leave.
	OutRejection = EEnemyGrenadeRejection::OutOfReach;
	const float ReachMultiplier = Context.bIsOnHoldPosition ? 1.f : Settings.GrenadeWalkRangeMultiplier;
	const float Distance = FVector::Dist(Context.UnitActor->GetActorLocation(), OutAimLocation);
	return Distance <= GrenadeComponent.GetThrowRange() * ReachMultiplier;
}

bool UEnemyUnitAbilityAIComponent::TryFindInfantryAimLocation(const FVector& TargetLocation, const float AoeRadius,
                                                              const UEnemyUnitAbilityAISettings& Settings,
                                                              FVector& OutAimLocation)
{
	GatherPlayerSoldierLocations();
	const float ClusterSearchRadius = AoeRadius * EnemyUnitAbilityAIConstants::InfantryClusterSearchRadiusMultiplier;
	FVector ClusterSum = FVector::ZeroVector;
	int32 ClusterCount = 0;
	for (const FVector& SoldierLocation : M_PlayerSoldierLocations)
	{
		if (FVector::DistSquared(SoldierLocation, TargetLocation) <= FMath::Square(ClusterSearchRadius))
		{
			ClusterSum += SoldierLocation;
			++ClusterCount;
		}
	}
	const FVector ClusterCentre = ClusterCount > 0 ? ClusterSum / ClusterCount : TargetLocation;
	const int32 SoldiersAtTarget = CountPlayerSoldiersNear(TargetLocation, AoeRadius);
	const int32 SoldiersAtCentre = CountPlayerSoldiersNear(ClusterCentre, AoeRadius);
	OutAimLocation = SoldiersAtCentre > SoldiersAtTarget ? ClusterCentre : TargetLocation;
	return FMath::Max(SoldiersAtTarget, SoldiersAtCentre) >= Settings.MinSoldiersInGrenadeArea;
}

int32 UEnemyUnitAbilityAIComponent::CountPlayerSoldiersNear(const FVector& Location, const float Radius)
{
	GatherPlayerSoldierLocations();
	const float RadiusSquared = FMath::Square(Radius);
	int32 Count = 0;
	for (const FVector& SoldierLocation : M_PlayerSoldierLocations)
	{
		if (FVector::DistSquared(SoldierLocation, Location) <= RadiusSquared)
		{
			++Count;
		}
	}
	return Count;
}

void UEnemyUnitAbilityAIComponent::GatherPlayerSoldierLocations()
{
	if (bM_HasGatheredPlayerSoldiersThisTick)
	{
		return;
	}
	bM_HasGatheredPlayerSoldiersThisTick = true;
	M_PlayerSoldierLocations.Reset();
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return;
	}
	for (const ASquadUnit* Soldier : GameUnitManager->GetSquadUnitsOfPlayer(EnemyUnitAbilityAIConstants::HumanPlayerIndex))
	{
		if (IsValid(Soldier))
		{
			M_PlayerSoldierLocations.Add(Soldier->GetActorLocation());
		}
	}
}

bool UEnemyUnitAbilityAIComponent::GetIsGrenadeTargetClaimed(const FVector& AimLocation) const
{
	return M_GrenadeClaims.ContainsByPredicate([&AimLocation](const FEnemyGrenadeTargetClaim& Claim)
	{
		return FVector::DistSquared(Claim.Location, AimLocation) <= FMath::Square(Claim.Radius);
	});
}

void UEnemyUnitAbilityAIComponent::RemoveExpiredGrenadeClaims(const float NowSeconds)
{
	M_GrenadeClaims.RemoveAll([NowSeconds](const FEnemyGrenadeTargetClaim& Claim)
	{
		return Claim.ExpireSeconds <= NowSeconds;
	});
}

UGrenadeComponent* UEnemyUnitAbilityAIComponent::FindGrenadeComponentOfType(AActor* SquadActor,
                                                                           const int32 GrenadeAbilityRaw)
{
	if (not IsValid(SquadActor))
	{
		return nullptr;
	}
	TArray<UGrenadeComponent*> GrenadeComponents;
	SquadActor->GetComponents<UGrenadeComponent>(GrenadeComponents);
	for (UGrenadeComponent* GrenadeComponent : GrenadeComponents)
	{
		if (IsValid(GrenadeComponent)
			&& static_cast<int32>(GrenadeComponent->GetGrenadeAbilityType()) == GrenadeAbilityRaw)
		{
			return GrenadeComponent;
		}
	}
	return nullptr;
}

EEnemyAbilityTargetKind UEnemyUnitAbilityAIComponent::GetTargetKind(const AActor* TargetActor)
{
	if (not IsValid(TargetActor))
	{
		return EEnemyAbilityTargetKind::None;
	}
	if (TargetActor->IsA<ASquadUnit>())
	{
		return EEnemyAbilityTargetKind::Infantry;
	}
	if (TargetActor->IsA<ATankMaster>())
	{
		return EEnemyAbilityTargetKind::Armor;
	}
	return EEnemyAbilityTargetKind::Structure;
}

float UEnemyUnitAbilityAIComponent::GetActorHealthRatio(AActor* Actor, const bool bIsSquad)
{
	if (not bIsSquad)
	{
		const UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		return IsValid(Health) ? Health->GetHealthPercentage() : 1.f;
	}
	ASquadController* Squad = Cast<ASquadController>(Actor);
	if (not IsValid(Squad))
	{
		return 1.f;
	}
	float CurrentHealth = 0.f;
	float MaxHealth = 0.f;
	for (const ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		const UHealthComponent* Health = Soldier->FindComponentByClass<UHealthComponent>();
		if (IsValid(Health))
		{
			CurrentHealth += Health->GetCurrentHealth();
			MaxHealth += Health->GetMaxHealth();
		}
	}
	return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 1.f;
}

bool UEnemyUnitAbilityAIComponent::GetIsTargetHealthWorthAbility(AActor* TargetActor,
                                                                 const UEnemyUnitAbilityAISettings& Settings)
{
	return GetActorHealthRatio(TargetActor, false) >= Settings.SkipTargetsBelowHealthRatio;
}

float UEnemyUnitAbilityAIComponent::GetMaxWeaponRange(AActor* UnitActor, const bool bIsSquad)
{
	TArray<UWeaponState*> Weapons;
	if (bIsSquad)
	{
		if (ASquadController* Squad = Cast<ASquadController>(UnitActor))
		{
			Weapons = Squad->GetWeaponsOfSquad();
		}
	}
	else if (const ATankMaster* Tank = Cast<ATankMaster>(UnitActor))
	{
		for (ACPPTurretsMaster* Turret : Tank->GetTurrets())
		{
			if (IsValid(Turret))
			{
				Weapons.Append(Turret->GetWeapons());
			}
		}
		for (UHullWeaponComponent* HullWeapon : Tank->GetHullWeapons())
		{
			if (IsValid(HullWeapon))
			{
				Weapons.Append(HullWeapon->GetWeapons());
			}
		}
	}
	float MaxRange = 0.f;
	for (const UWeaponState* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			MaxRange = FMath::Max(MaxRange, Weapon->GetWeaponRangeBehaviourAdjusted());
		}
	}
	return MaxRange;
}

int32 UEnemyUnitAbilityAIComponent::GetAliveSoldierCount(AActor* SquadActor)
{
	const ASquadController* Squad = Cast<ASquadController>(SquadActor);
	return IsValid(Squad) ? Squad->GetCurrentAliveSquadUnitCountPure() : 0;
}

void UEnemyUnitAbilityAIComponent::OnBehaviourUsed(FEnemyAbilityAIUnit& Unit, const FEnemyAbilityUnitContext& Context,
                                                   const float NowSeconds)
{
	Unit.M_LastAbilityUseSeconds = NowSeconds;
	Unit.bM_HasUsedAbility = true;
	++M_Stats.BehavioursUsed;
#if !UE_BUILD_SHIPPING || RTS_WITH_MAP_TESTS
	OnEnemyUnitAbilityUsedForTest.Broadcast(Context.UnitActor, EAbilityID::IdApplyBehaviour, FVector::ZeroVector);
#endif
}

void UEnemyUnitAbilityAIComponent::DebugAbilityUse(const FEnemyAbilityUnitContext& Context, const EAbilityID Ability,
                                                   const FString& Detail) const
{
	if constexpr (EnemyAISettings::Debugging::UnitAbilityAIDebugging)
	{
		const FString Text = Global_GetAbilityIDAsString(Ability) + TEXT(": ") + Detail;
		DrawDebugString(GetWorld(),
		                Context.UnitActor->GetActorLocation() + FVector(0.f, 0.f,
		                                                                EnemyUnitAbilityAIConstants::DebugTextHeight),
		                Text, nullptr, FColor::Yellow, EnemyUnitAbilityAIConstants::DebugTextDuration);
	}
}
