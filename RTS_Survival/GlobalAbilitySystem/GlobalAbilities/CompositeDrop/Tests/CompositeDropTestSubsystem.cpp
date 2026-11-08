// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CompositeDropTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/CompositeDrop/GA_CompositeDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/CargoParachuteDescentComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCompositeDropTest, Log, All);

namespace CompositeDropTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float ScenarioTimeoutSeconds = 300.f;
	constexpr float AfterParentEndSeconds = 2.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float CameraDistance = 6500.f;
	constexpr float CameraPitch = -50.f;
	constexpr float CameraYaw = 35.f;
	constexpr float PeriodicShotSeconds = 15.f;
	// Paratroopers spread up to 750 cm, then the squad gathers; the Marder lands in its own spot.
	constexpr float SquadMaxDistanceFromTarget = 2500.f;
	constexpr float MarderMaxDistanceFromTarget = 3500.f;
	// The Marder scenario aims beside the strike's craters.
	// Beside the first scenario's Vultures: the scavengers land 30 m to the side of the Marder.
	const FVector2D MarderScenarioOffset(-3000.f, -3000.f);
	// The later scenarios land on their own ground, away from earlier squads and wrecks (map edge lies to -X, +Y).
	const FVector2D ElitePakScenarioOffset(0.f, 5000.f);
	const FVector2D PionierenPitScenarioOffset(-6000.f, 1500.f);
	// As the parachute payload test: crews finish deploying and take their posts after the last load landed.
	constexpr float GunSettleSeconds = 10.f;
	// A gun or pit counts as clear of the infantry this far from the nearest released squad.
	constexpr float MinDistanceFromInfantryCm = 800.f;
	constexpr float MinGunUpZ = 0.9f;
	constexpr float MinBuildingUpZ = 0.995f;
	constexpr float MaxGroundGapCm = 40.f;
	constexpr float MaxOperatorFromPostCm = 150.f;
	const FVector2D StrikeVictimOffsets[] = {FVector2D(600.f, 0.f), FVector2D(-600.f, 300.f)};
	constexpr float VictimSpawnAboveGroundCm = 120.f;
	constexpr int32 EnemyPlayer = 2;
	const TCHAR* const VictimClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	const TCHAR* const Ju87Marker = TEXT("StukaJu87");
	const TCHAR* const Ju390Marker = TEXT("JU390B");
	const TCHAR* const MarderMarker = TEXT("Marder_DualBarrel");
}

bool UCompositeDropTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("CompositeDropValidate"));
}

TStatId UCompositeDropTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCompositeDropTestSubsystem, STATGROUP_Tickables);
}

void UCompositeDropTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case ECompositeDropTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case ECompositeDropTestPhase::Running:
		TickRun();
		return;
	case ECompositeDropTestPhase::Finished:
		return;
	}
}

void UCompositeDropTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	const FString Label = M_Scenarios.IsValidIndex(M_ScenarioIndex) ? M_Scenarios[M_ScenarioIndex].Label : TEXT("setup");
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST CHECK PASS %s.%s %s"), *Label, *Description,
		       *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSCompositeDropTest, Error, TEXT("RTS_COMPOSITE_TEST CHECK FAIL %s.%s %s"), *Label, *Description,
	       *Details);
}

float UCompositeDropTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

float UCompositeDropTestSubsystem::GetRunSeconds() const
{
	return GetNow() - M_RunStartSeconds;
}

void UCompositeDropTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = ECompositeDropTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UCompositeDropTestSubsystem::TickWaitingForStart()
{
	using namespace CompositeDropTestPrivate;
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

	if (FParse::Param(FCommandLine::Get(), TEXT("CompositeDropStrikeControl")))
	{
		FScenario Control;
		Control.AbilityType = EGlobalAbility::GA_Ju87Strafing;
		Control.Label = TEXT("Ju87StrafingControl");
		Control.ExpectedJu87 = 1;
		Control.bExpectStrike = true;
		Control.bIsStrikeControl = true;
		M_Scenarios.Add(Control);
	}

	FScenario Luftlande;
	Luftlande.AbilityType = EGlobalAbility::GA_GerLuftlandeSturm;
	Luftlande.Label = TEXT("LuftlandeSturm");
	Luftlande.ExpectedJu87 = 2;
	Luftlande.ExpectedJu390 = 1;
	Luftlande.SquadMarker = TEXT("Vulture");
	Luftlande.ExpectedSquads = 2;
	Luftlande.bExpectStrike = true;
	Luftlande.bSquadsAfterStrike = true;
	Luftlande.bSpawnStrikeVictims = true;
	M_Scenarios.Add(Luftlande);

	FScenario MarderScavengers;
	MarderScavengers.AbilityType = EGlobalAbility::GA_GerMarderScavengerDrop;
	MarderScavengers.Label = TEXT("MarderScavengerDrop");
	MarderScavengers.ExpectedJu390 = 2;
	MarderScavengers.SquadMarker = TEXT("Scavengers");
	MarderScavengers.ExpectedSquads = 1;
	MarderScavengers.bExpectMarder = true;
	MarderScavengers.TargetOffset = MarderScenarioOffset;
	M_Scenarios.Add(MarderScavengers);

	FScenario ElitePak;
	ElitePak.AbilityType = EGlobalAbility::GA_GerEliteScavengersPakDrop;
	ElitePak.Label = TEXT("EliteScavengersPakDrop");
	ElitePak.ExpectedJu390 = 2;
	ElitePak.SquadMarker = TEXT("EliteScavengers");
	ElitePak.ExpectedSquads = 2;
	ElitePak.ExpectedGunSquads = 2;
	ElitePak.TargetOffset = ElitePakScenarioOffset;
	ElitePak.SettleSeconds = GunSettleSeconds;
	M_Scenarios.Add(ElitePak);

	FScenario PionierenPit;
	PionierenPit.AbilityType = EGlobalAbility::GA_GerSturmPionierenPakPitDrop;
	PionierenPit.Label = TEXT("SturmPionierenPakPitDrop");
	PionierenPit.ExpectedJu390 = 2;
	PionierenPit.SquadMarker = TEXT("SturmPion");
	PionierenPit.ExpectedSquads = 2;
	PionierenPit.ExpectedBuildings = 1;
	PionierenPit.TargetOffset = PionierenPitScenarioOffset;
	PionierenPit.SettleSeconds = GunSettleSeconds;
	M_Scenarios.Add(PionierenPit);

	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST ANCHOR %s"), *M_Anchor.ToCompactString());
	StartScenario(0);
}

