// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.

/**
 * WHEN ADDING A NEW UNIT MASTER -- CHILD TO THE GAME
 *	make a new EAllUnitType, and set the new unit's type in the construction script
 *	Add a new array of buttons in the ButtonActor (spawned on map)
 *	Add a new array of abilities for the UnitType in the controller construction script
 *	Add a new Pair to the HashMap TMapUnitTypeToAbilityIndex
 *	Add a new Boolean to the TSelectedUnitTypes, This array also determines the Hierarchy of unit selection,
 *	so change order if needed
 *	Add a new Pair to the HashMap TMapUnitTypeToSelectionIndex
 *	Make sure that the unit has all implementations of Bp specific Abilities, Like the InfMaster AttackUnit
 *
 * WHEN ADDING A NEW RANGED WEAPON MASTER -- CHILD TO THE GAME
 *	Add a new struct to @TRangedWeaponData with all the weapon's stats
 *	IN bp Constructor implement @SetWeaponValues
 *	IN bp set weapon owner and controller references set bool @IsOwnedByUnitMaster
 *	IN bp set event @UpdateWeaponVectors -> change @socketlocation and @forwardvector
 */
#pragma once

#include "CoreMinimal.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "AsyncRTSAssetsSpawner/AsyncBxpRequestState/AsyncBxpRequestState.h"
#include "GameUI_Utils/AGameUIController.h"
#include "Formation/FormationMovement.h"
#include "Kismet/KismetMathLibrary.h"
#include "ConstructionPreview/CPPConstructionPreview.h"
#include "ConstructionPreview/BuildingPreviewMode/PlayerBuildingPreviewMode.h"
#include "Formation/FormationPositionEffects/PlayerFormationPositionEffects.h"
#include "FViewportScreenshotTask/FViewportScreenshotTask.h"
#include "GameInitCallbacks/MainMenuUICallbacks.h"
#include "PlayerFieldConstructionData/PlayerFieldConstructionCandidate.h"
#include "PlayerRotationArrowSettings/PlayerRotationArrowSettings.h"
#include "PlayerEscapeMenuManager/PlayerEscapeMenuHelper.h"
#include "RTS_Survival/GameUI/EscapeMenu/PlayerEscapeMenuSettings.h"
#include "RTS_Survival/Audio/RTSVoiceLineHelpers/RTS_VoiceLineHelpers.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachWeaponAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityTypes.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "SelectionHelpers/SelectionChangeAction.h"

#include "CPPController.generated.h"

class APlayerAimAbility;
class UAttachedWeaponAbilityComponent;
class UPlayerPortraitManager;
class UW_Portrait;
struct FNomadicPreviewAttachments;
class AScavengeableObject;
enum class ERTSPrimaryClickContext;
struct FPlayerRotationArrowSettings;
enum class ERTSPauseGameOptions : uint8;
enum class ERTSVoiceLine : uint8;
class UPlayerAudioController;
struct FMainMenuUICallbacks;
class UW_ChoosePlayerStartLocation;
class ALandscapedeformManager;
class UW_HoveringActor;
class UPlayerControlGroupManager;
class AFowManager;
enum class EPlayerError : uint8;
class UPlayerBuildRadiusManager;
class URadiusComp;
enum class ETechnology : uint8;
class UTechnologyEffect;
class UPlayerTechManager;
class ASquadUnit;
union FTargetUnion;
class UPlayerCommandTypeDecoder;
class UPlayerCameraController;
class URTSGameSettingsHandler;
class UPlayerResourceManager;
class UPlayerStartGameControl;
class ARTSAsyncSpawner;
class ARTSNavigator;
class ACameraPawn;
struct FActionUIParameters;
class ABuildingExpansion;
class IBuildingExpansionOwner;
enum class EBuildingExpansionType : uint8;
class UMainGameUI;
class ANomadicVehicle;
// forward declaration
class RTS_SURVIVAL_API ASquadController;
class RTS_SURVIVAL_API ACPPHUD;
class RTS_SURVIVAL_API ACPPGameState;
class RTS_SURVIVAL_API ACPPResourceMaster;
class RTS_SURVIVAL_API ASelectablePawnMaster;
class RTS_SURVIVAL_API ASelectableActorObjectsMaster;
class RTS_SURVIVAL_API UPlayerProfileLoader;
class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FPauseGameState
{
	GENERATED_BODY()
	// Whether the game is paused or not.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	bool bM_IsGamePaused = false;

	// Game locking is set by specific widgets; while locked the game cannot be unpaused.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	bool bM_IsGameLocked = false;
};

/**
 *
 */
USTRUCT(BlueprintType)
struct FRangedWeaponData
{
	GENERATED_BODY()

	// Name of the weapon to access datatables
	FString WeaponName;
	// Base damage of one bullet
	float AverageDamage;
	// percentage used to fluctuate damage
	float DamageFlux;
	// in centimeters
	float Range;
	// used before damage calculation
	float ArmorPen;
	// amount of bullets before reloading
	int MagCapacity;
	// entire mag reload speed in sec
	float ReloadSpeed;
	// cooldown used in between shots
	float AverageCooldown;
	// percentage used to fluctuate cooldown
	int CooldownFlux;
	// if the weapon is on burst mode, this is how many bullets are fired in one burst
	int BulletsInBurst;
	// cooldown used between shots in one burst
	float BurstCooldown;
	// 1 to 100, how accurate the weapon is; 100 means no misses
	int Accuracy;
};


/**
 * @brief enum for all actions possible depending on the clicked actor
 */
UENUM(BlueprintType)
enum class ECommandType : uint8
{
	Movement,
	MoveRallyPoint,
	Attack,
	AttackGround,
	AlliedCharacter,
	EnemyCharacter,
	AlliedActor,
	AlliedAirBase,
	EnemyActor,
	AlliedBuilding,
	HarvestResource,
	PickupItem,
	DestructableEnvActor,
	DestroyDestructableEnvActor,
	ScavengeObject,
	DestroyScavengableObject,
	RotateTowards,
	Repair,
	EnterCargo,
	CaptureActor,
};

static FString GetStringCommandType(const ECommandType CommandType)
{
	switch (CommandType)
	{
	case ECommandType::Movement:
		return TEXT("Movement");
	case ECommandType::Attack:
		return TEXT("Attack");
	case ECommandType::AlliedCharacter:
		return TEXT("AlliedCharacter");
	case ECommandType::EnemyCharacter:
		return TEXT("EnemyCharacter");
	case ECommandType::AlliedActor:
		return TEXT("AlliedActor");
	case ECommandType::AlliedAirBase:
		return TEXT("AlliedAirBase");
	case ECommandType::EnemyActor:
		return TEXT("EnemyActor");
	case ECommandType::AlliedBuilding:
		return TEXT("AlliedBuilding");
	case ECommandType::HarvestResource:
		return TEXT("HarvestResource");
	case ECommandType::PickupItem:
		return TEXT("PickupItem");
	case ECommandType::RotateTowards:
		return TEXT("RotateTowards");
	case ECommandType::Repair:
		return TEXT("Repair");
	case ECommandType::CaptureActor:
		return TEXT("CaptureActor");
	default:
		return TEXT("Unknown");
	}
}


USTRUCT()
struct FPlayerProfileLoadingStatus
{
	GENERATED_BODY()
	bool bHasLoadedPlayerProfile = false;
	// Whether to call 	M_PlayerResourceManager->InitializeResourcesSettings(RTSGameSettings) once the profile is loaded.
	// If not, then the profile was loaded before the game settings were loaded and InitializeResourcesSettings was called immediately.
	bool bInitializeResourcesSettingsOnLoad = false;
	// If this is set to true, then the player card profile was loaded before the game settings were loaded.
	// This means the player Resource Manager could not add resource bonuses provided by those cards immediately.
	// Instead, the bonuses are now stored in the manager and await for the HQ to be spawned.
	bool bInitializeHqResourceBonusesFromProfileCardsOnLoad = false;
};

/**
 * @brief Main player controller used to route input, UI, and player gameplay setup.
 * @note InitPlayerController: call in blueprint to initialize runtime references.
 */
UCLASS()
class RTS_SURVIVAL_API ACPPController : public APlayerController
{
	GENERATED_BODY()

