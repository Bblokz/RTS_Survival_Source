// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SonnengewehrTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Sonnengewehr/GA_Sonnengewehr.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Sonnengewehr/SonnengewehrBeam.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSonnengewehrTest, Log, All);

namespace SonnengewehrTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;
	constexpr float StartDelaySeconds = 3.f;
	// Squads spawn their soldiers a moment after the controller.
	constexpr float PlaceVictimsSeconds = 6.f;
	constexpr float RunTimeoutSeconds = 60.f;
	// After the sweep: one screenshot while the scorch mark still glows, one once it has cooled.
	constexpr float GlowingScorchShotSeconds = 3.f;
	constexpr float AfterSweepSeconds = 9.5f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float StripLengthCm = 5000.f;
	constexpr float VictimSpawnAboveGroundCm = 120.f;

	// Along the strip from its start, and to its side (+ is left of the sweep direction).
	struct FVictimSpot
	{
		float Along;
		float Lateral;
	};
	const FVictimSpot OnPathTanks[] = {{1000.f, 0.f}, {2500.f, 0.f}, {4000.f, 0.f}};
	const FVictimSpot OffPathTanks[] = {{2500.f, 1700.f}, {1000.f, -1700.f}};
	const FVictimSpot OnPathSquad = {3200.f, 0.f};
	const FVictimSpot OffPathSquad = {3200.f, -2300.f};
	// The firing player's own units right on the path: the beam must spare them (no friendly fire, 2026-11-05).
	const FVictimSpot FriendlyOnPathTank = {1750.f, 0.f};
	const FVictimSpot FriendlyOnPathSquad = {4650.f, 0.f};
	constexpr int32 MinFriendlyOnPath = 2;
	// GA_GerSonnengewehr costs as much as the Panzer Grenadiers (user decision 2026-11-05).
	constexpr int32 ExpectedRadixiteCost = 675;

	// Inside this lateral distance minus the half width a unit must burn; past half width plus this it must not.
	constexpr float OnPathInsetCm = 50.f;
	constexpr float OffPathMarginCm = 700.f;
	// The beam looks for units every damage interval; a frame or two on top of that.
	constexpr float TimingToleranceSeconds = 0.35f;
	// A tank's hull reaches this far in front of its origin, so the beam edge touches it that much earlier.
	constexpr float VehicleReachCm = 450.f;
	// A soldier's capsule reaches past its origin too.
	constexpr float SoldierReachCm = 100.f;
	constexpr float HealthDropEpsilon = 0.5f;

	const TCHAR* const TankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	const TCHAR* const SquadClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");

	// Camera framing.
	constexpr float OverviewDistance = 7500.f;
	constexpr float OverviewPitch = -48.f;
	constexpr float FollowDistance = 4200.f;
	constexpr float FollowPitch = -40.f;
	// The camera looks across the strip so the sweep runs over the screen.
	constexpr float CameraYawOffset = 70.f;
	constexpr float MidSweepFraction = 0.5f;
	constexpr float LateSweepFraction = 0.85f;
	constexpr float WarningShotSeconds = 3.4f;
	constexpr float IgnitionShotSeconds = 0.3f;
}

bool USonnengewehrTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("SonnengewehrValidate"));
}

TStatId USonnengewehrTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USonnengewehrTestSubsystem, STATGROUP_Tickables);
}

void USonnengewehrTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case ESonnengewehrTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case ESonnengewehrTestPhase::PlacingVictims:
		TickPlacingVictims();
		return;
	case ESonnengewehrTestPhase::Running:
		TickRunning();
		return;
	case ESonnengewehrTestPhase::AfterSweep:
		TickAfterSweep();
		return;
	case ESonnengewehrTestPhase::Finished:
		return;
	}
}

void USonnengewehrTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSSonnengewehrTest, Error, TEXT("RTS_SONNEN_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float USonnengewehrTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

float USonnengewehrTestSubsystem::GetRunSeconds() const
{
	return GetNow() - M_RunStartSeconds;
}

void USonnengewehrTestSubsystem::EnterPhase(const ESonnengewehrTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
}

void USonnengewehrTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	EnterPhase(ESonnengewehrTestPhase::Finished);
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void USonnengewehrTestSubsystem::TickWaitingForStart()
{
	using namespace SonnengewehrTestPrivate;
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
	BuildStrip();
	SpawnVictims();
	EnterPhase(ESonnengewehrTestPhase::PlacingVictims);
}

bool USonnengewehrTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace SonnengewehrTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SonnengewehrAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("SonnengewehrAnchorY="), AnchorY))
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

