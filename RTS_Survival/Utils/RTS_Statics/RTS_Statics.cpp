#include "RTS_Statics.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionManager.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/WorldSubSystem/AnimatedTextWorldSubsystem.h"
#include "RTS_Survival/LandscapeDeformSystem/LandscapeDeformManager/LandscapeDeformManager.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/Camera/CameraPawn.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Player/PlayerAudioController/PlayerAudioController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/Procedural/LandscapeDivider/RTSLandscapeDivider.h"
#include "RTS_Survival/StartLocation/PlayerStartLocationMngr/PlayerStartLocationManager.h"
#include "RTS_Survival/Utils/Navigator/RTSNavigator.h"
#include "SubSystems/EnemyControllerSubsystem/EnemyControllerSubsystem.h"
#include "SubSystems/ExplosionManagerSubsystem/ExplosionManagerSubsystem.h"

FRTS_Statics::FRTS_Statics()
{
}

FRTS_Statics::~FRTS_Statics()
{
}

UAnimatedTextWidgetPoolManager* FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(
	const UObject* WorldContextObject)
{
	if (UWorld* World = WorldContextObject->GetWorld())
	{
		UAnimatedTextWorldSubsystem* WorldSubsystem = World->GetSubsystem<UAnimatedTextWorldSubsystem>();
		if (WorldSubsystem && WorldSubsystem->GetAnimatedTextWidgetPoolManager())
		{
			return WorldSubsystem->GetAnimatedTextWidgetPoolManager();
		}
	}
	return nullptr;
}

ACPPController* FRTS_Statics::GetRTSController(const UObject* WorldContextObject)
{
	if (GEngine)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
		{
			if (ACPPController* RTSController = Cast<ACPPController>(World->GetFirstPlayerController()))
			{
				if (IsValid(RTSController))
				{
					return RTSController;
				}
				return nullptr;
			}
		}
	}
	return nullptr;
}

bool FRTS_Statics::IsShiftPressed(const UObject* WorldContextObject)
{
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		return RTSController->GetIsShiftPressed();
	}
	return false;
}

bool FRTS_Statics::IsControlPressed(const UObject* WorldContextObject)
{
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		return RTSController->GetIsControlPressed();
	}
	return false;
}

UMainGameUI* FRTS_Statics::GetMainGameUI(const UObject* WorldContextObject)
{
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		UMainGameUI* MainGameUI = RTSController->GetMainMenuUI();
		if (IsValid(MainGameUI))
		{
			return MainGameUI;
		}
	}
	return nullptr;
}

UTrainingMenuManager* FRTS_Statics::GetTrainingMenuManager(const UObject* WorldContextObject)
{
	if (UMainGameUI* MainGameUI = GetMainGameUI(WorldContextObject))
	{
		UTrainingMenuManager* TrainingMenuManager = MainGameUI->GetTrainingMenuManager();
		if (IsValid(TrainingMenuManager))
		{
			return TrainingMenuManager;
		}
	}
	return nullptr;
}

UPlayerResourceManager* FRTS_Statics::GetPlayerResourceManager(const UObject* WorldContextObject)
{
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		UPlayerResourceManager* ResourceManager = RTSController->GetPlayerResourceManager();
		if (IsValid(ResourceManager))
		{
			return ResourceManager;
		}
	}
	return nullptr;
}

ARTSAsyncSpawner* FRTS_Statics::GetAsyncSpawner(const UObject* WorldContextObject)
{
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		ARTSAsyncSpawner* AsyncSpawner = RTSController->GetRTSAsyncSpawner();
		if (IsValid(AsyncSpawner))
		{
			return AsyncSpawner;
		}
	}
	return nullptr;
}

UPlayerTechManager* FRTS_Statics::GetPlayerTechManager(const UObject* WorldContextObject)
{
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		UPlayerTechManager* PlayerTechManager = RTSController->GetPlayerTechManager();
		if (IsValid(PlayerTechManager))
		{
			return PlayerTechManager;
		}
	}
	return nullptr;
}

ARTSNavigator* FRTS_Statics::GetRTSNavigator(const UObject* WorldContextObject)
{
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		ARTSNavigator* RTSNavigator = RTSController->GetRTSNavigator();
		if (IsValid(RTSNavigator))
		{
			return RTSNavigator;
		}
	}
	return nullptr;
}

