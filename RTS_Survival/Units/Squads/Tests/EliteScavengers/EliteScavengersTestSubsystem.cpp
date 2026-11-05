// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EliteScavengersTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogRTSEliteScavTest, Log, All);

namespace EliteScavengersTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float SpawnTimeoutSeconds = 30.f;
	constexpr float DropTimeoutSeconds = 150.f;
	// Seconds the dropped squads get after their release to settle before the final checks.
	constexpr float SettleSeconds = 4.f;
	constexpr float DescentShotSeconds = 3.f;
	constexpr float CameraTraceLength = 500000.f;
	// The directly spawned squad stands this far from the drop anchor, outside the landing spiral.
	constexpr float DirectSpawnOffset = 2500.f;
	// Landing spread of the template is 750; the spiral of two squads stays well inside this.
	constexpr float MaxLandingDistance = 3000.f;
	constexpr int32 ExpectedSoldiers = 5;
	constexpr int32 ExpectedStgCount = 3;
	constexpr int32 ExpectedFaustCount = 2;
	constexpr int32 ExpectedDroppedSquads = 2;
	constexpr float HealthMultiplier = 1.33f;
	constexpr int32 TierICooldownMin = 60;
	constexpr int32 TierICooldownMax = 90;
}

bool UEliteScavengersTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("EliteScavengersValidate"));
}

TStatId UEliteScavengersTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEliteScavengersTestSubsystem, STATGROUP_Tickables);
}

void UEliteScavengersTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EEliteScavengersTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EEliteScavengersTestPhase::WaitingForSpawnedSquad:
		TickWaitingForSpawnedSquad();
		return;
	case EEliteScavengersTestPhase::WaitingForParadrop:
		TickWaitingForParadrop();
		return;
	case EEliteScavengersTestPhase::Finished:
		return;
	}
}

void UEliteScavengersTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSEliteScavTest, Display, TEXT("RTS_ELITESCAV_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSEliteScavTest, Error, TEXT("RTS_ELITESCAV_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UEliteScavengersTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UEliteScavengersTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSEliteScavTest, Display, TEXT("RTS_ELITESCAV_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EEliteScavengersTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UEliteScavengersTestSubsystem::TickWaitingForStart()
{
	using namespace EliteScavengersTestPrivate;
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
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	StartDirectSpawn();
}

bool UEliteScavengersTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace EliteScavengersTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("EliteScavAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("EliteScavAnchorY="), AnchorY))
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

void UEliteScavengersTestSubsystem::StartDirectSpawn()
{
	using namespace EliteScavengersTestPrivate;
	M_Phase = EEliteScavengersTestPhase::WaitingForSpawnedSquad;
	M_PhaseStartSeconds = GetNow();
	if (not GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor + FVector(DirectSpawnOffset, 0.f, 0.f),
	                                                    M_DirectSpawnLocation))
	{
		M_DirectSpawnLocation = M_Anchor + FVector(DirectSpawnOffset, 0.f, 0.f);
	}
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	Check(IsValid(Spawner), TEXT("async_spawner_found"));
	if (not IsValid(Spawner))
	{
		Finish();
		return;
	}
	TWeakObjectPtr<UEliteScavengersTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_EliteScavengers)),
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
	Check(bRequested, TEXT("spawner_accepted_elite_scavengers_option"));
	UE_LOG(LogRTSEliteScavTest, Display, TEXT("RTS_ELITESCAV_TEST SPAWN_REQUESTED anchor=%s at=%s"),
	       *M_Anchor.ToCompactString(), *M_DirectSpawnLocation.ToCompactString());
	if (not bRequested)
	{
		Finish();
	}
}

void UEliteScavengersTestSubsystem::OnDirectSquadSpawned(AActor* SpawnedActor)
{
	M_SpawnedSquad = Cast<ASquadController>(SpawnedActor);
	UE_LOG(LogRTSEliteScavTest, Display, TEXT("RTS_ELITESCAV_TEST SPAWNED %s"),
	       IsValid(SpawnedActor) ? *SpawnedActor->GetClass()->GetName() : TEXT("none"));
}

