#pragma once

#include "CoreMinimal.h"

#include "HealthBarVisibilitySettings.generated.h"

USTRUCT(BlueprintType)
struct FHealthBarVisibilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAlwaysDisplay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDisplayOnHover = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDisplayOnSelected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDisplayOnDamaged = false;
};

USTRUCT(BlueprintType)
struct FHealthBarCustomization 
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseGreenRedGradient = true;

	// Will overwrite gradient settings bar and instead set to custom provided color.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseEnemyColorIfEnemy = true;

	// Used to color HealthBar if bUseEnemyColorIfEnemy is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor EnemyColor = FLinearColor::Red;

	// Default damage color.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor DamageBarColor = FLinearColor::Red;
	
	// Used to color the DamageBar if bUseEnemyColorIfEnemy is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor EnemyDamageColor = FLinearColor::Gray;

	// Used to color the HealthBar if bUseGreenRedGradient is false.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor OverWriteGradientColor = FLinearColor::Green;

	// Used by any unit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor BackgroundColor = FLinearColor::Black;
	
};