void USonnengewehrTestSubsystem::BuildStrip()
{
	using namespace SonnengewehrTestPrivate;
	M_StripDirection = FVector::ForwardVector;
	const FVector HalfLine = M_StripDirection * (StripLengthCm * 0.5f);
	GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor - HalfLine, M_StripStart);
	GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor + HalfLine, M_StripEnd);
	UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST STRIP start=%s end=%s"),
	       *M_StripStart.ToCompactString(), *M_StripEnd.ToCompactString());
}

void USonnengewehrTestSubsystem::SpawnVictims()
{
	using namespace SonnengewehrTestPrivate;
	int32 TankIndex = 0;
	for (const FVictimSpot& Spot : OnPathTanks)
	{
		AddVictim(SpawnUnit(TankClassPath, Spot.Along, Spot.Lateral, EnemyPlayer),
		          FString::Printf(TEXT("tank_on_%d"), TankIndex++), true);
	}
	TankIndex = 0;
	for (const FVictimSpot& Spot : OffPathTanks)
	{
		AddVictim(SpawnUnit(TankClassPath, Spot.Along, Spot.Lateral, EnemyPlayer),
		          FString::Printf(TEXT("tank_off_%d"), TankIndex++), true);
	}
	AddVictim(SpawnUnit(TankClassPath, FriendlyOnPathTank.Along, FriendlyOnPathTank.Lateral, TestPlayer),
	          TEXT("friendly_tank_on"), true, true);
	SpawnSquad(TEXT("squad_on"), OnPathSquad.Along, OnPathSquad.Lateral, EnemyPlayer);
	SpawnSquad(TEXT("squad_off"), OffPathSquad.Along, OffPathSquad.Lateral, EnemyPlayer);
	SpawnSquad(TEXT("friendly_squad_on"), FriendlyOnPathSquad.Along, FriendlyOnPathSquad.Lateral, TestPlayer);
	PacifyVictims();
}

void USonnengewehrTestSubsystem::SpawnSquad(const TCHAR* Label, const float Along, const float Lateral,
                                            const uint8 OwningPlayer)
{
	using namespace SonnengewehrTestPrivate;
	ASquadController* Squad = Cast<ASquadController>(SpawnUnit(SquadClassPath, Along, Lateral, OwningPlayer));
	Check(IsValid(Squad), FString::Printf(TEXT("spawned_%s"), Label));
	M_Squads.Add(Squad);
	M_SquadLabels.Add(Label);
	M_SquadOwners.Add(OwningPlayer);
}

void USonnengewehrTestSubsystem::PacifyVictims() const
{
	for (const FVictim& Victim : M_Victims)
	{
		const ATankMaster* Tank = Cast<ATankMaster>(Victim.Actor.Get());
		if (not IsValid(Tank))
		{
			continue;
		}
		for (ACPPTurretsMaster* Turret : Tank->GetTurrets())
		{
			if (IsValid(Turret))
			{
				Turret->DisableTurret();
			}
		}
		for (UHullWeaponComponent* HullWeapon : Tank->GetHullWeapons())
		{
			if (IsValid(HullWeapon))
			{
				HullWeapon->DisableHullWeapon();
			}
		}
	}
	for (const TWeakObjectPtr<ASquadController>& Squad : M_Squads)
	{
		if (not Squad.IsValid())
		{
			continue;
		}
		for (const ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			AInfantryWeaponMaster* Weapon = IsValid(Soldier) ? Soldier->GetInfantryWeapon() : nullptr;
			if (IsValid(Weapon))
			{
				Weapon->DisableWeaponSearch();
			}
		}
	}
}

AActor* USonnengewehrTestSubsystem::SpawnUnit(const TCHAR* ClassPath, const float Along, const float Lateral,
                                             const uint8 OwningPlayer)
{
	using namespace SonnengewehrTestPrivate;
	UWorld* World = GetWorld();
	UClass* UnitClass = LoadClass<AActor>(nullptr, ClassPath);
	if (not IsValid(World) || not IsValid(UnitClass))
	{
		Check(false, TEXT("victim_class_loaded"), ClassPath);
		return nullptr;
	}
	const FVector Left = FVector::CrossProduct(FVector::UpVector, M_StripDirection);
	FVector Ground;
	GlobalAbilityHelpers::TryFindGroundLocation(this, M_StripStart + M_StripDirection * Along + Left * Lateral, Ground);
	const FTransform Transform(M_StripDirection.Rotation(), Ground + FVector(0.f, 0.f, VictimSpawnAboveGroundCm));
	return FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, UnitClass, Transform, OwningPlayer);
}