void UEliteScavengersTestSubsystem::TickWaitingForSpawnedSquad()
{
	using namespace EliteScavengersTestPrivate;
	ViewFrom(M_DirectSpawnLocation, 1400.f, -35.f);
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
	// Give the soldiers a moment to leave the spawn placement and stand still for the screenshot.
	if (GetNow() - M_PhaseStartSeconds < 4.f)
	{
		return;
	}
	Check(true, TEXT("spawned_squad_loaded"), Squad->GetClass()->GetName());
	Check(Squad->GetClass()->GetName().StartsWith(TEXT("Squad_Ger_EliteScavengers")), TEXT("spawner_row_class"),
	      Squad->GetClass()->GetName());
	CheckSquad(Squad, TEXT("spawned"));
	RequestScreenshot(TEXT("SpawnedSquad"));
	StartParadrop();
}

void UEliteScavengersTestSubsystem::CheckSquad(ASquadController* Squad, const FString& Label)
{
	using namespace EliteScavengersTestPrivate;
	const URTSComponent* SquadRTS = Squad->GetRTSComponent();
	const ESquadSubtype Subtype = IsValid(SquadRTS) ? SquadRTS->GetSubtypeAsSquadSubtype() : ESquadSubtype::Squad_None;
	Check(Subtype == ESquadSubtype::Squad_Ger_EliteScavengers, Label + TEXT("_subtype"),
	      UEnum::GetValueAsString(Subtype));
	Check(Global_GetSquadDisplayName(Subtype) == TEXT("Elite Scavengers"), Label + TEXT("_display_name"),
	      Global_GetSquadDisplayName(Subtype));

	const ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	const int32 Owner = IsValid(SquadRTS) ? SquadRTS->GetOwningPlayer() : TestPlayer;
	const float ScavengerHealth = IsValid(GameState)
		                              ? GameState->GetSquadDataOfPlayer(Owner, ESquadSubtype::Squad_Ger_Scavengers).MaxHealth
		                              : 0.f;
	const float ExpectedHealth = FMath::RoundToFloat(ScavengerHealth * HealthMultiplier);

	TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	Check(Units.Num() == ExpectedSoldiers, Label + TEXT("_soldier_count"), FString::Printf(TEXT("%d"), Units.Num()));
	int32 StgCount = 0;
	int32 FaustCount = 0;
	int32 HealthMatches = 0;
	FString HealthList;
	FString WeaponList;
	for (ASquadUnit* Unit : Units)
	{
		const AInfantryWeaponMaster* Weapon = Unit->GetInfantryWeapon();
		const FString WeaponClassName = IsValid(Weapon) ? Weapon->GetClass()->GetName() : TEXT("none");
		const UWeaponState* WeaponState = Unit->GetWeaponState();
		const EWeaponName WeaponName = IsValid(WeaponState)
			                               ? WeaponState->GetRawWeaponData().WeaponName
			                               : EWeaponName::DEFAULT_WEAPON;
		if (WeaponClassName.Contains(TEXT("STG44")) && WeaponName == EWeaponName::STG44_7_92MM)
		{
			++StgCount;
		}
		if (WeaponClassName.Contains(TEXT("PanzerFaust")) && WeaponName == EWeaponName::PanzerFaust)
		{
			++FaustCount;
		}
		WeaponList += FString::Printf(TEXT("%s(%s) "), *WeaponClassName, *Global_GetWeaponDisplayName(WeaponName));
		const UHealthComponent* Health = Unit->GetHealthComponent();
		const float MaxHealth = IsValid(Health) ? Health->GetMaxHealth() : -1.f;
		HealthList += FString::Printf(TEXT("%.1f "), MaxHealth);
		if (FMath::Abs(MaxHealth - ExpectedHealth) <= 0.51f && ScavengerHealth > 0.f)
		{
			++HealthMatches;
		}
	}
	Check(StgCount == ExpectedStgCount, Label + TEXT("_stg44_count"), FString::Printf(TEXT("%d: %s"), StgCount, *WeaponList));
	Check(FaustCount == ExpectedFaustCount, Label + TEXT("_panzerfaust_count"),
	      FString::Printf(TEXT("%d: %s"), FaustCount, *WeaponList));
	Check(HealthMatches == Units.Num() && Units.Num() > 0, Label + TEXT("_health_is_scavenger_x1.33"),
	      FString::Printf(TEXT("scavenger=%.1f expected=%.1f units=[%s]"), ScavengerHealth, ExpectedHealth, *HealthList));
}

