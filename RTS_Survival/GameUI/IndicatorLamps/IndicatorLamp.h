// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "IndicatorLamp.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UWidgetTree;

/**
 * @brief One analog indicator lamp on the HUD: an Image drawing its own dynamic instance of an M_UI_IndicatorLamp
 * material. The material blinks by itself; the owner only says whether the lamp is lit and ticks it, so the
 * lamp fades in and out like a filament instead of snapping.
 */
USTRUCT()
struct FIndicatorLamp
{
	GENERATED_BODY()

	/**
	 * @brief Builds the (hit-test invisible) lamp image; it starts dark.
	 * @param WidgetTree Tree of the widget that will hold the image.
	 * @param LampMaterial M_UI_IndicatorLamp instance (colour and blink pattern).
	 * @param LampSize Edge length in UI units.
	 * @param bHideWhenOff Collapse the image once the lamp is fully dark instead of showing the dark jewel.
	 * @return The image to place, or nullptr when the material is missing.
	 */
	UImage* Build(UWidgetTree& WidgetTree, UMaterialInterface* LampMaterial, const float LampSize,
	              const bool bHideWhenOff);

	void SetIsLit(const bool bIsLit);

	/** Ramps the lamp's power towards lit or dark. */
	void Tick(const float DeltaTime);

private:
	UPROPERTY()
	TObjectPtr<UImage> M_Image;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_Material;

	float M_Power = 0.f;
	bool bM_IsLit = false;
	bool bM_HideWhenOff = false;

	void ApplyPower() const;

	// Silent on purpose: a lamp with no material configured is simply not built, which is not an error.
	bool GetIsBuilt() const;
};
