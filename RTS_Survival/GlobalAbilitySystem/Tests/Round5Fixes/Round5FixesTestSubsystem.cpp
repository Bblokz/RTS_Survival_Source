// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Round5FixesTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/CommanderSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityCostState.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftDrop/GA_AircraftDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/GlobalAbilityPanel/W_GlobalAbilityPanel.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/W_GA_Item/W_GA_Item.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponState/TeamWeaponState.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSRound5Test, Log, All);

namespace Round5FixesTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float LandscapeEdgeMargin = 2500.f;

	// Range step. The leFH 18 reaches HowitzerArtilleryRange (8800); the target lies between the two ranges.
	constexpr float ExpectedLeFH18Range = 8800.f;
	constexpr float BeyondLeFHTargetDistance = 9700.f;
	constexpr float LeFHControlFraction = 0.6f;
	constexpr float GunSpawnTimeoutSeconds = 60.f;
	constexpr float GunSettleSeconds = 8.f;
	constexpr float FireWindowSeconds = 45.f;
	constexpr float ControlShotTimeoutSeconds = 45.f;
	// The two guns stand this far apart across the firing direction.
	constexpr float GunSideOffset = 1600.f;
	// The guns fire back toward the map edge side only if the centre is too close: keep their targets on the map.
	constexpr float GunBackOffset = 4000.f;

	// Cargo door step.
	const FName CargoDoorSocket = TEXT("CargoDoor");
	const TCHAR* const DropAircraftMarker = TEXT("JU390");
	constexpr int32 PairDropSquads = 2;
	constexpr float DropTimeoutSeconds = 200.f;
	constexpr float SampleIntervalSeconds = 0.5f;
	constexpr float ScreenshotIntervalSeconds = 1.f;
	constexpr int32 MaxWalkOutScreenshots = 14;
	// A squad counts as appearing at the door when its walkers' centre is this close to the door's ground point.
	constexpr float AtDoorRadius = 450.f;
	// ...and as out when its centre is this close to its spot on the circle.
	constexpr float AtSpotRadius = 500.f;
	constexpr float WalkOutTimeoutSeconds = 45.f;
	constexpr float ReleaseMoveDistance = 1200.f;
	constexpr float ReleaseMoveTimeoutSeconds = 30.f;
	constexpr float ReleaseMinMoved = 800.f;
	constexpr float MaxLandingDistance = 3500.f;

	// Mission caps.
	constexpr float MissionCapsTimeoutSeconds = 120.f;

	EGlobalAbilityTier ParseTier(const FString& Text, bool& bOutValid)
	{
		const UEnum* TierEnum = StaticEnum<EGlobalAbilityTier>();
		const int64 Value = TierEnum->GetValueByNameString(Text);
		bOutValid = Value != INDEX_NONE;
		return bOutValid ? static_cast<EGlobalAbilityTier>(Value) : EGlobalAbilityTier::TierIV;
	}

	FGlobalAbilityRequirements* GetMutableRequirements(UGlobalAbility* Ability)
	{
		const FStructProperty* RequirementsProperty = CastField<FStructProperty>(
			UGlobalAbility::StaticClass()->FindPropertyByName(TEXT("M_AbilityRequirements")));
		if (RequirementsProperty == nullptr || not IsValid(Ability))
		{
			return nullptr;
		}
		return RequirementsProperty->ContainerPtrToValuePtr<FGlobalAbilityRequirements>(Ability);
	}

	const FGlobalAbilityRequirements* GetRequirements(const UGlobalAbility* Ability)
	{
		const FStructProperty* RequirementsProperty = CastField<FStructProperty>(
			UGlobalAbility::StaticClass()->FindPropertyByName(TEXT("M_AbilityRequirements")));
		if (RequirementsProperty == nullptr || not IsValid(Ability))
		{
			return nullptr;
		}
		return RequirementsProperty->ContainerPtrToValuePtr<FGlobalAbilityRequirements>(Ability);
	}

	const FAircraftDropGlobalAbilitySettings* GetDropSettings(const UGlobalAbility* Ability)
	{
		const FStructProperty* SettingsProperty = CastField<FStructProperty>(
			UGA_AircraftDrop::StaticClass()->FindPropertyByName(TEXT("M_AircraftDropSettings")));
		if (SettingsProperty == nullptr || not IsValid(Cast<UGA_AircraftDrop>(Ability)))
		{
			return nullptr;
		}
		return SettingsProperty->ContainerPtrToValuePtr<FAircraftDropGlobalAbilitySettings>(Ability);
	}
}

void URound5FixesTestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// A mission map started directly has no commander, so the player's kit would be empty: pick the Field
	// Commander before the controller loads the kit (a mission manager that sets one manually still overrides it).
	URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (FCString::Strifind(FCommandLine::Get(), TEXT("Round5MissionCaps=")) != nullptr && GameInstance
		&& GameInstance->GetPlayerCommander() == ERTSCommander::NoCommander)
	{
		GameInstance->SetPlayerCommander(ERTSCommander::BalancedCommander);
	}
}

bool URound5FixesTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("Round5FixesValidate"));
}

TStatId URound5FixesTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URound5FixesTestSubsystem, STATGROUP_Tickables);
}

void URound5FixesTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case ERound5FixesTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case ERound5FixesTestPhase::TemplateChecks:
		RunTemplateChecks();
		return;
	case ERound5FixesTestPhase::WaitingForGuns:
		TickWaitingForGuns();
		return;
	case ERound5FixesTestPhase::GunsSettling:
		TickGunsSettling();
		return;
	case ERound5FixesTestPhase::FiringBeyondLeFH:
		TickFiringBeyondLeFH();
		return;
	case ERound5FixesTestPhase::LeFHControlShot:
		TickLeFHControlShot();
		return;
	case ERound5FixesTestPhase::RunningCargoDoorDrop:
		TickRunningCargoDoorDrop();
		return;
	case ERound5FixesTestPhase::ReleasedMoveCheck:
		TickReleasedMoveCheck();
		return;
	case ERound5FixesTestPhase::MissionCaps:
		TickMissionCaps();
		return;
	case ERound5FixesTestPhase::Finished:
		return;
	}
}

void URound5FixesTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSRound5Test, Error, TEXT("RTS_ROUND5_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float URound5FixesTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

float URound5FixesTestSubsystem::GetPhaseSeconds() const
{
	return GetNow() - M_PhaseStartSeconds;
}

void URound5FixesTestSubsystem::EnterPhase(const ERound5FixesTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
	UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST PHASE %s at=%.1f"), *UEnum::GetValueAsString(NewPhase),
	       GetNow());
}

