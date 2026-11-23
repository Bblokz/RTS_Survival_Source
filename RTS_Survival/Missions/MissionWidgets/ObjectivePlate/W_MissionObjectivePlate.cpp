// Copyright (C) Bas Blokzijl - All rights reserved.

// UW_Mission's objective plate (design A1-c): built in C++ around the Blueprint's own title and description.
#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MissionObjectivePlateButton.h"
#include "MissionObjectivePlateSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "MissionObjectivePlate"

namespace MissionObjectivePlateLayout
{
	// Shield edge lengths in UI units (width, height) per objective type and state.
	const FVector2D ExpandedMajorShield(56.0, 68.0);
	const FVector2D ExpandedBonusShield(44.0, 54.0);
	const FVector2D TitleMajorShield(40.0, 50.0);
	const FVector2D TitleBonusShield(34.0, 42.0);

	// The glass icon sits in the upper, wide part of the shield.
	constexpr float IconWidthFraction = 0.6f;
	constexpr float IconTopFraction = 0.13f;

	// The plate starts under the shield's middle so the shield is bolted over the plate's left edge.
	constexpr float PlateUnderShieldFraction = 0.45f;
	constexpr float PlateTopOffset = 3.f;
	constexpr float ContentGapAfterShield = 12.f;
	constexpr float ContentRightPadding = 12.f;
	constexpr float TitleTopPadding = 12.f;
	constexpr float TitleBottomPadding = 12.f;
	constexpr float ExpandedTopPadding = 15.f;
	constexpr float ExpandedBottomPadding = 20.f;
	constexpr float BonusExpandedWidthReduction = 20.f;
	constexpr float DescriptionWrapSlack = 18.f;

	constexpr float ExpandedKnobSize = 30.f;
	constexpr float TitleMajorKnobSize = 26.f;
	constexpr float TitleBonusKnobSize = 22.f;
	constexpr float KnobLeftGap = 8.f;

	constexpr float GrooveHeight = 2.f;
	const FMargin GroovePadding(0.f, 7.f, 4.f, 8.f);
	// The typewriter text block reports a little less height than its last line draws; the bottom pad covers it.
	const FMargin DescriptionPadding(2.f, 0.f, 6.f, 12.f);
	const FMargin ConfirmRowPadding(0.f, 8.f, 0.f, 0.f);
	const FMargin ConfirmButtonPadding(16.f, 3.f, 16.f, 4.f);
	constexpr float ConfirmButtonHeight = 30.f;
	constexpr float ConfirmButtonMinWidth = 92.f;
	constexpr float LabelInlineGap = 8.f;

	// Whole-number font sizes (ShippingReadyUI.md); letter spacing in 1/1000 em.
	constexpr int32 LabelFontSize = 11;
	constexpr int32 LabelLetterSpacing = 160;
	constexpr int32 ConfirmFontSize = 15;
	constexpr int32 ConfirmLetterSpacing = 160;
	const FVector2D TextShadowOffset(1.0, 1.0);
	const FLinearColor TextShadowColor(0.f, 0.f, 0.f, 0.8f);
	// sRGB #08090A: the dark groove between the title and the description.
	const FLinearColor GrooveColor(0.0024f, 0.0027f, 0.003f, 1.f);

	const FName KnobPointUpParameter(TEXT("PointUp"));
	const FName KnobHoverParameter(TEXT("Hover"));
	const FName KnobPressParameter(TEXT("Press"));
	constexpr int32 KnobNormalIndex = 0;
	constexpr int32 KnobHoveredIndex = 1;
	constexpr int32 KnobPressedIndex = 2;
	constexpr int32 KnobMaterialCount = 3;
}

