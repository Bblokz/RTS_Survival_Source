// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldWorkshopTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Landscape.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/BuildingDrop/GA_BuildingParachuteDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/CompositeDrop/GA_CompositeDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/FieldWorkshop/FieldWorkshopComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/FieldWorkshop/FieldWorkshopRearmAura.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/FieldWorkshop/FieldWorkshopRepairBehaviour.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/CargoParachuteDescentComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFieldWorkshopTest, Log, All);

namespace FieldWorkshopTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr int32 EnemyPlayer = 2;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float WorkshopTimeoutSeconds = 150.f;
	constexpr float GunSettleSeconds = 10.f;
	constexpr float GunTimeoutSeconds = 150.f;
	constexpr float TankPrepareDelaySeconds = 2.f;
	// Long enough for the aura's 1 s sweeps to reach squads whose soldiers spawn a moment after the controller.
	constexpr float MeasureSeconds = 12.f;
	constexpr float DamagedHealthFraction = 0.4f;
	constexpr float InsideRadiusFraction = 0.55f;
	constexpr float OutsideRadiusMarginCm = 1500.f;
	// At least 60% of the workshop's 25 HP/s over the measured time (the first aura tick may come a second late).
	constexpr float ExpectedRepairPerSecond = 25.f;
	constexpr float MinRepairShare = 0.6f;
	constexpr float MaxUnrepairedGainHp = 1.f;
	constexpr float PackUpTimingToleranceSeconds = 3.f;
	constexpr float RemovalExtraSeconds = 4.f;
	constexpr float AfterPackUpCheckSeconds = 3.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float CameraPitch = -55.f;
	constexpr float CameraYaw = 35.f;
	constexpr float WorkshopCameraDistance = 5200.f;
	constexpr float DropCameraDistance = 8000.f;
	constexpr float MinGunUpZ = 0.9f;
	constexpr float MinBuildingUpZ = 0.995f;
	constexpr float MaxGroundGapCm = 40.f;
	constexpr float MaxOperatorFromPostCm = 150.f;
	// Guns land beside the workshop, not on it: the HQ repair bay pad is about 7.5 x 6 m.
	constexpr float MinGunFromWorkshopCm = 1200.f;
	constexpr int32 ExpectedGunSquads = 2;
	constexpr float TankSpawnAboveGroundCm = 120.f;
	constexpr float SpendShotDistanceCm = 6000.f;
	constexpr float SpendShotHeightCm = 150.f;
	// Scenario B: its own ground, away from scenario A's wreck-free area (map edge lies to -X, +Y).
	// The drop scenario's ground is empty again once its workshop packed up and its tanks are gone.
	const FVector2D KillScenarioOffset(0.f, 0.f);
	// The site must have landscape this far round the anchor (radius, tanks outside it, the guns) ...
	constexpr float SiteGroundRadiusCm = 4500.f;
	constexpr int32 SiteGroundProbeCount = 16;
	// ... and no enemy unit this close (a gun that sees one fights it and can be killed before the checks).
	constexpr float SiteEnemyFreeRadiusCm = 9000.f;
	constexpr float SiteSearchStepCm = 2500.f;
	constexpr int32 SiteSearchMaxSteps = 24;
	constexpr float KillRepairSeconds = 5.f;
	constexpr float KillResidualSeconds = 2.f;
	constexpr float KillPlateauSeconds = 5.f;
	constexpr float KillTimeoutSeconds = 30.f;
	const TCHAR* const WorkshopClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/FieldWorkshop/BXP_FieldWorkshop.BXP_FieldWorkshop_C");
	const TCHAR* const FriendlyTankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	const TCHAR* const EnemyTankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	const TCHAR* const InfantrySquadClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers.Squad_Ger_Scavengers_C");
	const TCHAR* const Pak40SquadClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Pak40_75MM/Ger_SquadTW_Pak40_75MM.Ger_SquadTW_Pak40_75MM_C");
	// BP_Beh_Weapon_AmmoResupply multiplies the reload time by 0.7.
	constexpr float BuffedReloadFactor = 0.7f;
	constexpr float ReloadFactorTolerance = 0.03f;
	const TCHAR* const AmmoResupplyBehaviourPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Behaviours/Weapon/BP_Beh_Weapon_AmmoResupply.BP_Beh_Weapon_AmmoResupply_C");
}

bool UFieldWorkshopTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("FieldWorkshopValidate"));
}

TStatId UFieldWorkshopTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFieldWorkshopTestSubsystem, STATGROUP_Tickables);
}

void UFieldWorkshopTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	if (M_Step != EFieldWorkshopTestStep::WaitingForStart && M_Step != EFieldWorkshopTestStep::Finished)
	{
		UpdateCamera();
	}
	switch (M_Step)
	{
	case EFieldWorkshopTestStep::WaitingForStart:
		TickWaitingForStart();
		return;
	case EFieldWorkshopTestStep::WaitingForWorkshop:
		TickWaitingForWorkshop();
		return;
	case EFieldWorkshopTestStep::WaitingForGuns:
		TickWaitingForGuns();
		return;
	case EFieldWorkshopTestStep::WaitingForTanks:
		TickWaitingForTanks();
		return;
	case EFieldWorkshopTestStep::Measuring:
		TickMeasuring();
		return;
	case EFieldWorkshopTestStep::WaitingForPackUp:
		TickWaitingForPackUp();
		return;
	case EFieldWorkshopTestStep::WaitingForRemoval:
		TickWaitingForRemoval();
		return;
	case EFieldWorkshopTestStep::KillWaitingForRepair:
		TickKillWaitingForRepair();
		return;
	case EFieldWorkshopTestStep::KillAfterDeath:
		TickKillAfterDeath();
		return;
	case EFieldWorkshopTestStep::Finished:
		return;
	}
}

void UFieldWorkshopTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST CHECK PASS %s.%s %s"),
		       *M_ScenarioLabel, *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSFieldWorkshopTest, Error, TEXT("RTS_FIELD_WORKSHOP_TEST CHECK FAIL %s.%s %s"), *M_ScenarioLabel,
	       *Description, *Details);
}

float UFieldWorkshopTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

float UFieldWorkshopTestSubsystem::GetStepSeconds() const
{
	return GetNow() - M_StepStartSeconds;
}

void UFieldWorkshopTestSubsystem::SetStep(const EFieldWorkshopTestStep NewStep)
{
	M_Step = NewStep;
	M_StepStartSeconds = GetNow();
	UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST STEP %s at=%.1f"),
	       *UEnum::GetValueAsString(NewStep), GetNow());
}

void UFieldWorkshopTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Step = EFieldWorkshopTestStep::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

// ------------------------------------------------------------------ start

