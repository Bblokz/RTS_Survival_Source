// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_SurvivalEventBanner.generated.h"

class UTextBlock;
class USurvivalUiStyleAsset;
struct FSurvivalUiStyle;

/**
 * @brief Code-built event panel in the survival HUD style (left edge, under the minimap row): what the event is,
 * how strong the guard is, where it goes and how long the player has. The event subsystem creates it, fills it and
 * updates the countdown line; it removes itself after the event.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalEventBanner : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Shows the panel with its text lines.
	 * @param Title Upper-case headline ("SOVIET FUEL CONVOY").
	 * @param Lines Body lines (guard, cargo, route, instruction).
	 */
	void ShowEvent(const FText& Title, const TArray<FText>& Lines);

	/** @brief Replaces the countdown line under the body ("Time left 01:20"). */
	void SetCountdownText(const FText& Countdown);

	/** @brief Replaces the status line (outcome: "Convoy escaped", "All trucks destroyed"). */
	void SetStatusText(const FText& Status);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<USurvivalUiStyleAsset> M_StyleAsset;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_BodyText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CountdownText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_StatusText;

	const FSurvivalUiStyle* GetStyle() const;
	void BuildTree();
	UTextBlock* MakeText(const FName Name, bool bHeading, int32 Size, const FSlateColor& Color);
};
