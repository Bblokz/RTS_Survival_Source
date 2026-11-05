// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RedSpineTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogRTSRedSpineTest, Log, All);

namespace RedSpineTestPrivate
{
	struct FRedSpineDropCase
	{
		EGlobalAbility Ability;
		const TCHAR* Label;
		int32 SquadCount;
		EGlobalAbilityTier Tier;
		int32 CooldownMin;
		int32 CooldownMax;
	};

	const FRedSpineDropCase DropCases[] = {
		{EGlobalAbility::GA_GerRedSpineDrop, TEXT("SingleDrop"), 1, EGlobalAbilityTier::TierI, 60, 90},
		{EGlobalAbility::GA_GerRedSpinePairDrop, TEXT("PairDrop"), 2, EGlobalAbilityTier::TierII, 120, 180},
	};
	constexpr int32 DropCaseCount = UE_ARRAY_COUNT(DropCases);

	constexpr int32 TestPlayer = 1;
	constexpr int32 ExpectedWalkers = 3;
	const TCHAR* const WalkerClassMarker = TEXT("Ger_Inf_Walker_RedSpine");
	const TCHAR* const SquadClassMarker = TEXT("Squad_Ger_RedSpine");
	const TCHAR* const WeaponClassMarker = TEXT("MP46");
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
	// The pair drop lands this far from the single drop, towards the map centre.
	constexpr float DefaultPairOffset = 5000.f;
	// Every test location stays this far inside the landscape: RTS_ToCopySmall's -Y edge is close to the anchor.
	constexpr float LandscapeEdgeMargin = 2500.f;
	constexpr float CameraTraceLength = 500000.f;
	// Feet within this of the ground below the walker count as standing on it.
	constexpr float FootGroundTolerance = 60.f;
	constexpr float GroundTraceLength = 5000.f;
	// Half the fuselage width plus a walker's radius: closer to the aircraft's long axis than this is inside it.
	constexpr float FuselageHalfWidth = 300.f;
	constexpr float MaxLandingDistance = 3500.f;
}

bool URedSpineTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("RedSpineValidate"));
}

TStatId URedSpineTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URedSpineTestSubsystem, STATGROUP_Tickables);
}

void URedSpineTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case ERedSpineTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case ERedSpineTestPhase::WaitingForSpawnedSquad:
		TickWaitingForSpawnedSquad();
		return;
	case ERedSpineTestPhase::MovingSpawnedSquad:
		TickMovingSpawnedSquad();
		return;
	case ERedSpineTestPhase::AttackingGround:
		TickAttackingGround();
		return;
	case ERedSpineTestPhase::RunningDrop:
		TickRunningDrop();
		return;
	case ERedSpineTestPhase::MovingDroppedSquads:
		TickMovingDroppedSquads();
		return;
	case ERedSpineTestPhase::Finished:
		return;
	}
}

void URedSpineTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSRedSpineTest, Error, TEXT("RTS_REDSPINE_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float URedSpineTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void URedSpineTestSubsystem::EnterPhase(const ERedSpineTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
	UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST PHASE %s at=%.1f"), *UEnum::GetValueAsString(NewPhase),
	       GetNow());
}

void URedSpineTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = ERedSpineTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void URedSpineTestSubsystem::TickWaitingForStart()
{
	using namespace RedSpineTestPrivate;
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
	UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST LANDSCAPE min=%s max=%s anchor=%s to_centre=%s"),
	       *M_LandscapeBounds.Min.ToCompactString(), *M_LandscapeBounds.Max.ToCompactString(),
	       *M_Anchor.ToCompactString(), *M_TowardCentre.ToCompactString());
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	StartDirectSpawn();
}

bool URedSpineTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace RedSpineTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("RedSpineAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("RedSpineAnchorY="), AnchorY))
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

void URedSpineTestSubsystem::StartDirectSpawn()
{
	using namespace RedSpineTestPrivate;
	EnterPhase(ERedSpineTestPhase::WaitingForSpawnedSquad);
	const FVector Side(-M_TowardCentre.Y, M_TowardCentre.X, 0.f);
	M_DirectSpawnLocation = KeepInsideLandscape(M_Anchor + Side * DirectSpawnSideOffset);
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	Check(IsValid(Spawner), TEXT("async_spawner_found"));
	if (not IsValid(Spawner))
	{
		Finish();
		return;
	}
	TWeakObjectPtr<URedSpineTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_RedSpine)),
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
	Check(bRequested, TEXT("spawner_accepted_red_spine_option"));
	UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST SPAWN_REQUESTED anchor=%s at=%s"),
	       *M_Anchor.ToCompactString(), *M_DirectSpawnLocation.ToCompactString());
	if (not bRequested)
	{
		Finish();
	}
}