void UFieldWorkshopTestSubsystem::TickWaitingForStart()
{
	using namespace FieldWorkshopTestPrivate;
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
	if (not FindAnchor(M_Anchor) || not FindClearSite(M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"));
		Finish();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST ANCHOR %s"), *M_Anchor.ToCompactString());
	if (FParse::Param(FCommandLine::Get(), TEXT("FieldWorkshopKillOnly")))
	{
		StartKillScenario();
		return;
	}
	StartDropScenario();
}

bool UFieldWorkshopTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace FieldWorkshopTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("FieldWorkshopAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("FieldWorkshopAnchorY="), AnchorY))
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
	if (not World->LineTraceSingleByChannel(GroundHit, CameraLocation,
	                                        CameraLocation + CameraForward * CameraTraceLength, ECC_Visibility))
	{
		return false;
	}
	return GlobalAbilityHelpers::TryFindGroundLocation(this, GroundHit.Location, OutAnchor);
}

bool UFieldWorkshopTestSubsystem::FindClearSite(FVector& InOutAnchor) const
{
	using namespace FieldWorkshopTestPrivate;
	FBox LandscapeBounds(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		LandscapeBounds += It->GetComponentsBoundingBox(true);
	}
	const FVector LandscapeCentre = LandscapeBounds.IsValid ? LandscapeBounds.GetCenter() : FVector::ZeroVector;
	UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST LANDSCAPE bounds=%s"),
	       *LandscapeBounds.ToString());
	FVector Candidate = InOutAnchor;
	for (int32 Step = 0; Step <= SiteSearchMaxSteps; ++Step)
	{
		FString Reason;
		if (GetIsSiteClear(Candidate, Reason))
		{
			UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST SITE %s after %d steps"),
			       *Candidate.ToCompactString(), Step);
			InOutAnchor = Candidate;
			return true;
		}
		UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST SITE_REJECTED %s: %s"),
		       *Candidate.ToCompactString(), *Reason);
		const FVector ToCentre = (LandscapeCentre - Candidate).GetSafeNormal2D();
		if (ToCentre.IsNearlyZero())
		{
			return false;
		}
		GlobalAbilityHelpers::TryFindGroundLocation(this, Candidate + ToCentre * SiteSearchStepCm, Candidate);
	}
	return false;
}

bool UFieldWorkshopTestSubsystem::GetIsSiteClear(const FVector& Candidate, FString& OutReason) const
{
	using namespace FieldWorkshopTestPrivate;
	FVector Ground;
	if (not GlobalAbilityHelpers::TryFindGroundLocation(this, Candidate, Ground))
	{
		OutReason = TEXT("no landscape at the centre");
		return false;
	}
	for (int32 Probe = 0; Probe < SiteGroundProbeCount; ++Probe)
	{
		const FVector Offset = FRotator(0.f, 360.f * Probe / SiteGroundProbeCount, 0.f).Vector() * SiteGroundRadiusCm;
		if (not GlobalAbilityHelpers::TryFindGroundLocation(this, Candidate + Offset, Ground))
		{
			OutReason = FString::Printf(TEXT("no landscape at %s"), *(Candidate + Offset).ToCompactString());
			return false;
		}
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		if (not IsValid(RTSComponent) || RTSComponent->GetOwningPlayer() != EnemyPlayer)
		{
			continue;
		}
		if (FVector::Dist2D(It->GetActorLocation(), Candidate) < SiteEnemyFreeRadiusCm)
		{
			OutReason = FString::Printf(TEXT("enemy %s at %s"), *It->GetName(),
			                            *It->GetActorLocation().ToCompactString());
			return false;
		}
	}
	return true;
}

void UFieldWorkshopTestSubsystem::StartDropScenario()
{
	using namespace FieldWorkshopTestPrivate;
	M_ScenarioLabel = TEXT("Drop");
	M_CameraTarget = M_Anchor;
	M_CameraDistance = DropCameraDistance;
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
	UGlobalAbility* Copy = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(EGlobalAbility::GA_GerFieldWorkshopDrop);
	M_Ability = Cast<UGA_CompositeDrop>(Copy);
	Check(IsValid(M_Ability), TEXT("template_is_composite_drop"),
	      IsValid(Copy) ? Copy->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(M_Ability) || not CheckTemplate(*M_Ability))
	{
		Finish();
		return;
	}
	M_Ability->InitGlobalAbility(TestPlayer, Manager, Controller);
	TMap<uint8, int32> ResourcesBefore;
	if (not EnsureCanAfford(*M_Ability, ResourcesBefore))
	{
		Finish();
		return;
	}
	SnapshotExistingActors();
	M_Ability->OnGlobalAbilityEnded.AddUObject(this, &UFieldWorkshopTestSubsystem::OnParentEnded);
	SetStep(EFieldWorkshopTestStep::WaitingForWorkshop);
	// The player's path: activate from the panel, then click the target.
	Controller->OnGlobaAbilityActivated(M_Ability->GetAimSettings(), FGlobalAbilitySoundSettings(), M_Ability);
	Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(M_Anchor), TEXT("click_handled"));
	CheckPayment(*M_Ability, ResourcesBefore);
	RequestScreenshot(TEXT("Drop_Clicked"));
}

bool UFieldWorkshopTestSubsystem::CheckTemplate(const UGA_CompositeDrop& Ability)
{
	const TArray<FCompositeDropChild>& Entries = Ability.GetChildEntries();
	Check(Entries.Num() == 2, TEXT("two_children"), FString::Printf(TEXT("children=%d"), Entries.Num()));
	if (Entries.Num() != 2)
	{
		return false;
	}
	Check(IsValid(Cast<UGA_BuildingParachuteDrop>(Entries[0].Ability)), TEXT("child_0_is_building_drop"),
	      IsValid(Entries[0].Ability) ? Entries[0].Ability->GetClass()->GetName() : TEXT("none"));
	Check(IsValid(Cast<UGA_Paratroopers>(Entries[1].Ability)), TEXT("child_1_is_paradrop"),
	      IsValid(Entries[1].Ability) ? Entries[1].Ability->GetClass()->GetName() : TEXT("none"));
	Check(Entries[1].TargetOffset.Size() >= 2000.f, TEXT("guns_aimed_beside_workshop"),
	      Entries[1].TargetOffset.ToString());
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const UGlobalAbility* Child = Entries[Index].Ability;
		Check(IsValid(Child) && Child->GetOuter() == &Ability, FString::Printf(TEXT("child_%d_owned_by_copy"), Index));
	}
	const FGLobalAbilityCostState& Costs = Ability.GetAbilityCosts();
	const bool bRadixiteOnly = Costs.Costs.ResourceCosts.Num() == 1
		&& Costs.Costs.ResourceCosts.Contains(ERTSResourceType::Resource_Radixite);
	Check(bRadixiteOnly && Costs.CoolDownTime > 0, TEXT("radixite_only_cost_and_cooldown"),
	      FString::Printf(TEXT("radixite=%d resources=%d cooldown=%d"),
	                      Costs.Costs.ResourceCosts.FindRef(ERTSResourceType::Resource_Radixite),
	                      Costs.Costs.ResourceCosts.Num(), Costs.CoolDownTime));
	Check(Ability.GetAbilityTier() == EGlobalAbilityTier::TierIII, TEXT("tier_three"),
	      UEnum::GetValueAsString(Ability.GetAbilityTier()));
	return IsValid(Entries[0].Ability) && IsValid(Entries[1].Ability);
}

