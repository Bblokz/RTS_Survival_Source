// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MinimapTerrainCaptureComponent.generated.h"

class AFowManager;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UWorld;

/**
 * @brief Add to the RTS game state to capture a static playable-area image for the minimap.
 * The temporary camera exists only during the initial capture; this component never ticks.
 */
UCLASS(ClassGroup = (RTS))
class RTS_SURVIVAL_API UMinimapTerrainCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMinimapTerrainCaptureComponent();

	/** @return The captured playable-area texture, or nullptr after reporting failed initialization. */
	UFUNCTION(BlueprintPure, Category = "MiniMap")
	UTextureRenderTarget2D* GetMinimapTerrainRenderTarget() const;

protected:
	virtual void BeginPlay() override;

private:
	void BeginPlay_CapturePlayableArea();
	bool CreateMinimapTerrainRenderTarget();

	/**
	 * @brief Keeps the temporary capture camera isolated from all later minimap work.
	 * @param FowManager Supplies the playable-area center and half-width.
	 * @param World World in which the temporary camera is spawned and immediately destroyed.
	 * @return True when the one-shot scene capture was submitted.
	 */
	bool CapturePlayableArea(const AFowManager& FowManager, UWorld& World);

	/**
	 * @brief Makes the render target line up with the FOW manager's world-to-minimap UV mapping.
	 * @param SceneCaptureComponent Temporary camera component receiving capture-only settings.
	 * @param FowManager Supplies the playable-area center and half-width.
	 */
	void ConfigureSceneCapture(
		USceneCaptureComponent2D& SceneCaptureComponent,
		const AFowManager& FowManager) const;

	bool GetIsValidMinimapTerrainRenderTarget() const;

	UPROPERTY(EditDefaultsOnly, Category = "MiniMap|Terrain Capture", meta = (ClampMin = "64", UIMin = "64", ClampMax = "4096", UIMax = "4096"))
	int32 M_RenderTargetResolution = 1024;

	UPROPERTY(EditDefaultsOnly, Category = "MiniMap|Terrain Capture", meta = (ClampMin = "1.0", UIMin = "1000.0"))
	float M_CaptureHeight = 100000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "MiniMap|Terrain Capture")
	FLinearColor M_ClearColor = FLinearColor::Black;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> M_MinimapTerrainRenderTarget = nullptr;
};