bool UCompositeDropTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace CompositeDropTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("CompositeDropAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("CompositeDropAnchorY="), AnchorY))
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

void UCompositeDropTestSubsystem::ResetScenarioState()
{
	M_PreExistingActors.Reset();
	M_Aircraft.Reset();
	M_Squads.Reset();
	M_Marder.Reset();
	M_MarderDeliveredSeconds = -1.f;
	M_FirstStrikeShotSeconds = -1.f;
	M_LastStrikeShotSeconds = -1.f;
	M_ChildIndices.Reset();
	M_ChildStartSeconds.Reset();
	M_ChildEndSeconds.Reset();
	M_ParentEndSeconds = -1.f;
	M_EndedEventCount = 0;
	M_ParentEndOrder = 0;
	M_LastChildEndOrder = 0;
	M_FirstTouchdownSeconds = -1.f;
	M_StrikeVictims.Reset();
	M_ParentEndCount = 0;
	M_LastShotSeconds = -1.f;
	bM_TookLandingShot = false;
	M_Ability = nullptr;
}

void UCompositeDropTestSubsystem::SnapshotExistingActors()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AAircraftMaster>() || It->IsA<ASquadController>() || It->IsA<ATankMaster>()
			|| It->IsA<ABuildingExpansion>())
		{
			M_PreExistingActors.Add(*It);
		}
	}
}

void UCompositeDropTestSubsystem::StartScenario(const int32 ScenarioIndex)
{
	using namespace CompositeDropTestPrivate;
	ResetScenarioState();
	M_ScenarioIndex = ScenarioIndex;
	const FScenario& Scenario = M_Scenarios[ScenarioIndex];
	M_Phase = ECompositeDropTestPhase::Running;
	M_RunStartSeconds = GetNow();
	M_ScenarioTarget = M_Anchor;
	if (not Scenario.TargetOffset.IsZero())
	{
		GlobalAbilityHelpers::TryFindGroundLocation(
			this, M_Anchor + FVector(Scenario.TargetOffset.X, Scenario.TargetOffset.Y, 0.f), M_ScenarioTarget);
	}
	UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST SCENARIO %s target=%s"), *Scenario.Label,
	       *M_ScenarioTarget.ToCompactString());

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
	UGlobalAbility* Copy = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Scenario.AbilityType);
	if (Scenario.bIsStrikeControl)
	{
		StartStrikeControl(Copy);
		return;
	}
	M_Ability = Cast<UGA_CompositeDrop>(Copy);
	Check(IsValid(M_Ability), TEXT("template_is_composite_drop"),
	      IsValid(Copy) ? Copy->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(M_Ability) || not CheckTemplate(*M_Ability, Scenario))
	{
		FinishScenario();
		return;
	}
	M_Ability->InitGlobalAbility(TestPlayer, Manager, Controller);
	M_SquadTarget = M_ScenarioTarget;
	for (const FCompositeDropChild& Entry : M_Ability->GetChildEntries())
	{
		if (IsValid(Cast<UGA_Paratroopers>(Entry.Ability)))
		{
			// The first paratrooper child carries the infantry; a later one carries guns.
			GlobalAbilityHelpers::TryFindGroundLocation(
				this, M_ScenarioTarget + FVector(Entry.TargetOffset.X, Entry.TargetOffset.Y, 0.f), M_SquadTarget);
			break;
		}
	}

	TMap<uint8, int32> ResourcesBefore;
	if (not EnsureCanAfford(*M_Ability, ResourcesBefore))
	{
		FinishScenario();
		return;
	}
	if (Scenario.bSpawnStrikeVictims)
	{
		SpawnStrikeVictims();
	}
	SnapshotExistingActors();
	M_Ability->OnChildStarted.AddUObject(this, &UCompositeDropTestSubsystem::OnChildStarted);
	M_Ability->OnGlobalAbilityEnded.AddUObject(this, &UCompositeDropTestSubsystem::OnParentEnded);

	// The player's path: activate from the panel, then click the target.
	Controller->OnGlobaAbilityActivated(M_Ability->GetAimSettings(), FGlobalAbilitySoundSettings(), M_Ability);
	Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(M_ScenarioTarget), TEXT("click_handled"));
	CheckPaymentAndCooldown(*M_Ability, ResourcesBefore);
	RequestScreenshot(Scenario.Label + TEXT("_Clicked"));
}

void UCompositeDropTestSubsystem::SpawnStrikeVictims()
{
	using namespace CompositeDropTestPrivate;
	UWorld* World = GetWorld();
	UClass* VictimClass = LoadClass<AActor>(nullptr, VictimClassPath);
	if (not IsValid(World) || not IsValid(VictimClass))
	{
		Check(false, TEXT("strike_victim_class"));
		return;
	}
	for (const FVector2D& Offset : StrikeVictimOffsets)
	{
		FVector Ground;
		GlobalAbilityHelpers::TryFindGroundLocation(this, M_ScenarioTarget + FVector(Offset.X, Offset.Y, 0.f), Ground);
		AActor* Victim = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
			*World, VictimClass, FTransform(Ground + FVector(0.f, 0.f, VictimSpawnAboveGroundCm)), EnemyPlayer);
		M_StrikeVictims.Add(Victim);
		UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST STRIKE_VICTIM %s at=%s"),
		       IsValid(Victim) ? *Victim->GetName() : TEXT("none"), *Ground.ToCompactString());
	}
}

