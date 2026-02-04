// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_FactionWorldGenerationSettings.generated.h"

class AFactionPlayerController;
class UButton;
class UCheckBox;
class UComboBoxString;
class UHorizontalBox;
class URichTextBlock;
class USlider;

enum class EEnemyWorldPersonality : uint8;
enum class ERTSGameDifficulty : uint8;

/**
 * @brief Campaign generation settings panel used after the faction difficulty is chosen.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionWorldGenerationSettings : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetFactionPlayerController(AFactionPlayerController* FactionPlayerController);
	void SetDifficultyText(const ERTSGameDifficulty SelectedDifficulty);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_Back = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_Generate = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> M_ShowAdvancedSettings = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> M_ExtraDifficultyBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> M_ExtraDifficulty = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> M_PersonalityBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> M_PersonalityComboBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_DifficultyText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> M_SeedSlider = nullptr;

private:
	UPROPERTY()
	TWeakObjectPtr<AFactionPlayerController> M_FactionPlayerController;

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleGenerateClicked();

	UFUNCTION()
	void HandleShowAdvancedSettingsChanged(const bool bIsChecked);

	void SetupPersonalityComboBox();
	void UpdateAdvancedSettingsVisibility(const bool bIsVisible);
	EEnemyWorldPersonality GetSelectedPersonality() const;
	int32 GetSelectedSeed() const;
	bool GetIsValidFactionPlayerController() const;
	bool GetIsValidBackButton() const;
	bool GetIsValidGenerateButton() const;
	bool GetIsValidShowAdvancedSettingsCheckBox() const;
	bool GetIsValidExtraDifficultyBox() const;
	bool GetIsValidExtraDifficultyCheckBox() const;
	bool GetIsValidPersonalityBox() const;
	bool GetIsValidPersonalityComboBox() const;
	bool GetIsValidDifficultyText() const;
	bool GetIsValidSeedSlider() const;
};