void URound5FixesTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = ERound5FixesTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void URound5FixesTestSubsystem::TickWaitingForStart()
{
	using namespace Round5FixesTestPrivate;
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
	if (FParse::Param(FCommandLine::Get(), TEXT("Round5MissionCaps"))
		|| FCString::Strifind(FCommandLine::Get(), TEXT("Round5MissionCaps=")) != nullptr)
	{
		EnterPhase(ERound5FixesTestPhase::MissionCaps);
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
	UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST LANDSCAPE min=%s max=%s anchor=%s to_centre=%s"),
	       *M_LandscapeBounds.Min.ToCompactString(), *M_LandscapeBounds.Max.ToCompactString(),
	       *M_Anchor.ToCompactString(), *M_TowardCentre.ToCompactString());
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	EnterPhase(ERound5FixesTestPhase::TemplateChecks);
}

bool URound5FixesTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace Round5FixesTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("Round5AnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("Round5AnchorY="), AnchorY))
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

UGlobalAbilitiesManager* URound5FixesTestSubsystem::GetPlayerAbilityManager() const
{
	const ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	return IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
}

UGlobalAbility* URound5FixesTestSubsystem::MakeAbilityCopy(const EGlobalAbility AbilityType)
{
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	if (not IsValid(AbilitySet))
	{
		return nullptr;
	}
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	return AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(AbilityType);
}

bool URound5FixesTestSubsystem::EnsureCanAfford(const UGlobalAbility& Ability)
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	if (not IsValid(Resources))
	{
		Check(false, TEXT("player_resource_manager"));
		return false;
	}
	bool bCanAfford = true;
	for (const TPair<ERTSResourceType, int32>& Cost : Ability.GetAbilityCosts().Costs.ResourceCosts)
	{
		const int32 Missing = Cost.Value - Resources->GetResourceAmount(Cost.Key);
		if (Missing > 0)
		{
			// A bare test map has no HQ, so no storage: add both, as the cheat does.
			Resources->CheatAddStorage(Cost.Key, Missing);
		}
		bCanAfford &= Resources->GetResourceAmount(Cost.Key) >= Cost.Value;
	}
	return bCanAfford;
}

// ---------------------------------------------------------------------------------------------------------------
// 1) Templates
// ---------------------------------------------------------------------------------------------------------------

void URound5FixesTestSubsystem::RunTemplateChecks()
{
	CheckTemplateRequirements();
	CheckBf109Wording();
	CheckCargoDoorFlags();
	ClickJu87StrikeWithoutTechs();
	StartGuns();
}

void URound5FixesTestSubsystem::CheckTemplateRequirements()
{
	using namespace Round5FixesTestPrivate;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	const UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                               ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                               : nullptr;
	Check(IsValid(AbilitySet), TEXT("ability_set_loaded"));
	if (not IsValid(AbilitySet))
	{
		return;
	}
	const UEnum* AbilityEnum = StaticEnum<EGlobalAbility>();
	int32 Templates = 0;
	int32 WithTech = 0;
	FString Offenders;
	for (int32 Index = 0; Index < AbilityEnum->NumEnums() - 1; ++Index)
	{
		const EGlobalAbility Type = static_cast<EGlobalAbility>(AbilityEnum->GetValueByIndex(Index));
		const UGlobalAbility* Template = AbilitySet->FindAbilityTemplate(Type);
		const FGlobalAbilityRequirements* Requirements = GetRequirements(Template);
		if (Requirements == nullptr)
		{
			continue;
		}
		++Templates;
		const FString TypeName = AbilityEnum->GetNameStringByIndex(Index);
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST TEMPLATE %s tier=%s tech=%s unit=%s"), *TypeName,
		       *UEnum::GetValueAsString(Template->GetAbilityTier()),
		       *UEnum::GetValueAsString(Requirements->RequiredTechnology),
		       *Requirements->RequiredUnit.GetTrainingName());
		// For now no global ability needs a technology or a building/unit (2026-11-07).
		if (Requirements->RequiredTechnology != ETechnology::Tech_NONE || not Requirements->RequiredUnit.IsNone())
		{
			++WithTech;
			Offenders += TypeName + TEXT("=") + UEnum::GetValueAsString(Requirements->RequiredTechnology) + TEXT("/")
				+ Requirements->RequiredUnit.GetTrainingName() + TEXT(" ");
		}
	}
	Check(Templates > 0 && WithTech == 0, TEXT("no_template_requires_a_technology_or_unit"),
	      FString::Printf(TEXT("templates=%d with_tech=%d %s"), Templates, WithTech, *Offenders));
}

void URound5FixesTestSubsystem::CheckBf109Wording()
{
	const UGlobalAbility* Template = nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	const UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                               ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                               : nullptr;
	Template = IsValid(AbilitySet) ? AbilitySet->FindAbilityTemplate(EGlobalAbility::GA_2xBf110Strafing) : nullptr;
	if (not IsValid(Template))
	{
		Check(false, TEXT("bf109_strafing_template_exists"));
		return;
	}
	const FString Title = Template->GetUISettings().Title.ToString();
	const FString Description = Template->GetUISettings().Description.ToString();
	UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST BF109 title=\"%s\" description=\"%s\""), *Title,
	       *Description);
	Check(Title.Contains(TEXT("Bf 109")) && not Title.Contains(TEXT("110")), TEXT("bf109_strafing_title"), Title);
	Check(not Description.Contains(TEXT("110")), TEXT("bf109_strafing_description_has_no_bf110"), Description);
}

void URound5FixesTestSubsystem::CheckCargoDoorFlags()
{
	using namespace Round5FixesTestPrivate;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	const UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                               ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                               : nullptr;
	if (not IsValid(AbilitySet))
	{
		return;
	}
	struct FFlagCase
	{
		EGlobalAbility Ability;
		bool bExpected;
	};
	// Red Spine drops walk out; a vehicle and an ordinary infantry drop keep today's behaviour.
	const FFlagCase Cases[] = {
		{EGlobalAbility::GA_GerRedSpineDrop, true},
		{EGlobalAbility::GA_GerRedSpinePairDrop, true},
		{EGlobalAbility::GA_GerSpotterPzIIIDrop, false},
		{EGlobalAbility::GA_ScavengersDrop, false},
	};
	for (const FFlagCase& Case : Cases)
	{
		const FAircraftDropGlobalAbilitySettings* Settings = GetDropSettings(AbilitySet->FindAbilityTemplate(Case.Ability));
		const FString Label = UEnum::GetValueAsString(Case.Ability);
		if (Settings == nullptr)
		{
			Check(not Case.bExpected, TEXT("cargo_door_flag ") + Label, TEXT("not an aircraft drop"));
			continue;
		}
		Check(Settings->bSquadsWalkOutOfCargoDoor == Case.bExpected, TEXT("cargo_door_flag ") + Label,
		      FString::Printf(TEXT("walk_out=%d expected=%d"), Settings->bSquadsWalkOutOfCargoDoor, Case.bExpected));
	}
	// Reported, not required: the Fels Krieger drop is authored by its own agent.
	const FAircraftDropGlobalAbilitySettings* FelsKrieger = GetDropSettings(
		AbilitySet->FindAbilityTemplate(EGlobalAbility::GA_GerFelsKriegerDrop));
	UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST FELS_KRIEGER_DROP walk_out=%s"),
	       FelsKrieger == nullptr ? TEXT("no template") : FelsKrieger->bSquadsWalkOutOfCargoDoor ? TEXT("1") : TEXT("0"));
}

