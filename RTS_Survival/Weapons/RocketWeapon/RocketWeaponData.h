#pragma once

#include "CoreMinimal.h"

#include "RocketWeaponData.generated.h"

class UStaticMesh;

USTRUCT(Blueprintable, BlueprintType)
struct FRocketWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UStaticMesh* RocketMesh = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float RandomSwingYawDeviationMin = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float RandomSwingYawDeviationMax = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.1", UIMin="0.1"))
	float StraightSpeedMlt = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.1", UIMin="0.1"))
	float InCurveSpeedMlt = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="1.0", ClampMax="100.0", UIMin="1.0", UIMax="100.0"))
	float RandomSwingStrength = 50.0f;
};
