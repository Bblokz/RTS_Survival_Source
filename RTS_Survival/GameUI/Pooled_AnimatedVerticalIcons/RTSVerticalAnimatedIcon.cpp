#include "RTSVerticalAnimatedIcon.h"

#include "AnimatedIconDataAsset.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool UW_RTSVerticalAnimatedIcon::Initialize()
{
	if (not Super::Initialize())
	{
		return false;
	}
	// NativeOnInitialized may be skipped before a local player context exists during pool prewarming.
	Initialize_CreateDefaultLayout();
	SetDormant();
	return true;
}

void UW_RTSVerticalAnimatedIcon::Initialize_CreateDefaultLayout()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	M_IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("M_IconSizeBox"));
	UScaleBox* ScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("IconScaleBox"));
	M_IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("M_IconImage"));
	if (not IsLayoutReady() || not IsValid(ScaleBox))
	{
		return;
	}

	ScaleBox->SetStretch(EStretch::ScaleToFit);
	ScaleBox->AddChild(M_IconImage);
	M_IconSizeBox->AddChild(ScaleBox);
	WidgetTree->RootWidget = M_IconSizeBox;
}

bool UW_RTSVerticalAnimatedIcon::ActivateIcon(const FRTSVerticalAnimatedIconDefinition& Definition,
	UTexture2D* Texture)
{
	if (not IsLayoutReady() || not IsValid(Texture))
	{
		return false;
	}

	SetDormant();
	M_IconSizeBox->SetWidthOverride(Definition.DisplaySize.X);
	M_IconSizeBox->SetHeightOverride(Definition.DisplaySize.Y);
	FSlateBrush IconBrush;
	IconBrush.SetResourceObject(Texture);
	IconBrush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
	IconBrush.DrawAs = ESlateBrushDrawType::Image;
	IconBrush.Tiling = ESlateBrushTileType::NoTile;
	M_IconImage->SetBrush(IconBrush);
	M_IconImage->SetColorAndOpacity(Definition.Tint);
	M_IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	SetIsEnabled(true);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(1.0f);
	return true;
}

void UW_RTSVerticalAnimatedIcon::SetDormant()
{
	SetVisibility(ESlateVisibility::Collapsed);
	SetRenderOpacity(0.0f);
	SetRenderTransform(FWidgetTransform());
}

bool UW_RTSVerticalAnimatedIcon::IsLayoutReady() const
{
	return GetIsValidIconSizeBox() && GetIsValidIconImage();
}

bool UW_RTSVerticalAnimatedIcon::GetIsValidIconSizeBox() const
{
	if (IsValid(M_IconSizeBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_IconSizeBox"),
		TEXT("GetIsValidIconSizeBox"), this);
	return false;
}

bool UW_RTSVerticalAnimatedIcon::GetIsValidIconImage() const
{
	if (IsValid(M_IconImage))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_IconImage"),
		TEXT("GetIsValidIconImage"), this);
	return false;
}
