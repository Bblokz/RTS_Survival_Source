// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_MapObjectFilterToggle.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "RTS_Survival/GameUI/MiniMap/MapObjectFilters/RTSMapObjectFilterSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MapObjectFilterToggleConstants
{
	constexpr float SwatchSize = 10.f;
	constexpr float SwatchGap = 8.f;
	constexpr float HeaderToOptionsGap = 7.f;
	constexpr float OptionGap = 6.f;

	constexpr int32 LabelFontSize = 12;
	constexpr int32 TextOutlineSize = 1;
	const FLinearColor LabelColor(0.86f, 0.93f, 0.96f, 1.f);

	// Button faces from UISkins/_generator/map_filter_buttons.py, drawn at 2 texels per unit (112 x 80 px).
	const FVector2D OptionButtonSize(56.f, 40.f);
	const TCHAR* const ButtonTextureFolder = TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/MapFilterButtons/");
	// Indexed by ERTSMapObjectFilterMode.
	const TCHAR* const ButtonTextureNames[] = {TEXT("Off"), TEXT("Tactical"), TEXT("Both")};
	constexpr int32 ModeCount = UE_ARRAY_COUNT(ButtonTextureNames);

	// The face texture carries the light; hover lifts an unlit button towards full brightness, press darkens.
	const FLinearColor DimNormalTint(0.8f, 0.8f, 0.8f, 1.f);
	const FLinearColor HoveredTint(1.f, 1.f, 1.f, 1.f);
	const FLinearColor PressedTint(0.7f, 0.7f, 0.7f, 1.f);

	// The project's glass pane shader, fitted to the button's glass well (3 of 40 units in, 3 unit corners).
	const TCHAR* const GlassPaneMaterialPath =
		TEXT("/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/M_UI_GlassPane.M_UI_GlassPane");
	constexpr float GlassPaneInsetFrac = 0.075f;
	constexpr float GlassPaneCornerRadiusPx = 3.f;
	// Scratchier than the action buttons' panes (same values as the weapon item glass).
	const TPair<const TCHAR*, float> GlassPaneScalars[] = {
		{TEXT("InsetFrac"), GlassPaneInsetFrac}, {TEXT("InsetPx"), 0.f},
		{TEXT("CornerRadiusPx"), GlassPaneCornerRadiusPx}, {TEXT("RimIntensity"), 0.34f},
		{TEXT("EdgeShadow"), 0.24f}, {TEXT("StreakIntensity"), 0.09f}, {TEXT("ScratchIntensity"), 0.45f},
		{TEXT("ScratchCoverage"), 0.62f}, {TEXT("ScratchCellSize"), 0.3f}, {TEXT("ScratchEdgeBias"), 0.4f},
		{TEXT("DustIntensity"), 0.08f},
	};
	const FName ScratchSeedParameter(TEXT("ScratchSeed"));
	constexpr float BaseScratchSeed = 5.f;
	constexpr float ScratchSeedStep = 3.7f;

	FSlateBrush MakeFaceBrush(UTexture2D* const FaceTexture, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(FaceTexture);
		Brush.ImageSize = OptionButtonSize;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeOptionStyle(UTexture2D* const FaceTexture, const bool bIsSelected)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeFaceBrush(FaceTexture, bIsSelected ? HoveredTint : DimNormalTint));
		Style.SetHovered(MakeFaceBrush(FaceTexture, HoveredTint));
		Style.SetPressed(MakeFaceBrush(FaceTexture, PressedTint));
		Style.SetDisabled(MakeFaceBrush(FaceTexture, PressedTint));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		return Style;
	}
}

void UW_MapObjectFilterToggle::SetupFilterToggle(const ERTSMapObjectFilter Filter, const FText& Label,
                                                 const FLinearColor& SwatchColor)
{
	M_Filter = Filter;
	if (IsValid(M_LabelText))
	{
		M_LabelText->SetText(Label);
	}
	if (IsValid(M_Swatch))
	{
		M_Swatch->SetColorAndOpacity(SwatchColor);
	}
	RefreshSelectedMode();
}

void UW_MapObjectFilterToggle::RefreshSelectedMode()
{
	const URTSMapObjectFilterSubsystem* const MapObjectFilters = GetMapObjectFilterSubsystem();
	ApplyOptionStyles(IsValid(MapObjectFilters) ? MapObjectFilters->GetFilterMode(M_Filter)
		                  : ERTSMapObjectFilterMode::Off);
}

void UW_MapObjectFilterToggle::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_MapObjectFilterToggle::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	LoadOptionTextures();
	UVerticalBox* const RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
	                                                                        TEXT("FilterToggleRoot"));
	WidgetTree->RootWidget = RootBox;
	RootBox->AddChildToVerticalBox(BuildHeaderRow());
	UVerticalBoxSlot* const OptionsSlot = RootBox->AddChildToVerticalBox(BuildOptionRow());
	OptionsSlot->SetPadding(FMargin(0.f, MapObjectFilterToggleConstants::HeaderToOptionsGap, 0.f, 0.f));
}

