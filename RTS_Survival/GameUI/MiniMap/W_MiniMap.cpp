// W_MiniMap.cpp
// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_MiniMap.h"

#include "Components/Image.h"
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
}

void UW_MiniMap::NativeConstruct()
{
	Super::NativeConstruct();

	ForceVolatile(true);
	SetClipping(EWidgetClipping::ClipToBoundsAlways);
	M_MinimapIconBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
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

	return DrawMiniMapIcons(MyCullingRect, OutDrawElements, NextLayerId);
}

int32 UW_MiniMap::DrawMiniMapIcons(const FSlateRect& MyCullingRect,
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

	const TArray<FRTSMinimapIconDrawData>& MiniMapIconDrawData = FowManager->GetMiniMapIconDrawData();
	if (MiniMapIconDrawData.Num() <= 0)
	{
		return LayerId;
	}

	(void)MyCullingRect;
	const int32 IconLayerId = LayerId + 1;
	for (const FRTSMinimapIconDrawData& EachIcon : MiniMapIconDrawData)
	{
		if (EachIcon.M_IconSizePixels <= 0.0f)
		{
			continue;
		}

		const FVector2D IconSize = FVector2D(EachIcon.M_IconSizePixels, EachIcon.M_IconSizePixels);
		const FVector2D IconTopLeft = FVector2D(
			EachIcon.M_UV.X * MiniMapSize.X - (IconSize.X * 0.5f),
			EachIcon.M_UV.Y * MiniMapSize.Y - (IconSize.Y * 0.5f));

		const FPaintGeometry IconGeometry = MiniMapGeometry.ToPaintGeometry(
			IconSize,
			FSlateLayoutTransform(IconTopLeft));
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
		const FVector2D Size = MiniMapGeometry.GetLocalSize();
		if (Size.X > 0 && Size.Y > 0)
		{
			const FVector2D UV = LocalPos / Size;
			OnMiniMapClicked.Broadcast(UV);
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
