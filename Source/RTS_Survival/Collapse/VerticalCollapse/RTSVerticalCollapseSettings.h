
// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSVerticalCollapseSettings.generated.h"

/**
 * @brief Designer settings for a vertical-collapse animation: move actor down over time,
 *        optional random yaw and pitch/roll, and optional destroy-on-finish.
 */
USTRUCT(BlueprintType)
struct FRTSVerticalCollapseSettings
{
	GENERATED_BODY()

	/** How far (UU) to sink the actor along -Z. Positive values go into the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VerticalCollapse", meta=(ClampMin="0.0"))
	float DeltaZ = 300.0f;

	/** Total time (s) to reach the full DeltaZ and final rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VerticalCollapse", meta=(ClampMin="0.0"))
	float CollapseTime = 1.0f;

	/**
	 * Random yaw magnitude range (degrees). If both are ~0, yaw stays unchanged.
	 * We pick a random magnitude in [MinRandomYaw, MaxRandomYaw], then randomly apply + or - sign.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VerticalCollapse", meta=(ClampMin="0.0"))
	float MinRandomYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VerticalCollapse", meta=(ClampMin="0.0"))
	float MaxRandomYaw = 0.0f;

	/**
	 * Random pitch/roll combined magnitude range (degrees). If both are ~0, pitch and roll stay unchanged.
	 * A single magnitude in [MinPitchRoll, MaxPitchRoll] is chosen, and applied across a random 2D direction
	 * in the (pitch, roll) plane so that one value controls both pitch and roll together.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VerticalCollapse", meta=(ClampMin="0.0"))
	float MinPitchRoll = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VerticalCollapse", meta=(ClampMin="0.0"))
	float MaxPitchRoll = 0.0f;

	/**
	 * If true, the owning actor will be destroyed at the end of the vertical collapse.
	 * When enabled, the optional completion callback will NOT be executed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VerticalCollapse")
	bool bDestroyPostVerticalCollapse = false;
};
