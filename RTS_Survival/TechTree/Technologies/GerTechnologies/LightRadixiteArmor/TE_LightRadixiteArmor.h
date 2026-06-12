// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_LightRadixiteArmor.generated.h"

class ATankMaster;

/**
 * @brief Adds the correct radixite armour mesh and armour value changes to matching light tank subtypes.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_LightRadixiteArmor : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_LightRadixiteArmor();

protected:
	virtual TArray<ETankSubtype> GetTanksToApplyTo_Internal() const override;
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Radixite Armor")
	TArray<ETankSubtype> Panzer38TSubtypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Radixite Armor")
	TArray<ETankSubtype> PanzerIISubtypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Radixite Armor")
	TArray<ETankSubtype> Sdkfz140Subtypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Radixite Armor")
	UStaticMesh* Panzer38TRadixiteMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Radixite Armor")
	UStaticMesh* PanzerIIRadixiteMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Radixite Armor")
	UStaticMesh* Sdkfz140RadixiteMesh = nullptr;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Radixite Armor",
		meta = (FormerlySerializedAs = "ArmorValueMlt", AllowPrivateAccess = "true"))
	float M_ArmorValueMultiplier;

	UStaticMesh* GetMeshToApply(const ETankSubtype TankSubtype) const;
};
