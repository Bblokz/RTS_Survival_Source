// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_InfantryWeaponUpgradeBase.generated.h"

struct FWeaponData;
enum class EWeaponName : uint8;

/**
 * @brief A base class to upgrade infantry weapons.
 * Expects a set of affected infantry units to be provided
 * as well as a set of affected weapons to upgrade the game state with.
 *
 * Provides a virtual function to apply the effect to the weapon.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_InfantryWeaponUpgradeBase : public UTechnologyEffect
{
    GENERATED_BODY()

public:
    /**
     * @brief Applies the technology effect to infantry weapons.
     * @param WorldContextObject The context object for obtaining world information.
     */
    virtual void ApplyTechnologyEffect(const UObject* WorldContextObject) override;

protected:
    /**
     * @brief Applies the effect to a valid actor (infantry unit).
     * @param ValidActor The actor to which the effect will be applied.
     */
    virtual void OnApplyEffectToActor(AActor* ValidActor) override;

    /**
     * @brief Returns the set of target actor classes (infantry units).
     * @return A set of actor classes that are the targets of this effect.
     */
    virtual TSet<TSubclassOf<AActor>> GetTargetActorClasses() const override;

    /** The set of infantry units affected by this upgrade. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSet<TSubclassOf<AActor>> M_AffectedInfantryUnits;

    /** The set of weapon names affected by this upgrade. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSet<EWeaponName> M_AffectedWeapons;

    /**
     * @brief Applies the actual technology effect to the weapon data.
     * @param ValidWeaponData A pointer to valid weapon data to be modified.
     */
    virtual void ApplyEffectToWeapon(FWeaponData* ValidWeaponData);

    /** Index of the weapon to upgrade, default is the first weapon in the infantry unit. */
    int32 ToUpgradeWeaponIndex = 0;

private:
    /** Whether the game state has been updated for the affected weapons. */
    bool bHasUpdatedGameState = false;

    /**
     * @brief Upgrades the game state for the affected weapons.
     * @param WorldContextObject The context object for obtaining world information.
     * @param OwningPlayer The player index owning the weapons to be upgraded.
     */
    void UpgradeGameStateForAffectedWeapons(UObject* WorldContextObject, const int32 OwningPlayer);
};
