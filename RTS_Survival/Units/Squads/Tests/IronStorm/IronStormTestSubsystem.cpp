// Copyright (C) Bas Blokzijl - All rights reserved.

#include "IronStormTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "LandscapeProxy.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogRTSIronStormTest, Log, All);

namespace IronStormTestPrivate
{
	struct FIronStormDropCase
	{
		EGlobalAbility Ability;
		const TCHAR* Label;
		EGlobalAbilityTier Tier;
		int32 CooldownMin;
		int32 CooldownMax;
		int32 ExpectedRadixite;
		int32 ExpectedIronStormSquads;
		int32 ExpectedPanzerGrenadierSquads;
	};

	// Costs: 1.25x the squads' radixite training cost (Tools/IronStorm/author_iron_storm_paradrops.py).
	const FIronStormDropCase DropCases[] = {
		{EGlobalAbility::GA_GerIronStormParadrop, TEXT("IronStormParadrop"), EGlobalAbilityTier::TierI, 60, 90, 875, 1, 0},
		{
			EGlobalAbility::GA_GerPzGrenIronStormParadrop, TEXT("PzGrenIronStormParadrop"), EGlobalAbilityTier::TierII,
			120, 180, 3450, 2, 2
		},
	};
	constexpr int32 DropCaseCount = UE_ARRAY_COUNT(DropCases);

	constexpr int32 TestPlayer = 1;
	constexpr int32 ExpectedIronStormSoldiers = 4;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float SpawnTimeoutSeconds = 30.f;
	constexpr float SpawnSettleSeconds = 4.f;
	constexpr float DropTimeoutSeconds = 180.f;
	// Seconds the dropped squads get after their release to settle before the final checks.
	constexpr float SettleSeconds = 4.f;
	constexpr float DescentShotSeconds = 3.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float LandscapeEdgeMargin = 2500.f;
	// The directly spawned squad stands this far from the drop anchor, outside the landing spiral.
	constexpr float DirectSpawnOffset = 2500.f;
	// The second drop lands this far from the first, so the two drops' squads never mix.
	constexpr float SecondDropOffset = 4500.f;
	// Landing spread of the templates is 750-1000 cm; four squads stay well inside this.
	constexpr float MaxLandingDistance = 3500.f;
	constexpr float HealthTolerance = 0.51f;
}

bool UIronStormTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("IronStormValidate"));
}

TStatId UIronStormTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UIronStormTestSubsystem, STATGROUP_Tickables);
}

void UIronStormTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EIronStormTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EIronStormTestPhase::WaitingForSpawnedSquad:
		TickWaitingForSpawnedSquad();
		return;
	case EIronStormTestPhase::WaitingForParadrop:
		TickWaitingForParadrop();
		return;
	case EIronStormTestPhase::Finished:
		return;
	}
}

void UIronStormTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSIronStormTest, Display, TEXT("RTS_IRONSTORM_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSIronStormTest, Error, TEXT("RTS_IRONSTORM_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UIronStormTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UIronStormTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSIronStormTest, Display, TEXT("RTS_IRONSTORM_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EIronStormTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UIronStormTestSubsystem::TickWaitingForStart()
{
	using namespace IronStormTestPrivate;
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
	UE_LOG(LogRTSIronStormTest, Display, TEXT("RTS_IRONSTORM_TEST LANDSCAPE min=%s max=%s anchor=%s"),
	       *M_LandscapeBounds.Min.ToCompactString(), *M_LandscapeBounds.Max.ToCompactString(),
	       *M_Anchor.ToCompactString());
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	StartDirectSpawn();
}

bool UIronStormTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace IronStormTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("IronStormAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("IronStormAnchorY="), AnchorY))
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

FBox UIronStormTestSubsystem::FindLandscapeBounds() const
{
	FBox Bounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		Bounds += It->GetComponentsBoundingBox(true);
	}
	return Bounds;
}

FVector UIronStormTestSubsystem::KeepInsideLandscape(const FVector& Location) const
{
	using namespace IronStormTestPrivate;
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

void UIronStormTestSubsystem::StartDirectSpawn()
{
	using namespace IronStormTestPrivate;
	M_Phase = EIronStormTestPhase::WaitingForSpawnedSquad;
	M_PhaseStartSeconds = GetNow();
	M_DirectSpawnLocation = KeepInsideLandscape(M_Anchor + FVector(DirectSpawnOffset, 0.f, 0.f));
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	Check(IsValid(Spawner), TEXT("async_spawner_found"));
	if (not IsValid(Spawner))
	{
		Finish();
		return;
	}
	TWeakObjectPtr<UIronStormTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_IronStorm)),
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
	Check(bRequested, TEXT("spawner_accepted_iron_storm_option"));
	UE_LOG(LogRTSIronStormTest, Display, TEXT("RTS_IRONSTORM_TEST SPAWN_REQUESTED anchor=%s at=%s"),
	       *M_Anchor.ToCompactString(), *M_DirectSpawnLocation.ToCompactString());
	if (not bRequested)
	{
		Finish();
	}
}

void UIronStormTestSubsystem::OnDirectSquadSpawned(AActor* SpawnedActor)
{
	M_SpawnedSquad = Cast<ASquadController>(SpawnedActor);
	UE_LOG(LogRTSIronStormTest, Display, TEXT("RTS_IRONSTORM_TEST SPAWNED %s"),
	       IsValid(SpawnedActor) ? *SpawnedActor->GetClass()->GetName() : TEXT("none"));
}

void UIronStormTestSubsystem::TickWaitingForSpawnedSquad()
{
	using namespace IronStormTestPrivate;
	ViewFrom(M_DirectSpawnLocation, 1200.f, -30.f);
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
	if (GetNow() - M_PhaseStartSeconds < SpawnSettleSeconds)
	{
		return;
	}
	Check(true, TEXT("spawned_squad_loaded"), Squad->GetClass()->GetName());
	// Before the 2026-11-05 fix the spawner's Iron Storm row held Squad_Ger_SturmCommando.
	Check(Squad->GetClass()->GetName().StartsWith(TEXT("Squad_Ger_IronStorm")), TEXT("spawner_row_class"),
	      Squad->GetClass()->GetName());
	CheckIronStormSquad(Squad, TEXT("spawned"));
	RequestScreenshot(TEXT("SpawnedSquad"));
	StartParadrop(0);
}

void UIronStormTestSubsystem::CheckIronStormSquad(ASquadController* Squad, const FString& Label)
{
	using namespace IronStormTestPrivate;
	const URTSComponent* SquadRTS = Squad->GetRTSComponent();
	const ESquadSubtype Subtype = IsValid(SquadRTS) ? SquadRTS->GetSubtypeAsSquadSubtype() : ESquadSubtype::Squad_None;
	Check(Subtype == ESquadSubtype::Squad_Ger_IronStorm, Label + TEXT("_subtype"), UEnum::GetValueAsString(Subtype));
	Check(Squad->GetClass()->GetName().StartsWith(TEXT("Squad_Ger_IronStorm")), Label + TEXT("_class"),
	      Squad->GetClass()->GetName());
	const int32 Owner = IsValid(SquadRTS) ? SquadRTS->GetOwningPlayer() : INDEX_NONE;
	Check(Owner == TestPlayer, Label + TEXT("_owner_player_1"), FString::Printf(TEXT("%d"), Owner));

	const ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	const float ExpectedHealth = IsValid(GameState)
		                             ? GameState->GetSquadDataOfPlayer(TestPlayer, ESquadSubtype::Squad_Ger_IronStorm).MaxHealth
		                             : 0.f;

	TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	Check(Units.Num() == ExpectedIronStormSoldiers, Label + TEXT("_soldier_count"),
	      FString::Printf(TEXT("%d"), Units.Num()));
	int32 StgCount = 0;
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
		WeaponList += FString::Printf(TEXT("%s/%s(%s) "), *Unit->GetClass()->GetName(), *WeaponClassName,
		                              *Global_GetWeaponDisplayName(WeaponName));
		const UHealthComponent* Health = Unit->GetHealthComponent();
		const float MaxHealth = IsValid(Health) ? Health->GetMaxHealth() : -1.f;
		HealthList += FString::Printf(TEXT("%.1f "), MaxHealth);
		if (ExpectedHealth > 0.f && FMath::Abs(MaxHealth - ExpectedHealth) <= HealthTolerance)
		{
			++HealthMatches;
		}
	}
	Check(StgCount == Units.Num() && Units.Num() > 0, Label + TEXT("_all_stg44"),
	      FString::Printf(TEXT("%d: %s"), StgCount, *WeaponList));
	Check(HealthMatches == Units.Num() && Units.Num() > 0, Label + TEXT("_health_is_iron_storm_squad_data"),
	      FString::Printf(TEXT("expected=%.1f units=[%s]"), ExpectedHealth, *HealthList));
}