void USonnengewehrTestSubsystem::AddVictim(AActor* Actor, const FString& Label, const bool bIsTank,
                                           const bool bIsFriendly)
{
	if (not IsValid(Actor))
	{
		Check(false, TEXT("victim_spawned"), Label);
		return;
	}
	FVictim Victim;
	Victim.Actor = Actor;
	Victim.ActorKey = Actor;
	Victim.Label = Label;
	Victim.bIsTank = bIsTank;
	Victim.bIsFriendly = bIsFriendly;
	M_Victims.Add(Victim);
}

void USonnengewehrTestSubsystem::TickPlacingVictims()
{
	using namespace SonnengewehrTestPrivate;
	UpdateCamera();
	PacifyVictims();
	if (GetNow() - M_PhaseStartSeconds < PlaceVictimsSeconds)
	{
		return;
	}
	CollectSquadSoldiers();
	PacifyVictims();
	ClassifyVictims();
	RequestScreenshot(TEXT("Placed"));
	FireAbility();
}

void USonnengewehrTestSubsystem::CollectSquadSoldiers()
{
	using namespace SonnengewehrTestPrivate;
	for (int32 Index = 0; Index < M_Squads.Num(); ++Index)
	{
		ASquadController* Squad = M_Squads[Index].Get();
		if (not IsValid(Squad))
		{
			continue;
		}
		int32 SoldierIndex = 0;
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			if (not IsValid(Soldier))
			{
				continue;
			}
			// A squad spawns its soldiers after the owner override is restored; give them the squad's owner.
			const uint8 SquadOwner = M_SquadOwners.IsValidIndex(Index) ? M_SquadOwners[Index] : EnemyPlayer;
			FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, SquadOwner);
			AddVictim(Soldier, FString::Printf(TEXT("%s_soldier_%d"), *M_SquadLabels[Index], SoldierIndex++), false,
			          SquadOwner == TestPlayer);
		}
		Check(SoldierIndex > 0, FString::Printf(TEXT("%s_has_soldiers"), *M_SquadLabels[Index]),
		      FString::Printf(TEXT("soldiers=%d"), SoldierIndex));
	}
}

void USonnengewehrTestSubsystem::ClassifyVictims()
{
	using namespace SonnengewehrTestPrivate;
	// The beam half width is the template's aim AreaRadius; read it from a template copy before firing.
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	const UGlobalAbility* Template = IsValid(AbilitySet)
		                                 ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(
			                                 EGlobalAbility::GA_GerSonnengewehr)
		                                 : nullptr;
	const float HalfWidth = IsValid(Template) ? Template->GetAimSettings().AreaAim.AreaRadius : 0.f;
	const FVector Left = FVector::CrossProduct(FVector::UpVector, M_StripDirection);
	for (FVictim& Victim : M_Victims)
	{
		const AActor* Actor = Victim.Actor.Get();
		if (not IsValid(Actor))
		{
			continue;
		}
		const FVector Offset = Actor->GetActorLocation() - M_StripStart;
		Victim.Along = FVector::DotProduct(Offset, M_StripDirection);
		Victim.Lateral = FVector::DotProduct(Offset, Left);
		const float SideDistance = FMath::Abs(Victim.Lateral);
		Victim.Lane = SideDistance <= HalfWidth - OnPathInsetCm
			              ? ESonnengewehrVictimLane::OnPath
			              : SideDistance >= HalfWidth + OffPathMarginCm
			              ? ESonnengewehrVictimLane::OffPath
			              : ESonnengewehrVictimLane::Edge;
		const UHealthComponent* Health = Actor->FindComponentByClass<UHealthComponent>();
		Victim.StartHealth = IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
		Victim.LastHealth = Victim.StartHealth;
		UE_LOG(LogRTSSonnengewehrTest, Display,
		       TEXT("RTS_SONNEN_TEST VICTIM %s class=%s lane=%s along=%.0f lateral=%.0f health=%.0f"), *Victim.Label,
		       *Actor->GetClass()->GetName(), *UEnum::GetValueAsString(Victim.Lane), Victim.Along, Victim.Lateral,
		       Victim.StartHealth);
	}
	Check(HalfWidth > 0.f, TEXT("template_beam_half_width"), FString::Printf(TEXT("half_width=%.0f"), HalfWidth));
}

