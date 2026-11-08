// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RailTrooperAnimTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "AnimationRuntime.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSRailTrooperTest, Log, All);

namespace RailTrooperAnimTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr int32 ExpectedTroopers = 4;
	constexpr EGlobalAbility ParadropAbility = EGlobalAbility::GA_GerRailTroopersParadrop;
	const TCHAR* const TrooperClassMarker = TEXT("Ger_Inf_RailTrooper");
	const TCHAR* const SquadClassMarker = TEXT("Squad_Ger_RailTroopers");
	const TCHAR* const WeaponClassMarker = TEXT("M920");
	constexpr EWeaponName ExpectedWeapon = EWeaponName::M920_AtSniper;
	const TCHAR* const MeshMarker = TEXT("SK_RailTrooper");
	const TCHAR* const SkeletonMarker = TEXT("InfantryMaster_Skeleton");
	const TCHAR* const AnimClassMarker = TEXT("AnimBp_SquadUnit");
	const FName FootBone(TEXT("foot_l"));
	const FName HandBone(TEXT("hand_r"));
	// Component-space travel of the left foot during a walk cycle is tens of centimetres; a frozen pose stays at 0.
	constexpr float MinWalkFootTravel = 15.f;
	// Idle breathing and recoil move the right hand a little; anything above this proves the pose is animated.
	constexpr float MinAnimatedHandTravel = 0.5f;
	// The rifle-ready pose holds the right hand far from where the reference pose (arms out) puts it.
	constexpr float MinHandOffsetFromRefPose = 10.f;
	// The rail rifle hangs this close to the right hand bone when it sits on GunSocket.
	constexpr float MaxWeaponToHandDistance = 60.f;
	constexpr float CloseUpDistance = 420.f;
	constexpr float CloseUpHeight = 90.f;
	// Half the infantry pace: a trooper moving at least this fast is walking, not sliding into place.
	constexpr float MinWalkSpeed = 150.f;

	// Auto exposure starts dark on map load; the first pictures waited too little (black / dim at 3 s).
	constexpr float StartDelaySeconds = 10.f;
	constexpr float SpawnTimeoutSeconds = 30.f;
	// The idle window starts once every trooper stands (slower than this) after walking off its spawn spot.
	constexpr float StandingSpeed = 5.f;
	constexpr float SettleTimeoutSeconds = 15.f;
	constexpr float IdleSampleSeconds = 3.f;
	constexpr float WalkShotSeconds = 2.f;
	// The walking picture looks across the walk direction, so the leg swing shows.
	constexpr float WalkSideViewYawOffset = 90.f;
	constexpr float MoveTimeoutSeconds = 30.f;
	constexpr float MoveDistance = 1500.f;
	constexpr float MoveArrivedRadius = 450.f;
	constexpr float AttackTimeoutSeconds = 15.f;
	constexpr float FireSampleSeconds = 2.5f;
	constexpr float DropTimeoutSeconds = 180.f;
	// Landed troopers get this long to stand up and settle before their idle pose is sampled.
	constexpr float LandedSettleSeconds = 3.f;
	constexpr float DropIdleSampleSeconds = 3.f;
	constexpr float DirectSpawnSideOffset = 3500.f;
	// The paradrop lands this far to the other side of the anchor, away from the directly spawned squad.
	constexpr float DropSideOffset = 3500.f;
	constexpr float LandscapeEdgeMargin = 2500.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float DropOverviewDistance = 4000.f;
}

bool URailTrooperAnimTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("RailTrooperAnimValidate"));
}

TStatId URailTrooperAnimTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URailTrooperAnimTestSubsystem, STATGROUP_Tickables);
}

void URailTrooperAnimTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case ERailTrooperAnimTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case ERailTrooperAnimTestPhase::WaitingForSpawnedSquad:
		TickWaitingForSpawnedSquad();
		return;
	case ERailTrooperAnimTestPhase::MovingSpawnedSquad:
		TickMovingSpawnedSquad();
		return;
	case ERailTrooperAnimTestPhase::AttackingGround:
		TickAttackingGround();
		return;
	case ERailTrooperAnimTestPhase::RunningParadrop:
		TickRunningParadrop();
		return;
	case ERailTrooperAnimTestPhase::MovingDroppedSquads:
		TickMovingDroppedSquads();
		return;
	case ERailTrooperAnimTestPhase::Finished:
		return;
	}
}

void URailTrooperAnimTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSRailTrooperTest, Error, TEXT("RTS_RAILTROOPER_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float URailTrooperAnimTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void URailTrooperAnimTestSubsystem::EnterPhase(const ERailTrooperAnimTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
	UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST PHASE %s at=%.1f"),
	       *UEnum::GetValueAsString(NewPhase), GetNow());
}

void URailTrooperAnimTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = ERailTrooperAnimTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void URailTrooperAnimTestSubsystem::TickWaitingForStart()
{
	using namespace RailTrooperAnimTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller))
	{
		return;
	}
	// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
	UPlayerStartGameControl* StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_StartSeconds < 0.f)
	{
		M_StartSeconds = GetNow();
		return;
	}
	if (GetNow() - M_StartSeconds < StartDelaySeconds)
	{
		return;
	}
	if (not FindAnchor(M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"));
		Finish();
		return;
	}
	M_LandscapeBounds = FindLandscapeBounds();
	M_Anchor = KeepInsideLandscape(M_Anchor);
	const FVector ToCentre = M_LandscapeBounds.IsValid
		                         ? (M_LandscapeBounds.GetCenter() - M_Anchor).GetSafeNormal2D()
		                         : FVector(0.f, 1.f, 0.f);
	M_TowardCentre = ToCentre.IsNearlyZero() ? FVector(0.f, 1.f, 0.f) : ToCentre;
	UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST LANDSCAPE min=%s max=%s anchor=%s"),
	       *M_LandscapeBounds.Min.ToCompactString(), *M_LandscapeBounds.Max.ToCompactString(),
	       *M_Anchor.ToCompactString());
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	// The first capture of a run comes out black (twice in a row); spend it on a warm-up picture.
	RequestScreenshot(TEXT("WarmUp_Ignore"));
	StartDirectSpawn();
}

bool URailTrooperAnimTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace RailTrooperAnimTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("RailTrooperAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("RailTrooperAnchorY="), AnchorY))
	{
		return GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(AnchorX, AnchorY, 0.f), OutAnchor);
	}
	const UWorld* World = GetWorld();
	const APlayerController* Controller = FRTS_Statics::GetRTSController(World);
	if (not IsValid(Controller) || not IsValid(Controller->PlayerCameraManager))
	{
		return false;
	}
	const FVector CameraLocation = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, CameraLocation, CameraLocation + CameraForward * CameraTraceLength,
	                                        ECC_Visibility))
	{
		return false;
	}
	return GlobalAbilityHelpers::TryFindGroundLocation(this, GroundHit.Location, OutAnchor);
}

void URailTrooperAnimTestSubsystem::StartDirectSpawn()
{
	using namespace RailTrooperAnimTestPrivate;
	EnterPhase(ERailTrooperAnimTestPhase::WaitingForSpawnedSquad);
	const FVector Side(-M_TowardCentre.Y, M_TowardCentre.X, 0.f);
	M_DirectSpawnLocation = KeepInsideLandscape(M_Anchor + Side * DirectSpawnSideOffset);
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	Check(IsValid(Spawner), TEXT("async_spawner_found"));
	if (not IsValid(Spawner))
	{
		Finish();
		return;
	}
	TWeakObjectPtr<URailTrooperAnimTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_RailTroopers)),
		M_DirectSpawnLocation,
		this,
		0,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnDirectSquadSpawned(SpawnedActor);
			}
		},
		FRotator::ZeroRotator);
	Check(bRequested, TEXT("spawner_accepted_rail_troopers_option"));
	UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST SPAWN_REQUESTED at=%s"),
	       *M_DirectSpawnLocation.ToCompactString());
	if (not bRequested)
	{
		Finish();
	}
}

void URailTrooperAnimTestSubsystem::OnDirectSquadSpawned(AActor* SpawnedActor)
{
	M_SpawnedSquad = Cast<ASquadController>(SpawnedActor);
	UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST SPAWNED %s"),
	       IsValid(SpawnedActor) ? *SpawnedActor->GetClass()->GetName() : TEXT("none"));
}