UHorizontalBox* UW_MapObjectFilterToggle::BuildHeaderRow()
{
	using namespace MapObjectFilterToggleConstants;
	UHorizontalBox* const HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
	                                                                              TEXT("HeaderRow"));
	USizeBox* const SwatchBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SwatchBox"));
	SwatchBox->SetWidthOverride(SwatchSize);
	SwatchBox->SetHeightOverride(SwatchSize);
	M_Swatch = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Swatch"));
	// A white disc tinted by the image colour, like the dots on the map.
	FSlateBrush SwatchBrush = FSlateRoundedBoxBrush(FLinearColor::White, SwatchSize * 0.5f);
	M_Swatch->SetBrush(SwatchBrush);
	SwatchBox->SetContent(M_Swatch);
	UHorizontalBoxSlot* const SwatchSlot = HeaderRow->AddChildToHorizontalBox(SwatchBox);
	SwatchSlot->SetVerticalAlignment(VAlign_Center);
	SwatchSlot->SetPadding(FMargin(0.f, 0.f, SwatchGap, 0.f));

	M_LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
	M_LabelText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), LabelFontSize,
	                                                     FFontOutlineSettings(TextOutlineSize)));
	M_LabelText->SetColorAndOpacity(FSlateColor(LabelColor));
	UHorizontalBoxSlot* const LabelSlot = HeaderRow->AddChildToHorizontalBox(M_LabelText);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	return HeaderRow;
}

void UW_MapObjectFilterToggle::LoadOptionTextures()
{
	using namespace MapObjectFilterToggleConstants;
	M_DimTextures.SetNum(ModeCount);
	M_LitTextures.SetNum(ModeCount);
	for (int32 ModeIndex = 0; ModeIndex < ModeCount; ++ModeIndex)
	{
		const FString BaseName = FString(TEXT("T_MapFilterButton_")) + ButtonTextureNames[ModeIndex];
		const FString DimName = BaseName + TEXT("_dim");
		const FString LitName = BaseName + TEXT("_lit");
		M_DimTextures[ModeIndex] = LoadObject<UTexture2D>(
			nullptr, *(FString(ButtonTextureFolder) + DimName + TEXT(".") + DimName));
		M_LitTextures[ModeIndex] = LoadObject<UTexture2D>(
			nullptr, *(FString(ButtonTextureFolder) + LitName + TEXT(".") + LitName));
		if (not IsValid(M_DimTextures[ModeIndex]) || not IsValid(M_LitTextures[ModeIndex]))
		{
			RTSFunctionLibrary::ReportError("Map filter button texture missing: " + BaseName
				+ "\n Import them with ui_hud_parts.import_map_filter_buttons()."
				"\n See UW_MapObjectFilterToggle::LoadOptionTextures");
		}
	}
}

UHorizontalBox* UW_MapObjectFilterToggle::BuildOptionRow()
{
	UHorizontalBox* const OptionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
	                                                                              TEXT("OptionRow"));
	M_OptionButtons.SetNum(MapObjectFilterToggleConstants::ModeCount);
	M_GlassPaneMaterials.SetNum(MapObjectFilterToggleConstants::ModeCount);
	AddOptionButton(*OptionRow, ERTSMapObjectFilterMode::Off,
	                NSLOCTEXT("MapObjectFilters", "OffTooltip", "Off: not shown on either map"));
	AddOptionButton(*OptionRow, ERTSMapObjectFilterMode::TacticalMapOnly,
	                NSLOCTEXT("MapObjectFilters", "TacticalOnlyTooltip",
	                          "Tactical map only: shown while the tactical map is open"));
	AddOptionButton(*OptionRow, ERTSMapObjectFilterMode::MiniMapAndTacticalMap,
	                NSLOCTEXT("MapObjectFilters", "BothTooltip",
	                          "Minimap and tactical map: shown on both maps"));
	return OptionRow;
}

