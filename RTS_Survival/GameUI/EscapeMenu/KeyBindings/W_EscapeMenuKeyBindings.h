#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_EscapeMenuKeyBindings.generated.h"

class ACPPController;
class UButton;
class UMainGameUI;
class UVerticalBox;
class UW_EscapeMenuKeyBindingEntry;
class UInputMappingContext;

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

protected:
	virtual void NativeConstruct() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "KeyBindings")
	TSubclassOf<UW_EscapeMenuKeyBindingEntry> M_KeyBindingEntryClass;

private:
	void BindButtonCallbacks();
	void BindBackButton();
	void BuildKeyBindingEntries();

	UInputMappingContext* GetDefaultMappingContext() const;

	bool GetIsValidPlayerController() const;
	bool GetIsValidMainGameUI() const;
	bool GetIsValidKeyBindingsList() const;
	bool GetIsValidKeyBindingEntryClass() const;
	bool GetIsValidButtonBack() const;

	UFUNCTION()
	void HandleBackClicked();

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> M_KeyBindingsList = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonBack = nullptr;
};