void URedSpineTestSubsystem::OnDirectSquadSpawned(AActor* SpawnedActor)
{
	M_SpawnedSquad = Cast<ASquadController>(SpawnedActor);
	UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST SPAWNED %s"),
	       IsValid(SpawnedActor) ? *SpawnedActor->GetClass()->GetName() : TEXT("none"));
}

void URedSpineTestSubsystem::TickWaitingForSpawnedSquad()
{
	using namespace RedSpineTestPrivate;
	ViewFrom(M_DirectSpawnLocation, 1300.f, -25.f);
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
	// Give the walkers a moment to leave the spawn placement and stand still for the screenshot.
	if (GetNow() - M_PhaseStartSeconds < 4.f)
	{
		return;
	}
	Check(true, TEXT("spawned_squad_loaded"), Squad->GetClass()->GetName());
	Check(Squad->GetClass()->GetName().StartsWith(SquadClassMarker), TEXT("spawner_row_class"),
	      Squad->GetClass()->GetName());
	CheckSquad(Squad, TEXT("spawned"));
	CheckWalkersOnGround(Squad, TEXT("spawned"));
	RequestScreenshot(TEXT("SpawnedSquad"));

	M_MoveTarget = KeepInsideLandscape(GetSquadCentre(Squad) + M_TowardCentre * MoveDistance);
	const ECommandQueueError MoveResult = Squad->MoveToLocation(M_MoveTarget, true, FRotator::ZeroRotator);
	Check(MoveResult == ECommandQueueError::NoError, TEXT("spawned_move_order_accepted"),
	      UEnum::GetValueAsString(MoveResult));
	M_DirectSpawnLocation = GetSquadCentre(Squad);
	EnterPhase(ERedSpineTestPhase::MovingSpawnedSquad);
}

void URedSpineTestSubsystem::TickMovingSpawnedSquad()
{
	using namespace RedSpineTestPrivate;
	ASquadController* Squad = M_SpawnedSquad.Get();
	if (not IsValid(Squad))
	{
		Check(false, TEXT("spawned_squad_alive_for_move"));
		Finish();
		return;
	}
	const FVector Centre = GetSquadCentre(Squad);
	ViewFrom(Centre, 1500.f, -25.f, 225.f);
	const float ToTarget = FVector::Dist2D(Centre, M_MoveTarget);
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (ToTarget > MoveArrivedRadius && Elapsed < MoveTimeoutSeconds)
	{
		return;
	}
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
	EnterPhase(ERedSpineTestPhase::AttackingGround);
}

void URedSpineTestSubsystem::TickAttackingGround()
{
	using namespace RedSpineTestPrivate;
	ASquadController* Squad = M_SpawnedSquad.Get();
	if (not IsValid(Squad))
	{
		Check(false, TEXT("spawned_squad_alive_for_attack"));
		Finish();
		return;
	}
	ViewFrom(GetSquadCentre(Squad), 1500.f, -25.f, 225.f);
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
	Squad->StopCommand();
	StartDrop(0);
}

