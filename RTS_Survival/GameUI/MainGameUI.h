// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameUI_InitData.h"
#include "ActionUI/ActionUIContainer/FActionUIContainer.h"
#include "Blueprint/UserWidget.h"
#include "BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "Components/CanvasPanel.h"
#include "Portrait/PortraitWidget/W_Portrait.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "Resources/PlayerEnergyBar/W_PlayerEnergyBar.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Game/GameState/HideGameUI/RTSUIElement.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/Player/GameInitCallbacks/OnArchiveLoadedCallBacks/OnArchiveLoadedCallbacks.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "Templates/UniquePtr.h"
#include "TrainingUI/TrainingMenuManager.h"
#include "MainGameUI.generated.h"

struct FSelectedUnitsWidgetState;
struct FInit_ActionUI;
struct FInit_WeaponUI;
class UW_BottomCenterUI;
class UW_ArchiveNotificationHolder;
class UW_Archive;
enum class EMiniMapStartDirection : uint8;
class AFowManager;
class UW_MiniMap;
class UW_ControlGroups;
class UW_BxpDescription;
class UTechTree;
class UW_SelectedUnitInfo;
class UW_ItemActionUI;
class UW_AmmoPicker;
class UW_AmmoButton;
class UW_WeaponItem;
class UActionUIManager;
class UW_PlayerResource;
class UTrainerComponent;
class RTS_SURVIVAL_API UW_TrainingItem;
struct RTS_SURVIVAL_API FBuildingExpansionItem;
enum class EBuildingExpansionStatus : uint8;
enum class EBuildingExpansionType : uint8;
enum class EShowBottomRightUIPanel : uint8;
class RTS_SURVIVAL_API IBuildingExpansionOwner;
class RTS_SURVIVAL_API ACPPController;
// A widget representing an expansion of the building.
class RTS_SURVIVAL_API UW_ItemBuildingExpansion;
// A widget representing an option for a building expansion.
class RTS_SURVIVAL_API UW_OptionBuildingExpansion;

UENUM(BlueprintType)
enum class EActionUINomadicButton : uint8
{
	// Set display name metadata for each enum value to remove the prefix
	EAUI_ShowConvertToBuilding UMETA(DisplayName = "Show Convert To Building"),

	EAUI_ShowCancelBuilding UMETA(DisplayName = "Show Cancel Conver To  Building"),

	EAUI_ShowConvertToVehicle UMETA(DisplayName = "Show Convert To Vehicle"),

	EAUI_ShowCancelVehicleConversion UMETA(DisplayName = "Show Cancel Convert To Vehicle")
};


/**
 * To provide specific state of the primary unit selected and propagate it to the GameUI.
 */
USTRUCT(BlueprintType)
struct FActionUIParameters
{
	GENERATED_BODY()

	FActionUIParameters()
		: bShowBuildingUI(false),
		  NomadicButtonState(EActionUINomadicButton::EAUI_ShowConvertToBuilding),
		  bShowTrainingUI(false),
		  bShowWeaponUI(false), bShowActionUIAbilities(false),
		  NomadicSubtype(ENomadicSubtype::Nomadic_None),
		  TankSubtype(ETankSubtype::Tank_None),
		  SquadSubtype(ESquadSubtype::Squad_None),
		  BxpSubtype(EBuildingExpansionType::BXT_Invalid),
		  AircraftSubType(EAircraftSubtype::Aircarft_None)

	{
	}

	FActionUIParameters(const bool bBShowBuildingUI,
	                    const EActionUINomadicButton NomadicButtonState,
	                    const bool bBShowTrainingUI,
	                    const bool bBShowWeaponUI,
	                    const ENomadicSubtype NomadicSubtype,
	                    const ETankSubtype TankSubtype,
	                    const ESquadSubtype SquadSubtype,
	                    const EBuildingExpansionType BxpSubtype,
	                    const EAircraftSubtype AircraftSubtype)
		: bShowBuildingUI(bBShowBuildingUI),
		  NomadicButtonState(NomadicButtonState),
		  bShowTrainingUI(bBShowTrainingUI),
		  bShowWeaponUI(bBShowWeaponUI), bShowActionUIAbilities(false),
		  NomadicSubtype(NomadicSubtype),
		  TankSubtype(TankSubtype),
		  SquadSubtype(SquadSubtype),
		  BxpSubtype(BxpSubtype),
		  AircraftSubType(AircraftSubtype)
	{
	}

