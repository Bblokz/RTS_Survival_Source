// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraActor.h"
#include "RTSShowcaseCamera.generated.h"

UENUM(BlueprintType)
enum class ERTSShowcaseShotFormat : uint8
{
	// 16:9 landscape still.
	Horizontal16x9,
	// 1:2 portrait still (twice as tall as wide), the store-page / poster format.
	Vertical1x2,
	// 2:1 wide still for large army overviews.
	Panorama2x1
};

/**
 * @brief Camera preset of the showcase screenshot map: place one per framing; RTS.Showcase.Shot renders it.
 * The filmback follows the shot format, so piloting the camera in the editor shows exactly the saved picture.
 * Stage lighting is switched by actor tags "ShowcaseLight_<StageName>" (see FRTSShowcaseStageLighting).
 */
UCLASS()
class RTS_SURVIVAL_API ARTSShowcaseCamera : public ACineCameraActor
{
	GENERATED_BODY()

public:
	ARTSShowcaseCamera(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;

	// Scene this camera belongs to; only the lights tagged ShowcaseLight_<StageName> are on while it renders.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Showcase")
	FName StageName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Showcase")
	FString ShotName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Showcase")
	ERTSShowcaseShotFormat ShotFormat = ERTSShowcaseShotFormat::Horizontal16x9;

	// Pixels along the long edge of the saved picture.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Showcase", meta = (ClampMin = "256", ClampMax = "16384"))
	int32 LongEdgePixels = 7680;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Showcase", meta = (MultiLine = true))
	FString Notes;

	/** @return Saved picture size, scaled by ResolutionScale (previews use < 1). */
	FIntPoint GetOutputResolution(float ResolutionScale) const;

	/** Stage and shot name joined, used for file names and the console filter. */
	FString GetShotId() const;

	/** Turns this camera's stage lighting on and every other stage's off in the editor world. */
	UFUNCTION(CallInEditor, Category = "Showcase")
	void ActivateStageLighting();

private:
	void ApplyFilmbackForFormat();
};

/**
 * @brief Switches the per-stage sky, sun, fog and post-process actors of the showcase map.
 * Actors tagged ShowcaseLight_<Stage> belong to that stage; all others are left alone.
 */
struct RTS_SURVIVAL_API FRTSShowcaseStageLighting
{
	/**
	 * @brief Enables the lighting actors of one stage and disables those of all other stages.
	 * @param World World whose actors are switched (editor or game).
	 * @param StageName Stage to light; NAME_None switches every stage off.
	 * @param bMarkModified True in the editor, so the change can be undone and saved.
	 * @return Number of lighting actors that belong to StageName.
	 */
	static int32 ActivateStage(UWorld* World, FName StageName, bool bMarkModified);

	static const TCHAR* GetTagPrefix() { return TEXT("ShowcaseLight_"); }

private:
	static void SetActorLightingEnabled(AActor& Actor, bool bEnabled, bool bMarkModified);
};
