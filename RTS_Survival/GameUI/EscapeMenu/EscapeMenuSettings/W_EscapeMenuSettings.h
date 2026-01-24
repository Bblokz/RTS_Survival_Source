#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_EscapeMenuSettings.generated.h"

class ACPPController;
class UButton;
class UMainGameUI;

/** @brief Escape menu settings widget; it can be opened over the escape menu and closed back into it. */
UCLASS()
class RTS_SURVIVAL_API UW_EscapeMenuSettings : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayerController(ACPPController* NewPlayerController);

protected:
	virtual void NativeConstruct() override;

private:
	void BindButtonCallbacks();
	void BindCloseButton();

	bool GetIsValidMainGameUI() const;

	UFUNCTION()
	void HandleCloseClicked();

	bool EnsureButtonIsValid(const UButton* ButtonToCheck, const FString& ButtonName, const FString& FunctionName) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ButtonCloseSettings = nullptr;
};