void UCompositeDropTestSubsystem::RemoveStrikeVictims()
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<AActor>& Victim : M_StrikeVictims)
	{
		if (not Victim.IsValid())
		{
			continue;
		}
		++Alive;
		UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST STRIKE_VICTIM_REMOVED %s hidden=%d"),
		       *Victim->GetName(), Victim->IsHidden());
		Victim->Destroy();
	}
	UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST STRIKE_VICTIMS alive=%d of %d at=%.1fs"), Alive,
	       M_StrikeVictims.Num(), GetRunSeconds());
	M_StrikeVictims.Reset();
}

void UCompositeDropTestSubsystem::StartStrikeControl(UGlobalAbility* Copy)
{
	using namespace CompositeDropTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	M_ControlAbility = Copy;
	Check(IsValid(Copy) && IsValid(Manager), TEXT("control_template"));
	if (not IsValid(Copy) || not IsValid(Manager))
	{
		FinishScenario();
		return;
	}
	SnapshotExistingActors();
	Copy->InitGlobalAbility(TestPlayer, Manager, Controller);
	Copy->ExecuteAbilityAtLocation(M_ScenarioTarget);
}

bool UCompositeDropTestSubsystem::CheckTemplate(const UGA_CompositeDrop& Ability, const FScenario& Scenario)
{
	const TArray<FCompositeDropChild>& Entries = Ability.GetChildEntries();
	Check(Entries.Num() == 2, TEXT("two_children"), FString::Printf(TEXT("children=%d"), Entries.Num()));
	bool bAllValid = Entries.Num() > 0;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FCompositeDropChild& Entry = Entries[Index];
		const UGlobalAbility* Child = Entry.Ability;
		bAllValid &= IsValid(Child);
		// A child outered elsewhere (the data asset) would be shared by every copy of the template.
		Check(IsValid(Child) && Child->GetOuter() == &Ability, FString::Printf(TEXT("child_%d_owned_by_copy"), Index),
		      IsValid(Child) ? Child->GetOuter()->GetName() : TEXT("none"));
		UE_LOG(LogRTSCompositeDropTest, Display,
		       TEXT("RTS_COMPOSITE_TEST CHILD %d class=%s delay=%.1f offset=%s aim=%s"), Index,
		       IsValid(Child) ? *Child->GetClass()->GetName() : TEXT("none"), Entry.StartDelaySeconds,
		       *Entry.TargetOffset.ToString(), *UEnum::GetValueAsString(Entry.AimMode));
	}
	const FGLobalAbilityCostState& Costs = Ability.GetAbilityCosts();
	Check(Costs.CoolDownTime > 0 && not Costs.Costs.ResourceCosts.IsEmpty(), TEXT("parent_has_cost_and_cooldown"),
	      FString::Printf(TEXT("cooldown=%d resources=%d tier=%s"), Costs.CoolDownTime,
	                      Costs.Costs.ResourceCosts.Num(), *UEnum::GetValueAsString(Ability.GetAbilityTier())));
	return bAllValid;
}

bool UCompositeDropTestSubsystem::EnsureCanAfford(const UGA_CompositeDrop& Ability, TMap<uint8, int32>& OutBefore)
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
		UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST RESOURCE %s cost=%d have=%d storage=%d"),
		       *UEnum::GetValueAsString(Cost.Key), Cost.Value, Amount, Resources->GetResourceStorage(Cost.Key));
		bCanAfford &= Amount >= Cost.Value;
	}
	Check(bCanAfford, TEXT("player_can_afford"));
	return bCanAfford;
}

void UCompositeDropTestSubsystem::CheckPaymentAndCooldown(const UGA_CompositeDrop& Ability,
                                                          const TMap<uint8, int32>& Before)
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
	Check(Costs.CoolDownRemaining == Costs.CoolDownTime && Costs.CoolDownTime > 0, TEXT("parent_cooldown_started"),
	      FString::Printf(TEXT("remaining=%d time=%d"), Costs.CoolDownRemaining, Costs.CoolDownTime));
	int32 ChildrenOnCooldown = 0;
	for (const UGlobalAbility* Child : Ability.GetRunningChildren())
	{
		ChildrenOnCooldown += Child->GetAbilityCosts().CoolDownRemaining > 0 ? 1 : 0;
	}
	Check(ChildrenOnCooldown == 0, TEXT("children_not_on_cooldown"),
	      FString::Printf(TEXT("running_children=%d on_cooldown=%d"), Ability.GetRunningChildren().Num(),
	                      ChildrenOnCooldown));
}

void UCompositeDropTestSubsystem::OnChildStarted(UGA_CompositeDrop* Parent, const int32 ChildIndex,
                                                 UGlobalAbility* Child)
{
	M_ChildStartSeconds.Add(ChildIndex, GetRunSeconds());
	// The Vultures must not land in a firefight with the strike's targets.
	if (ChildIndex > 0 && not M_StrikeVictims.IsEmpty())
	{
		RemoveStrikeVictims();
	}
	M_ChildIndices.Add(Child, ChildIndex);
	if (IsValid(Child))
	{
		Child->OnGlobalAbilityEnded.AddUObject(this, &UCompositeDropTestSubsystem::OnChildEnded);
	}
	UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST CHILD_STARTED %d %s at=%.1fs"), ChildIndex,
	       IsValid(Child) ? *Child->GetClass()->GetName() : TEXT("none"), GetRunSeconds());
}