void USonnengewehrTestSubsystem::FireAbility()
{
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
	UGlobalAbility* Copy = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(EGlobalAbility::GA_GerSonnengewehr);
	M_Ability = Cast<UGA_Sonnengewehr>(Copy);
	Check(IsValid(M_Ability), TEXT("template_is_sonnengewehr"), IsValid(Copy) ? Copy->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(M_Ability) || not CheckTemplate(*M_Ability))
	{
		Finish();
		return;
	}
	M_Ability->InitGlobalAbility(SonnengewehrTestPrivate::TestPlayer, Manager, Controller);
	TMap<uint8, int32> ResourcesBefore;
	if (not EnsureCanAfford(*M_Ability, ResourcesBefore))
	{
		Finish();
		return;
	}
	M_Ability->OnBeamSpawned.AddUObject(this, &USonnengewehrTestSubsystem::OnBeamSpawned);
	M_Ability->OnGlobalAbilityEnded.AddUObject(this, &USonnengewehrTestSubsystem::OnAbilityEnded);

	// The player's path: activate from the panel, click the start of the strip, then its end.
	M_RunStartSeconds = GetNow();
	Controller->OnGlobaAbilityActivated(M_Ability->GetAimSettings(), FGlobalAbilitySoundSettings(), M_Ability);
	Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(M_StripStart), TEXT("first_click_anchors_strip"));
	Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(M_StripEnd), TEXT("second_click_fires"));
	CheckPaymentAndCooldown(*M_Ability, ResourcesBefore);
	EnterPhase(ESonnengewehrTestPhase::Running);
}

bool USonnengewehrTestSubsystem::CheckTemplate(const UGA_Sonnengewehr& Ability)
{
	const FGlobalAbilityAreaAimSettings& AreaAim = Ability.GetAimSettings().AreaAim;
	const FGLobalAbilityCostState& Costs = Ability.GetAbilityCosts();
	const FSonnengewehrSettings& Settings = Ability.GetSonnengewehrSettings();
	Check(AreaAim.Shape == EGlobalAbilityAimShape::Strip && AreaAim.AreaRadius > 0.f
	      && AreaAim.MaxStripLength >= SonnengewehrTestPrivate::StripLengthCm, TEXT("template_strip_aim"),
	      FString::Printf(TEXT("half_width=%.0f min=%.0f max=%.0f material=%s"), AreaAim.AreaRadius,
	                      AreaAim.MinStripLength, AreaAim.MaxStripLength,
	                      IsValid(AreaAim.AreaMaterial) ? *AreaAim.AreaMaterial->GetName() : TEXT("none")));
	Check(Ability.GetAbilityTier() == EGlobalAbilityTier::TierIII, TEXT("template_tier_iii"),
	      UEnum::GetValueAsString(Ability.GetAbilityTier()));
	Check(Costs.CoolDownTime >= 180 && Costs.CoolDownTime <= 240 && not Costs.Costs.ResourceCosts.IsEmpty(),
	      TEXT("template_cost_and_cooldown"),
	      FString::Printf(TEXT("cooldown=%d resources=%d"), Costs.CoolDownTime, Costs.Costs.ResourceCosts.Num()));
	const int32 RadixiteCost = Costs.Costs.ResourceCosts.FindRef(ERTSResourceType::Resource_Radixite);
	Check(Costs.Costs.ResourceCosts.Num() == 1 && RadixiteCost == SonnengewehrTestPrivate::ExpectedRadixiteCost,
	      TEXT("template_costs_675_radixite_only"), FString::Printf(TEXT("radixite=%d resources=%d"), RadixiteCost,
		      Costs.Costs.ResourceCosts.Num()));
	Check(not Settings.bBurnOwnUnits, TEXT("template_spares_own_units"));
	Check(not Ability.GetUISettings().Title.IsEmpty() && Ability.GetUISettings().Title.ToString() != TEXT("Ability Title")
	      && IsValid(Ability.GetUISettings().AbilityIcon), TEXT("template_title_and_icon"),
	      Ability.GetUISettings().Title.ToString());
	UE_LOG(LogRTSSonnengewehrTest, Display,
	       TEXT("RTS_SONNEN_TEST SETTINGS warning=%.1f ignition=%.1f sweep=%.1f fade=%.1f damage=%.0f interval=%.2f"),
	       Settings.WarningSeconds, Settings.IgnitionSeconds, Settings.SweepSeconds, Settings.FadeSeconds,
	       Settings.BeamDamage, Settings.DamageIntervalSeconds);
	return AreaAim.AreaRadius > 0.f;
}