ACameraPawn* FRTS_Statics::GetPlayerCameraPawn(const UObject* WorldContextObject)
{
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, 0))
	{
		if (ACameraPawn* PlayerCameraPawn = Cast<ACameraPawn>(Pawn))
		{
			return PlayerCameraPawn;
		}
	}
	RTSFunctionLibrary::ReportError("Could not get player camera pawn for actor: " + WorldContextObject->GetName() +
		"\n See FRTS_Statics::GetPlayerCameraPawn");

	return nullptr;
}

ACPPGameState* FRTS_Statics::GetGameState(const UObject* WorldContextObject)
{
	AGameStateBase* GameState = UGameplayStatics::GetGameState(WorldContextObject);
	if (IsValid(GameState))
	{
		ACPPGameState* RTSGameState = Cast<ACPPGameState>(GameState);
		if (IsValid(RTSGameState))
		{
			return RTSGameState;
		}
	}
	return nullptr;
}

URTSGameInstance* FRTS_Statics::GetRTSGameInstance(const UObject* WorldContextObject)
{
	URTSGameInstance* GameInstance = Cast<URTSGameInstance>(
		UGameplayStatics::GetGameInstance(WorldContextObject));
	if( IsValid(GameInstance))
	{
		return GameInstance;
	}
	return nullptr;
}

UGameExplosionsManager* FRTS_Statics::GetGameExplosionsManager(const UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject); UWorld* World = WorldContextObject->GetWorld())
	{
		UExplosionManagerSubsystem* EMS = World->GetSubsystem<UExplosionManagerSubsystem>();
		// Retrieve the manager pointer
		UGameExplosionsManager* ExploMgr = EMS->GetExplosionManager();
		if (IsValid(ExploMgr))
		{
			return ExploMgr;
		}
	}
	return nullptr;
}

AEnemyController* FRTS_Statics::GetEnemyController(const UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject); UWorld* World = WorldContextObject->GetWorld())
	{
		UEnemyControllerSubsystem* EMS = World->GetSubsystem<UEnemyControllerSubsystem>();
		// Retrieve the manager pointer
		AEnemyController* EnemyController = EMS->GetEnemyController();
		if (IsValid(EnemyController))
		{
			return EnemyController;
		}
	}
	return nullptr;
}

UGameUnitManager* FRTS_Statics::GetGameUnitManager(const UObject* WorldContextObject)
{
	if (ACPPGameState* GameState = GetGameState(WorldContextObject))
	{
		UGameUnitManager* GameUnitManager = GameState->GetGameUnitManager();
		if (IsValid(GameUnitManager))
		{
			return GameUnitManager;
		}
	}
	return nullptr;
}

URTSGameSettingsHandler* FRTS_Statics::GetRTSGameUpdate(const UObject* WorldContextObject)
{
	if (ACPPGameState* GameState = GetGameState(WorldContextObject))
	{
		URTSGameSettingsHandler* GameUpdate = GetRTSGameUpdate(GameState);
		if (IsValid(GameUpdate))
		{
			return GameUpdate;
		}
	}
	return nullptr;
}

ANomadicVehicle* FRTS_Statics::GetPlayerHq(const UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (ACPPController* RTSController = GetRTSController(WorldContextObject); IsValid(RTSController))
		{
			if (ANomadicVehicle* PlayerHq = RTSController->GetPlayerHq(); PlayerHq)
			{
				return PlayerHq;
			}
		}
	}
	return nullptr;
}

UPlayerPortraitManager* FRTS_Statics::GetPlayerPortraitManager(const UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (ACPPController* RTSController = GetRTSController(WorldContextObject))
		{
			UPlayerPortraitManager* PortraitManager = RTSController->GetPlayerPortraitManager();
			if (IsValid(PortraitManager))
			{
				return PortraitManager;
			}
		}
	}
	return nullptr;
}

