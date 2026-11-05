// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FelsKriegerTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "LandscapeProxy.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/UnitDataHelpers/UnitDataHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftDrop/GA_AircraftDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFelsKriegerTest, Log, All);

namespace FelsKriegerTestPrivate
{
	struct FFelsKriegerDropCase
	{
		EGlobalAbility Ability;
		const TCHAR* Label;
		int32 SquadCount;
		EGlobalAbilityTier Tier;
		int32 CooldownMin;
		int32 CooldownMax;
	};

	const FFelsKriegerDropCase DropCases[] = {
		{EGlobalAbility::GA_GerFelsKriegerDrop, TEXT("FelsKriegerDrop"), 2, EGlobalAbilityTier::TierIII, 180, 240},
	};
	constexpr int32 DropCaseCount = UE_ARRAY_COUNT(DropCases);

	constexpr int32 TestPlayer = 1;
	constexpr int32 ExpectedWalkers = 3;
	const TCHAR* const WalkerClassMarker = TEXT("Ger_Inf_Walker_FelsKrieger");
	const TCHAR* const SquadClassMarker = TEXT("Squad_Ger_FelsKrieger");
	const TCHAR* const WeaponClassMarker = TEXT("RipperGun");
	constexpr EWeaponName ExpectedWeapon = EWeaponName::RipperGun_7_62MM;
	const TCHAR* const MeshMarker = TEXT("SK_FelsKrieger");
	const TCHAR* const SkeletonMarker = TEXT("InfantryMaster_Skeleton");
	const TCHAR* const AnimClassMarker = TEXT("AnimBp_SquadUnit");
	const FName FootBone(TEXT("foot_l"));
	const FName HandBone(TEXT("hand_r"));
	// Component-space travel of the left foot during a walk cycle is tens of centimetres; a frozen pose stays at 0.
	constexpr float MinWalkFootTravel = 15.f;
	// Idle breathing and recoil move the right hand a little; anything above this proves the pose is animated.
	constexpr float MinAnimatedHandTravel = 0.5f;
	constexpr float CloseUpDistance = 520.f;
	// Half the slow infantry pace: a walker moving at least this fast is walking, not sliding into place.
	constexpr float MinWalkSpeed = 200.f;
	const TCHAR* const DropAircraftMarker = TEXT("JU390");
	// Drops cost about 125% of the squads' radixite training cost (radixite only, user rule 2026-11-05).
	constexpr float DropCostFactor = 1.25f;
	constexpr float DropCostTolerance = 0.06f;

	constexpr float StartDelaySeconds = 3.f;
	constexpr float SpawnTimeoutSeconds = 30.f;
	constexpr float MoveTimeoutSeconds = 30.f;
	constexpr float MoveDistance = 1500.f;
	constexpr float MoveArrivedRadius = 450.f;
	constexpr float MinMovedDistance = 1000.f;
	constexpr float AttackTimeoutSeconds = 15.f;
	constexpr float DropTimeoutSeconds = 240.f;
	// After the squads of a drop are complete: the landed check, then the take-off has to be over.
	constexpr float LandedCheckDelaySeconds = 2.5f;
	constexpr float AfterTakeOffSeconds = 20.f;
	// The directly spawned squad stands this far to the side of the drop point (across the line to the map centre).
	constexpr float DirectSpawnSideOffset = 3500.f;
	// The drop lands this far from the anchor, on the side away from the directly spawned squad. Not towards the
	// map centre: RTS_ToCopySmall has armed gun pits there that shot a Red Spine squad (its run 3).
	constexpr float DefaultDropOffset = 4500.f;
	// Every test location stays this far inside the landscape: RTS_ToCopySmall's -Y edge is close to the anchor.
	constexpr float LandscapeEdgeMargin = 2500.f;
	constexpr float CameraTraceLength = 500000.f;
	// Feet within this of the ground below the walker count as standing on it.
	constexpr float FootGroundTolerance = 60.f;
	constexpr float GroundTraceLength = 5000.f;
	// A walker's capsule radius: a walker closer than this to the airframe's footprint stands in or under it.
	constexpr float WalkerClearance = 42.f;
	constexpr float MaxLandingDistance = 3500.f;
}

bool UFelsKriegerTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("FelsKriegerValidate"));
}

TStatId UFelsKriegerTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFelsKriegerTestSubsystem, STATGROUP_Tickables);
}

void UFelsKriegerTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EFelsKriegerTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EFelsKriegerTestPhase::WaitingForSpawnedSquad:
		TickWaitingForSpawnedSquad();
		return;
	case EFelsKriegerTestPhase::MovingSpawnedSquad:
		TickMovingSpawnedSquad();
		return;
	case EFelsKriegerTestPhase::AttackingGround:
		TickAttackingGround();
		return;
	case EFelsKriegerTestPhase::RunningDrop:
		TickRunningDrop();
		return;
	case EFelsKriegerTestPhase::MovingDroppedSquads:
		TickMovingDroppedSquads();
		return;
	case EFelsKriegerTestPhase::Finished:
		return;
	}
}

void UFelsKriegerTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSFelsKriegerTest, Error, TEXT("RTS_FELSKRIEGER_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UFelsKriegerTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UFelsKriegerTestSubsystem::EnterPhase(const EFelsKriegerTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
	UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST PHASE %s at=%.1f"), *UEnum::GetValueAsString(NewPhase),
	       GetNow());
}

void UFelsKriegerTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EFelsKriegerTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UFelsKriegerTestSubsystem::TickWaitingForStart()
{
	using namespace FelsKriegerTestPrivate;
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
	UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST LANDSCAPE min=%s max=%s anchor=%s to_centre=%s"),
	       *M_LandscapeBounds.Min.ToCompactString(), *M_LandscapeBounds.Max.ToCompactString(),
	       *M_Anchor.ToCompactString(), *M_TowardCentre.ToCompactString());
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	StartDirectSpawn();
}

