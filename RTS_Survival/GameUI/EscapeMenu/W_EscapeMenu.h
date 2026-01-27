#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_EscapeMenu.generated.h"

class ACPPController;
class UButton;
class UMainGameUI;

/**
 * @brief Escape menu root widget that forwards button intent to the main game UI manager.
 *
 * Designers style this widget in Blueprint while C++ keeps the callbacks wired to gameplay flow.
 */
UCLASS()
class RTS_SURVIVAL_API UW_EscapeMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Provides the owning controller so button callbacks can reach gameplay systems.
	 * @param NewPlayerController Player controller that owns the escape menu instance.
	 */
	void SetPlayerController(ACPPController* NewPlayerController);

protected:
	virtual void NativeConstruct() override;

private:
	void BindButtonCallbacks();
	void BindResumeGameButton();
	void BindSettingsButton();
	void BindKeyBindingsButton();
	void BindArchiveButton();
	void BindRestartLevelButton();
	void BindExitToMainMenuButton();

	bool GetIsValidPlayerController() const;
	bool GetIsValidMainGameUI() const;

	UFUNCTION()
	void HandleResumeGameClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleKeyBindingsClicked();

	UFUNCTION()
	void HandleArchiveClicked();

	UFUNCTION()
	void HandleRestartLevelClicked();

	UFUNCTION()
	void HandleExitToMainMenuClicked();

	bool EnsureButtonIsValid(const UButton* ButtonToCheck, const FString& ButtonName, const FString& FunctionName) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonResumeGame = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonSettings = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonKeyBindings = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonArchive = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonRestartLevel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonExitToMainMenu = nullptr;
};