void UEliteScavengersTestSubsystem::StartParadrop()
{
	using namespace EliteScavengersTestPrivate;
	M_Phase = EEliteScavengersTestPhase::WaitingForParadrop;
	M_PhaseStartSeconds = GetNow();
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	Check(IsValid(Manager) && IsValid(AbilitySet), TEXT("ability_manager_and_set"));
	if (not IsValid(Manager) || not IsValid(AbilitySet))
	{
		Finish();
		return;
	}
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	M_Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(EGlobalAbility::GA_GerEliteScavengersParadrop);
	UGA_Paratroopers* Paradrop = Cast<UGA_Paratroopers>(M_Ability);
	Check(IsValid(Paradrop), TEXT("paradrop_is_paratroopers_ability"),
	      IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Paradrop))
	{
		Finish();
		return;
	}
	const FGLobalAbilityCostState& Costs = Paradrop->GetAbilityCosts();
	const int32* Radixite = Costs.Costs.ResourceCosts.Find(ERTSResourceType::Resource_Radixite);
	const int32* Metal = Costs.Costs.ResourceCosts.Find(ERTSResourceType::Resource_Metal);
	Check(Paradrop->GetAbilityTier() == EGlobalAbilityTier::TierI, TEXT("paradrop_tier_I"),
	      Global_GetGlobalAbilityTierDisplayName(Paradrop->GetAbilityTier()));
	Check(Costs.CoolDownTime >= TierICooldownMin && Costs.CoolDownTime <= TierICooldownMax,
	      TEXT("paradrop_cooldown_in_tier_I_band"), FString::Printf(TEXT("%d"), Costs.CoolDownTime));
	Check(Radixite != nullptr && *Radixite > 0, TEXT("paradrop_has_cost"),
	      FString::Printf(TEXT("radixite=%d metal=%d"), Radixite ? *Radixite : 0, Metal ? *Metal : 0));

	Paradrop->InitGlobalAbility(TestPlayer, Manager, Controller);
	Paradrop->ExecuteAbilityAtLocation(M_Anchor);
	UE_LOG(LogRTSEliteScavTest, Display, TEXT("RTS_ELITESCAV_TEST PARADROP_EXECUTED anchor=%s"),
	       *M_Anchor.ToCompactString());
}

