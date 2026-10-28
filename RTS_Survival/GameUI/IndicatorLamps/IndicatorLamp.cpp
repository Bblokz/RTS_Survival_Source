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
	M_Image->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool FIndicatorLamp::GetIsBuilt() const
{
	return IsValid(M_Image) && IsValid(M_Material);
}
