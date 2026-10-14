// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "W_FullMapView.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/GlassButtons/W_GlassIconButton.h"
#include "RTS_Survival/GameUI/MiniMap/W_MiniMap.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace FullMapViewConstants
{
	const FLinearColor BackdropColor(0.004f, 0.005f, 0.006f, 0.84f);

	// Band of the frame plate around the map; the grid labels sit in it. Matches the Custom_MapFrame slice margin
	// (80 texels at 2 texels per unit), so the map covers the plate exactly up to its raised lip.
	constexpr float FramePaddingUnits = 40.0f;
	// The map must leave room for the title above and the hint below the frame.
	constexpr float MaxScreenWidthShare = 0.9f;

	constexpr float CloseButtonSize = 46.0f;
	// The close button sits on the frame's top right corner, half over the rim.
	constexpr float CloseButtonOverhang = 14.0f;

	// The large map has far more pixels than the minimap: more, finer scratch cells keep the same look.
	constexpr float LargeMapScratchDensityScale = 2.5f;

	constexpr int32 GridCells = 8;
	constexpr float GridTickLength = 7.0f;
	constexpr float GridTickThickness = 1.0f;
	const FLinearColor GridLabelColor(0.62f, 0.9f, 1.0f, 0.92f);
	const FLinearColor GridTickColor(0.0f, 0.9f, 1.0f, 0.55f);
	constexpr int32 GridLabelFontSize = 12;

	constexpr int32 TitleFontSize = 22;
	constexpr int32 TitleLetterSpacing = 320;
	constexpr float TitleGapAboveFrame = 26.0f;
	const FLinearColor TitleColor(0.86f, 0.93f, 0.96f, 1.0f);

	constexpr int32 HintFontSize = 11;
	constexpr float HintGapBelowFrame = 18.0f;
	const FLinearColor HintColor(0.55f, 0.62f, 0.66f, 0.9f);

	constexpr int32 TextOutlineSize = 1;

	const FName CameraFrameTopParameter(TEXT("P1P2"));
	const FName CameraFrameBottomParameter(TEXT("P3P4"));
	const FName ActiveVisionParameter(TEXT("Active"));
	const FName PassiveVisionParameter(TEXT("Passive"));
	const FName TerrainParameter(TEXT("Terrain"));
}

void UW_FullMapView::InitFullMapView(UW_MiniMap* SmallMiniMap, UMainGameUI* OwningMainGameUI)
{
	M_SmallMiniMap = SmallMiniMap;
	M_MainGameUI = OwningMainGameUI;
	if (not GetIsValidSmallMiniMap() || not GetIsValidLargeMap())
	{
		return;
	}
	InitLargeMapTextures(*SmallMiniMap);
	// Same start direction as the minimap, so north points the same way in both.
	M_LargeMap->SetRenderTransformAngle(SmallMiniMap->GetRenderTransformAngle());
	ApplyLargeMapGlass(*SmallMiniMap);
	MirrorCameraFrame();
}

void UW_FullMapView::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_FullMapView::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	FitFrameToViewport(MyGeometry.GetLocalSize());
	// The camera pawn only updates the minimap's frustum; the large map follows it.
	MirrorCameraFrame();
}

FReply UW_FullMapView::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The backdrop blocks the world: clicks next to the map must not select or command units.
	return FReply::Handled();
}

FReply UW_FullMapView::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

int32 UW_FullMapView::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                  const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                  const int32 LayerId, const FWidgetStyle& InWidgetStyle,
                                  const bool bParentEnabled) const
{
	const int32 MaxLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
	                                            InWidgetStyle, bParentEnabled);
	return DrawGridCoordinates(AllottedGeometry, OutDrawElements, MaxLayerId + 1);
}

void UW_FullMapView::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UOverlay* const RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FullMapRoot"));
	WidgetTree->RootWidget = RootOverlay;
	BuildBackdrop(*RootOverlay);
	BuildFrame(*RootOverlay);
}

void UW_FullMapView::BuildBackdrop(UOverlay& RootOverlay)
{
	UImage* const Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Backdrop"));
	Backdrop->SetColorAndOpacity(FullMapViewConstants::BackdropColor);
	UOverlaySlot* const BackdropSlot = RootOverlay.AddChildToOverlay(Backdrop);
	BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
	BackdropSlot->SetVerticalAlignment(VAlign_Fill);
}