AFowManager* FRTS_Statics::GetFowManager(const UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (const ACPPController* RTSController = GetRTSController(WorldContextObject))
		{
			AFowManager* FowManager = RTSController->GetFowManager();
			if (IsValid(FowManager))
			{
				return FowManager;
			}
		}
		// In case the controller has not yet had begin play we can attempt to find the fow manager on the map.
		if (AFowManager* FowManager = Cast<AFowManager>(
			UGameplayStatics::GetActorOfClass(WorldContextObject, AFowManager::StaticClass())))
		{
			if (IsValid(FowManager))
			{
				return FowManager;
			}
		}
	}
	return nullptr;
}

ALandscapedeformManager* FRTS_Statics::GetLandscapeDeformManager(const UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (ACPPController* RTSController = GetRTSController(WorldContextObject))
		{
			ALandscapedeformManager* LandscapeDeformManager = RTSController->GetLandscapeDeformManager();
			if (IsValid(LandscapeDeformManager))
			{
				return LandscapeDeformManager;
			}
		}
	}
	return nullptr;
}

UPlayerBuildRadiusManager* FRTS_Statics::GetPlayerBuildRadiusManager(const UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		if (const ACPPController* PlayerController = GetRTSController(WorldContextObject))
		{
			return PlayerController->GetPlayerBuildRadiusManager();
		}
	}
	return nullptr;
}

UPlayerControlGroupManager* FRTS_Statics::GetPlayerControlGroupManager(const UObject* WorldContextObject)
{
	if (IsValid(WorldContextObject))
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
		if (IsValid(PlayerController))
		{
			if (ACPPController* RTSController = Cast<ACPPController>(PlayerController))
			{
				return RTSController->GetPlayerControlGroupManager();
			}
		}
	}
	return nullptr;
}

ARTSLandscapeDivider* FRTS_Statics::GetRTSLandscapeDivider(const TObjectPtr<UObject> WorldContextObject)
{
	if (not IsValid(WorldContextObject))
	{
		return nullptr;
	}
	// get all actors on the map of this class.
	TArray<AActor*> LandscapeDividers;
	UGameplayStatics::GetAllActorsOfClass(WorldContextObject, ARTSLandscapeDivider::StaticClass(), LandscapeDividers);
	if (LandscapeDividers.Num() > 0)
	{
		ARTSLandscapeDivider* LandscapeDivider = Cast<ARTSLandscapeDivider>(LandscapeDividers[0]);
		if (IsValid(LandscapeDivider))
		{
			return LandscapeDivider;
		}
	}
	if constexpr (DeveloperSettings::Debugging::GArmorCalculation_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::ReportError("Could not get landscape divider for actor: " + WorldContextObject->GetName() +
			"\n See FRTS_Statics::GetRTSLandscapeDivider");
	}
	return nullptr;
}

UPlayerStartLocationManager* FRTS_Statics::GetPlayerStartLocationManager(const UObject* WorldContextObject)
{
	if (not IsValid(WorldContextObject))
	{
		return nullptr;
	}
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		UPlayerStartLocationManager* PlayerStartLocationManager = RTSController->GetPlayerStartLocationManager();
		if (IsValid(PlayerStartLocationManager))
		{
			return PlayerStartLocationManager;
		}
	}
	return nullptr;
}

UPlayerCameraController* FRTS_Statics::GetPlayerCameraController(const UObject* WorldContextObject)
{
	if (not IsValid(WorldContextObject))
	{
		return nullptr;
	}
	if (ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		UPlayerCameraController* PlayerCameraController = RTSController->GetValidPlayerCameraController();
		if (IsValid(PlayerCameraController))
		{
			return PlayerCameraController;
		}
	}
	return nullptr;
}

UPlayerAudioController* FRTS_Statics::GetPlayerAudioController(const UObject* WorldContextObject)
{
	if (not IsValid(WorldContextObject))
	{
		return nullptr;
	}
	if (const ACPPController* RTSController = GetRTSController(WorldContextObject))
	{
		UPlayerAudioController* Audio = RTSController->GetPlayerAudioController();
		if (IsValid(Audio))
		{
			return Audio;
		}
	}
	return nullptr;
}

