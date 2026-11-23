// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Game/GameState/HideGameUI/RTSUIElement.h"

#include "W_SurrenderingTruckCountdown.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * @brief Scuttle countdown shown over a surrendering cargo truck by a screen-space widget component on the truck, so
 * it always faces the camera and hides with the truck in the fog of war. Works with no asset: the native class builds
 * a dark plate with the seconds left and a shrinking red bar. A Blueprint child may lay out its own CountdownBar and
 * CountdownText (both optional) instead. Follows the "hide all game UI" toggle (IRTSUIElement); the truck passes
 * the current state when it creates the widget.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurrenderingTruckCountdown : public UUserWidget, public IRTSUIElement
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	virtual void OnHideAllGameUI(const bool bHide) override;

	/**
	 * @brief Shows how long the crew still waits before blowing the truck up.
	 * @param SecondsLeft Seconds until the scuttle.
	 * @param FractionLeft SecondsLeft over the full countdown (1 at the start, 0 at the blast).
	 * @param TextFormat Label with {0} for the whole seconds left.
	 */
	void SetCountdown(float SecondsLeft, float FractionLeft, const FText& TextFormat);

protected:
	// Optional in a Blueprint child; the native class creates both.
	UPROPERTY(BlueprintReadOnly, Category = "Countdown", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CountdownBar = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Countdown", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountdownText = nullptr;

	// Restyle in a Blueprint child (e.g. flash the bar in the last seconds).
	UFUNCTION(BlueprintImplementableEvent, Category = "Countdown")
	void BP_OnCountdownUpdated(float SecondsLeft, float FractionLeft);

private:
	void BuildNativeLayout();
};
