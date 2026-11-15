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
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
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

bool UMissionMapTestLibrary::MapTest_HasAbility(AActor* Unit, const EAbilityID Ability)
{
	ICommands* Commands = Cast<ICommands>(Unit);
	return Commands != nullptr && Commands->HasAbility(Ability);
}