bool UFelsKriegerTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace FelsKriegerTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("FelsKriegerAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("FelsKriegerAnchorY="), AnchorY))
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

void UFelsKriegerTestSubsystem::StartDirectSpawn()
{
	using namespace FelsKriegerTestPrivate;
	EnterPhase(EFelsKriegerTestPhase::WaitingForSpawnedSquad);
	const FVector Side(-M_TowardCentre.Y, M_TowardCentre.X, 0.f);
	M_DirectSpawnLocation = KeepInsideLandscape(M_Anchor + Side * DirectSpawnSideOffset);
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	Check(IsValid(Spawner), TEXT("async_spawner_found"));
	if (not IsValid(Spawner))
	{
		Finish();
		return;
	}
	TWeakObjectPtr<UFelsKriegerTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_FelsKrieger)),
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
	Check(bRequested, TEXT("spawner_accepted_fels_krieger_option"));
	UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST SPAWN_REQUESTED anchor=%s at=%s"),
	       *M_Anchor.ToCompactString(), *M_DirectSpawnLocation.ToCompactString());
	if (not bRequested)
	{
		Finish();
	}
}

void UFelsKriegerTestSubsystem::OnDirectSquadSpawned(AActor* SpawnedActor)
{
	M_SpawnedSquad = Cast<ASquadController>(SpawnedActor);
	UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST SPAWNED %s"),
	       IsValid(SpawnedActor) ? *SpawnedActor->GetClass()->GetName() : TEXT("none"));
}

void UFelsKriegerTestSubsystem::TickWaitingForSpawnedSquad()
{
	using namespace FelsKriegerTestPrivate;
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
	// Close-up on the first walker, chest high, while the squad stands idle.
	const TArray<ASquadUnit*> IdleUnits = Squad->GetSquadUnitsChecked();
	if (not IdleUnits.IsEmpty())
	{
		ViewFrom(IdleUnits[0]->GetActorLocation() + FVector(0.f, 0.f, 60.f), CloseUpDistance, -8.f, 205.f);
	}
	if (M_FootStartPositions.IsEmpty())
	{
		ResetPoseTracking();
	}
	SamplePose(Squad);
	// Give the walkers a moment to leave the spawn placement and stand still for the screenshot.
	if (GetNow() - M_PhaseStartSeconds < 4.f)
	{
		return;
	}
	Check(true, TEXT("spawned_squad_loaded"), Squad->GetClass()->GetName());
	Check(Squad->GetClass()->GetName().StartsWith(SquadClassMarker), TEXT("spawner_row_class"),
	      Squad->GetClass()->GetName());
	CheckSquad(Squad, TEXT("spawned"));
	CheckWalkerAnimation(Squad, TEXT("spawned"));
	CheckWalkersOnGround(Squad, TEXT("spawned"));
	LogPoseTracking(TEXT("idle"));
	Check(M_MaxHandTravel >= MinAnimatedHandTravel, TEXT("spawned_idle_pose_animated"),
	      FString::Printf(TEXT("hand_travel=%.2f foot_travel=%.2f"), M_MaxHandTravel, M_MaxFootTravel));
	RequestScreenshot(TEXT("CloseUp"));

	M_MoveTarget = KeepInsideLandscape(GetSquadCentre(Squad) + M_TowardCentre * MoveDistance);
	const ECommandQueueError MoveResult = Squad->MoveToLocation(M_MoveTarget, true, FRotator::ZeroRotator);
	Check(MoveResult == ECommandQueueError::NoError, TEXT("spawned_move_order_accepted"),
	      UEnum::GetValueAsString(MoveResult));
	M_DirectSpawnLocation = GetSquadCentre(Squad);
	ResetPoseTracking();
	EnterPhase(EFelsKriegerTestPhase::MovingSpawnedSquad);
}

void UFelsKriegerTestSubsystem::TickMovingSpawnedSquad()
{
	using namespace FelsKriegerTestPrivate;
	ASquadController* Squad = M_SpawnedSquad.Get();
	if (not IsValid(Squad))
	{
		Check(false, TEXT("spawned_squad_alive_for_move"));
		Finish();
		return;
	}
	const FVector Centre = GetSquadCentre(Squad);
	ViewFrom(Centre + FVector(0.f, 0.f, 60.f), 1000.f, -15.f, 225.f);
	SamplePose(Squad);
	const float ToTarget = FVector::Dist2D(Centre, M_MoveTarget);
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bM_WalkShotTaken && Elapsed >= 1.5f)
	{
		bM_WalkShotTaken = true;
		RequestScreenshot(TEXT("Walking"));
	}
	if (ToTarget > MoveArrivedRadius && Elapsed < MoveTimeoutSeconds)
	{
		return;
	}
	LogPoseTracking(TEXT("walk"));
	Check(M_MaxWalkerSpeed > MinWalkSpeed, TEXT("spawned_walkers_have_walk_speed"),
	      FString::Printf(TEXT("max_speed=%.0f"), M_MaxWalkerSpeed));
	Check(M_MaxFootTravel >= MinWalkFootTravel, TEXT("spawned_walk_cycle_moves_feet"),
	      FString::Printf(TEXT("foot_travel=%.1f cm (component space)"), M_MaxFootTravel));
	const float Moved = FVector::Dist2D(Centre, M_DirectSpawnLocation);
	Check(ToTarget <= MoveArrivedRadius, TEXT("spawned_reached_move_target"),
	      FString::Printf(TEXT("to_target=%.0f moved=%.0f after=%.1fs"), ToTarget, Moved, Elapsed));
	Check(Moved >= MinMovedDistance, TEXT("spawned_walkers_moved"), FString::Printf(TEXT("%.0f cm"), Moved));
	CheckWalkersOnGround(Squad, TEXT("spawned_after_move"));
	RequestScreenshot(TEXT("SpawnedAfterMove"));

	const FVector GroundTarget = Centre + FVector(1200.f, 500.f, 0.f);
	FVector GroundTargetOnGround = GroundTarget;
	GlobalAbilityHelpers::TryFindGroundLocation(this, GroundTarget, GroundTargetOnGround);
	const ECommandQueueError AttackResult = Squad->AttackGround(GroundTargetOnGround, true);
	Check(AttackResult == ECommandQueueError::NoError, TEXT("spawned_attack_ground_order_accepted"),
	      UEnum::GetValueAsString(AttackResult));
	ResetPoseTracking();
	EnterPhase(EFelsKriegerTestPhase::AttackingGround);
}