namespace MissionObjectivePlateHelpers
{
	FSlateBrush MakeMaterialBrush(UObject* const Material, const FVector2D& ImageSize, const bool bTile)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Material);
		Brush.ImageSize = ImageSize;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = bTile ? ESlateBrushTileType::Both : ESlateBrushTileType::NoTile;
		return Brush;
	}

	FSlateBrush MakeInvisibleBrush()
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
		return Brush;
	}

	FSlateFontInfo MakeFont(const UMissionObjectivePlateSettings& Settings, const int32 Size, const int32 LetterSpacing)
	{
		const UFont* const Font = Settings.LabelFont.LoadSynchronous();
		FSlateFontInfo FontInfo = IsValid(Font)
			                          ? FSlateFontInfo(Font, Size, Settings.LabelTypeface)
			                          : FCoreStyle::GetDefaultFontStyle(Settings.LabelTypeface, Size);
		FontInfo.LetterSpacing = LetterSpacing;
		return FontInfo;
	}

	void StyleText(UTextBlock* const Text, const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(MissionObjectivePlateLayout::TextShadowOffset);
		Text->SetShadowColorAndOpacity(MissionObjectivePlateLayout::TextShadowColor);
	}

	FButtonStyle MakeButtonStyle(const FSlateBrush& Normal, const FSlateBrush& Hovered, const FSlateBrush& Pressed,
	                             const FMargin& Padding)
	{
		FButtonStyle Style;
		Style.SetNormal(Normal);
		Style.SetHovered(Hovered);
		Style.SetPressed(Pressed);
		Style.SetNormalPadding(Padding);
		Style.SetPressedPadding(Padding);
		return Style;
	}

	FVector2D GetShieldSize(const bool bIsBonus, const bool bIsExpanded)
	{
		using namespace MissionObjectivePlateLayout;
		if (bIsExpanded)
		{
			return bIsBonus ? ExpandedBonusShield : ExpandedMajorShield;
		}
		return bIsBonus ? TitleBonusShield : TitleMajorShield;
	}

	float GetKnobSize(const bool bIsBonus, const bool bIsExpanded)
	{
		using namespace MissionObjectivePlateLayout;
		if (bIsExpanded)
		{
			return ExpandedKnobSize;
		}
		return bIsBonus ? TitleBonusKnobSize : TitleMajorKnobSize;
	}
}

void UW_Mission::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildObjectivePlate();
}

void UW_Mission::BuildObjectivePlate()
{
	const UMissionObjectivePlateSettings* const Settings = GetDefault<UMissionObjectivePlateSettings>();
	if (IsDesignTime() || not IsValid(Settings) || not Settings->bUseObjectivePlates || M_ObjectivePlate.GetIsBuilt())
	{
		return;
	}
	if (not IsValid(WidgetTree) || not GetIsValidRichTextBlocks() || not BuildObjectivePlate_LoadMaterials(*Settings))
	{
		return;
	}
	BuildObjectivePlate_KnobMaterials(*Settings);

	// The Blueprint's text blocks keep their styles and graph bindings; only their parent changes.
	M_RichTitle->RemoveFromParent();
	M_RichDescription->RemoveFromParent();

	UVerticalBox* const Content = BuildObjectivePlate_Frame(*Settings);
	Content->AddChildToVerticalBox(BuildObjectivePlate_Header(*Settings));
	Content->AddChildToVerticalBox(BuildObjectivePlate_Body(*Settings));

	WidgetTree->RootWidget = M_ObjectivePlate.IndentBorder;
	M_ObjectivePlate.Root = M_ObjectivePlate.IndentBorder;
	ApplyObjectivePlateState();
}

bool UW_Mission::BuildObjectivePlate_LoadMaterials(const UMissionObjectivePlateSettings& Settings)
{
	M_ObjectivePlate.MajorShieldMaterial = Settings.MajorShieldMaterial.LoadSynchronous();
	M_ObjectivePlate.BonusShieldMaterial = Settings.BonusShieldMaterial.LoadSynchronous();
	M_ObjectivePlate.MajorIconMaterial = Settings.MajorIconMaterial.LoadSynchronous();
	M_ObjectivePlate.BonusIconMaterial = Settings.BonusIconMaterial.LoadSynchronous();
	if (IsValid(Settings.PlateMaterial.LoadSynchronous()) && IsValid(Settings.KnobMaterial.LoadSynchronous())
		&& IsValid(M_ObjectivePlate.MajorShieldMaterial) && IsValid(M_ObjectivePlate.BonusShieldMaterial)
		&& IsValid(M_ObjectivePlate.MajorIconMaterial) && IsValid(M_ObjectivePlate.BonusIconMaterial))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Mission objective plate materials are missing (Project Settings > Mission "
		"Objective Plates); the mission widget keeps W_BP_Mission's own layout.\nAt: " + GetName());
	return false;
}