void UCompositeDropTestSubsystem::OnChildEnded(UGlobalAbility* Child)
{
	const int32* ChildIndex = M_ChildIndices.Find(Child);
	const int32 Index = ChildIndex ? *ChildIndex : INDEX_NONE;
	M_ChildEndSeconds.Add(Index, GetRunSeconds());
	M_LastChildEndOrder = ++M_EndedEventCount;
	UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST CHILD_ENDED %d at=%.1fs"), Index,
	       GetRunSeconds());
}

void UCompositeDropTestSubsystem::OnParentEnded(UGlobalAbility* Parent)
{
	++M_ParentEndCount;
	M_ParentEndSeconds = GetRunSeconds();
	M_ParentEndOrder = ++M_EndedEventCount;
	UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST PARENT_ENDED at=%.1fs"), M_ParentEndSeconds);
}

void UCompositeDropTestSubsystem::TickRun()
{
	using namespace CompositeDropTestPrivate;
	TrackAircraft();
	TrackSquads();
	TrackMarder();
	UpdateCamera();
	if (M_LastShotSeconds < 0.f || GetRunSeconds() - M_LastShotSeconds >= PeriodicShotSeconds)
	{
		M_LastShotSeconds = GetRunSeconds();
		RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + FString::Printf(TEXT("_t%03.0f"), GetRunSeconds()));
	}
	const float WaitAfterParent = FMath::Max(AfterParentEndSeconds, M_Scenarios[M_ScenarioIndex].SettleSeconds);
	const bool bParentDone = M_ParentEndCount > 0 && GetRunSeconds() - M_ParentEndSeconds >= WaitAfterParent;
	constexpr float StrikeControlSeconds = 90.f;
	const bool bControlDone = M_Scenarios[M_ScenarioIndex].bIsStrikeControl && GetRunSeconds() >= StrikeControlSeconds;
	if (bParentDone || bControlDone || GetRunSeconds() >= ScenarioTimeoutSeconds)
	{
		FinishScenario();
	}
}

void UCompositeDropTestSubsystem::TrackAircraft()
{
	using namespace CompositeDropTestPrivate;
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		AAircraftMaster* Aircraft = *It;
		if (not IsValid(Aircraft) || M_PreExistingActors.Contains(Aircraft))
		{
			continue;
		}
		FAircraftTrack* Track = M_Aircraft.FindByPredicate([Aircraft](const FAircraftTrack& Candidate)
		{
			return Candidate.Aircraft.Get() == Aircraft;
		});
		if (Track == nullptr)
		{
			FAircraftTrack NewTrack;
			NewTrack.Aircraft = Aircraft;
			NewTrack.ClassName = Aircraft->GetClass()->GetName();
			M_Aircraft.Add(NewTrack);
			Track = &M_Aircraft.Last();
			UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST AIRCRAFT %s at=%.1fs location=%s"),
			       *Track->ClassName, GetRunSeconds(), *Aircraft->GetActorLocation().ToCompactString());
		}
		if (Track->ClassName.Contains(Ju87Marker))
		{
			TrackStrikeAircraft(*Aircraft, *Track);
		}
	}
}

void UCompositeDropTestSubsystem::TrackStrikeAircraft(AAircraftMaster& Aircraft, FAircraftTrack& Track)
{
	constexpr float StateLogInterval = 2.f;
	const TArray<UWeaponState*> Weapons = Aircraft.GetAllAircraftWeapons();
	if (Track.LastMagazines.Num() != Weapons.Num())
	{
		Track.LastMagazines.Init(0, Weapons.Num());
		Track.Shots.Init(0, Weapons.Num());
		Track.Calibres.Init(0.f, Weapons.Num());
		for (int32 WeaponIndex = 0; WeaponIndex < Weapons.Num(); ++WeaponIndex)
		{
			if (IsValid(Weapons[WeaponIndex]))
			{
				Track.LastMagazines[WeaponIndex] = Weapons[WeaponIndex]->GetCurrentMagCapacity();
				Track.Calibres[WeaponIndex] = Weapons[WeaponIndex]->GetRawWeaponData().WeaponCalibre;
			}
		}
	}
	for (int32 WeaponIndex = 0; WeaponIndex < Weapons.Num(); ++WeaponIndex)
	{
		if (not IsValid(Weapons[WeaponIndex]))
		{
			continue;
		}
		const int32 Magazine = Weapons[WeaponIndex]->GetCurrentMagCapacity();
		if (Magazine < Track.LastMagazines[WeaponIndex])
		{
			Track.Shots[WeaponIndex] += Track.LastMagazines[WeaponIndex] - Magazine;
			MarkStrikeShot();
		}
		Track.LastMagazines[WeaponIndex] = Magazine;
	}
	// Bombs are not in the weapon list: the bomb bay counts the bombs it still holds.
	const UBombComponent* BombBay = Aircraft.GetBombComponent();
	const int32 BombCount = IsValid(BombBay) ? BombBay->GetActiveBombCount() : INDEX_NONE;
	if (Track.LastBombCount != INDEX_NONE && BombCount < Track.LastBombCount)
	{
		Track.BombsThrown += Track.LastBombCount - BombCount;
		MarkStrikeShot();
	}
	Track.LastBombCount = BombCount;
	if (GetRunSeconds() >= Track.NextStateLogSeconds)
	{
		Track.NextStateLogSeconds = GetRunSeconds() + StateLogInterval;
		UE_LOG(LogRTSCompositeDropTest, Display,
		       TEXT("RTS_COMPOSITE_TEST STRIKE_STATE at=%.1fs dist=%.0f z=%.0f mag=%d bombs=%d"), GetRunSeconds(),
		       FVector::Dist2D(Aircraft.GetActorLocation(), M_ScenarioTarget), Aircraft.GetActorLocation().Z,
		       Track.LastMagazines.IsEmpty() ? -1 : Track.LastMagazines[0], BombCount);
	}
}

