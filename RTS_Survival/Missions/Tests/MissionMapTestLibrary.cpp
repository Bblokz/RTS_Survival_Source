// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionMapTestLibrary.h"

#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingLayout/TrainingLayoutTypes.h"
#include "EngineUtils.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/MissionClasses/MoveCamera/MoveCameraMission.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

int32 UMissionMapTestLibrary::MapTest_ClickTrainingOption(ANomadicVehicle* Trainer, const FTrainingOption Option)
{
#if RTS_WITH_MAP_TESTS
	constexpr int32 NotOnMenu = 0;
	constexpr int32 Queued = 1;
	constexpr int32 LockedByMission = 2;
	constexpr int32 NotQueued = 3;
	if (not IsValid(Trainer))
	{
		return NotOnMenu;
	}
	UTrainingMenuManager* TrainingMenuManager = FRTS_Statics::GetTrainingMenuManager(Trainer);
	UTrainerComponent* TrainerComponent = Trainer->GetTrainerComponent();
	if (not IsValid(TrainingMenuManager) || not IsValid(TrainerComponent))
	{
		return NotOnMenu;
	}
	const bool bOnMenu = TrainerComponent->GetTrainingSubSections().ContainsByPredicate(
		[&Option](const FTrainingSubSectionState& SubSection) { return SubSection.Options.Contains(Option); });
	if (not bOnMenu)
	{
		return NotOnMenu;
	}
	bool bLockedByMission = false;
	if (TrainingMenuManager->MapTest_ClickTrainingOption(TrainerComponent, Option, bLockedByMission))
	{
		return Queued;
	}
	return bLockedByMission ? LockedByMission : NotQueued;
#else
	return 0;
#endif
}

bool UMissionMapTestLibrary::MapTest_StartBxpPlacement(ANomadicVehicle* ExpansionOwner,
                                                       const EBuildingExpansionType ExpansionType)
{
#if RTS_WITH_MAP_TESTS
	if (not IsValid(ExpansionOwner))
	{
		return false;
	}
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(ExpansionOwner);
	if (not IsValid(PlayerController))
	{
		return false;
	}
	const TArray<FBxpOptionData> Options = ExpansionOwner->GetUnlockedBuildingExpansionTypes();
	const FBxpOptionData* ExpansionOption = Options.FindByPredicate([ExpansionType](const FBxpOptionData& Option)
	{
		return Option.ExpansionType == ExpansionType;
	});
	const TArray<FBuildingExpansionItem>& Slots = ExpansionOwner->GetBuildingExpansions();
	const int32 SlotIndex = Slots.IndexOfByPredicate([](const FBuildingExpansionItem& Item)
	{
		return Item.Expansion == nullptr && Item.ExpansionStatus == EBuildingExpansionStatus::BXS_UnlockedButNotBuild;
	});
	if (ExpansionOption == nullptr || SlotIndex == INDEX_NONE)
	{
		return false;
	}
	TScriptInterface<IBuildingExpansionOwner> Owner;
	Owner.SetObject(ExpansionOwner);
	Owner.SetInterface(static_cast<IBuildingExpansionOwner*>(ExpansionOwner));
	// What the option button does when the player clicks it.
	PlayerController->ExpandBuildingWithBxpAndStartPreview(*ExpansionOption, Owner, SlotIndex, false);
	return true;
#else
	return false;
#endif
}

bool UMissionMapTestLibrary::MapTest_IsBxpWaitingForPlacement(const UObject* WorldContextObject)
{
#if RTS_WITH_MAP_TESTS
	const ACPPController* PlayerController = FRTS_Statics::GetRTSController(WorldContextObject);
	return IsValid(PlayerController) && PlayerController->MapTest_GetIsBxpWaitingForPlacement();
#else
	return false;
#endif
}

bool UMissionMapTestLibrary::MapTest_PlaceLoadedBxp(const UObject* WorldContextObject, const FVector ClickLocation)
{
#if RTS_WITH_MAP_TESTS
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(WorldContextObject);
	if (not IsValid(PlayerController))
	{
		return false;
	}
	FVector ClickedLocation = ClickLocation;
	return PlayerController->MapTest_PlaceLoadedBxpAtLocation(ClickedLocation);
#else
	return false;
#endif
}

