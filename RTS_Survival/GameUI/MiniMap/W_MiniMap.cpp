// W_MiniMap.cpp
// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_MiniMap.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/GameUI/MiniMap/CombatPings/RTSCombatPingSettings.h"
#include "RTS_Survival/GameUI/MiniMap/CombatPings/RTSCombatPingSubsystem.h"
#include "RTS_Survival/GameUI/MiniMap/MapObjectFilters/RTSMapObjectFilterSettings.h"
#include "RTS_Survival/GameUI/MiniMap/MapObjectFilters/RTSMapObjectFilterSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MiniMapCombatPingConstants
{
	constexpr float FadeInSeconds = 0.12f;
	constexpr float FadeOutSeconds = 0.6f;

	// Rings start just outside the core and slow down as they reach the full radius.
	constexpr float RippleStartRadiusShare = 0.2f;
	constexpr float RippleEaseExponent = 2.5f;
	constexpr float RippleStartThicknessScale = 1.6f;
	constexpr float RippleEndThicknessScale = 0.6f;

	// Brackets snap in from outside the ripples to just inside them (a lock-on), then turn slowly.
	constexpr int32 BracketCount = 4;
	constexpr float BracketArcDegrees = 50.f;
	constexpr float BracketLockOnSeconds = 0.35f;
	constexpr float BracketStartRadiusShare = 1.6f;
	constexpr float BracketRadiusShare = 0.62f;
	constexpr float BracketThicknessScale = 1.25f;
	constexpr float BracketLockOnEaseExponent = 3.f;

	constexpr float CoreDiameterShare = 0.32f;
	constexpr float CorePulseAmount = 0.18f;
	// The core is lifted towards white so it reads on red terrain and on red enemy icons.
	constexpr float CoreWhiteBlend = 0.4f;

	// Stacked discs of shrinking size fake a soft glow without a texture.
	constexpr int32 GlowDiscCount = 3;
	constexpr float GlowDiameterShare = 1.3f;

	constexpr int32 FullCircleSegments = 48;
	constexpr int32 MinArcSegments = 4;
	constexpr float FullCircleDegrees = 360.f;

	/** @return 0..1 opacity of the whole ping: a quick fade in and a slower fade out at the end of its lifetime. */
	float GetPingEnvelope(const float AgeSeconds, const float LifetimeSeconds)
	{
		const float FadeIn = FMath::Clamp(AgeSeconds / FadeInSeconds, 0.f, 1.f);
		const float FadeOut = FMath::Clamp((LifetimeSeconds - AgeSeconds) / FadeOutSeconds, 0.f, 1.f);
		return FadeIn * FadeOut;
	}

	void DrawArc(const FVector2D& Center, const float Radius, const float StartDegrees, const float ArcDegrees,
	             const float Thickness, const FLinearColor& Color, const FGeometry& AllottedGeometry,
	             FSlateWindowElementList& OutDrawElements, const int32 LayerId)
	{
		const int32 SegmentCount = FMath::Max(
			MinArcSegments, FMath::CeilToInt(FullCircleSegments * ArcDegrees / FullCircleDegrees));
		TArray<FVector2f> Points;
		Points.Reserve(SegmentCount + 1);
		for (int32 PointIndex = 0; PointIndex <= SegmentCount; ++PointIndex)
		{
			const float AngleRadians = FMath::DegreesToRadians(
				StartDegrees + ArcDegrees * PointIndex / static_cast<float>(SegmentCount));
			const FVector2D Direction(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians));
			Points.Add(FVector2f(Center + Direction * Radius));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
		                             ESlateDrawEffect::None, Color, true, Thickness);
	}
}

namespace MiniMapReinforcementsPingConstants
{
	constexpr float FillOpacity = 0.28f;
	// Space between the label and the ring, as a share of the label height.
	constexpr float TextPaddingShare = 0.6f;
	constexpr int32 TextOutlineSizePixels = 1;

	// One ring keeps leaving the circle so the spot catches the eye without hiding the label.
	constexpr float RipplePeriodSeconds = 1.5f;
	constexpr float RippleEndRadiusShare = 1.45f;
	constexpr float RippleEaseExponent = 2.f;
	constexpr float RippleOpacityShare = 0.7f;

	// The ring breathes slightly over the same period.
	constexpr float RingPulseAmount = 0.04f;
	constexpr float TextWhiteBlend = 0.75f;
}

UImage* UW_MiniMap::GetIsValidMiniMapImg() const
{
	if (not IsValid(MiniMapImg))
	{
		RTSFunctionLibrary::ReportError("MiniMap widget could not provide valid MiniMapImage");
		return nullptr;
	}
	return MiniMapImg;
}

AFowManager* UW_MiniMap::GetIsValidFowManager() const
{
	if (IsValid(M_FowManager))
	{
		bM_HasReportedMissingFowManager = false;
		return M_FowManager;
	}

	if (not bM_HasReportedMissingFowManager)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_FowManager",
			"GetIsValidFowManager",
			this);
		bM_HasReportedMissingFowManager = true;
	}
	return nullptr;
}