bool UFieldWorkshopTestSubsystem::EnsureCanAfford(const UGA_CompositeDrop& Ability, TMap<uint8, int32>& OutBefore)
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	Check(IsValid(Resources), TEXT("player_resource_manager"));
	if (not IsValid(Resources))
	{
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
		const int32 Amount = Resources->GetResourceAmount(Cost.Key);
		OutBefore.Add(static_cast<uint8>(Cost.Key), Amount);
		bCanAfford &= Amount >= Cost.Value;
	}
	Check(bCanAfford, TEXT("player_can_afford"));
	return bCanAfford;
}

void UFieldWorkshopTestSubsystem::CheckPayment(const UGA_CompositeDrop& Ability, const TMap<uint8, int32>& Before)
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	const UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	if (not IsValid(Resources))
	{
		return;
	}
	bool bPaidOnce = true;
	FString Details;
	for (const TPair<ERTSResourceType, int32>& Cost : Ability.GetAbilityCosts().Costs.ResourceCosts)
	{
		const int32 Paid = Before.FindRef(static_cast<uint8>(Cost.Key)) - Resources->GetResourceAmount(Cost.Key);
		bPaidOnce &= Paid == Cost.Value;
		Details += FString::Printf(TEXT("%s paid=%d cost=%d "), *UEnum::GetValueAsString(Cost.Key), Paid, Cost.Value);
	}
	Check(bPaidOnce, TEXT("cost_paid_once"), Details);
	const FGLobalAbilityCostState& Costs = Ability.GetAbilityCosts();
	Check(Costs.CoolDownRemaining == Costs.CoolDownTime, TEXT("cooldown_started"),
	      FString::Printf(TEXT("remaining=%d time=%d"), Costs.CoolDownRemaining, Costs.CoolDownTime));
}

void UFieldWorkshopTestSubsystem::OnParentEnded(UGlobalAbility* Parent)
{
	M_ParentEndSeconds = GetNow();
	UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST PARENT_ENDED at=%.1f"), GetNow());
}

// ------------------------------------------------------- scenario A: drop

void UFieldWorkshopTestSubsystem::TickWaitingForWorkshop()
{
	using namespace FieldWorkshopTestPrivate;
	if (not M_Workshop.IsValid())
	{
		TryFindNewWorkshop();
	}
	if (M_WorkshopComponent.IsValid() && M_WorkshopComponent->GetWorkshopState() == EFieldWorkshopState::Working)
	{
		M_WorkStartSeconds = M_WorkshopComponent->GetWorkStartSeconds();
		CheckLandedWorkshop();
		M_CameraTarget = M_Workshop->GetActorLocation();
		M_CameraDistance = WorkshopCameraDistance;
		RequestScreenshot(TEXT("Drop_WorkshopLanded"));
		SetStep(EFieldWorkshopTestStep::WaitingForGuns);
		return;
	}
	if (GetStepSeconds() > WorkshopTimeoutSeconds)
	{
		Check(false, TEXT("workshop_landed_and_working"),
		      FString::Printf(TEXT("found=%d"), M_Workshop.IsValid()));
		Finish();
	}
}

bool UFieldWorkshopTestSubsystem::TryFindNewWorkshop()
{
	for (AActor* Actor : GetNewActorsOfClass(ABuildingExpansion::StaticClass()))
	{
		UFieldWorkshopComponent* Component = Actor->FindComponentByClass<UFieldWorkshopComponent>();
		if (not IsValid(Component))
		{
			continue;
		}
		M_Workshop = Cast<ABuildingExpansion>(Actor);
		M_WorkshopComponent = Component;
		UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST WORKSHOP %s state=%s"),
		       *Actor->GetName(), *UEnum::GetValueAsString(Component->GetWorkshopState()));
		return true;
	}
	return false;
}

void UFieldWorkshopTestSubsystem::CheckLandedWorkshop()
{
	using namespace FieldWorkshopTestPrivate;
	ABuildingExpansion* Workshop = M_Workshop.Get();
	const FString Name = Workshop->GetClass()->GetName();
	Check(Name.Contains(TEXT("BXP_FieldWorkshop")), TEXT("workshop_class"), Name);
	Check(not Workshop->IsHidden() && Workshop->GetActorEnableCollision(), TEXT("workshop_visible_with_collision"));
	Check(Workshop->FindComponentByClass<UCargoParachuteDescentComponent>() == nullptr, TEXT("workshop_chute_removed"));
	const float UpZ = Workshop->GetActorUpVector().Z;
	Check(UpZ >= MinBuildingUpZ, TEXT("workshop_upright"), FString::Printf(TEXT("up_z=%.4f"), UpZ));
	FVector Ground;
	const bool bHasGround = GlobalAbilityHelpers::TryFindGroundLocation(this, Workshop->GetActorLocation(), Ground);
	const float Gap = bHasGround ? Workshop->GetActorLocation().Z - Ground.Z : 99999.f;
	Check(FMath::Abs(Gap) <= MaxGroundGapCm, TEXT("workshop_on_ground"),
	      FString::Printf(TEXT("gap=%.1f at=%s"), Gap, *Workshop->GetActorLocation().ToCompactString()));
	const URTSComponent* RTSComponent = Workshop->GetRTSComponent();
	const int32 Owner = IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1;
	Check(Owner == TestPlayer, TEXT("workshop_owned_by_player"), FString::Printf(TEXT("owner=%d"), Owner));
	Check(Workshop->GetStatus() == EBuildingExpansionStatus::BXS_Built, TEXT("workshop_built"),
	      UEnum::GetValueAsString(Workshop->GetStatus()));
	Check(Workshop->IsUnitAlive(), TEXT("workshop_alive"));
	const USelectionComponent* Selection = Workshop->GetSelectionComponent();
	Check(IsValid(Selection) && Selection->GetCanBeSelected(), TEXT("workshop_selectable"));
	const UHealthComponent* Health = Workshop->GetHealthComponent();
	Check(IsValid(Health) && Health->GetMaxHealth() > 0.f, TEXT("workshop_has_health"),
	      FString::Printf(TEXT("max_hp=%.0f"), IsValid(Health) ? Health->GetMaxHealth() : -1.f));
	Check(M_WorkshopComponent->GetWorkshopRadius() > 0.f && M_WorkshopComponent->GetIsShowingWorkingRadius(),
	      TEXT("working_radius_shown"), FString::Printf(TEXT("radius=%.0f"), M_WorkshopComponent->GetWorkshopRadius()));
	const float Distance = FVector::Dist2D(Workshop->GetActorLocation(), M_Anchor);
	UE_LOG(LogRTSFieldWorkshopTest, Display,
	       TEXT("RTS_FIELD_WORKSHOP_TEST LANDED at=%s from_target=%.0f work_start=%.1f working_s=%.0f"),
	       *Workshop->GetActorLocation().ToCompactString(), Distance, M_WorkStartSeconds,
	       M_WorkshopComponent->GetWorkshopSettings().WorkingSeconds);
}