void URailTrooperAnimTestSubsystem::TickWaitingForSpawnedSquad()
{
	using namespace RailTrooperAnimTestPrivate;
	ViewFrom(M_DirectSpawnLocation, 1500.f, -25.f, 225.f);
	ASquadController* Squad = M_SpawnedSquad.Get();
	const bool bLoaded = IsValid(Squad) && Squad->GetIsSquadFullyLoadedAndInitialized();
	if (not bLoaded)
	{
		if (GetNow() - M_PhaseStartSeconds >= SpawnTimeoutSeconds)
		{
			Check(false, TEXT("spawned_squad_loaded"), IsValid(Squad) ? TEXT("not loaded") : TEXT("no squad"));
			Finish();
		}
		return;
	}
	ViewCloseUp(Squad, 205.f);
	const bool bTimedOut = GetNow() - M_PhaseStartSeconds >= SettleTimeoutSeconds;
	// Any trooper still walking into formation restarts the idle window.
	if (not GetAreTroopersStanding(Squad) && not bTimedOut)
	{
		M_IdleSettledSeconds = -1.f;
		return;
	}
	if (M_IdleSettledSeconds < 0.f)
	{
		Check(not bTimedOut, TEXT("spawned_troopers_stand_still_after_spawn"));
		M_IdleSettledSeconds = GetNow();
		ResetPoseTracking();
	}
	SamplePose(Squad);
	if (GetNow() - M_IdleSettledSeconds < IdleSampleSeconds)
	{
		return;
	}
	Check(Squad->GetClass()->GetName().StartsWith(SquadClassMarker), TEXT("spawner_row_class"),
	      Squad->GetClass()->GetName());
	CheckSquad(Squad, TEXT("spawned"));
	CheckTrooperAnimationSetup(Squad, TEXT("spawned"));
	CheckPoseAnimated(TEXT("spawned_idle"), false);
	RequestScreenshot(TEXT("Idle_CloseUp"));

	M_MoveTarget = KeepInsideLandscape(GetSquadCentre(Squad) + M_TowardCentre * MoveDistance);
	const ECommandQueueError MoveResult = Squad->MoveToLocation(M_MoveTarget, true, FRotator::ZeroRotator);
	Check(MoveResult == ECommandQueueError::NoError, TEXT("spawned_move_order_accepted"),
	      UEnum::GetValueAsString(MoveResult));
	M_DirectSpawnLocation = GetSquadCentre(Squad);
	ResetPoseTracking();
	EnterPhase(ERailTrooperAnimTestPhase::MovingSpawnedSquad);
}

void URailTrooperAnimTestSubsystem::TickMovingSpawnedSquad()
{
	using namespace RailTrooperAnimTestPrivate;
	ASquadController* Squad = M_SpawnedSquad.Get();
	if (not IsValid(Squad))
	{
		Check(false, TEXT("spawned_squad_alive_for_move"));
		Finish();
		return;
	}
	const FVector Centre = GetSquadCentre(Squad);
	ViewFrom(Centre + FVector(0.f, 0.f, CloseUpHeight), 750.f, -12.f,
	         M_TowardCentre.Rotation().Yaw + WalkSideViewYawOffset);
	SamplePose(Squad);
	const float ToTarget = FVector::Dist2D(Centre, M_MoveTarget);
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bM_WalkShotTaken && Elapsed >= WalkShotSeconds)
	{
		bM_WalkShotTaken = true;
		RequestScreenshot(TEXT("Walking"));
	}
	if (ToTarget > MoveArrivedRadius && Elapsed < MoveTimeoutSeconds)
	{
		return;
	}
	CheckPoseAnimated(TEXT("spawned_walk"), true);
	Check(ToTarget <= MoveArrivedRadius, TEXT("spawned_reached_move_target"),
	      FString::Printf(TEXT("to_target=%.0f after=%.1fs"), ToTarget, Elapsed));

	const FVector GroundTarget = Centre + FVector(1200.f, 500.f, 0.f);
	FVector GroundTargetOnGround = GroundTarget;
	GlobalAbilityHelpers::TryFindGroundLocation(this, GroundTarget, GroundTargetOnGround);
	const ECommandQueueError AttackResult = Squad->AttackGround(GroundTargetOnGround, true);
	Check(AttackResult == ECommandQueueError::NoError, TEXT("spawned_attack_ground_order_accepted"),
	      UEnum::GetValueAsString(AttackResult));
	ResetPoseTracking();
	EnterPhase(ERailTrooperAnimTestPhase::AttackingGround);
}