UVerticalBox* UW_Mission::BuildObjectivePlate_Frame(const UMissionObjectivePlateSettings& Settings)
{
	UBorder* const IndentBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ObjPlate_Indent"));
	IndentBorder->SetBrush(MissionObjectivePlateHelpers::MakeInvisibleBrush());
	IndentBorder->SetPadding(FMargin(0.f));
	IndentBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	M_ObjectivePlate.IndentBorder = IndentBorder;

	UOverlay* const Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ObjPlate_Stack"));
	IndentBorder->SetContent(Stack);

	UBorder* const PlateOffset = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ObjPlate_Offset"));
	PlateOffset->SetBrush(MissionObjectivePlateHelpers::MakeInvisibleBrush());
	PlateOffset->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	M_ObjectivePlate.PlateOffsetBorder = PlateOffset;
	UOverlaySlot* const PlateSlot = Stack->AddChildToOverlay(PlateOffset);
	PlateSlot->SetHorizontalAlignment(HAlign_Left);
	PlateSlot->SetVerticalAlignment(VAlign_Top);

	USizeBox* const PlateSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ObjPlate_Width"));
	PlateOffset->SetContent(PlateSizeBox);
	M_ObjectivePlate.PlateSizeBox = PlateSizeBox;

	UBorder* const PlateBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ObjPlate_Plate"));
	PlateBorder->SetBrush(MissionObjectivePlateHelpers::MakeMaterialBrush(
		Settings.PlateMaterial.LoadSynchronous(), Settings.PlateBrushSize, true));
	PlateBorder->SetBrushColor(FLinearColor::White);
	PlateSizeBox->SetContent(PlateBorder);
	M_ObjectivePlate.PlateBorder = PlateBorder;

	// Added after the plate so the shield draws over the plate's left edge.
	UOverlaySlot* const ShieldSlot = Stack->AddChildToOverlay(BuildObjectivePlate_Shield(Settings));
	ShieldSlot->SetHorizontalAlignment(HAlign_Left);
	ShieldSlot->SetVerticalAlignment(VAlign_Top);

	UVerticalBox* const Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObjPlate_Content"));
	PlateBorder->SetContent(Content);
	return Content;
}

UWidget* UW_Mission::BuildObjectivePlate_Shield(const UMissionObjectivePlateSettings& Settings)
{
	USizeBox* const ShieldSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ObjPlate_ShieldSize"));
	ShieldSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_ObjectivePlate.ShieldSizeBox = ShieldSizeBox;

	UOverlay* const ShieldStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ObjPlate_ShieldStack"));
	ShieldSizeBox->SetContent(ShieldStack);

	UImage* const ShieldImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ObjPlate_Shield"));
	ShieldImage->SetBrushFromMaterial(M_ObjectivePlate.MajorShieldMaterial);
	UOverlaySlot* const ShieldImageSlot = ShieldStack->AddChildToOverlay(ShieldImage);
	ShieldImageSlot->SetHorizontalAlignment(HAlign_Fill);
	ShieldImageSlot->SetVerticalAlignment(VAlign_Fill);
	M_ObjectivePlate.ShieldImage = ShieldImage;

	// The icon is its own image so its glass material can be tuned or swapped without touching the shield.
	USizeBox* const IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ObjPlate_IconSize"));
	UOverlaySlot* const IconSlot = ShieldStack->AddChildToOverlay(IconSizeBox);
	IconSlot->SetHorizontalAlignment(HAlign_Center);
	IconSlot->SetVerticalAlignment(VAlign_Top);
	M_ObjectivePlate.IconSizeBox = IconSizeBox;

	UImage* const IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ObjPlate_GlassIcon"));
	IconImage->SetBrushFromMaterial(M_ObjectivePlate.MajorIconMaterial);
	IconSizeBox->SetContent(IconImage);
	M_ObjectivePlate.IconImage = IconImage;
	return ShieldSizeBox;
}