int32 UMissionMapTestLibrary::MapTest_ConvertNomadicAt(ANomadicVehicle* NomadicTruck, const FVector Location)
{
#if RTS_WITH_MAP_TESTS
	constexpr int32 NeededBuildRadiusFlag = 10;
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(NomadicTruck);
	if (not IsValid(PlayerController))
	{
		return 0;
	}
	bool bNeededBuildRadius = false;
	const int32 Result = PlayerController->MapTest_ConvertNomadicAt(NomadicTruck, Location, bNeededBuildRadius);
	return bNeededBuildRadius ? Result + NeededBuildRadiusFlag : Result;
#else
	return 0;
#endif
}

namespace MissionMapTestLocationIntro
{
	// The throwaway mission of the last MapTest_StartLocationIntro; the mission manager keeps it alive.
	TWeakObjectPtr<UMissionBase> TestMission;
}

bool UMissionMapTestLibrary::MapTest_StartLocationIntro(const UObject* WorldContextObject,
                                                       const FLocationIntroTexts Texts,
                                                       const ELocationIntroDriftAxis DriftAxis,
                                                       const float DriftDistance,
                                                       const float DurationSeconds,
                                                       const bool bPanCamera,
                                                       const FLocationIntroPanSettings PanSettings,
                                                       const FLocationIntroPresentation Presentation,
                                                       const bool bCompleteMissionRightAway)
{
#if RTS_WITH_MAP_TESTS
	UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return false;
	}
	TActorIterator<AMissionManager> ManagerIterator(World);
	AMissionManager* const MissionManager = ManagerIterator ? *ManagerIterator : nullptr;
	if (not IsValid(MissionManager))
	{
		return false;
	}
	UMoveCameraMission* const Mission = NewObject<UMoveCameraMission>(MissionManager);
	MissionManager->ActivateNewMission(Mission);
	MissionMapTestLocationIntro::TestMission = Mission;
	constexpr int32 TestIntroId = 1;
	const bool bAccepted = Mission->ShowLocationIntro(Texts.Location, Texts.Date, DriftAxis, DriftDistance,
	                                                  DurationSeconds, bPanCamera, PanSettings, TestIntroId,
	                                                  Texts.LocationSubtitle, Texts.TimeOfDay, Presentation);
	if (bCompleteMissionRightAway)
	{
		Mission->NextMission = NewObject<UMoveCameraMission>(MissionManager);
		Mission->OnMissionComplete();
	}
	return bAccepted;
#else
	return false;
#endif
}

void UMissionMapTestLibrary::MapTest_GetLocationIntroState(const UObject* WorldContextObject,
                                                          bool& bOutIntroPlaying,
                                                          bool& bOutScreenHandedBack,
                                                          FVector& OutCameraLocation,
                                                          FRotator& OutCameraRotation)
{
	bOutIntroPlaying = false;
	bOutScreenHandedBack = false;
	OutCameraLocation = FVector::ZeroVector;
	OutCameraRotation = FRotator::ZeroRotator;
#if RTS_WITH_MAP_TESTS
	if (MissionMapTestLocationIntro::TestMission.IsValid())
	{
		bOutIntroPlaying = MissionMapTestLocationIntro::TestMission->GetIsLocationIntroPlaying();
	}
	const ACPPController* const PlayerController = FRTS_Statics::GetRTSController(WorldContextObject);
	bOutScreenHandedBack = IsValid(PlayerController) && PlayerController->GetCanStartScreenTakeover();
	const UPlayerCameraController* const CameraController = FRTS_Statics::GetPlayerCameraController(
		WorldContextObject);
	if (IsValid(CameraController))
	{
		CameraController->GetCameraPawnPose(OutCameraLocation, OutCameraRotation);
	}
#endif
}

