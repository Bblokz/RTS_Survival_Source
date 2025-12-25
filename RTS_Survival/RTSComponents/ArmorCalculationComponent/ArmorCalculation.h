// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Resistances/Resistances.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

#include "ArmorCalculation.generated.h"


USTRUCT(BlueprintType)
struct FArmorSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FTransform ArmorBoxTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FBox ArmorBox = FBox(FVector(-50.0f, -50.0f, -50.0f), FVector(50.0f, 50.0f, 50.0f));

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	float ArmorValue = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	EArmorPlate ArmorType = EArmorPlate::Plate_Front;
};

using DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh;

USTRUCT()
struct FArmorSetup
{
	GENERATED_BODY()

	FArmorSetup()
	{
		for (int i = 0; i < MaxArmorPlatesPerMesh; i++)
		{
			ArmorSettings0[i].ArmorValue = 0.0f;
			ArmorSettings1[i].ArmorValue = 0.0f;
			ArmorSettings2[i].ArmorValue = 0.0f;
		}
	}

	UPROPERTY()
	FArmorSettings ArmorSettings0[MaxArmorPlatesPerMesh];
	UPROPERTY()
	FArmorSettings ArmorSettings1[MaxArmorPlatesPerMesh];
	UPROPERTY()
	FArmorSettings ArmorSettings2[MaxArmorPlatesPerMesh];

	UPROPERTY()
	UMeshComponent* MeshWithArmor0 = nullptr;
	UPROPERTY()
	UMeshComponent* MeshWithArmor1 = nullptr;
	UPROPERTY()
	UMeshComponent* MeshWithArmor2 = nullptr;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UArmorCalculation : public UActorComponent
{
	GENERATED_BODY()

public:
	UArmorCalculation();

	// Removes any registered meshes and their armor plates.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ArmorSettings")
	void ClearArmorSetup();

	/**
	 * Initializes armor calculation for the provided MeshWithArmor by storing the supplied ArmorSettings.
	 * If all armor slots are used, it reports an error via RTSFunctionLibrary::ReportError.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ArmorSettings")
	void InitArmorCalculation(UMeshComponent* MeshWithArmor, TArray<FArmorSettings> ArmorSettingsForMesh,
	                          const uint8 PlayerOwningArmor);

	/**
	 * Calculates the effective armor at the hit location.
	 *
	 * @param WeakHitComponent                The component that was hit.
	 * @param HitLocation                 World-space location of the hit.
	 * @param ProjectileDirection         Normalized direction of the incoming projectile.
	 * @param ImpactNormal                World-space impact normal.
	 * @param OutRawArmorValue            [out] The raw armor value for the impacted armor plate.
	 * @param OutAdjustedArmorPenForAngle [out] The adjusted armor penetration value for the impact angle.
	 * @param OutPlateHit
	 * @return The effective armor value adjusted for the impact angle.
	 */
	float GetEffectiveArmorOnHit(
		TWeakObjectPtr<UPrimitiveComponent> WeakHitComponent,
		const FVector& HitLocation,
		const FVector& ProjectileDirection,
		const FVector& ImpactNormal,
		float& OutRawArmorValue,
		float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit);

	float GetEffectiveDamageOnHit(
		ERTSDamageType DamageType,
		const float BaseDamage,
		TWeakObjectPtr<UPrimitiveComponent> WeakHitComponent,
		const FVector& HitLocation) const;
	
	float GetDamageOnArmorPlateResistanceAdjusted(float BaseDamage, const FArmorSettings* SelectedArmorSettings,
	                                                     FDamageMltPerSide ResistanceMultipliers,
	                                                     const FTransform& MeshTransform,
	                                                     const FVector& HitLocation) const;

	// Draw debug boxes for all registered armor plates.
	UFUNCTION(CallInEditor, BlueprintCallable, NotBlueprintable, Category = "ArmorDebug")
	void DebugArmorPlates() const;

	
	FArmorSetup GetArmorSetup() const
	{
		return M_ArmorSetup;
	}

protected:
	virtual void BeginPlay() override;

private:
	// Holds the armor configuration for up to three meshes.
	UPROPERTY()
	FArmorSetup M_ArmorSetup;

	// Multiplier of special damage types per armor plate type.
	UPROPERTY()
	FLaserRadiationDamageMlt M_LaserRadiationDamageMlt;

	
	FDamageMltPerSide GetResistanceForDamageType(const ERTSDamageType DamageType) const;
	 float GetDamageFromHitPlate(const FArmorSettings& ArmorPlate,
	                                                  const FDamageMltPerSide ResistanceMultipliers, const float BaseDamage) const;
	

	// Helper: Calculates the impact angle (in degrees) between the projectile direction and the impact normal.
	float CalculateImpactAngle(const FVector& ProjectileDirection, const FVector& ImpactNormal) const;

	// Helper: Adjusts the given ArmorValue by the impact angle.
	// The OutPenetrationAdjustment (input as a base value) is modified by an exponential decay factor.
	float GetArmorAtAngle(float ArmorValue, float AngleDegrees, float& OutPenetrationAdjustment) const;

	// Helper: Identifies which registered mesh corresponds to the hit component.
	bool IdentifyHitMesh(const UPrimitiveComponent* HitComponent, const FArmorSettings*& OutSelectedArmorSettings,
	                     UMeshComponent*& OutRegisteredMesh) const;

	// Helper: Iterates over the armor plates to find the one that was hit.
	float EvaluateArmorPlatesForHit(const FArmorSettings* SelectedArmorSettings,
	                                const FTransform& MeshTransform,
	                                const FVector& HitLocation,
	                                const FVector& ProjectileDirection,
	                                const FVector& ImpactNormal,
	                                float& OutRawArmorValue,
	                                float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit) const;

	float GetEffectiveArmor(const FVector& HitLocation,
	                        const FVector& ProjectileDirection,
	                        const FVector& ImpactNormal, float RawArmorValue, float& OutAdjustedArmorPenForAngle) const;


	float NoArmorHitGetClosest(const FArmorSettings* SelectedArmorSettings, const FTransform& MeshTransform,
	                           const FVector& HitLocation, const FVector& ProjectileDirection,
	                           const FVector& ImpactNormal, float& OutRawArmorValue,
	                           float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlatehit) const;


	/**
     * Sorts the input array of armor settings based on the static armor hierarchy.
     * The highest hierarchy value comes first.
     *
     * @param OutSortedArmorSettings The array to sort.
     */
	void SortArmorArray(TArray<FArmorSettings>& OutSortedArmorSettings) const;
	void Debug_PostSort(TArray<FArmorSettings>& ArmorSettingsSorted) const;
};