void UCompositeDropTestSubsystem::MarkStrikeShot()
{
	if (M_FirstStrikeShotSeconds < 0.f)
	{
		M_FirstStrikeShotSeconds = GetRunSeconds();
		UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST FIRST_STRIKE_SHOT at=%.1fs"),
		       M_FirstStrikeShotSeconds);
		RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + TEXT("_StrikeFiring"));
	}
	M_LastStrikeShotSeconds = GetRunSeconds();
}

void UCompositeDropTestSubsystem::TrackSquads()
{
	const FScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	if (Scenario.SquadMarker.IsEmpty())
	{
		return;
	}
	for (TActorIterator<ASquadController> It(GetWorld()); It; ++It)
	{
		ASquadController* Squad = *It;
		if (not IsValid(Squad) || M_PreExistingActors.Contains(Squad)
			|| not Squad->GetClass()->GetName().Contains(Scenario.SquadMarker))
		{
			continue;
		}
		FSquadTrack* Track = M_Squads.FindByPredicate([Squad](const FSquadTrack& Candidate)
		{
			return Candidate.Squad.Get() == Squad;
		});
		if (Track == nullptr)
		{
			FSquadTrack NewTrack;
			NewTrack.Squad = Squad;
			NewTrack.ClassName = Squad->GetClass()->GetName();
			M_Squads.Add(NewTrack);
			Track = &M_Squads.Last();
			UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST SQUAD_SPAWNED %s at=%.1fs in_drop=%d"),
			       *Track->ClassName, GetRunSeconds(), Squad->GetIsInParachuteDrop());
		}
		const URTSComponent* RTSComponent = Squad->GetRTSComponent();
		Track->Owner = IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : 0;
		if (Squad->GetIsInParachuteDrop())
		{
			Track->bM_WasInDrop = true;
			TrackFirstTouchdown(*Squad);
			continue;
		}
		if (Track->ReleasedSeconds < 0.f && Track->bM_WasInDrop)
		{
			Track->ReleasedSeconds = GetRunSeconds();
			Track->ReleasedLocation = Squad->GetActorLocation();
			TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
			if (Units.Num() > 0 && IsValid(Units[0]))
			{
				Track->ReleasedLocation = Units[0]->GetActorLocation();
			}
			UE_LOG(LogRTSCompositeDropTest, Display,
			       TEXT("RTS_COMPOSITE_TEST SQUAD_RELEASED %s at=%.1fs owner=%d units=%d location=%s"),
			       *Track->ClassName, Track->ReleasedSeconds, Track->Owner, Units.Num(),
			       *Track->ReleasedLocation.ToCompactString());
			if (not bM_TookLandingShot)
			{
				bM_TookLandingShot = true;
				RequestScreenshot(Scenario.Label + TEXT("_SquadLanded"));
			}
		}
	}
}

void UCompositeDropTestSubsystem::TrackFirstTouchdown(ASquadController& Squad)
{
	// A standing soldier's capsule centre is about a metre above the ground.
	constexpr float TouchdownHeightCm = 200.f;
	if (M_FirstTouchdownSeconds >= 0.f)
	{
		return;
	}
	for (const ASquadUnit* Unit : Squad.GetSquadUnitsChecked())
	{
		if (not IsValid(Unit) || Unit->IsHidden())
		{
			continue;
		}
		if (Unit->GetActorLocation().Z <= M_SquadTarget.Z + TouchdownHeightCm)
		{
			M_FirstTouchdownSeconds = GetRunSeconds();
			UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST FIRST_TOUCHDOWN %s at=%.1fs"),
			       *Unit->GetName(), M_FirstTouchdownSeconds);
			return;
		}
	}
}

float UCompositeDropTestSubsystem::GetStrikeWindowSeconds() const
{
	if (not IsValid(M_Ability))
	{
		return -1.f;
	}
	for (const FCompositeDropChild& Entry : M_Ability->GetChildEntries())
	{
		const UGlobalAbility* Child = Entry.Ability;
		const FStructProperty* SettingsProperty = IsValid(Child)
			                                          ? CastField<FStructProperty>(Child->GetClass()->FindPropertyByName(
				                                          TEXT("M_AircraftStrafingSettings")))
			                                          : nullptr;
		if (SettingsProperty == nullptr)
		{
			continue;
		}
		const void* Settings = SettingsProperty->ContainerPtrToValuePtr<void>(Child);
		const FFloatProperty* TimeProperty = CastField<FFloatProperty>(
			SettingsProperty->Struct->FindPropertyByName(TEXT("TotalStrafeTime")));
		return TimeProperty != nullptr ? TimeProperty->GetPropertyValue_InContainer(Settings) : -1.f;
	}
	return -1.f;
}

void UCompositeDropTestSubsystem::TrackMarder()
{
	using namespace CompositeDropTestPrivate;
	if (not M_Scenarios[M_ScenarioIndex].bExpectMarder)
	{
		return;
	}
	if (not M_Marder.IsValid())
	{
		for (TActorIterator<ATankMaster> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && not M_PreExistingActors.Contains(*It)
				&& It->GetClass()->GetName().Contains(MarderMarker))
			{
				M_Marder = *It;
				UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST MARDER_SPAWNED %s at=%.1fs"),
				       *It->GetClass()->GetName(), GetRunSeconds());
				break;
			}
		}
	}
	if (not M_Marder.IsValid() || M_MarderDeliveredSeconds >= 0.f)
	{
		return;
	}
	if (M_Marder->GetAttachParentActor() == nullptr)
	{
		M_MarderDeliveredSeconds = GetRunSeconds();
		UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST MARDER_DELIVERED at=%.1fs location=%s"),
		       M_MarderDeliveredSeconds, *M_Marder->GetActorLocation().ToCompactString());
		RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + TEXT("_MarderDelivered"));
	}
}

