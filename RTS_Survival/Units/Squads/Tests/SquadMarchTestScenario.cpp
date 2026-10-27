#include "SquadMarchTestScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "ShowFlags.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSquadMarchTest, Log, All);

namespace SquadMarchTestScenarioPrivate
{
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr float FindSquadTimeoutSeconds = 60.0f;
	// Unseen soldiers walk slower than their speed in an unattended run, so the walks get a lot of time.
	constexpr float PhaseTimeoutSeconds = 120.0f;
	constexpr float MarchDistance = 1400.0f;
	constexpr float MinimumWalkedDistance = 350.0f;
	constexpr float SpeedTolerance = 1.0f;
	// The montage rate is set from the speed of the previous animation update; allow for that lag.
	constexpr float PlayRateTolerance = 0.25f;
	constexpr float MinimumMarchPlayRate = 0.35f;
	constexpr float TestDamage = 1.0f;
	constexpr int32 PicturesToTake = 4;
	constexpr int32 FramesBetweenPictures = 12;
	// Pictures start once the squad is in its stride, away from where it stood.
	constexpr float WalkedDistanceBeforePictures = 150.0f;
	constexpr int32 DestinationAttempts = 8;

	bool GetIsLivingSoldier(ASquadUnit* Soldier)
	{
		return IsValid(Soldier) && Soldier->IsUnitAlive();
	}

	bool GetIsUsablePlayerSquad(ASquadController* Squad)
	{
		if (not IsValid(Squad) || not Squad->GetIsSquadFullyLoadedAndInitialized() || Squad->GetSquadUnitsCount() < 2)
		{
			return false;
		}
		const URTSComponent* SquadRTSComponent = Squad->GetRTSComponent();
		const UCargoSquad* CargoSquad = Squad->FindComponentByClass<UCargoSquad>();
		const bool bIsInsideCargo = IsValid(CargoSquad) && CargoSquad->GetIsInsideCargo();
		return IsValid(SquadRTSComponent) && SquadRTSComponent->GetOwningPlayer() == PlayerOwnedTeam &&
			not Squad->GetSquadAlreadyHasTeamWeapon() && not bIsInsideCargo;
	}

	// The weapon on the shoulder is what the pictures are for, so a squad of riflemen is preferred.
	bool GetIsRifleSquad(ASquadController& Squad)
	{
		for (const ASquadUnit* Soldier : Squad.GetSquadUnitsChecked())
		{
			if (IsValid(Soldier) && Soldier->GetClass()->GetName().Contains(TEXT("Rifle")))
			{
				return true;
			}
		}
		return false;
	}

	void SetViewportLighting(const bool bLit)
	{
		if (GEngine != nullptr && GEngine->GameViewport != nullptr)
		{
			ApplyViewMode(bLit ? VMI_Lit : VMI_Unlit, true, GEngine->GameViewport->EngineShowFlags);
		}
	}

	float GetWalkSpeed(const ASquadUnit& Soldier)
	{
		const UCharacterMovementComponent* SoldierMovement = Soldier.GetCharacterMovement();
		return IsValid(SoldierMovement) ? SoldierMovement->MaxWalkSpeed : 0.0f;
	}

	bool GetIsMarchPlaying(const ASquadUnit& Soldier)
	{
		const USquadUnitAnimInstance* SoldierAnimation = Soldier.GetAnimBP_SquadUnit();
		return IsValid(SoldierAnimation) && SoldierAnimation->GetIsMarchAnimationPlaying();
	}
}

void FSquadMarchTestScenario::Start()
{
	M_Phase = ESquadMarchTestPhase::WaitingForCoverScan;
	UE_LOG(LogRTSSquadMarchTest, Display, TEXT("RTS_MARCH_TEST scheduled"));
}

void FSquadMarchTestScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(
		LogRTSSquadMarchTest,
		Display,
		TEXT("RTS_MARCH_TEST check=%s %s %s"),
		bCondition ? TEXT("PASS") : TEXT("FAIL"),
		Description,
		*Details);
}

void FSquadMarchTestScenario::EnterPhase(const UWorld& World, const ESquadMarchTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartWorldSeconds = World.GetTimeSeconds();
	M_PhaseDeadlineWorldSeconds = M_PhaseStartWorldSeconds + (NewPhase == ESquadMarchTestPhase::FindingSquad
		? SquadMarchTestScenarioPrivate::FindSquadTimeoutSeconds
		: SquadMarchTestScenarioPrivate::PhaseTimeoutSeconds);
}

