#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_EscapeMenuKeyBindings.generated.h"

class ACPPController;
class UButton;
class UEditableTextBox;
class UMainGameUI;
class UScrollBox;
class UW_EscapeMenuKeyBindingEntry;
class UInputMappingContext;
class UW_KeyBindingPopup;
class UInputAction;
class URTSHotkeyProviderSubsystem;
class UW_HotKey;

USTRUCT(BlueprintType)
struct FEscapeMenuKeyBindingEntryData

{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UW_EscapeMenuKeyBindingEntry> EntryWidget = nullptr;
	FString ActionDisplayName;
};


/**
 * @brief Escape menu panel that builds a list of key binding entries for runtime remapping.
 *
 * Designers provide the list container and entry widget in Blueprint while C++ drives the data population.
 */
UCLASS()
class RTS_SURVIVAL_API UW_EscapeMenuKeyBindings : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Provides the owning controller so key bindings can be queried and updated.
	 * @param NewPlayerController Player controller that owns the key bindings menu.
	 */
	void SetPlayerController(ACPPController* NewPlayerController);
	void HandleKeyBindingsMenuClosed();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "KeyBindings")
	TSubclassOf<UW_EscapeMenuKeyBindingEntry> M_KeyBindingEntryClass;

	UPROPERTY(EditDefaultsOnly, Category = "KeyBindings")
	TSubclassOf<UW_KeyBindingPopup> M_KeyBindingPopupClass;

