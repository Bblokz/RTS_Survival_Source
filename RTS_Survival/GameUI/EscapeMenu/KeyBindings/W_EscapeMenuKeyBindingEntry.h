#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Commands/InputChord.h"

#include "W_EscapeMenuKeyBindingEntry.generated.h"

class ACPPController;
class UInputAction;
class UInputKeySelector;
class URichTextBlock;

DECLARE_DELEGATE_RetVal_TwoParams(bool, FKeyBindingValidationDelegate, UInputAction*, const FKey&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKeyBindingUpdated, UInputAction*, const FKey&);

/**
 * @brief Row widget that binds a single input action to a selectable key widget.
 *
 * Instances are created by the key bindings menu to expose runtime remapping for each action.
 */
UCLASS()
class RTS_SURVIVAL_API UW_EscapeMenuKeyBindingEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Supplies the owning controller and action so this entry can update the mapping context.
	 * @param NewPlayerController Player controller owning the input mapping context.
	 * @param ActionToBind Action represented by this row.
	 * @param CurrentKey Key currently bound to the action.
	 */
	void SetupEntry(ACPPController* NewPlayerController, UInputAction* ActionToBind, const FKey& CurrentKey);

	void SetKeyBindingValidationDelegate(FKeyBindingValidationDelegate ValidationDelegate);
	FOnKeyBindingUpdated& OnKeyBindingUpdated();

	void UpdateKeyBinding(const FKey& NewKey);
	bool GetIsKeyBound() const;
	FKey GetCurrentKey() const;
	FString GetActionDisplayName() const;

protected:
	virtual void NativeConstruct() override;

private:
	bool GetIsValidPlayerController() const;
	bool GetIsValidAction() const;
	bool GetIsValidActionNameText() const;
	bool GetIsValidKeySelector() const;

	void UpdateDisplayedKey(const FKey& NewKey);
	FString BuildActionDisplayName(const FName& ActionName) const;

	UFUNCTION()
	void HandleKeySelected(FInputChord SelectedKey);

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> M_Action = nullptr;

	FKey M_CurrentKey;
	FString M_ActionDisplayName;

	FKeyBindingValidationDelegate M_KeyBindingValidationDelegate;
	FOnKeyBindingUpdated M_OnKeyBindingUpdated;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_ActionNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInputKeySelector> M_KeySelector = nullptr;
};