AActor* UMissionMapTestLibrary::MapTest_SpawnActor(const UObject* WorldContextObject,
                                                   const TSubclassOf<AActor> ActorClass, const FVector Location,
                                                   const FRotator Rotation)
{
	UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr || ActorClass == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return World->SpawnActor<AActor>(ActorClass, Location, Rotation, Params);
}

bool UMissionMapTestLibrary::MapTest_HasAbility(AActor* Unit, const EAbilityID Ability)
{
	ICommands* Commands = Cast<ICommands>(Unit);
	return Commands != nullptr && Commands->HasAbility(Ability);
}

int32 UMissionMapTestLibrary::MapTest_HoldTankFire(AActor* Tank)
{
	int32 StoppedWeapons = 0;
#if RTS_WITH_MAP_TESTS
	const ATankMaster* const TankMaster = Cast<ATankMaster>(Tank);
	if (not IsValid(TankMaster))
	{
		return 0;
	}
	for (ACPPTurretsMaster* const Turret : TankMaster->GetTurrets())
	{
		if (IsValid(Turret))
		{
			Turret->DisableTurret();
			++StoppedWeapons;
		}
	}
	for (UHullWeaponComponent* const HullWeapon : TankMaster->GetHullWeapons())
	{
		if (IsValid(HullWeapon))
		{
			HullWeapon->DisableHullWeapon();
			++StoppedWeapons;
		}
	}
#endif
	return StoppedWeapons;
}

int32 UMissionMapTestLibrary::MapTest_PlaceSquadSoldiers(AActor* Squad, const TArray<FVector>& GroundSpots)
{
	int32 PlacedSoldiers = 0;
#if RTS_WITH_MAP_TESTS
	ASquadController* const SquadController = Cast<ASquadController>(Squad);
	const URTSComponent* const RTSComponent = IsValid(SquadController)
		                                          ? SquadController->FindComponentByClass<URTSComponent>()
		                                          : nullptr;
	if (RTSComponent == nullptr)
	{
		return 0;
	}
	const uint8 SquadOwner = static_cast<uint8>(RTSComponent->GetOwningPlayer());
	for (ASquadUnit* const Soldier : SquadController->GetSquadUnitsChecked())
	{
		if (not IsValid(Soldier) || not GroundSpots.IsValidIndex(PlacedSoldiers))
		{
			continue;
		}
		FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, SquadOwner);
		FVector Spot = GroundSpots[PlacedSoldiers];
		Spot.Z += Soldier->GetSimpleCollisionHalfHeight() + 2.f;
		Soldier->Force_TerminateMovement();
		Soldier->SetActorLocation(Spot, false, nullptr, ETeleportType::TeleportPhysics);
		++PlacedSoldiers;
	}
#endif
	return PlacedSoldiers;
}

TArray<UMissionBase*> UMissionMapTestLibrary::MapTest_GetActiveMissions(const UObject* WorldContextObject)
{
	TArray<UMissionBase*> ActiveMissions;
#if RTS_WITH_MAP_TESTS
	UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return ActiveMissions;
	}
	for (TActorIterator<AMissionManager> It(World); It; ++It)
	{
		for (UMissionBase* const Mission : It->GetActiveMissions())
		{
			if (IsValid(Mission))
			{
				ActiveMissions.Add(Mission);
			}
		}
	}
#endif
	return ActiveMissions;
}

bool UMissionMapTestLibrary::MapTest_GetIsMissionWaitingForTrigger(const UMissionBase* Mission)
{
#if RTS_WITH_MAP_TESTS
	return IsValid(Mission) && Mission->MissionState.bIsMissionStarted && not Mission->MissionState.bIsTriggered
		&& not Mission->MissionState.bIsMissionComplete;
#else
	return false;
#endif
}

bool UMissionMapTestLibrary::MapTest_GetIsMissionRunning(const UMissionBase* Mission)
{
#if RTS_WITH_MAP_TESTS
	return IsValid(Mission) && Mission->MissionState.bIsMissionStarted && Mission->MissionState.bIsTriggered
		&& not Mission->MissionState.bIsMissionComplete;
#else
	return false;
#endif
}