	UPROPERTY(BlueprintReadOnly)
	bool bShowBuildingUI;

	UPROPERTY(BlueprintReadOnly)
	EActionUINomadicButton NomadicButtonState;

	UPROPERTY(BLueprintReadOnly)
	bool bShowTrainingUI;

	UPROPERTY(BlueprintReadOnly)
	bool bShowWeaponUI;

	UPROPERTY(BlueprintReadOnly)
	bool bShowActionUIAbilities;

	UPROPERTY(BlueprintReadOnly)
	ENomadicSubtype NomadicSubtype;

	UPROPERTY(BlueprintReadOnly)
	ETankSubtype TankSubtype;

	UPROPERTY(BlueprintReadOnly)
	ESquadSubtype SquadSubtype;

	UPROPERTY(BlueprintReadOnly)
	EBuildingExpansionType BxpSubtype;

	UPROPERTY(BlueprintReadOnly)
	EAircraftSubtype AircraftSubType;
};


/**
 * @brief Handles top-level game UI including action, training, and minimap widgets.
 * @note UpdateActionUIForNewUnit: call from selection logic to refresh widgets for the current primary unit.
 */
UCLASS()
class RTS_SURVIVAL_API UMainGameUI : public UUserWidget, public IRTSUIElement
{
	GENERATED_BODY()

	// To access the function to set the bottom UI panel to bxp description widget. 
	friend class RTS_SURVIVAL_API UW_OptionBuildingExpansion;

	// To access the function to set the bottom UI panel to training description widget.
	friend class RTS_SURVIVAL_API UW_ItemBuildingExpansion;

	// To access the primary selected actor.
	friend class RTS_SURVIVAL_API UW_BottomCenterUI;

	// To access the function to set the bottom ui panel to training description widget. 
	friend class RTS_SURVIVAL_API UTrainingMenuManager;

	friend class RTS_SURVIVAL_API UTechTree;
	// to access archive.
	friend struct FOnArchiveLoadedCallbacks;

public:
	void OpenTechTree();
	void SetMainMenuVisiblity(const bool bVisible);
	void SetMissionManagerWidget(UUserWidget* MissionManagerWidget);
	/** @reutrn Whether the left click was consumed to close an open option menu. */
	bool CLoseOptionUIOnLeftClick();

	/** @brief Rebuild Selection UI from flattened tile-states and page index. */
	void RebuildSelectionUI(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates, int32 ActivePageIndex) const;

	virtual void OnHideAllGameUI(const bool bHide) override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void AddNewArchiveItem(const ERTSArchiveItem ItemType, const FTrainingOption OptionalUnit = FTrainingOption(),
	                       const int32 SortingPriority = 0, const float NotificationTime = 0);

	void OnOpenArchive();
	void OnCloseArchive();

	void OnHoverWeaponItem(const bool bIsHovering);
	void OnHoverActionUIItem(const bool bIsHovering);
	void OnHoverSelectedUnitInfo(const bool bIsHovering);

	TObjectPtr<UW_MiniMap> GetIsValidMiniMap() const;

	// Allows saving callbacks to when the soft archive pointer is loaded. Will exe immediately if already loaded.
	FOnArchiveLoadedCallbacks OnArchiveLoadedCallbacks;

	/**
	 * @brief Checks if the requesting actor is the primary selected actor, if so the cancel building button is shown.
	 * @param RequestingActor The actor that requested the cancel building icon to be shown.
	 * @post If the actor is primary; the CancelBuilding button is shown instead of the construct building button.
	 */
	void RequestShowCancelBuilding(const AActor* RequestingActor) const;

