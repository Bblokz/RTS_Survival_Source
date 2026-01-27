#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Commands/InputChord.h"

#include "W_EscapeMenuKeyBindingEntry.generated.h"

class ACPPController;
class UInputAction;
class UInputKeySelector;
class URichTextBlock;

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

protected:
	virtual void NativeConstruct() override;

private:
	bool GetIsValidPlayerController() const;
	bool GetIsValidAction() const;
	bool GetIsValidActionNameText() const;
	bool GetIsValidKeySelector() const;

	void UpdateDisplayedKey(const FKey& NewKey);

	UFUNCTION()
	void HandleKeySelected(const FInputChord& SelectedKey);

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> M_Action = nullptr;

	FKey M_CurrentKey;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_ActionNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInputKeySelector> M_KeySelector = nullptr;
};