void UFieldWorkshopTestSubsystem::TickWaitingForGuns()
{
	using namespace FieldWorkshopTestPrivate;
	RestrictGunFire();
	const bool bSettled = M_ParentEndSeconds >= 0.f && GetNow() - M_ParentEndSeconds >= GunSettleSeconds;
	if (not bSettled)
	{
		if (GetStepSeconds() > GunTimeoutSeconds)
		{
			Check(false, TEXT("drop_finished"), TEXT("the composite never ended"));
			Finish();
		}
		return;
	}
	CheckGunSquads();
	RestrictGunFire();
	RequestScreenshot(TEXT("Drop_GunsDeployed"));
	SpawnTestTanks();
	SpawnTestSquads();
	SetStep(EFieldWorkshopTestStep::WaitingForTanks);
}

void UFieldWorkshopTestSubsystem::CheckGunSquads()
{
	using namespace FieldWorkshopTestPrivate;
	const TArray<AActor*> GunSquads = GetNewActorsOfClass(ATeamWeaponController::StaticClass());
	for (TActorIterator<ATeamWeaponController> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST GUN_SQUAD %s at=%s pre_existing=%d alive=%d"),
		       *It->GetName(), *It->GetActorLocation().ToCompactString(), M_PreExistingActors.Contains(*It),
		       It->IsUnitAlive());
	}
	Check(GunSquads.Num() == ExpectedGunSquads, TEXT("gun_squads_dropped"),
	      FString::Printf(TEXT("squads=%d"), GunSquads.Num()));
	for (AActor* Actor : GunSquads)
	{
		ATeamWeaponController* Squad = Cast<ATeamWeaponController>(Actor);
		const FString SquadName = Squad->GetClass()->GetName();
		Check(SquadName.Contains(TEXT("PaK40")) || SquadName.Contains(TEXT("Pak40")), TEXT("gun_is_pak40"), SquadName);
		Check(not Squad->GetIsInParachuteDrop(), TEXT("gun_squad_released"), SquadName);
		const URTSComponent* RTSComponent = Squad->GetRTSComponent();
		Check(IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == TestPlayer, TEXT("gun_owned_by_player"),
		      SquadName);
		ATeamWeapon* Gun = Squad->GetControlledTeamWeaponNoReport();
		Check(IsValid(Gun), TEXT("gun_exists"), SquadName);
		if (not IsValid(Gun))
		{
			continue;
		}
		const float UpZ = Gun->GetActorUpVector().Z;
		Check(UpZ >= MinGunUpZ, TEXT("gun_upright"), FString::Printf(TEXT("up_z=%.3f"), UpZ));
		float GroundZ = 0.f;
		const bool bHasGround = Gun->TryGetGroundHeightAt(Gun->GetActorLocation(), GroundZ);
		const float Gap = bHasGround ? Gun->GetActorLocation().Z - GroundZ : 99999.f;
		Check(bHasGround && FMath::Abs(Gap) <= MaxGroundGapCm, TEXT("gun_on_ground"),
		      FString::Printf(TEXT("gap=%.1f"), Gap));
		Check(Squad->GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed, TEXT("gun_deployed"),
		      Squad->GetOrderDebugDescription());
		TArray<ASquadUnit*> Operators;
		Squad->GetCrewOperators(Operators);
		Check(Operators.Num() >= Gun->GetRequiredOperators() && Gun->GetRequiredOperators() > 0, TEXT("gun_crewed"),
		      FString::Printf(TEXT("operators=%d required=%d"), Operators.Num(), Gun->GetRequiredOperators()));
		int32 OperatorsAtPost = 0;
		int32 OperatorsCrewing = 0;
		for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
		{
			const ASquadUnit* Operator = Operators[OperatorIndex];
			FVector PostLocation;
			OperatorsAtPost += Squad->TryGetOperatorCrewPositionLocation(OperatorIndex, PostLocation)
				&& FVector::Dist2D(Operator->GetActorLocation(), PostLocation) <= MaxOperatorFromPostCm ? 1 : 0;
			const USquadUnitAnimInstance* AnimInstance = Operator->GetAnimBP_SquadUnit();
			OperatorsCrewing += IsValid(AnimInstance) && AnimInstance->GetIsTeamWeaponCrewAnimationActive() ? 1 : 0;
		}
		Check(OperatorsAtPost == Operators.Num(), TEXT("operators_at_posts"),
		      FString::Printf(TEXT("%d of %d"), OperatorsAtPost, Operators.Num()));
		Check(OperatorsCrewing == Operators.Num(), TEXT("operators_crewing_gun"),
		      FString::Printf(TEXT("%d of %d"), OperatorsCrewing, Operators.Num()));
		const float FromWorkshop = M_Workshop.IsValid()
			                           ? FVector::Dist2D(Gun->GetActorLocation(), M_Workshop->GetActorLocation())
			                           : -1.f;
		Check(FromWorkshop >= MinGunFromWorkshopCm, TEXT("gun_beside_workshop_not_on_it"),
		      FString::Printf(TEXT("distance=%.0f"), FromWorkshop));
	}
}

void UFieldWorkshopTestSubsystem::RestrictGunFire()
{
	// The guns would kill the enemy test tank at once; the test only needs them deployed.
	for (AActor* Actor : GetNewActorsOfClass(ATeamWeaponController::StaticClass()))
	{
		const ATeamWeaponController* Squad = Cast<ATeamWeaponController>(Actor);
		ATeamWeapon* Gun = IsValid(Squad) ? Squad->GetControlledTeamWeaponNoReport() : nullptr;
		if (not IsValid(Gun))
		{
			continue;
		}
		for (UWeaponState* Weapon : Gun->GetWeapons())
		{
			if (IsValid(Weapon))
			{
				Weapon->SetModuleFireRestriction(this, true);
			}
		}
	}
}

void UFieldWorkshopTestSubsystem::SpawnTestTanks()
{
	// Angles keep the tanks off the gun side (+Y) and apart from each other.
	AddTestTank(TEXT("DamagedInside"), FieldWorkshopTestPrivate::TestPlayer, true, true, false, 200.f);
	AddTestTank(TEXT("SpentInside"), FieldWorkshopTestPrivate::TestPlayer, true, false, true, 265.f);
	AddTestTank(TEXT("FriendlyOutside"), FieldWorkshopTestPrivate::TestPlayer, false, true, true, 160.f);
	AddTestTank(TEXT("EnemyInside"), FieldWorkshopTestPrivate::EnemyPlayer, true, true, true, 325.f);
}

