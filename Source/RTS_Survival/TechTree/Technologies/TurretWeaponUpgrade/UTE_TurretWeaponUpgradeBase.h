#pragma once
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"

#include "UTE_TurretWeaponUpgradeBase.generated.h"

struct FWeaponData;
enum class EWeaponName : uint8;

/**
 * @brief A base class to upgrade turrets and their weapons.
 * Expects a set of affected turrets to be provided
 * as well as a set of affected weapons to upgrade the game state with.
 *
 * provides a virtual function to apply the effect to the weapon.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_TurretWeaponUpgradeBase : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	virtual void ApplyTechnologyEffect(const UObject* WorldContextObject) override;

protected:
	virtual void OnApplyEffectToActor(AActor* ValidActor) override;

	virtual TSet<TSubclassOf<AActor>> GetTargetActorClasses() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSet<TSubclassOf<AActor>> M_AffectedTurrets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSet<EWeaponName> M_AffectedWeapons;

	// This function applies the actual technology effect.
	virtual void ApplyEffectToWeapon(FWeaponData* ValidWeaponData);

	// Default is the fist weapon in the turret.
	int ToUpgradeWeaponIndex  = 0;

private:
	bool bHasUpdatedGameState = false;

	void UpgradeGameStateForAffectedWeapons(UObject* WorldContextObject, const int32 OwningPlayer);
};