void URound5FixesTestSubsystem::ClickJu87StrikeWithoutTechs()
{
	using namespace Round5FixesTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = GetPlayerAbilityManager();
	UGlobalAbility* Ability = MakeAbilityCopy(EGlobalAbility::GA_3xJu87Strafing);
	Check(IsValid(Controller) && IsValid(Manager) && IsValid(Ability), TEXT("ju87_strike_copy"));
	if (not IsValid(Controller) || not IsValid(Manager) || not IsValid(Ability))
	{
		return;
	}
	M_StartedAbilities.Add(Ability);
	Ability->InitGlobalAbility(TestPlayer, Manager, Controller);
	const UPlayerTechManager* TechManager = Controller->GetPlayerTechManager();
	const FGlobalAbilityRequirements* Requirements = GetRequirements(Ability);
	const bool bTechNone = Requirements != nullptr && Requirements->RequiredTechnology == ETechnology::Tech_NONE;
	Check(bTechNone, TEXT("ju87_strike_requires_no_technology"),
	      Requirements ? UEnum::GetValueAsString(Requirements->RequiredTechnology) : TEXT("no requirements"));
	Check(IsValid(TechManager) && not TechManager->HasTechResearched(ETechnology::Tech_HarvesterSpeed),
	      TEXT("harvester_speed_not_researched"));
	Check(Manager->QueryTechnologyRequirementForAbility(Ability), TEXT("ju87_strike_tech_gate_open"));
	// Only the technology gate is under test: a building requirement (a bare test map has no buildings) is logged and
	// cleared on this copy.
	if (FGlobalAbilityRequirements* MutableRequirements = GetMutableRequirements(Ability))
	{
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST JU87_REQUIREMENTS tech=%s unit=%s unit_met=%d"),
		       *UEnum::GetValueAsString(MutableRequirements->RequiredTechnology),
		       *MutableRequirements->RequiredUnit.GetTrainingName(), Manager->QueryUnitRequirementForAbility(Ability));
		MutableRequirements->RequiredUnit = FTrainingOption();
	}
	if (not EnsureCanAfford(*Ability))
	{
		Check(false, TEXT("ju87_strike_affordable"));
		return;
	}
	// The map has no mission manager: the whole kit is allowed (Tier III fits).
	Check(Manager->QueryTierAllowedForAbility(Ability), TEXT("ju87_strike_tier_allowed"));
	UE_LOG(LogRTSRound5Test, Display,
	       TEXT("RTS_ROUND5_TEST JU87_STATE requirements=%d tech=%d unit=%d costs=%d cooldown_ready=%d"),
	       Manager->QueryRequirementForAbility(Ability), Manager->QueryTechnologyRequirementForAbility(Ability),
	       Manager->QueryUnitRequirementForAbility(Ability), Manager->QueryCostsForAbility(Ability),
	       Manager->QueryCooldownForAbility(Ability));

	// The player's path: activate from the panel, then click the target (a strip needs two clicks).
	const UPlayerResourceManager* Resources = Controller->GetPlayerResourceManager();
	const FGLobalAbilityCostState Costs = Ability->GetAbilityCosts();
	TMap<ERTSResourceType, int32> Before;
	for (const TPair<ERTSResourceType, int32>& Cost : Costs.Costs.ResourceCosts)
	{
		Before.Add(Cost.Key, IsValid(Resources) ? Resources->GetResourceAmount(Cost.Key) : 0);
	}
	// The strike flies over a point well away from where the guns and the drop will be.
	const FVector StrikePoint = KeepInsideLandscape(M_Anchor + M_TowardCentre * 6000.f);
	const bool bIsStrip = Ability->GetAimSettings().AreaAim.Shape == EGlobalAbilityAimShape::Strip;
	Controller->OnGlobaAbilityActivated(Ability->GetAimSettings(), FGlobalAbilitySoundSettings(), Ability);
	if (bIsStrip)
	{
		const FVector Half(1000.f, 0.f, 0.f);
		Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(StrikePoint - Half), TEXT("ju87_strip_anchored"));
		Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(StrikePoint + Half), TEXT("ju87_click_handled"));
	}
	else
	{
		Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(StrikePoint), TEXT("ju87_click_handled"));
	}
	bool bPaid = IsValid(Resources) && Costs.Costs.ResourceCosts.Num() > 0;
	FString PaidText;
	for (const TPair<ERTSResourceType, int32>& Cost : Costs.Costs.ResourceCosts)
	{
		const int32 Paid = Before.FindRef(Cost.Key) - (IsValid(Resources) ? Resources->GetResourceAmount(Cost.Key) : 0);
		bPaid &= Paid == Cost.Value;
		PaidText += FString::Printf(TEXT("%s paid=%d "), *UEnum::GetValueAsString(Cost.Key), Paid);
	}
	Check(bPaid, TEXT("ju87_strike_executed_and_paid"), PaidText);
	Check(Ability->GetAbilityCosts().CoolDownRemaining == Costs.CoolDownTime, TEXT("ju87_strike_cooldown_started"),
	      FString::Printf(TEXT("remaining=%d of %d"), Ability->GetAbilityCosts().CoolDownRemaining, Costs.CoolDownTime));
}

// ---------------------------------------------------------------------------------------------------------------
// 2) sFH 18 outranges the leFH 18
// ---------------------------------------------------------------------------------------------------------------

void URound5FixesTestSubsystem::StartGuns()
{
	using namespace Round5FixesTestPrivate;
	EnterPhase(ERound5FixesTestPhase::WaitingForGuns);
	// The guns stand back from the anchor and fire toward the map centre, so their targets stay on the map.
	const FVector Side(-M_TowardCentre.Y, M_TowardCentre.X, 0.f);
	const FVector Base = KeepInsideLandscape(M_Anchor - M_TowardCentre * GunBackOffset);
	SpawnGunSquad(static_cast<uint8>(ESquadSubtype::Squad_Ger_SFH18_150mm), KeepInsideLandscape(Base + Side * GunSideOffset),
	              true);
	SpawnGunSquad(static_cast<uint8>(ESquadSubtype::Squad_Ger_LefH18), KeepInsideLandscape(Base - Side * GunSideOffset),
	              false);
}