void UIronStormTestSubsystem::StartParadrop(const int32 DropIndex)
{
	using namespace IronStormTestPrivate;
	M_Phase = EIronStormTestPhase::WaitingForParadrop;
	M_PhaseStartSeconds = GetNow();
	M_DropIndex = DropIndex;
	bM_SawActiveDrop = false;
	bM_TookDescentShot = false;
	M_FirstSquadSeenSeconds = -1.f;
	M_DropFinishedSeconds = -1.f;
	M_KnownSquads.Reset();
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		M_KnownSquads.Add(*It);
	}
	M_DropAnchor = DropIndex == 0
		               ? M_Anchor
		               : KeepInsideLandscape(M_Anchor + FVector(0.f, SecondDropOffset, 0.f));

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
	const FIronStormDropCase& DropCase = DropCases[DropIndex];
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	M_Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(DropCase.Ability);
	UGA_Paratroopers* Paradrop = Cast<UGA_Paratroopers>(M_Ability);
	Check(IsValid(Paradrop), FString(DropCase.Label) + TEXT("_is_paratroopers_ability"),
	      IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Paradrop))
	{
		Finish();
		return;
	}
	CheckDropTemplate(Paradrop, DropIndex);
	Paradrop->InitGlobalAbility(TestPlayer, Manager, Controller);
	Paradrop->ExecuteAbilityAtLocation(M_DropAnchor);
	UE_LOG(LogRTSIronStormTest, Display, TEXT("RTS_IRONSTORM_TEST PARADROP_EXECUTED %s anchor=%s"), DropCase.Label,
	       *M_DropAnchor.ToCompactString());
}

void UIronStormTestSubsystem::CheckDropTemplate(const UGlobalAbility* Ability, const int32 DropIndex)
{
	using namespace IronStormTestPrivate;
	const FIronStormDropCase& DropCase = DropCases[DropIndex];
	const FString Label(DropCase.Label);
	const FGLobalAbilityCostState& Costs = Ability->GetAbilityCosts();
	const int32* Radixite = Costs.Costs.ResourceCosts.Find(ERTSResourceType::Resource_Radixite);
	Check(Ability->GetAbilityTier() == DropCase.Tier, Label + TEXT("_tier"),
	      Global_GetGlobalAbilityTierDisplayName(Ability->GetAbilityTier()));
	Check(Costs.CoolDownTime >= DropCase.CooldownMin && Costs.CoolDownTime <= DropCase.CooldownMax,
	      Label + TEXT("_cooldown_in_tier_band"), FString::Printf(TEXT("%d"), Costs.CoolDownTime));
	Check(Radixite != nullptr && *Radixite == DropCase.ExpectedRadixite && Costs.Costs.ResourceCosts.Num() == 1,
	      Label + TEXT("_cost_radixite_only"),
	      FString::Printf(TEXT("radixite=%d resources=%d expected=%d"), Radixite ? *Radixite : 0,
	                      Costs.Costs.ResourceCosts.Num(), DropCase.ExpectedRadixite));
}

