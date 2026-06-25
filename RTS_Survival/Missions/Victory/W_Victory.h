#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_Victory.generated.h"

class UButton;

/**
 * @brief Mission victory menu that receives its level targets from the mission manager before being shown.
 * @note TriggerVictory: call before adding to viewport so the buttons know which levels to load.
 */
UCLASS()
class RTS_SURVIVAL_API UW_Victory : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Mission|Victory")
	void TriggerVictory(TSoftObjectPtr<UWorld> MapToLoadOnContinueVictory,
	                    TSoftObjectPtr<UWorld> MenuLevelToLoad,
	                    bool bShowBackToMenu);

protected:
	virtual void NativeConstruct() override;

private:
	void BindButtonCallbacks();
	void BindContinueButton();
	void BindBackToMainMenuButton();
	void ApplyBackToMainMenuVisibility() const;
	void OpenConfiguredLevel(const TSoftObjectPtr<UWorld>& LevelToLoad, const FString& ActionName) const;
	bool GetIsValidContinueButton(const FString& FunctionName) const;
	bool GetIsValidBackToMainMenuButton(const FString& FunctionName) const;
	bool EnsureLevelIsValid(const TSoftObjectPtr<UWorld>& LevelToLoad, const FString& ActionName) const;

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleBackToMainMenuClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_Continue = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_BackToMainMenu = nullptr;

	UPROPERTY()
	TSoftObjectPtr<UWorld> M_MapToLoadOnContinueVictory;

	UPROPERTY()
	TSoftObjectPtr<UWorld> M_MenuLevelToLoad;

	bool bM_ShowBackToMenu = true;
};