void UW_FullMapView::BuildFrame(UOverlay& RootOverlay)
{
	M_FrameSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FrameSizeBox"));
	UOverlaySlot* const FrameSlot = RootOverlay.AddChildToOverlay(M_FrameSizeBox);
	FrameSlot->SetHorizontalAlignment(HAlign_Center);
	FrameSlot->SetVerticalAlignment(VAlign_Center);

	UOverlay* const FrameOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("FrameOverlay"));
	M_FrameSizeBox->SetContent(FrameOverlay);

	UBorder* const FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FrameBorder"));
	FrameBorder->SetBrush(MakeFrameBrush());
	FrameBorder->SetPadding(FMargin(FullMapViewConstants::FramePaddingUnits));
	UOverlaySlot* const BorderSlot = FrameOverlay->AddChildToOverlay(FrameBorder);
	BorderSlot->SetHorizontalAlignment(HAlign_Fill);
	BorderSlot->SetVerticalAlignment(VAlign_Fill);

	const TSubclassOf<UW_MiniMap> MiniMapClass = M_MiniMapClass.LoadSynchronous();
	if (not MiniMapClass)
	{
		RTSFunctionLibrary::ReportError("The full map view could not load its minimap class."
			"\n See UW_FullMapView::BuildFrame");
	}
	else
	{
		M_LargeMap = WidgetTree->ConstructWidget<UW_MiniMap>(MiniMapClass, TEXT("LargeMap"));
		M_LargeMap->OnMiniMapClicked.AddUniqueDynamic(this, &UW_FullMapView::OnLargeMapClicked);
		FrameBorder->SetContent(M_LargeMap);
	}
	BuildCloseButton(*FrameOverlay);
}

void UW_FullMapView::BuildCloseButton(UOverlay& FrameOverlay)
{
	M_CloseButton = WidgetTree->ConstructWidget<UW_GlassIconButton>(UW_GlassIconButton::StaticClass(),
	                                                                 TEXT("CloseButton"));
	M_CloseButton->SetupGlassButton(M_CloseNormalTexture.LoadSynchronous(), M_CloseHoveredTexture.LoadSynchronous(),
	                                M_ClosePressedTexture.LoadSynchronous(), FullMapViewConstants::CloseButtonSize,
	                                NSLOCTEXT("FullMapView", "CloseTooltip", "Close the map"));
	M_CloseButton->OnGlassButtonClicked.AddUObject(this, &UW_FullMapView::OnCloseClicked);
	UOverlaySlot* const CloseSlot = FrameOverlay.AddChildToOverlay(M_CloseButton);
	CloseSlot->SetHorizontalAlignment(HAlign_Right);
	CloseSlot->SetVerticalAlignment(VAlign_Top);
	const float Overhang = FullMapViewConstants::CloseButtonOverhang;
	CloseSlot->SetPadding(FMargin(0.0f, -Overhang, -Overhang, 0.0f));
}

FSlateBrush UW_FullMapView::MakeFrameBrush() const
{
	// Slice materials draw at their BrushSize and tile, so the corners keep their size at any frame size.
	constexpr float FrameBrushSizeUnits = 128.0f;
	FSlateBrush Brush;
	Brush.SetResourceObject(M_FrameMaterial.LoadSynchronous());
	Brush.ImageSize = FVector2D(FrameBrushSizeUnits, FrameBrushSizeUnits);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = ESlateBrushTileType::Both;
	Brush.Margin = FMargin(0.0f);
	return Brush;
}

void UW_FullMapView::InitLargeMapTextures(UW_MiniMap& SmallMiniMap) const
{
	UImage* const SmallMapImage = SmallMiniMap.GetIsValidMiniMapImg();
	if (not IsValid(SmallMapImage))
	{
		return;
	}
	const UMaterialInstanceDynamic* const SmallMaterial = SmallMapImage->GetDynamicMaterial();
	if (not IsValid(SmallMaterial))
	{
		return;
	}
	UTexture* ActiveVision = nullptr;
	UTexture* PassiveVision = nullptr;
	UTexture* Terrain = nullptr;
	SmallMaterial->GetTextureParameterValue(FMaterialParameterInfo(FullMapViewConstants::ActiveVisionParameter), ActiveVision);
	SmallMaterial->GetTextureParameterValue(FMaterialParameterInfo(FullMapViewConstants::PassiveVisionParameter), PassiveVision);
	SmallMaterial->GetTextureParameterValue(FMaterialParameterInfo(FullMapViewConstants::TerrainParameter), Terrain);
	M_LargeMap->InitMiniMapRTs(ActiveVision, PassiveVision, Terrain, SmallMiniMap.GetIsValidFowManager());
}

void UW_FullMapView::ApplyLargeMapGlass(const UW_MiniMap& SmallMiniMap) const
{
	FMiniMapGlassSettings GlassSettings = SmallMiniMap.GetGlassSettings();
	GlassSettings.GlassWidth = M_LargeMapGlassWidth;
	GlassSettings.ScratchDensity *= FullMapViewConstants::LargeMapScratchDensityScale;
	M_LargeMap->SetGlassSettings(GlassSettings);
}