void UW_MiniMap::InitMiniMapRTs(const TObjectPtr<UTexture>& Active,
                                const TObjectPtr<UTexture>& Passive,
                                const TObjectPtr<UTexture>& Terrain,
                                const TObjectPtr<AFowManager>& FowManager)
{
	M_FowManager = FowManager;
	bM_HasReportedMissingFowManager = false;

	UImage* const MiniMapImage = GetIsValidMiniMapImg();
	if (not Active || not Passive || not Terrain || not MiniMapImage || not GetIsValidFowManager())
	{
		return;
	}

	UMaterialInstanceDynamic* const DynamicMaterial = MiniMapImage->GetDynamicMaterial();
	if (not IsValid(DynamicMaterial))
	{
		RTSFunctionLibrary::ReportError("Could not get valid dynamic material for MiniMapImg\nSee UW_MiniMap::InitMiniMapRTs");
		return;
	}

	DynamicMaterial->SetTextureParameterValue(FName("Active"), Active);
	DynamicMaterial->SetTextureParameterValue(FName("Passive"), Passive);
	DynamicMaterial->SetTextureParameterValue(FName("Terrain"), Terrain);
	RefreshCustomMiniMapIconBrushes();
	ApplyGlassSettings();
}

void UW_MiniMap::SetGlassSettings(const FMiniMapGlassSettings& NewGlassSettings)
{
	M_GlassSettings = NewGlassSettings;
	ApplyGlassSettings();
}

void UW_MiniMap::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Also runs in the UMG designer, so edits to the glass settings preview live.
	EnsureGlassOverlayMaterial();
	ApplyGlassSettings();
}

void UW_MiniMap::NativeConstruct()
{
	Super::NativeConstruct();

	ForceVolatile(true);
	SetClipping(EWidgetClipping::ClipToBoundsAlways);
	M_MinimapIconBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	EnsureGlassOverlayMaterial();
	ApplyGlassSettings();
}

void UW_MiniMap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The start direction rotates the widget after construction; keep the glass lit from the screen's top left.
	if (not FMath::IsNearlyEqual(GetRenderTransformAngle(), M_GlassAppliedAngle))
	{
		ApplyGlassSettings();
	}

	AFowManager* const FowManager = GetIsValidFowManager();
	if (not IsValid(FowManager))
	{
		return;
	}

	FowManager->RefreshCustomMiniMapIconDrawDataForMiniMap();
}

int32 UW_MiniMap::NativePaint(const FPaintArgs& Args,
                              const FGeometry& AllottedGeometry,
                              const FSlateRect& MyCullingRect,
                              FSlateWindowElementList& OutDrawElements,
                              int32 LayerId,
                              const FWidgetStyle& InWidgetStyle,
                              bool bParentEnabled) const
{
	const int32 NextLayerId = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	const int32 FilterLayerId = DrawMapObjectFilters(AllottedGeometry, OutDrawElements, NextLayerId);
	const int32 IconLayerId = DrawMiniMapUnitColorIcons(AllottedGeometry, OutDrawElements, FilterLayerId);
	const int32 CustomIconLayerId = DrawCustomMiniMapTextureIcons(AllottedGeometry, OutDrawElements, IconLayerId);
	const int32 TextLayerId = DrawCustomMiniMapText(AllottedGeometry, OutDrawElements, CustomIconLayerId);
	const int32 CombatPingLayerId = DrawCombatPings(AllottedGeometry, OutDrawElements, TextLayerId);
	const int32 ReinforcementsLayerId = DrawReinforcementsPings(AllottedGeometry, OutDrawElements, CombatPingLayerId);
	return DrawGlassOverlay(AllottedGeometry, OutDrawElements, ReinforcementsLayerId);
}

void UW_MiniMap::EnsureGlassOverlayMaterial()
{
	if (IsValid(M_GlassOverlayMaterialInstance))
	{
		return;
	}
	UMaterialInterface* const GlassMaterial = M_GlassOverlayMaterial.LoadSynchronous();
	if (not IsValid(GlassMaterial))
	{
		RTSFunctionLibrary::ReportError(
			"MiniMap glass overlay material is not set or could not be loaded."
			"\n See UW_MiniMap::EnsureGlassOverlayMaterial");
		return;
	}
	M_GlassOverlayMaterialInstance = UMaterialInstanceDynamic::Create(GlassMaterial, this);
	M_GlassOverlayBrush.SetResourceObject(M_GlassOverlayMaterialInstance);
	M_GlassOverlayBrush.DrawAs = ESlateBrushDrawType::Image;
}

void UW_MiniMap::ApplyGlassSettings()
{
	M_GlassAppliedAngle = GetRenderTransformAngle();
	if (IsValid(M_GlassOverlayMaterialInstance))
	{
		ApplyGlassParameters(*M_GlassOverlayMaterialInstance);
	}
	if (not IsValid(MiniMapImg))
	{
		return;
	}
	if (UMaterialInstanceDynamic* const MapMaterial = MiniMapImg->GetDynamicMaterial())
	{
		ApplyGlassParameters(*MapMaterial);
	}
}