void UFieldWorkshopTestSubsystem::AddTestTank(const FString& Label, const int32 Owner, const bool bInside,
                                              const bool bDamage, const bool bSpend, const float AngleDegrees)
{
	using namespace FieldWorkshopTestPrivate;
	if (not M_Workshop.IsValid() || not M_WorkshopComponent.IsValid())
	{
		return;
	}
	const float Radius = M_WorkshopComponent->GetWorkshopRadius();
	const float Distance = bInside ? Radius * InsideRadiusFraction : Radius + OutsideRadiusMarginCm;
	const FVector Direction = FRotator(0.f, AngleDegrees, 0.f).Vector();
	FTestTank TestTank;
	TestTank.Label = Label;
	TestTank.Owner = Owner;
	TestTank.bInside = bInside;
	TestTank.bDamage = bDamage;
	TestTank.bSpend = bSpend;
	// Facing away from the workshop, so the shots that empty the magazines fly off the scene.
	TestTank.Tank = SpawnTank(Owner == TestPlayer ? FriendlyTankClassPath : EnemyTankClassPath,
	                          M_Workshop->GetActorLocation() + Direction * Distance, AngleDegrees, Owner);
	Check(TestTank.Tank.IsValid(), Label + TEXT("_spawned"));
	if (TestTank.Tank.IsValid())
	{
		for (UWeaponState* Weapon : FRTSWeaponHelpers::GetWeaponsMountedOnTank(TestTank.Tank.Get()))
		{
			if (IsValid(Weapon))
			{
				Weapon->SetModuleFireRestriction(this, true);
			}
		}
	}
	M_Tanks.Add(TestTank);
}

void UFieldWorkshopTestSubsystem::TickWaitingForTanks()
{
	using namespace FieldWorkshopTestPrivate;
	RestrictTestSquadFire();
	if (GetStepSeconds() < TankPrepareDelaySeconds)
	{
		return;
	}
	PrepareTestTanks();
	M_TanksPreparedSeconds = GetNow();
	RequestScreenshot(TEXT("Drop_TanksParked"));
	SetStep(EFieldWorkshopTestStep::Measuring);
}

void UFieldWorkshopTestSubsystem::PrepareTestTanks()
{
	using namespace FieldWorkshopTestPrivate;
	for (FTestTank& TestTank : M_Tanks)
	{
		ATankMaster* Tank = TestTank.Tank.Get();
		if (not IsValid(Tank))
		{
			continue;
		}
		UHealthComponent* Health = Tank->GetHealthComponent();
		if (TestTank.bDamage && IsValid(Health))
		{
			Health->SetCurrentHealth(Health->GetMaxHealth() * DamagedHealthFraction);
		}
		TestTank.HealthAtStart = IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
		if (TestTank.bSpend)
		{
			SpendTankWeapons(TestTank);
			Check(not TestTank.SpentWeapons.IsEmpty(), TestTank.Label + TEXT("_has_spent_weapons"),
			      FString::Printf(TEXT("spent=%d"), TestTank.SpentWeapons.Num()));
		}
		UE_LOG(LogRTSFieldWorkshopTest, Display,
		       TEXT("RTS_FIELD_WORKSHOP_TEST TANK %s owner=%d inside=%d hp=%.0f/%.0f spent=%d at=%s dist=%.0f"),
		       *TestTank.Label, TestTank.Owner, TestTank.bInside, TestTank.HealthAtStart,
		       IsValid(Health) ? Health->GetMaxHealth() : -1.f, TestTank.SpentWeapons.Num(),
		       *Tank->GetActorLocation().ToCompactString(),
		       M_Workshop.IsValid() ? FVector::Dist2D(Tank->GetActorLocation(), M_Workshop->GetActorLocation()) : -1.f);
	}
}

void UFieldWorkshopTestSubsystem::SpendTankWeapons(FTestTank& TestTank)
{
	using namespace FieldWorkshopTestPrivate;
	ATankMaster* Tank = TestTank.Tank.Get();
	const FVector AimPoint = Tank->GetActorLocation() + Tank->GetActorForwardVector() * SpendShotDistanceCm
		+ FVector(0.f, 0.f, SpendShotHeightCm);
	for (UWeaponState* Weapon : FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank))
	{
		if (not IsValid(Weapon) || Weapon->GetRawWeaponData().MagCapacity <= 0)
		{
			continue;
		}
		// One shot leaves the magazine short; stopping with the reload cancelled keeps it that way, as for a tank
		// that drove back from a fight: it only reloads when it fires again.
		Weapon->SetModuleFireRestriction(this, false);
		Weapon->Fire(AimPoint);
		Weapon->StopFire(true, true);
		Weapon->SetModuleFireRestriction(this, true);
		if (not Weapon->IsWeaponFullyLoaded() && not Weapon->GetIsReloading())
		{
			TestTank.SpentWeapons.Add(Weapon);
		}
	}
}

void UFieldWorkshopTestSubsystem::TickMeasuring()
{
	using namespace FieldWorkshopTestPrivate;
	RestrictTestSquadFire();
	if (not bM_TookMidShot && GetStepSeconds() >= MeasureSeconds * 0.5f)
	{
		bM_TookMidShot = true;
		RequestScreenshot(TEXT("Drop_Repairing"));
	}
	if (GetStepSeconds() < MeasureSeconds)
	{
		return;
	}
	CheckTankResults();
	SetStep(EFieldWorkshopTestStep::WaitingForPackUp);
}

int32 UFieldWorkshopTestSubsystem::CountRefilledWeapons(const FTestTank& TestTank) const
{
	int32 Refilled = 0;
	for (const TWeakObjectPtr<UWeaponState>& Weapon : TestTank.SpentWeapons)
	{
		Refilled += Weapon.IsValid() && Weapon->IsWeaponFullyLoaded() ? 1 : 0;
	}
	return Refilled;
}

void UFieldWorkshopTestSubsystem::CheckTankResults()
{
	using namespace FieldWorkshopTestPrivate;
	const float Seconds = GetNow() - M_TanksPreparedSeconds;
	for (const FTestTank& TestTank : M_Tanks)
	{
		const ATankMaster* Tank = TestTank.Tank.Get();
		Check(IsValid(Tank), TestTank.Label + TEXT("_alive"));
		if (not IsValid(Tank))
		{
			continue;
		}
		const UHealthComponent* Health = Tank->GetHealthComponent();
		const float Gain = IsValid(Health) ? Health->GetCurrentHealth() - TestTank.HealthAtStart : 0.f;
		const bool bExpectWorkshop = TestTank.bInside && TestTank.Owner == TestPlayer;
		const FString Details = FString::Printf(TEXT("gain=%.1f over=%.1fs rate=%.1f/s refilled=%d of %d"), Gain,
		                                        Seconds, Seconds > 0.f ? Gain / Seconds : 0.f,
		                                        CountRefilledWeapons(TestTank), TestTank.SpentWeapons.Num());
		if (TestTank.bDamage && bExpectWorkshop)
		{
			Check(Gain >= ExpectedRepairPerSecond * Seconds * MinRepairShare, TestTank.Label + TEXT("_repaired"),
			      Details);
		}
		if (TestTank.bDamage && not bExpectWorkshop)
		{
			Check(Gain <= MaxUnrepairedGainHp, TestTank.Label + TEXT("_not_repaired"), Details);
		}
		if (TestTank.bSpend)
		{
			// "Rearm" is only a faster reload (user decision 2026-11-05): nobody tops a magazine up.
			Check(CountRefilledWeapons(TestTank) == 0 && not TestTank.SpentWeapons.IsEmpty(),
			      TestTank.Label + TEXT("_no_magazine_top_up"), Details);
		}
		CheckReloadBuff(TestTank.Label, FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank), TestTank.Owner,
		                bExpectWorkshop);
		CheckTankBehaviours(TestTank, bExpectWorkshop);
	}
	CheckTestSquadResults();
	RequestScreenshot(TEXT("Drop_Measured"));
}

