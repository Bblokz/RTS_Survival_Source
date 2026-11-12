// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_HotKey.generated.h"

class URichTextBlock;
class URTSHotkeyProviderSubsystem;

/**
 * @brief Rich text hotkey display widget that receives key strings from UI logic.
 * Call ShowChordedAction once to turn it into a self-updating badge for a chorded action (e.g. "ALT Q"):
 * it follows key binding changes and collapses while the action is unbound.
 */
UCLASS()
class RTS_SURVIVAL_API UW_HotKey : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetKeyText(const FText& NewKeyText);

	/**
	 * @brief Shows the chorded action's current hotkey as "<Modifier> <Key>" and keeps it in sync with rebinding.
	 * @param ActionName Chorded input action whose binding to show (e.g. IA_BuildingExpansion1).
	 * @param ModifierStyle Rich text row for the modifier part.
	 * @param KeyStyle Rich text row for the key part.
	 */
	void ShowChordedAction(const FName& ActionName, const FString& ModifierStyle, const FString& KeyStyle);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* M_KeyText;

private:
	bool GetIsValidKeyText() const;

	// Set by ShowChordedAction; NAME_None keeps the widget a plain text setter.
	FName M_ChordedActionName = NAME_None;
	FString M_ChordedModifierStyle;
	FString M_ChordedKeyStyle;

	UPROPERTY()
	TWeakObjectPtr<URTSHotkeyProviderSubsystem> M_HotkeyProviderSubsystem;

	FDelegateHandle M_ChordedActionHotkeyHandle;

	void BindChordedActionUpdates();
	void UnbindChordedActionUpdates();
	void HandleChordedActionHotkeyUpdated(const FName ActionName, const FText& HotkeyText);
	void RefreshChordedActionText();
};