void UFelsKriegerTestSubsystem::TickAttackingGround()
{
	using namespace FelsKriegerTestPrivate;
	ASquadController* Squad = M_SpawnedSquad.Get();
	if (not IsValid(Squad))
	{
		Check(false, TEXT("spawned_squad_alive_for_attack"));
		Finish();
		return;
	}
	ViewFrom(GetSquadCentre(Squad) + FVector(0.f, 0.f, 60.f), 900.f, -12.f, 160.f);
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
		if (Current < Full || WeaponState->GetIsReloading())
		{
			bM_AttackGroundFired = true;
		}
	}
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (bM_AttackGroundFired && Elapsed >= 2.f)
	{
		RequestScreenshot(TEXT("SpawnedAttackGround"));
	}
	if (not bM_AttackGroundFired && Elapsed < AttackTimeoutSeconds)
	{
		return;
	}
	if (bM_AttackGroundFired && Elapsed < 2.f)
	{
		return;
	}
	Check(bM_AttackGroundFired, TEXT("spawned_walkers_fire_on_attack_ground"),
	      FString::Printf(TEXT("after=%.1fs mags=[%s]"), Elapsed, *Mags));
	LogPoseTracking(TEXT("fire"));
	Check(M_MaxHandTravel >= MinAnimatedHandTravel, TEXT("spawned_fire_pose_animated"),
	      FString::Printf(TEXT("hand_travel=%.2f"), M_MaxHandTravel));
	Squad->StopCommand();
	StartDrop(0);
}

void UFelsKriegerTestSubsystem::CheckSquad(ASquadController* Squad, const FString& Label)
{
	using namespace FelsKriegerTestPrivate;
	const URTSComponent* SquadRTS = Squad->GetRTSComponent();
	const ESquadSubtype Subtype = IsValid(SquadRTS) ? SquadRTS->GetSubtypeAsSquadSubtype() : ESquadSubtype::Squad_None;
	Check(Subtype == ESquadSubtype::Squad_Ger_FelsKrieger, Label + TEXT("_subtype"), UEnum::GetValueAsString(Subtype));
	const int32 Owner = IsValid(SquadRTS) ? SquadRTS->GetOwningPlayer() : -1;
	Check(Owner == TestPlayer, Label + TEXT("_owned_by_player_1"), FString::Printf(TEXT("%d"), Owner));
	Check(Squad->GetIsSquadFullyLoadedAndInitialized(), Label + TEXT("_fully_loaded"));

	const ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	const FSquadData Data = IsValid(GameState)
		                        ? GameState->GetSquadDataOfPlayer(TestPlayer, ESquadSubtype::Squad_Ger_FelsKrieger)
		                        : FSquadData();
	const float ExpectedHealth = DeveloperSettings::GameBalance::UnitHealth::FelsKriegerWalkerHealth;
	Check(FMath::IsNearlyEqual(Data.MaxHealth, ExpectedHealth, 0.5f), Label + TEXT("_squad_data_health"),
	      FString::Printf(TEXT("data=%.1f expected=%.1f"), Data.MaxHealth, ExpectedHealth));
	// Squad soldiers take the squad data's resistances unless their Blueprint overrides them with a preset.
	const FResistanceAndDamageReductionData HeavyArmour =
		FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(ExpectedHealth);
	Check(Data.ResistancesAndDamageMlt.TargetTypeIcon == HeavyArmour.TargetTypeIcon,
	      Label + TEXT("_squad_data_heavy_infantry_armour"),
	      UEnum::GetValueAsString(Data.ResistancesAndDamageMlt.TargetTypeIcon));

	TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	Check(Units.Num() == ExpectedWalkers, Label + TEXT("_walker_count"), FString::Printf(TEXT("%d"), Units.Num()));
	int32 WalkerClassCount = 0;
	int32 WeaponCount = 0;
	int32 HealthMatches = 0;
	int32 ArmourMatches = 0;
	int32 OwnerMatches = 0;
	FString Details;
	for (ASquadUnit* Unit : Units)
	{
		const FString UnitClass = Unit->GetClass()->GetName();
		WalkerClassCount += UnitClass.StartsWith(WalkerClassMarker) ? 1 : 0;
		const AInfantryWeaponMaster* Weapon = Unit->GetInfantryWeapon();
		const FString WeaponClass = IsValid(Weapon) ? Weapon->GetClass()->GetName() : TEXT("none");
		const UWeaponState* WeaponState = Unit->GetWeaponState();
		const EWeaponName WeaponName = IsValid(WeaponState)
			                               ? WeaponState->GetRawWeaponData().WeaponName
			                               : EWeaponName::DEFAULT_WEAPON;
		WeaponCount += WeaponClass.Contains(WeaponClassMarker) && WeaponName == ExpectedWeapon ? 1 : 0;
		const UHealthComponent* Health = Unit->GetHealthComponent();
		const float MaxHealth = IsValid(Health) ? Health->GetMaxHealth() : -1.f;
		HealthMatches += FMath::IsNearlyEqual(MaxHealth, ExpectedHealth, 0.5f) ? 1 : 0;
		ArmourMatches += GetHasNoResistanceOverride(Unit) ? 1 : 0;
		const URTSComponent* UnitRTS = Unit->FindComponentByClass<URTSComponent>();
		OwnerMatches += IsValid(UnitRTS) && UnitRTS->GetOwningPlayer() == TestPlayer ? 1 : 0;
		Details += FString::Printf(TEXT("%s/%s(%s)/%.0fhp "), *UnitClass, *WeaponClass,
		                           *Global_GetWeaponDisplayName(WeaponName), MaxHealth);
	}
	const bool bHasUnits = Units.Num() > 0;
	Check(bHasUnits && WalkerClassCount == Units.Num(), Label + TEXT("_units_are_fels_krieger_walkers"), Details);
	Check(bHasUnits && WeaponCount == Units.Num(), Label + TEXT("_every_walker_has_ripper_gun"), Details);
	Check(bHasUnits && HealthMatches == Units.Num(), Label + TEXT("_walker_health"),
	      FString::Printf(TEXT("expected=%.0f %s"), ExpectedHealth, *Details));
	Check(bHasUnits && ArmourMatches == Units.Num(), Label + TEXT("_walkers_keep_squad_armour_no_override"),
	      FString::Printf(TEXT("%d/%d"), ArmourMatches, Units.Num()));
	Check(bHasUnits && OwnerMatches == Units.Num(), Label + TEXT("_walkers_owned_by_player_1"),
	      FString::Printf(TEXT("%d/%d"), OwnerMatches, Units.Num()));
}