void UFieldWorkshopTestSubsystem::SpawnTestSquads()
{
	using namespace FieldWorkshopTestPrivate;
	// Angles keep the squads off the tanks (160-325 degrees) and the dropped guns (+Y, 30 m out).
	AddTestSquad(TEXT("InfantryInside"), InfantrySquadClassPath, true, 20.f);
	AddTestSquad(TEXT("Pak40Inside"), Pak40SquadClassPath, true, 125.f);
	AddTestSquad(TEXT("InfantryOutside"), InfantrySquadClassPath, false, 45.f);
}

void UFieldWorkshopTestSubsystem::AddTestSquad(const FString& Label, const TCHAR* ClassPath, const bool bInside,
                                               const float AngleDegrees)
{
	using namespace FieldWorkshopTestPrivate;
	UWorld* World = GetWorld();
	UClass* SquadClass = LoadClass<AActor>(nullptr, ClassPath);
	if (not M_Workshop.IsValid() || not M_WorkshopComponent.IsValid() || not IsValid(World) || not IsValid(SquadClass))
	{
		Check(false, Label + TEXT("_spawned"), ClassPath);
		return;
	}
	const float Radius = M_WorkshopComponent->GetWorkshopRadius();
	const float Distance = bInside ? Radius * InsideRadiusFraction : Radius + OutsideRadiusMarginCm;
	const FVector Spot = M_Workshop->GetActorLocation() + FRotator(0.f, AngleDegrees, 0.f).Vector() * Distance;
	FVector Ground;
	if (not GlobalAbilityHelpers::TryFindGroundLocation(this, Spot, Ground))
	{
		Check(false, Label + TEXT("_spawned"), TEXT("no landscape under the squad spot"));
		return;
	}
	const FTransform Transform(FRotator(0.f, AngleDegrees, 0.f), Ground + FVector(0.f, 0.f, TankSpawnAboveGroundCm));
	FTestSquad TestSquad;
	TestSquad.Label = Label;
	TestSquad.bInside = bInside;
	TestSquad.Squad = Cast<ASquadController>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, SquadClass, Transform, static_cast<uint8>(TestPlayer)));
	Check(TestSquad.Squad.IsValid(), Label + TEXT("_spawned"));
	M_TestSquads.Add(TestSquad);
}

void UFieldWorkshopTestSubsystem::RestrictTestSquadFire()
{
	for (const FTestSquad& TestSquad : M_TestSquads)
	{
		if (not TestSquad.Squad.IsValid())
		{
			continue;
		}
		for (UWeaponState* Weapon : TestSquad.Squad->GetWeaponsOfSquad())
		{
			if (IsValid(Weapon))
			{
				Weapon->SetModuleFireRestriction(this, true);
			}
		}
	}
}

void UFieldWorkshopTestSubsystem::CheckTestSquadResults()
{
	using namespace FieldWorkshopTestPrivate;
	UClass* AmmoBehaviourClass = LoadClass<UBehaviour>(nullptr, AmmoResupplyBehaviourPath);
	for (const FTestSquad& TestSquad : M_TestSquads)
	{
		ASquadController* Squad = TestSquad.Squad.Get();
		Check(IsValid(Squad), TestSquad.Label + TEXT("_alive"));
		if (not IsValid(Squad))
		{
			continue;
		}
		CheckReloadBuff(TestSquad.Label, Squad->GetWeaponsOfSquad(), TestPlayer, TestSquad.bInside);
		const UBehaviourComp* Behaviours = Squad->GetBehaviourComponentOfSquad();
		const bool bHasAmmo = IsValid(Behaviours) && IsValid(AmmoBehaviourClass)
			&& IsValid(Behaviours->GetBehaviourByClass(AmmoBehaviourClass));
		Check(bHasAmmo == TestSquad.bInside, TestSquad.Label + (TestSquad.bInside
			                                                        ? TEXT("_has_ammo_resupply_buff")
			                                                        : TEXT("_no_workshop_behaviours")),
		      FString::Printf(TEXT("ammo=%d behaviour_component=%d"), bHasAmmo, IsValid(Behaviours)));
	}
}