void UW_MiniMap::ApplyGlassParameters(UMaterialInstanceDynamic& MaterialInstance) const
{
	const FMiniMapGlassSettings& Glass = M_GlassSettings;
	const float GlassWidth = Glass.bShowGlass ? Glass.GlassWidth : 0.0f;
	MaterialInstance.SetScalarParameterValue(TEXT("GlassWidth"), GlassWidth);
	MaterialInstance.SetScalarParameterValue(TEXT("GlassCornerRadius"), Glass.CornerRadius);
	MaterialInstance.SetScalarParameterValue(TEXT("Refraction"), Glass.bShowGlass ? Glass.Refraction : 0.0f);
	MaterialInstance.SetScalarParameterValue(TEXT("FresnelPower"), Glass.FresnelPower);
	MaterialInstance.SetVectorParameterValue(TEXT("EdgeTint"), Glass.EdgeTint);
	MaterialInstance.SetScalarParameterValue(TEXT("RimIntensity"), Glass.RimIntensity);
	MaterialInstance.SetScalarParameterValue(TEXT("RimWidthPx"), Glass.RimWidthPx);
	MaterialInstance.SetScalarParameterValue(TEXT("InnerShadow"), Glass.InnerShadow);
	MaterialInstance.SetScalarParameterValue(TEXT("StreakIntensity"), Glass.StreakIntensity);
	MaterialInstance.SetScalarParameterValue(TEXT("StreakAngle"), Glass.StreakAngleDegrees);
	MaterialInstance.SetScalarParameterValue(TEXT("StreakWidth"), Glass.StreakWidth);
	MaterialInstance.SetScalarParameterValue(TEXT("StreakOffset"), Glass.StreakOffset);
	MaterialInstance.SetScalarParameterValue(TEXT("FrostIntensity"), Glass.FrostIntensity);
	MaterialInstance.SetScalarParameterValue(TEXT("FrostScale"), Glass.FrostScale);
	MaterialInstance.SetScalarParameterValue(TEXT("ScratchIntensity"), Glass.ScratchIntensity);
	MaterialInstance.SetScalarParameterValue(TEXT("ScratchDensity"), Glass.ScratchDensity);
	MaterialInstance.SetScalarParameterValue(TEXT("ScratchCoverage"), Glass.ScratchCoverage);
	MaterialInstance.SetScalarParameterValue(TEXT("ScratchLength"), Glass.ScratchLength);
	MaterialInstance.SetScalarParameterValue(TEXT("ScratchWidthPx"), Glass.ScratchWidthPx);
	MaterialInstance.SetScalarParameterValue(TEXT("ScratchEdgeBias"), Glass.ScratchEdgeBias);
	MaterialInstance.SetScalarParameterValue(TEXT("ScratchSeed"), Glass.ScratchSeed);
	// Light and streak are given in screen space; undo the widget's rotation inside the shader.
	MaterialInstance.SetScalarParameterValue(TEXT("ScreenRotation"), M_GlassAppliedAngle);
}

int32 UW_MiniMap::DrawGlassOverlay(const FGeometry& AllottedGeometry,
                                   FSlateWindowElementList& OutDrawElements,
                                   const int32 LayerId) const
{
	if (not M_GlassSettings.bShowGlass || not IsValid(M_GlassOverlayMaterialInstance) || not IsValid(MiniMapImg))
	{
		return LayerId;
	}
	const FGeometry MiniMapGeometry = MiniMapImg->GetCachedGeometry();
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	if (MiniMapSize.X <= 0.0f || MiniMapSize.Y <= 0.0f)
	{
		return LayerId;
	}
	const FVector2D TopLeftInWidget = AllottedGeometry.AbsoluteToLocal(MiniMapGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D SizeInWidget = AllottedGeometry.AbsoluteToLocal(MiniMapGeometry.LocalToAbsolute(MiniMapSize))
		- TopLeftInWidget;
	const int32 GlassLayerId = LayerId + 1;
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		GlassLayerId,
		AllottedGeometry.ToPaintGeometry(SizeInWidget, FSlateLayoutTransform(TopLeftInWidget)),
		&M_GlassOverlayBrush,
		ESlateDrawEffect::None,
		FLinearColor::White);
	return GlassLayerId;
}

int32 UW_MiniMap::DrawMiniMapUnitColorIcons(const FGeometry& AllottedGeometry,
                                            FSlateWindowElementList& OutDrawElements,
                                            const int32 LayerId) const
{
	AFowManager* FowManager = GetIsValidFowManager();
	if (not IsValid(FowManager))
	{
		return LayerId;
	}

	const UImage* const MiniMapImage = GetIsValidMiniMapImg();
	if (not IsValid(MiniMapImage))
	{
		return LayerId;
	}

	const FGeometry MiniMapGeometry = MiniMapImage->GetCachedGeometry();
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	if (MiniMapSize.X <= 0.0f || MiniMapSize.Y <= 0.0f)
	{
		return LayerId;
	}

	const TArray<FRTSMinimapIconDrawData>& MiniMapIconDrawData = FowManager->GetMiniMapIconDrawData();
	if (MiniMapIconDrawData.Num() <= 0)
	{
		return LayerId;
	}

	const int32 IconLayerId = LayerId + 1;
	for (const FRTSMinimapIconDrawData& EachIcon : MiniMapIconDrawData)
	{
		if (EachIcon.M_IconSizePixels <= 0.0f)
		{
			continue;
		}

		const FVector2D IconSize = FVector2D(EachIcon.M_IconSizePixels, EachIcon.M_IconSizePixels);
		// Use the same full-image UV space as the render target material and camera minimap code paths.
		const FVector2D IconCenterLocalToMiniMap = FVector2D(
			EachIcon.M_UV.X * MiniMapSize.X,
			EachIcon.M_UV.Y * MiniMapSize.Y);
		const FVector2D IconCenterAbsolute = MiniMapGeometry.LocalToAbsolute(IconCenterLocalToMiniMap);
		const FVector2D IconTopLeftInWidget = AllottedGeometry.AbsoluteToLocal(IconCenterAbsolute)
			- (IconSize * 0.5f);
		const FPaintGeometry IconGeometry = AllottedGeometry.ToPaintGeometry(
			IconSize,
			FSlateLayoutTransform(IconTopLeftInWidget));
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			IconLayerId,
			IconGeometry,
			&M_MinimapIconBrush,
			ESlateDrawEffect::None,
			EachIcon.M_IconColor);
	}

	return IconLayerId;
}