void FSquadMarchTestScenario::Finish(const UWorld& World)
{
	M_Phase = ESquadMarchTestPhase::Inactive;
	SquadMarchTestScenarioPrivate::SetViewportLighting(true);
	UE_LOG(
		LogRTSSquadMarchTest,
		Display,
		TEXT("RTS_MARCH_TEST RESULT %s failed_checks=%d map=%s"),
		M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"),
		M_FailedCheckCount,
		*World.GetName());
}

bool FSquadMarchTestScenario::FinishWhenTimedOut(const UWorld& World, const TCHAR* ExpectedEvent)
{
	if (World.GetTimeSeconds() < M_PhaseDeadlineWorldSeconds)
	{
		return false;
	}
	Check(false, ExpectedEvent, TEXT("timed out"));
	Finish(World);
	return true;
}

void FSquadMarchTestScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == ESquadMarchTestPhase::Inactive || not IsValid(World))
	{
		return;
	}
	if (M_Phase == ESquadMarchTestPhase::WaitingForCoverScan)
	{
		// The map boots paused behind the start-game widget.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
		if (CoverSubsystem.GetHasCompletedFullScan())
		{
			EnterPhase(*World, ESquadMarchTestPhase::FindingSquad);
		}
		return;
	}
	if (M_Phase == ESquadMarchTestPhase::FindingSquad)
	{
		TickFindingSquad(*World);
		return;
	}
	if (not M_Squad.IsValid())
	{
		Check(false, TEXT("the test's squad lived through the test"));
		Finish(*World);
		return;
	}
	KeepSquadMovingSmoothly();
	if (M_Phase == ESquadMarchTestPhase::MarchWaitingForStride)
	{
		TickMarchWaitingForStride(*World);
		return;
	}
	TickPatrolWaitingForStride(*World);
}

void FSquadMarchTestScenario::TickFindingSquad(UWorld& World)
{
	FVector AwayFromEnemies = FVector::ForwardVector;
	ASquadController* Squad = FindSquadFarthestFromEnemies(World, AwayFromEnemies);
	if (not IsValid(Squad))
	{
		if (World.GetTimeSeconds() >= M_PhaseDeadlineWorldSeconds)
		{
			Check(false, TEXT("a player squad with at least two soldiers is on the map"));
			Finish(World);
		}
		return;
	}
	M_Squad = Squad;
	M_SquadStartLocation = Squad->GetActorLocation();
	if (not FindWalkableDestination(World, AwayFromEnemies))
	{
		Check(false, TEXT("there is walkable ground to march to"));
		Finish(World);
		return;
	}
	M_BaseWalkSpeeds.Reset();
	for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		if (SquadMarchTestScenarioPrivate::GetIsLivingSoldier(Soldier))
		{
			M_BaseWalkSpeeds.Add(Soldier, SquadMarchTestScenarioPrivate::GetWalkSpeed(*Soldier));
		}
	}
	UE_LOG(
		LogRTSSquadMarchTest,
		Display,
		TEXT("RTS_MARCH_TEST cast squad=%s soldiers=%d march_cm=%.0f"),
		*Squad->GetName(),
		M_BaseWalkSpeeds.Num(),
		FVector::Dist2D(M_SquadStartLocation, M_MarchDestination));
	StartMarch(World);
}