void URound5FixesTestSubsystem::SpawnGunSquad(const uint8 SquadSubtype, const FVector& Location, const bool bIsSfh18)
{
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner))
	{
		Check(false, TEXT("async_spawner_found"));
		return;
	}
	TWeakObjectPtr<URound5FixesTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Squad, SquadSubtype),
		Location,
		this,
		0,
		[WeakThis, bIsSfh18](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			ASquadController* Squad = Cast<ASquadController>(SpawnedActor);
			if (bIsSfh18)
			{
				WeakThis->M_Sfh18Squad = Squad;
			}
			else
			{
				WeakThis->M_LeFH18Squad = Squad;
			}
		},
		M_TowardCentre.Rotation());
	Check(bRequested, bIsSfh18 ? TEXT("sfh18_spawn_requested") : TEXT("lefh18_spawn_requested"),
	      Location.ToCompactString());
}

ATeamWeapon* URound5FixesTestSubsystem::GetGun(ASquadController* Squad)
{
	ATeamWeaponController* TeamWeaponController = Cast<ATeamWeaponController>(Squad);
	return IsValid(TeamWeaponController) ? TeamWeaponController->GetControlledTeamWeaponNoReport() : nullptr;
}

UWeaponState* URound5FixesTestSubsystem::GetMainWeapon(ATeamWeapon* Gun)
{
	if (not IsValid(Gun))
	{
		return nullptr;
	}
	TArray<UWeaponState*> Weapons = Gun->GetWeapons();
	return Weapons.Num() > 0 && IsValid(Weapons[0]) ? Weapons[0] : nullptr;
}

bool URound5FixesTestSubsystem::GetHasFired(ASquadController* Squad)
{
	// One round in the magazine: the shot starts the reload, so a gun that is no longer fully loaded has fired.
	const UWeaponState* Weapon = GetMainWeapon(GetGun(Squad));
	return IsValid(Weapon) && not Weapon->IsWeaponFullyLoaded();
}

void URound5FixesTestSubsystem::TickWaitingForGuns()
{
	using namespace Round5FixesTestPrivate;
	auto IsDeployed = [](ASquadController* Squad)
	{
		const ATeamWeaponController* TeamWeaponController = Cast<ATeamWeaponController>(Squad);
		return IsValid(TeamWeaponController) && IsValid(GetGun(Squad))
			&& TeamWeaponController->GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed;
	};
	const bool bBothDeployed = IsDeployed(M_Sfh18Squad.Get()) && IsDeployed(M_LeFH18Squad.Get());
	if (bBothDeployed)
	{
		Check(true, TEXT("both_guns_deployed"), FString::Printf(TEXT("after=%.1fs"), GetPhaseSeconds()));
		EnterPhase(ERound5FixesTestPhase::GunsSettling);
		return;
	}
	if (GetPhaseSeconds() >= GunSpawnTimeoutSeconds)
	{
		Check(false, TEXT("both_guns_deployed"), FString::Printf(TEXT("sfh18=%d lefh18=%d"),
		                                                        IsDeployed(M_Sfh18Squad.Get()),
		                                                        IsDeployed(M_LeFH18Squad.Get())));
		StartCargoDoorDrop();
	}
}

void URound5FixesTestSubsystem::TickGunsSettling()
{
	using namespace Round5FixesTestPrivate;
	if (GetPhaseSeconds() < GunSettleSeconds)
	{
		return;
	}
	ATeamWeaponController* Sfh18 = Cast<ATeamWeaponController>(M_Sfh18Squad.Get());
	ATeamWeaponController* LeFH18 = Cast<ATeamWeaponController>(M_LeFH18Squad.Get());
	if (not IsValid(Sfh18) || not IsValid(LeFH18))
	{
		Check(false, TEXT("guns_still_valid"));
		StartCargoDoorDrop();
		return;
	}
	const float Sfh18Range = Sfh18->GetTeamWeaponRange();
	const float LeFH18Range = LeFH18->GetTeamWeaponRange();
	const UWeaponState* Sfh18Weapon = GetMainWeapon(GetGun(Sfh18));
	const UWeaponState* LeFH18Weapon = GetMainWeapon(GetGun(LeFH18));
	UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST RANGES sfh18_team_weapon=%.0f sfh18_weapon=%.0f "
		       "lefh18_team_weapon=%.0f lefh18_weapon=%.0f"), Sfh18Range,
	       IsValid(Sfh18Weapon) ? Sfh18Weapon->GetRange() : -1.f, LeFH18Range,
	       IsValid(LeFH18Weapon) ? LeFH18Weapon->GetRange() : -1.f);
	Check(FMath::IsNearlyEqual(LeFH18Range, ExpectedLeFH18Range, 50.f),
	      TEXT("lefh18_range_is_8800"), FString::Printf(TEXT("%.0f"), LeFH18Range));
	Check(Sfh18Range > BeyondLeFHTargetDistance && Sfh18Range > LeFH18Range, TEXT("sfh18_range_beyond_lefh18"),
	      FString::Printf(TEXT("sfh18=%.0f lefh18=%.0f target=%.0f"), Sfh18Range, LeFH18Range,
	                      BeyondLeFHTargetDistance));
	Check(not GetHasFired(Sfh18) && not GetHasFired(LeFH18), TEXT("both_guns_loaded_before_order"));
	M_Sfh18Target = OrderAttackGround(Sfh18, BeyondLeFHTargetDistance);
	M_LeFH18Target = OrderAttackGround(LeFH18, BeyondLeFHTargetDistance);
	bM_Sfh18Fired = false;
	bM_LeFH18Fired = false;
	const FVector Mid = (GetGun(Sfh18)->GetActorLocation() + GetGun(LeFH18)->GetActorLocation()) * 0.5f;
	ViewFrom(Mid + FVector(0.f, 0.f, 150.f), 2600.f, -18.f, M_TowardCentre.Rotation().Yaw + 150.f);
	RequestScreenshot(TEXT("Guns_Ordered"));
	EnterPhase(ERound5FixesTestPhase::FiringBeyondLeFH);
}

