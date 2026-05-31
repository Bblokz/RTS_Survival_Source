// W_MiniMap.cpp
// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_MiniMap.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

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
                                const TObjectPtr<AFowManager>& FowManager)
{
	M_FowManager = FowManager;
	bM_HasReportedMissingFowManager = false;

	UImage* const MiniMapImage = GetIsValidMiniMapImg();
	if (not Active || not Passive || not MiniMapImage || not GetIsValidFowManager())
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
	RefreshCustomMiniMapIconBrushes();
}

void UW_MiniMap::NativeConstruct()
{
	Super::NativeConstruct();

	ForceVolatile(true);
	SetClipping(EWidgetClipping::ClipToBoundsAlways);
	M_MinimapIconBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
}

void UW_MiniMap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

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

	const int32 IconLayerId = DrawMiniMapUnitColorIcons(AllottedGeometry, OutDrawElements, NextLayerId);
	return DrawCustomMiniMapTextureIcons(AllottedGeometry, OutDrawElements, IconLayerId);
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