bool USonnengewehrTestSubsystem::EnsureCanAfford(const UGA_Sonnengewehr& Ability, TMap<uint8, int32>& OutBefore)
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

void USonnengewehrTestSubsystem::CheckPaymentAndCooldown(const UGA_Sonnengewehr& Ability,
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
	Check(Costs.CoolDownRemaining == Costs.CoolDownTime && Costs.CoolDownTime > 0, TEXT("cooldown_started"),
	      FString::Printf(TEXT("remaining=%d time=%d"), Costs.CoolDownRemaining, Costs.CoolDownTime));
}

void USonnengewehrTestSubsystem::OnBeamSpawned(UGlobalAbility* Ability, ASonnengewehrBeam* Beam)
{
	M_Beam = Beam;
	M_BeamSpawnSeconds = GetRunSeconds();
	if (IsValid(Beam))
	{
		Beam->OnActorBurnt.AddUObject(this, &USonnengewehrTestSubsystem::OnActorBurnt);
		UE_LOG(LogRTSSonnengewehrTest, Display,
		       TEXT("RTS_SONNEN_TEST BEAM_SPAWNED at=%.2fs start=%s length=%.0f half_width=%.0f"), M_BeamSpawnSeconds,
		       *Beam->GetStripStart().ToCompactString(), Beam->GetStripLength(), Beam->GetBeamHalfWidth());
	}
}

void USonnengewehrTestSubsystem::OnActorBurnt(ASonnengewehrBeam* Beam, AActor* Actor, const float Damage)
{
	++M_TotalBurns;
	M_LastBurnSeconds = GetRunSeconds();
	const float BeamDistance = IsValid(Beam) ? Beam->GetBeamDistance() : -1.f;
	FVictim* Victim = M_Victims.FindByPredicate([Actor](const FVictim& Candidate)
	{
		return Candidate.ActorKey == Actor;
	});
	UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST BURNT %s at=%.2fs beam=%.0f damage=%.1f phase=%s"),
	       Victim ? *Victim->Label : (IsValid(Actor) ? *Actor->GetName() : TEXT("none")), M_LastBurnSeconds,
	       BeamDistance, Damage, IsValid(Beam) ? *UEnum::GetValueAsString(Beam->GetPhase()) : TEXT("none"));
	if (Victim == nullptr)
	{
		return;
	}
	++Victim->BurnCount;
	if (Victim->BurntSeconds < 0.f)
	{
		Victim->BurntSeconds = M_LastBurnSeconds;
		Victim->BurntBeamDistance = BeamDistance;
		Victim->BurntDamage = Damage;
		// Pending kill is not freed before the next garbage collection, so the location is still readable here.
		Victim->BurntAlong = IsValid(Beam) && Actor != nullptr
			                     ? FVector::DotProduct(Actor->GetActorLocation() - Beam->GetStripStart(),
			                                           Beam->GetStripDirection())
			                     : Victim->Along;
	}
}

void USonnengewehrTestSubsystem::OnAbilityEnded(UGlobalAbility* Ability)
{
	++M_AbilityEndCount;
	M_AbilityEndSeconds = GetRunSeconds();
	M_BurnsAtAbilityEnd = M_TotalBurns;
	UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST ABILITY_ENDED at=%.2fs burns=%d"),
	       M_AbilityEndSeconds, M_TotalBurns);
}

void USonnengewehrTestSubsystem::TickRunning()
{
	using namespace SonnengewehrTestPrivate;
	UpdateCamera();
	PacifyVictims();
	PollVictimHealth();
	TrackBeamPhases();
	if (M_AbilityEndCount > 0)
	{
		EnterPhase(ESonnengewehrTestPhase::AfterSweep);
		return;
	}
	if (GetRunSeconds() >= RunTimeoutSeconds)
	{
		Check(false, TEXT("ability_ended_before_timeout"));
		RunFinalChecks();
		Finish();
	}
}