ASquadController* FSquadMarchTestScenario::FindSquadFarthestFromEnemies(
	UWorld& World,
	FVector& OutAwayFromEnemies) const
{
	using namespace SquadMarchTestScenarioPrivate;
	TArray<FVector> EnemyLocations;
	TArray<ASquadController*> PlayerSquads;
	for (TActorIterator<ASquadController> SquadIterator(&World); SquadIterator; ++SquadIterator)
	{
		ASquadController* Squad = *SquadIterator;
		const URTSComponent* SquadRTSComponent = IsValid(Squad) ? Squad->GetRTSComponent() : nullptr;
		if (not IsValid(SquadRTSComponent))
		{
			continue;
		}
		if (SquadRTSComponent->GetOwningPlayer() != PlayerOwnedTeam)
		{
			EnemyLocations.Add(Squad->GetActorLocation());
			continue;
		}
		if (GetIsUsablePlayerSquad(Squad))
		{
			PlayerSquads.Add(Squad);
		}
	}
	// -SquadMarchSquad=<part of a squad name> picks the squad; otherwise riflemen go first.
	FString SquadNameFilter;
	FParse::Value(FCommandLine::Get(), TEXT("SquadMarchSquad="), SquadNameFilter);
	TArray<ASquadController*> PreferredSquads;
	for (ASquadController* Squad : PlayerSquads)
	{
		const bool bIsPreferred = SquadNameFilter.IsEmpty()
			? GetIsRifleSquad(*Squad)
			: Squad->GetName().Contains(SquadNameFilter);
		if (bIsPreferred)
		{
			PreferredSquads.Add(Squad);
		}
	}
	if (not PreferredSquads.IsEmpty())
	{
		PlayerSquads = PreferredSquads;
	}
	ASquadController* BestSquad = nullptr;
	float BestEnemyDistance = -1.0f;
	for (ASquadController* Squad : PlayerSquads)
	{
		const FVector SquadLocation = Squad->GetActorLocation();
		float NearestEnemyDistance = TNumericLimits<float>::Max();
		FVector NearestEnemyLocation = SquadLocation - FVector::ForwardVector;
		for (const FVector& EnemyLocation : EnemyLocations)
		{
			const float EnemyDistance = FVector::Dist2D(SquadLocation, EnemyLocation);
			if (EnemyDistance < NearestEnemyDistance)
			{
				NearestEnemyDistance = EnemyDistance;
				NearestEnemyLocation = EnemyLocation;
			}
		}
		if (NearestEnemyDistance > BestEnemyDistance)
		{
			BestEnemyDistance = NearestEnemyDistance;
			BestSquad = Squad;
			OutAwayFromEnemies = (SquadLocation - NearestEnemyLocation).GetSafeNormal2D();
		}
	}
	return BestSquad;
}

bool FSquadMarchTestScenario::FindWalkableDestination(UWorld& World, const FVector& PreferredDirection)
{
	using namespace SquadMarchTestScenarioPrivate;
	// Away from the enemy first, then turning further round until there is ground to walk on.
	constexpr float TurnPerAttemptDegrees = 45.0f;
	for (int32 Attempt = 0; Attempt < DestinationAttempts; ++Attempt)
	{
		const float TurnDegrees = TurnPerAttemptDegrees * ((Attempt + 1) / 2) * (Attempt % 2 == 0 ? 1.0f : -1.0f);
		const FVector Direction = PreferredDirection.RotateAngleAxis(TurnDegrees, FVector::UpVector);
		bool bIsNavigable = false;
		const FVector Destination = RTSFunctionLibrary::GetLocationProjected(
			&World,
			M_SquadStartLocation + Direction * MarchDistance,
			true,
			bIsNavigable);
		if (bIsNavigable)
		{
			M_MarchDestination = Destination;
			return true;
		}
	}
	return false;
}

void FSquadMarchTestScenario::StartMarch(UWorld& World)
{
	using namespace SquadMarchTestScenarioPrivate;
	ASquadController* Squad = M_Squad.Get();
	const ECommandQueueError MarchError = Squad->MarchToLocation(M_MarchDestination, true);
	Check(
		MarchError == ECommandQueueError::NoError && Squad->GetActiveCommandID() == EAbilityID::IdMarch,
		TEXT("the march order is accepted without a card entry and becomes the squad's command"),
		*UEnum::GetValueAsString(Squad->GetActiveCommandID()));
	for (const TPair<TWeakObjectPtr<ASquadUnit>, float>& BaseSpeed : M_BaseWalkSpeeds)
	{
		ASquadUnit* Soldier = BaseSpeed.Key.Get();
		if (not GetIsLivingSoldier(Soldier))
		{
			continue;
		}
		const float ExpectedSpeed = BaseSpeed.Value * Soldier->GetMarchSpeedMultiplier();
		Check(
			FMath::IsNearlyEqual(GetWalkSpeed(*Soldier), ExpectedSpeed, SpeedTolerance) &&
			Soldier->GetActiveCommand() == EAbilityID::IdMarch && GetIsMarchPlaying(*Soldier),
			TEXT("a marching soldier walks at the march speed with the march montage looping"),
			FString::Printf(
				TEXT("soldier=%s walk_speed=%.0f expected=%.0f base=%.0f montage=%d"),
				*Soldier->GetName(),
				GetWalkSpeed(*Soldier),
				ExpectedSpeed,
				BaseSpeed.Value,
				GetIsMarchPlaying(*Soldier) ? 1 : 0));
	}
	Check(
		Squad->PatrolToLocation(M_MarchDestination, false) == ECommandQueueError::QueueHasPatrol,
		TEXT("nothing can be queued behind a march"));
	M_PicturesTaken = 0;
	M_FramesUntilPicture = FramesBetweenPictures;
	EnterPhase(World, ESquadMarchTestPhase::MarchWaitingForStride);
}

