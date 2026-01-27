// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Player/CPPController.h"
#include "TechTree.generated.h"

class UW_PlayerResource;
class RTS_SURVIVAL_API UMainGameUI;
class UButton;
class UCanvasPanel;
/**
 * @brief Widget that presents the tech tree and handles panning with UI setup from the main game UI.
 */
UCLASS()
class RTS_SURVIVAL_API UTechTree : public UUserWidget
{
	GENERATED_BODY()
public:
	inline void SetMainGameUI(UMainGameUI* NewMainGameUI){M_MainGameUI = NewMainGameUI;};
	inline void SetPlayerController(ACPPController* NewPlayerController){M_PlayerController = NewPlayerController;};
	

protected:
	UFUNCTION(BlueprintCallable)
	void GoBackToMainGameUI();
	virtual void BeginDestroy() override;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UButton* M_BackButton;

	// Mouse events for panning
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// TechTreeCanvas holds all tech items
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* TechTreeCanvas;

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* ResourcesCanvas;

	UFUNCTION(BlueprintCallable)
	void SetupResources(TArray<UW_PlayerResource*> TResourceWidgets);

private:
	// Panning variables
	bool bIsPanning;
	FVector2D LastMousePosition;

	// Panning limits
	FVector2D MinPanPosition;
	FVector2D MaxPanPosition;

	// Panning positions
	FVector2D CurrentTranslation;
	FVector2D TargetTranslation;

	UPROPERTY()
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	void NativeConstruct();
	// Initialize panning bounds based on tech tree size and viewport size
	void InitializePanBounds();
	bool GetIsValidPlayerController();
	bool GetIsValidMainGameUI() const;
};