FReply UW_MiniMap::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                           const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const UImage* const MiniMapImage = GetIsValidMiniMapImg();
		if (not IsValid(MiniMapImage))
		{
			return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
		}

		const FGeometry MiniMapGeometry = MiniMapImage->GetCachedGeometry();
		if (not MiniMapGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
		}

		const FVector2D LocalPos = MiniMapGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
		if (MiniMapSize.X <= 0.0f || MiniMapSize.Y <= 0.0f)
		{
			return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
		}

		const FVector2D UV = LocalPos / MiniMapSize;
		OnMiniMapClicked.Broadcast(UV);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}


int32 UW_MiniMap::DrawCustomMiniMapTextureIcons(const FGeometry& AllottedGeometry,
                                                FSlateWindowElementList& OutDrawElements,
                                                const int32 LayerId) const
{
	AFowManager* const FowManager = GetIsValidFowManager();
	if (not IsValid(FowManager))
	{
		return LayerId;
	}

	const UImage* const MiniMapImage = GetIsValidMiniMapImg();
	if (not IsValid(MiniMapImage))
	{
		return LayerId;
	}

	const FGeometry MiniMapGeometry = MiniMapImage->GetCachedGeometry();
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	if (MiniMapSize.X <= 0.0f || MiniMapSize.Y <= 0.0f)
	{
		return LayerId;
	}

	const TArray<FRTSMinimapCustomIconDrawData>& CustomMiniMapIconDrawData = FowManager->GetCustomMiniMapIconDrawData();
	if (CustomMiniMapIconDrawData.Num() <= 0)
	{
		return LayerId;
	}

	const int32 IconLayerId = LayerId + 1;
	for (const FRTSMinimapCustomIconDrawData& EachIcon : CustomMiniMapIconDrawData)
	{
		DrawCustomMiniMapTextureIcon(
			EachIcon,
			MiniMapGeometry,
			AllottedGeometry,
			MiniMapSize,
			OutDrawElements,
			IconLayerId);
	}

	return IconLayerId;
}

void UW_MiniMap::DrawCustomMiniMapTextureIcon(const FRTSMinimapCustomIconDrawData& IconDrawData,
                                              const FGeometry& MiniMapGeometry,
                                              const FGeometry& AllottedGeometry,
                                              const FVector2D& MiniMapSize,
                                              FSlateWindowElementList& OutDrawElements,
                                              const int32 LayerId) const
{
	if (IconDrawData.M_IconSizePixels <= 0.0f || IconDrawData.M_IconType == EMinimapIconType::None)
	{
		return;
	}

	const FSlateBrush* const IconBrush = GetCustomMiniMapIconBrush(IconDrawData.M_IconType);
	if (IconBrush == nullptr)
	{
		return;
	}

	const FVector2D IconSize = FVector2D(IconDrawData.M_IconSizePixels, IconDrawData.M_IconSizePixels);
	const FVector2D IconCenterLocalToMiniMap = FVector2D(
		IconDrawData.M_UV.X * MiniMapSize.X,
		IconDrawData.M_UV.Y * MiniMapSize.Y);
	const FVector2D IconCenterAbsolute = MiniMapGeometry.LocalToAbsolute(IconCenterLocalToMiniMap);
	const FVector2D IconTopLeftInWidget = AllottedGeometry.AbsoluteToLocal(IconCenterAbsolute)
		- (IconSize * 0.5f);
	const FPaintGeometry IconGeometry = AllottedGeometry.ToPaintGeometry(
		IconSize,
		FSlateLayoutTransform(IconTopLeftInWidget));
	FSlateDrawElement::MakeRotatedBox(
		OutDrawElements,
		LayerId,
		IconGeometry,
		IconBrush,
		ESlateDrawEffect::None,
		FMath::DegreesToRadians(IconDrawData.M_RotationDegrees),
		TOptional<FVector2D>(IconSize * 0.5f),
		FSlateDrawElement::RelativeToElement,
		FLinearColor::White);
}

int32 UW_MiniMap::DrawCustomMiniMapText(const FGeometry& AllottedGeometry,
	                                    FSlateWindowElementList& OutDrawElements,
	                                    const int32 LayerId) const
{
	const AFowManager* const FowManager = GetIsValidFowManager();
	if (not IsValid(FowManager))
	{
		return LayerId;
	}

	const UImage* const MiniMapImage = GetIsValidMiniMapImg();
	if (not IsValid(MiniMapImage))
	{
		return LayerId;
	}

	const FGeometry MiniMapGeometry = MiniMapImage->GetCachedGeometry();
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	if (MiniMapSize.X <= 0.0f || MiniMapSize.Y <= 0.0f)
	{
		return LayerId;
	}

	const TArray<FRTSMinimapTextDrawData>& TextDrawData = FowManager->GetCustomMiniMapTextDrawData();
	if (TextDrawData.IsEmpty())
	{
		return LayerId;
	}

	const int32 TextLayerId = LayerId + 1;
	for (const FRTSMinimapTextDrawData& EachText : TextDrawData)
	{
		DrawCustomMiniMapTextEntry(
			EachText,
			MiniMapGeometry,
			AllottedGeometry,
			MiniMapSize,
			OutDrawElements,
			TextLayerId);
	}

	return TextLayerId;
}