	friend class RTS_SURVIVAL_API AGameUIController;
	friend class RTS_SURVIVAL_API UPlayerTechManager;
	friend class RTS_SURVIVAL_API UPlayerControlGroupManager;
	friend class RTS_SURVIVAL_API UPlayerProfileLoader;
	friend class RTS_SURVIVAL_API UPlayerResourceManager;
	// For calling StartGameAtLocation with the picked location by the Start Location UI / Mngr.
	friend class RTS_SURVIVAL_API UPlayerStartLocationManager;

public:
	// Constructor
	ACPPController();

	void InitPortrait(UW_Portrait* PortraitWidget) const;

	/**
	 * @brief Capture a screenshot of the current game viewport at its current size.
	 * @param bIncludeUI Include UMG/Slate/HUD in the screenshot (engine handles this flag).
	 * @param InnerPercent
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void TakeScreenShot(const bool bIncludeUI, const float InnerPercent);


	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void PauseGame(const ERTSPauseGameOptions PauseOption);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetGameUIVisibility(const bool bVisible);

	// Pauses and prevents unpause until this function is called with false.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void PauseAndLockGame(const bool bLock);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "EscapeMenu")
	void OpenEscapeMenu();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "EscapeMenu")
	void CloseEscapeMenu();

	void OpenEscapeMenuSettings();
	void CloseEscapeMenuSettings();
	void OpenEscapeMenuKeyBindings();
	void CloseEscapeMenuKeyBindings();

	UFUNCTION(BlueprintCallable, Category = "Input")
	UInputMappingContext* GetDefaultInputMappingContext() const;

	/**
	 * @brief Updates a single action binding so UI-driven remaps can take effect immediately.
	 * @param ActionToRebind Action whose key mapping should change.
	 * @param OldKey The key currently mapped to the action.
	 * @param NewKey The newly selected key to apply.
	 */
	void ChangeKeyBinding(UInputAction* ActionToRebind, const FKey OldKey, const FKey NewKey);
	/**
	 * @brief Removes a single action binding so the key can be reused elsewhere.
	 * @param ActionToUnbind Action whose key mapping should be removed.
	 * @param BoundKey The key currently mapped to the action.
	 */
	void UnbindKeyBinding(UInputAction* ActionToUnbind, const FKey BoundKey);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	UPlayerPortraitManager* GetPlayerPortraitManager() const;

	/** @return whether the menu was successfully hidden/Shown and the camera movement disabled/enabled successfully.
	 * @note Suppresses all voicelines except the play custom announcer voice lines.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	bool OnCinematicTakeOver(const bool bStartCinematic);
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline bool GetIsGameOnPause() const { return M_PauseGameState.bM_IsGamePaused; };

	// Tick
	virtual void Tick(float DeltaTime) override;
	// inputs
	virtual void SetupInputComponent() override;

	void DisplayErrorMessage(const EPlayerError Error);
	bool GetHasPlayerProfileLoaded() const { return M_PlayerProfileLoadingStatus.bHasLoadedPlayerProfile; }

	/**
	 * @brief Play a voice line of a unit potentially different from the main selected unit.
	 * @param Unit The unit to get the voice lines from.
	 * @param VoiceLine The type of voice line to play.
	 * @param bForcePlay Stop the current voice line and force play this one.
	 * @param bQueueIfNotPlayed If not able to play the voice line, queue it.
	 */
	void PlayVoiceLine(const AActor* Unit, const ERTSVoiceLine VoiceLine, const bool bForcePlay = false,
	                   const bool bQueueIfNotPlayed = false) const;

	void PlayAnnouncerVoiceLine(EAnnouncerVoiceLineType VoiceLineType, bool bQueueIfNotPlayed = false,
	                            bool bInterruptRegularVoiceLines = false) const;

	// Suppresses all voice lines except the PlayCustomAnnouncer voicelines.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Audio")
	void SetSuppressRegularVoiceLines(const bool bSuppress);


	/**
	 * @brief Play a 3D (spatial) voice‐line at a world location.
	 * @param PrimarySelectedUnit  The unit whose voice‐line category to use.
	 * @param VoiceLineType        Which event (Select, Attack, etc.).
	 * @param Location             World location to play the sound at.
	 * @param bIgnorePlayerCooldown
	 * @return The audio component if the voice line could be played.
	 */
	UAudioComponent* PlaySpatialVoiceLine(
		const AActor* PrimarySelectedUnit,
		ERTSVoiceLine VoiceLineType,
		const FVector& Location, bool bIgnorePlayerCooldown) const;


	// Called for custom missions to init the player profile.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void MissionOnHqSpawned(const FUnitCost BonusPerResource);

	/**
	 * @brief Spawns a batch of reinforcement units and directs them to a projected formation point.
	 * @param WaveElements Options and spawn locations for each unit in the batch.
	 * @param ProjectionExtentScale Scale used to project the formation location to navigable space.
	 * @param CenterFormationLocation Center location for the formation movement orders.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Reinforcements")
	void SpawnReinforcementAttackWave(
		const TArray<FAttackWaveElement>& WaveElements,
		const float ProjectionExtentScale,
		const FVector& CenterFormationLocation);

	/**
	 * @brief Called by CPPGameState when the UpdateComponent is loaded.
	 * Inits the start resources in the player resource manager.
	 * @param RTSGameSettings The game update component.
	 */
	void OnRTSGameSettingsLoaded(URTSGameSettingsHandler* RTSGameSettings);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Resources")
	inline UPlayerResourceManager* GetPlayerResourceManager() const { return M_PlayerResourceManager; }

	inline UPlayerTechManager* GetPlayerTechManager() const { return M_PlayerTechManager; }
	inline UPlayerBuildRadiusManager* GetPlayerBuildRadiusManager() const { return M_PlayerBuildRadiusManager; }
	inline ANomadicVehicle* GetPlayerHq() const { return M_PlayerHQ; }
	inline ACameraPawn* GetCameraPawn() const { return CameraRef; }
	inline AFowManager* GetFowManager() const { return M_FowManager; }
	// Used to set the reference to the Start location choosing UI.
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	inline UPlayerStartLocationManager* GetPlayerStartLocationManager() const { return M_PlayerStartLocationManager; }


	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	inline UPlayerAudioController* GetPlayerAudioController() const { return M_PlayerAudioController; }

	inline UPlayerControlGroupManager* GetPlayerControlGroupManager() const { return M_PlayerControlGroupManager; }

	inline bool GetIsShiftPressed() const { return bIsHoldingShift; }
	inline bool GetIsControlPressed() const { return bIsHoldingControl; }

	ALandscapedeformManager* GetLandscapeDeformManager();

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	UPlayerCameraController* GetValidPlayerCameraController();

	// Propagates to the camera controller; can enable / disable the edge scrolling and other camera movement.
	void SetCameraMovementDisabled(const bool bDisable) const;