void USonnengewehrTestSubsystem::PollVictimHealth()
{
	using namespace SonnengewehrTestPrivate;
	for (FVictim& Victim : M_Victims)
	{
		AActor* Actor = Victim.Actor.Get();
		const bool bAlive = RTSFunctionLibrary::RTSIsValid(Actor);
		const UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		const float CurrentHealth = bAlive && IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
		if (Victim.HurtSeconds < 0.f && CurrentHealth < Victim.StartHealth - HealthDropEpsilon)
		{
			Victim.HurtSeconds = GetRunSeconds();
		}
		if (not bAlive && not Victim.bDied)
		{
			Victim.bDied = true;
			UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST DIED %s at=%.2fs"), *Victim.Label,
			       GetRunSeconds());
		}
		Victim.LastHealth = CurrentHealth;
	}
}

void USonnengewehrTestSubsystem::TrackBeamPhases()
{
	using namespace SonnengewehrTestPrivate;
	const ASonnengewehrBeam* Beam = M_Beam.Get();
	if (not IsValid(Beam))
	{
		return;
	}
	const ESonnengewehrPhase Phase = Beam->GetPhase();
	const float Progress = Beam->GetStripLength() > 0.f ? Beam->GetBeamDistance() / Beam->GetStripLength() : 0.f;
	if (not bM_TookWarningShot && Phase == ESonnengewehrPhase::Warning
		&& GetRunSeconds() - M_BeamSpawnSeconds >= WarningShotSeconds)
	{
		bM_TookWarningShot = true;
		RequestScreenshot(TEXT("Warning"));
	}
	if (M_SweepStartSeconds < 0.f && Phase == ESonnengewehrPhase::Sweeping)
	{
		M_SweepStartSeconds = GetRunSeconds();
		UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST SWEEP_STARTED at=%.2fs"), M_SweepStartSeconds);
	}
	if (not bM_TookIgnitionShot && M_SweepStartSeconds >= 0.f
		&& GetRunSeconds() - M_SweepStartSeconds >= IgnitionShotSeconds)
	{
		bM_TookIgnitionShot = true;
		RequestScreenshot(TEXT("SweepStart"));
	}
	if (not bM_TookMidShot && Phase == ESonnengewehrPhase::Sweeping && Progress >= MidSweepFraction)
	{
		bM_TookMidShot = true;
		RequestScreenshot(TEXT("SweepMid"));
	}
	if (not bM_TookLateShot && Phase == ESonnengewehrPhase::Sweeping && Progress >= LateSweepFraction)
	{
		bM_TookLateShot = true;
		RequestScreenshot(TEXT("SweepLate"));
	}
	if (M_SweepEndSeconds < 0.f && (Phase == ESonnengewehrPhase::Fading || Phase == ESonnengewehrPhase::Spent))
	{
		M_SweepEndSeconds = GetRunSeconds();
		UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST SWEEP_ENDED at=%.2fs"), M_SweepEndSeconds);
	}
}

void USonnengewehrTestSubsystem::TickAfterSweep()
{
	using namespace SonnengewehrTestPrivate;
	UpdateCamera();
	PacifyVictims();
	PollVictimHealth();
	TrackBeamPhases();
	const float SecondsAfterSweep = GetNow() - M_PhaseStartSeconds;
	if (not bM_TookGlowingScorchShot && SecondsAfterSweep >= GlowingScorchShotSeconds)
	{
		bM_TookGlowingScorchShot = true;
		RequestScreenshot(TEXT("ScorchGlowing"));
	}
	if (SecondsAfterSweep < AfterSweepSeconds)
	{
		return;
	}
	RequestScreenshot(TEXT("ScorchCooled"));
	RunFinalChecks();
	Finish();
}