	/**
	 * @brief Checks if the requesting actor is the primary selected actor,
	 * only then the construct building button is shown.
	 * @param RequestingActor The actor that requested the construct building icon to be shown.
	 * @post If the actor is primary; the construct building button is shown.
	 */
	void RequestShowConstructBuilding(const AActor* RequestingActor) const;

	/**
	 * @brief Checks if the requesting actor is the primary selected actor,
	 * if so then the convert to vehicle button is shown.
	 * @param RequestingActor The actor requesting the UI to change.
	 */
	void RequestShowConvertToVehicle(const AActor* RequestingActor) const;

	/**
	 * @brief Checks if the requesting actor is the primary selected actor, if so the cancel convert to vehicle button is shown.
	 * @param RequestingActor The actor that requested the cancel vehicle conversion icon to be shown.
	 * @post The CancelBuilding button is shown instead of the construct building button.
	 */
	void RequestShowCancelVehicleConversion(const AActor* RequestingActor) const;

	/**
	 * @brief updates the ui depending on the trucks's state.
	 * @param ConvertedTruck The truck that just converted.
	 * @param bConvertedToBuilding Whether the truck was converted to a building or not.
	 */
	void OnTruckConverted(
		AActor* ConvertedTruck,
		const bool bConvertedToBuilding);

	/**
	 * Update the specific building expansion item in the UI if the requesting actor is the primary selected actor.
	 * @param RequestingActor The actor that requested the UI to be updated.
	 * @param BuildingExpansionType The (possibly) new type of the building expansion button.
	 * @param BuildingExpansionStatus The new status of the building expansion button.
	 * @param ConstructionRules Determines how the Bxp was built; is used to place a packed expansion.
	 * @param IndexExpansionSlot The index of the building expansion slot to update.
	 */
	void RequestUpdateSpecificBuildingExpansionItem(
		AActor* RequestingActor,
		const EBuildingExpansionType BuildingExpansionType,
		const EBuildingExpansionStatus BuildingExpansionStatus,
		const FBxpConstructionRules& ConstructionRules, const int IndexExpansionSlot);

	/**
	 * @brief Sets up the item and option building expansion UI for the provided owner.
	 * @param BuildingExpansionOwner The owner that has an array of building expansions and unlocked building expansion types.
	 */
	void InitBuildingExpansionUIForNewUnit(const IBuildingExpansionOwner* BuildingExpansionOwner);


	/**
	 * @brief Initializes the training UI with the provided trainer.
	 * @param Trainer The primary selected trainer.
	 * @param bIsAbleToTrain If false we only set the refernce to this trainer so that when it is able the UI will
	 * be updated.
	 */
	void SetupTrainingUIForNewTrainer(UTrainerComponent* Trainer, const bool bIsAbleToTrain);

	/**
	 * @brief Initialises the weapon UI for the provided actor.
	 * @param PrimarySelected The actor for which we check what weapon UI needs to be initialised.
	 * @return Whether any weapon UI element was filled. Will be false if the actor has no weapons.
	 */
	bool SetupWeaponUIForSelected(AActor* PrimarySelected);

	/**
	 * @brief Setup the action UI elements with the provided abilities.
	 * @param TAbilities The abilities for the primary selected unit.
	 * @param PrimaryUnitType The primary selected unit's type.
	 * @param NomadicSubtype The nomadic subtype of the primary selected unit, is set to Nomadic_None if not a nomadic unit.
	 * @param PrmarySelected The primary selected actor.
	 * @param TankSubtype The tank subtype of the primary selected unit, is set to Tank_None if not a tank.
	 * @param SquadSubtype
	 * @param BxpSubtype
	 * @return Whether any valid ability was initialised in the action UI elements, if not we need
	 * to hide the action UI.
	 */
	bool SetupUnitActionUIForUnit(
		const TArray<FUnitAbilityEntry>& TAbilities,
		EAllUnitType PrimaryUnitType,
		AActor* PrimarySelected,
		const ENomadicSubtype NomadicSubtype,
		const ETankSubtype TankSubtype, const ESquadSubtype SquadSubtype,
		const EBuildingExpansionType BxpSubtype) const;


