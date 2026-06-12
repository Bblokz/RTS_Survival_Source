#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyTypes.h"
#include "W_EscapeMenuChordedKeyBindingEntry.generated.h"

class ACPPController;
class UComboBoxString;
class UInputAction;
class UInputKeySelector;
class URichTextBlock;

DECLARE_DELEGATE_RetVal_TwoParams(bool, FChordedKeyBindingValidationDelegate, UInputAction*, const FRTSModifierHotkey&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnChordedKeyBindingUpdated, UInputAction*, const FRTSModifierHotkey&);

/**
 * @brief Escape menu row for actions that are activated by one modifier plus one regular key.
 */
UCLASS()
class RTS_SURVIVAL_API UW_EscapeMenuChordedKeyBindingEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Prepares this row to edit the modifier and secondary key as a single chord.
	 * @param NewPlayerController Controller that owns the chorded IMC mutation logic.
	 * @param ActionToBind Chorded action represented by this row.
	 * @param CurrentHotkey Current modifier/key pair, or invalid key when unbound.
	 */
	void SetupEntry(ACPPController* NewPlayerController, UInputAction* ActionToBind, const FRTSModifierHotkey& CurrentHotkey);
	void SetKeyBindingValidationDelegate(FChordedKeyBindingValidationDelegate ValidationDelegate);
	FOnChordedKeyBindingUpdated& OnChordedKeyBindingUpdated();
	void UpdateKeyBinding(const FRTSModifierHotkey& NewHotkey);
	bool GetIsKeyBound() const;
	FRTSModifierHotkey GetCurrentHotkey() const;
	FString GetActionDisplayName() const;

protected:
	virtual void NativeConstruct() override;

private:
	void PopulateModifierSelector();
	void UpdateDisplayedHotkey(const FRTSModifierHotkey& NewHotkey);
	ERTSHotkeyModifier GetSelectedModifier() const;
	FString BuildActionDisplayName(const FName& ActionName) const;
	bool GetIsInvalidSecondaryKey(const FKey& Key) const;
	bool GetIsDisallowedControlGroupSemanticKey(const FRTSModifierHotkey& Hotkey) const;

	bool GetIsValidPlayerController() const;
	bool GetIsValidAction() const;
	bool GetIsValidActionNameText() const;
	bool GetIsValidModifierSelector() const;
	bool GetIsValidKeySelector() const;

	UFUNCTION()
	void HandleModifierSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleKeySelected(FInputChord SelectedKey);

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> M_Action = nullptr;

	FRTSModifierHotkey M_CurrentHotkey;
	FString M_ActionDisplayName;

	FChordedKeyBindingValidationDelegate M_KeyBindingValidationDelegate;
	FOnChordedKeyBindingUpdated M_OnChordedKeyBindingUpdated;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_ActionNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> M_ModifierSelector = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInputKeySelector> M_KeySelector = nullptr;
};