void UFelsKriegerTestSubsystem::CheckWalkersOnGround(ASquadController* Squad, const FString& Label)
{
	using namespace FelsKriegerTestPrivate;
	UWorld* World = GetWorld();
	int32 Grounded = 0;
	FString Details;
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	for (ASquadUnit* Unit : Units)
	{
		const UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
		const bool bFalling = not IsValid(Movement) || Movement->IsFalling();
		const UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
		const float HalfHeight = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
		const FVector Location = Unit->GetActorLocation();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FelsKriegerGround), false);
		for (TActorIterator<ASquadUnit> It(World); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		const FVector TraceEnd = Location - FVector(0.f, 0.f, GroundTraceLength);
		FHitResult GroundHit;
		const bool bHit = World->LineTraceSingleByChannel(GroundHit, Location + FVector(0.f, 0.f, GroundTraceLength),
		                                                  TraceEnd, COLLISION_TRACE_LANDSCAPE, Params);
		FHitResult BelowHit;
		World->LineTraceSingleByChannel(BelowHit, Location, TraceEnd, ECC_Visibility, Params);
		const AActor* Below = BelowHit.GetActor();
		const bool bOnAircraft = IsValid(Below) && Below->IsA<AAircraftMaster>();
		const float FeetAboveGround = bHit ? (Location.Z - HalfHeight) - GroundHit.ImpactPoint.Z : 99999.f;
		const bool bFeetOnGround = FMath::Abs(FeetAboveGround) <= FootGroundTolerance;
		if (not bFalling && bHit && not bOnAircraft && bFeetOnGround && not Unit->IsHidden())
		{
			++Grounded;
		}
		Details += FString::Printf(TEXT("[%s feet=%.0f below=%s %s] "), bFalling ? TEXT("falling") : TEXT("walking"),
		                           FeetAboveGround, IsValid(Below) ? *Below->GetClass()->GetName() : TEXT("nothing"),
		                           Unit->IsHidden() ? TEXT("hidden") : TEXT("shown"));
	}
	Check(Units.Num() > 0 && Grounded == Units.Num(), Label + TEXT("_walkers_on_ground"), Details);
}