int32 UCompositeDropTestSubsystem::CountAircraft(const TCHAR* ClassMarker) const
{
	int32 Count = 0;
	for (const FAircraftTrack& Track : M_Aircraft)
	{
		Count += Track.ClassName.Contains(ClassMarker) ? 1 : 0;
	}
	return Count;
}

void UCompositeDropTestSubsystem::FinishScenario()
{
	using namespace CompositeDropTestPrivate;
	if (not M_Scenarios.IsValidIndex(M_ScenarioIndex))
	{
		Finish();
		return;
	}
	const FScenario& Scenario = M_Scenarios[M_ScenarioIndex];
	RequestScreenshot(Scenario.Label + TEXT("_Final"));
	if (Scenario.bIsStrikeControl)
	{
		Check(CountAircraft(Ju87Marker) == Scenario.ExpectedJu87, TEXT("ju87_count"),
		      FString::Printf(TEXT("seen=%d"), CountAircraft(Ju87Marker)));
		Check(M_FirstStrikeShotSeconds >= 0.f, TEXT("strike_fired"),
		      FString::Printf(TEXT("first=%.1fs last=%.1fs"), M_FirstStrikeShotSeconds, M_LastStrikeShotSeconds));
		M_ControlAbility = nullptr;
	}
	if (IsValid(M_Ability))
	{
		Check(CountAircraft(Ju87Marker) == Scenario.ExpectedJu87, TEXT("ju87_count"),
		      FString::Printf(TEXT("seen=%d expected=%d"), CountAircraft(Ju87Marker), Scenario.ExpectedJu87));
		Check(CountAircraft(Ju390Marker) == Scenario.ExpectedJu390, TEXT("ju390_count"),
		      FString::Printf(TEXT("seen=%d expected=%d"), CountAircraft(Ju390Marker), Scenario.ExpectedJu390));
		Check(M_Aircraft.Num() == Scenario.ExpectedJu87 + Scenario.ExpectedJu390, TEXT("no_other_aircraft"),
		      FString::Printf(TEXT("seen=%d"), M_Aircraft.Num()));
		if (Scenario.bExpectStrike)
		{
			Check(M_FirstStrikeShotSeconds >= 0.f, TEXT("strike_fired"),
			      FString::Printf(TEXT("first=%.1fs last=%.1fs"), M_FirstStrikeShotSeconds, M_LastStrikeShotSeconds));
		}

		int32 ReleasedSquads = 0;
		int32 PlayerSquads = 0;
		int32 NearSquads = 0;
		float FirstReleaseSeconds = TNumericLimits<float>::Max();
		for (const FSquadTrack& Track : M_Squads)
		{
			ReleasedSquads += Track.ReleasedSeconds >= 0.f ? 1 : 0;
			PlayerSquads += Track.Owner == TestPlayer ? 1 : 0;
			const float Distance = FVector::Dist2D(Track.ReleasedLocation, M_SquadTarget);
			NearSquads += Track.ReleasedSeconds >= 0.f && Distance <= SquadMaxDistanceFromTarget ? 1 : 0;
			if (Track.ReleasedSeconds >= 0.f)
			{
				FirstReleaseSeconds = FMath::Min(FirstReleaseSeconds, Track.ReleasedSeconds);
			}
		}
		Check(M_Squads.Num() == Scenario.ExpectedSquads, TEXT("squads_spawned"),
		      FString::Printf(TEXT("seen=%d expected=%d marker=%s"), M_Squads.Num(), Scenario.ExpectedSquads,
		                      *Scenario.SquadMarker));
		Check(ReleasedSquads == Scenario.ExpectedSquads, TEXT("squads_landed_and_released"),
		      FString::Printf(TEXT("released=%d"), ReleasedSquads));
		Check(PlayerSquads == Scenario.ExpectedSquads, TEXT("squads_owned_by_player"),
		      FString::Printf(TEXT("player=%d"), PlayerSquads));
		Check(NearSquads == Scenario.ExpectedSquads, TEXT("squads_near_their_target"),
		      FString::Printf(TEXT("near=%d max=%.0f squad_target=%s"), NearSquads, SquadMaxDistanceFromTarget,
		                      *M_SquadTarget.ToCompactString()));
		for (const FAircraftTrack& Track : M_Aircraft)
		{
			for (int32 WeaponIndex = 0; WeaponIndex < Track.Shots.Num(); ++WeaponIndex)
			{
				UE_LOG(LogRTSCompositeDropTest, Display,
				       TEXT("RTS_COMPOSITE_TEST STRIKE_WEAPON %s weapon=%d calibre=%.1f shots=%d"), *Track.ClassName,
				       WeaponIndex, Track.Calibres[WeaponIndex], Track.Shots[WeaponIndex]);
			}
			if (Track.LastBombCount != INDEX_NONE)
			{
				UE_LOG(LogRTSCompositeDropTest, Display, TEXT("RTS_COMPOSITE_TEST STRIKE_BOMBS %s thrown=%d"),
				       *Track.ClassName, Track.BombsThrown);
			}
		}
		if (Scenario.bSquadsAfterStrike)
		{
			Check(M_LastStrikeShotSeconds >= 0.f && FirstReleaseSeconds > M_LastStrikeShotSeconds,
			      TEXT("squads_land_after_strike"),
			      FString::Printf(TEXT("last_strike_shot=%.1fs first_release=%.1fs margin=%.1fs"),
			                      M_LastStrikeShotSeconds, FirstReleaseSeconds,
			                      FirstReleaseSeconds - M_LastStrikeShotSeconds));
			// The Ju 87s stop firing when their strike time runs out, so no paratrooper may touch down before it.
			const float StrikeWindow = GetStrikeWindowSeconds();
			const float* StrikeEnd = M_ChildEndSeconds.Find(0);
			Check(StrikeWindow > 0.f && M_FirstTouchdownSeconds > StrikeWindow,
			      TEXT("first_touchdown_after_strike_window"),
			      FString::Printf(TEXT("strike_window=%.1fs first_touchdown=%.1fs strike_aircraft_gone=%.1fs"),
			                      StrikeWindow, M_FirstTouchdownSeconds, StrikeEnd ? *StrikeEnd : -1.f));
		}
		if (Scenario.bExpectMarder)
		{
			Check(M_Marder.IsValid() && M_MarderDeliveredSeconds >= 0.f, TEXT("marder_delivered"),
			      FString::Printf(TEXT("at=%.1fs"), M_MarderDeliveredSeconds));
			if (M_Marder.IsValid())
			{
				ATankMaster* Marder = M_Marder.Get();
				const URTSComponent* MarderRTS = Marder->FindComponentByClass<URTSComponent>();
				const int32 MarderOwner = IsValid(MarderRTS) ? MarderRTS->GetOwningPlayer() : 0;
				Check(MarderOwner == TestPlayer, TEXT("marder_owned_by_player"),
				      FString::Printf(TEXT("owner=%d"), MarderOwner));
				const float Distance = FVector::Dist2D(Marder->GetActorLocation(), M_ScenarioTarget);
				Check(Distance <= MarderMaxDistanceFromTarget, TEXT("marder_near_target"),
				      FString::Printf(TEXT("distance=%.0f"), Distance));
			}
		}

		CheckGunSquads(Scenario);
		CheckDroppedBuildings(Scenario);

		// Every child started, every child ended, and the parent once, after the last child.
		const int32 ChildCount = M_Ability->GetChildEntries().Num();
		float LastChildEnd = -1.f;
		for (const TPair<int32, float>& ChildEnd : M_ChildEndSeconds)
		{
			LastChildEnd = FMath::Max(LastChildEnd, ChildEnd.Value);
		}
		Check(M_ChildStartSeconds.Num() == ChildCount, TEXT("all_children_started"),
		      FString::Printf(TEXT("started=%d children=%d"), M_ChildStartSeconds.Num(), ChildCount));
		for (const TPair<int32, float>& ChildStart : M_ChildStartSeconds)
		{
			const float Delay = M_Ability->GetChildEntries().IsValidIndex(ChildStart.Key)
				                    ? M_Ability->GetChildEntries()[ChildStart.Key].StartDelaySeconds
				                    : -1.f;
			Check(FMath::Abs(ChildStart.Value - Delay) <= 0.5f, FString::Printf(TEXT("child_%d_started_on_delay"),
				      ChildStart.Key), FString::Printf(TEXT("at=%.1fs delay=%.1fs"), ChildStart.Value, Delay));
		}
		Check(M_ChildEndSeconds.Num() == ChildCount, TEXT("all_children_ended"),
		      FString::Printf(TEXT("ended=%d children=%d"), M_ChildEndSeconds.Num(), ChildCount));
		Check(M_ParentEndCount == 1, TEXT("parent_ended_once"), FString::Printf(TEXT("count=%d"), M_ParentEndCount));
		Check(M_ParentEndSeconds >= 0.f && LastChildEnd >= 0.f && M_ParentEndOrder > M_LastChildEndOrder,
		      TEXT("parent_ended_after_children"),
		      FString::Printf(TEXT("parent=%.1fs order=%d last_child=%.1fs order=%d"), M_ParentEndSeconds,
		                      M_ParentEndOrder, LastChildEnd, M_LastChildEndOrder));
		Check(M_Ability->GetRunningChildren().IsEmpty(), TEXT("no_children_left_running"));
		M_Ability->OnChildStarted.RemoveAll(this);
		M_Ability->OnGlobalAbilityEnded.RemoveAll(this);
	}
	if (M_Scenarios.IsValidIndex(M_ScenarioIndex + 1))
	{
		StartScenario(M_ScenarioIndex + 1);
		return;
	}
	Finish();
}

