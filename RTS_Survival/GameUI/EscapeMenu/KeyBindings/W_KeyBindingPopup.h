#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_KeyBindingPopup.generated.h"

class ACPPController;
class UButton;
class UInputAction;
class URichTextBlock;
class UWidgetSwitcher;
class UW_EscapeMenuKeyBindingEntry;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKeyBindingPopupUnbindRequested, UInputAction*, const FKey&);
DECLARE_MULTICAST_DELEGATE(FOnKeyBindingPopupExitRequested);

/**
 * @brief Popup widget used to focus on remapping a single key binding at a time.
 *
 * The key bindings menu spawns this widget to show either a binding entry or a conflict message.
 */
UCLASS()
class RTS_SURVIVAL_API UW_KeyBindingPopup : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Prepares the popup to remap a specific action without forcing navigation away from the list.
	 * @param NewPlayerController Player controller that owns the mapping context.
	 * @param ActionToBind Action that should be remapped in the popup.
	 * @param CurrentKey Key currently assigned to the action.
	 */
	void SetupPopup(ACPPController* NewPlayerController, UInputAction* ActionToBind, const FKey& CurrentKey);

	void ShowBindingEntry();
	void ShowCollisionMessage(const FString& CollidingActionName);
	void ShowUnboundActionsWarning(const FString& WarningText);
	void ClosePopup();

	UW_EscapeMenuKeyBindingEntry* GetKeyBindingEntry() const;
	FOnKeyBindingPopupUnbindRequested& OnUnbindRequested();
	FOnKeyBindingPopupExitRequested& OnConfirmExitRequested();
	FOnKeyBindingPopupExitRequested& OnCancelExitRequested();

protected:
	virtual void NativeConstruct() override;

private:
	bool GetIsValidKeyBindingEntry() const;
	bool GetIsValidPopupSwitcher() const;
	bool GetIsValidTitleText() const;
	bool GetIsValidConflictText() const;
	bool GetIsValidUnderstoodButton() const;
	bool GetIsValidUnbindButton() const;
	bool GetIsValidBindWarningText() const;
	bool GetIsValidConfirmExitButton() const;
	bool GetIsValidCancelExitButton() const;
	bool GetIsValidActionToBind() const;

	bool SetBindingContext(ACPPController* NewPlayerController, UInputAction* ActionToBind, const FKey& CurrentKey);
	void SetTitleText(const FString& TitleText);

	UFUNCTION()
	void HandleUnderstoodClicked();

	UFUNCTION()
	void HandleUnbindClicked();

	UFUNCTION()
	void HandleConfirmExitClicked();

	UFUNCTION()
	void HandleCancelExitClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> M_PopupSwitcher = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_Title = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_EscapeMenuKeyBindingEntry> M_KeyBindingEntry = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_ConflictText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_UnderstoodButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_UnbindButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URichTextBlock> M_BindWarningText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ConfirmExitButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_CancelExitButton = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> M_ActionToBind = nullptr;

	FKey M_CurrentKey;

	bool bM_HasBindingContext = false;
	bool bM_ShouldReturnToEntryOnUnderstood = false;

	FOnKeyBindingPopupUnbindRequested M_OnUnbindRequested;
	FOnKeyBindingPopupExitRequested M_OnConfirmExitRequested;
	FOnKeyBindingPopupExitRequested M_OnCancelExitRequested;
};
