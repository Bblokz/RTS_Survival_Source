// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"

#include "W_IdleUnitButton.generated.h"

class UButton;
class UTextBlock;
class UTexture2D;

UENUM(BlueprintType)
enum class EIdleUnitButtonType : uint8
{
	Harvester,
	NomadicTruck
};

/**
 * @brief Glass button in the minimap panel that counts the player's idle harvesters or idle nomadic trucks;
 * each click selects the next idle one and focuses the camera on it. Builds its own widget tree, so it is
 * placed in the HUD as is; the button is disabled while nothing of its type is idle.
 */
UCLASS()
class RTS_SURVIVAL_API UW_IdleUnitButton : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, Category = "IdleUnits")
	EIdleUnitButtonType M_IdleUnitType = EIdleUnitButtonType::Harvester;

	// Glass button art (glass_buttons.py): normal, hovered and pressed state of the same dome.
	UPROPERTY(EditAnywhere, Category = "IdleUnits")
	TObjectPtr<UTexture2D> M_NormalTexture;

	UPROPERTY(EditAnywhere, Category = "IdleUnits")
	TObjectPtr<UTexture2D> M_HoveredTexture;

	UPROPERTY(EditAnywhere, Category = "IdleUnits")
	TObjectPtr<UTexture2D> M_PressedTexture;

	// Edge length of the square button in UI units.
	UPROPERTY(EditAnywhere, Category = "IdleUnits", meta = (ClampMin = "16.0"))
	float M_ButtonSize = 46.0f;

private:
	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CountText;

	FTimerHandle M_RefreshTimerHandle;

	// Position in the idle list for the next click, so repeated clicks walk through every idle unit.
	int32 M_CycleIndex = 0;

	void BuildWidgetTree();
	FButtonStyle MakeGlassButtonStyle() const;
	FSlateBrush MakeGlassBrush(UTexture2D* Texture, const FLinearColor& Tint) const;

	UFUNCTION()
	void RefreshIdleCount();

	UFUNCTION()
	void OnIdleButtonClicked();

	void GatherIdleUnits(TArray<AActor*>& OutIdleUnits) const;
	void GatherIdleHarvesters(TArray<AActor*>& OutIdleUnits) const;
	void GatherIdleNomadicTrucks(TArray<AActor*>& OutIdleUnits) const;

	static bool GetIsOwnedByPlayer(const AActor& Actor);
	static bool GetIsCommandIdle(AActor& Actor);
	FText GetTooltipText(const int32 IdleCount) const;

	bool GetIsValidButton() const;
	bool GetIsValidCountText() const;
};