UWidget* UW_Mission::BuildObjectivePlate_Header(const UMissionObjectivePlateSettings& Settings)
{
	UHorizontalBox* const HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ObjPlate_Header"));

	// Clicking the title toggles like the knob does (W_BP_Mission's title button expanded the mission too).
	// Mouse-only: a click on the panel never takes keyboard focus from the RTS hotkeys.
	UButton* const HeaderButton = WidgetTree->ConstructWidget<UMissionObjectivePlateButton>(
		UMissionObjectivePlateButton::StaticClass(), TEXT("ObjPlate_HeaderButton"));
	const FSlateBrush Invisible = MissionObjectivePlateHelpers::MakeInvisibleBrush();
	HeaderButton->SetStyle(MissionObjectivePlateHelpers::MakeButtonStyle(Invisible, Invisible, Invisible, FMargin(0.f)));
	HeaderButton->OnClicked.AddDynamic(this, &UW_Mission::OnClickedObjectiveToggle);
	M_ObjectivePlate.HeaderButton = HeaderButton;
	UHorizontalBoxSlot* const HeaderButtonSlot = HeaderRow->AddChildToHorizontalBox(HeaderButton);
	HeaderButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	HeaderButtonSlot->SetVerticalAlignment(VAlign_Center);

	UButtonSlot* const TitleColumnSlot = Cast<UButtonSlot>(HeaderButton->AddChild(BuildObjectivePlate_TitleColumn(Settings)));
	if (IsValid(TitleColumnSlot))
	{
		TitleColumnSlot->SetPadding(FMargin(0.f));
		TitleColumnSlot->SetHorizontalAlignment(HAlign_Fill);
		TitleColumnSlot->SetVerticalAlignment(VAlign_Center);
	}

	UHorizontalBoxSlot* const KnobSlot = HeaderRow->AddChildToHorizontalBox(BuildObjectivePlate_Knob());
	KnobSlot->SetVerticalAlignment(VAlign_Center);
	KnobSlot->SetPadding(FMargin(MissionObjectivePlateLayout::KnobLeftGap, 0.f, 0.f, 0.f));
	return HeaderRow;
}

UWidget* UW_Mission::BuildObjectivePlate_TitleColumn(const UMissionObjectivePlateSettings& Settings)
{
	using namespace MissionObjectivePlateLayout;
	const FSlateFontInfo LabelFont = MissionObjectivePlateHelpers::MakeFont(Settings, LabelFontSize, LabelLetterSpacing);
	UVerticalBox* const TitleColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObjPlate_TitleColumn"));

	UTextBlock* const LabelAbove = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ObjPlate_LabelAbove"));
	MissionObjectivePlateHelpers::StyleText(LabelAbove, LabelFont, Settings.MajorLabelColor);
	TitleColumn->AddChildToVerticalBox(LabelAbove);
	M_ObjectivePlate.LabelAbove = LabelAbove;

	UHorizontalBox* const TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ObjPlate_TitleRow"));
	TitleColumn->AddChildToVerticalBox(TitleRow);

	UTextBlock* const LabelInline = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ObjPlate_LabelInline"));
	MissionObjectivePlateHelpers::StyleText(LabelInline, LabelFont, Settings.BonusLabelColor);
	LabelInline->SetText(LOCTEXT("BonusInline", "BONUS"));
	UHorizontalBoxSlot* const LabelInlineSlot = TitleRow->AddChildToHorizontalBox(LabelInline);
	LabelInlineSlot->SetVerticalAlignment(VAlign_Center);
	LabelInlineSlot->SetPadding(FMargin(0.f, 0.f, LabelInlineGap, 0.f));
	M_ObjectivePlate.LabelInline = LabelInline;

	UHorizontalBoxSlot* const TitleSlot = TitleRow->AddChildToHorizontalBox(M_RichTitle);
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return TitleColumn;
}

