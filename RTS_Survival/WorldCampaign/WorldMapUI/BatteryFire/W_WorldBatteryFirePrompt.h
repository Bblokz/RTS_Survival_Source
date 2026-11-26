// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "W_WorldBatteryFirePrompt.generated.h"

class AWorldPlayerController;
class UBorder;
class UButton;
class UTextBlock;
class UWorldBatteryFireDirector;
struct FWorldCampaignUILook;

/**
 * @brief The small plate at the bottom of the map while a battery waits for its target: what to click, a Cancel
 * button and, after a wrong click, why it was refused. The campaign HUD creates it and calls InitBatteryFirePrompt
 * once; from then on it follows the battery fire director's picking state by itself.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldBatteryFirePrompt : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Builds the plate with the shared look and listens to the director of the controller.
	 * @param WorldPlayerController Controller that owns the battery fire director.
	 * @param Look The campaign HUD's loaded look; only read here, not kept.
	 */
	void InitBatteryFirePrompt(AWorldPlayerController* WorldPlayerController, const FWorldCampaignUILook* Look);

	/** Ends the pick; used when the map widgets are hidden or input is blocked. */
	void CancelPicking();

protected:
	virtual void NativeDestruct() override;

	// A click on the plate never falls through to the map as a pick; a right click cancels like one on the map.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void BuildWidgetTree(const FWorldCampaignUILook& Look);
	void HandlePickingChanged();
	void HandlePickRefused(const FText& Reason);
	void HideFailReason();

	UFUNCTION()
	void HandleCancelClicked();

	bool GetIsValidDirector() const;

	UPROPERTY()
	TWeakObjectPtr<UWorldBatteryFireDirector> M_Director;

	FTimerHandle M_HideFailReasonTimer;

	UPROPERTY()
	TObjectPtr<UBorder> M_Plate;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_PromptText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_FailText;

	UPROPERTY()
	TObjectPtr<UButton> M_CancelButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CancelLabel;
};