void UW_MapObjectFilterToggle::AddOptionButton(UHorizontalBox& OptionRow, const ERTSMapObjectFilterMode Mode,
                                               const FText& OptionTooltip)
{
	using namespace MapObjectFilterToggleConstants;
	const int32 ModeIndex = static_cast<int32>(Mode);
	USizeBox* const ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ButtonSizeBox->SetWidthOverride(OptionButtonSize.X);
	ButtonSizeBox->SetHeightOverride(OptionButtonSize.Y);
	UOverlay* const ButtonOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	ButtonSizeBox->SetContent(ButtonOverlay);

	UButton* const OptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OptionButton->SetToolTipText(OptionTooltip);
	BindOptionClick(*OptionButton, Mode);
	UOverlaySlot* const ButtonSlot = ButtonOverlay->AddChildToOverlay(OptionButton);
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	ButtonSlot->SetVerticalAlignment(VAlign_Fill);
	M_OptionButtons[ModeIndex] = OptionButton;

	// Glass over the face; the button underneath still gets hover and clicks.
	if (UImage* const GlassPane = CreateGlassPane(ModeIndex))
	{
		UOverlaySlot* const GlassSlot = ButtonOverlay->AddChildToOverlay(GlassPane);
		GlassSlot->SetHorizontalAlignment(HAlign_Fill);
		GlassSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UHorizontalBoxSlot* const RowSlot = OptionRow.AddChildToHorizontalBox(ButtonSizeBox);
	const bool bIsLastOption = ModeIndex == ModeCount - 1;
	RowSlot->SetPadding(FMargin(0.f, 0.f, bIsLastOption ? 0.f : OptionGap, 0.f));
}

void UW_MapObjectFilterToggle::BindOptionClick(UButton& OptionButton, const ERTSMapObjectFilterMode Mode)
{
	switch (Mode)
	{
	case ERTSMapObjectFilterMode::Off:
		OptionButton.OnClicked.AddUniqueDynamic(this, &UW_MapObjectFilterToggle::OnOffClicked);
		break;
	case ERTSMapObjectFilterMode::TacticalMapOnly:
		OptionButton.OnClicked.AddUniqueDynamic(this, &UW_MapObjectFilterToggle::OnTacticalMapOnlyClicked);
		break;
	case ERTSMapObjectFilterMode::MiniMapAndTacticalMap:
		OptionButton.OnClicked.AddUniqueDynamic(this, &UW_MapObjectFilterToggle::OnMiniMapAndTacticalMapClicked);
		break;
	}
}

UImage* UW_MapObjectFilterToggle::CreateGlassPane(const int32 ModeIndex)
{
	using namespace MapObjectFilterToggleConstants;
	UMaterialInterface* const GlassPaneMaterial = LoadObject<UMaterialInterface>(nullptr, GlassPaneMaterialPath);
	if (not IsValid(GlassPaneMaterial))
	{
		RTSFunctionLibrary::ReportError("Could not load M_UI_GlassPane for the map filter buttons."
			"\n See UW_MapObjectFilterToggle::CreateGlassPane");
		return nullptr;
	}
	UMaterialInstanceDynamic* const GlassMaterial = UMaterialInstanceDynamic::Create(GlassPaneMaterial, this);
	for (const TPair<const TCHAR*, float>& Scalar : GlassPaneScalars)
	{
		GlassMaterial->SetScalarParameterValue(Scalar.Key, Scalar.Value);
	}
	GlassMaterial->SetScalarParameterValue(ScratchSeedParameter, BaseScratchSeed + ScratchSeedStep * ModeIndex);
	M_GlassPaneMaterials[ModeIndex] = GlassMaterial;

	UImage* const GlassPane = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	GlassPane->SetBrushFromMaterial(GlassMaterial);
	GlassPane->SetVisibility(ESlateVisibility::HitTestInvisible);
	return GlassPane;
}

void UW_MapObjectFilterToggle::ApplyOptionStyles(const ERTSMapObjectFilterMode SelectedMode)
{
	using namespace MapObjectFilterToggleConstants;
	for (int32 ModeIndex = 0; ModeIndex < M_OptionButtons.Num(); ++ModeIndex)
	{
		const bool bIsSelected = ModeIndex == static_cast<int32>(SelectedMode);
		const TArray<TObjectPtr<UTexture2D>>& Faces = bIsSelected ? M_LitTextures : M_DimTextures;
		if (IsValid(M_OptionButtons[ModeIndex]) && Faces.IsValidIndex(ModeIndex))
		{
			M_OptionButtons[ModeIndex]->SetStyle(MakeOptionStyle(Faces[ModeIndex], bIsSelected));
		}
	}
}

void UW_MapObjectFilterToggle::OnOffClicked()
{
	SelectMode(ERTSMapObjectFilterMode::Off);
}

void UW_MapObjectFilterToggle::OnTacticalMapOnlyClicked()
{
	SelectMode(ERTSMapObjectFilterMode::TacticalMapOnly);
}

void UW_MapObjectFilterToggle::OnMiniMapAndTacticalMapClicked()
{
	SelectMode(ERTSMapObjectFilterMode::MiniMapAndTacticalMap);
}

void UW_MapObjectFilterToggle::SelectMode(const ERTSMapObjectFilterMode NewMode)
{
	URTSMapObjectFilterSubsystem* const MapObjectFilters = GetMapObjectFilterSubsystem();
	if (not IsValid(MapObjectFilters))
	{
		return;
	}
	MapObjectFilters->SetFilterMode(M_Filter, NewMode);
	ApplyOptionStyles(NewMode);
}

URTSMapObjectFilterSubsystem* UW_MapObjectFilterToggle::GetMapObjectFilterSubsystem() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? World->GetSubsystem<URTSMapObjectFilterSubsystem>() : nullptr;
}
