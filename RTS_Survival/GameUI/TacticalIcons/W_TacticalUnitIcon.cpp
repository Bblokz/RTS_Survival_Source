// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "W_TacticalUnitIcon.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace TacticalUnitIconConstants
{
	const TCHAR* const GlassMaterialPath =
		TEXT("/Game/RTS_Survival/Textures/GameUI/UnitIcons/Tactical/Glass/M_UI_TacticalIconGlass.M_UI_TacticalIconGlass");
	const FName IconParameter = TEXT("Icon");
	const FName LightColorParameter = TEXT("LightColor");
}

UW_TacticalUnitIcon::UW_TacticalUnitIcon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, M_GlassMaterial(FSoftObjectPath(TacticalUnitIconConstants::GlassMaterialPath))
{
}

bool UW_TacticalUnitIcon::Initialize()
{
	const bool bInitializedThisCall = Super::Initialize();
	// Built here rather than in NativeOnInitialized so the designer preview gets the tree as well.
	BuildWidgetTree();
	return bInitializedThisCall;
}

void UW_TacticalUnitIcon::SetupTacticalIcon(const FTacticalUnitIconSettings& Settings)
{
	M_Settings = Settings;
	ApplySettings();
}

void UW_TacticalUnitIcon::SetIconSize(const float IconSize)
{
	M_IconSize = IconSize;
	ApplyIconSize();
}

void UW_TacticalUnitIcon::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyIconSize();
	ApplySettings();
}

void UW_TacticalUnitIcon::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	M_SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidgetTree->RootWidget = M_SizeBox;

	M_GlassImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	// Decoration on top of a portrait: never take hover or clicks away from what is underneath.
	M_GlassImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_SizeBox->SetContent(M_GlassImage);
	SetVisibility(ESlateVisibility::HitTestInvisible);

	ApplyIconSize();
	ApplySettings();
}

void UW_TacticalUnitIcon::ApplySettings()
{
	if (not IsValid(M_Settings.IconTexture))
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (not GetIsValidGlassImage() || not EnsureGlassMaterialInstance())
	{
		return;
	}
	M_GlassMaterialInstance->SetTextureParameterValue(TacticalUnitIconConstants::IconParameter, M_Settings.IconTexture);
	M_GlassMaterialInstance->SetVectorParameterValue(TacticalUnitIconConstants::LightColorParameter,
	                                                 M_Settings.LightColor);
	M_GlassImage->SetBrushFromMaterial(M_GlassMaterialInstance);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_TacticalUnitIcon::ApplyIconSize() const
{
	if (not GetIsValidSizeBox())
	{
		return;
	}
	M_SizeBox->SetWidthOverride(M_IconSize);
	M_SizeBox->SetHeightOverride(M_IconSize);
}

bool UW_TacticalUnitIcon::EnsureGlassMaterialInstance()
{
	if (IsValid(M_GlassMaterialInstance))
	{
		return true;
	}
	UMaterialInterface* const GlassMaterial = M_GlassMaterial.LoadSynchronous();
	if (not IsValid(GlassMaterial))
	{
		RTSFunctionLibrary::ReportError(
			"Tactical unit icon could not load its glass material: " + M_GlassMaterial.ToString() +
			"\n See UW_TacticalUnitIcon::EnsureGlassMaterialInstance");
		return false;
	}
	M_GlassMaterialInstance = UMaterialInstanceDynamic::Create(GlassMaterial, this);
	return IsValid(M_GlassMaterialInstance);
}

bool UW_TacticalUnitIcon::GetIsValidGlassImage() const
{
	if (IsValid(M_GlassImage))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_GlassImage",
		"GetIsValidGlassImage",
		this);
	return false;
}

bool UW_TacticalUnitIcon::GetIsValidSizeBox() const
{
	if (IsValid(M_SizeBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_SizeBox",
		"GetIsValidSizeBox",
		this);
	return false;
}