void UFieldWorkshopTestSubsystem::CheckReloadBuff(const FString& Label, const TArray<UWeaponState*>& Weapons,
                                                  const int32 Owner, const bool bExpectBuff)
{
	using namespace FieldWorkshopTestPrivate;
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	const float ExpectedFactor = bExpectBuff ? BuffedReloadFactor : 1.f;
	int32 Measured = 0;
	int32 Matching = 0;
	FString Details;
	for (const UWeaponState* Weapon : Weapons)
	{
		if (not IsValid(Weapon) || not IsValid(GameState))
		{
			continue;
		}
		const FWeaponData& Raw = Weapon->GetRawWeaponData();
		const FWeaponData* Stock = GameState->GetWeaponDataOfPlayer(Owner, Raw.WeaponName);
		if (Stock == nullptr || Stock->ReloadSpeed <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const float Factor = Raw.ReloadSpeed / Stock->ReloadSpeed;
		++Measured;
		Matching += FMath::Abs(Factor - ExpectedFactor) <= ReloadFactorTolerance ? 1 : 0;
		Details += FString::Printf(TEXT("%s %.2f/%.2f=x%.2f "), *UEnum::GetValueAsString(Raw.WeaponName),
		                           Raw.ReloadSpeed, Stock->ReloadSpeed, Factor);
	}
	Check(Measured > 0 && Matching == Measured,
	      Label + (bExpectBuff ? TEXT("_reloads_faster") : TEXT("_normal_reload")),
	      FString::Printf(TEXT("expected x%.2f matching=%d of %d: %s"), ExpectedFactor, Matching, Measured, *Details));
}

void UFieldWorkshopTestSubsystem::CheckTankBehaviours(const FTestTank& TestTank, const bool bExpectWorkshopEffects)
{
	using namespace FieldWorkshopTestPrivate;
	const ATankMaster* Tank = TestTank.Tank.Get();
	const UBehaviourComp* Behaviours = IsValid(Tank) ? Tank->FindComponentByClass<UBehaviourComp>() : nullptr;
	if (not IsValid(Behaviours))
	{
		Check(false, TestTank.Label + TEXT("_has_behaviour_component"));
		return;
	}
	UClass* AmmoBehaviourClass = LoadClass<UBehaviour>(nullptr, AmmoResupplyBehaviourPath);
	const bool bHasRepair = IsValid(Behaviours->GetBehaviourByClass(UFieldWorkshopRepairBehaviour::StaticClass()));
	const bool bHasAmmo = IsValid(AmmoBehaviourClass) && IsValid(Behaviours->GetBehaviourByClass(AmmoBehaviourClass));
	const FString Details = FString::Printf(TEXT("repair=%d ammo=%d"), bHasRepair, bHasAmmo);
	if (not bExpectWorkshopEffects)
	{
		Check(not bHasRepair && not bHasAmmo, TestTank.Label + TEXT("_no_workshop_behaviours"), Details);
		return;
	}
	Check(bHasAmmo, TestTank.Label + TEXT("_has_ammo_resupply_buff"), Details);
	if (TestTank.bDamage)
	{
		Check(bHasRepair, TestTank.Label + TEXT("_has_repair_behaviour"), Details);
	}
}

void UFieldWorkshopTestSubsystem::TickWaitingForPackUp()
{
	using namespace FieldWorkshopTestPrivate;
	if (not M_WorkshopComponent.IsValid() || not M_Workshop.IsValid())
	{
		Check(false, TEXT("workshop_alive_until_pack_up"));
		Finish();
		return;
	}
	const float WorkingSeconds = M_WorkshopComponent->GetWorkshopSettings().WorkingSeconds;
	if (M_WorkshopComponent->GetWorkshopState() != EFieldWorkshopState::PackingUp)
	{
		if (GetNow() - M_WorkStartSeconds > WorkingSeconds + PackUpTimingToleranceSeconds * 2.f)
		{
			Check(false, TEXT("workshop_packs_up"), UEnum::GetValueAsString(M_WorkshopComponent->GetWorkshopState()));
			Finish();
		}
		return;
	}
	M_PackUpSeconds = GetNow();
	const float WorkedSeconds = M_PackUpSeconds - M_WorkStartSeconds;
	Check(FMath::Abs(WorkedSeconds - WorkingSeconds) <= PackUpTimingToleranceSeconds, TEXT("packs_up_after_working_time"),
	      FString::Printf(TEXT("worked=%.1fs expected=%.0fs"), WorkedSeconds, WorkingSeconds));
	Check(not M_WorkshopComponent->GetIsShowingWorkingRadius(), TEXT("radius_hidden_on_pack_up"));
	Check(not M_Workshop->GetActorEnableCollision(), TEXT("building_packing_up_no_collision"));
	M_PackUpStartZ = M_Workshop->GetActorLocation().Z;
	// Damage the repaired tank again: from now on nothing may repair it.
	for (FTestTank& TestTank : M_Tanks)
	{
		UHealthComponent* Health = TestTank.Tank.IsValid() ? TestTank.Tank->GetHealthComponent() : nullptr;
		if (TestTank.bDamage && IsValid(Health))
		{
			Health->SetCurrentHealth(Health->GetMaxHealth() * DamagedHealthFraction);
			TestTank.HealthAtStart = Health->GetCurrentHealth();
		}
	}
	RequestScreenshot(TEXT("Drop_PackingUp"));
	SetStep(EFieldWorkshopTestStep::WaitingForRemoval);
}

void UFieldWorkshopTestSubsystem::TickWaitingForRemoval()
{
	using namespace FieldWorkshopTestPrivate;
	const float PackUpSeconds = M_WorkshopComponent.IsValid()
		                            ? M_WorkshopComponent->GetWorkshopSettings().PackUpSeconds
		                            : 6.f;
	const bool bWaitedForChecks = GetStepSeconds() >= AfterPackUpCheckSeconds;
	if (M_Workshop.IsValid() && not bWaitedForChecks)
	{
		return;
	}
	if (M_Workshop.IsValid() && not bM_CheckedSinking)
	{
		bM_CheckedSinking = true;
		const float Sunk = M_PackUpStartZ - M_Workshop->GetActorLocation().Z;
		Check(Sunk > 0.f, TEXT("building_sinks_while_packing_up"), FString::Printf(TEXT("sunk=%.0f cm"), Sunk));
		RequestScreenshot(TEXT("Drop_Sinking"));
	}
	if (M_Workshop.IsValid() && GetStepSeconds() < PackUpSeconds + RemovalExtraSeconds)
	{
		return;
	}
	Check(not M_Workshop.IsValid(), TEXT("workshop_removed_after_pack_up"),
	      FString::Printf(TEXT("after=%.1fs"), GetStepSeconds()));
	for (const FTestTank& TestTank : M_Tanks)
	{
		if (not TestTank.bDamage || not TestTank.Tank.IsValid())
		{
			continue;
		}
		const UHealthComponent* Health = TestTank.Tank->GetHealthComponent();
		const float Gain = IsValid(Health) ? Health->GetCurrentHealth() - TestTank.HealthAtStart : 0.f;
		Check(Gain <= MaxUnrepairedGainHp, TestTank.Label + TEXT("_no_repair_after_pack_up"),
		      FString::Printf(TEXT("gain=%.1f"), Gain));
		CheckTankBehaviours(TestTank, false);
	}
	RequestScreenshot(TEXT("Drop_Removed"));
	for (const FTestTank& TestTank : M_Tanks)
	{
		if (TestTank.Tank.IsValid())
		{
			TestTank.Tank->Destroy();
		}
	}
	M_Tanks.Reset();
	StartKillScenario();
}

// ----------------------------------------------- scenario B: killed early

void UFieldWorkshopTestSubsystem::StartKillScenario()
{
	using namespace FieldWorkshopTestPrivate;
	M_ScenarioLabel = TEXT("Kill");
	M_TanksPreparedSeconds = -1.f;
	M_HealthAfterKill = -1.f;
	FVector Ground;
	GlobalAbilityHelpers::TryFindGroundLocation(
		this, M_Anchor + FVector(KillScenarioOffset.X, KillScenarioOffset.Y, 0.f), Ground);
	M_CameraTarget = Ground;
	M_CameraDistance = WorkshopCameraDistance;
	ABuildingExpansion* Workshop = SpawnWorkshopOnGround(Ground);
	Check(IsValid(Workshop), TEXT("workshop_spawned"));
	if (not IsValid(Workshop))
	{
		Finish();
		return;
	}
	M_Workshop = Workshop;
	M_WorkshopComponent = Workshop->FindComponentByClass<UFieldWorkshopComponent>();
	Check(M_WorkshopComponent.IsValid(), TEXT("workshop_has_component"));
	if (not M_WorkshopComponent.IsValid())
	{
		Finish();
		return;
	}
	SetStep(EFieldWorkshopTestStep::KillWaitingForRepair);
}

ABuildingExpansion* UFieldWorkshopTestSubsystem::SpawnWorkshopOnGround(const FVector& Location)
{
	using namespace FieldWorkshopTestPrivate;
	UWorld* World = GetWorld();
	UClass* WorkshopClass = LoadClass<ABuildingExpansion>(nullptr, WorkshopClassPath);
	if (not IsValid(World) || not IsValid(WorkshopClass))
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingExpansion* Workshop = World->SpawnActor<ABuildingExpansion>(
		WorkshopClass, Location, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(Workshop))
	{
		return nullptr;
	}
	// As UGA_BuildingParachuteDrop builds it, without the fall.
	if (URTSComponent* RTSComponent = Workshop->GetRTSComponent())
	{
		RTSComponent->SetOwningPlayerRuntime(TestPlayer);
	}
	Workshop->StartFieldDefenseConstruction(Location, FRotator::ZeroRotator);
	Workshop->FinishFieldDefenseConstruction();
	return Workshop;
}

void UFieldWorkshopTestSubsystem::TickKillWaitingForRepair()
{
	using namespace FieldWorkshopTestPrivate;
	if (not M_WorkshopComponent.IsValid() || not M_Workshop.IsValid())
	{
		Check(false, TEXT("workshop_alive_before_kill"));
		Finish();
		return;
	}
	const bool bWorking = M_WorkshopComponent->GetWorkshopState() == EFieldWorkshopState::Working;
	if (M_Tanks.IsEmpty())
	{
		if (not bWorking)
		{
			if (GetStepSeconds() > KillTimeoutSeconds)
			{
				Check(false, TEXT("placed_workshop_works"));
				Finish();
			}
			return;
		}
		Check(true, TEXT("placed_workshop_works"));
		AddTestTank(TEXT("DamagedInside"), TestPlayer, true, true, false, 200.f);
		M_StepStartSeconds = GetNow();
		return;
	}
	if (M_TanksPreparedSeconds < 0.f)
	{
		if (GetStepSeconds() < TankPrepareDelaySeconds)
		{
			return;
		}
		PrepareTestTanks();
		M_TanksPreparedSeconds = GetNow();
		M_StepStartSeconds = GetNow();
		return;
	}
	if (GetStepSeconds() < KillRepairSeconds)
	{
		return;
	}
	const FTestTank& TestTank = M_Tanks[0];
	const UHealthComponent* Health = TestTank.Tank.IsValid() ? TestTank.Tank->GetHealthComponent() : nullptr;
	const float Gain = IsValid(Health) ? Health->GetCurrentHealth() - TestTank.HealthAtStart : 0.f;
	Check(Gain > ExpectedRepairPerSecond, TEXT("repairs_before_kill"), FString::Printf(TEXT("gain=%.1f"), Gain));
	RequestScreenshot(TEXT("Kill_BeforeKill"));
	M_Workshop->TakeFatalDamage();
	M_KillSeconds = GetNow();
	SetStep(EFieldWorkshopTestStep::KillAfterDeath);
}

void UFieldWorkshopTestSubsystem::TickKillAfterDeath()
{
	using namespace FieldWorkshopTestPrivate;
	const FTestTank* TestTank = M_Tanks.IsEmpty() ? nullptr : &M_Tanks[0];
	const UHealthComponent* Health = TestTank && TestTank->Tank.IsValid()
		                                 ? TestTank->Tank->GetHealthComponent()
		                                 : nullptr;
	if (M_HealthAfterKill < 0.f)
	{
		if (GetStepSeconds() < KillResidualSeconds)
		{
			return;
		}
		const bool bDestroyedState = not M_WorkshopComponent.IsValid()
			|| M_WorkshopComponent->GetWorkshopState() == EFieldWorkshopState::Destroyed;
		Check(bDestroyedState, TEXT("workshop_state_destroyed"));
		Check(not M_WorkshopComponent.IsValid() || not M_WorkshopComponent->GetIsShowingWorkingRadius(),
		      TEXT("radius_hidden_on_death"));
		Check(not M_Workshop.IsValid() || not M_Workshop->IsUnitAlive(), TEXT("workshop_dead"));
		M_HealthAfterKill = IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
		RequestScreenshot(TEXT("Kill_AfterKill"));
		return;
	}
	if (GetStepSeconds() < KillResidualSeconds + KillPlateauSeconds)
	{
		return;
	}
	const float Gain = IsValid(Health) ? Health->GetCurrentHealth() - M_HealthAfterKill : 0.f;
	Check(Gain <= MaxUnrepairedGainHp, TEXT("no_repair_after_kill"),
	      FString::Printf(TEXT("gain=%.1f over=%.0fs"), Gain, KillPlateauSeconds));
	if (TestTank)
	{
		CheckTankBehaviours(*TestTank, false);
	}
	UE_LOG(LogRTSFieldWorkshopTest, Display, TEXT("RTS_FIELD_WORKSHOP_TEST KILLED_WORKSHOP actor_exists=%d after=%.1fs"),
	       M_Workshop.IsValid(), GetNow() - M_KillSeconds);
	RequestScreenshot(TEXT("Kill_Plateau"));
	Finish();
}

// ------------------------------------------------------------------ helpers

TArray<AActor*> UFieldWorkshopTestSubsystem::GetNewActorsOfClass(const UClass* ActorClass) const
{
	TArray<AActor*> NewActors;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->IsA(ActorClass) && not M_PreExistingActors.Contains(*It))
		{
			NewActors.Add(*It);
		}
	}
	return NewActors;
}