void UIronStormTestSubsystem::TickWaitingForParadrop()
{
	using namespace IronStormTestPrivate;
	UGA_Paratroopers* Paradrop = Cast<UGA_Paratroopers>(M_Ability);
	if (not IsValid(Paradrop))
	{
		Check(false, TEXT("paradrop_alive"));
		Finish();
		return;
	}
	const FString Label(DropCases[M_DropIndex].Label);
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	const bool bActive = Paradrop->GetHasActiveDrop();
	bM_SawActiveDrop |= bActive;
	const bool bAnySquad = bActive && not Paradrop->GetActiveDropSquads().IsEmpty();

	// Camera: wide on the aircraft until the first squads exist, then on the landing area.
	const AAircraftMaster* Aircraft = Paradrop->GetActiveDropAircraft();
	if (M_FirstSquadSeenSeconds < 0.f && IsValid(Aircraft))
	{
		ViewFrom(Aircraft->GetActorLocation(), 6000.f, -30.f);
	}
	else
	{
		ViewFrom(M_DropAnchor + FVector(0.f, 0.f, 900.f), 3800.f, -25.f);
	}
	if (bAnySquad && M_FirstSquadSeenSeconds < 0.f)
	{
		M_FirstSquadSeenSeconds = GetNow();
		RequestScreenshot(Label + TEXT("_DropStarted"));
	}
	if (M_FirstSquadSeenSeconds >= 0.f && not bM_TookDescentShot && GetNow() - M_FirstSquadSeenSeconds >= DescentShotSeconds)
	{
		bM_TookDescentShot = true;
		RequestScreenshot(Label + TEXT("_Descending"));
	}
	if (bM_SawActiveDrop && not bActive && M_DropFinishedSeconds < 0.f)
	{
		M_DropFinishedSeconds = GetNow();
		RequestScreenshot(Label + TEXT("_Landed"));
		UE_LOG(LogRTSIronStormTest, Display, TEXT("RTS_IRONSTORM_TEST DROP_FINISHED %s at=%.1fs"), *Label, Elapsed);
	}
	if (M_DropFinishedSeconds >= 0.f && GetNow() - M_DropFinishedSeconds >= SettleSeconds)
	{
		FinishParadrop();
		return;
	}
	if (Elapsed >= DropTimeoutSeconds)
	{
		Check(false, Label + TEXT("_finished_in_time"), FString::Printf(TEXT("active=%d"), bActive));
		FinishParadrop();
	}
}

bool UIronStormTestSubsystem::GetIsKnownSquad(const ASquadController* Squad) const
{
	for (const TWeakObjectPtr<ASquadController>& Known : M_KnownSquads)
	{
		if (Known.Get() == Squad)
		{
			return true;
		}
	}
	return false;
}