void UFelsKriegerTestSubsystem::CheckWalkersOutsideAircraft(const FString& Label)
{
	using namespace FelsKriegerTestPrivate;
	UWorld* World = GetWorld();
	AAircraftMaster* Aircraft = M_DropAircraft.Get();
	Check(IsValid(Aircraft), Label + TEXT("_aircraft_present_while_landed"));
	if (not IsValid(Aircraft))
	{
		return;
	}
	const FTransform AircraftTransform = Aircraft->GetActorTransform();
	// The Ju 390 mesh has no collision of its own, so the visible airframe (wings included) is the footprint.
	const FBox LocalBox = Aircraft->CalculateComponentsBoundingBoxInLocalSpace(true);
	UE_LOG(LogRTSFelsKriegerTest, Display,
	       TEXT("RTS_FELSKRIEGER_TEST AIRCRAFT %s at=%s rot=%s airborne=%d local_box_min=%s max=%s"),
	       *Aircraft->GetClass()->GetName(), *Aircraft->GetActorLocation().ToCompactString(),
	       *Aircraft->GetActorRotation().ToCompactString(), Aircraft->GetIsAircraftAirborne() ? 1 : 0,
	       *LocalBox.Min.ToCompactString(), *LocalBox.Max.ToCompactString());

	int32 Outside = 0;
	int32 Total = 0;
	FString Details;
	for (const TWeakObjectPtr<ASquadController>& SquadPtr : M_DroppedSquads)
	{
		ASquadController* Squad = SquadPtr.Get();
		if (not IsValid(Squad))
		{
			continue;
		}
		for (ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
		{
			++Total;
			const UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
			bool bOverlapsAircraft = false;
			if (IsValid(Capsule))
			{
				TArray<FOverlapResult> Overlaps;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(FelsKriegerAircraftOverlap), false, Unit);
				World->OverlapMultiByChannel(Overlaps, Capsule->GetComponentLocation(), Capsule->GetComponentQuat(),
				                             ECC_Pawn, Capsule->GetCollisionShape(), Params);
				for (const FOverlapResult& Overlap : Overlaps)
				{
					bOverlapsAircraft |= Overlap.GetActor() == Aircraft;
				}
			}
			const FVector Local = AircraftTransform.InverseTransformPosition(Unit->GetActorLocation());
			const bool bInsideFuselage = Local.Y > LocalBox.Min.Y - WalkerClearance
				&& Local.Y < LocalBox.Max.Y + WalkerClearance
				&& Local.X > LocalBox.Min.X - WalkerClearance && Local.X < LocalBox.Max.X + WalkerClearance;
			Outside += not bOverlapsAircraft && not bInsideFuselage ? 1 : 0;
			Details += FString::Printf(TEXT("[local=%s overlap=%d under_airframe=%d] "), *Local.ToCompactString(),
			                           bOverlapsAircraft ? 1 : 0, bInsideFuselage ? 1 : 0);
		}
	}
	Check(Total > 0 && Outside == Total, Label + TEXT("_walkers_outside_aircraft"), Details);
}

void UFelsKriegerTestSubsystem::StartDrop(const int32 DropIndex)
{
	using namespace FelsKriegerTestPrivate;
	M_DropIndex = DropIndex;
	const FFelsKriegerDropCase& Case = DropCases[DropIndex];
	EnterPhase(EFelsKriegerTestPhase::RunningDrop);
	bM_DropEnded = false;
	bM_CheckedLandedState = false;
	M_SquadsCompleteSeconds = -1.f;
	M_FirstSquadSeenSeconds = -1.f;
	M_DropEndedSeconds = -1.f;
	M_MostDropAircraftSeen = 0;
	M_DropAircraft.Reset();
	M_DroppedSquads.Reset();
	M_KnownSquads.Reset();
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		M_KnownSquads.Add(*It);
	}

	float DropOffset = DefaultDropOffset;
	FParse::Value(FCommandLine::Get(), TEXT("FelsKriegerDropOffset="), DropOffset);
	const FVector Side(-M_TowardCentre.Y, M_TowardCentre.X, 0.f);
	M_DropAnchor = KeepInsideLandscape(M_Anchor - Side * DropOffset);

	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	Check(IsValid(Manager) && IsValid(AbilitySet), FString(Case.Label) + TEXT("_ability_manager_and_set"));
	if (not IsValid(Manager) || not IsValid(AbilitySet))
	{
		Finish();
		return;
	}
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	M_Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Case.Ability);
	UGA_AircraftDrop* Drop = Cast<UGA_AircraftDrop>(M_Ability);
	Check(IsValid(Drop), FString(Case.Label) + TEXT("_is_aircraft_drop"),
	      IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Drop))
	{
		Finish();
		return;
	}

	const FGLobalAbilityCostState& Costs = Drop->GetAbilityCosts();
	const int32* Radixite = Costs.Costs.ResourceCosts.Find(ERTSResourceType::Resource_Radixite);
	FString CostText;
	int32 OtherResources = 0;
	for (const TPair<ERTSResourceType, int32>& Cost : Costs.Costs.ResourceCosts)
	{
		CostText += FString::Printf(TEXT("%s=%d "), *UEnum::GetValueAsString(Cost.Key), Cost.Value);
		OtherResources += Cost.Key != ERTSResourceType::Resource_Radixite && Cost.Value != 0 ? 1 : 0;
	}
	const ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	const FSquadData Data = IsValid(GameState)
		                        ? GameState->GetSquadDataOfPlayer(TestPlayer, ESquadSubtype::Squad_Ger_FelsKrieger)
		                        : FSquadData();
	const int32* SquadRadixite = Data.Cost.ResourceCosts.Find(ERTSResourceType::Resource_Radixite);
	const float ExpectedRadixite = SquadRadixite ? *SquadRadixite * Case.SquadCount * DropCostFactor : 0.f;
	Check(Drop->GetAbilityTier() == Case.Tier, FString(Case.Label) + TEXT("_tier"),
	      Global_GetGlobalAbilityTierDisplayName(Drop->GetAbilityTier()));
	Check(Costs.CoolDownTime >= Case.CooldownMin && Costs.CoolDownTime <= Case.CooldownMax,
	      FString(Case.Label) + TEXT("_cooldown_in_tier_band"), FString::Printf(TEXT("%d"), Costs.CoolDownTime));
	Check(OtherResources == 0 && Radixite != nullptr, FString(Case.Label) + TEXT("_cost_radixite_only"), CostText);
	Check(Radixite != nullptr && ExpectedRadixite > 0.f
	      && FMath::Abs(*Radixite - ExpectedRadixite) <= ExpectedRadixite * DropCostTolerance,
	      FString(Case.Label) + TEXT("_cost_is_125pct_of_training_radixite"),
	      FString::Printf(TEXT("cost=%d expected~%.0f (squad %d x %d x %.2f)"), Radixite ? *Radixite : 0,
	                      ExpectedRadixite, SquadRadixite ? *SquadRadixite : 0, Case.SquadCount, DropCostFactor));

	TWeakObjectPtr<UFelsKriegerTestSubsystem> WeakThis(this);
	// Only the running drop counts: the previous drop's aircraft may still be on its way out.
	const TWeakObjectPtr<UGlobalAbility> WeakDrop(Drop);
	Drop->OnGlobalAbilityEnded.AddLambda([WeakThis, WeakDrop](UGlobalAbility* EndedAbility)
	{
		if (WeakThis.IsValid() && WeakDrop.Get() == EndedAbility && WeakThis->M_Ability == EndedAbility)
		{
			WeakThis->bM_DropEnded = true;
		}
	});
	M_StartedDrops.Add(Drop);
	Drop->InitGlobalAbility(TestPlayer, Manager, Controller);
	Drop->ExecuteAbilityAtLocation(M_DropAnchor);
	UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST DROP_EXECUTED %s anchor=%s"), Case.Label,
	       *M_DropAnchor.ToCompactString());
}