void URailTrooperAnimTestSubsystem::TickAttackingGround()
{
	using namespace RailTrooperAnimTestPrivate;
	ASquadController* Squad = M_SpawnedSquad.Get();
	if (not IsValid(Squad))
	{
		Check(false, TEXT("spawned_squad_alive_for_attack"));
		Finish();
		return;
	}
	ViewCloseUp(Squad, 160.f);
	SamplePose(Squad);
	FString Mags;
	for (ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
	{
		const UWeaponState* WeaponState = Unit->GetWeaponState();
		if (not IsValid(WeaponState))
		{
			continue;
		}
		const int32 Full = WeaponState->GetRawWeaponData().MagCapacity;
		const int32 Current = WeaponState->GetCurrentMagCapacity();
		Mags += FString::Printf(TEXT("%d/%d%s "), Current, Full, WeaponState->GetIsReloading() ? TEXT("R") : TEXT(""));
		bM_AttackGroundFired |= Current < Full || WeaponState->GetIsReloading();
	}
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bM_AttackGroundFired && Elapsed < AttackTimeoutSeconds)
	{
		return;
	}
	if (bM_AttackGroundFired && Elapsed < FireSampleSeconds)
	{
		return;
	}
	Check(bM_AttackGroundFired, TEXT("spawned_troopers_fire_on_attack_ground"),
	      FString::Printf(TEXT("after=%.1fs mags=[%s]"), Elapsed, *Mags));
	CheckPoseAnimated(TEXT("spawned_fire"), false);
	RequestScreenshot(TEXT("Firing"));
	Squad->StopCommand();
	StartParadrop();
}

void URailTrooperAnimTestSubsystem::StartParadrop()
{
	using namespace RailTrooperAnimTestPrivate;
	EnterPhase(ERailTrooperAnimTestPhase::RunningParadrop);
	M_DroppedSquads.Reset();
	M_KnownSquads.Reset();
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		M_KnownSquads.Add(*It);
	}
	const FVector Side(-M_TowardCentre.Y, M_TowardCentre.X, 0.f);
	M_DropAnchor = KeepInsideLandscape(M_Anchor - Side * DropSideOffset);

	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	Check(IsValid(Manager) && IsValid(AbilitySet), TEXT("paradrop_ability_manager_and_set"));
	if (not IsValid(Manager) || not IsValid(AbilitySet))
	{
		Finish();
		return;
	}
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	M_Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(ParadropAbility);
	Check(IsValid(M_Ability), TEXT("paradrop_template_found"),
	      IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(M_Ability))
	{
		Finish();
		return;
	}
	M_Ability->InitGlobalAbility(TestPlayer, Manager, Controller);
	M_Ability->ExecuteAbilityAtLocation(M_DropAnchor);
	UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST PARADROP_EXECUTED %s at=%s"),
	       *M_Ability->GetClass()->GetName(), *M_DropAnchor.ToCompactString());
}

void URailTrooperAnimTestSubsystem::CollectNewSquads()
{
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		if (M_KnownSquads.Contains(*It) || M_DroppedSquads.Contains(*It))
		{
			continue;
		}
		const URTSComponent* SquadRTS = It->GetRTSComponent();
		if (IsValid(SquadRTS) && SquadRTS->GetSubtypeAsSquadSubtype() == ESquadSubtype::Squad_Ger_RailTroopers)
		{
			M_DroppedSquads.Add(*It);
			UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST DROPPED_SQUAD %s at=%.1f"),
			       *It->GetName(), GetNow() - M_PhaseStartSeconds);
		}
	}
}