void URedSpineTestSubsystem::CheckSquad(ASquadController* Squad, const FString& Label)
{
	using namespace RedSpineTestPrivate;
	const URTSComponent* SquadRTS = Squad->GetRTSComponent();
	const ESquadSubtype Subtype = IsValid(SquadRTS) ? SquadRTS->GetSubtypeAsSquadSubtype() : ESquadSubtype::Squad_None;
	Check(Subtype == ESquadSubtype::Squad_Ger_RedSpine, Label + TEXT("_subtype"), UEnum::GetValueAsString(Subtype));
	const int32 Owner = IsValid(SquadRTS) ? SquadRTS->GetOwningPlayer() : -1;
	Check(Owner == TestPlayer, Label + TEXT("_owned_by_player_1"), FString::Printf(TEXT("%d"), Owner));
	Check(Squad->GetIsSquadFullyLoadedAndInitialized(), Label + TEXT("_fully_loaded"));

	const ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	const FSquadData Data = IsValid(GameState)
		                        ? GameState->GetSquadDataOfPlayer(TestPlayer, ESquadSubtype::Squad_Ger_RedSpine)
		                        : FSquadData();
	const float ExpectedHealth = DeveloperSettings::GameBalance::UnitHealth::RedSpineWalkerHealth;
	Check(FMath::IsNearlyEqual(Data.MaxHealth, ExpectedHealth, 0.5f), Label + TEXT("_squad_data_health"),
	      FString::Printf(TEXT("data=%.1f expected=%.1f"), Data.MaxHealth, ExpectedHealth));
	const FResistanceAndDamageReductionData HeavyArmour =
		FUnitResistanceDataHelpers::GetIHeavyInfantryArmorResistances(ExpectedHealth);

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
		WeaponCount += WeaponClass.Contains(WeaponClassMarker) && WeaponName == EWeaponName::MP46 ? 1 : 0;
		const UHealthComponent* Health = Unit->GetHealthComponent();
		const float MaxHealth = IsValid(Health) ? Health->GetMaxHealth() : -1.f;
		HealthMatches += FMath::IsNearlyEqual(MaxHealth, ExpectedHealth, 0.5f) ? 1 : 0;
		bool bFoundResistance = false;
		const FResistanceAndDamageReductionData Resistance =
			FUnitResistanceDataHelpers::GetUnitResistanceData(Unit, bFoundResistance);
		ArmourMatches += bFoundResistance && Resistance.TargetTypeIcon == HeavyArmour.TargetTypeIcon ? 1 : 0;
		const URTSComponent* UnitRTS = Unit->FindComponentByClass<URTSComponent>();
		OwnerMatches += IsValid(UnitRTS) && UnitRTS->GetOwningPlayer() == TestPlayer ? 1 : 0;
		Details += FString::Printf(TEXT("%s/%s(%s)/%.0fhp "), *UnitClass, *WeaponClass,
		                           *Global_GetWeaponDisplayName(WeaponName), MaxHealth);
	}
	const bool bHasUnits = Units.Num() > 0;
	Check(bHasUnits && WalkerClassCount == Units.Num(), Label + TEXT("_units_are_red_spine_walkers"), Details);
	Check(bHasUnits && WeaponCount == Units.Num(), Label + TEXT("_every_walker_has_mp46"), Details);
	Check(bHasUnits && HealthMatches == Units.Num(), Label + TEXT("_walker_health"),
	      FString::Printf(TEXT("expected=%.0f %s"), ExpectedHealth, *Details));
	Check(bHasUnits && ArmourMatches == Units.Num(), Label + TEXT("_heavy_infantry_armour"),
	      FString::Printf(TEXT("%d/%d"), ArmourMatches, Units.Num()));
	Check(bHasUnits && OwnerMatches == Units.Num(), Label + TEXT("_walkers_owned_by_player_1"),
	      FString::Printf(TEXT("%d/%d"), OwnerMatches, Units.Num()));
}

void URedSpineTestSubsystem::CheckWalkersOnGround(ASquadController* Squad, const FString& Label)
{
	using namespace RedSpineTestPrivate;
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
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RedSpineGround), false);
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

void URedSpineTestSubsystem::CheckWalkersOutsideAircraft(const FString& Label)
{
	using namespace RedSpineTestPrivate;
	UWorld* World = GetWorld();
	AAircraftMaster* Aircraft = M_DropAircraft.Get();
	Check(IsValid(Aircraft), Label + TEXT("_aircraft_present_while_landed"));
	if (not IsValid(Aircraft))
	{
		return;
	}
	const FTransform AircraftTransform = Aircraft->GetActorTransform();
	const FBox LocalBox = Aircraft->CalculateComponentsBoundingBoxInLocalSpace(false);
	UE_LOG(LogRTSRedSpineTest, Display,
	       TEXT("RTS_REDSPINE_TEST AIRCRAFT %s at=%s rot=%s airborne=%d local_box_min=%s max=%s"),
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
				FCollisionQueryParams Params(SCENE_QUERY_STAT(RedSpineAircraftOverlap), false, Unit);
				World->OverlapMultiByChannel(Overlaps, Capsule->GetComponentLocation(), Capsule->GetComponentQuat(),
				                             ECC_Pawn, Capsule->GetCollisionShape(), Params);
				for (const FOverlapResult& Overlap : Overlaps)
				{
					bOverlapsAircraft |= Overlap.GetActor() == Aircraft;
				}
			}
			const FVector Local = AircraftTransform.InverseTransformPosition(Unit->GetActorLocation());
			const bool bInsideFuselage = FMath::Abs(Local.Y) < FuselageHalfWidth
				&& Local.X > LocalBox.Min.X && Local.X < LocalBox.Max.X;
			Outside += not bOverlapsAircraft && not bInsideFuselage ? 1 : 0;
			Details += FString::Printf(TEXT("[local=%s overlap=%d fuselage=%d] "), *Local.ToCompactString(),
			                           bOverlapsAircraft ? 1 : 0, bInsideFuselage ? 1 : 0);
		}
	}
	Check(Total > 0 && Outside == Total, Label + TEXT("_walkers_outside_aircraft"), Details);
}

