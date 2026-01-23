// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "W_GameDifficultyPicker.generated.h"

class UBorder;
class UButton;

/**
 * @brief Presented to players to choose a mission difficulty before gameplay starts.
 */
UCLASS()
class RTS_SURVIVAL_API UW_GameDifficultyPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnDifficultyHovered(ERTSGameDifficulty HoveredDifficulty);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_NewToRTSDifficultyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_NormalDifficultyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_HardDifficultyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_BrutalDifficultyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_IronmanDifficultyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> M_DifficultyInfoBorder;

private:
	UFUNCTION()
	void OnNewToRTSDifficultyHovered();

	UFUNCTION()
	void OnNormalDifficultyHovered();

	UFUNCTION()
	void OnHardDifficultyHovered();

	UFUNCTION()
	void OnBrutalDifficultyHovered();

	UFUNCTION()
	void OnIronmanDifficultyHovered();

	UFUNCTION()
	void OnNewToRTSDifficultyUnhovered();

	UFUNCTION()
	void OnNormalDifficultyUnhovered();

	UFUNCTION()
	void OnHardDifficultyUnhovered();

	UFUNCTION()
	void OnBrutalDifficultyUnhovered();

	UFUNCTION()
	void OnIronmanDifficultyUnhovered();

	UFUNCTION()
	void OnNewToRTSDifficultyClicked();

	UFUNCTION()
	void OnNormalDifficultyClicked();

	UFUNCTION()
	void OnHardDifficultyClicked();

	UFUNCTION()
	void OnBrutalDifficultyClicked();

	UFUNCTION()
	void OnIronmanDifficultyClicked();

	void HandleDifficultyHovered(const ERTSGameDifficulty HoveredDifficulty);
	void HandleDifficultyUnhovered();
	void HandleDifficultyClicked(const ERTSGameDifficulty SelectedDifficulty);

	/**
	 * @brief Applies the selected difficulty and closes the picker once chosen.
	 * @param DifficultyPercentage Percentage configured for the selected difficulty.
	 * @param SelectedDifficulty Difficulty enum that was chosen by the player.
	 */
	void OnDifficultyChosen(const int32 DifficultyPercentage, const ERTSGameDifficulty SelectedDifficulty);

	void SetDifficultyInfoBorderVisible(const bool bIsVisible);
	bool GetIsAnyDifficultyButtonHovered() const;

	bool GetIsValidNewToRTSDifficultyButton() const;
	bool GetIsValidNormalDifficultyButton() const;
	bool GetIsValidHardDifficultyButton() const;
	bool GetIsValidBrutalDifficultyButton() const;
	bool GetIsValidIronmanDifficultyButton() const;
	bool GetIsValidDifficultyInfoBorder() const;
};