void USonnengewehrTestSubsystem::RunFinalChecks()
{
	const ASonnengewehrBeam* Beam = M_Beam.Get();
	Check(IsValid(Beam), TEXT("beam_spawned"), FString::Printf(TEXT("at=%.2fs"), M_BeamSpawnSeconds));
	CheckVictimLanes();
	CheckSweepOrder();
	CheckTiming();
	Check(M_AbilityEndCount == 1, TEXT("ability_ended_once"), FString::Printf(TEXT("count=%d"), M_AbilityEndCount));
	Check(M_TotalBurns == M_BurnsAtAbilityEnd, TEXT("no_burns_after_end"),
	      FString::Printf(TEXT("at_end=%d total=%d"), M_BurnsAtAbilityEnd, M_TotalBurns));
	if (IsValid(Beam))
	{
		// The scorch mark stays behind while the beam itself is gone.
		Check(Beam->GetPhase() == ESonnengewehrPhase::Spent, TEXT("beam_spent_scorch_remains"),
		      UEnum::GetValueAsString(Beam->GetPhase()));
	}
	if (IsValid(M_Ability))
	{
		M_Ability->OnBeamSpawned.RemoveAll(this);
		M_Ability->OnGlobalAbilityEnded.RemoveAll(this);
	}
}

void USonnengewehrTestSubsystem::CheckVictimLanes()
{
	int32 OnPath = 0;
	int32 OffPath = 0;
	int32 FriendlyOnPath = 0;
	for (const FVictim& Victim : M_Victims)
	{
		const FString Details = FString::Printf(
			TEXT("lane=%s along=%.0f lateral=%.0f health=%.0f->%.0f burnt_at=%.2fs beam=%.0f damage=%.1f burns=%d died=%d"),
			*UEnum::GetValueAsString(Victim.Lane), Victim.Along, Victim.Lateral, Victim.StartHealth, Victim.LastHealth,
			Victim.BurntSeconds, Victim.BurntBeamDistance, Victim.BurntDamage, Victim.BurnCount, Victim.bDied);
		if (Victim.bIsFriendly)
		{
			// Wherever it stood: the beam never burns its own side, so a friendly unit is untouched and alive.
			FriendlyOnPath += Victim.Lane == ESonnengewehrVictimLane::OnPath ? 1 : 0;
			Check(Victim.BurnCount == 0 && Victim.HurtSeconds < 0.f && not Victim.bDied,
			      Victim.Label + TEXT("_spared"), Details);
			continue;
		}
		switch (Victim.Lane)
		{
		case ESonnengewehrVictimLane::OnPath:
			++OnPath;
			Check(Victim.BurnCount == 1 && Victim.HurtSeconds >= 0.f, Victim.Label + TEXT("_burnt_once"), Details);
			Check(Victim.bDied, Victim.Label + TEXT("_died"), Details);
			break;
		case ESonnengewehrVictimLane::OffPath:
			++OffPath;
			Check(Victim.BurnCount == 0 && Victim.HurtSeconds < 0.f && not Victim.bDied,
			      Victim.Label + TEXT("_untouched"), Details);
			break;
		case ESonnengewehrVictimLane::Edge:
			UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST EDGE %s %s"), *Victim.Label, *Details);
			break;
		}
	}
	Check(OnPath >= 4 && OffPath >= 4, TEXT("enough_victims_each_side"),
	      FString::Printf(TEXT("on=%d off=%d"), OnPath, OffPath));
	Check(FriendlyOnPath >= SonnengewehrTestPrivate::MinFriendlyOnPath, TEXT("friendly_units_on_the_path"),
	      FString::Printf(TEXT("friendly_on=%d"), FriendlyOnPath));
}

void USonnengewehrTestSubsystem::CheckSweepOrder()
{
	using namespace SonnengewehrTestPrivate;
	const ASonnengewehrBeam* Beam = M_Beam.Get();
	const float HalfWidth = IsValid(Beam) ? Beam->GetBeamHalfWidth() : 0.f;
	TArray<const FVictim*> Burnt;
	for (const FVictim& Victim : M_Victims)
	{
		if (Victim.BurntSeconds < 0.f)
		{
			continue;
		}
		Burnt.Add(&Victim);
		// The beam must have been at the victim when it burnt: its edge reached the victim's hull, its centre had not
		// passed it by more than the half width.
		const float Reach = Victim.bIsTank ? VehicleReachCm : SoldierReachCm;
		const bool bBeamAtVictim = Victim.BurntBeamDistance >= Victim.BurntAlong - HalfWidth - Reach - 1.f
			&& Victim.BurntBeamDistance <= Victim.BurntAlong + HalfWidth + 1.f;
		Check(bBeamAtVictim, Victim.Label + TEXT("_burnt_when_beam_arrived"),
		      FString::Printf(TEXT("along_at_click=%.0f along_at_burn=%.0f beam=%.0f half_width=%.0f"), Victim.Along,
		                      Victim.BurntAlong, Victim.BurntBeamDistance, HalfWidth));
	}
	Burnt.Sort([](const FVictim& Left, const FVictim& Right)
	{
		return Left.BurntAlong < Right.BurntAlong;
	});
	// Units further along the strip burn later; two units at the same spot can burn in the same tick.
	bool bInOrder = true;
	FString Order;
	for (int32 Index = 0; Index < Burnt.Num(); ++Index)
	{
		Order += FString::Printf(TEXT("%s@%.0f=%.2fs "), *Burnt[Index]->Label, Burnt[Index]->BurntAlong,
		                         Burnt[Index]->BurntSeconds);
		if (Index > 0 && Burnt[Index]->BurntSeconds + TimingToleranceSeconds < Burnt[Index - 1]->BurntSeconds)
		{
			bInOrder = false;
		}
	}
	Check(bInOrder && Burnt.Num() > 0, TEXT("burnt_in_sweep_order"), Order);
}

