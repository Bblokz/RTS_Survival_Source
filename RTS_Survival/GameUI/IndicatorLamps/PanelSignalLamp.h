// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "PanelSignalLamp.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UWidgetTree;

/**
 * @brief A signal lamp set into a panel (the info takeover page lamps): the lamp plus a halo that lights the panel
 * around it, each an Image drawing its own dynamic instance of an M_UI_SignalLamp material (pass 0 lamp, pass 1 halo).
 * The owner places the halo in a layer under the lamps, at the lamp's layout size (a render transform scales it up,
 * so it never moves the layout), says whether the lamp is lit and ticks it so it fades like a filament.
 * Without a material the lamp swaps two plain brushes and has no halo.
 */
USTRUCT()
struct FPanelSignalLamp
{
	GENERATED_BODY()

	/**
	 * @brief Builds the lamp (and with a material the halo) as hit-test invisible images; the lamp starts dark.
	 * @param WidgetTree Tree of the widget that will hold the images.
	 * @param LampMaterial M_UI_SignalLamp instance; null uses the fallback brushes and builds no halo.
	 * @param FallbackLitBrush Shown while lit when there is no material.
	 * @param FallbackUnlitBrush Shown while dark when there is no material.
	 * @param HaloScale Edge of the halo over the edge of the lamp.
	 * @param TimeOffset Seconds added to the material clock, so neighbouring lamps flicker out of step.
	 */
	void Build(UWidgetTree& WidgetTree, UMaterialInterface* LampMaterial, const FSlateBrush& FallbackLitBrush,
	           const FSlateBrush& FallbackUnlitBrush, const float HaloScale, const float TimeOffset);

	UImage* GetLampImage() const { return M_LampImage; }

	/** @return The halo image; null when the lamp has no material. */
	UImage* GetHaloImage() const { return M_HaloImage; }

	void SetIsLit(const bool bIsLit);
	bool GetIsLit() const { return bM_IsLit; }

	/** Ramps the lamp's power towards lit or dark over FadeSeconds of real time. */
	void Tick(const float DeltaTime, const float FadeSeconds);

	/** @return 0 dark .. 1 fully lit. */
	float GetPower() const { return M_Power; }

private:
	UPROPERTY()
	TObjectPtr<UImage> M_LampImage;

	UPROPERTY()
	TObjectPtr<UImage> M_HaloImage;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_LampMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_HaloMaterial;

	UPROPERTY()
	FSlateBrush M_FallbackLitBrush;

	UPROPERTY()
	FSlateBrush M_FallbackUnlitBrush;

	float M_Power = 0.f;
	bool bM_IsLit = false;

	UImage* BuildImage(UWidgetTree& WidgetTree) const;
	UMaterialInstanceDynamic* MakeLampPassMaterial(UMaterialInterface& LampMaterial, UImage& Image, const float Pass,
	                                               const float HaloScale, const float TimeOffset) const;
	void ApplyPower() const;
	bool GetHasMaterial() const;
};
