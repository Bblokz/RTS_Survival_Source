#pragma once

#include "CoreMinimal.h"
#include "HealthBarSettings.generated.h"

USTRUCT(BlueprintType)
struct FHealthBarDynamicMaterialParameter
{
	GENERATED_BODY()

	// Value to lerp bar to.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName TargetHealthName = "TargetHealth";

	// Value to start bar at.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName StartHealthName = "StartHealth";

	// Determines when lerping starts.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName StartTimeDamageName = "StartTimeDamage";
};

USTRUCT(BlueprintType)
struct FDamagePointDynamicMaterialParameter
{
	GENERATED_BODY()
	
	// Value to lerp bar to.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName TargetHealthName = "TargetHealth";

	// Value to start bar at.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName StartHealthName = "StartHealth";

	// Determines when lerping starts.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName StartTimeDamageName = "StartTimeDamage";
};

USTRUCT(BlueprintType)
struct FHealthBarDMIColorParameter
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName ProgressBarColorName = "ProgressBarColor";

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName DamageBarColorName = "DamageBarColor";

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName BgBarColorName = "BGBarColor";

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName AmountSliceName = "AmountSlices";
};

USTRUCT(BlueprintType)
struct FHealthBarDMIAmmoParameter
{
	GENERATED_BODY()

	// The total amount of bullets in the ammo icon.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName MaxMagCapacity = "MaxMagCapacity";

	// How many rounds are left in the mag of the weapon.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName CurrentMagAmount= "CurrentMagAmount";

	// The ratio in (0,1) of how thick a vertical slice is in the ammo icon.
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	FName VerticalSliceUVRatio= "VerticalSliceUVRatio";
};
