// Copyright (C) Bas Blokzijl - All rights reserved.

#include "IndicatorLamp.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/GameUI/IndicatorLamps/IndicatorLampSettings.h"

namespace IndicatorLampConstants
{
	const FName PowerParameter(TEXT("Power"));
	const FName BlinkModeParameter(TEXT("BlinkMode"));
	const FName HaloScaleParameter(TEXT("HaloScale"));
	const FName LampColorParameter(TEXT("LampColor"));
	// The lamp parameters the halo copies, so its light pulses in step with the jewel.
	const FName SharedScalarParameters[] = {
		FName(TEXT("TimeOffset")), FName(TEXT("BlinkMode")), FName(TEXT("BlinkHz")), FName(TEXT("BlinkPhase")),
		FName(TEXT("Duty")), FName(TEXT("MinOn"))
	};
	constexpr float SteadyBlinkMode = 0.f;
	constexpr float DarkPower = 0.001f;
}

namespace
{
	// Offscreen UI captures (UUISnapshotSubsystem) cannot wait for idle units or a power deficit.
	bool GetIsForcedLitForCapture()
	{
#if !UE_BUILD_SHIPPING
		static const bool bForcedLit = FParse::Param(FCommandLine::Get(), TEXT("UISnapshotLampsOn"));
		return bForcedLit;
#else
		return false;
#endif
	}
}

UImage* FIndicatorLamp::Build(UWidgetTree& WidgetTree, UMaterialInterface* LampMaterial, const float LampSize,
                              const bool bHideWhenOff)
{
	if (not IsValid(LampMaterial))
	{
		return nullptr;
	}
	M_Image = WidgetTree.ConstructWidget<UImage>(UImage::StaticClass());
	M_Material = UMaterialInstanceDynamic::Create(LampMaterial, M_Image);
	FSlateBrush Brush;
	Brush.SetResourceObject(M_Material);
	Brush.ImageSize = FVector2D(LampSize, LampSize);
	M_Image->SetBrush(Brush);
	if (GetIsForcedLitForCapture())
	{
		// A still must not catch the lamp in the dark half of its blink.
		M_Material->SetScalarParameterValue(IndicatorLampConstants::BlinkModeParameter,
		                                    IndicatorLampConstants::SteadyBlinkMode);
	}
	M_Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	bM_HideWhenOff = bHideWhenOff;
	M_Power = 0.f;
	ApplyPower();
	SetIsLit(false);
	return M_Image;
}

UImage* FIndicatorLamp::BuildHalo(UWidgetTree& WidgetTree, UMaterialInterface* HaloMaterial, const float HaloScale)
{
	if (not GetIsBuilt() || not IsValid(HaloMaterial))
	{
		return nullptr;
	}
	M_HaloImage = WidgetTree.ConstructWidget<UImage>(UImage::StaticClass());
	M_HaloMaterial = UMaterialInstanceDynamic::Create(HaloMaterial, M_HaloImage);
	M_HaloMaterial->SetScalarParameterValue(IndicatorLampConstants::HaloScaleParameter, HaloScale);
	CopyLampLookToHalo();
	FSlateBrush Brush;
	Brush.SetResourceObject(M_HaloMaterial);
	Brush.ImageSize = M_Image->GetBrush().ImageSize;
	M_HaloImage->SetBrush(Brush);
	// Laid out like the lamp and centred on it; the scale only changes where it draws.
	M_HaloImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(HaloScale, HaloScale),
	                                                 FVector2D::ZeroVector, 0.f));
	M_HaloImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	M_HaloImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyPower();
	return M_HaloImage;
}

void FIndicatorLamp::CopyLampLookToHalo() const
{
	using namespace IndicatorLampConstants;
	// Every M_UI_IndicatorLamp instance has these parameters (its master defines them all).
	for (const FName& Parameter : SharedScalarParameters)
	{
		M_HaloMaterial->SetScalarParameterValue(Parameter, M_Material->K2_GetScalarParameterValue(Parameter));
	}
	M_HaloMaterial->SetVectorParameterValue(LampColorParameter,
	                                        M_Material->K2_GetVectorParameterValue(LampColorParameter));
}

void FIndicatorLamp::SetIsLit(const bool bIsLit)
{
	bM_IsLit = bIsLit || GetIsForcedLitForCapture();
}

void FIndicatorLamp::Tick(const float DeltaTime)
{
	if (not GetIsBuilt())
	{
		return;
	}
	const float TargetPower = bM_IsLit ? 1.f : 0.f;
	if (M_Power == TargetPower)
	{
		return;
	}
	const float FadeSeconds = GetDefault<UIndicatorLampSettings>()->FadeSeconds;
	M_Power = FadeSeconds > 0.f
		          ? FMath::FInterpConstantTo(M_Power, TargetPower, DeltaTime, 1.f / FadeSeconds)
		          : TargetPower;
	ApplyPower();
}

void FIndicatorLamp::ApplyPower() const
{
	if (not GetIsBuilt())
	{
		return;
	}
	M_Material->SetScalarParameterValue(IndicatorLampConstants::PowerParameter, M_Power);
	const bool bShow = not bM_HideWhenOff || M_Power > IndicatorLampConstants::DarkPower;
	const ESlateVisibility Visibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	M_Image->SetVisibility(Visibility);
	if (IsValid(M_HaloImage) && IsValid(M_HaloMaterial))
	{
		M_HaloMaterial->SetScalarParameterValue(IndicatorLampConstants::PowerParameter, M_Power);
		M_HaloImage->SetVisibility(Visibility);
	}
}

bool FIndicatorLamp::GetIsBuilt() const
{
	return IsValid(M_Image) && IsValid(M_Material);
}