bool UMissionMapTestLibrary::MapTest_FireMissionTrigger(UMissionBase* Mission)
{
#if RTS_WITH_MAP_TESTS
	if (not MapTest_GetIsMissionWaitingForTrigger(Mission))
	{
		return false;
	}
	Mission->OnTriggerActivated();
	return true;
#else
	return false;
#endif
}

namespace MissionMapTestLibraryPrivate
{
	UGlobalAbilitiesManager* GetPlayerGlobalAbilitiesManager(const UObject* WorldContextObject)
	{
		ACPPController* const PlayerController = FRTS_Statics::GetRTSController(WorldContextObject);
		return IsValid(PlayerController) ? PlayerController->GetGlobalAbilitiesManager() : nullptr;
	}
}

bool UMissionMapTestLibrary::MapTest_UsePlayerGlobalAbility(const UObject* WorldContextObject,
                                                            const EGlobalAbility Ability, const FVector Location)
{
#if RTS_WITH_MAP_TESTS
	const UGlobalAbilitiesManager* const Manager =
		MissionMapTestLibraryPrivate::GetPlayerGlobalAbilitiesManager(WorldContextObject);
	UGlobalAbility* const LoadedAbility = IsValid(Manager) ? Manager->FindLoadedAbilityByType(Ability) : nullptr;
	if (not IsValid(LoadedAbility))
	{
		return false;
	}
	// A used ability always starts its cooldown or loses a charge: compare to tell a blocked click apart.
	const int32 CooldownBefore = LoadedAbility->GetAbilityCosts().CoolDownRemaining;
	const int32 ChargesBefore = LoadedAbility->GetRemainingCharges();
	LoadedAbility->OnClickedAbilityLocation(Location);
	return LoadedAbility->GetAbilityCosts().CoolDownRemaining != CooldownBefore
		|| LoadedAbility->GetRemainingCharges() != ChargesBefore;
#else
	return false;
#endif
}

void UMissionMapTestLibrary::MapTest_GetPlayerGlobalAbilityState(const UObject* WorldContextObject,
                                                                 const EGlobalAbility Ability, bool& bOutLoaded,
                                                                 bool& bOutPanelVisible, int32& OutLoadedCount,
                                                                 int32& OutCharges, int32& OutCooldownTime,
                                                                 int32& OutCooldownRemaining,
                                                                 int32& OutResourceCostCount,
                                                                 int32& OutStrafeAircraft)
{
	bOutLoaded = false;
	bOutPanelVisible = false;
	OutLoadedCount = 0;
	OutCharges = INDEX_NONE;
	OutCooldownTime = 0;
	OutCooldownRemaining = 0;
	OutResourceCostCount = 0;
	OutStrafeAircraft = 0;
#if RTS_WITH_MAP_TESTS
	const UGlobalAbilitiesManager* const Manager =
		MissionMapTestLibraryPrivate::GetPlayerGlobalAbilitiesManager(WorldContextObject);
	if (not IsValid(Manager))
	{
		return;
	}
	bOutPanelVisible = Manager->GetIsAbilityPanelVisible();
	OutLoadedCount = Manager->GetLoadedAbilityCount();
	const UGlobalAbility* const LoadedAbility = Manager->FindLoadedAbilityByType(Ability);
	if (not IsValid(LoadedAbility))
	{
		return;
	}
	bOutLoaded = true;
	OutCharges = LoadedAbility->GetRemainingCharges();
	OutCooldownTime = LoadedAbility->GetAbilityCosts().CoolDownTime;
	OutCooldownRemaining = LoadedAbility->GetAbilityCosts().CoolDownRemaining;
	OutResourceCostCount = LoadedAbility->GetAbilityCosts().Costs.ResourceCosts.Num();
	if (const UGA_AircraftStrafing* const Strafing = Cast<UGA_AircraftStrafing>(LoadedAbility))
	{
		OutStrafeAircraft = Strafing->GetAircraftCount();
	}
#endif
}
