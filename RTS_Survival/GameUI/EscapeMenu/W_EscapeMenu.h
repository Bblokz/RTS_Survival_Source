#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_EscapeMenu.generated.h"

class ACPPController;
class UButton;
class UMainGameUI;

/** @brief Escape menu root widget; it delegates all button actions to the main game UI manager. */
UCLASS()
class RTS_SURVIVAL_API UW_EscapeMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayerController(ACPPController* NewPlayerController);

protected:
	virtual void NativeConstruct() override;

private:
	void BindButtonCallbacks();
	void BindResumeGameButton();
	void BindSettingsButton();
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
	TObjectPtr<UButton> M_ButtonArchive = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonRestartLevel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonExitToMainMenu = nullptr;
};