bool URailTrooperAnimTestSubsystem::GetAreDroppedSquadsLanded() const
{
	if (M_DroppedSquads.IsEmpty())
	{
		return false;
	}
	for (const TWeakObjectPtr<ASquadController>& SquadPtr : M_DroppedSquads)
	{
		ASquadController* Squad = SquadPtr.Get();
		if (not IsValid(Squad) || not Squad->GetIsSquadFullyLoadedAndInitialized())
		{
			return false;
		}
		for (const ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
		{
			// Troopers under a parachute are not "falling" (they hang in the parachute montage): only walking counts.
			const UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
			if (not IsValid(Movement) || not Movement->IsMovingOnGround() || Unit->IsHidden())
			{
				return false;
			}
		}
	}
	return true;
}

bool URailTrooperAnimTestSubsystem::GetAreTroopersStanding(ASquadController* Squad)
{
	using namespace RailTrooperAnimTestPrivate;
	for (const ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
	{
		if (IsValid(Unit) && Unit->GetVelocity().Size2D() > StandingSpeed)
		{
			return false;
		}
	}
	return true;
}

void URailTrooperAnimTestSubsystem::TickRunningParadrop()
{
	using namespace RailTrooperAnimTestPrivate;
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	CollectNewSquads();
	ASquadController* FirstDropped = M_DroppedSquads.IsEmpty() ? nullptr : M_DroppedSquads[0].Get();
	if (M_DroppedLandedSeconds < 0.f)
	{
		ViewFrom(M_DropAnchor + FVector(0.f, 0.f, 300.f), DropOverviewDistance, -30.f);
		if (GetAreDroppedSquadsLanded())
		{
			M_DroppedLandedSeconds = GetNow();
			UE_LOG(LogRTSRailTrooperTest, Display, TEXT("RTS_RAILTROOPER_TEST DROP_LANDED squads=%d at=%.1fs"),
			       M_DroppedSquads.Num(), Elapsed);
			RequestScreenshot(TEXT("Paradrop_Landed_Overview"));
		}
		else if (Elapsed >= DropTimeoutSeconds)
		{
			Check(false, TEXT("paradrop_squads_landed_in_time"),
			      FString::Printf(TEXT("squads=%d"), M_DroppedSquads.Num()));
			Finish();
		}
		return;
	}
	if (not IsValid(FirstDropped))
	{
		Check(false, TEXT("paradrop_squad_alive"));
		Finish();
		return;
	}
	ViewCloseUp(FirstDropped, 205.f);
	const float SinceLanded = GetNow() - M_DroppedLandedSeconds;
	if (SinceLanded < LandedSettleSeconds)
	{
		return;
	}
	if (not bM_DropIdleSampled)
	{
		bM_DropIdleSampled = true;
		ResetPoseTracking();
	}
	SamplePose(FirstDropped);
	if (SinceLanded < LandedSettleSeconds + DropIdleSampleSeconds)
	{
		return;
	}
	Check(not M_DroppedSquads.IsEmpty(), TEXT("paradrop_delivered_rail_troopers"),
	      FString::Printf(TEXT("squads=%d"), M_DroppedSquads.Num()));
	int32 SquadIndex = 0;
	for (const TWeakObjectPtr<ASquadController>& SquadPtr : M_DroppedSquads)
	{
		const FString Label = FString::Printf(TEXT("dropped%d"), SquadIndex++);
		if (ASquadController* Squad = SquadPtr.Get())
		{
			CheckSquad(Squad, Label);
			CheckTrooperAnimationSetup(Squad, Label);
		}
	}
	CheckPoseAnimated(TEXT("dropped0_idle"), false);
	RequestScreenshot(TEXT("Paradrop_Idle_CloseUp"));
	StartDroppedSquadsMove();
}

void URailTrooperAnimTestSubsystem::StartDroppedSquadsMove()
{
	using namespace RailTrooperAnimTestPrivate;
	M_DroppedMoveTargets.Reset();
	M_DroppedMoveStarts.Reset();
	for (const TWeakObjectPtr<ASquadController>& SquadPtr : M_DroppedSquads)
	{
		ASquadController* Squad = SquadPtr.Get();
		if (not IsValid(Squad))
		{
			continue;
		}
		const FVector Centre = GetSquadCentre(Squad);
		const FVector Target = KeepInsideLandscape(Centre + M_TowardCentre * MoveDistance);
		const ECommandQueueError MoveResult = Squad->MoveToLocation(Target, true, FRotator::ZeroRotator);
		Check(MoveResult == ECommandQueueError::NoError, Squad->GetName() + TEXT("_move_order_accepted"),
		      UEnum::GetValueAsString(MoveResult));
		M_DroppedMoveTargets.Add(Target);
		M_DroppedMoveStarts.Add(Centre);
	}
	ResetPoseTracking();
	bM_WalkShotTaken = false;
	EnterPhase(ERailTrooperAnimTestPhase::MovingDroppedSquads);
}

void URailTrooperAnimTestSubsystem::TickMovingDroppedSquads()
{
	using namespace RailTrooperAnimTestPrivate;
	ASquadController* FirstDropped = M_DroppedSquads.IsEmpty() ? nullptr : M_DroppedSquads[0].Get();
	if (not IsValid(FirstDropped) || M_DroppedMoveTargets.IsEmpty())
	{
		Check(false, TEXT("dropped0_alive_for_move"));
		Finish();
		return;
	}
	const FVector Centre = GetSquadCentre(FirstDropped);
	ViewFrom(Centre + FVector(0.f, 0.f, CloseUpHeight), 750.f, -12.f,
	         M_TowardCentre.Rotation().Yaw + WalkSideViewYawOffset);
	SamplePose(FirstDropped);
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bM_WalkShotTaken && Elapsed >= WalkShotSeconds)
	{
		bM_WalkShotTaken = true;
		RequestScreenshot(TEXT("Paradrop_Walking"));
	}
	const float ToTarget = FVector::Dist2D(Centre, M_DroppedMoveTargets[0]);
	if (ToTarget > MoveArrivedRadius && Elapsed < MoveTimeoutSeconds)
	{
		return;
	}
	CheckPoseAnimated(TEXT("dropped0_walk"), true);
	Check(ToTarget <= MoveArrivedRadius, TEXT("dropped0_reached_move_target"),
	      FString::Printf(TEXT("to_target=%.0f after=%.1fs"), ToTarget, Elapsed));
	Finish();
}

void URailTrooperAnimTestSubsystem::CheckSquad(ASquadController* Squad, const FString& Label)
{
	using namespace RailTrooperAnimTestPrivate;
	const URTSComponent* SquadRTS = Squad->GetRTSComponent();
	const ESquadSubtype Subtype = IsValid(SquadRTS) ? SquadRTS->GetSubtypeAsSquadSubtype() : ESquadSubtype::Squad_None;
	Check(Subtype == ESquadSubtype::Squad_Ger_RailTroopers, Label + TEXT("_subtype"), UEnum::GetValueAsString(Subtype));
	const int32 Owner = IsValid(SquadRTS) ? SquadRTS->GetOwningPlayer() : -1;
	Check(Owner == TestPlayer, Label + TEXT("_owned_by_player_1"), FString::Printf(TEXT("%d"), Owner));
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	Check(Units.Num() == ExpectedTroopers, Label + TEXT("_trooper_count"), FString::Printf(TEXT("%d"), Units.Num()));
	int32 ClassMatches = 0;
	int32 WeaponMatches = 0;
	FString Details;
	for (const ASquadUnit* Unit : Units)
	{
		const FString UnitClass = Unit->GetClass()->GetName();
		ClassMatches += UnitClass.StartsWith(TrooperClassMarker) ? 1 : 0;
		const AInfantryWeaponMaster* Weapon = Unit->GetInfantryWeapon();
		const FString WeaponClass = IsValid(Weapon) ? Weapon->GetClass()->GetName() : TEXT("none");
		const UWeaponState* WeaponState = Unit->GetWeaponState();
		const EWeaponName WeaponName = IsValid(WeaponState)
			                               ? WeaponState->GetRawWeaponData().WeaponName
			                               : EWeaponName::DEFAULT_WEAPON;
		WeaponMatches += WeaponClass.Contains(WeaponClassMarker) && WeaponName == ExpectedWeapon ? 1 : 0;
		Details += FString::Printf(TEXT("%s/%s "), *UnitClass, *WeaponClass);
	}
	const bool bHasUnits = Units.Num() > 0;
	Check(bHasUnits && ClassMatches == Units.Num(), Label + TEXT("_units_are_rail_troopers"), Details);
	Check(bHasUnits && WeaponMatches == Units.Num(), Label + TEXT("_every_trooper_has_m920"), Details);
}

void URailTrooperAnimTestSubsystem::CheckTrooperAnimationSetup(ASquadController* Squad, const FString& Label)
{
	using namespace RailTrooperAnimTestPrivate;
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	int32 MeshMatches = 0;
	int32 SkeletonMatches = 0;
	int32 AnimMatches = 0;
	int32 WeaponInHand = 0;
	for (const ASquadUnit* Unit : Units)
	{
		const USkeletalMeshComponent* Mesh = Unit->GetMesh();
		const USkeletalMesh* MeshAsset = IsValid(Mesh) ? Mesh->GetSkeletalMeshAsset() : nullptr;
		const USkeleton* Skeleton = IsValid(MeshAsset) ? MeshAsset->GetSkeleton() : nullptr;
		const UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
		const FString MeshName = IsValid(MeshAsset) ? MeshAsset->GetName() : TEXT("none");
		const FString SkeletonName = IsValid(Skeleton) ? Skeleton->GetName() : TEXT("none");
		const FString AnimName = IsValid(AnimInstance) ? AnimInstance->GetClass()->GetName() : TEXT("none");
		MeshMatches += MeshName.StartsWith(MeshMarker) ? 1 : 0;
		SkeletonMatches += SkeletonName.StartsWith(SkeletonMarker) ? 1 : 0;
		AnimMatches += AnimName.StartsWith(AnimClassMarker) ? 1 : 0;

		const AInfantryWeaponMaster* Weapon = Unit->GetInfantryWeapon();
		const USceneComponent* WeaponHolder = IsValid(Weapon) ? Weapon->GetParentComponent() : nullptr;
		const FName WeaponSocket = IsValid(WeaponHolder) ? WeaponHolder->GetAttachSocketName() : NAME_None;
		const FVector HandLocation = IsValid(Mesh) ? Mesh->GetBoneLocation(HandBone) : FVector::ZeroVector;
		const float WeaponToHand = IsValid(Weapon)
			                           ? static_cast<float>(FVector::Dist(Weapon->GetActorLocation(), HandLocation))
			                           : -1.f;
		WeaponInHand += WeaponToHand >= 0.f && WeaponToHand <= MaxWeaponToHandDistance ? 1 : 0;
		UE_LOG(LogRTSRailTrooperTest, Display,
		       TEXT("RTS_RAILTROOPER_TEST ANIM %s %s mesh=%s skeleton=%s anim_instance=%s weapon_socket=%s "
			       "weapon_to_hand=%.1f"),
		       *Label, *Unit->GetName(), *MeshName, *SkeletonName, *AnimName, *WeaponSocket.ToString(), WeaponToHand);
	}
	const bool bHasUnits = Units.Num() > 0;
	Check(bHasUnits && MeshMatches == Units.Num(), Label + TEXT("_troopers_use_sk_rail_trooper"),
	      FString::Printf(TEXT("%d/%d"), MeshMatches, Units.Num()));
	Check(bHasUnits && SkeletonMatches == Units.Num(), Label + TEXT("_mesh_on_infantry_master_skeleton"),
	      FString::Printf(TEXT("%d/%d"), SkeletonMatches, Units.Num()));
	Check(bHasUnits && AnimMatches == Units.Num(), Label + TEXT("_squad_anim_bp_running"),
	      FString::Printf(TEXT("%d/%d"), AnimMatches, Units.Num()));
	Check(bHasUnits && WeaponInHand == Units.Num(), Label + TEXT("_m920_in_right_hand"),
	      FString::Printf(TEXT("%d/%d within %.0f cm of hand_r"), WeaponInHand, Units.Num(), MaxWeaponToHandDistance));
}

void URailTrooperAnimTestSubsystem::CheckPoseAnimated(const FString& Label, const bool bExpectWalkCycle)
{
	using namespace RailTrooperAnimTestPrivate;
	LogPoseTracking(Label);
	Check(M_MaxHandOffsetFromRefPose >= MinHandOffsetFromRefPose, Label + TEXT("_not_reference_pose"),
	      FString::Printf(TEXT("hand_offset_from_ref_pose=%.1f cm"), M_MaxHandOffsetFromRefPose));
	if (not bExpectWalkCycle)
	{
		Check(M_MaxHandTravel >= MinAnimatedHandTravel, Label + TEXT("_pose_animated"),
		      FString::Printf(TEXT("hand_travel=%.2f foot_travel=%.2f"), M_MaxHandTravel, M_MaxFootTravel));
		return;
	}
	Check(M_MaxTrooperSpeed > MinWalkSpeed, Label + TEXT("_troopers_have_walk_speed"),
	      FString::Printf(TEXT("max_speed=%.0f"), M_MaxTrooperSpeed));
	Check(M_MaxFootTravel >= MinWalkFootTravel, Label + TEXT("_walk_cycle_moves_feet"),
	      FString::Printf(TEXT("foot_travel=%.1f cm (component space)"), M_MaxFootTravel));
}

void URailTrooperAnimTestSubsystem::ResetPoseTracking()
{
	M_MaxFootTravel = 0.f;
	M_MaxHandTravel = 0.f;
	M_MaxHandOffsetFromRefPose = 0.f;
	M_MaxTrooperSpeed = 0.f;
	M_FootStartPositions.Reset();
	M_HandStartPositions.Reset();
	M_MontagesSeen.Reset();
}

void URailTrooperAnimTestSubsystem::SamplePose(ASquadController* Squad)
{
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	const bool bFirstSample = M_FootStartPositions.Num() != Units.Num();
	if (bFirstSample)
	{
		M_FootStartPositions.Reset();
		M_HandStartPositions.Reset();
	}
	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		if (IsValid(Units[Index]))
		{
			SampleTrooperPose(*Units[Index], Index, bFirstSample);
		}
	}
}