void UFelsKriegerTestSubsystem::CollectNewSquads()
{
	using namespace FelsKriegerTestPrivate;
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		if (M_KnownSquads.Contains(*It) || M_DroppedSquads.Contains(*It))
		{
			continue;
		}
		const URTSComponent* SquadRTS = It->GetRTSComponent();
		if (IsValid(SquadRTS) && SquadRTS->GetSubtypeAsSquadSubtype() == ESquadSubtype::Squad_Ger_FelsKrieger)
		{
			M_DroppedSquads.Add(*It);
			UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST DROPPED_SQUAD %s at=%.1f"), *It->GetName(),
			       GetNow() - M_PhaseStartSeconds);
		}
	}
}

void UFelsKriegerTestSubsystem::TickRunningDrop()
{
	using namespace FelsKriegerTestPrivate;
	const FFelsKriegerDropCase& Case = DropCases[M_DropIndex];
	const float Elapsed = GetNow() - M_PhaseStartSeconds;

	int32 AircraftCount = 0;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		if (not It->GetClass()->GetName().Contains(DropAircraftMarker))
		{
			continue;
		}
		++AircraftCount;
		const float Distance = FVector::Dist2D(It->GetActorLocation(), M_DropAnchor);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			if (M_SquadsCompleteSeconds < 0.f)
			{
				M_DropAircraft = *It;
			}
		}
	}
	M_MostDropAircraftSeen = FMath::Max(M_MostDropAircraftSeen, AircraftCount);

	CollectNewSquads();
	bool bAllLoaded = M_DroppedSquads.Num() >= Case.SquadCount;
	for (const TWeakObjectPtr<ASquadController>& Squad : M_DroppedSquads)
	{
		bAllLoaded &= Squad.IsValid() && Squad->GetIsSquadFullyLoadedAndInitialized();
	}

	if (M_DroppedSquads.IsEmpty() && M_DropAircraft.IsValid())
	{
		ViewFrom(M_DropAircraft->GetActorLocation(), 7000.f, -30.f);
	}
	else
	{
		ViewFrom(M_DropAnchor + FVector(0.f, 0.f, 300.f), 4200.f, -32.f);
	}

	// The aircraft has to be on the ground when the first walkers appear (they walk out of it, they do not jump).
	if (not M_DroppedSquads.IsEmpty() && M_FirstSquadSeenSeconds < 0.f)
	{
		M_FirstSquadSeenSeconds = GetNow();
		const AAircraftMaster* Aircraft = M_DropAircraft.Get();
		Check(IsValid(Aircraft) && not Aircraft->GetIsAircraftAirborne(),
		      FString(Case.Label) + TEXT("_aircraft_landed_when_walkers_appear"),
		      IsValid(Aircraft) ? Aircraft->GetActorLocation().ToCompactString() : TEXT("no aircraft"));
	}
	if (bAllLoaded && M_SquadsCompleteSeconds < 0.f)
	{
		M_SquadsCompleteSeconds = GetNow();
		UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST SQUADS_COMPLETE %s at=%.1fs squads=%d"),
		       Case.Label, Elapsed, M_DroppedSquads.Num());
	}
	if (M_SquadsCompleteSeconds >= 0.f && not bM_CheckedLandedState
		&& GetNow() - M_SquadsCompleteSeconds >= LandedCheckDelaySeconds)
	{
		bM_CheckedLandedState = true;
		CheckWalkersOutsideAircraft(FString(Case.Label) + TEXT("_landed"));
		RequestScreenshot(FString(Case.Label) + TEXT("_Landed"));
	}
	if (bM_DropEnded && M_DropEndedSeconds < 0.f)
	{
		M_DropEndedSeconds = GetNow();
		UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST DROP_ENDED %s at=%.1fs"), Case.Label, Elapsed);
	}

	const bool bTakeOffOver = bM_CheckedLandedState
		&& (bM_DropEnded || GetNow() - M_SquadsCompleteSeconds >= AfterTakeOffSeconds);
	if (bTakeOffOver)
	{
		FinishDropChecks();
		return;
	}
	if (Elapsed >= DropTimeoutSeconds)
	{
		Check(false, FString(Case.Label) + TEXT("_finished_in_time"),
		      FString::Printf(TEXT("squads=%d loaded=%d aircraft_seen=%d"), M_DroppedSquads.Num(), bAllLoaded ? 1 : 0,
		                      M_MostDropAircraftSeen));
		FinishDropChecks();
	}
}