	/**
	 * @brief Uses the provided action UI parameters to determine which UI elements to show.
	 * Also sets the primary selected actor reference.
	 * @param ActionUIParameters To determine which elements to show.
	 * @param NewPrimarySelectedActor The selected actor that the UI is based on.
	 * @note COSMETIC ONLY; assumes setup data is already provided to the action UI elements.
	 */
	void UpdateActionUIForNewUnit(const FActionUIParameters& ActionUIParameters,
	                              const TObjectPtr<AActor>& NewPrimarySelectedActor);

	UTrainingMenuManager* GetTrainingMenuManager() const;

	/**
	 * @brief Init the minimap with the render target references from the fow manager.
	 * @param FowManager The manager to provide the render targets.
	 * @param StartDirection The direction according to which we want the minimap to be turned.
	 */
	void InitMiniMap(const TObjectPtr<AFowManager>& FowManager, const EMiniMapStartDirection StartDirection);

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintCallable, Category = "TechTree")
	void LoadTechTree();

	void OnTechTreeClassLoaded();

	void AddTechTreeToViewport(UTechTree* TechTreeWidget);

	void OnCloseTechTree();

	UPROPERTY(meta=(BindWidget))
	UCanvasPanel* MainUICanvas;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TechTree")
	TSoftClassPtr<UTechTree> TechTreeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Archive")
	TSoftClassPtr<UW_Archive> ArchiveClass;

	void LoadArchiveMenu();
	void OnArchiveClassLoaded();
	void AddArchiveToViewPort(UW_Archive* ArchiveWidget);


	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitMainGameUI(
		FActionUIContainer ActionUIContainerWidgets,
		FInit_ActionUI ActionUIWidgets,
		FInit_WeaponUI WeaponUIWidgets,
		UW_BxpDescription* NewBxpDescriptionWidget,
		ACPPController* NewPlayerController,
		UBorder* NewTrainingUIBorder,
		const TArray<UW_TrainingItem*> NewTrainingItemWidgets,
		UW_TrainingRequirement* TrainingRequirementWidget,
		TArray<UW_PlayerResource*> NewPlayerResourceWidgets,
		UW_PlayerEnergyBar* NewPlayerEnergyBar,
		UW_EnergyBarInfo* NewPlayerEnergyBarInfo,
		UW_TrainingDescription* NewTrainingDescription,
		UW_ControlGroups* NewControlGroups,
		UW_ArchiveNotificationHolder* NewArchiveNotificiationHolder, UW_BottomCenterUI* NewBottomCenterUI, UW_Portrait*
		NewPortrait, FInit_BehaviourUI BehaviourUIWidgets);

	// The primary selected actor in the game.
	UPROPERTY(BlueprintReadOnly, Category = "Primary Selection")
	AActor* PrimarySelectedActor;

	/**
	 * @brief Shows the action UI, bxp descrption or the training description in the bottom right panel of the game UI.
	 * Also has an option to show none of them.
	 * @param NewBottomRightUIPanel What the new panel will be.
	 * @param BxpType Needed if set to display bxp description; the type of building expansion to display.
	 * @param bIsBuilt Needed if set to display bxp description; whether the bxp is built or not.
	 * @param TrainingWidgetState Needed if set to display training description; the training option to display.
	 */
	void SetBottomUIPanel(
		const EShowBottomRightUIPanel NewBottomRightUIPanel,
		const EBuildingExpansionType BxpType = EBuildingExpansionType::BXT_Invalid,
		const bool bIsBuilt = false,
		const FTrainingWidgetState TrainingWidgetState = FTrainingWidgetState());