FVector URound5FixesTestSubsystem::OrderAttackGround(ASquadController* Squad, const float Distance)
{
	ATeamWeapon* Gun = GetGun(Squad);
	if (not IsValid(Gun))
	{
		return FVector::ZeroVector;
	}
	const FVector RawTarget = Gun->GetActorLocation() + M_TowardCentre * Distance;
	FVector Target = RawTarget;
	const bool bOnGround = GlobalAbilityHelpers::TryFindGroundLocation(this, RawTarget, Target);
	const ECommandQueueError Error = Squad->AttackGround(Target, true);
	Check(bOnGround && Error == ECommandQueueError::NoError, TEXT("attack_ground_order ") + Squad->GetName(),
	      FString::Printf(TEXT("distance=%.0f (2D %.0f) target=%s error=%d"), Distance,
	                      FVector::Dist2D(Gun->GetActorLocation(), Target), *Target.ToCompactString(),
	                      static_cast<int32>(Error)));
	return Target;
}

void URound5FixesTestSubsystem::TickFiringBeyondLeFH()
{
	using namespace Round5FixesTestPrivate;
	ASquadController* Sfh18 = M_Sfh18Squad.Get();
	ASquadController* LeFH18 = M_LeFH18Squad.Get();
	if (not bM_Sfh18Fired && GetHasFired(Sfh18))
	{
		bM_Sfh18Fired = true;
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST SFH18_FIRED after=%.1fs distance=%.0f"),
		       GetPhaseSeconds(), FVector::Dist2D(GetGun(Sfh18)->GetActorLocation(), M_Sfh18Target));
		RequestScreenshot(TEXT("Sfh18_Fired_Beyond_8800"));
	}
	if (not bM_LeFH18Fired && GetHasFired(LeFH18))
	{
		bM_LeFH18Fired = true;
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST LEFH18_FIRED after=%.1fs"), GetPhaseSeconds());
	}
	if (GetPhaseSeconds() < FireWindowSeconds)
	{
		return;
	}
	Check(bM_Sfh18Fired, TEXT("sfh18_fires_beyond_lefh18_range"),
	      FString::Printf(TEXT("target %.0f cm"), BeyondLeFHTargetDistance));
	const ATeamWeapon* LeFH18Gun = GetGun(LeFH18);
	UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST LEFH18_AFTER_WINDOW gun=%s target_dist=%.0f"),
	       IsValid(LeFH18Gun) ? *LeFH18Gun->GetActorLocation().ToCompactString() : TEXT("none"),
	       IsValid(LeFH18Gun) ? FVector::Dist2D(LeFH18Gun->GetActorLocation(), M_LeFH18Target) : -1.f);
	Check(IsValid(LeFH18Gun) && FVector::Dist2D(LeFH18Gun->GetActorLocation(), M_LeFH18Target) > ExpectedLeFH18Range,
	      TEXT("lefh18_stayed_out_of_range"));
	Check(not bM_LeFH18Fired, TEXT("lefh18_cannot_fire_that_far"),
	      FString::Printf(TEXT("target %.0f cm, window %.0f s"), BeyondLeFHTargetDistance, FireWindowSeconds));
	ATeamWeaponController* LeFH18Controller = Cast<ATeamWeaponController>(LeFH18);
	if (not IsValid(LeFH18Controller))
	{
		StartCargoDoorDrop();
		return;
	}
	// Control: the same leFH 18 fires at a point inside its range, so the miss above was the range.
	M_LeFH18Target = OrderAttackGround(LeFH18, LeFH18Controller->GetTeamWeaponRange() * LeFHControlFraction);
	EnterPhase(ERound5FixesTestPhase::LeFHControlShot);
}

void URound5FixesTestSubsystem::TickLeFHControlShot()
{
	using namespace Round5FixesTestPrivate;
	if (GetHasFired(M_LeFH18Squad.Get()))
	{
		Check(true, TEXT("lefh18_fires_inside_its_range"), FString::Printf(TEXT("after=%.1fs"), GetPhaseSeconds()));
		StartCargoDoorDrop();
		return;
	}
	if (GetPhaseSeconds() >= ControlShotTimeoutSeconds)
	{
		Check(false, TEXT("lefh18_fires_inside_its_range"));
		StartCargoDoorDrop();
	}
}

// ---------------------------------------------------------------------------------------------------------------
// 3) Red Spine walkers leave the Ju 390 by its cargo door
// ---------------------------------------------------------------------------------------------------------------

void URound5FixesTestSubsystem::StartCargoDoorDrop()
{
	using namespace Round5FixesTestPrivate;
	EnterPhase(ERound5FixesTestPhase::RunningCargoDoorDrop);
	M_KnownSquads.Reset();
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		M_KnownSquads.Add(*It);
	}
	// To the side of the anchor, away from the guns behind it and the strike in front.
	const FVector Side(-M_TowardCentre.Y, M_TowardCentre.X, 0.f);
	M_DropAnchor = KeepInsideLandscape(M_Anchor + Side * 3000.f);
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = GetPlayerAbilityManager();
	M_Ability = MakeAbilityCopy(EGlobalAbility::GA_GerRedSpinePairDrop);
	UGA_AircraftDrop* Drop = Cast<UGA_AircraftDrop>(M_Ability);
	Check(IsValid(Drop) && IsValid(Manager), TEXT("red_spine_pair_drop_copy"));
	if (not IsValid(Drop) || not IsValid(Manager))
	{
		Finish();
		return;
	}
	TWeakObjectPtr<URound5FixesTestSubsystem> WeakThis(this);
	const TWeakObjectPtr<UGlobalAbility> WeakDrop(Drop);
	Drop->OnGlobalAbilityEnded.AddLambda([WeakThis, WeakDrop](UGlobalAbility* EndedAbility)
	{
		if (WeakThis.IsValid() && WeakDrop.Get() == EndedAbility)
		{
			WeakThis->bM_DropEnded = true;
		}
	});
	M_StartedAbilities.Add(Drop);
	Drop->InitGlobalAbility(TestPlayer, Manager, Controller);
	Drop->ExecuteAbilityAtLocation(M_DropAnchor);
	UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST DROP_EXECUTED anchor=%s"), *M_DropAnchor.ToCompactString());
	ViewFrom(M_DropAnchor, 3200.f, -50.f, M_TowardCentre.Rotation().Yaw + 120.f);
}

bool URound5FixesTestSubsystem::UpdateDoorLocation()
{
	using namespace Round5FixesTestPrivate;
	AAircraftMaster* Aircraft = M_DropAircraft.Get();
	const USkeletalMeshComponent* Mesh = IsValid(Aircraft) ? Aircraft->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (not IsValid(Mesh) || not Mesh->DoesSocketExist(CargoDoorSocket))
	{
		return false;
	}
	const FVector Socket = Mesh->GetSocketLocation(CargoDoorSocket);
	FVector Ground = Socket;
	GlobalAbilityHelpers::TryFindGroundLocation(this, Socket, Ground);
	M_DoorGroundLocation = Ground;
	bM_HasDoorLocation = true;
	return true;
}

