// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PanelSignalLamp.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace PanelSignalLampConstants
{
	const FName PassParameter(TEXT("Pass"));
	const FName PowerParameter(TEXT("Power"));
	const FName HaloScaleParameter(TEXT("HaloScale"));
	const FName TimeOffsetParameter(TEXT("TimeOffset"));
	constexpr float LampPass = 0.f;
	constexpr float HaloPass = 1.f;
}

void FPanelSignalLamp::Build(UWidgetTree& WidgetTree, UMaterialInterface* LampMaterial,
                             const FSlateBrush& FallbackLitBrush, const FSlateBrush& FallbackUnlitBrush,
                             const float HaloScale, const float TimeOffset)
{
	using namespace PanelSignalLampConstants;
	M_FallbackLitBrush = FallbackLitBrush;
	M_FallbackUnlitBrush = FallbackUnlitBrush;
	M_LampImage = BuildImage(WidgetTree);
	M_HaloImage = nullptr;
	M_LampMaterial = nullptr;
	M_HaloMaterial = nullptr;
	M_Power = 0.f;
	bM_IsLit = false;
	if (not IsValid(LampMaterial))
	{
		M_LampImage->SetBrush(M_FallbackUnlitBrush);
		return;
	}
	M_LampMaterial = MakeLampPassMaterial(*LampMaterial, *M_LampImage, LampPass, HaloScale, TimeOffset);
	M_HaloImage = BuildImage(WidgetTree);
	M_HaloMaterial = MakeLampPassMaterial(*LampMaterial, *M_HaloImage, HaloPass, HaloScale, TimeOffset);
	// Laid out at the lamp's size and centred on it; the scale only changes where it draws.
	M_HaloImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(HaloScale, HaloScale),
	                                                 FVector2D::ZeroVector, 0.f));
	M_HaloImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ApplyPower();
}

void FPanelSignalLamp::SetIsLit(const bool bIsLit)
{
	bM_IsLit = bIsLit;
	if (not GetHasMaterial() && IsValid(M_LampImage))
	{
		M_LampImage->SetBrush(bIsLit ? M_FallbackLitBrush : M_FallbackUnlitBrush);
		M_Power = bIsLit ? 1.f : 0.f;
	}
}

void FPanelSignalLamp::Tick(const float DeltaTime, const float FadeSeconds)
{
	const float TargetPower = bM_IsLit ? 1.f : 0.f;
	if (not GetHasMaterial() || M_Power == TargetPower)
	{
		return;
	}
	M_Power = FadeSeconds > 0.f
		          ? FMath::FInterpConstantTo(M_Power, TargetPower, DeltaTime, 1.f / FadeSeconds)
		          : TargetPower;
	ApplyPower();
}

UImage* FPanelSignalLamp::BuildImage(UWidgetTree& WidgetTree) const
{
	UImage* const Image = WidgetTree.ConstructWidget<UImage>(UImage::StaticClass());
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Image;
}

UMaterialInstanceDynamic* FPanelSignalLamp::MakeLampPassMaterial(UMaterialInterface& LampMaterial, UImage& Image,
                                                                 const float Pass, const float HaloScale,
                                                                 const float TimeOffset) const
{
	using namespace PanelSignalLampConstants;
	UMaterialInstanceDynamic* const Material = UMaterialInstanceDynamic::Create(&LampMaterial, &Image);
	Material->SetScalarParameterValue(PassParameter, Pass);
	Material->SetScalarParameterValue(HaloScaleParameter, HaloScale);
	Material->SetScalarParameterValue(TimeOffsetParameter, TimeOffset);
	FSlateBrush Brush;
	Brush.SetResourceObject(Material);
	Image.SetBrush(Brush);
	return Material;
}

void FPanelSignalLamp::ApplyPower() const
{
	using namespace PanelSignalLampConstants;
	if (IsValid(M_LampMaterial))
	{
		M_LampMaterial->SetScalarParameterValue(PowerParameter, M_Power);
	}
	if (IsValid(M_HaloMaterial))
	{
		M_HaloMaterial->SetScalarParameterValue(PowerParameter, M_Power);
	}
}

bool FPanelSignalLamp::GetHasMaterial() const
{
	// Silent on purpose: no material is the configured brush fallback, not an error.
	return IsValid(M_LampMaterial);
}