AMissionManager* FRTS_Statics::GetGameMissionManager(const UObject* WorldContextObject)
{
	// find it on the map.
	if (AMissionManager* MissionManager = Cast<AMissionManager>(
		UGameplayStatics::GetActorOfClass(WorldContextObject, AMissionManager::StaticClass())))
	{
		if (IsValid(MissionManager))
		{
			return MissionManager;
		}
	}
	RTSFunctionLibrary::ReportError("Could not get mission manager for actor: " + WorldContextObject->GetName() +
		"\n See FRTS_Statics::GetGameMissionManager"
  "\n is there a mission manager on  this map?");
	return nullptr;
}

FString FRTS_Statics::Global_GetTrainingOptionString(const FTrainingOption& TrainingOption)
{
	FString UnitName;
	const FString TypeName = Global_GetUnitTypeString(TrainingOption.UnitType);
	switch (TrainingOption.UnitType)
	{
	case EAllUnitType::UNType_None:
		return "INVALID TRAININGOPTION";
	case EAllUnitType::UNType_Squad:
		UnitName = Global_GetSquadDisplayName(static_cast<ESquadSubtype>(TrainingOption.SubtypeValue));
		return UnitName + " of type: " + TypeName;
	case EAllUnitType::UNType_Harvester:
		return "INVALID TRAININGOPTION";
	case EAllUnitType::UNType_Tank:
		UnitName = Global_GetTankDisplayName(static_cast<ETankSubtype>(TrainingOption.SubtypeValue));
		return UnitName + " of type: " + TypeName;
	case EAllUnitType::UNType_Nomadic:
		UnitName = Global_GetNomadicDisplayName(static_cast<ENomadicSubtype>(TrainingOption.SubtypeValue));
		return UnitName + " of type: " + TypeName;
	case EAllUnitType::UNType_BuildingExpansion:
		UnitName = Global_GetBxpTypeEnumAsString(static_cast<EBuildingExpansionType>(TrainingOption.SubtypeValue));
		return UnitName + " of type: " + TypeName;
	case EAllUnitType::UNType_Aircraft:
		UnitName = Global_GetAircraftDisplayName(static_cast<EAircraftSubtype>(TrainingOption.SubtypeValue));
		return UnitName + " of type: " + TypeName;
	}
	return "INVALID TRAININGOPTION";
}

TMap<ERTSResourceType, int32> FRTS_Statics::GetResourceCostsOfTrainingOption(
	const FTrainingOption& TrainingOption,
	bool& bIsValidCosts,
	const UObject* WorldContextObject)
{
	bIsValidCosts = true;
	switch (TrainingOption.UnitType)
	{
	case EAllUnitType::UNType_None:
		{
			bIsValidCosts = false;
			return {};
		}
	case EAllUnitType::UNType_Squad:
		{
			const ESquadSubtype SquadType = static_cast<ESquadSubtype>(TrainingOption.SubtypeValue);
			return FRTS_Statics::BP_GetSquadDataOfPlayer(1, SquadType, WorldContextObject, bIsValidCosts).Cost.
				ResourceCosts;
		}
	case EAllUnitType::UNType_Harvester:
		{
			bIsValidCosts = false;
			return {};
		}
	case EAllUnitType::UNType_Tank:
		{
			const ETankSubtype TankType = static_cast<ETankSubtype>(TrainingOption.SubtypeValue);
			return FRTS_Statics::BP_GetTankDataOfPlayer(1, TankType, WorldContextObject, bIsValidCosts).Cost.
				ResourceCosts;
		}
	case EAllUnitType::UNType_Nomadic:
		{
			const ENomadicSubtype NomadicType = static_cast<ENomadicSubtype>(TrainingOption.SubtypeValue);
			return FRTS_Statics::BP_GetNomadicDataOfPlayer(1, NomadicType, WorldContextObject, bIsValidCosts).Cost.
				ResourceCosts;
		}
	case EAllUnitType::UNType_BuildingExpansion:
		{
			const EBuildingExpansionType BxpType = static_cast<EBuildingExpansionType>(TrainingOption.SubtypeValue);
			return FRTS_Statics::BP_GetPlayerBxpData(BxpType, WorldContextObject, bIsValidCosts).Cost.ResourceCosts;
		}
	case EAllUnitType::UNType_Aircraft:

		const EAircraftSubtype AircraftType = static_cast<EAircraftSubtype>(TrainingOption.SubtypeValue);
		return FRTS_Statics::BP_GetAircraftDataOfPlayer(1, AircraftType, WorldContextObject, bIsValidCosts).Cost.
			ResourceCosts;
	}
	RTSFunctionLibrary::ReportError(
		"Error resource not found. \n At function GetResourceCostsOfTrainingOption in PlayerResourceManager.cpp."
		"\n Resource: " + Global_GetResourceTypeAsString(ERTSResourceType::Resource_None));
	bIsValidCosts = false;
	return {};
}

