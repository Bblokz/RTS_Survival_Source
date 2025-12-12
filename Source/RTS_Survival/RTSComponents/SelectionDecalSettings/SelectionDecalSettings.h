#pragma once

#include "CoreMinimal.h"

#include "SelectionDecalSettings.generated.h"


/**
 * a collection of parameters useful for definining a second state to which the decal of the selection component can be set.
 */
USTRUCT(BlueprintType, Blueprintable)
struct FSelectionDecalSettings
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	mutable UMaterialInterface* State2_SelectionDecalMat = nullptr;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	mutable UMaterialInterface* Sate2_DeselectionDecalMat = nullptr;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector State1_SelectionDecalSize = FVector(1.0f, 1.0f, 1.0f);
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector State2_SelectionDecalSize = FVector(1.0f, 1.0f, 1.0f);
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector State1_DecalPosition = FVector(0.0f, 0.0f, 0.0f);
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector State2_DecalPosition = FVector(0.0f, 0.0f, 0.0f);
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector State1_AreaSize = FVector(1.0f, 1.0f, 1.0f);
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector State2_AreaSize = FVector(1.0f, 1.0f, 1.0f);
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector State1_AreaPosition = FVector(0.0f, 0.0f, 0.0f);
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FVector State2_AreaPosition = FVector(0.0f, 0.0f, 0.0f);
};