void UFelsKriegerTestSubsystem::FinishDropChecks()
{
	using namespace FelsKriegerTestPrivate;
	const FFelsKriegerDropCase& Case = DropCases[M_DropIndex];
	const FString Label = Case.Label;
	Check(M_MostDropAircraftSeen >= 1, Label + TEXT("_ju390_flew_in"),
	      FString::Printf(TEXT("seen=%d"), M_MostDropAircraftSeen));
	const AAircraftMaster* Aircraft = M_DropAircraft.Get();
	Check(not IsValid(Aircraft) || Aircraft->GetIsAircraftAirborne(), Label + TEXT("_aircraft_took_off"));
	Check(M_DroppedSquads.Num() == Case.SquadCount, Label + TEXT("_squad_count"),
	      FString::Printf(TEXT("%d expected %d"), M_DroppedSquads.Num(), Case.SquadCount));
	RequestScreenshot(Label + TEXT("_AfterTakeOff"));

	int32 Index = 0;
	for (const TWeakObjectPtr<ASquadController>& SquadPtr : M_DroppedSquads)
	{
		ASquadController* Squad = SquadPtr.Get();
		const FString SquadLabel = FString::Printf(TEXT("%s_squad%d"), *Label, Index++);
		if (not IsValid(Squad))
		{
			Check(false, SquadLabel + TEXT("_alive"));
			continue;
		}
		CheckSquad(Squad, SquadLabel);
		CheckWalkersOnGround(Squad, SquadLabel);
		const FVector Centre = GetSquadCentre(Squad);
		Check(FVector::Dist2D(Centre, M_DropAnchor) <= MaxLandingDistance, SquadLabel + TEXT("_near_drop_point"),
		      FString::Printf(TEXT("%.0f cm"), FVector::Dist2D(Centre, M_DropAnchor)));
	}

	// Each dropped squad walks away from the landing point, in its own direction.
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
		FVector Away = (Centre - M_DropAnchor).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = FVector(-1.f, 0.f, 0.f);
		}
		const FVector Target = KeepInsideLandscape(Centre + Away * MoveDistance);
		const ECommandQueueError MoveResult = Squad->MoveToLocation(Target, true, FRotator::ZeroRotator);
		Check(MoveResult == ECommandQueueError::NoError, Squad->GetName() + TEXT("_move_order_accepted"),
		      UEnum::GetValueAsString(MoveResult));
		M_DroppedMoveTargets.Add(Target);
		M_DroppedMoveStarts.Add(Centre);
		UE_LOG(LogRTSFelsKriegerTest, Display, TEXT("RTS_FELSKRIEGER_TEST DROPPED_MOVE %s from=%s to=%s"), *Squad->GetName(),
		       *Centre.ToCompactString(), *Target.ToCompactString());
	}
	EnterPhase(EFelsKriegerTestPhase::MovingDroppedSquads);
}

void UFelsKriegerTestSubsystem::TickMovingDroppedSquads()
{
	using namespace FelsKriegerTestPrivate;
	const FString Label = DropCases[M_DropIndex].Label;
	ViewFrom(M_DropAnchor + FVector(0.f, 0.f, 300.f), 4200.f, -32.f);
	bool bAllArrived = true;
	int32 TargetIndex = 0;
	for (const TWeakObjectPtr<ASquadController>& SquadPtr : M_DroppedSquads)
	{
		if (not SquadPtr.IsValid() || not M_DroppedMoveTargets.IsValidIndex(TargetIndex))
		{
			++TargetIndex;
			continue;
		}
		bAllArrived &= FVector::Dist2D(GetSquadCentre(SquadPtr.Get()), M_DroppedMoveTargets[TargetIndex]) <=
			MoveArrivedRadius;
		++TargetIndex;
	}
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bAllArrived && Elapsed < MoveTimeoutSeconds)
	{
		return;
	}
	TargetIndex = 0;
	for (const TWeakObjectPtr<ASquadController>& SquadPtr : M_DroppedSquads)
	{
		const FString SquadLabel = FString::Printf(TEXT("%s_squad%d"), *Label, TargetIndex);
		if (not SquadPtr.IsValid() || not M_DroppedMoveTargets.IsValidIndex(TargetIndex))
		{
			Check(false, SquadLabel + TEXT("_alive_for_move"));
			++TargetIndex;
			continue;
		}
		const FVector Centre = GetSquadCentre(SquadPtr.Get());
		const float ToTarget = FVector::Dist2D(Centre, M_DroppedMoveTargets[TargetIndex]);
		const float Moved = FVector::Dist2D(Centre, M_DroppedMoveStarts[TargetIndex]);
		Check(ToTarget <= MoveArrivedRadius, SquadLabel + TEXT("_reached_move_target"),
		      FString::Printf(TEXT("to_target=%.0f after=%.1fs"), ToTarget, Elapsed));
		Check(Moved >= MinMovedDistance, SquadLabel + TEXT("_walkers_moved"), FString::Printf(TEXT("%.0f cm"), Moved));
		CheckWalkersOnGround(SquadPtr.Get(), SquadLabel + TEXT("_after_move"));
		++TargetIndex;
	}
	RequestScreenshot(Label + TEXT("_AfterMove"));
	if (M_DropIndex + 1 < DropCaseCount)
	{
		StartDrop(M_DropIndex + 1);
		return;
	}
	Finish();
}

