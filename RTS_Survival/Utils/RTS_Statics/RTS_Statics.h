#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInType/DigInType.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"
#include "RTS_Survival/UnitData/DigInData.h"
#include "RTS_Survival/UnitData/NomadicVehicleData.h"
#include "RTS_Survival/UnitData/SquadData.h"
#include "RTS_Survival/UnitData/VehicleData.h"


enum class ERTSFaction : uint8;
class URTSGameInstance;
class AMissionManager;
class UAnimatedTextWidgetPoolManager;
class UAnimatedIconWidgetPoolManager;
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
class UPlayerUnitVariantManager;
class ARTSNavigator;
class ARTSAsyncSpawner;
class UPlayerResourceManager;
class ACPPController;

/** @brief Gameplay code uses these world-context helpers to access shared RTS services. */
class RTS_SURVIVAL_API FRTS_Statics
{
	FRTS_Statics();
	~FRTS_Statics();

public:

	// ------------------ World subsystem helpers ------------
	static UAnimatedTextWidgetPoolManager* GetVerticalAnimatedTextWidgetPoolManager(const UObject* WorldContextObject);
	static UAnimatedIconWidgetPoolManager* GetVerticalAnimatedIconWidgetPoolManager(const UObject* WorldContextObject);

	// -------------------- Begin Player Related Objects --------------------
	/** @return A IsValid CPPController or null. */
	static ACPPController* GetRTSController(const UObject* WorldContextObject);

	/**
	 * @return True in the pre-game hangar (RTS_FactionSelection): its player controller is an
	 * AFactionPlayerController, yet it spawns real units (the hangar preview and the commander screen's ability
	 * preview). Code that needs the mission's ACPPController uses this to skip its "no controller" report there.
	 */
	static bool GetIsInPreGameHangar(const UObject* WorldContextObject);
	
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

	/** @return Whether the active mission difficulty has enabled mutation behaviours. */
	static bool AreMutationsOn(const UObject* WorldContextObject);
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

	/** @return A IsValid PlayerUnitVariantManager or null (no player controller in this world). */
	static UPlayerUnitVariantManager* GetPlayerUnitVariantManager(const UObject* WorldContextObject);

	/** @return A IsValid Navigator or null. */
	static ARTSNavigator* GetRTSNavigator(const UObject* WorldContextObject);

	/** @return A IsValid CPPGameState or null. */
	static ACPPGameState* GetGameState(const UObject* WorldContextObject);

	static URTSGameInstance* GetRTSGameInstance(const UObject* WorldContextObject);

	static ERTSFaction GetPlayerFaction(const UObject* WorldContextObject);

	/** @return A IsValid GameExplosionsManager or null. */
	static UGameExplosionsManager* GetGameExplosionsManager(const UObject* WorldContextObject);

	/**
	 * @return The default enemy controller: the one of the first AI player, or the lowest-numbered one when that
	 * player has none. Null when the map has no controller.
	 */
	static AEnemyController* GetEnemyController(const UObject* WorldContextObject);

	/**
	 * @brief Finds the controller that plays a player, for code that knows whose unit or ability it handles.
	 * @param WorldContextObject Any object of the world.
	 * @param Player Owning player of the unit or ability.
	 * @return For AI players 2..MaxPlayers their own controller, or null when the map has none for that player.
	 * Every other owner (human, neutral, unset) has no controller of its own and gets the default one, as before.
	 */
	static AEnemyController* GetEnemyControllerForPlayer(const UObject* WorldContextObject, int32 Player);

	/** @return The controller for the owner of this unit (see GetEnemyControllerForPlayer); null for a null unit. */
	static AEnemyController* GetEnemyControllerOfUnit(const AActor* Unit);

	/** @return Every enemy controller of the world, lowest player number first. */
	static TArray<AEnemyController*> GetAllEnemyControllers(const UObject* WorldContextObject);

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