	/*--------------------------------- OBJECT REFS ---------------------------------*/

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ReferenceCasts")
	ACameraPawn* CameraRef;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ReferenceCasts")
	USpringArmComponent* SpringarmRef;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ReferenceCasts")
	ACPPConstructionPreview* CPPConstructionPreviewRef;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "ReferenceCasts")
	ACPPHUD* HUDRef;

	UPROPERTY(BlueprintReadWrite, Category = "ReferenceCasts")
	ACPPGameState* CPPGameStateRef;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FTransform EmptyTransfrom;

	// used to determine the speed by which a construction's placement ghost can be turned
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ConstructionRotationRate;

	inline ARTSNavigator* GetRTSNavigator() const { return M_RTSNavigator; }

	inline ARTSAsyncSpawner* GetRTSAsyncSpawner() const { return M_RTSAsyncSpawner; }

	TMap<ETechnology, TSoftClassPtr<UTechnologyEffect>> GetTechnologyEffectsMap() const
	{
		return M_TechnologyEffectsMap;
	}

	/*--------------------------------- Building ---------------------------------*/

	/**
	 * Calculates the cursor world position snapping on landscape.
	 * @param SightDistance Max distance to trace for landscape. Default set DeveloperSettings::UIUX::SightDistanceMouse
	 * @param bOutIsValidCalculation Whether the calculation was valid.
	 * @return The cursor world position.
	 */
	UFUNCTION(BlueprintCallable)
	FVector GetCursorWorldPosition(const float SightDistance, bool& bOutIsValidCalculation) const;

	/**
	 * @brief Inits the bxp entry on the owner and updates the ui. Instantly places the bxp on its socket
	 * or origin position depending on the type of instant spawn.
	 * @param SpawnedBxp The buidling expansion spawned.
	 * @param BxpOwner The owner of the building expansion, which will be updated with the new bxp. 
	 * @param BxpConstructionRulesAndType The rules for constructing this bxp.
	 * @param ExpansionSlotIndex The index of the item in the owner's expansion array this bxp belongs to.
	 * @return Whether the bxp entry could be constructed and placed instantly.
	 *
	 * @note This function is public as it is also used for batch bxp placement of instantly placed expansions upon
	 * converting trucks back to buildings.
	 * @note See IBuildingExpansionOwner::CreateAttachedPackedBxp for more details.
	 */
	bool OnBxpSpawnedAsync_InstantPlacement(
		ABuildingExpansion* SpawnedBxp,
		const TScriptInterface<IBuildingExpansionOwner>& BxpOwner,
		const FBxpOptionData& BxpConstructionRulesAndType,
		const int ExpansionSlotIndex) const;


	/*--------------------------------- Game UI ---------------------------------*/

	inline bool GetIsPlayerInTechTree() const { return bM_IsInTechTree; }
	void SetIsPlayerInTechTree(const bool bIsInTechTree);
	void SetIsPlayerInArchive(const bool bIsInArchive);

	UMainGameUI* GetMainMenuUI() const { return M_MainGameUI; }

	// Keeps track of callbacks that need to be invoked when the main menu is loaded.
	// In case the menu is already loaded, the callback is invoked immediately.
	// use CallBackOnMenuReady.
	UPROPERTY()
	FMainMenuUICallbacks OnMainMenuCallbacks;

	/**
	 * Cancels the building construction on the requesting actor if it is of the NomadicVehicle Type.
	 * @param RequestingActor The actor that requested the building to be cancelled.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="MainGameUI")
	void CancelBuilding(AActor* RequestingActor);

	/**
	 * Constructs the building on the requesting actor if it is of the NomadicVehicle Type.
	 * @param RequestingActor The actor that requested the building to be constructed.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="MainGameUI")
	void ConstructBuilding(AActor* RequestingActor);

	/**
	 * @brief Instructs the actor to convert to vehicle if it is a nomadic vehicle.
	 * @param RequestingActor The vehicle that requested the conversion.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="MainGameUI")
	void ConvertBackToVehicle(AActor* RequestingActor);

	/**
	 * @brief Instructs the actor to stop the conversion to vehicle.
	 * @param RequestingActor the vehicle that wants to stop converting.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="MainGameUI")
	void CancelVehicleConversion(AActor* RequestingActor);

	/**
	 * @brief Called when a truck finished converting to either a building or a vehicle.
	 * @param ConvertedTruck the truck that was converted.
	 * @param bConvertedToBuilding Whether the truck was converted to a building or not.
	 * @post the main game UI is updated accordingly.
	 */
	void TruckConverted(
		ANomadicVehicle* ConvertedTruck,
		const bool bConvertedToBuilding) const;


	/**
	 * @brief Used for placing Building Expansions with preview.
	 * @note There are two typesCan either be:
	 * @note 1) Completely new in which case we always start a preview so the use can select a location.
	 * @note 2) Unpacked BXP of free placement form which always needs a user picked location.
	 * @note Is called from MainGameUI after the user clicks on a building expansion option
	 */
	void ExpandBuildingWithBxpAndStartPreview(
		const FBxpOptionData& BuildingExpansionTypeData,
		TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner,
		const int ExpansionSlotIndex,
		const bool bIsUnpacked_Preview_Expansion);

	/**
	 * Called when clicking a building expansion item that is either searching for a placement location for a new
	 * bxp or searching to place a packed bxp.
	 * @param BuildingExpansionOwner 
	 * @param bIsPackedExpansion
	 * @param BxpType 
	 */
	void OnClickedCancelBxpPlacement(
		const TScriptInterface<IBuildingExpansionOwner>& BuildingExpansionOwner,
		const bool bIsPackedExpansion,
		const EBuildingExpansionType BxpType);


	void RebuildPackedExpansion(
		const FBxpOptionData& BuildingExpansionTypeData,
		const TScriptInterface<IBuildingExpansionOwner>& BuildingExpansionOwner,
		const int ExpansionSlotIndex);

	/** @brief function called by the Async spawner when the building expansion is spawned.
	 * @param SpawnedBxp: The spawned building expansion.
	 * @param BxpOwner: The owner of the building expansion.
	 * @param BxpConstructionRulesAndType: The type of building expansion.
	 * @param ExpansionSlotIndex: The index in the array of expansions to add the expansion to.
	 * @param BxpLoadingType: Whether the expansion is unpacked or build for the first time.
	 */
	void OnBxpSpawnedAsync(
		ABuildingExpansion* SpawnedBxp,
		TScriptInterface<IBuildingExpansionOwner> BxpOwner,
		const FBxpOptionData& BxpConstructionRulesAndType,
		const int ExpansionSlotIndex,
		const EAsyncBxpLoadingType BxpLoadingType);

	/** @brief Is Game UI extended? e.g. with building menu */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool IsExtendingGameUI;

	// Checks if there are enough resources to build (true)
	UFUNCTION(BlueprintPure, Category = "Building Construction")
	bool CheckResourcesForConstruction();

	/**
	 * @brief Uses the units obtained in the selection rectangle to either add new units to the selection or do a full
	 * reset on the selection if @fullSelectionReset = true.
	 *
	 * If a Selected unit's SquadController was not yet
	 * added the controller will be added and all other Ums that are of the same squad and not selected are selected
	 * as well.
	 * @pre: The provided arrays contain units that can be selected.
	 * @param MarqueeSquadUnits: The UMs obtained from the selection rectangle.
	 * @param MarqueeSelectableActors: The SelectableActorObjectMasters that are obtained from the selection rectangle.
	 */
	void SelectUnitsFromMarquee(const TArray<ASquadUnit*>& MarqueeSquadUnits,
	                            const TArray<ASelectableActorObjectsMaster*>& MarqueeSelectableActors,
	                            const TArray<ASelectablePawnMaster*>& MarqueeSelectablePawns);

	/**
	 * @brief Empties all selection arrays and sets status to deselected for those actors that were selected.
	 * @note: FOR SelectedUnitMasters:
	 * @note sets all units to Deselected
	 * @note: FOR SquadControllers:
	 * @note clears TSquadControllers array
	 * @note: FOR SelectedActorMasters:
	 * @note todo deselect actors
	 */
	void ResetSelectionArrays();

	/** @brief How many seconds passed since last frame */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DeltaSeconds;


	void ActivateActionButton(const int32 ActionButtonAbilityIndex);


	/** @brief all SquadControllers of which the player currently has direct control. */
	UPROPERTY(BlueprintReadOnly)
	TArray<ASquadController*> TSelectedSquadControllers;

	/** @brief All SelectableActors that the player currently controls. */
	UPROPERTY(BlueprintReadOnly)
	TArray<ASelectableActorObjectsMaster*> TSelectedActorsMasters;

	/** @brief: all PawnMasters that the player currently controls. */
	UPROPERTY(BlueprintReadOnly)
	TArray<ASelectablePawnMaster*> TSelectedPawnMasters;


	void CancelBuildingExpansionPreview(
		TScriptInterface<IBuildingExpansionOwner> BxpOwner,
		const bool bIsPackedBxp);

	/**
	 * @brief Stops the building expansion's construction animation and destroys it.
	 * @param BxpOwner The owner of the building expansion, which will destroy it.
	 * @param BuildingExpansion The building expansion to destroy.
	 * @param bDestroyPackedBxp Whether the building expansion is packed up or not.
	 * @note if the bxp is packed we set the status back to packed and make sure to save the type of expansion.
	 */
	void CancelBuildingExpansionConstruction(TScriptInterface<IBuildingExpansionOwner> BxpOwner,
	                                         ABuildingExpansion* BuildingExpansion,
	                                         const bool bDestroyPackedBxp) const;

	/**
	 * @brief Moves the camera position to the actor to focus.
	 * @param ActorToFocus The actor to move the camera to.
	 */
	void FocusCameraOnActor(const AActor* ActorToFocus) const;
	/**
	 * @brief Moves the camera to the specified location and sets the zoom level to base.
	 * @param LocationToFocus Location to move camera to.
	 */
	void FocusCameraOnLocation(const FVector& LocationToFocus) const;

	/**
	 * @brief Public callable from squads of which all units died.
	 * @param SquadControllerToRemove The squad controller to remove from the selection.
	 * @post If this squad type is not longer represented in the selection, the ActionUIHierarchy is rebuilt.
	 * @post If this squad was primary selected then we refresh the game UI.
	 */
	void RemoveSquadFromSelectionAndUpdateUI(ASquadController* SquadControllerToRemove);

	void RemoveActorFromSelectionAndUpdateUI(ASelectableActorObjectsMaster* ActorToRemove);
	void RemovePawnFromSelectionAndUpdateUI(ASelectablePawnMaster* PawnToRemove);

	// --------------------------- Selection UI interface
	/** Keep only units of UnitID.UnitType across all selection arrays. If the selection already contains only that type, UI remains unchanged. */
	void OnlySelectUnitsOfType(const FTrainingOption& UnitID);

	/** Remove exactly one selected unit at SelectionArrayIndex if its training option matches UnitID. Reports an error on type/index mismatch. */
	void RemoveUnitAtIndex(const FTrainingOption& UnitID, int32 SelectionArrayIndex);

	/** Cycle the Game UI hierarchy so that the primary selected type equals UnitID.UnitType (if present). Returns true on success. */
	bool TryAdvancePrimaryToUnitType(const FTrainingOption& UnitID, const int32 SelectionArrayIndex);

	/**
    * @brief Select all units of the same training option (type + subtype) as the given actor that are currently visible on screen.
    *
    * @param BasisActor Any selectable actor (squad unit, selectable pawn, or selectable actor). Its RTS training option
    *        determines which units to consider.
    *
    * @note Behavior by modifier keys:
    * @note - Shift held: add only the on-screen units of that type that aren’t already selected.
    * @note - Shift held (and ALL on-screen matches are already selected): remove those on-screen matches from the selection.
    * @note - Control held: clear current selection first, then select those on-screen matches.
    * @note - No modifier: clear current selection first, then select those on-screen matches.
    *
    * @post Ends by calling UpdateUIForSelectionAction with the proper action (hierarchy rebuild vs. selection-only).
    */
	void SelectOnScreenUnitsOfSameTypeAs(AActor* BasisActor);

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "EscapeMenu")
	void OnHitEscape();

	// Keeps  track of an array of landscape deform components to write radii to a render target.
	UPROPERTY()
	ALandscapedeformManager* M_LdfManager;

	UPROPERTY()
	TObjectPtr<UPlayerStartGameControl> PlayerStartGameControlComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AimAbility")
	TSubclassOf<APlayerAimAbility> M_PlayerAimAbilityClass;


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ControlGroups")
	void UseControlGroup(const int32 GroupIndex);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ControlGroups")
	void MoveCameraToControlGroup(const int32 GroupIndex);


	// Updates the cursor depending on the active ability set by action buttons.
	// Will reset to default cursor when no ability is active.
	void UpdateCursor();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Activate action UI button")
	void OnActionButtonShortCut(const int32 Index);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	UPlayerCommandTypeDecoder* GetCommandTypeDecoder()
	{
		if (GetIsValidCommandTypeDecoder()) { return M_CommandTypeDecoder; }
		return nullptr;
	};

	// Set in Blueprint upon keyboard action.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Camera")
	void SetModifierCameraMovementSpeed(const float NewSpeed);

	/** @return The camera pan speed from the developer settings
	 * @note used in blueprint for pan camera logic.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category = "Camera")
	float GetCameraPanSpeed();

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable, Category = "Camera")
	float GetCameraPitchLimit();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "MainUI")
	void DisplayErrorMessageCpp(const FText& ErrorMessage);

	// Determines how long the left mouse button should be held to trigger a marquee selection.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Settings")
	float TimeHoldLeftMarquee;

	UFUNCTION()
	void ClickedStartGameButton();

	/**
	 * @brief Setsup the references.
	 * @param NewMainGameUI Ref to main game UI.
	 * @param NewRTSAsyncSpawner Ref to async spawner.
	 * @param SpawnCenter
	 * @param Technologies
	 * @param HoverWidgetClass
	 * @param bDoNotLoadPlayerProfileUnits Can be set on the player spawn actor; disables loading profile units IF not using
	 * a procedural generated map.
	 * @param StartDirection
	 */
	UFUNCTION(BlueprintCallable, Category = "ReferenceCasts", NotBlueprintable)
	void InitPlayerController(UMainGameUI* NewMainGameUI,
	                          ARTSAsyncSpawner* NewRTSAsyncSpawner, const FVector& SpawnCenter,
	                          TMap<ETechnology, TSoftClassPtr<UTechnologyEffect>>
	                          Technologies, TSubclassOf<UW_HoveringActor> HoverWidgetClass,
	                          const bool bDoNotLoadPlayerProfileUnits, const EMiniMapStartDirection StartDirection);

	inline AGameUIController* GetGameUIController() const { return M_GameUIController; }

	/** @brief called on start of secondary button click*/
	UFUNCTION(BlueprintCallable)
	void SecondaryClickStart();

	/** @brief called on end secondary button click*/
	UFUNCTION(BlueprintCallable)
	void SecondaryClick();

	/** @brief called on single rightclick */
	UFUNCTION(BlueprintCallable)
	void PrimaryClick();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FPlayerRotationArrowSettings PlayerRotationArrow;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FPlayerFormationPositionEffects PlayerFormationEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "EscapeMenu")
	FPlayerEscapeMenuSettings PlayerEscapeMenuSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> M_DefaultInputMappingContext = nullptr;

	// Start Location of secondary click.
	FVector M_SecondaryStartMouseProjectedLocation;

	// The clicked actor when the secondary click started.
	TWeakObjectPtr<AActor> M_SecondaryStartClickedActor;

	bool GetSecondaryClickHitActorAndLocation(
		AActor*& OutClickedActor, FVector& OutHitLocation) const;

	void BeginPlay_SetupRotationArrow();
	void BeginPlay_InitEscapeMenuHelper();


	/**
	 * @brief When the player tabs through the action UI hierarchy.
	 * calls for Update ActionUI with the new UnitType.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void TabThroughActionUIHierarchy();

	/** @brief is the shift button held? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bIsHoldingShift;

	/** @brief is the control button held? */
	UPROPERTY(BlueprintReadWrite)
	bool bIsHoldingControl;

	/**
	 * @brief sets initial position of mouse to draw, using the HUD
	 * sets variable StartSelecting to true which initiates the HUD to fill selection-arrays with units
	 * that are inside the selection box.
	 * @note: the selection ends by releasing the left mouse button, on single LMB clicks @CPPMarqueeSelectionEnd
	 * does not execute its body because @CPPMarqueeSelectionStart is only called when the LMB is held long enough
	 */
	UFUNCTION(BlueprintCallable)
	void CPPMarqueeSelectionStart();

	/**
	 * @brief Stops Actor selection in HUD.
	 * Obtains all UMs and actors in the selection rectangle from HUD
	 * Passes on whether shift is held to determine if all currently selected units need to be deselected
	 * before the next selection arrays are build
	 * Then Unit selection is done using @ref SelectUnitsFromMarquee
	 * Rebuilds ActionUIHierarchy using @BuildActionUIHierarchy
	 * @post ActionUIHierarchy is rebuild using the newly selected Units
	 */
	UFUNCTION(BlueprintCallable)
	void CPPMarqueeSelectionEnd();

	// When the rotate left input is triggered.
	UFUNCTION(BlueprintCallable)
	void RotateLeft();

	// When the rotate right input is triggered.
	UFUNCTION(BlueprintCallable)
	void RotateRight();

