#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanel.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIContainer/FActionUIContainer.h"
#include "RTS_Survival/GameUI/ActionUI/WeaponUI/OnHoverDescription/W_WeaponDescription.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "UObject/Object.h"

#include "ActionUIManager.generated.h"

class UW_SelectedUnitDescription;
enum class EWeaponShellType : uint8;
enum class EVeterancyIconSet : uint8;
class ATankMaster;
enum class ESquadSubtype : uint8;
class UW_AmmoButton;
class UW_AmmoPicker;
class UWeaponState;
class UHealthComponent;
enum class ENomadicSubtype : uint8;
enum class ETankSubtype : uint8;
class UW_SelectedUnitInfo;
class ACPPController;
enum class EAllUnitType : uint8;
enum class EAbilityID : uint8;
class UW_ItemActionUI;
struct FWeaponData;
class UCanvasPanel;
class UMainGameUI;
class UW_WeaponItem;

UCLASS()
class RTS_SURVIVAL_API UActionUIManager : public UObject
{
	GENERATED_BODY()

public:
	void InitActionUIManager(
		TArray<UW_WeaponItem*> TWeaponUIItemsInMenu,
		UMainGameUI* MainGameUI,
		const TArray<UW_ItemActionUI*>& ActionUIElementsInMenu,
		ACPPController* PlayerController,
		UW_SelectedUnitInfo* SelectedUnitInfo,
		const TObjectPtr<UW_AmmoPicker>& AmmoPicker, UW_WeaponDescription* WeaponDescription, FActionUIContainer
		ActionUIContainerWidgets, UW_SelectedUnitDescription* SelectedUnitDescriptionWidget, UUserWidget*
		ActionUIDescriptionWidget);

	void SetActionUIVisibility(const bool bShowActionUI) const;
	void HideAllHoverInfoWidgets() const;
	void HideAmmoPicker() const;

	void OnHoverActionUIItem(const bool bIsHover) const;
	void OnHoverSelectedUnitInfo(const bool bIsHover)const;
	void OnHoverWeaponItem(const bool bIsHover) const;

	void OnShellTypeSelected(const EWeaponShellType SelectedShellType) const;

	void RequestUpdateAbilityUIForPrimary(ICommands* RequestingUnit);

	/** @brief Changes the visiblity of all the weapon UI elements depending on the provided
	 * visibilty setting.
	 * @param bVisible: Slate visbility setting
	 */
	void SetWeaponUIVisibility(const bool bVisible);

	/**
	 * @brief Propagates the weapon data of the primary selected actor to the weapon UI elements.
	 * @param SelectedActor The primary selected actor that possibly carries weapons.
	 * @return Whether any Weapon UI element was filled.
	 */
	bool SetupWeaponUIForSelectedActor(AActor* SelectedActor);

	/**
	 * @brief Init the action UI abilities for the selected unit using the provided array of abilities.
	 * The slate will be setup in the derived blueprints of the action ui element widgets.
	 * @param TAbilities The array of abilities for the primary selected unit.
	 * @param PrimaryUnitType The type of the primary selected unit.
	 * @param NomadicSubtype The subtype of the primary selected unit, set to Nomadic_None if not a nomadic unit.
	 * @param TankSubtype The subtype of the primary selected unit, set to Tank_None if not a tank.
	 * @param SquadSubtype
	 * @param BxpSubtype
	 * @param SelectedActor The primary selected actor.
	 * @return Whether there was any valid ability that needed to be initialised.
	 */
	bool SetUpActionUIForSelectedActor(const TArray<EAbilityID>& TAbilities,
	                                   const EAllUnitType PrimaryUnitType,
	                                   const ENomadicSubtype NomadicSubtype,
	                                   const ETankSubtype TankSubtype,
	                                   const ESquadSubtype SquadSubtype, const EBuildingExpansionType BxpSubtype, AActor* SelectedActor);

	void UpdateHealthBar(const float NewPercentage, const float MaxHp, const float CurrentHp) const;

	void UpdateExperienceBar(const float ExperiencePercentage, const int32 CumulativeExp, const int32 ExpNeededForNextLevel, const int32
	                         CurrentLevel, const int32 MaxLevel, const EVeterancyIconSet VeterancyIconSet) const;

private:
	UPROPERTY()
	UW_SelectedUnitInfo* M_SelectedUnitInfo;
	bool GetIsValidSelectedUnitInfo() const;