void UW_FullMapView::FitFrameToViewport(const FVector2D& ViewportSize)
{
	if (not IsValid(M_FrameSizeBox) || ViewportSize.Equals(M_SizedForViewport))
	{
		return;
	}
	M_SizedForViewport = ViewportSize;
	const float FrameSide = FMath::Min(ViewportSize.Y * M_ScreenHeightShare,
	                                   ViewportSize.X * FullMapViewConstants::MaxScreenWidthShare);
	M_FrameSizeBox->SetWidthOverride(FMath::RoundToFloat(FrameSide));
	M_FrameSizeBox->SetHeightOverride(FMath::RoundToFloat(FrameSide));
}

void UW_FullMapView::MirrorCameraFrame() const
{
	// Runs every tick: silent checks, InitFullMapView reports missing maps once when the view opens.
	if (not M_SmallMiniMap.IsValid() || not IsValid(M_LargeMap))
	{
		return;
	}
	UImage* const SmallMapImage = M_SmallMiniMap->GetIsValidMiniMapImg();
	UImage* const LargeMapImage = M_LargeMap->GetIsValidMiniMapImg();
	if (not IsValid(SmallMapImage) || not IsValid(LargeMapImage))
	{
		return;
	}
	const UMaterialInstanceDynamic* const SmallMaterial = SmallMapImage->GetDynamicMaterial();
	UMaterialInstanceDynamic* const LargeMaterial = LargeMapImage->GetDynamicMaterial();
	if (not IsValid(SmallMaterial) || not IsValid(LargeMaterial))
	{
		return;
	}
	for (const FName& ParameterName : {
		     FullMapViewConstants::CameraFrameTopParameter, FullMapViewConstants::CameraFrameBottomParameter
	     })
	{
		FLinearColor Corners;
		if (SmallMaterial->GetVectorParameterValue(FMaterialParameterInfo(ParameterName), Corners))
		{
			LargeMaterial->SetVectorParameterValue(ParameterName, Corners);
		}
	}
}

void UW_FullMapView::OnLargeMapClicked(const FVector2D LocalClickUV)
{
	if (not GetIsValidSmallMiniMap())
	{
		return;
	}
	// The camera pawn listens to the minimap; the view stays open so the camera frame can be seen moving.
	M_SmallMiniMap->OnMiniMapClicked.Broadcast(LocalClickUV);
}

void UW_FullMapView::OnCloseClicked()
{
	if (not GetIsValidMainGameUI())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	M_MainGameUI->CloseFullMapView();
}

bool UW_FullMapView::GetLargeMapLocalRect(const FGeometry& AllottedGeometry, FVector2D& OutTopLeft,
                                          FVector2D& OutSize) const
{
	if (not IsValid(M_LargeMap) || not IsValid(M_LargeMap->GetIsValidMiniMapImg()))
	{
		return false;
	}
	// Bounding rect: the map may be rotated by the start direction (a square keeps its bounds).
	const FSlateRect MapRect = M_LargeMap->GetIsValidMiniMapImg()->GetCachedGeometry().GetRenderBoundingRect();
	OutTopLeft = AllottedGeometry.AbsoluteToLocal(MapRect.GetTopLeft());
	OutSize = AllottedGeometry.AbsoluteToLocal(MapRect.GetBottomRight()) - OutTopLeft;
	return OutSize.X > 1.0f && OutSize.Y > 1.0f;
}

