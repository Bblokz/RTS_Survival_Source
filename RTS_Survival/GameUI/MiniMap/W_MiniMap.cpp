// W_MiniMap.cpp
// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_MiniMap.h"
#include "Components/Image.h"
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

void UW_MiniMap::InitMiniMapRTs(const TObjectPtr<UTexture>& Active,
                                const TObjectPtr<UTexture>& Passive) const
{
	if (not Active || not Passive || not GetIsValidMiniMapImg())
	{
		return;
	}
	auto DynamicMaterial = MiniMapImg->GetDynamicMaterial();
	if (not IsValid(DynamicMaterial))
	{
		RTSFunctionLibrary::ReportError("Could not get valid dynamic material for MiniMapImg\nSee UW_MiniMap::InitMiniMapRTs");
		return;
	}
	DynamicMaterial->SetTextureParameterValue(FName("Active"), Active);
	DynamicMaterial->SetTextureParameterValue(FName("Passive"), Passive);
}

FReply UW_MiniMap::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                           const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		const FVector2D Size = InGeometry.GetLocalSize();
		if (Size.X > 0 && Size.Y > 0)
		{
			const FVector2D UV = LocalPos / Size;
			OnMiniMapClicked.Broadcast(UV);
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