TArray<AActor*> UCompositeDropTestSubsystem::GetNewActorsOfClass(const UClass* ActorClass) const
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

float UCompositeDropTestSubsystem::GetDistanceToNearestInfantry(const FVector& Location) const
{
	float Nearest = TNumericLimits<float>::Max();
	for (const FSquadTrack& Track : M_Squads)
	{
		if (Track.ReleasedSeconds >= 0.f)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist2D(Location, Track.ReleasedLocation));
		}
	}
	return Nearest;
}

void UCompositeDropTestSubsystem::CheckGunSquads(const FScenario& Scenario)
{
	using namespace CompositeDropTestPrivate;
	if (Scenario.ExpectedGunSquads <= 0)
	{
		return;
	}
	const TArray<AActor*> GunSquads = GetNewActorsOfClass(ATeamWeaponController::StaticClass());
	Check(GunSquads.Num() == Scenario.ExpectedGunSquads, TEXT("gun_squads_dropped"),
	      FString::Printf(TEXT("squads=%d expected=%d"), GunSquads.Num(), Scenario.ExpectedGunSquads));
	for (AActor* Actor : GunSquads)
	{
		ATeamWeaponController* Squad = Cast<ATeamWeaponController>(Actor);
		const FString SquadName = Squad->GetClass()->GetName();
		Check(not Squad->GetIsInParachuteDrop(), TEXT("gun_squad_released"), SquadName);
		const URTSComponent* RTSComponent = Squad->GetRTSComponent();
		Check(IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == TestPlayer, TEXT("gun_squad_owned_by_player"),
		      SquadName);
		ATeamWeapon* Gun = Squad->GetControlledTeamWeaponNoReport();
		Check(IsValid(Gun), TEXT("gun_exists"), SquadName);
		if (not IsValid(Gun))
		{
			continue;
		}
		Check(not Gun->IsHidden() && Gun->GetActorEnableCollision(), TEXT("gun_visible_with_collision"), Gun->GetName());
		Check(Gun->FindComponentByClass<UCargoParachuteDescentComponent>() == nullptr, TEXT("gun_chute_removed"));
		const float UpZ = Gun->GetActorUpVector().Z;
		Check(UpZ >= MinGunUpZ, TEXT("gun_upright"), FString::Printf(TEXT("up_z=%.3f"), UpZ));
		float GroundZ = 0.f;
		const bool bHasGround = Gun->TryGetGroundHeightAt(Gun->GetActorLocation(), GroundZ);
		const float Gap = bHasGround ? Gun->GetActorLocation().Z - GroundZ : 99999.f;
		Check(bHasGround && FMath::Abs(Gap) <= MaxGroundGapCm, TEXT("gun_on_ground"),
		      FString::Printf(TEXT("gap=%.1f at=%s"), Gap, *Gun->GetActorLocation().ToCompactString()));
		Check(Squad->GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed, TEXT("gun_deployed"),
		      Squad->GetOrderDebugDescription());
		TArray<ASquadUnit*> Operators;
		Squad->GetCrewOperators(Operators);
		Check(Operators.Num() >= Gun->GetRequiredOperators() && Gun->GetRequiredOperators() > 0,
		      TEXT("gun_crewed"),
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
		const float FromInfantry = GetDistanceToNearestInfantry(Gun->GetActorLocation());
		Check(FromInfantry >= MinDistanceFromInfantryCm, TEXT("gun_clear_of_infantry"),
		      FString::Printf(TEXT("nearest_squad=%.0f"), FromInfantry));
	}
}

void UCompositeDropTestSubsystem::CheckDroppedBuildings(const FScenario& Scenario)
{
	using namespace CompositeDropTestPrivate;
	if (Scenario.ExpectedBuildings <= 0)
	{
		return;
	}
	const TArray<AActor*> Buildings = GetNewActorsOfClass(ABuildingExpansion::StaticClass());
	Check(Buildings.Num() == Scenario.ExpectedBuildings, TEXT("buildings_dropped"),
	      FString::Printf(TEXT("buildings=%d expected=%d"), Buildings.Num(), Scenario.ExpectedBuildings));
	for (AActor* Actor : Buildings)
	{
		ABuildingExpansion* Building = Cast<ABuildingExpansion>(Actor);
		const FString Name = Building->GetClass()->GetName();
		Check(not Building->IsHidden() && Building->GetActorEnableCollision(), TEXT("building_visible_with_collision"),
		      Name);
		Check(Building->FindComponentByClass<UCargoParachuteDescentComponent>() == nullptr,
		      TEXT("building_chute_removed"), Name);
		const float UpZ = Building->GetActorUpVector().Z;
		Check(UpZ >= MinBuildingUpZ, TEXT("building_upright"), FString::Printf(TEXT("%s up_z=%.4f"), *Name, UpZ));
		FVector Ground;
		const bool bHasGround = GlobalAbilityHelpers::TryFindGroundLocation(this, Building->GetActorLocation(), Ground);
		const float Gap = bHasGround ? Building->GetActorLocation().Z - Ground.Z : 99999.f;
		Check(FMath::Abs(Gap) <= MaxGroundGapCm, TEXT("building_on_ground"),
		      FString::Printf(TEXT("%s gap=%.1f at=%s"), *Name, Gap, *Building->GetActorLocation().ToCompactString()));
		const URTSComponent* RTSComponent = Building->GetRTSComponent();
		const int32 Owner = IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1;
		Check(Owner == TestPlayer, TEXT("building_owned_by_player"), FString::Printf(TEXT("%s owner=%d"), *Name, Owner));
		Check(Building->GetStatus() == EBuildingExpansionStatus::BXS_Built, TEXT("building_built"),
		      FString::Printf(TEXT("%s status=%s"), *Name, *UEnum::GetValueAsString(Building->GetStatus())));
		Check(Building->IsUnitAlive(), TEXT("building_alive"), Name);
		const USelectionComponent* Selection = Building->GetSelectionComponent();
		Check(IsValid(Selection) && Selection->GetCanBeSelected(), TEXT("building_selectable"), Name);
		Check(not Building->GetTurrets().IsEmpty(), TEXT("building_has_weapons"),
		      FString::Printf(TEXT("%s turrets=%d"), *Name, Building->GetTurrets().Num()));
		const float FromInfantry = GetDistanceToNearestInfantry(Building->GetActorLocation());
		Check(FromInfantry >= MinDistanceFromInfantryCm, TEXT("building_clear_of_infantry"),
		      FString::Printf(TEXT("nearest_squad=%.0f"), FromInfantry));
	}
}

void UCompositeDropTestSubsystem::UpdateCamera() const
{
	using namespace CompositeDropTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(CameraPitch, CameraYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(M_ScenarioTarget - ViewRotation.Vector() * CameraDistance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void UCompositeDropTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("CompositeDropTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UCompositeDropTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UCompositeDropTestSubsystem::Tick(float DeltaTime) {}
TStatId UCompositeDropTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCompositeDropTestSubsystem, STATGROUP_Tickables);
}

#endif
