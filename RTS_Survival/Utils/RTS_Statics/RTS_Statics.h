#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInType/DigInType.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/UnitData/DigInData.h"
#include "RTS_Survival/UnitData/NomadicVehicleData.h"
#include "RTS_Survival/UnitData/SquadData.h"
#include "RTS_Survival/UnitData/VehicleData.h"


class AMissionManager;
class UAnimatedTextWidgetPoolManager;
class UPlayerPortraitManager;
class AEnemyController;
class UPlayerAudioController;
class UGameExplosionsManager;
class UPlayerCameraController;
class UPlayerStartLocationManager;
class ARTSLandscapeDivider;
class ALandscapedeformManager;
class UTrainingMenuManager;
class UMainGameUI;
class UPlayerControlGroupManager;
class AFowManager;
class ACameraPawn;
class UPlayerBuildRadiusManager;
class ANomadicVehicle;
class ACPPGameState;
class URTSGameSettingsHandler;
class UPlayerTechManager;
class ARTSNavigator;
class ARTSAsyncSpawner;
class UPlayerResourceManager;
class ACPPController;

class RTS_SURVIVAL_API FRTS_Statics
{
	FRTS_Statics();
	~FRTS_Statics();

public:

	// ------------------ World subsystem helpers ------------
	static UAnimatedTextWidgetPoolManager* GetVerticalAnimatedTextWidgetPoolManager(const UObject* WorldContextObject);

	// -------------------- Begin Player Related Objects --------------------
	/** @return A IsValid CPPController or null. */
	static ACPPController* GetRTSController(const UObject* WorldContextObject);
	
	/** @return A IsValid PlayerResourceManager or null. */
	static UPlayerResourceManager* GetPlayerResourceManager(const UObject* WorldContextObject);

	/** @return A IsValid ACameraPawn or null. */
	static ACameraPawn* GetPlayerCameraPawn(const UObject* WorldContextObject);

	/** @return The Hq of hte player. */
	static ANomadicVehicle* GetPlayerHq(const UObject* WorldContextObject);

	static UPlayerPortraitManager* GetPlayerPortraitManager(const UObject* WorldContextObject);

	/** @return valid player build manager or null. */
	static UPlayerBuildRadiusManager* GetPlayerBuildRadiusManager(const UObject* WorldContextObject);

	/** @return valid player control group manager or null. */
	static UPlayerControlGroupManager* GetPlayerControlGroupManager(const UObject* WorldContextObject);

	/** @return Valid UPlayerStartLocationManager or null */
	static UPlayerStartLocationManager* GetPlayerStartLocationManager(const UObject* WorldContextObject);

	/** @return Valid PlayerCameraController or null */
	static UPlayerCameraController* GetPlayerCameraController(const UObject* WorldContextObject);

	static UPlayerAudioController* GetPlayerAudioController(const UObject* WorldContextObject);

	static AMissionManager* GetGameMissionManager(const UObject* WorldContextObject);
	// -------------------- End Player Related Objects --------------------

	/** @return Whether the player is holding down the shift key. */
	static bool IsShiftPressed(const UObject* WorldContextObject);

	/** @return Whether the player is holding down the control key. */
	static bool IsControlPressed(const UObject* WorldContextObject);

	/** @return A IsValid Main menu reference or null. */
	static UMainGameUI* GetMainGameUI(const UObject* WorldContextObject);

	/** @return A IsValid UTrainingMenuManager or null. */
	static UTrainingMenuManager* GetTrainingMenuManager(const UObject* WorldContextObject);

	/** @return A IsValid AsyncSpawner or null. */
	static ARTSAsyncSpawner* GetAsyncSpawner(const UObject* WorldContextObject);

	/** @return A IsValid PlayerTechManager or null. */
	static UPlayerTechManager* GetPlayerTechManager(const UObject* WorldContextObject);

	/** @return A IsValid Navigator or null. */
	static ARTSNavigator* GetRTSNavigator(const UObject* WorldContextObject);

	/** @return A IsValid CPPGameState or null. */
	static ACPPGameState* GetGameState(const UObject* WorldContextObject);

	/** @return A IsValid GameExplosionsManager or null. */
	static UGameExplosionsManager* GetGameExplosionsManager(const UObject* WorldContextObject);

	static AEnemyController* GetEnemyController(const UObject* WorldContextObject);

	/** @return A IsValid UGameUnitManager or null. */
	static UGameUnitManager* GetGameUnitManager(const UObject* WorldContextObject);

	/** @return A IsValid RTSGameUpdate or null. */
	static URTSGameSettingsHandler* GetRTSGameUpdate(const UObject* WorldContextObject);

	/** @retirm The IsValid FOW manager or null */
	static AFowManager* GetFowManager(const UObject* WorldContextObject);

	/** @return The IsValid LandscapeDeformManager or null. */
	static ALandscapedeformManager* GetLandscapeDeformManager(const UObject* WorldContextObject);


	/** @return Valid RTSLandscapeDivider or null */
	static ARTSLandscapeDivider* GetRTSLandscapeDivider(const TObjectPtr<UObject> WorldContextObject);

	static FString Global_GetTrainingOptionString(const FTrainingOption& TrainingOption);



	// ------------- OBTAIN CHECKED DATA ----------------
	static TMap<ERTSResourceType, int32> GetResourceCostsOfTrainingOption(const FTrainingOption& TrainingOption,
	                                                                      bool& bIsValidCosts, const UObject* WorldContextObject);

	static FTankData BP_GetTankDataOfPlayer(const int32 PlayerOwningTank, const ETankSubtype TankSubtype,
	                                        const UObject* WorldContextObject, bool& bOutIsValidData);

	static FAircraftData BP_GetAircraftDataOfPlayer(const int32 PlayerOwningAircraft, const EAircraftSubtype AircraftSubtype,
	                                              const UObject* WorldContextObject, bool& bOutIsValidData);

	static FNomadicData BP_GetNomadicDataOfPlayer(const int32 PlayerOwningNomadic, const ENomadicSubtype NomadicSubtype,
	                                              const UObject* WorldContextObject, bool& bOutIsValidData);

	static FSquadData BP_GetSquadDataOfPlayer(const int32 PlayerOwningSquad, const ESquadSubtype SquadSubtype,
	                                          const UObject* WorldContextObject, bool& bOutIsValidData);

	static FBxpData BP_GetPlayerBxpData(const EBuildingExpansionType BxpType, const UObject* WorldContextObject,
	                                    bool& bOutIsValidData);

	static FBxpData BP_GetEnemyBxpData(const EBuildingExpansionType BxpType, const UObject* WorldContextObject,
	                                    bool& bOutIsValidData);

	static FDigInData BP_GetDigInDataOfPlayer(const int32 PlayerOwningDigIn, const EDigInType DigInType,
	                                              const UObject* WorldContextObject, bool& bOutIsValidData);

	static int32 GetIndexOfAbilityForBaseTank(const int32 PlayerOwningTank,
	                                          const ETankSubtype TankSubtype,
	                                          const EAbilityID AbilityType, const UObject* WorldContextObject);
};