void FSquadMarchTestScenario::TickMarchWaitingForStride(UWorld& World)
{
	TickPictures(World);
	if (M_Squad->GetActiveCommandID() != EAbilityID::IdMarch)
	{
		const URTSComponent* SquadRTSComponent = M_Squad->GetRTSComponent();
		Check(
			false,
			TEXT("the squad keeps marching until the test hurts it"),
			FString::Printf(
				TEXT("command=%s in_combat=%d"),
				*UEnum::GetValueAsString(M_Squad->GetActiveCommandID()),
				IsValid(SquadRTSComponent) && SquadRTSComponent->GetIsUnitInCombat() ? 1 : 0));
		Finish(World);
		return;
	}
	const bool bHasPictures = M_PicturesTaken >= SquadMarchTestScenarioPrivate::PicturesToTake;
	if (not GetHasSquadWalkedOff() || not bHasPictures)
	{
		FinishWhenTimedOut(World, TEXT("the marching squad walks off with its pictures taken"));
		return;
	}
	CheckMarchingSoldiers();
	HurtOneSoldierAndCheckIdle(TEXT("march"));
	CheckSoldiersBackToNormal(TEXT("march"));
	StartPatrol(World);
}

bool FSquadMarchTestScenario::GetHasSquadWalkedOff() const
{
	float WalkedDistance = 0.0f;
	int32 LivingSoldiers = 0;
	for (const TPair<TWeakObjectPtr<ASquadUnit>, float>& BaseSpeed : M_BaseWalkSpeeds)
	{
		ASquadUnit* Soldier = BaseSpeed.Key.Get();
		if (SquadMarchTestScenarioPrivate::GetIsLivingSoldier(Soldier))
		{
			WalkedDistance += FVector::Dist2D(Soldier->GetActorLocation(), M_SquadStartLocation);
			++LivingSoldiers;
		}
	}
	return LivingSoldiers > 0 &&
		WalkedDistance / LivingSoldiers >= SquadMarchTestScenarioPrivate::MinimumWalkedDistance;
}

void FSquadMarchTestScenario::CheckMarchingSoldiers()
{
	using namespace SquadMarchTestScenarioPrivate;
	for (const TPair<TWeakObjectPtr<ASquadUnit>, float>& BaseSpeed : M_BaseWalkSpeeds)
	{
		ASquadUnit* Soldier = BaseSpeed.Key.Get();
		if (not GetIsLivingSoldier(Soldier))
		{
			continue;
		}
		const USquadUnitAnimInstance* SoldierAnimation = Soldier->GetAnimBP_SquadUnit();
		const float GroundSpeed = Soldier->GetVelocity().Size2D();
		const float ExpectedPlayRate = FMath::Max(
			MinimumMarchPlayRate,
			GroundSpeed / SoldierAnimation->GetMarchMontageNaturalSpeed());
		const float PlayRate = SoldierAnimation->GetMarchAnimationPlayRate();
		Check(
			GetIsMarchPlaying(*Soldier) && GroundSpeed <= GetWalkSpeed(*Soldier) + SpeedTolerance &&
			FMath::IsNearlyEqual(PlayRate, ExpectedPlayRate, PlayRateTolerance),
			TEXT("while walking the march montage keeps looping at a rate that follows the ground speed"),
			FString::Printf(
				TEXT("soldier=%s ground_speed=%.0f walk_speed=%.0f play_rate=%.2f expected_rate=%.2f"),
				*Soldier->GetName(),
				GroundSpeed,
				GetWalkSpeed(*Soldier),
				PlayRate,
				ExpectedPlayRate));
	}
}