void UW_MiniMap::DrawCustomMiniMapTextEntry(const FRTSMinimapTextDrawData& TextDrawData,
	                                         const FGeometry& MiniMapGeometry,
	                                         const FGeometry& AllottedGeometry,
	                                         const FVector2D& MiniMapSize,
	                                         FSlateWindowElementList& OutDrawElements,
	                                         const int32 LayerId) const
{
	if (TextDrawData.M_Text.IsEmpty() || TextDrawData.M_TextSizePixels <= 0.0f
		|| not FSlateApplication::IsInitialized())
	{
		return;
	}

	constexpr int32 TextOutlineSizePixels = 1;
	const FName BoldTypefaceName(TEXT("Bold"));
	const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle(
		BoldTypefaceName,
		TextDrawData.M_TextSizePixels,
		FFontOutlineSettings(TextOutlineSizePixels));
	FSlateRenderer* const SlateRenderer = FSlateApplication::Get().GetRenderer();
	if (SlateRenderer == nullptr)
	{
		return;
	}

	const TSharedRef<FSlateFontMeasure> FontMeasure =
		SlateRenderer->GetFontMeasureService();
	const FVector2D TextSize = FontMeasure->Measure(TextDrawData.M_Text, FontInfo);
	if (TextSize.X <= 0.0f || TextSize.Y <= 0.0f)
	{
		return;
	}

	const FVector2D TextCenterLocalToMiniMap = FVector2D(
		TextDrawData.M_UV.X * MiniMapSize.X,
		TextDrawData.M_UV.Y * MiniMapSize.Y);
	const FVector2D TextCenterAbsolute = MiniMapGeometry.LocalToAbsolute(TextCenterLocalToMiniMap);
	const FVector2D TextTopLeftInWidget = AllottedGeometry.AbsoluteToLocal(TextCenterAbsolute)
		- (TextSize * 0.5f);
	const float CounterRotationRadians = FMath::DegreesToRadians(-GetRenderTransformAngle());
	const FPaintGeometry TextGeometry = AllottedGeometry.ToPaintGeometry(
		TextSize,
		FSlateLayoutTransform(TextTopLeftInWidget),
		FSlateRenderTransform(FQuat2D(CounterRotationRadians)));

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		TextGeometry,
		TextDrawData.M_Text,
		FontInfo,
		ESlateDrawEffect::None,
		TextDrawData.M_TextColor);
}

void UW_MiniMap::RefreshCustomMiniMapIconBrushes()
{
	M_CustomMinimapIconBrushes.Reset();

	const AFowManager* const FowManager = GetIsValidFowManager();
	if (not IsValid(FowManager))
	{
		return;
	}

	TArray<FRTSMinimapIconBrushData> IconBrushDataList;
	FowManager->AppendCustomMiniMapIconBrushData(IconBrushDataList);
	M_CustomMinimapIconBrushes.Reserve(IconBrushDataList.Num());
	for (const FRTSMinimapIconBrushData& IconBrushData : IconBrushDataList)
	{
		UTexture2D* const IconTexture = IconBrushData.M_Texture.Get();
		if (IconBrushData.M_IconType == EMinimapIconType::None || not IsValid(IconTexture))
		{
			continue;
		}

		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(IconTexture);
		IconBrush.ImageSize = FVector2D(IconTexture->GetSizeX(), IconTexture->GetSizeY());
		IconBrush.DrawAs = ESlateBrushDrawType::Image;
		IconBrush.Tiling = ESlateBrushTileType::NoTile;
		IconBrush.Mirroring = ESlateBrushMirrorType::NoMirror;
		M_CustomMinimapIconBrushes.Add(IconBrushData.M_IconType, IconBrush);
	}
}

const FSlateBrush* UW_MiniMap::GetCustomMiniMapIconBrush(const EMinimapIconType IconType) const
{
	if (IconType == EMinimapIconType::None)
	{
		return nullptr;
	}

	return M_CustomMinimapIconBrushes.Find(IconType);
}

int32 UW_MiniMap::DrawCombatPings(const FGeometry& AllottedGeometry,
                                  FSlateWindowElementList& OutDrawElements,
                                  const int32 LayerId) const
{
	// No subsystem in the designer; pings are checked first so the validators only run while pings exist.
	const UWorld* const World = GetWorld();
	const URTSCombatPingSubsystem* const CombatPingSubsystem = IsValid(World)
		                                                           ? World->GetSubsystem<URTSCombatPingSubsystem>()
		                                                           : nullptr;
	if (not IsValid(CombatPingSubsystem) || CombatPingSubsystem->GetPings().IsEmpty())
	{
		return LayerId;
	}
	const AFowManager* const FowManager = GetIsValidFowManager();
	const UImage* const MiniMapImage = GetIsValidMiniMapImg();
	if (not IsValid(FowManager) || not IsValid(MiniMapImage))
	{
		return LayerId;
	}
	const FGeometry MiniMapGeometry = MiniMapImage->GetCachedGeometry();
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	if (MiniMapSize.X <= 0.0f || MiniMapSize.Y <= 0.0f)
	{
		return LayerId;
	}

	const URTSCombatPingSettings* const Settings = GetDefault<URTSCombatPingSettings>();
	const float MaxRadius = bM_IsTacticalMapView ? Settings->TacticalMapRadius : Settings->MiniMapRadius;
	const double NowSeconds = CombatPingSubsystem->GetRealTimeSeconds();
	for (const FRTSCombatPing& Ping : CombatPingSubsystem->GetPings())
	{
		const float AgeSeconds = static_cast<float>(NowSeconds - Ping.M_SpawnRealTimeSeconds);
		FVector2D PingUV;
		if (AgeSeconds < 0.f || AgeSeconds > Settings->PingLifetimeSeconds
			|| not FowManager->GetMiniMapUVFromWorldLocation(Ping.M_WorldLocation, PingUV))
		{
			continue;
		}
		const FVector2D CenterAbsolute = MiniMapGeometry.LocalToAbsolute(PingUV * MiniMapSize);
		DrawCombatPing(AllottedGeometry.AbsoluteToLocal(CenterAbsolute), AgeSeconds, MaxRadius, *Settings,
		               AllottedGeometry, OutDrawElements, LayerId + 1);
	}
	// Glow, rings and core each take a layer.
	constexpr int32 CombatPingLayerCount = 3;
	return LayerId + CombatPingLayerCount;
}