void URound5FixesTestSubsystem::CollectNewSquads()
{
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		if (M_KnownSquads.Contains(*It) || M_DroppedSquads.Contains(*It))
		{
			continue;
		}
		const URTSComponent* SquadRTS = It->GetRTSComponent();
		if (not IsValid(SquadRTS) || SquadRTS->GetSubtypeAsSquadSubtype() != ESquadSubtype::Squad_Ger_RedSpine)
		{
			continue;
		}
		// The controller is spawned where the drop puts the squad; its walkers are placed around it right after.
		M_DroppedSquads.Add(*It);
		M_SpawnPoints.Add(It->GetActorLocation());
		M_FirstSeenCentres.Add(FVector::ZeroVector);
		M_FirstSeenSeconds.Add(GetPhaseSeconds());
		M_MostDistanceFromDoor.Add(0.f);
		UE_LOG(LogRTSRound5Test, Display,
		       TEXT("RTS_ROUND5_TEST SQUAD_APPEARED %s at=%.1fs controller=%s door=%s door_dist=%.0f"),
		       *It->GetName(), GetPhaseSeconds(), *It->GetActorLocation().ToCompactString(),
		       *M_DoorGroundLocation.ToCompactString(),
		       bM_HasDoorLocation ? FVector::Dist2D(It->GetActorLocation(), M_DoorGroundLocation) : -1.f);
	}
}

void URound5FixesTestSubsystem::SampleDroppedSquads()
{
	using namespace Round5FixesTestPrivate;
	if (GetNow() < M_NextSampleSeconds)
	{
		return;
	}
	M_NextSampleSeconds = GetNow() + SampleIntervalSeconds;
	for (int32 Index = 0; Index < M_DroppedSquads.Num(); ++Index)
	{
		ASquadController* Squad = M_DroppedSquads[Index].Get();
		if (not IsValid(Squad) || not Squad->GetIsSquadFullyLoadedAndInitialized())
		{
			continue;
		}
		if (M_FirstSeenCentres[Index].IsZero())
		{
			M_FirstSeenCentres[Index] = GetSquadCentre(Squad);
			UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST SQUAD_LOADED squad=%d t=%.1f centre=%s door_dist=%.0f"),
			       Index, GetPhaseSeconds(), *M_FirstSeenCentres[Index].ToCompactString(),
			       FVector::Dist2D(M_FirstSeenCentres[Index], M_DoorGroundLocation));
		}
		FString Units;
		for (ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
		{
			Units += Unit->GetActorLocation().ToCompactString() + TEXT(" ");
		}
		const FVector Centre = GetSquadCentre(Squad);
		const float FromDoor = bM_HasDoorLocation ? FVector::Dist2D(Centre, M_DoorGroundLocation) : -1.f;
		M_MostDistanceFromDoor[Index] = FMath::Max(M_MostDistanceFromDoor[Index], FromDoor);
		UE_LOG(LogRTSRound5Test, Display,
		       TEXT("RTS_ROUND5_TEST WALK t=%.1f squad=%d centre=%s from_door=%.0f idle=%d units=%s"),
		       GetPhaseSeconds(), Index, *Centre.ToCompactString(), FromDoor, Squad->GetIsUnitIdle(), *Units);
	}
}

void URound5FixesTestSubsystem::TickRunningCargoDoorDrop()
{
	using namespace Round5FixesTestPrivate;
	// The aircraft of this drop: the Ju 390 nearest the drop point.
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		if (not It->GetClass()->GetName().Contains(DropAircraftMarker))
		{
			continue;
		}
		const float Distance = FVector::Dist2D(It->GetActorLocation(), M_DropAnchor);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			if (Distance < MaxLandingDistance)
			{
				M_DropAircraft = *It;
			}
		}
	}
	if (M_DropAircraft.IsValid())
	{
		M_LastAircraftSeenSeconds = GetPhaseSeconds();
		// The door point is read while the squads come out; the aircraft stands still on the ground then.
		if (M_DroppedSquads.Num() < PairDropSquads)
		{
			UpdateDoorLocation();
		}
	}
	const int32 SquadsBefore = M_DroppedSquads.Num();
	CollectNewSquads();
	if (SquadsBefore == 0 && M_DroppedSquads.Num() > 0)
	{
		const FVector Focus = bM_HasDoorLocation ? M_DoorGroundLocation : M_DropAnchor;
		ViewFrom(Focus, 2600.f, -55.f, M_TowardCentre.Rotation().Yaw + 120.f);
		M_NextScreenshotSeconds = GetNow();
		const AAircraftMaster* Aircraft = M_DropAircraft.Get();
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST AIRCRAFT %s at=%s yaw=%.0f door=%s door_from_aircraft=%s"),
		       IsValid(Aircraft) ? *Aircraft->GetClass()->GetName() : TEXT("none"),
		       IsValid(Aircraft) ? *Aircraft->GetActorLocation().ToCompactString() : TEXT("-"),
		       IsValid(Aircraft) ? Aircraft->GetActorRotation().Yaw : 0.f, *M_DoorGroundLocation.ToCompactString(),
		       IsValid(Aircraft)
			       ? *Aircraft->GetActorTransform().InverseTransformPosition(M_DoorGroundLocation).ToCompactString()
			       : TEXT("-"));
	}
	SampleDroppedSquads();
	if (M_DroppedSquads.Num() > 0 && GetNow() >= M_NextScreenshotSeconds
		&& M_ScreenshotCount < MaxWalkOutScreenshots + 2)
	{
		M_NextScreenshotSeconds = GetNow() + ScreenshotIntervalSeconds;
		RequestScreenshot(FString::Printf(TEXT("WalkOut_t%04.1f"), GetPhaseSeconds()));
	}

	bool bAllOut = M_DroppedSquads.Num() >= PairDropSquads;
	for (const TWeakObjectPtr<ASquadController>& Squad : M_DroppedSquads)
	{
		bAllOut &= Squad.IsValid() && Squad->GetIsUnitIdle();
	}
	const float SinceFirstSquad = M_FirstSeenSeconds.Num() > 0 ? GetPhaseSeconds() - M_FirstSeenSeconds[0] : 0.f;
	const bool bWalkOutTimedOut = M_FirstSeenSeconds.Num() > 0 && SinceFirstSquad >= WalkOutTimeoutSeconds;
	// Let the walkers stand a moment after the last one arrived (and the aircraft lift off) before checking.
	if ((bAllOut && SinceFirstSquad >= 6.f) || bWalkOutTimedOut)
	{
		FinishCargoDoorChecks();
		return;
	}
	if (GetPhaseSeconds() >= DropTimeoutSeconds)
	{
		Check(false, TEXT("pair_drop_squads_arrived"), FString::Printf(TEXT("squads=%d"), M_DroppedSquads.Num()));
		Finish();
	}
}