void FSquadMarchTestScenario::HurtOneSoldierAndCheckIdle(const TCHAR* OrderName)
{
	ASquadUnit* HurtSoldier = nullptr;
	for (const TPair<TWeakObjectPtr<ASquadUnit>, float>& BaseSpeed : M_BaseWalkSpeeds)
	{
		if (SquadMarchTestScenarioPrivate::GetIsLivingSoldier(BaseSpeed.Key.Get()))
		{
			HurtSoldier = BaseSpeed.Key.Get();
			break;
		}
	}
	if (not IsValid(HurtSoldier))
	{
		Check(false, TEXT("a soldier is left to hurt"), OrderName);
		return;
	}
	UGameplayStatics::ApplyDamage(
		HurtSoldier,
		SquadMarchTestScenarioPrivate::TestDamage,
		nullptr,
		nullptr,
		UDamageType::StaticClass());
	// Restore what the hit took so later checks do not see an injured soldier.
	if (UHealthComponent* SoldierHealth = HurtSoldier->GetHealthComponent())
	{
		SoldierHealth->SetCurrentHealth(SoldierHealth->GetMaxHealth());
	}
	Check(
		M_Squad->GetActiveCommandID() == EAbilityID::IdIdle && M_Squad->GetIsUnitIdle(),
		*FString::Printf(TEXT("one hurt soldier drops the whole squad out of its %s to idle"), OrderName),
		FString::Printf(
			TEXT("hurt=%s command=%s"),
			*HurtSoldier->GetName(),
			*UEnum::GetValueAsString(M_Squad->GetActiveCommandID())));
}

void FSquadMarchTestScenario::CheckSoldiersBackToNormal(const TCHAR* OrderName)
{
	using namespace SquadMarchTestScenarioPrivate;
	for (const TPair<TWeakObjectPtr<ASquadUnit>, float>& BaseSpeed : M_BaseWalkSpeeds)
	{
		ASquadUnit* Soldier = BaseSpeed.Key.Get();
		if (not GetIsLivingSoldier(Soldier))
		{
			continue;
		}
		Check(
			Soldier->GetActiveCommand() == EAbilityID::IdIdle &&
			FMath::IsNearlyEqual(GetWalkSpeed(*Soldier), BaseSpeed.Value, SpeedTolerance) &&
			not GetIsMarchPlaying(*Soldier),
			*FString::Printf(TEXT("after the %s every soldier is idle at full speed without the march montage"), OrderName),
			FString::Printf(
				TEXT("soldier=%s command=%s walk_speed=%.0f base=%.0f montage=%d"),
				*Soldier->GetName(),
				*UEnum::GetValueAsString(Soldier->GetActiveCommand()),
				GetWalkSpeed(*Soldier),
				BaseSpeed.Value,
				GetIsMarchPlaying(*Soldier) ? 1 : 0));
	}
}

void FSquadMarchTestScenario::StartPatrol(UWorld& World)
{
	ASquadController* Squad = M_Squad.Get();
	M_SquadStartLocation = Squad->GetActorLocation();
	// Back towards where the squad came from: that ground was just walked.
	const FVector PatrolDestination = M_SquadStartLocation +
		(M_SquadStartLocation - M_MarchDestination).GetSafeNormal2D() * SquadMarchTestScenarioPrivate::MarchDistance;
	bool bIsNavigable = false;
	const FVector ProjectedDestination = RTSFunctionLibrary::GetLocationProjected(
		&World,
		PatrolDestination,
		true,
		bIsNavigable);
	const ECommandQueueError PatrolError = Squad->PatrolToLocation(ProjectedDestination, true);
	Check(
		bIsNavigable && PatrolError == ECommandQueueError::NoError &&
		Squad->GetActiveCommandID() == EAbilityID::IdPatrol,
		TEXT("the squad accepts a patrol order"),
		FString::Printf(
			TEXT("navigable=%d error=%s"),
			bIsNavigable ? 1 : 0,
			*UEnum::GetValueAsString(PatrolError)));
	if (Squad->GetActiveCommandID() != EAbilityID::IdPatrol)
	{
		Finish(World);
		return;
	}
	EnterPhase(World, ESquadMarchTestPhase::PatrolWaitingForStride);
}