void UW_MiniMap::DrawCombatPing(const FVector2D& CenterInWidget,
                                const float AgeSeconds,
                                const float MaxRadius,
                                const URTSCombatPingSettings& Settings,
                                const FGeometry& AllottedGeometry,
                                FSlateWindowElementList& OutDrawElements,
                                const int32 LayerId) const
{
	using namespace MiniMapCombatPingConstants;
	const float Envelope = GetPingEnvelope(AgeSeconds, Settings.PingLifetimeSeconds);
	if (Envelope <= 0.f)
	{
		return;
	}
	const FLinearColor& PingColor = Settings.PingColor;
	if (Settings.GlowOpacity > 0.f)
	{
		const float GlowDiscOpacity = PingColor.A * Settings.GlowOpacity * Envelope / GlowDiscCount;
		for (int32 GlowDiscIndex = 0; GlowDiscIndex < GlowDiscCount; ++GlowDiscIndex)
		{
			const float DiameterScale = 1.f - GlowDiscIndex / static_cast<float>(GlowDiscCount);
			DrawMiniMapDisc(CenterInWidget, MaxRadius * GlowDiameterShare * DiameterScale,
			                   PingColor.CopyWithNewOpacity(GlowDiscOpacity), AllottedGeometry, OutDrawElements,
			                   LayerId);
		}
	}

	const int32 RingLayerId = LayerId + 1;
	DrawCombatPingRipples(CenterInWidget, AgeSeconds, MaxRadius, Envelope, Settings, AllottedGeometry,
	                      OutDrawElements, RingLayerId);
	if (Settings.bShowBrackets)
	{
		DrawCombatPingBrackets(CenterInWidget, AgeSeconds, MaxRadius, Envelope, Settings, AllottedGeometry,
		                       OutDrawElements, RingLayerId);
	}

	const float PulsePhaseRadians = AgeSeconds / Settings.RipplePeriodSeconds * UE_TWO_PI;
	const float CoreDiameter = MaxRadius * CoreDiameterShare
		* (1.f + CorePulseAmount * FMath::Sin(PulsePhaseRadians));
	const FLinearColor CoreColor = FMath::Lerp(PingColor, FLinearColor::White, CoreWhiteBlend);
	DrawMiniMapDisc(CenterInWidget, CoreDiameter, CoreColor.CopyWithNewOpacity(PingColor.A * Envelope),
	                   AllottedGeometry, OutDrawElements, RingLayerId + 1);
}

void UW_MiniMap::DrawCombatPingRipples(const FVector2D& CenterInWidget,
                                       const float AgeSeconds,
                                       const float MaxRadius,
                                       const float Envelope,
                                       const URTSCombatPingSettings& Settings,
                                       const FGeometry& AllottedGeometry,
                                       FSlateWindowElementList& OutDrawElements,
                                       const int32 LayerId) const
{
	using namespace MiniMapCombatPingConstants;
	const float Period = Settings.RipplePeriodSeconds;
	for (int32 RippleIndex = 0; RippleIndex < Settings.RippleCount; ++RippleIndex)
	{
		// Rings leave the centre one after another, evenly spread over one period.
		const float RippleAge = AgeSeconds - Period * RippleIndex / Settings.RippleCount;
		if (RippleAge < 0.f)
		{
			continue;
		}
		const float Phase = FMath::Frac(RippleAge / Period);
		const float Expansion = FMath::InterpEaseOut(0.f, 1.f, Phase, RippleEaseExponent);
		const float Radius = MaxRadius * FMath::Lerp(RippleStartRadiusShare, 1.f, Expansion);
		const float Thickness = Settings.RingThickness
			* FMath::Lerp(RippleStartThicknessScale, RippleEndThicknessScale, Phase);
		const float Opacity = Settings.PingColor.A * FMath::Square(1.f - Phase) * Envelope;
		DrawArc(CenterInWidget, Radius, 0.f, FullCircleDegrees, Thickness,
		        Settings.PingColor.CopyWithNewOpacity(Opacity), AllottedGeometry, OutDrawElements, LayerId);
	}
}

void UW_MiniMap::DrawCombatPingBrackets(const FVector2D& CenterInWidget,
                                        const float AgeSeconds,
                                        const float MaxRadius,
                                        const float Envelope,
                                        const URTSCombatPingSettings& Settings,
                                        const FGeometry& AllottedGeometry,
                                        FSlateWindowElementList& OutDrawElements,
                                        const int32 LayerId) const
{
	using namespace MiniMapCombatPingConstants;
	const float LockOn = FMath::Clamp(AgeSeconds / BracketLockOnSeconds, 0.f, 1.f);
	const float LockOnEased = FMath::InterpEaseOut(0.f, 1.f, LockOn, BracketLockOnEaseExponent);
	const float Radius = MaxRadius * FMath::Lerp(BracketStartRadiusShare, BracketRadiusShare, LockOnEased);
	const FLinearColor BracketColor = Settings.PingColor.CopyWithNewOpacity(
		Settings.PingColor.A * LockOn * Envelope);
	const float Thickness = Settings.RingThickness * BracketThicknessScale;
	const float SpinDegrees = AgeSeconds * Settings.BracketSpinDegreesPerSecond;
	const float BracketSpacingDegrees = FullCircleDegrees / BracketCount;
	for (int32 BracketIndex = 0; BracketIndex < BracketCount; ++BracketIndex)
	{
		const float StartDegrees = SpinDegrees + BracketIndex * BracketSpacingDegrees - BracketArcDegrees * 0.5f;
		DrawArc(CenterInWidget, Radius, StartDegrees, BracketArcDegrees, Thickness, BracketColor,
		        AllottedGeometry, OutDrawElements, LayerId);
	}
}