void URound5FixesTestSubsystem::FinishCargoDoorChecks()
{
	using namespace Round5FixesTestPrivate;
	Check(bM_HasDoorLocation, TEXT("ju390_has_cargo_door_socket"), M_DoorGroundLocation.ToCompactString());
	Check(M_DroppedSquads.Num() == PairDropSquads, TEXT("pair_drop_lets_out_two_squads"),
	      FString::Printf(TEXT("%d"), M_DroppedSquads.Num()));
	const UGA_AircraftDrop* Drop = Cast<UGA_AircraftDrop>(M_Ability);
	const FAircraftDropGlobalAbilitySettings* Settings = GetDropSettings(Drop);
	const float Radius = Settings ? Settings->RadiusSpawnSquads : 0.f;
	M_ReleaseMoveStarts.Reset();
	M_ReleaseMoveTargets.Reset();
	for (int32 Index = 0; Index < M_DroppedSquads.Num(); ++Index)
	{
		ASquadController* Squad = M_DroppedSquads[Index].Get();
		const FString Label = FString::Printf(TEXT("squad%d"), Index);
		if (not IsValid(Squad))
		{
			Check(false, Label + TEXT("_alive"));
			continue;
		}
		const float SpawnFromDoor = FVector::Dist2D(M_SpawnPoints[Index], M_DoorGroundLocation);
		Check(bM_HasDoorLocation && SpawnFromDoor <= AtDoorRadius, Label + TEXT("_spawned_at_cargo_door"),
		      FString::Printf(TEXT("controller spawned %.0f cm from the door"), SpawnFromDoor));
		const float FirstFromDoor = FVector::Dist2D(M_FirstSeenCentres[Index], M_DoorGroundLocation);
		Check(not M_FirstSeenCentres[Index].IsZero() && FirstFromDoor <= AtDoorRadius * 1.5f,
		      Label + TEXT("_walkers_first_seen_at_cargo_door"),
		      FString::Printf(TEXT("first loaded centre %.0f cm from the door"), FirstFromDoor));
		const FVector Centre = GetSquadCentre(Squad);
		const float FromDropPoint = FVector::Dist2D(Centre, M_DropAnchor);
		const float FromDoor = FVector::Dist2D(Centre, M_DoorGroundLocation);
		Check(FMath::Abs(FromDropPoint - Radius) <= AtSpotRadius, Label + TEXT("_walked_out_to_its_spot"),
		      FString::Printf(TEXT("centre %.0f cm from the landing point (circle %.0f), %.0f from the door"),
		                      FromDropPoint, Radius, FromDoor));
		Check(FromDoor >= Radius * 0.5f, Label + TEXT("_left_the_door"),
		      FString::Printf(TEXT("%.0f cm from the door"), FromDoor));
		Check(Squad->GetIsUnitIdle(), Label + TEXT("_released_idle"));
		const URTSComponent* SquadRTS = Squad->GetRTSComponent();
		Check(IsValid(SquadRTS) && SquadRTS->GetOwningPlayer() == TestPlayer, Label + TEXT("_owned_by_player"));
		// Released: an ordinary order moves it further out.
		const FVector Outward = (Centre - M_DropAnchor).GetSafeNormal2D();
		const FVector Target = KeepInsideLandscape(Centre + Outward * ReleaseMoveDistance);
		const ECommandQueueError Error = Squad->MoveToLocation(Target, true, Outward.Rotation());
		Check(Error == ECommandQueueError::NoError, Label + TEXT("_accepts_move_order"));
		M_ReleaseMoveStarts.Add(Centre);
		M_ReleaseMoveTargets.Add(Target);
	}
	RequestScreenshot(TEXT("WalkOut_Done"));
	EnterPhase(ERound5FixesTestPhase::ReleasedMoveCheck);
}

void URound5FixesTestSubsystem::TickReleasedMoveCheck()
{
	using namespace Round5FixesTestPrivate;
	SampleDroppedSquads();
	bool bAllMoved = true;
	for (int32 Index = 0; Index < M_DroppedSquads.Num() && Index < M_ReleaseMoveStarts.Num(); ++Index)
	{
		ASquadController* Squad = M_DroppedSquads[Index].Get();
		bAllMoved &= IsValid(Squad)
			&& FVector::Dist2D(GetSquadCentre(Squad), M_ReleaseMoveStarts[Index]) >= ReleaseMinMoved;
	}
	if (not bAllMoved && GetPhaseSeconds() < ReleaseMoveTimeoutSeconds)
	{
		return;
	}
	for (int32 Index = 0; Index < M_DroppedSquads.Num() && Index < M_ReleaseMoveStarts.Num(); ++Index)
	{
		ASquadController* Squad = M_DroppedSquads[Index].Get();
		const float Moved = IsValid(Squad) ? FVector::Dist2D(GetSquadCentre(Squad), M_ReleaseMoveStarts[Index]) : 0.f;
		Check(Moved >= ReleaseMinMoved, FString::Printf(TEXT("squad%d_follows_new_order"), Index),
		      FString::Printf(TEXT("moved %.0f cm"), Moved));
	}
	ViewFrom(M_DropAnchor, 3600.f, -55.f, M_TowardCentre.Rotation().Yaw + 120.f);
	RequestScreenshot(TEXT("Released_Moved"));
	Finish();
}

// ---------------------------------------------------------------------------------------------------------------
// Mission caps (mission maps only)
// ---------------------------------------------------------------------------------------------------------------