FTankData FRTS_Statics::BP_GetTankDataOfPlayer(const int32 PlayerOwningTank, const ETankSubtype TankSubtype,
                                               const UObject* WorldContextObject, bool& bOutIsValidData)
{
	bOutIsValidData = true;
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetTankDataOfPlayer(PlayerOwningTank, TankSubtype);
		}
	}
	bOutIsValidData = false;
	return FTankData();
}

FAircraftData FRTS_Statics::BP_GetAircraftDataOfPlayer(const int32 PlayerOwningAircraft,
                                                       const EAircraftSubtype AircraftSubtype,
                                                       const UObject* WorldContextObject, bool& bOutIsValidData)
{
	bOutIsValidData = true;
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetAircraftDataOfPlayer(AircraftSubtype, PlayerOwningAircraft);
		}
	}
	bOutIsValidData = false;
	return FAircraftData();
}

FNomadicData FRTS_Statics::BP_GetNomadicDataOfPlayer(const int32 PlayerOwningNomadic,
                                                     const ENomadicSubtype NomadicSubtype,
                                                     const UObject* WorldContextObject, bool& bOutIsValidData)
{
	bOutIsValidData = true;
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetNomadicDataOfPlayer(PlayerOwningNomadic, NomadicSubtype);
		}
	}
	bOutIsValidData = false;
	return FNomadicData();
}

FSquadData FRTS_Statics::BP_GetSquadDataOfPlayer(const int32 PlayerOwningSquad, const ESquadSubtype SquadSubtype,
                                                 const UObject* WorldContextObject, bool& bOutIsValidData)
{
	bOutIsValidData = true;
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetSquadDataOfPlayer(PlayerOwningSquad, SquadSubtype);
		}
	}
	bOutIsValidData = false;
	return FSquadData();
}

FBxpData FRTS_Statics::BP_GetPlayerBxpData(const EBuildingExpansionType BxpType, const UObject* WorldContextObject,
                                           bool& bOutIsValidData)
{
	bOutIsValidData = true;
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetPlayerBxpData(BxpType);
		}
	}
	bOutIsValidData = false;
	return FBxpData();
}

FBxpData FRTS_Statics::BP_GetEnemyBxpData(const EBuildingExpansionType BxpType, const UObject* WorldContextObject,
                                          bool& bOutIsValidData)
{
	bOutIsValidData = true;
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetEnemyBxpData(BxpType);
		}
	}
	bOutIsValidData = false;
	return FBxpData();
}

FDigInData FRTS_Statics::BP_GetDigInDataOfPlayer(const int32 PlayerOwningDigIn, const EDigInType DigInType,
                                                 const UObject* WorldContextObject, bool& bOutIsValidData)
{
	bOutIsValidData = true;
	if (IsValid(WorldContextObject))
	{
		if (const ACPPGameState* GameState = FRTS_Statics::GetGameState(WorldContextObject))
		{
			return GameState->GetDigInDataOfPlayer(PlayerOwningDigIn, DigInType);
		}
	}
	bOutIsValidData = false;
	return FDigInData();
}

int32 FRTS_Statics::GetIndexOfAbilityForBaseTank(const int32 PlayerOwningTank, const ETankSubtype TankSubtype,
                                                 const EAbilityID AbilityType, const UObject* WorldContextObject)
{
	bool bIsValidData = false;
	FTankData TankData = FRTS_Statics::BP_GetTankDataOfPlayer(PlayerOwningTank, TankSubtype, WorldContextObject,
	                                                          bIsValidData);
	if (!bIsValidData)
	{
		return INDEX_NONE;
	}
	for (int32 i = 0; i < TankData.Abilities.Num(); ++i)
	{
		if (TankData.Abilities[i].AbilityId == AbilityType)
		{
			return i;
		}
	}
	return INDEX_NONE;
}
