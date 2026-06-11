// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_LightRadixiteArmor.generated.h"

enum class EArmorPlate : uint8;
class ATankMaster;

/**
 * @brief Adds the correct radixite armour mesh and armour value changes to matching light tank subtypes.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_LightRadixiteArmor : public UTechnologyEffect
{
	GENERATED_BODY()

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Radixite Armor")
	float ArmorValueMlt = 1.15f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Spalliners")
	float M_HealthMultiplier = 1.1;
	

private:
	UStaticMesh* GetMeshToApply(const ETankSubtype TankSubtype) const;
	void ImproveArmor(ATankMaster* ValidTank) const;
	static bool IsArmorTypeToAdjust(EArmorPlate ArmorPlate);
};