private:
	// Whether the player is currently in the tech tree.
	UPROPERTY()
	bool bM_IsInTechTree;

	// An actor with a simple mesh and various materials for EPlayerAimAbilityTypes.
	// use IsPlayerAimActive to determine if the ability is active.
	UPROPERTY()
	TObjectPtr<APlayerAimAbility> M_PlayerAimAbility;
	bool bM_HasInitializedPlayerAimAbility = false;
	[[nodiscard]] bool GetIsValidPlayerAimAbilityClass() const;
	[[nodiscard]] bool GetIsValidPlayerAimAbilityActor() const;
	void UpdateAimAbilityAtCursorProjection(const float DeltaTime, const FHitResult& CursorProjection) const;

	void DetermineShowAimAbilityAtCursorProjection(const EAbilityID AbilityJustActivated, const int32 AbilitySubtype);

	void BeginPlay_SetupPlayerAimAbility();

	/**
	 * @brief Picks the best constructor among current selections, skipping excluded GUIDs.
	 *        Preference is given to the actor with the lowest queued construction ability count.
	 * @param OutFieldConstructionComp Ability component found on the chosen actor (matching ConstructionType).
	 * @param ConstructionType Which constructiontype must be supported by the chosen actor.
	 * @return The chosen actor or nullptr if none qualifies.
	 */
	AActor* GetNewFieldConstructionCandidate(UFieldConstructionAbilityComponent*& OutFieldConstructionComp,
	                                         const EFieldConstructionType ConstructionType);

	UPROPERTY()
	FViewportScreenshotTask M_ViewportScreenshotTask;

	void HandleBxpPlacementVoiceLine(const TScriptInterface<IBuildingExpansionOwner>& BxpOwner,
	                                 const EBuildingExpansionType BxpType) const;

	// Whether the player is currently in the archive.
	UPROPERTY()
	bool bM_IsInArchive;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ Primary Click ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Determines how the primary click should be handled.
	ERTSPrimaryClickContext M_PrimaryClickContext;

	void RegularPrimaryClick();

	// Clicked primary when secondary is held down, opens the formation widget.
	void PrimaryClickWhileSecondaryActive();

	// The formation widget is active and consumes the click.
	void PrimaryClickFormationWidget();

	// If we did not open the formation widget while the secondary click was active we go back to regular primary clicks.
	void OnSecondaryClickFinished_CheckPrimaryClickContext();
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ END Primary Click ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ Selection Logic Add And Remove ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

	/** @brief DO NOT CALL only through UpdateUIForSelectionAction.
	 * Whenever a new type of unit is added or a type of unit is completely removed from the selection this needs
	 * to be called to change the primary selected actor type and update the action UI.
	 */
	void RebuildGameUIHierarchy();
	/**
	 * @brief Resets the selection arrays and ensures only valid marquee units get added afterwards.
	 * @post All the selection Units have their selection decals and logic updated.
	 */
	void Internal_MarqueeFullSelectionReset_UpdateArraysAndDecals(
		const TArray<ASquadUnit*>& MarqueeSquadUnits,
		const TArray<ASelectableActorObjectsMaster*>& MarqueeSelectableActors,
		const TArray<ASelectablePawnMaster*>& MarqueeSelectablePawns);

	/**
	 * @brief Shift Adds the valid marquee units to the selection.
	 * @post The new, RTSValid, Unique units are added and have their selection logic and selection decals updated.
	 */
	ESelectionChangeAction Internal_MarqueeAddValidUniqueUnitsToSelection_UpdateArraysAndDecals(
		const TArray<ASquadUnit*>& MarqueeSquadUnits,
		const TArray<ASelectableActorObjectsMaster*>& MarqueeSelectableActors,
		const TArray<ASelectablePawnMaster*>& MarqueeSelectablePawns);

	bool Internal_IsMarqueeOnlyAlreadySelectedUnits(
		const TArray<ASquadUnit*>& MarqueeSquadUnits,
		const TArray<ASelectableActorObjectsMaster*>& MarqueeSelectableActors,
		const TArray<ASelectablePawnMaster*>& MarqueeSelectablePawn) const;

	ESelectionChangeAction Internal_RemoveMarqueeFromSelection(
		const TArray<ASquadUnit*>& MarqueeSquadUnits,
		const TArray<ASelectableActorObjectsMaster*>& MarqueeSelectableActors,
		const TArray<ASelectablePawnMaster*>& MarqueeSelectablePawn);

	bool GetIsHarvesterTankPawn(ASelectablePawnMaster* SelectedPawn) const;
	bool GetIsMarqueePawnsOnlyHarvester(const TArray<ASelectablePawnMaster*>& MarqueeSelectedPawns) const;
	bool GetHasSelectedHarvesterTank() const;
	bool GetHasNonHarvesterSelection() const;
	bool RemoveSelectedHarvesterTanks();
	bool FilterMarqueeSelection_RemoveHarvesterTanksIfMixed(
		const TArray<ASelectablePawnMaster*>& NewMarqueeSelectedPawns);

	void UpdateUIForSelectionAction(const ESelectionChangeAction Action,
	                                AActor* AddSelectedActorToPlayVoiceLine);
	void PlayActorSelectionVoiceLine(const AActor* SelectedActor) const;

	// If the player clicks a turret or a weapon of a unit we want to actually click the unit itself.
	void ChangeClickedActorIfIsChildActor(AActor*& ClickedActor) const;

	bool GetIsClickedUnitSelectable(AActor* ClickedActor) const;
	bool IsAnyUnitSelected() const;


	/**
	/** @return How the selection is changed and what that means for updating the UI.
	 */
	ESelectionChangeAction ClickUnit_GetSelectionAction(AActor* NotNullActor);
	/**
	 * @return What type of selection was preformed.
	 * @post The squad unit is removed or added to the selection depending on shift or control.
	 * or the entire selection is reset for only adding this squad unit.
	 */
	ESelectionChangeAction Click_OnSquadUnit(ASquadUnit* SquadUnit);

	/**
	 * @return What type of selection was preformed.
	 * @post The selectable pawn is removed or added to the selection depending on shift or control.
	 * or the entire selection is reset for only adding this selectable pawn.
	 */
	ESelectionChangeAction Click_OnSelectablePawn(ASelectablePawnMaster* SelectablePawn);

	/**
	 * @return How the selection is changed and what that means for updating the UI.
	 * @post The selectable actor is removed or added to the selection depending on shift or control.
	 * or the entire selection is reset for only adding this selectable actor.
	 */
	ESelectionChangeAction Click_OnSelectableActor(ASelectableActorObjectsMaster* SelectableActor);

	/** @return How the selection is changed and what that means for updating the UI.
	 * @post If removed the squad units their selection logic and decals are updated.
	 */
	ESelectionChangeAction Internal_RemoveSquadUnitFromSelection_UpdateDecals(const ASquadUnit* SquadUnitToRemove);

	/** @return How the selection is changed and what that means for updating the UI.
	 * @post If removed the pawn's selection logic and decal is updated.
	 */
	ESelectionChangeAction Internal_RemovePawnFromSelection_UpdateDecals(ASelectablePawnMaster* SelectablePawnToRemove);

	/** @return How the selection is changed and what that means for updating the UI.
	 * @post If removed the Actor's selection logic and decal is updated.
	 */
	ESelectionChangeAction Internal_RemoveActorFromSelection_UpdateDecals(
		ASelectableActorObjectsMaster* SelectableActorToRemove);

	/**
	 * @brief Tries to add the given SquadUnit and its SquadController to the selection.
	 * @param SquadUnit: the unit that is added to selection
	 * @return How the selection is changed and what that means for updating the UI.
	 * @note Does not perform UI updates on its own, only updates the selection arrays.
	 */
	ESelectionChangeAction Internal_AddSquadFromUnitToSelection_UpdateDecals(const ASquadUnit* SquadUnit);

	/**
	 * @brief Tries to add the given SelectablePawn to the selection.
	 * @param SelectablePawn: the pawn that is added to selection
	 * @return How the selection is changed and what that means for updating the UI.
	 * @note Does not perform UI updates on its own, only updates the selection arrays.
	 */
	ESelectionChangeAction Internal_AddPawnToSelection_UpdateDecals(ASelectablePawnMaster* SelectablePawn);

	/**
	 * @brief Tries to add the given selectable actor to the selection.
	 * @param SelectableActor: the Actor that is added to selection
	 * @return How the selection is changed and what that means for updating the UI.
	 * @note Does not perform UI updates on its own, only updates the selection arrays.
	 */
	ESelectionChangeAction Internal_AddActorToSelection_UpdateDecals(ASelectableActorObjectsMaster* SelectableActor);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// ++++++++++++++++++++++++ END Selection Logic Add And Remove ++++++++++++++++++++++++++++++++
	////////////////////////////////////////////////////////////////////////////////////////////////////

	ALandscapedeformManager* GetLandscapeDeformManagerFromWorld() const;

	// Set on init, by blueprint map.
	UPROPERTY()
	TMap<ETechnology, TSoftClassPtr<UTechnologyEffect>> M_TechnologyEffectsMap;

	// Used to see if the mouse was moved significantly enough to trigger a marquee selection.
	FVector M_InitMousePosGameLeftClick;

	/**
	 * @note: called after ActionButton related commands in main RC function, if there is no active ActionButton
	 * @brief: Determines if any units are selected, if so the ExecuteOrder functions are called
	 * @param ClickedActor: Actor target for commands
	 * @param ClickedLocation: Location target for commands
	 */
	void ExecuteCommandsForEachUnitSelected(AActor* ClickedActor, FVector& ClickedLocation);

	void CheckChangeCommandToMoveRallyPoint(ECommandType& OutCommand) const;

	/**
	 * @brief Propagates the command to the selected units.
	 * @param OutCommand The command type the unit needs to execute.\
	 * @param OutAbilityActivated The ability that the unit added to queue as a result of the issued command.
	 * @param OutAbilityActivated
	 * @param Target The target union containg the correctly casted clicked actor.
	 * @param ClickedLocation Where the player clicked.
	 * @param ClickedActor The raw clicked actor for VFX placements.
	 * @return How many commands were executed.
	 * @note The commandtype may change depending on what type of units are selected. e.g. if no units can pickup the
	 * clicked pickup item then we move them instead and need a move command visualisation.
	 */
	uint32 IssueCommandToSelectedUnits(
		ECommandType& OutCommand,
		EAbilityID& OutAbilityActivated,
		const FTargetUnion& Target,
		FVector& ClickedLocation, const AActor* ClickedActor);
	uint32 IssueOrderEnterCargo(AActor* CargoActor, EAbilityID& OutAbilityActivated);

	/**
	 * @brief Checks if the provided allied actor can be repaired and if so attempts to repair with selected units
	 * if that fails the units will move to that actor instead.
	 * @return Will move the units to the allied unit if it cannot be repaired.
	 * @param AlliedActor Actor owned by player.
	 * @param OutIssuedAbility will be the repair ability if the unit can be repaired and movement otherwise.
	 * @param ClickedLocation Backup move to location if allied actor is not valid.
	 * @note Does not change the ECommandType as we clicked a unit and do not want a placement effect.
	 */
	uint32 IssueOrderRepairAlly(AActor* AlliedActor, EAbilityID& OutIssuedAbility,
	                            FVector& ClickedLocation);

	uint32 IssueOrderAlliedAirfield(AActor* AlliedActor, EAbilityID& OutIssuedAbility,
	                                FVector& ClickedLocation);

	TArray<AAircraftMaster*> GetSelectedAircraft();

	uint32 IssueOrderHarvestResource(ACPPResourceMaster* Resource,
	                                 EAbilityID& OutIssuedAbility,
	                                 ECommandType& OutCommand, const FVector& ClickedLocation);
	uint32 IssueOrderPickUpItem(AItemsMaster* PickupItem,
	                            EAbilityID& OutIssuedAbility, ECommandType& OutCommand, const FVector& ClickedLocation);

	/**
	 * @brief Attempts to scavenge the clicked object, if no units can issue that order we move to the clicked location
	 * instead.
	 * @param ScavObject The scavengeable object to scavenge.
	 * @param OutIssuedAbility The ability issued to units.
	 * @param OutCommand The command type of the situation; determines the placement effect.
	 * @param ClickedLocation Backup movement location.
	 * @return How many commands were executed.
	 */
	uint32 IssueOrderScavengeItem(
		AScavengeableObject* ScavObject,
		EAbilityID& OutIssuedAbility, ECommandType& OutCommand, const FVector& ClickedLocation);

	void CreateVfxAndVoiceLineForIssuedCommand(
		const bool bResetAllPlacementEffects,
		const uint32 CommandsExecuted,
		const FVector& ClickedLocation,
		const ECommandType CommandTypeIssued,
		const EAbilityID AbilityActivated,
		const bool bForcePlayVoiceLine);


	uint32 MoveUnitsToLocation(const FVector& MoveLocation);
	uint32 OrderUnitsToAttackActor(AActor* TargetActor);
	/** @brief Orders all selected squads (only squads) to capture the given capture actor. */
	uint32 OrderUnitsCaptureActor(AActor* CaptureTargetActor);
	uint32 OrderUnitsToMoveRallyPoint(const FVector& RallyPointLocation);
	uint32 OrderUnitsAttackGround(const FVector& GroundLocation);
	void DirectActionButtonConversion(const EAbilityID ConversionAbility);

	/**
	 * @brief Rotates the selected units to the provided WorldRotation
	 * @param RotateToDirection The World rotation to align the units with.
	 * @return How many commands were exectued; how many untis added this rotation command to their queue.
	 */
	uint32 RotateUnitsToDirection(const FRotator& RotateToDirection);

	uint32 RotateUnitsToLocation(const FVector& RotateLocation);

	// Is set to true when an ActionButton is pressed that is not immediately executed
	// Then, on the next right click this ActionButton's ability (ActiveAbility) will be used to determine the command
	bool bM_IsActionButtonActive;

	/**
	 * @brief: Ability Activated by ActionButton when the button is not immediately executed,
	 * if the ability were to be immediately executed @see ExecuteActionButton
	 * This ability enum is used to determine the action on the next right click
	 * \property (ActiveAbility) test?
	 */
	EAbilityID M_ActiveAbility;

	EGrenadeAbilityType M_ActiveGrenadeAbilityType;
	EAimAbilityType M_ActiveAimAbilityType = EAimAbilityType::DefaultBrummbarFire;
	EAttachWeaponAbilitySubType M_ActiveAttachedWeaponAbilityType =
		EAttachWeaponAbilitySubType::Pz38AttachedMortarDefault;

	// Called in the main RC function, after all shift-related functions
	// Removes the unit from the selection array
	// Resets the ActionHierarchyUI if removed type is nolonger selected
	void RemoveSquadOfSquadUnitFromSelectionAndUpdateUI(const ASquadUnit* SquadUnitToRemove);


	/**
	 * @brief Fires the specific action button logic depending on the m_ActiveAbility.
	 * @param ClickedActor The actor clicked.
	 * @param ClickedLocation The location clicked.
	 * @return True if we stop action buttons after this click; action button is completed.
	 * @return False if the action button that is active needs another click [previous click was invalid for this action button]
	 * @pre All references to all selected units are valid.
	 */
	bool ExecuteActionButtonSecondClick(AActor* ClickedActor, FVector& ClickedLocation);

	/**
	 * @brief Ability from Action button, sub-function from main RightClick
	 * If shift is held, adds an attack command for each UM otherwise all TCommands are reset
	 * and this attack command becomes the first command
	 * Places a placementEffect depending on the clicked actor, if shift is held a new effect is added
	 * if shift is not held all previous effects are destroyed and only this effect is shown
	 * @param ClickedActor: The actor on the map that was right-clicked
	 * @param ClickedLocation: The location on the map that was right-clicked
	 * @pre All references to all selected units are valid.
	 */
	void ActionButtonAttack(AActor* ClickedActor, FVector& ClickedLocation);

	void ActionButtonAttackGround(const FVector& ClickedLocation);

	void CheckAttackCommandForAttackGround(ECommandType& OutCommandType) const;

	/**
	 * @brief Ability from Action button, sub-function from main RightClick
	 * If shift is held, adds an move command for each UM otherwise all TCommands are reset
	 * and this move command becomes the first command
	 * Places a placementEffect on the clicked location, if shift is held a new effect is added
	 * if shift is not held all previous effects are destroyed and only this effect is shown
	 * @param MoveLocation: The location on the map that was right-clicked
	 * @pre All references to all selected units are valid.
	 */
	void ActionButtonMove(FVector& MoveLocation);

	/**
	 * @brief Rotates the selected units to the clicked location.
	 * @param RotateLocation The world direction to rotate towards.
	 * @return To how many units the rotation command is added.
	 * @pre All references to all selected units are valid.
	 */
	void ActionButtonRotateTowards(const FVector& RotateLocation);

	void ActionButtonRepair(AActor* ClickedActor);

	/** @return Amount of issued orders*/
	int32 IssueOrderRepairToAllSelectedUnits(AActor* ClickedActor);

	/**
	 * 
	 * @param ReverseLocation
	 * @pre All references to all selected units are valid.
	 */
	void ActionButtonReverse(const FVector ReverseLocation);

	/**
	 * @brief Ability from Action button, sub-function from main RightClick
	 * If shift is held, adds an move command for each UM otherwise all TCommands are reset
	 * and this move command becomes the first command
	 * Places a placementEffect on the clicked location, if shift is held a new effect is added
	 * if shift is not held all previous effects are destroyed and only this effect is shown
	 * @param ClickedLocation: The location on the map that was right-clicked
	 * @pre All references to all selected units are valid.
	 */
	void ActionButtonPatrol(const FVector& ClickedLocation);

	void ActionButtonThrowGrenade(const FVector& ClickedLocation, const EGrenadeAbilityType GrenadeAbilityType);
	void ActionButtonAimAbility(const FVector& ClickedLocation, const EAimAbilityType AimAbilityType);
	void ActionButtonAttachedWeaponAbility(const FVector& ClickedLocation,
	                                       const EAttachWeaponAbilitySubType AttachedWeaponAbilityType);

	// Stops Movement, BT Logic, targets and TCommand
	// todo integrate with ICommands.
	void DirectActionButtonStop();

	// Commands units that can dig in to do so.
	void DirectActionButtonDigIn();

	void DirectActionButtonReinforce();

	void DirectActionButtonFieldConstruction(const EFieldConstructionType ConstructionType);

	// Some units have an UAttachedRockets component that can be used to fire rockets.
	void DirectionActionButtonFireRockets();
	void DirectActionButtonCancelRocketFire();

	void DirectActionButtonReturnToBase();
	void DirectActionButtonRetreat();
	void DirectActionButtonExitCargo();

	// Commands units that can break cover to do so.
	void DirectActionButtonBreakCover();

	void DirectActionButtonAttackGround();
	void DirectActionButtonThrowGrenade(const EGrenadeAbilityType GrenadeAbilityType);
	void DirectActionButtonCancelThrowGrenade(const EGrenadeAbilityType GrenadeAbilityType);
	void DirectActionButtonAimAbility(const EAimAbilityType AimAbilityType);
	void CreateAimAbilityRadius(AActor* Primary, const UAimAbilityComponent* AimComp);
	void HideAimAbilityRadiusIfNeeded();
	// Keeps track of the aim ability radius ID.
	int32 AimAbilityRadiusIndex = -1;
	void DirectActionButtonAttachedWeaponAbility(const EAttachWeaponAbilitySubType AttachedWeaponAbilityType);
	void CreateAttachedWeaponAbilityRadius(AActor* Primary, const UAttachedWeaponAbilityComponent* AbilityComp);
	void HideAttachedWeaponAbilityRadiusIfNeeded();
	int32 AttachedWeaponAbilityRadiusIndex = -1;
	void DirectActionButtonCancelAimAbility(const EAimAbilityType AimAbilityType);
	void DirectActionButtonBehaviourAbility(const EBehaviourAbilityType Type);

	void DirectActionButtonActivateModeAbility(const EModeAbilityType ModeType);
	void DirectActionButtonDisableModeAbility(const EModeAbilityType ModeType);

	void DirectActionButtonReturnCargo();


	// Looks for shift, if no shift this command is executed immediately
	// Does not reset any targets, only changes posture
	void DirectActionButtonProne();

	void DirectActionButtonSwitchWeapon();

	void DirectActionButtonSwitchMelee();

	/** @brief switches the weapon mode from burst to single or single to burst on all UMs controlled */
	void DirectActionButtonSwitchMode();

	void DirectActionButtonCreateBuilding();

	// The vehicle selected for building upon first clicking the building action button.
	UPROPERTY()
	ANomadicVehicle* M_NomadicVehicleSelectedForBuilding;

	// Keeps track of the unit that activated the construction preview for field construction.
	UPROPERTY()
	FFieldConstructionCandidate M_FieldConstructionCandidate;

	// The controller responsible for building the actionUI.
	UPROPERTY()
	AGameUIController* M_GameUIController;

	UPROPERTY()
	TObjectPtr<UFormationController> M_FormationController;

	// Whether there is currently an active building preview.
	// This is only the case if we activated a building ability but have not yet propagated a valid building
	// command with location to the construction vehicle.
	UPROPERTY()
	EPlayerBuildingPreviewMode M_IsBuildingPreviewModeActive;


	inline bool GetIsPreviewBuildingActive() const
	{
		return M_IsBuildingPreviewModeActive != EPlayerBuildingPreviewMode::BuildingPreviewModeOFF;
	}

	/**
	 * Stop the building expansion placement and stop building mode.
	 * @param BxpOwner If the bxp was already spawned we destroy it on the owner.
	 * else we cancel the request to spawn the bxp on the RTSAsyncSpawner.
	 * @param BxpLoadingType Whether the building expansion is packed up or not.
	 * If it is, we destroy the expansion but make sure to save the type of expansion as well as set the status to packed up.
	 * @post The building mode is stopped and the preview on the ConstructionPreview is destroyed.
	 */
	void StopBxpPreviewPlacementResetAsyncRequest(const TScriptInterface<IBuildingExpansionOwner>& BxpOwner,
	                                              const EAsyncBxpLoadingType BxpLoadingType);


	/**
	 * Tries to place the building at the clicked location.
	 * @param ClickedLocation The location to place the building at.
	 * @return True if the building was placed, false otherwise.
	 */
	bool TryPlaceBuilding(FVector& ClickedLocation);

	/**
	 * Attempts to place the bpx if it was loaded async.
	 * If the request is for a new (not packed bxp) then we first check if we can pay for it.
	 * @param InClickedLocation Where the player clicked to place the building.
	 * @return True if succesfully placed the building, false otherwise.
	 * @post If true, the M_AsyncBxpRequestState is reset.
	 * @post If false, the async state is not changed.
	 */
	bool PlaceBxpIfAsyncLoaded(FVector& InClickedLocation);

	void OnPlaceFieldConstructionAtLocation(const FVector& ValidConstructionLocation);

	bool TryPlaceFieldConstructionWithCachedCandidate(
		const FVector& ClickedLocation);

	/**
	 * @brief Finds the first available nomadic truck in selected units to convert to building at the given location.
	 * @param BuildingLocation The location to build the building at.
	 * @return Whether we found a truck for the conversion
	 * @pre Assumes that the placement location is valid.
	 * @post Either there was a valid location and an available truck was found which now has the building ability queued.
	 * Or no truck was available. In Both cases we need to cancel the preview and deactivate building preview mode
	 * as the command is propagated to a truck or cannot be executed.
	 */
	bool NomadicConvertToBuilding(const FVector& BuildingLocation);


	void OnBxpSpawnedAsync_CheckInstantPlacement_ResetAsyncRequest(
		ABuildingExpansion* SpawnedBxp,
		const TScriptInterface<IBuildingExpansionOwner>& BxpOwner,
		const FBxpOptionData& BxpConstructionRulesAndType,
		const int ExpansionSlotIndex);


	//-------------------------------------- BUILDING EXPANSION RELATED --------------------------------------//

	/**
	 * @brief Destroys the building expansion and stops preview mode.
	 * Goes through the switch of buidling modes and terminates the active one.
	 * @post M_AsyncBxpRequestState is Reset.
	 * @post Preview on the ConstructionPreview is destroyed.
	 * @post Building Mode is off.
	 */
	void StopBuildingPreviewMode();

	/**
	 * @brief Stop the preview mode of the nomadic building.
	 * @post m_NomadicVehicleSelectedForBuilding is reset.
	 * @post The preview on the ConstructionPreview is destroyed.
	 * @post The building mode is off.
	 */
	void StopNomadicPreviewPlacement();

	void StopFieldConstructionPlacement();

	/** @brief Also destroys the preview but without calling a cancel on the bxp placement.
	 * @note used in case we found a valid location to place our preview. */
	void StopPreviewAndBuildingMode(bool bIsPlacedSuccessfully = false);

	// Keeps track of the asynchronous building expansion request.
	FAsyncBxpRequestState M_AsyncBxpRequestState;

	UPROPERTY()
	UMainGameUI* M_MainGameUI;

	UPROPERTY(Transient)
	FPlayerEscapeMenuHelper M_PlayerEscapeMenuHelper;

	UPROPERTY()
	ARTSNavigator* M_RTSNavigator;

	// Reference to the async spawner
	UPROPERTY()
	ARTSAsyncSpawner* M_RTSAsyncSpawner;

	UPROPERTY()
	UPlayerResourceManager* M_PlayerResourceManager;

	UPROPERTY()
	UPlayerAudioController* M_PlayerAudioController;

	UPROPERTY()
	UPlayerPortraitManager* M_PlayerPortraitManager;

	UPROPERTY()
	UPlayerTechManager* M_PlayerTechManager;

	UPROPERTY()
	UPlayerProfileLoader* M_PlayerProfileLoader;

	UPROPERTY()
	UPlayerCameraController* M_PlayerCameraController;

	UPROPERTY()
	UPlayerCommandTypeDecoder* M_CommandTypeDecoder;

	UPROPERTY()
	UPlayerBuildRadiusManager* M_PlayerBuildRadiusManager;

	UPROPERTY()
	TObjectPtr<UPlayerControlGroupManager> M_PlayerControlGroupManager;

	UPROPERTY()
	TObjectPtr<UPlayerStartLocationManager> M_PlayerStartLocationManager;

	bool GetIsValidCommandTypeDecoder();
	bool GetIsValidPlayerResourceManager() const;
	bool GetIsValidPlayerHQ() const;
	bool GetIsValidPlayerAudioController() const;
	bool GetIsValidPlayerPortraitManager() const;
	bool GetIsValidPlayerCameraController() const;

	bool GetIsValidPlayerBuildRadiusManager() const;
	bool GetIsValidPlayerStartGameControlComponent() const;
	bool GetIsValidPlayerProfileLoader();
	bool GetIsValidGameUIController() const;
	bool GetIsValidAsyncSpawner() const;
	bool GetIsValidFormationController();
	bool GetIsValidPlayerControlGroupManager();
	bool GetIsValidPlayerTechManager();

	FNomadicPreviewAttachments GetNomadicPreviewAttachments(const ANomadicVehicle* NomadicVehicleToExpand) const;

	/**
	 * @brief Use this to start the preview on the cpp construction preview actor.
	 * @param BuildingMesh The mesh to preview.
	 * @param PreviewMode What kind of preview mode to use.
	 * @param bUseBuildRadius Whether we need a build radius.
	 * @param NomadicPreviewAttachments
	 * @note Hostlocation and BxpBuildRadius are not used for nomadic preview mode but are bxp only.
	 */
	void StartNomadicBuildingPreview(UStaticMesh* BuildingMesh,
	                                 const EPlayerBuildingPreviewMode PreviewMode,
	                                 const bool bUseBuildRadius,
	                                 const FNomadicPreviewAttachments& NomadicPreviewAttachments);

	void StartFieldConstructionPreview(
		UStaticMesh* PreviewMesh,
		const FFieldConstructionData& FieldConstructionData,
		AActor* FieldConstructionCandidate,
		UFieldConstructionAbilityComponent
		* FieldConstructionComponent);


	TArray<URadiusComp*> Get_NOMADIC_BuildRadiiAndMakeThemVisible() const;

	void PlayVoiceLineForPrimarySelected(ERTSVoiceLine VoiceLineType, const bool bForcePlay = false,
	                                     const bool bQueueIfNotPlayed = false) const;

	void StartBxpBuildingPreview(
		UStaticMesh* PreviewMesh,
		const bool bNeedBxpWithinConstructionRadius,
		const FVector& HostLocation,
		const FBxpConstructionData& BxpConstructionData);


	/** @brief Goes through the selection arrays and cleans up any invalid selections. */
	void EnsureSelectionsAreRTSValid();

	void DebugActorAndCommand(const AActor* ClickedActor, const ECommandType Command);

	/**
	 * @brief Sets the player HQ reference on the CppController and intializes the HQ DropOff reference on the
	 * player resource manager.
	 * @return Whether the HQ reference is valid.
	 */
	bool SetupPlayerHQReference();

	// // Used for a timer to retry setting the reference if we did not load the hq in time.
	// FTimerHandle M_HqReferenceHandle;

	UPROPERTY()
	TObjectPtr<ANomadicVehicle> M_PlayerHQ;

	void ShowPlayerBuildRadius(const bool bShowRadius) const;


	// Resets the current active ability and cursor.
	void DeactivateActionButton();

	void EnsureEscapeMenuHelperInitialized();
	bool TryHandleEscapeMenuBuildingModeActive();
	bool TryHandleEscapeMenuActionButtonActive();
	bool TryHandleEscapeMenuRotationArrowActive();
	[[nodiscard]] bool GetIsValidDefaultInputMappingContext() const;

	void InitFowManager();

	UPROPERTY()
	TObjectPtr<AFowManager> M_FowManager;

	bool GetIsValidFowManager() const;
	bool GetIsValidMainMenu() const;
	bool GetIsValidConstructionPreview() const;


	// ---------------------  HOVER LOGIC --------------------- //

	UPROPERTY()
	TSubclassOf<UW_HoveringActor> M_HoveringActorWidgetClass;

	UPROPERTY()
	UW_HoveringActor* M_HoveringActorWidget;

	// Tracks mouse position for hovering logic
	FVector2D M_LastFrameMousePosition;
	float M_TimeWithoutMouseMovement = 0.0f;

	// Tracks last hovered actor
	TWeakObjectPtr<AActor> LastHoveredActor;
	TWeakObjectPtr<AActor> M_CurrentHoveredActor;


	// Helper methods
	void UpdateHoveringActorInfo(float DeltaTime, const FVector2D CurrentMouseScreenPosition,
	                             const FHitResult& MouseHitResult, const bool bHit);
	void HideHoveringWidget();
	bool GetIsValidHoverWidget();
	void OnActorHovered(const AActor* HoveredActor, const bool bIsHovered) const;

	// Player Profile.
	void LoadPlayerProfile(const FVector& SpawnCenterOfUnits, const bool bDoNotLoadPlayerProfileUnits);
	/**
	 * @brief Called by UPlayerProfileLoader after all units are spawned.
	 * @note OnPlayerProfileLoaded
	 * @note OnLoadPlayerProfile
	 */
	void OnPlayerProfileLoadComplete();


	FPlayerProfileLoadingStatus M_PlayerProfileLoadingStatus;

	// Set by the CppGameState::BeginPlay when the settings are loaded.
	TWeakObjectPtr<URTSGameSettingsHandler> M_RTSGameSettingsHandler;

	/**
	 * @brief if there is a valid RTS landscape divider; if so, then we expect the starting location to begin with
	 * the player selecting one. Otherwise we use the provided spawn center as the starting location.
	 * @param bDoNotLoadPlayerProfileUnits: Set to true by the player spawn actor if we are on a map that does not
	 * want to make use of the player profile.
	 */
	void InitPlayerStartLocation(const FVector& SpawnCenter, const bool bDoNotLoadPlayerProfileUnits);
	/**
	 * @brief Moves the camera and start loading the player profile at the provided location.
	 * @param StartLocation The spawn center used for the player profile.
	 * @param bDoNotLoadPlayerProfileUnits
	 */
	void StartGameAtLocation(const FVector& StartLocation, const bool bDoNotLoadPlayerProfileUnits);

	bool GetIsGameWithChoosingStartingLocation();

	/**
	 * @brief Sets the camera to player start location with custom zoom level.
	 * @param LocationToFocus The start location to go to.
	 */
	void FocusCameraOnStartLocation(const FVector& LocationToFocus) const;

	/**
	 * @brief Sets the render targets on the mini map.
	 * @param StartDirection: Defines the orientation of the mini map.
	 * @pre Needs a valid FOW manager and Valid MainMenu
	 */
	void InitMiniMapWithFowDataOnMainMenu(const EMiniMapStartDirection StartDirection) const;

	UPROPERTY()
	FPauseGameState M_PauseGameState;

	bool PauseGame_GetIsGameLocked() const;


	void DebugPlayerSelection(const FString& Message, const FColor& Color = FColor::Blue) const;
};