int32 UW_MiniMap::DrawReinforcementsPings(const FGeometry& AllottedGeometry,
                                          FSlateWindowElementList& OutDrawElements,
                                          const int32 LayerId) const
{
	// Same early out as the combat pings: no subsystem in the designer, validators only run while pings exist.
	const UWorld* const World = GetWorld();
	const URTSCombatPingSubsystem* const CombatPingSubsystem = IsValid(World)
		                                                           ? World->GetSubsystem<URTSCombatPingSubsystem>()
		                                                           : nullptr;
	if (not IsValid(CombatPingSubsystem) || CombatPingSubsystem->GetReinforcementsPings().IsEmpty()
		|| not FSlateApplication::IsInitialized())
	{
		return LayerId;
	}
	const AFowManager* const FowManager = GetIsValidFowManager();
	const UImage* const MiniMapImage = GetIsValidMiniMapImg();
	if (not IsValid(FowManager) || not IsValid(MiniMapImage))
	{
		return LayerId;
	}
	const FGeometry MiniMapGeometry = MiniMapImage->GetCachedGeometry();
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	if (MiniMapSize.X <= 0.0f || MiniMapSize.Y <= 0.0f)
	{
		return LayerId;
	}

	const URTSCombatPingSettings* const Settings = GetDefault<URTSCombatPingSettings>();
	const double NowSeconds = CombatPingSubsystem->GetRealTimeSeconds();
	for (const FRTSReinforcementsPing& Ping : CombatPingSubsystem->GetReinforcementsPings())
	{
		const float AgeSeconds = static_cast<float>(NowSeconds - Ping.M_SpawnRealTimeSeconds);
		FVector2D PingUV;
		if (AgeSeconds < 0.f || AgeSeconds > Ping.M_DurationSeconds
			|| not FowManager->GetMiniMapUVFromWorldLocation(Ping.M_WorldLocation, PingUV))
		{
			continue;
		}
		const FVector2D CenterAbsolute = MiniMapGeometry.LocalToAbsolute(PingUV * MiniMapSize);
		DrawReinforcementsPing(AllottedGeometry.AbsoluteToLocal(CenterAbsolute), AgeSeconds, Ping.M_DurationSeconds,
		                       *Settings, AllottedGeometry, OutDrawElements, LayerId + 1);
	}
	// Fill, rings and label each take a layer.
	constexpr int32 ReinforcementsLayerCount = 3;
	return LayerId + ReinforcementsLayerCount;
}

void UW_MiniMap::DrawReinforcementsPing(const FVector2D& CenterInWidget,
                                        const float AgeSeconds,
                                        const float DurationSeconds,
                                        const URTSCombatPingSettings& Settings,
                                        const FGeometry& AllottedGeometry,
                                        FSlateWindowElementList& OutDrawElements,
                                        const int32 LayerId) const
{
	using namespace MiniMapReinforcementsPingConstants;
	using MiniMapCombatPingConstants::DrawArc;
	using MiniMapCombatPingConstants::FullCircleDegrees;
	const float Envelope = MiniMapCombatPingConstants::GetPingEnvelope(AgeSeconds, DurationSeconds);
	FSlateRenderer* const SlateRenderer = FSlateApplication::Get().GetRenderer();
	if (Envelope <= 0.f || SlateRenderer == nullptr)
	{
		return;
	}

	static const FText ReinforcementsLabel = NSLOCTEXT("MiniMap", "ReinforcementsPingLabel", "Reinforcements");
	const int32 TextSize = bM_IsTacticalMapView
		                       ? Settings.ReinforcementsTacticalMapTextSize
		                       : Settings.ReinforcementsMiniMapTextSize;
	const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle(
		FName(TEXT("Bold")), TextSize, FFontOutlineSettings(TextOutlineSizePixels));
	const FVector2D LabelSize = SlateRenderer->GetFontMeasureService()->Measure(ReinforcementsLabel, FontInfo);
	const float MinRadius = bM_IsTacticalMapView
		                        ? Settings.ReinforcementsTacticalMapRadius
		                        : Settings.ReinforcementsMiniMapRadius;
	const float Radius = FMath::Max(MinRadius, LabelSize.X * 0.5f + LabelSize.Y * TextPaddingShare);

	const FLinearColor& Color = Settings.ReinforcementsColor;
	DrawMiniMapDisc(CenterInWidget, Radius * 2.f, Color.CopyWithNewOpacity(Color.A * FillOpacity * Envelope),
	                AllottedGeometry, OutDrawElements, LayerId);

	const int32 RingLayerId = LayerId + 1;
	const float Phase = FMath::Frac(AgeSeconds / RipplePeriodSeconds);
	const float RingRadius = Radius * (1.f + RingPulseAmount * FMath::Sin(Phase * UE_TWO_PI));
	DrawArc(CenterInWidget, RingRadius, 0.f, FullCircleDegrees, Settings.ReinforcementsRingThickness,
	        Color.CopyWithNewOpacity(Color.A * Envelope), AllottedGeometry, OutDrawElements, RingLayerId);
	const float Expansion = FMath::InterpEaseOut(0.f, 1.f, Phase, RippleEaseExponent);
	const float RippleOpacity = Color.A * RippleOpacityShare * FMath::Square(1.f - Phase) * Envelope;
	DrawArc(CenterInWidget, Radius * FMath::Lerp(1.f, RippleEndRadiusShare, Expansion), 0.f, FullCircleDegrees,
	        Settings.ReinforcementsRingThickness, Color.CopyWithNewOpacity(RippleOpacity), AllottedGeometry,
	        OutDrawElements, RingLayerId);

	// The map may be rotated; the label is turned back so it always reads upright.
	const float CounterRotationRadians = FMath::DegreesToRadians(-GetRenderTransformAngle());
	const FPaintGeometry LabelGeometry = AllottedGeometry.ToPaintGeometry(
		LabelSize,
		FSlateLayoutTransform(CenterInWidget - LabelSize * 0.5f),
		FSlateRenderTransform(FQuat2D(CounterRotationRadians)));
	const FLinearColor LabelColor = FMath::Lerp(Color, FLinearColor::White, TextWhiteBlend);
	FSlateDrawElement::MakeText(OutDrawElements, RingLayerId + 1, LabelGeometry, ReinforcementsLabel, FontInfo,
	                            ESlateDrawEffect::None, LabelColor.CopyWithNewOpacity(Envelope));
}