	UPROPERTY()
	TObjectPtr<UMainGameUI> M_MainGameUI;
	bool GetIsValidMainGameUI() const;


	// Container with all weapon UI items.
	UPROPERTY()
	TArray<TObjectPtr<UW_WeaponItem>> M_TWeaponItems;

	UPROPERTY()
	TObjectPtr<UW_WeaponDescription> M_WeaponDescription;
	bool GetIsValidWeaponDescription() const;
	void SetWeaponDescriptionVisibility(const bool bVisible) const;

	UPROPERTY()
	TObjectPtr<UW_SelectedUnitDescription> M_SelectedUnitDescription;
	bool GetIsValidSelectedUnitDescription() const;
	void SetSelectedUnitDescriptionVisibility(const bool bVisible) const;

	UPROPERTY()
	TObjectPtr<UUserWidget> M_ActionUIDescriptionWidget;
	bool GetIsValidActionUIDescriptionWidget() const;
	void SetActionUIDescriptionWidgetVisibility(const bool bVisible) const;

	UPROPERTY()
	UW_AmmoPicker* M_AmmoPicker;
	bool GetIsValidAmmoPicker() const;

	TWeakObjectPtr<AActor> M_LastSelectedActor;
	bool GetIsValidLastSelectedActor() const;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	bool GetIsValidPlayerController() const;

	UPROPERTY()
	TArray<UW_ItemActionUI*> M_TActionUI_Items;

	UPROPERTY()
	UHealthComponent* M_SelectedHealthComponent;

	UPROPERTY()
	URTSExperienceComp* M_SelectedExperienceComponent;

	// The container of the border and horizontal box in which the whole action UI (Unit info, weapons, action buttons) is placed.
	// Used to show / collapse the action UI.
	FActionUIContainer M_ActionUIContainer;
	

	/**
	 * @brief Propagates weapon data to respective UI elements.
	 * @param WeaponStates The weapon state Uobjects of the primary unit. 
	 * the order determines the order in which the UI weapon elements are filled.
	 * @param BombComponent
	 * @return Whether any weapon UI element was filled.
	 */
	bool PropagateWeaponDataToUI(TArray<UWeaponState*> WeaponStates,
		UBombComponent* BombComponent = nullptr);

	inline bool GetIndexInAbilityItemRange(const int32& Index) const
	{
		return Index >= 0 && Index < M_TActionUI_Items.Num();
	}

	/**
	 * @brief Determines whether the unit uses any of the action buttons.
	 * @param TAbilities The abilities for the unit.
	 * @return Whether the selcted unit has any action button ability that is not none.
	 */
	inline bool ContainsAnyValidAbility(const TArray<EAbilityID>& TAbilities) const;

	/**
	 * 
	 * @param PrimarySelectedActor The primary selected actor.
	 * @param OutMaxHp The max health of the primary selected actor.
	 * @param OutCurrentHp The current health of the primary selected actor.
	 * @return THhe health percentage of the primary selected actor in 0 - 100 range.
	 */
	float SetupHealthComponent(const AActor* PrimarySelectedActor, float& OutMaxHp, float& OutCurrentHp);

	/**
     * Searches for the experience component on the provided actor and, if found,
     * registers it with the UI manager so that it can update the UI.
     * @param PrimarySelectedActor The currently selected actor.
     */
	void SetupExperienceComponent(const AActor* PrimarySelectedActor);

	void SetAmmoPickerVisiblity(const bool bVisible) const;

	TArray<UWeaponState*> GetWeaponsOfSquad(ASquadController* SquadController) const;

	bool UpdateAbilitiesUI(const TArray<EAbilityID>& InAbilitiesOfPrimary);

	
	// Set to the primary selected unit's interface to notify it is primary selected which allows
	// ICommands to update the UI when abilities are changed.
	TWeakInterfacePtr<ICommands> M_PrimarySelectedUnit;
	void RegisterPrimarySelected(AActor* NewPrimarySelected);

	bool GetIsCurrentPrimarySelectedValid()const;
};
