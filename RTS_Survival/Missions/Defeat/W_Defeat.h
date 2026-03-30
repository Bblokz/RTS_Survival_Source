#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_Defeat.generated.h"

class UButton;

/**
 * @brief Mission defeat menu that exposes restart and quit actions for blueprint-styled game-over flow.
 */
UCLASS()
class RTS_SURVIVAL_API UW_Defeat : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Mission|Defeat")
	void BP_OnRestartMap();

protected:
	virtual void NativeConstruct() override;

private:
	void BindButtonCallbacks();
	void BindQuitButton();
	void BindRestartButton();
	bool EnsureButtonIsValid(const UButton* ButtonToCheck, const FString& ButtonName, const FString& FunctionName) const;

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleRestartClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_Quit = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_Restart = nullptr;
};