void URailTrooperAnimTestSubsystem::SampleTrooperPose(const ASquadUnit& Unit, const int32 Index, const bool bFirstSample)
{
	using namespace RailTrooperAnimTestPrivate;
	const USkeletalMeshComponent* Mesh = Unit.GetMesh();
	const USkeletalMesh* MeshAsset = IsValid(Mesh) ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (not IsValid(MeshAsset))
	{
		return;
	}
	const FVector Foot = Mesh->GetBoneLocation(FootBone, EBoneSpaces::ComponentSpace);
	const FVector Hand = Mesh->GetBoneLocation(HandBone, EBoneSpaces::ComponentSpace);
	if (bFirstSample)
	{
		M_FootStartPositions.Add(Foot);
		M_HandStartPositions.Add(Hand);
		return;
	}
	if (not M_FootStartPositions.IsValidIndex(Index) || not M_HandStartPositions.IsValidIndex(Index))
	{
		return;
	}
	M_MaxFootTravel = FMath::Max(M_MaxFootTravel, static_cast<float>(FVector::Dist(Foot, M_FootStartPositions[Index])));
	M_MaxHandTravel = FMath::Max(M_MaxHandTravel, static_cast<float>(FVector::Dist(Hand, M_HandStartPositions[Index])));
	M_MaxTrooperSpeed = FMath::Max(M_MaxTrooperSpeed, static_cast<float>(Unit.GetVelocity().Size2D()));
	const int32 HandIndex = MeshAsset->GetRefSkeleton().FindBoneIndex(HandBone);
	if (HandIndex != INDEX_NONE)
	{
		const FVector RefHand = FAnimationRuntime::GetComponentSpaceTransformRefPose(
			MeshAsset->GetRefSkeleton(), HandIndex).GetLocation();
		M_MaxHandOffsetFromRefPose = FMath::Max(M_MaxHandOffsetFromRefPose,
		                                        static_cast<float>(FVector::Dist(Hand, RefHand)));
	}
	const UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	const UAnimMontage* Montage = IsValid(AnimInstance) ? AnimInstance->GetCurrentActiveMontage() : nullptr;
	if (IsValid(Montage))
	{
		M_MontagesSeen.Add(Montage->GetName());
	}
}