int32 UW_MiniMap::DrawMapObjectFilters(const FGeometry& AllottedGeometry,
                                       FSlateWindowElementList& OutDrawElements,
                                       const int32 LayerId) const
{
	// One cheap check while every filter is off (the default); no subsystem in the designer.
	const UWorld* const World = GetWorld();
	const URTSMapObjectFilterSubsystem* const MapObjectFilters = IsValid(World)
		                                                             ? World->GetSubsystem<URTSMapObjectFilterSubsystem>()
		                                                             : nullptr;
	if (not IsValid(MapObjectFilters) || not MapObjectFilters->GetShouldDrawAnyFilter(bM_IsTacticalMapView))
	{
		return LayerId;
	}
	const UImage* const MiniMapImage = GetIsValidMiniMapImg();
	if (not IsValid(MiniMapImage))
	{
		return LayerId;
	}
	const FGeometry MiniMapGeometry = MiniMapImage->GetCachedGeometry();
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	if (MiniMapSize.X <= 0.0f || MiniMapSize.Y <= 0.0f)
	{
		return LayerId;
	}

	const URTSMapObjectFilterSettings* const Settings = GetDefault<URTSMapObjectFilterSettings>();
	const int32 RingLayerId = LayerId + 1;
	for (const ERTSMapObjectFilter Filter : TEnumRange<ERTSMapObjectFilter>())
	{
		if (MapObjectFilters->GetShouldDrawFilter(Filter, bM_IsTacticalMapView))
		{
			DrawMapObjectFilterDots(MapObjectFilters->GetDiscoveredObjectUVs(Filter), Settings->GetLook(Filter),
			                        MiniMapGeometry, AllottedGeometry, OutDrawElements, RingLayerId);
		}
	}
	// Rings and dots each take a layer.
	constexpr int32 FilterLayerCount = 2;
	return LayerId + FilterLayerCount;
}

void UW_MiniMap::DrawMapObjectFilterDots(const TArray<FVector2D>& ObjectUVs,
                                         const FRTSMapObjectFilterLook& Look,
                                         const FGeometry& MiniMapGeometry,
                                         const FGeometry& AllottedGeometry,
                                         FSlateWindowElementList& OutDrawElements,
                                         const int32 LayerId) const
{
	const float DotDiameter = bM_IsTacticalMapView ? Look.TacticalMapDiameter : Look.MiniMapDiameter;
	const bool bDrawRing = Look.OutlineWidth > 0.f && Look.OutlineColor.A > 0.f;
	const float RingDiameter = DotDiameter + Look.OutlineWidth * 2.f;
	const FVector2D MiniMapSize = MiniMapGeometry.GetLocalSize();
	for (const FVector2D& ObjectUV : ObjectUVs)
	{
		const FVector2D CenterAbsolute = MiniMapGeometry.LocalToAbsolute(ObjectUV * MiniMapSize);
		const FVector2D CenterInWidget = AllottedGeometry.AbsoluteToLocal(CenterAbsolute);
		if (bDrawRing)
		{
			DrawMiniMapDisc(CenterInWidget, RingDiameter, Look.OutlineColor, AllottedGeometry, OutDrawElements,
			                LayerId);
		}
		DrawMiniMapDisc(CenterInWidget, DotDiameter, Look.Color, AllottedGeometry, OutDrawElements, LayerId + 1);
	}
}

void UW_MiniMap::DrawMiniMapDisc(const FVector2D& CenterInWidget,
                                    const float Diameter,
                                    const FLinearColor& Color,
                                    const FGeometry& AllottedGeometry,
                                    FSlateWindowElementList& OutDrawElements,
                                    const int32 LayerId) const
{
	// The unit icon brush is a fully rounded white box: a disc once tinted.
	const FVector2D DiscSize(Diameter, Diameter);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(DiscSize, FSlateLayoutTransform(CenterInWidget - DiscSize * 0.5f)),
		&M_MinimapIconBrush,
		ESlateDrawEffect::None,
		Color);
}