void UFieldWorkshopTestSubsystem::SnapshotExistingActors()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<ATeamWeaponController>() || It->IsA<ABuildingExpansion>())
		{
			M_PreExistingActors.Add(*It);
		}
	}
}

ATankMaster* UFieldWorkshopTestSubsystem::SpawnTank(const TCHAR* ClassPath, const FVector& Location, const float Yaw,
                                                    const int32 Owner) const
{
	using namespace FieldWorkshopTestPrivate;
	UWorld* World = GetWorld();
	UClass* TankClass = LoadClass<AActor>(nullptr, ClassPath);
	if (not IsValid(World) || not IsValid(TankClass))
	{
		return nullptr;
	}
	FVector Ground;
	if (not GlobalAbilityHelpers::TryFindGroundLocation(this, Location, Ground))
	{
		UE_LOG(LogRTSFieldWorkshopTest, Error, TEXT("RTS_FIELD_WORKSHOP_TEST no landscape under tank spot %s"),
		       *Location.ToCompactString());
		return nullptr;
	}
	const FTransform Transform(FRotator(0.f, Yaw, 0.f), Ground + FVector(0.f, 0.f, TankSpawnAboveGroundCm));
	return Cast<ATankMaster>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, TankClass, Transform,
	                                                                   static_cast<uint8>(Owner)));
}

void UFieldWorkshopTestSubsystem::UpdateCamera() const
{
	using namespace FieldWorkshopTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(CameraPitch, CameraYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(M_CameraTarget - ViewRotation.Vector() * M_CameraDistance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void UFieldWorkshopTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("FieldWorkshopTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UFieldWorkshopTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UFieldWorkshopTestSubsystem::Tick(float DeltaTime) {}
TStatId UFieldWorkshopTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFieldWorkshopTestSubsystem, STATGROUP_Tickables);
}

#endif