void UFelsKriegerTestSubsystem::CheckWalkerAnimation(ASquadController* Squad, const FString& Label)
{
	using namespace FelsKriegerTestPrivate;
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	int32 MeshMatches = 0;
	int32 SkeletonMatches = 0;
	int32 AnimMatches = 0;
	for (ASquadUnit* Unit : Units)
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
		UE_LOG(LogRTSFelsKriegerTest, Display,
		       TEXT("RTS_FELSKRIEGER_TEST ANIM %s %s mesh=%s skeleton=%s anim_instance=%s scale=%s tick_option=%d"),
		       *Label, *Unit->GetName(), *MeshName, *SkeletonName, *AnimName,
		       IsValid(Mesh) ? *Mesh->GetRelativeScale3D().ToCompactString() : TEXT("-"),
		       IsValid(Mesh) ? static_cast<int32>(Mesh->VisibilityBasedAnimTickOption) : -1);
	}
	const bool bHasUnits = Units.Num() > 0;
	Check(bHasUnits && MeshMatches == Units.Num(), Label + TEXT("_walkers_use_fels_krieger_mesh"),
	      FString::Printf(TEXT("%d/%d"), MeshMatches, Units.Num()));
	Check(bHasUnits && SkeletonMatches == Units.Num(), Label + TEXT("_mesh_on_infantry_skeleton"),
	      FString::Printf(TEXT("%d/%d"), SkeletonMatches, Units.Num()));
	Check(bHasUnits && AnimMatches == Units.Num(), Label + TEXT("_squad_anim_bp_running"),
	      FString::Printf(TEXT("%d/%d"), AnimMatches, Units.Num()));
}

void UFelsKriegerTestSubsystem::ResetPoseTracking()
{
	M_MaxFootTravel = 0.f;
	M_MaxHandTravel = 0.f;
	M_MaxWalkerSpeed = 0.f;
	M_FootStartPositions.Reset();
	M_HandStartPositions.Reset();
	M_MontagesSeen.Reset();
}

void UFelsKriegerTestSubsystem::SamplePose(ASquadController* Squad)
{
	using namespace FelsKriegerTestPrivate;
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	const bool bFirstSample = M_FootStartPositions.Num() != Units.Num();
	if (bFirstSample)
	{
		M_FootStartPositions.Reset();
		M_HandStartPositions.Reset();
	}
	for (int32 Index = 0; Index < Units.Num(); ++Index)
	{
		const USkeletalMeshComponent* Mesh = Units[Index]->GetMesh();
		const FVector Foot = IsValid(Mesh) ? Mesh->GetBoneLocation(FootBone, EBoneSpaces::ComponentSpace) : FVector::ZeroVector;
		const FVector Hand = IsValid(Mesh) ? Mesh->GetBoneLocation(HandBone, EBoneSpaces::ComponentSpace) : FVector::ZeroVector;
		if (bFirstSample)
		{
			M_FootStartPositions.Add(Foot);
			M_HandStartPositions.Add(Hand);
			continue;
		}
		if (not IsValid(Mesh) || not M_FootStartPositions.IsValidIndex(Index) || not M_HandStartPositions.IsValidIndex(Index))
		{
			continue;
		}
		M_MaxFootTravel = FMath::Max(M_MaxFootTravel, static_cast<float>(FVector::Dist(Foot, M_FootStartPositions[Index])));
		M_MaxHandTravel = FMath::Max(M_MaxHandTravel, static_cast<float>(FVector::Dist(Hand, M_HandStartPositions[Index])));
		M_MaxWalkerSpeed = FMath::Max(M_MaxWalkerSpeed, static_cast<float>(Units[Index]->GetVelocity().Size2D()));
		const UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		const UAnimMontage* Montage = IsValid(AnimInstance) ? AnimInstance->GetCurrentActiveMontage() : nullptr;
		if (IsValid(Montage))
		{
			M_MontagesSeen.Add(Montage->GetName());
		}
	}
}

void UFelsKriegerTestSubsystem::LogPoseTracking(const FString& Label) const
{
	FString Montages;
	for (const FString& Montage : M_MontagesSeen)
	{
		Montages += Montage + TEXT(" ");
	}
	UE_LOG(LogRTSFelsKriegerTest, Display,
	       TEXT("RTS_FELSKRIEGER_TEST POSE %s foot_travel=%.2f hand_travel=%.2f max_speed=%.0f montages=[%s]"), *Label,
	       M_MaxFootTravel, M_MaxHandTravel, M_MaxWalkerSpeed, *Montages);
}

bool UFelsKriegerTestSubsystem::GetHasNoResistanceOverride(const ASquadUnit* Unit)
{
	// OverwriteUnitArmorAndResistance is protected; read it through reflection.
	const FStructProperty* Property = FindFProperty<FStructProperty>(
		Unit->GetClass(), TEXT("OverwriteUnitArmorAndResistance"));
	if (Property == nullptr || Property->Struct != FSquadUnitHealthResistanceOverwrite::StaticStruct())
	{
		return false;
	}
	const FSquadUnitHealthResistanceOverwrite* Overwrite =
		Property->ContainerPtrToValuePtr<FSquadUnitHealthResistanceOverwrite>(Unit);
	return Overwrite->ResistancePresetType == EResistancePresetType::None
		&& FMath::IsNearlyEqual(Overwrite->HealthMlt, 1.f);
}

FBox UFelsKriegerTestSubsystem::FindLandscapeBounds() const
{
	FBox Bounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		Bounds += It->GetComponentsBoundingBox(true);
	}
	return Bounds;
}

FVector UFelsKriegerTestSubsystem::KeepInsideLandscape(const FVector& Location) const
{
	using namespace FelsKriegerTestPrivate;
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

FVector UFelsKriegerTestSubsystem::GetSquadCentre(ASquadController* Squad)
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
	{
		Sum += Unit->GetActorLocation();
		++Count;
	}
	return Count > 0 ? Sum / Count : Squad->GetActorLocation();
}

void UFelsKriegerTestSubsystem::ViewFrom(const FVector& Focus, const float Distance, const float Pitch,
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

void UFelsKriegerTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("FelsKriegerTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UFelsKriegerTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UFelsKriegerTestSubsystem::Tick(float DeltaTime) {}
TStatId UFelsKriegerTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFelsKriegerTestSubsystem, STATGROUP_Tickables);
}

#endif
