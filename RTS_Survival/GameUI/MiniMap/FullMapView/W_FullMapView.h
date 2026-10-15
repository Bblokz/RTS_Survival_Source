// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

#include "W_FullMapView.generated.h"

class UOverlay;
class UVerticalBox;
class UWidget;
class UMainGameUI;
class UMaterialInterface;
class USizeBox;
class UTexture2D;
class UW_GlassIconButton;
class UW_MapObjectFilterToggle;
class UW_MiniMap;

/**
 * @brief Full-screen map (like opening the map in an RPG): a large copy of the minimap with the same terrain, fog
 * of war, icons and camera frame, inside a detailed frame with grid coordinates. UMainGameUI opens it from the
 * minimap's map button and hides itself meanwhile; the close button (or clicking a spot, which also moves the
 * camera there) restores the normal game UI. Builds its own widget tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FullMapView : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Copies the small minimap's textures, fog of war manager, rotation and glass into the large map.
	 * @param SmallMiniMap The main game UI's minimap the large map mirrors.
	 * @param OwningMainGameUI Restored when the view closes.
	 */
	void InitFullMapView(UW_MiniMap* SmallMiniMap, UMainGameUI* OwningMainGameUI);

	/** Called by UMainGameUI when the view closes, so map object filters shown only here stop their work. */
	void OnFullMapViewClosed() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                          FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                          bool bParentEnabled) const override;

	// The minimap blueprint used for the large map (its material and icon drawing).
	UPROPERTY(EditAnywhere, Category = "FullMap")
	TSoftClassPtr<UW_MiniMap> M_MiniMapClass = TSoftClassPtr<UW_MiniMap>(
		FSoftObjectPath(TEXT("/Game/RTS_Survival/Blueprints/GameUI/MiniMap/W_BP_MiniMap.W_BP_MiniMap_C")));

	// Frame around the large map (custom_parts.py Custom_MapFrame slice set).
	UPROPERTY(EditAnywhere, Category = "FullMap")
	TSoftObjectPtr<UMaterialInterface> M_FrameMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/Instances/MI9_Custom_MapFrame_128x128_u_n.MI9_Custom_MapFrame_128x128_u_n")));

	UPROPERTY(EditAnywhere, Category = "FullMap")
	TSoftObjectPtr<UTexture2D> M_CloseNormalTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/GlassButtons/T_GlassButton_MapClose_n.T_GlassButton_MapClose_n")));

	UPROPERTY(EditAnywhere, Category = "FullMap")
	TSoftObjectPtr<UTexture2D> M_CloseHoveredTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/GlassButtons/T_GlassButton_MapClose_h.T_GlassButton_MapClose_h")));

	UPROPERTY(EditAnywhere, Category = "FullMap")
	TSoftObjectPtr<UTexture2D> M_ClosePressedTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/GlassButtons/T_GlassButton_MapClose_p.T_GlassButton_MapClose_p")));

	// Height of the map frame as a share of the screen height.
	UPROPERTY(EditAnywhere, Category = "FullMap", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float M_ScreenHeightShare = 0.86f;

	// The large map's glass band is thinner than the minimap's (same look, far more pixels).
	UPROPERTY(EditAnywhere, Category = "FullMap", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float M_LargeMapGlassWidth = 0.035f;

private:
	UPROPERTY()
	TObjectPtr<USizeBox> M_FrameSizeBox;

	UPROPERTY()
	TObjectPtr<UW_MiniMap> M_LargeMap;

	UPROPERTY()
	TObjectPtr<UW_GlassIconButton> M_CloseButton;

	// Left of the frame; positioned against the frame whenever the viewport size changes.
	UPROPERTY()
	TObjectPtr<UWidget> M_FilterPanel;

	UPROPERTY()
	TArray<TObjectPtr<UW_MapObjectFilterToggle>> M_FilterToggles;

	TWeakObjectPtr<UW_MiniMap> M_SmallMiniMap;
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	// Viewport size (UI units) the frame was last sized for; the frame only resizes when the viewport changes.
	FVector2D M_SizedForViewport = FVector2D::ZeroVector;

	void BuildWidgetTree();
	void BuildBackdrop(UOverlay& RootOverlay);
	void BuildFrame(UOverlay& RootOverlay);
	void BuildCloseButton(UOverlay& FrameOverlay);
	void BuildFilterPanel(UOverlay& RootOverlay);
	void AddFilterToggles(UVerticalBox& PanelBox);
	void RefreshFilterToggles() const;
	void SetIsTacticalMapOpenForFilters(const bool bIsOpen) const;
	FSlateBrush MakeFrameBrush() const;

	void InitLargeMapTextures(UW_MiniMap& SmallMiniMap) const;
	void ApplyLargeMapGlass(const UW_MiniMap& SmallMiniMap) const;
	void FitFrameToViewport(const FVector2D& ViewportSize);
	void MirrorCameraFrame() const;

	UFUNCTION()
	void OnLargeMapClicked(FVector2D LocalClickUV);

	void OnCloseClicked();

	/**
	 * @brief The grid labels sit in the frame band around the map, so they need the map's on-screen rectangle.
	 * @param AllottedGeometry This widget's paint geometry.
	 * @param OutTopLeft Map top left in this widget's local space.
	 * @param OutSize Map size in this widget's local space.
	 * @return False before the map has been laid out.
	 */
	bool GetLargeMapLocalRect(const FGeometry& AllottedGeometry, FVector2D& OutTopLeft, FVector2D& OutSize) const;

	int32 DrawGridCoordinates(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
	                          const int32 LayerId) const;

	void DrawCenteredText(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
	                      const int32 LayerId, const FText& Text, const FSlateFontInfo& FontInfo,
	                      const FVector2D& Center, const FLinearColor& Color) const;

	bool GetIsValidLargeMap() const;
	bool GetIsValidSmallMiniMap() const;
	bool GetIsValidMainGameUI() const;
};