int32 UW_FullMapView::DrawGridCoordinates(const FGeometry& AllottedGeometry,
                                          FSlateWindowElementList& OutDrawElements, const int32 LayerId) const
{
	FVector2D MapTopLeft;
	FVector2D MapSize;
	if (not GetLargeMapLocalRect(AllottedGeometry, MapTopLeft, MapSize))
	{
		return LayerId;
	}
	using namespace FullMapViewConstants;
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle(
		TEXT("Bold"), GridLabelFontSize, FFontOutlineSettings(TextOutlineSize));
	const FVector2D MapBottomRight = MapTopLeft + MapSize;
	const FVector2D CellSize = MapSize / GridCells;
	// Labels centred in the frame band, ticks from the map edge into the band at each cell boundary.
	const float BandCenter = FramePaddingUnits * 0.5f;
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	for (int32 CellIndex = 0; CellIndex < GridCells; ++CellIndex)
	{
		const FText ColumnLabel = FText::FromString(FString::Chr(TEXT('A') + CellIndex));
		const FText RowLabel = FText::AsNumber(CellIndex + 1);
		const float ColumnCenter = MapTopLeft.X + CellSize.X * (CellIndex + 0.5f);
		const float RowCenter = MapTopLeft.Y + CellSize.Y * (CellIndex + 0.5f);
		DrawCenteredText(AllottedGeometry, OutDrawElements, LayerId, ColumnLabel, LabelFont,
		                 FVector2D(ColumnCenter, MapTopLeft.Y - BandCenter), GridLabelColor);
		DrawCenteredText(AllottedGeometry, OutDrawElements, LayerId, ColumnLabel, LabelFont,
		                 FVector2D(ColumnCenter, MapBottomRight.Y + BandCenter), GridLabelColor);
		DrawCenteredText(AllottedGeometry, OutDrawElements, LayerId, RowLabel, LabelFont,
		                 FVector2D(MapTopLeft.X - BandCenter, RowCenter), GridLabelColor);
		DrawCenteredText(AllottedGeometry, OutDrawElements, LayerId, RowLabel, LabelFont,
		                 FVector2D(MapBottomRight.X + BandCenter, RowCenter), GridLabelColor);
		if (CellIndex == 0)
		{
			continue;
		}
		const float BoundaryX = MapTopLeft.X + CellSize.X * CellIndex;
		const float BoundaryY = MapTopLeft.Y + CellSize.Y * CellIndex;
		const TArray<TArray<FVector2f>> Ticks = {
			{FVector2f(BoundaryX, MapTopLeft.Y), FVector2f(BoundaryX, MapTopLeft.Y - GridTickLength)},
			{FVector2f(BoundaryX, MapBottomRight.Y), FVector2f(BoundaryX, MapBottomRight.Y + GridTickLength)},
			{FVector2f(MapTopLeft.X, BoundaryY), FVector2f(MapTopLeft.X - GridTickLength, BoundaryY)},
			{FVector2f(MapBottomRight.X, BoundaryY), FVector2f(MapBottomRight.X + GridTickLength, BoundaryY)},
		};
		for (const TArray<FVector2f>& Tick : Ticks)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeometry, Tick, ESlateDrawEffect::None,
			                             GridTickColor, true, GridTickThickness);
		}
	}

	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), TitleFontSize,
	                                                           FFontOutlineSettings(TextOutlineSize));
	TitleFont.LetterSpacing = TitleLetterSpacing;
	const float FrameTop = MapTopLeft.Y - FramePaddingUnits;
	const float FrameBottom = MapBottomRight.Y + FramePaddingUnits;
	const float MapCenterX = MapTopLeft.X + MapSize.X * 0.5f;
	DrawCenteredText(AllottedGeometry, OutDrawElements, LayerId,
	                 NSLOCTEXT("FullMapView", "Title", "TACTICAL MAP"), TitleFont,
	                 FVector2D(MapCenterX, FrameTop - TitleGapAboveFrame), TitleColor);
	const FSlateFontInfo HintFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), HintFontSize,
	                                                                FFontOutlineSettings(TextOutlineSize));
	DrawCenteredText(AllottedGeometry, OutDrawElements, LayerId,
	                 NSLOCTEXT("FullMapView", "Hint", "Click the map to move the camera there"), HintFont,
	                 FVector2D(MapCenterX, FrameBottom + HintGapBelowFrame), HintColor);
	return LayerId;
}

void UW_FullMapView::DrawCenteredText(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                                      const int32 LayerId, const FText& Text, const FSlateFontInfo& FontInfo,
                                      const FVector2D& Center, const FLinearColor& Color) const
{
	if (not FSlateApplication::IsInitialized())
	{
		return;
	}
	const FSlateRenderer* const SlateRenderer = FSlateApplication::Get().GetRenderer();
	if (SlateRenderer == nullptr)
	{
		return;
	}
	const FVector2D TextSize = SlateRenderer->GetFontMeasureService()->Measure(Text, FontInfo);
	const FPaintGeometry TextGeometry = AllottedGeometry.ToPaintGeometry(
		TextSize, FSlateLayoutTransform(Center - TextSize * 0.5f));
	FSlateDrawElement::MakeText(OutDrawElements, LayerId, TextGeometry, Text, FontInfo, ESlateDrawEffect::None,
	                            Color);
}

bool UW_FullMapView::GetIsValidLargeMap() const
{
	if (IsValid(M_LargeMap))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_LargeMap", "GetIsValidLargeMap", this);
	return false;
}

bool UW_FullMapView::GetIsValidSmallMiniMap() const
{
	if (M_SmallMiniMap.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_SmallMiniMap", "GetIsValidSmallMiniMap",
	                                                            this);
	return false;
}

bool UW_FullMapView::GetIsValidMainGameUI() const
{
	if (M_MainGameUI.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_MainGameUI", "GetIsValidMainGameUI", this);
	return false;
}
