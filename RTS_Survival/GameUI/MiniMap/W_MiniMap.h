// W_MiniMap.h
// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "RTS_Survival/GameUI/MiniMap/RTSMinimapIconHelpers.h"
#include "W_MiniMap.generated.h"

struct FRTSMinimapCustomIconDrawData;
class AFowManager;
class UImage;
class UTexture2D;

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

	AFowManager* GetIsValidFowManager();

	/** @brief Sets up the dynamic material’s Active/Passive render targets and Fog of War manager. */
	void InitMiniMapRTs(const TObjectPtr<UTexture>& Active,
	                    const TObjectPtr<UTexture>& Passive,
	                    const TObjectPtr<AFowManager>& FowManager);

	/** Fired when the user clicks the mini-map; UV coordinates in [0,1]. */
	UPROPERTY(BlueprintAssignable, Category="MiniMap")
	FOnMiniMapClicked OnMiniMapClicked;

protected:
	/** Bound widget in the UMG designer. */
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> MiniMapImg;

	virtual void NativeConstruct() override;

	virtual int32 NativePaint(const FPaintArgs& Args,
	                          const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect,
	                          FSlateWindowElementList& OutDrawElements,
	                          int32 LayerId,
	                          const FWidgetStyle& InWidgetStyle,
	                          bool bParentEnabled) const override;

	/** Override to detect left‐mouse clicks on the mini‐map. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
	                                       const FPointerEvent& InMouseEvent) override;

private:
	int32 DrawMiniMapUnitColorIcons(const FGeometry& AllottedGeometry,
	                                FSlateWindowElementList& OutDrawElements,
	                                const int32 LayerId);

	int32 DrawCustomMiniMapTextureIcons(const FGeometry& AllottedGeometry,
	                                   FSlateWindowElementList& OutDrawElements,
	                                   const int32 LayerId) const;

	void DrawCustomMiniMapTextureIcon(const FRTSMinimapCustomIconDrawData& IconDrawData,
	                                  const FGeometry& MiniMapGeometry,
	                                  const FGeometry& AllottedGeometry,
	                                  const FVector2D& MiniMapSize,
	                                  FSlateWindowElementList& OutDrawElements,
	                                  const int32 LayerId);

	FSlateBrush* GetCustomMiniMapIconBrush(UTexture2D* Texture);

	UPROPERTY()
	TObjectPtr<AFowManager> M_FowManager = nullptr;

	bool bM_HasReportedMissingFowManager = false;

	UPROPERTY()
	FSlateRoundedBoxBrush M_MinimapIconBrush = FSlateRoundedBoxBrush(FLinearColor::White);

	UPROPERTY()
	TMap<UTexture2D*, FSlateBrush> M_CustomMinimapIconBrushes;
};
