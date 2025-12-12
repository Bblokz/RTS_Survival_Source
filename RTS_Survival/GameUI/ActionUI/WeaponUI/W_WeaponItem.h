#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

#include "W_WeaponItem.generated.h"

enum class EWeaponDescriptionType : uint8;
class UBorder;
class UBombComponent;
class UActionUIManager;
class UW_AmmoPicker;
class UW_WeaponDescription;
struct FWeaponData;

UCLASS()
class RTS_SURVIVAL_API UW_WeaponItem : public UUserWidget

{
	GENERATED_BODY()

public:
	void InitWeaponItem(UW_AmmoPicker* AmmoPicker,
	                    UW_WeaponDescription* WeaponDescription, UActionUIManager* ActionUIManager);
	/**
	 * @brief Sets up the weapon name of the loaded weapon state on this widget and passes the weapon state on to the
	 * weapon description widget to initialise that one.
	 * @param WeaponState The loaded weapon state to display.
	 * @post The weapon state is stored in M_LoadedWeaponState so we can change the ammo type.
	 */
	void SetupWidgetForWeapon(UWeaponState* WeaponState);
	
	/**
	 * @brief Sets up the weapon name of the loaded Bomb component on this widget and passes the BombComp on to the
	 * weapon description widget to initialise that one.
	 * @param BombComp The loaded bomb component state to display.
	 * @post The BombComponent is stored in M_LoadedBombComponent.
	 */
	void SetupWidgetForBombComponent(UBombComponent* BombComp);

	/**
	 * @brief Called when the ammo type is changed on the weapon description widget.
	 * @param NewShellType The new shell type. 
	 */
	void OnNewShellTypeSelected(const EWeaponShellType NewShellType);

protected:
	// When hovering on the weapon item this widget will display weapon data.
	UPROPERTY(BlueprintReadOnly)
	UW_WeaponDescription* M_W_WeaponDescription;

	UFUNCTION(BlueprintImplementableEvent)
	void OnSetupWidgetForWeapon(const EWeaponName WeaponName);
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnUpdateAmmoIconForWeapon(const EWeaponShellType NewShellType);

	// When the weapon item is clicked, to load the ammo picker.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnClickedWeaponItem();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnHoverWeaponItem(const bool bIsHover);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UBorder* AmmoBorder;
	
private:
	// The weapon state that this widget is displaying, if any.
	UPROPERTY()
	UWeaponState* M_LoadedWeaponState;

	// The bomb component that this widget is displaying, if any.
	UPROPERTY()
	UBombComponent* M_LoadedBombComponent;

	// The ammo picker reference, set by init at beginplay, to pass on when we click this weapon item and need
	// The ammo picker to be initialised.
	UPROPERTY()
	UW_AmmoPicker* M_AmmoPicker;

	bool EnsureIsValidWeaponDescription();
	bool EnsureIsValidAmmoBorder() const;

	void SetupAmmoIcon(EWeaponDescriptionType WeaponDescriptionType, const EWeaponShellType WeaponShellType, const int32
	                   AmountOfShellsAvailable);

	
	UPROPERTY()
	TObjectPtr<UActionUIManager> M_ActionUIManager;

	
	bool GetIsValidActionUIManager() const;
	
	
};