private:
	void BindButtonCallbacks();
	void BindBackButton();
	void BindSearchBar();
	void BindActionButtons();
	void BindControlGroupButtons();
	void BuildKeyBindingEntries();
	void ApplySearchFilter(const FString& SearchText);
	void InitializeHotkeyBindings();

	void HandleActionButtonClicked(const int32 ActionButtonIndex);
	void HandleControlGroupButtonClicked(const int32 ControlGroupIndex);

	void OpenKeyBindingPopupForActionName(const FName& ActionName);
	void UpdateKeyBindingEntryForAction(const FName& ActionName, const FKey& NewKey);
	void EnsureKeyBindingPopupVisible();
	void CloseKeyBindingPopup();
	void BindKeyBindingPopupCallbacks();
	FString BuildUnboundActionsWarningText(const TArray<FString>& UnboundActionNames) const;
	TArray<FString> GetUnboundActionNames() const;
	bool TryGetFirstUnboundActionName(FName& OutActionName) const;
	void CacheHotkeyProviderSubsystem();
	void BindHotkeyUpdateDelegates();
	void UnbindHotkeyUpdateDelegates();
	void UpdateActionButtonHotkeys();
	void UpdateControlGroupHotkeys();
	void HandleActionSlotHotkeyUpdated(const int32 ActionSlotIndex, const FText& HotkeyText);
	void HandleControlGroupHotkeyUpdated(const int32 ControlGroupIndex, const FText& HotkeyText);

	UW_HotKey* GetActionButtonHotKeyByIndex(const int32 ActionSlotIndex) const;
	UW_HotKey* GetControlGroupHotKeyByIndex(const int32 ControlGroupIndex) const;
	const TCHAR* GetActionButtonHotKeyName(const int32 ActionSlotIndex) const;
	const TCHAR* GetControlGroupHotKeyName(const int32 ControlGroupIndex) const;
	bool GetUsesZeroBasedActionButtonIndexing() const;
	bool GetUsesZeroBasedControlGroupIndexing() const;
	int32 GetActionButtonActionIndex(const int32 ActionButtonIndex) const;
	int32 GetControlGroupActionIndex(const int32 ControlGroupIndex) const;

	FName GetCollisionActionName(const FName& ActionName, const FKey& ProposedKey) const;
	FString GetActionDisplayName(const FName& ActionName) const;
	bool GetIsSpecialBindingAction(const FName& ActionName) const;
	bool ValidateSpecialBinding(UInputAction* ActionToBind, const FKey& ProposedKey);

	UInputAction* GetInputActionByName(const FName& ActionName) const;
	FKey GetCurrentKeyForAction(const FName& ActionName) const;

	UInputMappingContext* GetDefaultMappingContext() const;

	bool GetIsValidPlayerController() const;
	bool GetIsValidMainGameUI() const;
	bool GetIsValidKeyBindingsList() const;
	bool GetIsValidKeyBindingEntryClass() const;
	bool GetIsValidKeyBindingPopupClass() const;
	bool GetIsValidButtonBack() const;
	bool GetIsValidSearchKeyBar() const;
	bool GetIsValidHotkeyProviderSubsystem() const;
	bool GetIsValidActionButtonHotKey(const int32 ActionSlotIndex) const;
	bool GetIsValidControlGroupHotKey(const int32 ControlGroupIndex) const;

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleSearchTextChanged(const FText& NewText);

	UFUNCTION()
	void HandleActionButton1Clicked();
	UFUNCTION()
	void HandleActionButton2Clicked();
	UFUNCTION()
	void HandleActionButton3Clicked();
	UFUNCTION()
	void HandleActionButton4Clicked();
	UFUNCTION()
	void HandleActionButton5Clicked();
	UFUNCTION()
	void HandleActionButton6Clicked();
	UFUNCTION()
	void HandleActionButton7Clicked();
	UFUNCTION()
	void HandleActionButton8Clicked();
	UFUNCTION()
	void HandleActionButton9Clicked();
	UFUNCTION()
	void HandleActionButton10Clicked();
	UFUNCTION()
	void HandleActionButton11Clicked();
	UFUNCTION()
	void HandleActionButton12Clicked();
	UFUNCTION()
	void HandleActionButton13Clicked();
	UFUNCTION()
	void HandleActionButton14Clicked();
	UFUNCTION()
	void HandleActionButton15Clicked();

	UFUNCTION()
	void HandleControlGroupButton1Clicked();
	UFUNCTION()
	void HandleControlGroupButton2Clicked();
	UFUNCTION()
	void HandleControlGroupButton3Clicked();
	UFUNCTION()
	void HandleControlGroupButton4Clicked();
	UFUNCTION()
	void HandleControlGroupButton5Clicked();
	UFUNCTION()
	void HandleControlGroupButton6Clicked();
	UFUNCTION()
	void HandleControlGroupButton7Clicked();
	UFUNCTION()
	void HandleControlGroupButton8Clicked();
	UFUNCTION()
	void HandleControlGroupButton9Clicked();
	UFUNCTION()
	void HandleControlGroupButton10Clicked();

	void HandleKeyBindingUpdated(UInputAction* ActionToBind, const FKey& NewKey);
	void HandlePopupUnbindRequested(UInputAction* ActionToUnbind, const FKey& CurrentKey);
	void HandlePopupConfirmExitRequested();
	void HandlePopupCancelExitRequested();

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> M_KeyBindingsList = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> M_SearchKeyBar = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonBack = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton1 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey1 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton2 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey2 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton3 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey3 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton4 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey4 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton5 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey5 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton6 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey6 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton7 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey7 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton8 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey8 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton9 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey9 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton10 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey10 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton11 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey11 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton12 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey12 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton13 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey13 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton14 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey14 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ActionButton15 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ActionButtonHotKey15 = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton1 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey1 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton2 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey2 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton3 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey3 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton4 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey4 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton5 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey5 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton6 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey6 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton7 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey7 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton8 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey8 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton9 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey9 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ControlGroupButton10 = nullptr;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UW_HotKey> M_ControlGroupHotKey10 = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UW_KeyBindingPopup> M_KeyBindingPopup = nullptr;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UW_EscapeMenuKeyBindingEntry>> M_ActionNameToEntry;

	UPROPERTY(Transient)
	TMap<FName, TWeakObjectPtr<UInputAction>> M_ActionNameToAction;

	TMap<FName, FKey> M_SpecialActionKeyBindings;

	UPROPERTY(Transient)
	TArray<FEscapeMenuKeyBindingEntryData> M_KeyBindingEntries;

	UPROPERTY(Transient)
	TWeakObjectPtr<URTSHotkeyProviderSubsystem> M_HotkeyProviderSubsystem;

	FDelegateHandle M_ActionSlotHotkeyHandle;
	FDelegateHandle M_ControlGroupHotkeyHandle;

	bool bM_HasConstructed = false;
};