void USonnengewehrTestSubsystem::CheckTiming()
{
	using namespace SonnengewehrTestPrivate;
	if (not IsValid(M_Ability))
	{
		return;
	}
	const FSonnengewehrSettings& Settings = M_Ability->GetSonnengewehrSettings();
	float FirstBurn = TNumericLimits<float>::Max();
	for (const FVictim& Victim : M_Victims)
	{
		if (Victim.BurntSeconds >= 0.f)
		{
			FirstBurn = FMath::Min(FirstBurn, Victim.BurntSeconds);
		}
	}
	const float Ignition = M_BeamSpawnSeconds + Settings.WarningSeconds;
	Check(M_BeamSpawnSeconds >= 0.f && FirstBurn >= Ignition - TimingToleranceSeconds, TEXT("nothing_burns_in_warning"),
	      FString::Printf(TEXT("beam=%.2fs ignition=%.2fs first_burn=%.2fs"), M_BeamSpawnSeconds, Ignition, FirstBurn));
	const float ExpectedSweepEnd = Ignition + Settings.IgnitionSeconds + Settings.SweepSeconds;
	Check(FMath::Abs(M_SweepEndSeconds - ExpectedSweepEnd) <= TimingToleranceSeconds, TEXT("sweep_took_its_time"),
	      FString::Printf(TEXT("sweep_start=%.2fs sweep_end=%.2fs expected_end=%.2fs"), M_SweepStartSeconds,
	                      M_SweepEndSeconds, ExpectedSweepEnd));
	Check(M_AbilityEndSeconds >= M_LastBurnSeconds && M_AbilityEndSeconds >= M_SweepEndSeconds - TimingToleranceSeconds,
	      TEXT("ability_ended_after_last_burn"),
	      FString::Printf(TEXT("ended=%.2fs last_burn=%.2fs sweep_end=%.2fs"), M_AbilityEndSeconds, M_LastBurnSeconds,
	                      M_SweepEndSeconds));
}

void USonnengewehrTestSubsystem::UpdateCamera() const
{
	using namespace SonnengewehrTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const ASonnengewehrBeam* Beam = M_Beam.Get();
	const bool bFollowBeam = IsValid(Beam) && M_Phase == ESonnengewehrTestPhase::Running
		&& (Beam->GetPhase() == ESonnengewehrPhase::Igniting || Beam->GetPhase() == ESonnengewehrPhase::Sweeping);
	const FVector Focus = bFollowBeam ? Beam->GetBeamGroundLocation() : (M_StripStart + M_StripEnd) * 0.5f;
	const float Yaw = M_StripDirection.Rotation().Yaw + CameraYawOffset;
	const FRotator ViewRotation(bFollowBeam ? FollowPitch : OverviewPitch, Yaw, 0.f);
	const float Distance = bFollowBeam ? FollowDistance : OverviewDistance;
	M_Camera->SetActorLocationAndRotation(Focus - ViewRotation.Vector() * Distance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void USonnengewehrTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("SonnengewehrTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSSonnengewehrTest, Display, TEXT("RTS_SONNEN_TEST SCREENSHOT %s at=%.2fs"), *FileName, GetRunSeconds());
}

#else

bool USonnengewehrTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void USonnengewehrTestSubsystem::Tick(float DeltaTime) {}
TStatId USonnengewehrTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USonnengewehrTestSubsystem, STATGROUP_Tickables);
}

#endif