void URedSpineTestSubsystem::StartDrop(const int32 DropIndex)
{
	using namespace RedSpineTestPrivate;
	M_DropIndex = DropIndex;
	const FRedSpineDropCase& Case = DropCases[DropIndex];
	EnterPhase(ERedSpineTestPhase::RunningDrop);
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

	M_DropAnchor = M_Anchor;
	if (DropIndex > 0)
	{
		float PairOffset = DefaultPairOffset;
		FParse::Value(FCommandLine::Get(), TEXT("RedSpinePairOffset="), PairOffset);
		M_DropAnchor = KeepInsideLandscape(M_Anchor + M_TowardCentre * PairOffset);
	}

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
		                        ? GameState->GetSquadDataOfPlayer(TestPlayer, ESquadSubtype::Squad_Ger_RedSpine)
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

	TWeakObjectPtr<URedSpineTestSubsystem> WeakThis(this);
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
	UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST DROP_EXECUTED %s anchor=%s"), Case.Label,
	       *M_DropAnchor.ToCompactString());
}

void URedSpineTestSubsystem::CollectNewSquads()
{
	using namespace RedSpineTestPrivate;
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		if (M_KnownSquads.Contains(*It) || M_DroppedSquads.Contains(*It))
		{
			continue;
		}
		const URTSComponent* SquadRTS = It->GetRTSComponent();
		if (IsValid(SquadRTS) && SquadRTS->GetSubtypeAsSquadSubtype() == ESquadSubtype::Squad_Ger_RedSpine)
		{
			M_DroppedSquads.Add(*It);
			UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST DROPPED_SQUAD %s at=%.1f"), *It->GetName(),
			       GetNow() - M_PhaseStartSeconds);
		}
	}
}

void URedSpineTestSubsystem::TickRunningDrop()
{
	using namespace RedSpineTestPrivate;
	const FRedSpineDropCase& Case = DropCases[M_DropIndex];
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
		UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST SQUADS_COMPLETE %s at=%.1fs squads=%d"),
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
		UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST DROP_ENDED %s at=%.1fs"), Case.Label, Elapsed);
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

void URedSpineTestSubsystem::FinishDropChecks()
{
	using namespace RedSpineTestPrivate;
	const FRedSpineDropCase& Case = DropCases[M_DropIndex];
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
		UE_LOG(LogRTSRedSpineTest, Display, TEXT("RTS_REDSPINE_TEST DROPPED_MOVE %s from=%s to=%s"), *Squad->GetName(),
		       *Centre.ToCompactString(), *Target.ToCompactString());
	}
	EnterPhase(ERedSpineTestPhase::MovingDroppedSquads);
}

void URedSpineTestSubsystem::TickMovingDroppedSquads()
{
	using namespace RedSpineTestPrivate;
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

FBox URedSpineTestSubsystem::FindLandscapeBounds() const
{
	FBox Bounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		Bounds += It->GetComponentsBoundingBox(true);
	}
	return Bounds;
}

FVector URedSpineTestSubsystem::KeepInsideLandscape(const FVector& Location) const
{
	using namespace RedSpineTestPrivate;
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

FVector URedSpineTestSubsystem::GetSquadCentre(ASquadController* Squad)
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

void URedSpineTestSubsystem::ViewFrom(const FVector& Focus, const float Distance, const float Pitch,
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

void URedSpineTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("RedSpineTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool URedSpineTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void URedSpineTestSubsystem::Tick(float DeltaTime) {}
TStatId URedSpineTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URedSpineTestSubsystem, STATGROUP_Tickables);
}

#endif
