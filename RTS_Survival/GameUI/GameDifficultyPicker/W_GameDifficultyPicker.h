// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "W_GameDifficultyPicker.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class URichTextBlock;
class UVerticalBox;

/** @brief The vehicle balance column and its note panel, built beside the difficulty buttons at runtime. */
USTRUCT()
struct FDifficultyPickerVehicleBalanceWidgets
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UButton> ArcadeButton;

	UPROPERTY()
	TObjectPtr<UButton> RealisticButton;

	UPROPERTY()
	TObjectPtr<URichTextBlock> ArcadeLabel;

	UPROPERTY()
	TObjectPtr<URichTextBlock> RealisticLabel;

	// Tells the player both a difficulty and a vehicle balance are needed before the game continues.
	UPROPERTY()
	TObjectPtr<URichTextBlock> PickBothHint;

	// Opens next to the column once a balance is picked and explains that balance.
	UPROPERTY()
	TObjectPtr<UBorder> NoteBorder;

	UPROPERTY()
	TObjectPtr<URichTextBlock> NoteTitle;

	UPROPERTY()
	TObjectPtr<URichTextBlock> NoteBody;
};

/** @brief What the player picked so far; the picker continues once both halves are set. */
struct FDifficultyPickerPendingChoice
{
	bool bHasDifficulty = false;
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	int32 DifficultyPercentage = 0;
	bool bHasVehicleBalance = false;
	// Guards against continuing twice when the player clicks again before the picker closes.
	bool bHasContinued = false;
};

/**
 * @brief Presented to players to choose a mission difficulty before gameplay starts.
 * Also builds a vehicle balance column (Arcade / Realistic) to the right of the difficulty buttons, so both picker
 * Blueprints get it without layout changes. The picker only continues once the player picked both; the balance is
 * stored on the game instance.
 */
UCLASS()
class RTS_SURVIVAL_API UW_GameDifficultyPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	void ConfigureForRestartCurrentMap();
	// Capture aid: shows a balance as picked (faded other option, note open) without storing it or continuing.
	void PreviewVehicleBalance(const ERTSVehicleBalance VehicleBalance) const { RefreshVehicleBalanceWidgets(VehicleBalance); }

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

	/**
	 * @brief Applies the selected difficulty and closes the picker once chosen.
	 * @param DifficultyPercentage Percentage configured for the selected difficulty.
	 * @param SelectedDifficulty Difficulty enum that was chosen by the player.
	 */
	virtual void OnDifficultyChosen(const int32 DifficultyPercentage, const ERTSGameDifficulty SelectedDifficulty);

private:
	enum class EDifficultySelectionAction : uint8
	{
		SetCurrentMission,
		RestartCurrentMap
	};

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
	void ApplyDifficultyToCurrentMission(
		const int32 DifficultyPercentage,
		const ERTSGameDifficulty SelectedDifficulty);
	void RestartCurrentMapWithDifficulty(
		const int32 DifficultyPercentage,
		const ERTSGameDifficulty SelectedDifficulty);

	void SetDifficultyInfoBorderVisible(const bool bIsVisible);
	bool GetIsAnyDifficultyButtonHovered() const;

	UFUNCTION()
	void OnRealisticBalanceClicked();

	UFUNCTION()
	void OnArcadeBalanceClicked();

	void NativeOnInitialized_BuildVehicleBalanceColumn();
	UVerticalBox* CreateVehicleBalanceColumn(const URichTextBlock& TemplateText);
	UBorder* CreateVehicleBalanceNote(const URichTextBlock& TemplateText);
	URichTextBlock* CreateVehicleBalanceText(const URichTextBlock& TemplateText);
	UButton* AddVehicleBalanceOption(UVerticalBox& Column, const URichTextBlock& TemplateText,
	                                 TObjectPtr<URichTextBlock>& OutLabel);
	void SelectVehicleBalance(const ERTSVehicleBalance VehicleBalance);
	void RefreshVehicleBalanceWidgets(const ERTSVehicleBalance VehicleBalance) const;
	bool GetAreVehicleBalanceWidgetsValid() const;

	// Fades the difficulty buttons that were not picked so the pick stays visible while the player picks a balance.
	void RefreshDifficultySelection() const;
	// Continues (OnDifficultyChosen) once both a difficulty and a vehicle balance are picked.
	void TryContinueWithPendingChoice();

	UPROPERTY()
	FDifficultyPickerVehicleBalanceWidgets M_VehicleBalanceWidgets;

	FDifficultyPickerPendingChoice M_PendingChoice;

	bool GetIsValidNewToRTSDifficultyButton() const;
	bool GetIsValidNormalDifficultyButton() const;
	bool GetIsValidHardDifficultyButton() const;
	bool GetIsValidBrutalDifficultyButton() const;
	bool GetIsValidIronmanDifficultyButton() const;
	bool GetIsValidDifficultyInfoBorder() const;

	EDifficultySelectionAction M_SelectionAction = EDifficultySelectionAction::SetCurrentMission;
};
