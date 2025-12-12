// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "W_Portrait.generated.h"

class USlateBrushAsset;

/**
 * Simple portrait widget that can asynchronously load and display
 * a portrait image based on the requested portrait type.
 */
UCLASS()
class RTS_SURVIVAL_API UW_Portrait : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Asynchronously update the portrait to the given type and show the widget. */
	void UpdatePortrait(ERTSPortraitTypes NewType);

	/** Hide the portrait and clear its brush. */
	void HidePortrait();

protected:
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	UImage* PortraitImage = nullptr;

	/** Soft references to the portrait brushes keyed by portrait type. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Portrait")
	TMap<ERTSPortraitTypes, TSoftObjectPtr<USlateBrushAsset>> PortraitBrushes;

	/** Duration in seconds for the fade-in animation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Portrait|Fade")
	float M_FadeInDurationSeconds = 2.0f;

	/** Tick interval for the fade-in timer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Portrait|Fade")
	float M_FadeInTickIntervalSeconds = 0.02f;

private:
	/** Called when an async brush load finishes. */
	void OnBrushLoaded(USlateBrushAsset* LoadedAsset);

	/** Start (or restart) the fade-in animation. */
	void StartFadeIn();

	/** Per-tick fade logic driven by the timer manager. */
	void TickFadeIn();

	/** Handle for the fade-in timer. */
	FTimerHandle M_FadeInTimerHandle;

	/** True while the widget is currently fading in. */
	bool bM_IsFadingIn = false;

	/** Accumulated time in seconds since the fade-in started. */
	float M_CurrentFadeTimeSeconds = 0.0f;
};