void URailTrooperAnimTestSubsystem::LogPoseTracking(const FString& Label) const
{
	FString Montages;
	for (const FString& Montage : M_MontagesSeen)
	{
		Montages += Montage + TEXT(" ");
	}
	UE_LOG(LogRTSRailTrooperTest, Display,
	       TEXT("RTS_RAILTROOPER_TEST POSE %s foot_travel=%.2f hand_travel=%.2f hand_offset_from_ref=%.1f "
		       "max_speed=%.0f montages=[%s]"),
	       *Label, M_MaxFootTravel, M_MaxHandTravel, M_MaxHandOffsetFromRefPose, M_MaxTrooperSpeed, *Montages);
}

FBox URailTrooperAnimTestSubsystem::FindLandscapeBounds() const
{
	FBox Bounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		Bounds += It->GetComponentsBoundingBox(true);
	}
	return Bounds;
}

FVector URailTrooperAnimTestSubsystem::KeepInsideLandscape(const FVector& Location) const
{
	using namespace RailTrooperAnimTestPrivate;
	FVector Clamped = Location;
	if (M_LandscapeBounds.IsValid)
	{
		Clamped.X = FMath::Clamp(Location.X, M_LandscapeBounds.Min.X + LandscapeEdgeMargin,
		                         M_LandscapeBounds.Max.X - LandscapeEdgeMargin);
		Clamped.Y = FMath::Clamp(Location.Y, M_LandscapeBounds.Min.Y + LandscapeEdgeMargin,
		                         M_LandscapeBounds.Max.Y - LandscapeEdgeMargin);
	}
	FVector OnGround = Clamped;
	GlobalAbilityHelpers::TryFindGroundLocation(this, Clamped, OnGround);
	return OnGround;
}

