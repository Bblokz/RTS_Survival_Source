// W_MiniMap.h
// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_MiniMap.generated.h"

class UImage;

/** Delegate broadcast whenever the mini‐map is clicked, giving UV in [0,1]. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMiniMapClicked, FVector2D, LocalClickUV);

/**
 * @brief Widget displaying the world mini‐map and handling click input.
 * @note NativeOnMouseButtonDown: catches clicks on the widget and fires OnMiniMapClicked.
 */
UCLASS()
class RTS_SURVIVAL_API UW_MiniMap : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @return the UImage used to render the mini‐map, or nullptr (and logs) if invalid */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UImage* GetIsValidMiniMapImg() const;

	/** @brief Sets up the dynamic material’s Active/Passive render targets. */
	void InitMiniMapRTs(const TObjectPtr<UTexture>& Active,
	                    const TObjectPtr<UTexture>& Passive) const;

	/** Fired when the user clicks the mini-map; UV coordinates in [0,1]. */
	UPROPERTY(BlueprintAssignable, Category="MiniMap")
	FOnMiniMapClicked OnMiniMapClicked;

protected:
	/** Bound widget in the UMG designer. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> MiniMapImg;

	/** Override to detect left‐mouse clicks on the mini‐map. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
	                                       const FPointerEvent& InMouseEvent) override;
};