void URound5FixesTestSubsystem::TickMissionCaps()
{
	using namespace Round5FixesTestPrivate;
	FString ExpectedText;
	FParse::Value(FCommandLine::Get(), TEXT("Round5MissionCaps="), ExpectedText);
	bool bValidExpected = false;
	const EGlobalAbilityTier Expected = ParseTier(ExpectedText, bValidExpected);
	const UWorld* World = GetWorld();
	const FString MapName = IsValid(World) ? World->GetMapName() : TEXT("none");
	const AMissionManager* MissionManager = FRTS_Statics::GetGameMissionManager(this);
	UGlobalAbilitiesManager* Manager = GetPlayerAbilityManager();
	const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	const UCommanderSettings* CommanderSettings = UCommanderSettings::Get();
	// The controller sets the cap right before it loads the kit: wait until the kit's first ability is loaded.
	bool bKitLoaded = false;
	TArray<EGlobalAbility> Kit;
	if (IsValid(GameInstance) && IsValid(CommanderSettings))
	{
		Kit = CommanderSettings->GetCommanderSettingsForType(GameInstance->GetPlayerCommander()).GlobalAbilities;
	}
	if (IsValid(Manager) && Kit.Num() > 0)
	{
		bKitLoaded = IsValid(Manager->FindLoadedAbilityByType(Kit[0]));
	}
	if (not bKitLoaded && GetPhaseSeconds() < MissionCapsTimeoutSeconds)
	{
		return;
	}
	Check(bValidExpected, TEXT("expected_tier_parsed"), ExpectedText);
	Check(IsValid(MissionManager), TEXT("mission_manager_found"), MapName);
	if (IsValid(MissionManager))
	{
		const EGlobalAbilityTier Cap = MissionManager->GetMaxPlayerGlobalAbilityTier();
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST MISSION_CAP map=%s manager=%s cap=%s expected=%s"),
		       *MapName, *MissionManager->GetName(), *UEnum::GetValueAsString(Cap),
		       *UEnum::GetValueAsString(Expected));
		Check(Cap == Expected, TEXT("mission_manager_cap ") + MapName, UEnum::GetValueAsString(Cap));
	}
	Check(bKitLoaded, TEXT("player_kit_loaded"), FString::Printf(TEXT("kit=%d"), Kit.Num()));
	if (not IsValid(Manager))
	{
		Finish();
		return;
	}
	// -Round5MissionFree=1/0: whether the mission makes the player's abilities free (checked only when passed).
	int32 ExpectedFree = -1;
	FParse::Value(FCommandLine::Get(), TEXT("Round5MissionFree="), ExpectedFree);
	if (ExpectedFree >= 0 && IsValid(MissionManager))
	{
		const bool bMissionFree = MissionManager->GetPlayerGlobalAbilitySettings().bDisableResourceCosts;
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST MISSION_CAP free=%d manager_free=%d expected=%d"),
		       bMissionFree, Manager->GetAreResourceCostsDisabled(), ExpectedFree);
		Check(bMissionFree == (ExpectedFree == 1), TEXT("mission_manager_free_costs ") + MapName);
		Check(Manager->GetAreResourceCostsDisabled() == (ExpectedFree == 1), TEXT("player_manager_free_costs"));
	}
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	const UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                               ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                               : nullptr;
	Check(IsValid(AbilitySet), TEXT("ability_set_loaded"));
	// The cap hides abilities: every kit ability up to the cap is loaded, none above it.
	int32 ExpectedLoadedCount = 0;
	for (const EGlobalAbility AbilityType : Kit)
	{
		const UGlobalAbility* Template = IsValid(AbilitySet) ? AbilitySet->FindAbilityTemplate(AbilityType) : nullptr;
		if (not IsValid(Template))
		{
			continue;
		}
		const bool bShouldBeLoaded = static_cast<uint8>(Template->GetAbilityTier()) <= static_cast<uint8>(Expected);
		const UGlobalAbility* Loaded = Manager->FindLoadedAbilityByType(AbilityType);
		ExpectedLoadedCount += bShouldBeLoaded ? 1 : 0;
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST MISSION_CAP kit %s tier=%s loaded=%d"),
		       *UEnum::GetValueAsString(AbilityType), *UEnum::GetValueAsString(Template->GetAbilityTier()),
		       IsValid(Loaded));
		Check(IsValid(Loaded) == bShouldBeLoaded, TEXT("kit_ability_tier_hidden ") + UEnum::GetValueAsString(AbilityType),
		      FString::Printf(TEXT("tier=%s loaded=%d"), *UEnum::GetValueAsString(Template->GetAbilityTier()),
		                      IsValid(Loaded)));
		if (IsValid(Loaded) && ExpectedFree == 1)
		{
			Check(Manager->QueryCostsForAbility(Loaded) && Loaded->GetAbilityCosts().Costs.ResourceCosts.Num() == 0,
			      TEXT("kit_ability_free ") + UEnum::GetValueAsString(AbilityType));
		}
	}
	// The side panel shows exactly the loaded abilities; the hidden ones leave no greyed-out slot.
	const ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UMainGameUI* MainGameUI = IsValid(Controller) ? Controller->GetMainMenuUI() : nullptr;
	UW_GlobalAbilityPanel* Panel = IsValid(MainGameUI) ? MainGameUI->GetGlobalAbilityPanel().Get() : nullptr;
	Check(IsValid(Panel), TEXT("ability_panel_found"));
	if (IsValid(Panel))
	{
		int32 VisibleItems = 0;
		for (const UW_GA_Item* Item : Panel->GetAbilityItems())
		{
			if (not IsValid(Item) || Item->GetVisibility() == ESlateVisibility::Collapsed
				|| Item->GetVisibility() == ESlateVisibility::Hidden)
			{
				continue;
			}
			++VisibleItems;
			const UGlobalAbility* Shown = Item->GetLoadedAbility();
			Check(IsValid(Shown) && Manager->QueryTierAllowedForAbility(Shown), TEXT("panel_item_within_cap"),
			      IsValid(Shown) ? UEnum::GetValueAsString(Shown->GetAbilityTier()) : TEXT("none"));
		}
		UE_LOG(LogRTSRound5Test, Display, TEXT("RTS_ROUND5_TEST MISSION_CAP panel visible=%d expected=%d"),
		       VisibleItems, ExpectedLoadedCount);
		Check(VisibleItems == ExpectedLoadedCount, TEXT("panel_visible_items"),
		      FString::Printf(TEXT("visible=%d expected=%d"), VisibleItems, ExpectedLoadedCount));
	}
	Finish();
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------

FBox URound5FixesTestSubsystem::FindLandscapeBounds() const
{
	FBox Bounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		Bounds += It->GetComponentsBoundingBox(true);
	}
	return Bounds;
}

FVector URound5FixesTestSubsystem::KeepInsideLandscape(const FVector& Location) const
{
	using namespace Round5FixesTestPrivate;
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

FVector URound5FixesTestSubsystem::GetSquadCentre(ASquadController* Squad)
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

void URound5FixesTestSubsystem::ViewFrom(const FVector& Focus, const float Distance, const float Pitch,
                                         const float Yaw) const
{
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(Pitch, Yaw, 0.f);
	M_Camera->SetActorLocationAndRotation(Focus - ViewRotation.Vector() * Distance, ViewRotation);
	Controller->SetViewTarget(M_Camera);
}

void URound5FixesTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("Round5FixesTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool URound5FixesTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void URound5FixesTestSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void URound5FixesTestSubsystem::Tick(float DeltaTime) {}
TStatId URound5FixesTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URound5FixesTestSubsystem, STATGROUP_Tickables);
}

#endif