FVector URailTrooperAnimTestSubsystem::GetSquadCentre(ASquadController* Squad)
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
	{
		Sum += Unit->GetActorLocation();
		++Count;
	}
	return Count > 0 ? Sum / Count : Squad->GetActorLocation();
}

void URailTrooperAnimTestSubsystem::ViewFrom(const FVector& Focus, const float Distance, const float Pitch,
                                             const float Yaw) const
{
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(Pitch, Yaw, 0.f);
	M_Camera->SetActorLocationAndRotation(Focus - ViewRotation.Vector() * Distance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void URailTrooperAnimTestSubsystem::ViewCloseUp(ASquadController* Squad, const float Yaw) const
{
	using namespace RailTrooperAnimTestPrivate;
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	if (Units.IsEmpty() || not IsValid(Units[0]))
	{
		return;
	}
	ViewFrom(Units[0]->GetActorLocation() + FVector(0.f, 0.f, CloseUpHeight - 60.f), CloseUpDistance, -8.f, Yaw);
}

void URailTrooperAnimTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("RailTrooperSkeletonTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool URailTrooperAnimTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void URailTrooperAnimTestSubsystem::Tick(float DeltaTime) {}
TStatId URailTrooperAnimTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URailTrooperAnimTestSubsystem, STATGROUP_Tickables);
}

#endif
