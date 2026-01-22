// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "StartGameWidget.generated.h"

class UButton;
class UPlayerStartGameControl;

/**
 * @brief Added to the viewport to gate gameplay until the player confirms the start.
 */
UCLASS()
class RTS_SURVIVAL_API UStartGameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitStartGameWidget(UPlayerStartGameControl* StartGameControlComponent);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void OnStartGameButtonClicked();

	bool GetIsValidStartGameButton() const;
	bool GetIsValidStartGameControlComponent() const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_StartGameButton;

	UPROPERTY()
	TWeakObjectPtr<UPlayerStartGameControl> M_StartGameControlComponent;
};