UWidget* UW_Mission::BuildObjectivePlate_Knob()
{
	using namespace MissionObjectivePlateLayout;
	USizeBox* const KnobSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ObjPlate_KnobSize"));
	M_ObjectivePlate.KnobSizeBox = KnobSizeBox;

	// Mouse-only: a click on the panel never takes keyboard focus from the RTS hotkeys.
	UButton* const KnobButton = WidgetTree->ConstructWidget<UMissionObjectivePlateButton>(
		UMissionObjectivePlateButton::StaticClass(), TEXT("ObjPlate_KnobButton"));
	const FVector2D KnobBrushSize(ExpandedKnobSize);
	KnobButton->SetStyle(MissionObjectivePlateHelpers::MakeButtonStyle(
		MissionObjectivePlateHelpers::MakeMaterialBrush(M_ObjectivePlate.KnobMaterials[KnobNormalIndex], KnobBrushSize, false),
		MissionObjectivePlateHelpers::MakeMaterialBrush(M_ObjectivePlate.KnobMaterials[KnobHoveredIndex], KnobBrushSize, false),
		MissionObjectivePlateHelpers::MakeMaterialBrush(M_ObjectivePlate.KnobMaterials[KnobPressedIndex], KnobBrushSize, false),
		FMargin(0.f)));
	KnobButton->OnClicked.AddDynamic(this, &UW_Mission::OnClickedObjectiveToggle);
	KnobSizeBox->SetContent(KnobButton);
	M_ObjectivePlate.KnobButton = KnobButton;
	return KnobSizeBox;
}

UWidget* UW_Mission::BuildObjectivePlate_Body(const UMissionObjectivePlateSettings& Settings)
{
	using namespace MissionObjectivePlateLayout;
	UVerticalBox* const Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObjPlate_Body"));
	M_ObjectivePlate.DescriptionHolder = Body;

	USizeBox* const GrooveSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ObjPlate_GrooveSize"));
	GrooveSize->SetHeightOverride(GrooveHeight);
	UImage* const Groove = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ObjPlate_Groove"));
	Groove->SetColorAndOpacity(GrooveColor);
	GrooveSize->SetContent(Groove);
	UVerticalBoxSlot* const GrooveSlot = Body->AddChildToVerticalBox(GrooveSize);
	GrooveSlot->SetPadding(GroovePadding);
	M_ObjectivePlate.Groove = GrooveSize;

	UVerticalBoxSlot* const DescriptionSlot = Body->AddChildToVerticalBox(M_RichDescription);
	DescriptionSlot->SetPadding(DescriptionPadding);

	UVerticalBoxSlot* const ConfirmSlot = Body->AddChildToVerticalBox(BuildObjectivePlate_ConfirmRow(Settings));
	ConfirmSlot->SetPadding(ConfirmRowPadding);
	return Body;
}

UWidget* UW_Mission::BuildObjectivePlate_ConfirmRow(const UMissionObjectivePlateSettings& Settings)
{
	using namespace MissionObjectivePlateLayout;
	UHorizontalBox* const ConfirmRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ObjPlate_ConfirmRow"));
	M_ObjectivePlate.ConfirmRow = ConfirmRow;
	USpacer* const Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("ObjPlate_ConfirmSpacer"));
	ConfirmRow->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	USizeBox* const ButtonSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ObjPlate_ConfirmSize"));
	ButtonSize->SetHeightOverride(ConfirmButtonHeight);
	ButtonSize->SetMinDesiredWidth(ConfirmButtonMinWidth);
	ConfirmRow->AddChildToHorizontalBox(ButtonSize);

	// Mouse-only: a click on the panel never takes keyboard focus from the RTS hotkeys.
	UButton* const ConfirmButton = WidgetTree->ConstructWidget<UMissionObjectivePlateButton>(
		UMissionObjectivePlateButton::StaticClass(), TEXT("ObjPlate_ConfirmButton"));
	ConfirmButton->SetStyle(MissionObjectivePlateHelpers::MakeButtonStyle(
		MissionObjectivePlateHelpers::MakeMaterialBrush(Settings.ConfirmButtonNormal.LoadSynchronous(), Settings.ConfirmButtonBrushSize, true),
		MissionObjectivePlateHelpers::MakeMaterialBrush(Settings.ConfirmButtonHovered.LoadSynchronous(), Settings.ConfirmButtonBrushSize, true),
		MissionObjectivePlateHelpers::MakeMaterialBrush(Settings.ConfirmButtonPressed.LoadSynchronous(), Settings.ConfirmButtonBrushSize, true),
		ConfirmButtonPadding));
	ConfirmButton->OnClicked.AddDynamic(this, &UW_Mission::OnClickedObjectiveConfirm);
	ButtonSize->SetContent(ConfirmButton);
	M_ObjectivePlate.ConfirmButton = ConfirmButton;

	UTextBlock* const ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ObjPlate_ConfirmText"));
	MissionObjectivePlateHelpers::StyleText(ConfirmText,
		MissionObjectivePlateHelpers::MakeFont(Settings, ConfirmFontSize, ConfirmLetterSpacing), Settings.ButtonTextColor);
	UButtonSlot* const TextSlot = Cast<UButtonSlot>(ConfirmButton->AddChild(ConfirmText));
	if (IsValid(TextSlot))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
	M_ObjectivePlate.ConfirmText = ConfirmText;
	return ConfirmRow;
}