private:
	void InitMainGameUI_InitActionAndWeaponUI(
		const FActionUIContainer& ActionUIContainerWidgets,
		const FInit_WeaponUI& WeaponUIWidgets,
		const FInit_ActionUI& ActionUIWidgets, const FInit_BehaviourUI& BehaviourUIWidgets,
		ACPPController* PlayerController);

	void InitMainGameUI_InitBuildingUI(
		UW_BottomCenterUI* NewBottomCenterUI,
		ACPPController* PlayerController, UW_BxpDescription* BxpDescriptionWidget);

	// Hides all training UI, building UI and action UI.
	void InitMainGameUI_HideWidgets();
	void SetTrainingUIVisibility(const bool bVisible);
	void OnHoverInfoWidget_HandleTrainingUI(const bool bIsHovering);
	bool GetIsTrainingUIVisible() const;
	// Contains all the building expansion widgets in the building UI primary scroll box.
	UPROPERTY()
	TArray<TObjectPtr<UW_ItemBuildingExpansion>> M_TItemBuildingExpansionWidgets;

	// Contains all the building expansion option widgets in the building UI "options scroll box."
	UPROPERTY()
	TArray<TObjectPtr<UW_OptionBuildingExpansion>> M_TOptionsBuildingExpansionWidgets;


	void UpdateBuildingUIForNewUnit(
		const bool bShowBuildingUI,
		const EActionUINomadicButton NomadicButtonState,
		const ENomadicSubtype NomadicSubtype);

	UPROPERTY()
	TObjectPtr<UW_BottomCenterUI> M_BottomCenterUI;

	bool GetIsValidBottomCenterUI() const;

	TScriptInterface<IBuildingExpansionOwner> GetPrimaryAsBxpOwner() const;

	UPROPERTY()
	TObjectPtr<ACPPController> M_PlayerController;

	bool GetIsValidPlayerController() const;

	UPROPERTY()
	bool bWasHiddenByAllGameUI = false;

	UPROPERTY()
	UTechTree* M_TechTree;

	// Contains all missions.
	TWeakObjectPtr<UUserWidget> M_MissionManagerWidget;

	UPROPERTY()
	TObjectPtr<UW_Archive> M_Archive;
	bool GetIsValidArchive() const;

	UPROPERTY()
	TObjectPtr<UW_ArchiveNotificationHolder> M_ArchiveNotificationHolder;
	bool GetIsValidArchiveNotificationHolder() const;
	/**
	 * Given the building expansion items, sets up the building expansion items in the UI.
	 * @note If there are more items than the UI can display, the rest is popped from the arrays.
	 * @param TBuildingExpansions The bxps of the primary selected actor.
	 * @param bAreBxpItemsEnabled Whether the building expansion items are enabled or not; depending on if the bxp owner
	 * is able to expand.
	 */
	void SetupBuildingExpansionItems(
		TArray<FBuildingExpansionItem>& TBuildingExpansions,
		const bool bAreBxpItemsEnabled);


	/**
	 * @brief Sets up the building expansion options for the building expansion UI with the correct types.
	 * @param TOptionsDataForBuildingExpansion The options to set up for the building expansion UI.
	 * @param BxpOwner
	 * @pre The options are provided by the primary selected actor and can all be built
	 * @pre Check for uniques before calling this!
	 * @post The options' images and text etc as well as their visibility are set up in the UI.
	 */
	void SetupBuildingExpansionOptions(TArray<FBxpOptionData>& TOptionsDataForBuildingExpansion,
	                                   const IBuildingExpansionOwner* BxpOwner);

	/**
	 * Only called by friend class building expansion item widget.
	 *
	 * @param BuildingExpansionType The type of the building expansion item clicked.
	 * @param BuildingExpansionStatus The construction/building state of the expansion.
	 * @param ConstructionRules
	 * @param IndexItemExpansionSlot The index of the building expansion item clicked.
	 */
	void ClickedItemBuildingExpansion(
		const EBuildingExpansionType BuildingExpansionType,
		const EBuildingExpansionStatus BuildingExpansionStatus,
		const FBxpConstructionRules& ConstructionRules, const int IndexItemExpansionSlot);

	/**
	 * Only called by friend class building expansion option widget.
	 * 
	 * @param BxpOptionConstructionAndTypeData The type of the building expansion option clicked.
	 * @param IndexOptionExpansionSlot The index
	 */
	void ClickedOptionBuildingExpansion(
		const FBxpOptionData& BxpOptionConstructionAndTypeData,
		const int IndexOptionExpansionSlot);

	/**
	 * @brief Casts the primary selected actor to find out if it is able to expand.
	 * @return Whether we can expand for the primary selected actor.
	 */
	bool GetIsPrimarySelectedAbleToExpand() const;

	/** @return True if the provided actor is the actor for which the UI is set up. */
	bool GetIsProvidedActorPrimarySelected(const AActor* RequestingActor) const;


	/**
	 * @brief Called when the cancel building expansion button is clicked on the W_ItemBuildingExpansion.
	 * Stops the construction of the building expansion and destroys the building expansion.
	 * @param IndexItemExpansionSlot The index of the building expansion item.
	 * @param bIsCancelledPackedBxp Whether the building expansion is packed up or not.
	 * @note if the bxp is packed up we set the status back to packed up and make sure to save the type.
	 */
	void CancelBuildingExpansionConstruction(
		const int IndexItemExpansionSlot,
		const bool bIsCancelledPackedBxp) const;

	void MoveCameraToBuildingExpansion(const int IndexExpansionSlot) const;

	/**
	 * If the bxp is not packed: Stops the preview in the controller
	 * and destroys the building expansion currently looking for placement.
	 * If the bxp is packed: Stops the preview in the controller and destroys the building expansion but makes sure the state
	 * is set to packed up.
	 * @param bIsCancelledPackedBxp Whether the bxp is packed up or not.
	 * @param BxpType
	 */
	void CancelBuildingExpansionPlacement(const bool bIsCancelledPackedBxp, const EBuildingExpansionType BxpType);

	/**
	 * @brief Opens the option UI for the clicked building expansion item.
	 * @param ActiveItemIndex The index of the building expansion item that was clicked.
	 */
	void OpenOptionUI(const int ActiveItemIndex);

	/**
	 * @brief Closes the option UI in bp and resets the index.
	 * @note Call this and not HideBXP_OptionUI alone as that does not reset the index!!
	 */
	UFUNCTION(BlueprintCallable)
	void CloseOptionUI();

	int M_ActiveBxpItemForBxpOptionUI_Index = -1;

	void PlacePackedUpBuildingExpansion(
		const int IndexExpansionSlot,
		const EBuildingExpansionType BuildingExpansionType, const FBxpConstructionRules& ConstructionRules) const;

	// Keeps track of the training UI widgets and the current queue for primary selected actor.
	UPROPERTY()
	TObjectPtr<UTrainingMenuManager> M_TrainingMenuManager;
	bool GetIsValidTrainingMenuManager() const;

	bool bM_WasTrainingUIHiddenDueToHoverInfoWidget = false;
	UPROPERTY()
	UBorder* M_TrainingUIBorder = nullptr;
	bool GetIsValidTrainingUIBorder() const;

	// Keeps track of the action ui buttons, the unit's image + description
	// and unit weapons.
	UPROPERTY()
	TObjectPtr<UActionUIManager> M_ActionUIManager;

	bool GetIsValidActionUIManager() const;


	// Initialises the resource widgets from left to right.
	void SetupResources(
		TArray<UW_PlayerResource*>& TResourceWidgets,
		const ACPPController* NewPlayerController, UW_PlayerEnergyBar* NewPlayerEnergyBar, UW_EnergyBarInfo*
		NewPlayerEnergyBarInfo) const;

	// The Bxp description widget that is shown when hovering over a building expansion option.
	UPROPERTY()
	TObjectPtr<UW_BxpDescription> M_BxpDescription;

	// The Training description widget that is shown when hovering over a training item.
	UPROPERTY()
	TObjectPtr<UW_TrainingDescription> M_TrainingDescription;

	UPROPERTY()
	EShowBottomRightUIPanel M_BottomRightUIPanel;

	UPROPERTY()
	TObjectPtr<UW_ControlGroups> M_ControlGroups;

	void PropagateControlGroupsToPlayer(UW_ControlGroups* ControlGroupsWidget) const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UW_MiniMap> M_MiniMap;

	bool EnsureMiniMapIsValid() const;
};