void UEliteScavengersTestSubsystem::TickWaitingForParadrop()
{
	using namespace EliteScavengersTestPrivate;
	UGA_Paratroopers* Paradrop = Cast<UGA_Paratroopers>(M_Ability);
	if (not IsValid(Paradrop))
	{
		Check(false, TEXT("paradrop_alive"));
		Finish();
		return;
	}
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	const bool bActive = Paradrop->GetHasActiveDrop();
	if (bActive)
	{
		bM_SawActiveDrop = true;
		for (const TWeakObjectPtr<ASquadController>& Squad : Paradrop->GetActiveDropSquads())
		{
			if (Squad.IsValid())
			{
				M_DroppedSquads.AddUnique(Squad);
			}
		}
	}

	// Camera: wide on the aircraft until the first squads exist, then on the landing area.
	const AAircraftMaster* Aircraft = Paradrop->GetActiveDropAircraft();
	if (M_DroppedSquads.IsEmpty() && IsValid(Aircraft))
	{
		ViewFrom(Aircraft->GetActorLocation(), 6000.f, -30.f);
	}
	else
	{
		ViewFrom(M_Anchor + FVector(0.f, 0.f, 900.f), 3800.f, -25.f);
	}
	if (not M_DroppedSquads.IsEmpty() && M_FirstSquadSeenSeconds < 0.f)
	{
		M_FirstSquadSeenSeconds = GetNow();
		RequestScreenshot(TEXT("DropStarted"));
	}
	if (M_FirstSquadSeenSeconds >= 0.f && not bM_TookDescentShot && GetNow() - M_FirstSquadSeenSeconds >= DescentShotSeconds)
	{
		bM_TookDescentShot = true;
		RequestScreenshot(TEXT("Descending"));
	}

	if (bM_SawActiveDrop && not bActive && M_DropFinishedSeconds < 0.f)
	{
		M_DropFinishedSeconds = GetNow();
		RequestScreenshot(TEXT("Landed"));
		UE_LOG(LogRTSEliteScavTest, Display, TEXT("RTS_ELITESCAV_TEST DROP_FINISHED at=%.1fs squads=%d"), Elapsed,
		       M_DroppedSquads.Num());
	}
	if (M_DropFinishedSeconds >= 0.f && GetNow() - M_DropFinishedSeconds >= SettleSeconds)
	{
		FinishParadrop();
		return;
	}
	if (Elapsed >= DropTimeoutSeconds)
	{
		Check(false, TEXT("paradrop_finished_in_time"), FString::Printf(TEXT("active=%d squads=%d"), bActive,
		                                                               M_DroppedSquads.Num()));
		FinishParadrop();
	}
}

void UEliteScavengersTestSubsystem::FinishParadrop()
{
	using namespace EliteScavengersTestPrivate;
	ViewFrom(M_Anchor, 2200.f, -40.f);
	RequestScreenshot(TEXT("Final"));
	Check(bM_SawActiveDrop, TEXT("paradrop_started"));
	// Every Elite Scavengers squad in the world that is not the directly spawned one came from the drop.
	TArray<ASquadController*> DroppedSquads;
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		const URTSComponent* SquadRTS = It->GetRTSComponent();
		if (*It == M_SpawnedSquad.Get() || not IsValid(SquadRTS)
			|| SquadRTS->GetSubtypeAsSquadSubtype() != ESquadSubtype::Squad_Ger_EliteScavengers)
		{
			continue;
		}
		DroppedSquads.Add(*It);
	}
	Check(DroppedSquads.Num() == ExpectedDroppedSquads, TEXT("paradrop_squad_count"),
	      FString::Printf(TEXT("world=%d tracked=%d"), DroppedSquads.Num(), M_DroppedSquads.Num()));
	int32 Index = 0;
	for (ASquadController* Squad : DroppedSquads)
	{
		const FString Label = FString::Printf(TEXT("dropped%d"), Index++);
		Check(not Squad->GetIsInParachuteDrop(), Label + TEXT("_released"));
		CheckSquad(Squad, Label);
		int32 LandedCount = 0;
		FString Positions;
		for (ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
		{
			const UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
			const bool bOnGround = IsValid(Movement) && not Movement->IsFalling();
			const float FromAnchor = FVector::Dist2D(Unit->GetActorLocation(), M_Anchor);
			Positions += FString::Printf(TEXT("%.0f/%s/%s "), FromAnchor, bOnGround ? TEXT("ground") : TEXT("air"),
			                             Unit->IsHidden() ? TEXT("hidden") : TEXT("shown"));
			if (bOnGround && not Unit->IsHidden() && FromAnchor <= MaxLandingDistance)
			{
				++LandedCount;
			}
		}
		Check(LandedCount == ExpectedSoldiers, Label + TEXT("_all_landed_near_target"), Positions);
	}
	Finish();
}

void UEliteScavengersTestSubsystem::ViewFrom(const FVector& Focus, const float Distance, const float Pitch) const
{
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(Pitch, 135.f, 0.f);
	M_Camera->SetActorLocationAndRotation(Focus - ViewRotation.Vector() * Distance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void UEliteScavengersTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("EliteScavengersTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UEliteScavengersTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UEliteScavengersTestSubsystem::Tick(float DeltaTime) {}
TStatId UEliteScavengersTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEliteScavengersTestSubsystem, STATGROUP_Tickables);
}

#endif