void UW_Mission::BuildObjectivePlate_KnobMaterials(const UMissionObjectivePlateSettings& Settings)
{
	using namespace MissionObjectivePlateLayout;
	UMaterialInterface* const KnobMaterial = Settings.KnobMaterial.LoadSynchronous();
	M_ObjectivePlate.KnobMaterials.Reset();
	for (int32 Index = 0; Index < KnobMaterialCount; ++Index)
	{
		UMaterialInstanceDynamic* const Instance = UMaterialInstanceDynamic::Create(KnobMaterial, this);
		Instance->SetScalarParameterValue(KnobHoverParameter, Index == KnobNormalIndex ? 0.f : 1.f);
		Instance->SetScalarParameterValue(KnobPressParameter, Index == KnobPressedIndex ? 1.f : 0.f);
		M_ObjectivePlate.KnobMaterials.Add(Instance);
	}
}

void UW_Mission::ApplyObjectivePlateState()
{
	if (not M_ObjectivePlate.GetIsBuilt() || not GetIsValidObjectivePlate())
	{
		return;
	}
	const bool bIsBonus = M_ShownWidgetState.ObjectiveType == EMissionObjectiveType::Bonus;
	const bool bIsExpanded = GetIsExpandedState();
	ApplyObjectivePlate_TypeVisuals(bIsBonus, bIsExpanded);
	ApplyObjectivePlate_Sizes(bIsBonus, bIsExpanded);
}

bool UW_Mission::GetIsValidObjectivePlate() const
{
	const FMissionObjectivePlateWidgets& Plate = M_ObjectivePlate;
	if (IsValid(Plate.IndentBorder) && IsValid(Plate.PlateSizeBox) && IsValid(Plate.PlateOffsetBorder)
		&& IsValid(Plate.PlateBorder) && IsValid(Plate.ShieldSizeBox) && IsValid(Plate.ShieldImage)
		&& IsValid(Plate.IconSizeBox) && IsValid(Plate.IconImage) && IsValid(Plate.LabelAbove)
		&& IsValid(Plate.LabelInline) && IsValid(Plate.HeaderButton) && IsValid(Plate.KnobButton)
		&& IsValid(Plate.KnobSizeBox) && IsValid(Plate.DescriptionHolder) && IsValid(Plate.ConfirmRow)
		&& IsValid(Plate.ConfirmButton) && IsValid(Plate.ConfirmText) && IsValid(M_RichTitle)
		&& IsValid(M_RichDescription))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_ObjectivePlate", "GetIsValidObjectivePlate", this);
	return false;
}