void FSquadMarchTestScenario::TickPatrolWaitingForStride(UWorld& World)
{
	if (M_Squad->GetActiveCommandID() != EAbilityID::IdPatrol)
	{
		Check(
			false,
			TEXT("the squad keeps patrolling until the test hurts it"),
			*UEnum::GetValueAsString(M_Squad->GetActiveCommandID()));
		Finish(World);
		return;
	}
	if (not GetHasSquadWalkedOff())
	{
		FinishWhenTimedOut(World, TEXT("the patrolling squad walks off"));
		return;
	}
	HurtOneSoldierAndCheckIdle(TEXT("patrol"));
	CheckSoldiersBackToNormal(TEXT("patrol"));
	Finish(World);
}

void FSquadMarchTestScenario::KeepSquadMovingSmoothly() const
{
	for (const ASquadUnit* Soldier : M_Squad->GetSquadUnitsChecked())
	{
		UCharacterMovementComponent* SoldierMovement = IsValid(Soldier) ? Soldier->GetCharacterMovement() : nullptr;
		if (IsValid(SoldierMovement))
		{
			// The optimizer sets its interval again on its own updates, so this is repeated every frame.
			SoldierMovement->SetComponentTickInterval(0.0f);
		}
	}
}

void FSquadMarchTestScenario::TickPictures(UWorld& World)
{
	using namespace SquadMarchTestScenarioPrivate;
	if (M_PicturesTaken >= PicturesToTake)
	{
		return;
	}
	ASquadUnit* Subject = nullptr;
	for (const TPair<TWeakObjectPtr<ASquadUnit>, float>& BaseSpeed : M_BaseWalkSpeeds)
	{
		if (GetIsLivingSoldier(BaseSpeed.Key.Get()))
		{
			Subject = BaseSpeed.Key.Get();
			break;
		}
	}
	if (not IsValid(Subject) ||
		FVector::Dist2D(Subject->GetActorLocation(), M_SquadStartLocation) < WalkedDistanceBeforePictures)
	{
		return;
	}
	// Unlit: soldiers that walk out of the revealed area would otherwise come out black.
	SetViewportLighting(false);
	AimCameraAtSquad(World);
	if (--M_FramesUntilPicture > 0)
	{
		return;
	}
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SquadMarchTest"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString FileName = FString::Printf(TEXT("march_%02d.png"), M_PicturesTaken);
	FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, FileName), false, false);
	UE_LOG(LogRTSSquadMarchTest, Display, TEXT("RTS_MARCH_TEST picture=%s"), *FileName);
	++M_PicturesTaken;
	M_FramesUntilPicture = FramesBetweenPictures;
}

void FSquadMarchTestScenario::AimCameraAtSquad(UWorld& World)
{
	ACameraActor* Camera = M_Camera.Get();
	if (not IsValid(Camera))
	{
		Camera = World.SpawnActor<ACameraActor>();
		M_Camera = Camera;
	}
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	ASquadUnit* Subject = nullptr;
	for (const TPair<TWeakObjectPtr<ASquadUnit>, float>& BaseSpeed : M_BaseWalkSpeeds)
	{
		if (SquadMarchTestScenarioPrivate::GetIsLivingSoldier(BaseSpeed.Key.Get()))
		{
			Subject = BaseSpeed.Key.Get();
			break;
		}
	}
	if (not IsValid(Camera) || not IsValid(PlayerController) || not IsValid(Subject))
	{
		return;
	}
	if (UCameraComponent* CameraComponent = Camera->GetCameraComponent())
	{
		CameraComponent->bConstrainAspectRatio = false;
	}
	// From the side and a little ahead, so the rifle on the shoulder and the stride both show.
	constexpr float SideDistance = 380.0f;
	constexpr float AheadDistance = 120.0f;
	constexpr float CameraHeight = 120.0f;
	const FVector MarchDirection = (M_MarchDestination - M_SquadStartLocation).GetSafeNormal2D();
	const FVector SideDirection = FVector::CrossProduct(FVector::UpVector, MarchDirection);
	const FVector SubjectLocation = Subject->GetActorLocation();
	const FVector CameraLocation = SubjectLocation + SideDirection * SideDistance + MarchDirection * AheadDistance +
		FVector(0.0, 0.0, CameraHeight);
	Camera->SetActorLocationAndRotation(CameraLocation, (SubjectLocation - CameraLocation).Rotation());
	PlayerController->SetViewTarget(Camera);
}

#endif // RTS_WITH_MAP_TESTS