void UIronStormTestSubsystem::FinishParadrop()
{
	using namespace IronStormTestPrivate;
	const FIronStormDropCase& DropCase = DropCases[M_DropIndex];
	const FString Label(DropCase.Label);
	ViewFrom(M_DropAnchor, 2400.f, -40.f);
	RequestScreenshot(Label + TEXT("_Final"));
	Check(bM_SawActiveDrop, Label + TEXT("_started"));

	int32 IronStormSquads = 0;
	int32 PanzerGrenadierSquads = 0;
	int32 OtherSquads = 0;
	int32 SquadIndex = 0;
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		ASquadController* Squad = *It;
		if (GetIsKnownSquad(Squad))
		{
			continue;
		}
		const URTSComponent* SquadRTS = Squad->GetRTSComponent();
		const ESquadSubtype Subtype = IsValid(SquadRTS) ? SquadRTS->GetSubtypeAsSquadSubtype() : ESquadSubtype::Squad_None;
		const bool bPlayerSquad = IsValid(SquadRTS) && SquadRTS->GetOwningPlayer() == TestPlayer;
		if (not bPlayerSquad && FVector::Dist2D(Squad->GetActorLocation(), M_DropAnchor) > 2.f * MaxLandingDistance)
		{
			// The map's own enemy squads may spawn while the drop flies; they are not part of it.
			UE_LOG(LogRTSIronStormTest, Display, TEXT("RTS_IRONSTORM_TEST IGNORED_FAR_SQUAD %s at %s"),
			       *Squad->GetClass()->GetName(), *Squad->GetActorLocation().ToCompactString());
			continue;
		}
		const FString SquadLabel = FString::Printf(TEXT("%s_squad%d"), *Label, SquadIndex++);
		CheckDroppedSquadLanded(Squad, SquadLabel);
		if (Subtype == ESquadSubtype::Squad_Ger_IronStorm)
		{
			++IronStormSquads;
			CheckIronStormSquad(Squad, SquadLabel);
			continue;
		}
		if (Subtype == ESquadSubtype::Squad_Ger_PanzerGrenadiere)
		{
			++PanzerGrenadierSquads;
			continue;
		}
		++OtherSquads;
		UE_LOG(LogRTSIronStormTest, Warning, TEXT("RTS_IRONSTORM_TEST UNEXPECTED_SQUAD %s %s"),
		       *Squad->GetClass()->GetName(), *UEnum::GetValueAsString(Subtype));
	}
	Check(IronStormSquads == DropCase.ExpectedIronStormSquads, Label + TEXT("_iron_storm_squads"),
	      FString::Printf(TEXT("%d expected %d"), IronStormSquads, DropCase.ExpectedIronStormSquads));
	Check(PanzerGrenadierSquads == DropCase.ExpectedPanzerGrenadierSquads, Label + TEXT("_panzergrenadier_squads"),
	      FString::Printf(TEXT("%d expected %d"), PanzerGrenadierSquads, DropCase.ExpectedPanzerGrenadierSquads));
	Check(OtherSquads == 0, Label + TEXT("_no_other_squads"), FString::Printf(TEXT("%d"), OtherSquads));

	if (M_DropIndex + 1 < DropCaseCount)
	{
		StartParadrop(M_DropIndex + 1);
		return;
	}
	Finish();
}

void UIronStormTestSubsystem::CheckDroppedSquadLanded(ASquadController* Squad, const FString& Label)
{
	using namespace IronStormTestPrivate;
	const URTSComponent* SquadRTS = Squad->GetRTSComponent();
	const int32 Owner = IsValid(SquadRTS) ? SquadRTS->GetOwningPlayer() : INDEX_NONE;
	Check(Owner == TestPlayer, Label + TEXT("_owned_by_player_1"),
	      FString::Printf(TEXT("%s owner=%d"), *Squad->GetClass()->GetName(), Owner));
	Check(not Squad->GetIsInParachuteDrop(), Label + TEXT("_released"));
	const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
	int32 LandedCount = 0;
	FString Positions;
	for (ASquadUnit* Unit : Units)
	{
		const UCharacterMovementComponent* Movement = Unit->GetCharacterMovement();
		const bool bOnGround = IsValid(Movement) && not Movement->IsFalling();
		const float FromAnchor = FVector::Dist2D(Unit->GetActorLocation(), M_DropAnchor);
		Positions += FString::Printf(TEXT("%.0f/%s/%s "), FromAnchor, bOnGround ? TEXT("ground") : TEXT("air"),
		                             Unit->IsHidden() ? TEXT("hidden") : TEXT("shown"));
		if (bOnGround && not Unit->IsHidden() && FromAnchor <= MaxLandingDistance)
		{
			++LandedCount;
		}
	}
	Check(LandedCount == Units.Num() && Units.Num() > 0, Label + TEXT("_all_landed_near_target"),
	      FString::Printf(TEXT("%s %d/%d: %s"), *Squad->GetClass()->GetName(), LandedCount, Units.Num(), *Positions));
}

void UIronStormTestSubsystem::ViewFrom(const FVector& Focus, const float Distance, const float Pitch) const
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

void UIronStormTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("IronStormTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UIronStormTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UIronStormTestSubsystem::Tick(float DeltaTime) {}
TStatId UIronStormTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UIronStormTestSubsystem, STATGROUP_Tickables);
}

#endif