void UW_Mission::ApplyObjectivePlate_TypeVisuals(const bool bIsBonus, const bool bIsExpanded)
{
	const UMissionObjectivePlateSettings* const Settings = GetDefault<UMissionObjectivePlateSettings>();
	const bool bShowsConfirm = M_WidgetState == EMissionWidgetState::OMS_ExpandedNext;
	M_ObjectivePlate.ShieldImage->SetBrushFromMaterial(bIsBonus
		                                                   ? M_ObjectivePlate.BonusShieldMaterial
		                                                   : M_ObjectivePlate.MajorShieldMaterial);
	M_ObjectivePlate.IconImage->SetBrushFromMaterial(bIsBonus
		                                                 ? M_ObjectivePlate.BonusIconMaterial
		                                                 : M_ObjectivePlate.MajorIconMaterial);

	M_ObjectivePlate.LabelAbove->SetText(bIsBonus
		                                     ? LOCTEXT("BonusObjective", "BONUS OBJECTIVE")
		                                     : LOCTEXT("MajorObjective", "MAJOR OBJECTIVE"));
	M_ObjectivePlate.LabelAbove->SetColorAndOpacity(FSlateColor(bIsBonus ? Settings->BonusLabelColor : Settings->MajorLabelColor));
	M_ObjectivePlate.LabelAbove->SetVisibility(bIsExpanded ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	M_ObjectivePlate.LabelInline->SetVisibility(bIsBonus && not bIsExpanded
		                                            ? ESlateVisibility::HitTestInvisible
		                                            : ESlateVisibility::Collapsed);

	M_ObjectivePlate.DescriptionHolder->SetVisibility(bIsExpanded
		                                                  ? ESlateVisibility::SelfHitTestInvisible
		                                                  : ESlateVisibility::Collapsed);
	M_RichDescription->SetVisibility(bIsExpanded ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	M_ObjectivePlate.ConfirmRow->SetVisibility(bShowsConfirm ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	M_ObjectivePlate.ConfirmText->SetText(M_NextButtonType == EMissionWidgetNextButton::Btn_OK
		                                      ? LOCTEXT("ConfirmOK", "OK")
		                                      : LOCTEXT("ConfirmNext", "NEXT"));

	// Like W_BP_Mission: a mission waiting for OK / Next cannot be collapsed.
	M_ObjectivePlate.KnobSizeBox->SetVisibility(bShowsConfirm ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	for (UMaterialInstanceDynamic* const KnobMaterial : M_ObjectivePlate.KnobMaterials)
	{
		if (IsValid(KnobMaterial))
		{
			KnobMaterial->SetScalarParameterValue(MissionObjectivePlateLayout::KnobPointUpParameter, bIsExpanded ? 1.f : 0.f);
		}
	}
}

void UW_Mission::ApplyObjectivePlate_Sizes(const bool bIsBonus, const bool bIsExpanded)
{
	using namespace MissionObjectivePlateLayout;
	const UMissionObjectivePlateSettings* const Settings = GetDefault<UMissionObjectivePlateSettings>();
	const FVector2D ShieldSize = MissionObjectivePlateHelpers::GetShieldSize(bIsBonus, bIsExpanded);
	M_ObjectivePlate.ShieldSizeBox->SetWidthOverride(ShieldSize.X);
	M_ObjectivePlate.ShieldSizeBox->SetHeightOverride(ShieldSize.Y);
	const float IconSize = FMath::RoundToFloat(ShieldSize.X * IconWidthFraction);
	M_ObjectivePlate.IconSizeBox->SetWidthOverride(IconSize);
	M_ObjectivePlate.IconSizeBox->SetHeightOverride(IconSize);
	if (UOverlaySlot* const IconSlot = Cast<UOverlaySlot>(M_ObjectivePlate.IconSizeBox->Slot))
	{
		IconSlot->SetPadding(FMargin(0.f, FMath::RoundToFloat(ShieldSize.Y * IconTopFraction), 0.f, 0.f));
	}

	const float IndentWidth = bIsBonus ? Settings->BonusIndent : 0.f;
	M_ObjectivePlate.IndentBorder->SetPadding(FMargin(IndentWidth, 0.f, 0.f, 0.f));
	const float PlateLeft = FMath::RoundToFloat(ShieldSize.X * PlateUnderShieldFraction);
	M_ObjectivePlate.PlateOffsetBorder->SetPadding(FMargin(PlateLeft, PlateTopOffset, 0.f, 0.f));
	const float ContentLeft = ShieldSize.X - PlateLeft + ContentGapAfterShield;
	M_ObjectivePlate.PlateBorder->SetPadding(FMargin(ContentLeft,
	                                                 bIsExpanded ? ExpandedTopPadding : TitleTopPadding,
	                                                 ContentRightPadding,
	                                                 bIsExpanded ? ExpandedBottomPadding : TitleBottomPadding));

	// Untagged titles take the plate's font; tagged ones keep their DT_RichText row.
	M_RichTitle->SetDefaultFont(MissionObjectivePlateHelpers::MakeFont(
		*Settings, bIsExpanded ? Settings->ExpandedTitleFontSize : Settings->TitleFontSize, 0));
	M_RichTitle->SetDefaultColorAndOpacity(FSlateColor(Settings->TitleColor));
	M_RichTitle->SetDefaultShadowOffset(TextShadowOffset);
	M_RichTitle->SetDefaultShadowColorAndOpacity(TextShadowColor);

	const float KnobSize = MissionObjectivePlateHelpers::GetKnobSize(bIsBonus, bIsExpanded);
	M_ObjectivePlate.KnobSizeBox->SetWidthOverride(KnobSize);
	M_ObjectivePlate.KnobSizeBox->SetHeightOverride(KnobSize);

	if (not bIsExpanded)
	{
		M_ObjectivePlate.PlateSizeBox->ClearWidthOverride();
		M_ObjectivePlate.PlateSizeBox->SetMinDesiredWidth(Settings->MinTitlePlateWidth - IndentWidth);
		return;
	}
	const float PlateWidth = Settings->ExpandedPlateWidth - (bIsBonus ? IndentWidth + BonusExpandedWidthReduction : 0.f);
	M_ObjectivePlate.PlateSizeBox->SetWidthOverride(PlateWidth);
	M_RichDescription->SetWrapTextAt(FMath::Max(PlateWidth - ContentLeft - ContentRightPadding - DescriptionWrapSlack, 1.f));
}

void UW_Mission::RequestWidgetStateFromPlate(const EMissionWidgetState NewState)
{
	M_WidgetState = NewState;
	ApplyObjectivePlateState();
	OnChangeWidgetState(NewState, EMissionTextSpeed::TS_None);
}

void UW_Mission::OnClickedObjectiveToggle()
{
	if (M_WidgetState == EMissionWidgetState::OMS_ExpandedNext)
	{
		return;
	}
	if (GetIsExpandedState())
	{
		RequestWidgetStateFromPlate(EMissionWidgetState::OMS_Minimized);
		return;
	}
	RequestWidgetStateFromPlate(bM_UseNextAsExpandedWidget
		                            ? EMissionWidgetState::OMS_ExpandedNext
		                            : EMissionWidgetState::OMS_Expanded);
}

void UW_Mission::OnClickedObjectiveConfirm()
{
	OnClickedNextMission();
}

bool UW_Mission::SimulateClickObjectiveToggle()
{
	if (not M_ObjectivePlate.GetIsBuilt() || not GetIsValidObjectivePlate())
	{
		return false;
	}
	UButton* const ClickedButton = M_ObjectivePlate.KnobSizeBox->GetVisibility() != ESlateVisibility::Collapsed
		                               ? M_ObjectivePlate.KnobButton.Get()
		                               : M_ObjectivePlate.HeaderButton.Get();
	ClickedButton->OnClicked.Broadcast();
	return true;
}

bool UW_Mission::GetAreObjectivePlateButtonsBound() const
{
	if (not M_ObjectivePlate.GetIsBuilt() || not GetIsValidObjectivePlate())
	{
		return false;
	}
	return M_ObjectivePlate.KnobButton->OnClicked.IsBound() && M_ObjectivePlate.HeaderButton->OnClicked.IsBound()
		&& M_ObjectivePlate.ConfirmButton->OnClicked.IsBound();
}

#undef LOCTEXT_NAMESPACE
