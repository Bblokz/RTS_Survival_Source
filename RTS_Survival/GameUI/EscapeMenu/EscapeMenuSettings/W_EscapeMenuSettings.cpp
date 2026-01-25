#include "RTS_Survival/GameUI/EscapeMenu/EscapeMenuSettings/W_EscapeMenuSettings.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EscapeMenuSettingsWidgetNames
{
	const FName RootOverlay = TEXT("Overlay_Root");
	const FName BackgroundBlur = TEXT("BackgroundBlur_Fullscreen");
	const FName SizeBoxMenuRoot = TEXT("SizeBox_MenuRoot");
	const FName VerticalBoxMenuRoot = TEXT("VerticalBox_MenuRoot");
	const FName VerticalBoxSections = TEXT("VerticalBox_Sections");
	const FName HorizontalBoxFooterButtons = TEXT("HorizontalBox_FooterButtons");

	const FName VerticalBoxGraphicsSection = TEXT("VerticalBox_GraphicsSection");
	const FName VerticalBoxAudioSection = TEXT("VerticalBox_AudioSection");
	const FName VerticalBoxControlsSection = TEXT("VerticalBox_ControlsSection");

	const FName TextGraphicsHeader = TEXT("RichText_GraphicsHeader");
	const FName TextWindowModeLabel = TEXT("RichText_WindowModeLabel");
	const FName TextResolutionLabel = TEXT("RichText_ResolutionLabel");
	const FName TextVSyncLabel = TEXT("RichText_VSyncLabel");
	const FName TextOverallQualityLabel = TEXT("RichText_OverallQualityLabel");
	const FName TextViewDistanceLabel = TEXT("RichText_ViewDistanceLabel");
	const FName TextShadowsLabel = TEXT("RichText_ShadowsLabel");
	const FName TextTexturesLabel = TEXT("RichText_TexturesLabel");
	const FName TextEffectsLabel = TEXT("RichText_EffectsLabel");
	const FName TextPostProcessingLabel = TEXT("RichText_PostProcessingLabel");
	const FName TextFrameRateLimitLabel = TEXT("RichText_FrameRateLimitLabel");

	const FName ComboWindowMode = TEXT("Combo_WindowMode");
	const FName ComboResolution = TEXT("Combo_Resolution");
	const FName CheckVSync = TEXT("Check_VSync");
	const FName ComboOverallQuality = TEXT("Combo_OverallQuality");
	const FName ComboViewDistanceQuality = TEXT("Combo_ViewDistanceQuality");
	const FName ComboShadowsQuality = TEXT("Combo_ShadowsQuality");
	const FName ComboTexturesQuality = TEXT("Combo_TexturesQuality");
	const FName ComboEffectsQuality = TEXT("Combo_EffectsQuality");
	const FName ComboPostProcessingQuality = TEXT("Combo_PostProcessingQuality");
	const FName SliderFrameRateLimit = TEXT("Slider_FrameRateLimit");

	const FName TextAudioHeader = TEXT("RichText_AudioHeader");
	const FName TextMasterVolumeLabel = TEXT("RichText_MasterVolumeLabel");
	const FName TextMusicVolumeLabel = TEXT("RichText_MusicVolumeLabel");
	const FName TextSfxVolumeLabel = TEXT("RichText_SfxVolumeLabel");
	const FName SliderMasterVolume = TEXT("Slider_MasterVolume");
	const FName SliderMusicVolume = TEXT("Slider_MusicVolume");
	const FName SliderSfxVolume = TEXT("Slider_SfxVolume");

	const FName TextControlsHeader = TEXT("RichText_ControlsHeader");
	const FName TextMouseSensitivityLabel = TEXT("RichText_MouseSensitivityLabel");
	const FName TextInvertYAxisLabel = TEXT("RichText_InvertYAxisLabel");
	const FName SliderMouseSensitivity = TEXT("Slider_MouseSensitivity");
	const FName CheckInvertYAxis = TEXT("Check_InvertYAxis");

	const FName ButtonApply = TEXT("Button_Apply");
	const FName ButtonBackOrCancel = TEXT("Button_BackOrCancel");
	const FName TextApplyButton = TEXT("RichText_ApplyButton");
	const FName TextBackOrCancelButton = TEXT("RichText_BackOrCancelButton");

	const FName RowWindowMode = TEXT("Row_WindowMode");
	const FName RowResolution = TEXT("Row_Resolution");
	const FName RowVSync = TEXT("Row_VSync");
	const FName RowOverallQuality = TEXT("Row_OverallQuality");
	const FName RowViewDistance = TEXT("Row_ViewDistance");
	const FName RowShadows = TEXT("Row_Shadows");
	const FName RowTextures = TEXT("Row_Textures");
	const FName RowEffects = TEXT("Row_Effects");
	const FName RowPostProcessing = TEXT("Row_PostProcessing");
	const FName RowFrameRateLimit = TEXT("Row_FrameRateLimit");

	const FName RowMasterVolume = TEXT("Row_MasterVolume");
	const FName RowMusicVolume = TEXT("Row_MusicVolume");
	const FName RowSfxVolume = TEXT("Row_SfxVolume");

	const FName RowMouseSensitivity = TEXT("Row_MouseSensitivity");
	const FName RowInvertYAxis = TEXT("Row_InvertYAxis");
}

namespace EscapeMenuSettingsOptionDefaults
{
	constexpr int32 WindowModeOptionCount = 3;
	constexpr int32 QualityOptionCount = 4;

	const TCHAR* const WindowModeOptionKeys[WindowModeOptionCount] =
	{
		TEXT("SETTINGS_WINDOW_MODE_FULLSCREEN_OPTION"),
		TEXT("SETTINGS_WINDOW_MODE_WINDOWED_FULLSCREEN_OPTION"),
		TEXT("SETTINGS_WINDOW_MODE_WINDOWED_OPTION")
	};

	const TCHAR* const QualityOptionKeys[QualityOptionCount] =
	{
		TEXT("SETTINGS_QUALITY_LOW_OPTION"),
		TEXT("SETTINGS_QUALITY_MEDIUM_OPTION"),
		TEXT("SETTINGS_QUALITY_HIGH_OPTION"),
		TEXT("SETTINGS_QUALITY_EPIC_OPTION")
	};

	template <typename TWidgetType>
	void AssignWidgetIfFound(UWidgetTree* WidgetTree, TObjectPtr<TWidgetType>& WidgetReference, const FName WidgetName)
	{
		if (WidgetTree == nullptr)
		{
			return;
		}

		if (TWidgetType* const FoundWidget = WidgetTree->FindWidget<TWidgetType>(WidgetName))
		{
			WidgetReference = FoundWidget;
		}
	}

	void AssignWidgetName(UWidget* WidgetToName, UWidgetTree* WidgetTree, const FName WidgetName)
	{
		if (WidgetToName == nullptr || WidgetTree == nullptr)
		{
			return;
		}

		WidgetToName->Rename(*WidgetName.ToString(), WidgetTree);
	}
}

void UW_EscapeMenuSettings::SetPlayerController(ACPPController* NewPlayerController)
{
	if (not IsValid(NewPlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("Escape settings menu received an invalid player controller reference."));
		return;
	}

	M_PlayerController = NewPlayerController;
	M_MainGameUI = NewPlayerController->GetMainMenuUI();
}

void UW_EscapeMenuSettings::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	InitWidgetTree();
	CacheWidgetReferencesFromTree();

	const FString FunctionName = TEXT("UW_EscapeMenuSettings::NativeOnInitialized");
	if (not EnsureWidgetReferencesValid(FunctionName))
	{
		return;
	}

	bM_IsInitialisingControls = true;
	EnsureWindowModeOptionTexts();
	EnsureQualityOptionTexts();
	PopulateWindowModeOptions();
	PopulateQualityOptions();
	SetSliderRange(M_SliderFrameRateLimit, M_SliderRanges.M_FrameRateLimitMin, M_SliderRanges.M_FrameRateLimitMax);
	SetSliderRange(M_SliderMouseSensitivity, M_SliderRanges.M_MouseSensitivityMin, M_SliderRanges.M_MouseSensitivityMax);
	bM_IsInitialisingControls = false;

	UpdateAllRichTextBlocks();
}

void UW_EscapeMenuSettings::NativePreConstruct()
{
	Super::NativePreConstruct();
	InitWidgetTree();
	CacheWidgetReferencesFromTree();

	const FString FunctionName = TEXT("UW_EscapeMenuSettings::NativePreConstruct");
	if (not EnsureWidgetReferencesValid(FunctionName))
	{
		return;
	}

	bM_IsInitialisingControls = true;
	EnsureWindowModeOptionTexts();
	EnsureQualityOptionTexts();
	PopulateWindowModeOptions();
	PopulateQualityOptions();
	SetSliderRange(M_SliderFrameRateLimit, M_SliderRanges.M_FrameRateLimitMin, M_SliderRanges.M_FrameRateLimitMax);
	SetSliderRange(M_SliderMouseSensitivity, M_SliderRanges.M_MouseSensitivityMin, M_SliderRanges.M_MouseSensitivityMax);
	bM_IsInitialisingControls = false;

	UpdateAllRichTextBlocks();
}

void UW_EscapeMenuSettings::NativeConstruct()
{
	Super::NativeConstruct();
	InitWidgetTree();
	CacheWidgetReferencesFromTree();

	const FString FunctionName = TEXT("UW_EscapeMenuSettings::NativeConstruct");
	if (not EnsureWidgetReferencesValid(FunctionName))
	{
		return;
	}

	CacheSettingsSubsystem();
	BindButtonCallbacks();
	BindSettingCallbacks();
	RefreshControlsFromSubsystem();
}

void UW_EscapeMenuSettings::CacheSettingsSubsystem()
{
	UGameInstance* const GameInstance = GetGameInstance();
	if (GameInstance == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Game instance was null while caching the settings subsystem."));
		return;
	}

	URTSSettingsMenuSubsystem* const SettingsSubsystem = GameInstance->GetSubsystem<URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to get URTSSettingsMenuSubsystem from the game instance."));
		return;
	}

	M_SettingsSubsystem = SettingsSubsystem;
}

void UW_EscapeMenuSettings::RefreshControlsFromSubsystem()
{
	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const FString FunctionName = TEXT("UW_EscapeMenuSettings::RefreshControlsFromSubsystem");
	if (not EnsureWidgetReferencesValid(FunctionName))
	{
		return;
	}

	bM_IsInitialisingControls = true;
	M_SettingsSubsystem->LoadSettings();
	PopulateComboBoxOptions();

	FRTSSettingsSnapshot CurrentSettingsSnapshot;
	if (not M_SettingsSubsystem->GetCurrentSettingsValues(CurrentSettingsSnapshot))
	{
		bM_IsInitialisingControls = false;
		return;
	}

	InitialiseControlsFromSnapshot(CurrentSettingsSnapshot);
	bM_IsInitialisingControls = false;
}

void UW_EscapeMenuSettings::InitWidgetTree()
{
	if (WidgetTree == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("WidgetTree was null while building EscapeMenuSettings."));
		return;
	}

	if (WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	InitRootLayout();
}

void UW_EscapeMenuSettings::InitRootLayout()
{
	M_RootOverlay = WidgetTree->ConstructWidget<UOverlay>();
	if (M_RootOverlay == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct the root overlay for EscapeMenuSettings."));
		return;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(M_RootOverlay, WidgetTree, EscapeMenuSettingsWidgetNames::RootOverlay);

	WidgetTree->RootWidget = M_RootOverlay;

	M_BackgroundBlurFullscreen = WidgetTree->ConstructWidget<UBackgroundBlur>();
	if (M_BackgroundBlurFullscreen == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct the fullscreen background blur for EscapeMenuSettings."));
		return;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(M_BackgroundBlurFullscreen, WidgetTree, EscapeMenuSettingsWidgetNames::BackgroundBlur);

	if (UOverlaySlot* const BlurSlot = M_RootOverlay->AddChildToOverlay(M_BackgroundBlurFullscreen))
	{
		BlurSlot->SetHorizontalAlignment(HAlign_Fill);
		BlurSlot->SetVerticalAlignment(VAlign_Fill);
	}

	M_SizeBoxMenuRoot = WidgetTree->ConstructWidget<USizeBox>();
	if (M_SizeBoxMenuRoot == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct the menu root size box for EscapeMenuSettings."));
		return;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(M_SizeBoxMenuRoot, WidgetTree, EscapeMenuSettingsWidgetNames::SizeBoxMenuRoot);

	if (UOverlaySlot* const MenuRootSlot = M_RootOverlay->AddChildToOverlay(M_SizeBoxMenuRoot))
	{
		MenuRootSlot->SetHorizontalAlignment(HAlign_Fill);
		MenuRootSlot->SetVerticalAlignment(VAlign_Fill);
	}

	M_VerticalBoxMenuRoot = WidgetTree->ConstructWidget<UVerticalBox>();
	if (M_VerticalBoxMenuRoot == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct the menu root vertical box for EscapeMenuSettings."));
		return;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(M_VerticalBoxMenuRoot, WidgetTree, EscapeMenuSettingsWidgetNames::VerticalBoxMenuRoot);

	M_SizeBoxMenuRoot->SetContent(M_VerticalBoxMenuRoot);

	M_VerticalBoxSections = WidgetTree->ConstructWidget<UVerticalBox>();
	if (M_VerticalBoxSections == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct the sections vertical box for EscapeMenuSettings."));
		return;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(M_VerticalBoxSections, WidgetTree, EscapeMenuSettingsWidgetNames::VerticalBoxSections);

	M_VerticalBoxMenuRoot->AddChildToVerticalBox(M_VerticalBoxSections);

	InitGraphicsSection(M_VerticalBoxSections);
	InitAudioSection(M_VerticalBoxSections);
	InitControlsSection(M_VerticalBoxSections);
	InitFooterButtons();
}

void UW_EscapeMenuSettings::InitGraphicsSection(UVerticalBox* SectionsContainer)
{
	M_VerticalBoxGraphicsSection = CreateSectionContainer(
		SectionsContainer,
		EscapeMenuSettingsWidgetNames::VerticalBoxGraphicsSection,
		M_TextGraphicsHeader,
		EscapeMenuSettingsWidgetNames::TextGraphicsHeader,
		M_GraphicsText.M_HeaderText
	);
	if (M_VerticalBoxGraphicsSection == nullptr)
	{
		return;
	}

	InitGraphicsDisplaySettings(M_VerticalBoxGraphicsSection);
	InitGraphicsScalabilitySettings(M_VerticalBoxGraphicsSection);
	InitGraphicsFrameRateSetting(M_VerticalBoxGraphicsSection);
}

void UW_EscapeMenuSettings::InitGraphicsDisplaySettings(UVerticalBox* GraphicsSection)
{
	M_ComboWindowMode = CreateComboBox(EscapeMenuSettingsWidgetNames::ComboWindowMode);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowWindowMode,
		M_TextWindowModeLabel,
		EscapeMenuSettingsWidgetNames::TextWindowModeLabel,
		M_GraphicsText.M_WindowModeLabelText,
		M_ComboWindowMode
	);

	M_ComboResolution = CreateComboBox(EscapeMenuSettingsWidgetNames::ComboResolution);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowResolution,
		M_TextResolutionLabel,
		EscapeMenuSettingsWidgetNames::TextResolutionLabel,
		M_GraphicsText.M_ResolutionLabelText,
		M_ComboResolution
	);

	M_CheckVSync = CreateCheckBox(EscapeMenuSettingsWidgetNames::CheckVSync);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowVSync,
		M_TextVSyncLabel,
		EscapeMenuSettingsWidgetNames::TextVSyncLabel,
		M_GraphicsText.M_VSyncLabelText,
		M_CheckVSync
	);

	M_ComboOverallQuality = CreateComboBox(EscapeMenuSettingsWidgetNames::ComboOverallQuality);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowOverallQuality,
		M_TextOverallQualityLabel,
		EscapeMenuSettingsWidgetNames::TextOverallQualityLabel,
		M_GraphicsText.M_OverallQualityLabelText,
		M_ComboOverallQuality
	);
}

void UW_EscapeMenuSettings::InitGraphicsScalabilitySettings(UVerticalBox* GraphicsSection)
{
	M_ComboViewDistanceQuality = CreateComboBox(EscapeMenuSettingsWidgetNames::ComboViewDistanceQuality);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowViewDistance,
		M_TextViewDistanceLabel,
		EscapeMenuSettingsWidgetNames::TextViewDistanceLabel,
		M_GraphicsText.M_ViewDistanceLabelText,
		M_ComboViewDistanceQuality
	);

	M_ComboShadowsQuality = CreateComboBox(EscapeMenuSettingsWidgetNames::ComboShadowsQuality);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowShadows,
		M_TextShadowsLabel,
		EscapeMenuSettingsWidgetNames::TextShadowsLabel,
		M_GraphicsText.M_ShadowsLabelText,
		M_ComboShadowsQuality
	);

	M_ComboTexturesQuality = CreateComboBox(EscapeMenuSettingsWidgetNames::ComboTexturesQuality);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowTextures,
		M_TextTexturesLabel,
		EscapeMenuSettingsWidgetNames::TextTexturesLabel,
		M_GraphicsText.M_TexturesLabelText,
		M_ComboTexturesQuality
	);

	M_ComboEffectsQuality = CreateComboBox(EscapeMenuSettingsWidgetNames::ComboEffectsQuality);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowEffects,
		M_TextEffectsLabel,
		EscapeMenuSettingsWidgetNames::TextEffectsLabel,
		M_GraphicsText.M_EffectsLabelText,
		M_ComboEffectsQuality
	);

	M_ComboPostProcessingQuality = CreateComboBox(EscapeMenuSettingsWidgetNames::ComboPostProcessingQuality);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowPostProcessing,
		M_TextPostProcessingLabel,
		EscapeMenuSettingsWidgetNames::TextPostProcessingLabel,
		M_GraphicsText.M_PostProcessingLabelText,
		M_ComboPostProcessingQuality
	);
}

void UW_EscapeMenuSettings::InitGraphicsFrameRateSetting(UVerticalBox* GraphicsSection)
{
	M_SliderFrameRateLimit = CreateSlider(EscapeMenuSettingsWidgetNames::SliderFrameRateLimit);
	CreateSettingRow(
		GraphicsSection,
		EscapeMenuSettingsWidgetNames::RowFrameRateLimit,
		M_TextFrameRateLimitLabel,
		EscapeMenuSettingsWidgetNames::TextFrameRateLimitLabel,
		M_GraphicsText.M_FrameRateLimitLabelText,
		M_SliderFrameRateLimit
	);
}

void UW_EscapeMenuSettings::InitAudioSection(UVerticalBox* SectionsContainer)
{
	M_VerticalBoxAudioSection = CreateSectionContainer(
		SectionsContainer,
		EscapeMenuSettingsWidgetNames::VerticalBoxAudioSection,
		M_TextAudioHeader,
		EscapeMenuSettingsWidgetNames::TextAudioHeader,
		M_AudioText.M_HeaderText
	);
	if (M_VerticalBoxAudioSection == nullptr)
	{
		return;
	}

	M_SliderMasterVolume = CreateSlider(EscapeMenuSettingsWidgetNames::SliderMasterVolume);
	CreateSettingRow(
		M_VerticalBoxAudioSection,
		EscapeMenuSettingsWidgetNames::RowMasterVolume,
		M_TextMasterVolumeLabel,
		EscapeMenuSettingsWidgetNames::TextMasterVolumeLabel,
		M_AudioText.M_MasterVolumeLabelText,
		M_SliderMasterVolume
	);

	M_SliderMusicVolume = CreateSlider(EscapeMenuSettingsWidgetNames::SliderMusicVolume);
	CreateSettingRow(
		M_VerticalBoxAudioSection,
		EscapeMenuSettingsWidgetNames::RowMusicVolume,
		M_TextMusicVolumeLabel,
		EscapeMenuSettingsWidgetNames::TextMusicVolumeLabel,
		M_AudioText.M_MusicVolumeLabelText,
		M_SliderMusicVolume
	);

	M_SliderSfxVolume = CreateSlider(EscapeMenuSettingsWidgetNames::SliderSfxVolume);
	CreateSettingRow(
		M_VerticalBoxAudioSection,
		EscapeMenuSettingsWidgetNames::RowSfxVolume,
		M_TextSfxVolumeLabel,
		EscapeMenuSettingsWidgetNames::TextSfxVolumeLabel,
		M_AudioText.M_SfxVolumeLabelText,
		M_SliderSfxVolume
	);
}

void UW_EscapeMenuSettings::InitControlsSection(UVerticalBox* SectionsContainer)
{
	M_VerticalBoxControlsSection = CreateSectionContainer(
		SectionsContainer,
		EscapeMenuSettingsWidgetNames::VerticalBoxControlsSection,
		M_TextControlsHeader,
		EscapeMenuSettingsWidgetNames::TextControlsHeader,
		M_ControlsText.M_HeaderText
	);
	if (M_VerticalBoxControlsSection == nullptr)
	{
		return;
	}

	M_SliderMouseSensitivity = CreateSlider(EscapeMenuSettingsWidgetNames::SliderMouseSensitivity);
	CreateSettingRow(
		M_VerticalBoxControlsSection,
		EscapeMenuSettingsWidgetNames::RowMouseSensitivity,
		M_TextMouseSensitivityLabel,
		EscapeMenuSettingsWidgetNames::TextMouseSensitivityLabel,
		M_ControlsText.M_MouseSensitivityLabelText,
		M_SliderMouseSensitivity
	);

	M_CheckInvertYAxis = CreateCheckBox(EscapeMenuSettingsWidgetNames::CheckInvertYAxis);
	CreateSettingRow(
		M_VerticalBoxControlsSection,
		EscapeMenuSettingsWidgetNames::RowInvertYAxis,
		M_TextInvertYAxisLabel,
		EscapeMenuSettingsWidgetNames::TextInvertYAxisLabel,
		M_ControlsText.M_InvertYAxisLabelText,
		M_CheckInvertYAxis
	);
}

void UW_EscapeMenuSettings::InitFooterButtons()
{
	M_HorizontalBoxFooterButtons = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (M_HorizontalBoxFooterButtons == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct the footer button row for EscapeMenuSettings."));
		return;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(M_HorizontalBoxFooterButtons, WidgetTree, EscapeMenuSettingsWidgetNames::HorizontalBoxFooterButtons);

	M_VerticalBoxMenuRoot->AddChildToVerticalBox(M_HorizontalBoxFooterButtons);

	M_ButtonApply = CreateButton(
		EscapeMenuSettingsWidgetNames::ButtonApply,
		M_TextApplyButton,
		EscapeMenuSettingsWidgetNames::TextApplyButton,
		M_ButtonText.M_ApplyButtonText
	);
	if (M_ButtonApply != nullptr)
	{
		M_HorizontalBoxFooterButtons->AddChildToHorizontalBox(M_ButtonApply);
	}

	M_ButtonBackOrCancel = CreateButton(
		EscapeMenuSettingsWidgetNames::ButtonBackOrCancel,
		M_TextBackOrCancelButton,
		EscapeMenuSettingsWidgetNames::TextBackOrCancelButton,
		M_ButtonText.M_BackOrCancelButtonText
	);
	if (M_ButtonBackOrCancel != nullptr)
	{
		M_HorizontalBoxFooterButtons->AddChildToHorizontalBox(M_ButtonBackOrCancel);
	}
}

UVerticalBox* UW_EscapeMenuSettings::CreateSectionContainer(
	UVerticalBox* ParentContainer,
	const FName SectionName,
	TObjectPtr<URichTextBlock>& OutSectionHeader,
	const FName HeaderName,
	const FText& HeaderText)
{
	if (ParentContainer == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Parent container was null while creating a settings section."));
		return nullptr;
	}

	UVerticalBox* const SectionContainer = WidgetTree->ConstructWidget<UVerticalBox>();
	if (SectionContainer == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct a settings section container."));
		return nullptr;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(SectionContainer, WidgetTree, SectionName);

	ParentContainer->AddChildToVerticalBox(SectionContainer);

	OutSectionHeader = CreateRichTextBlock(HeaderName, HeaderText);
	if (OutSectionHeader == nullptr)
	{
		return SectionContainer;
	}

	SectionContainer->AddChildToVerticalBox(OutSectionHeader);
	return SectionContainer;
}

UHorizontalBox* UW_EscapeMenuSettings::CreateSettingRow(
	UVerticalBox* ParentContainer,
	const FName RowName,
	TObjectPtr<URichTextBlock>& OutLabel,
	const FName LabelName,
	const FText& LabelText,
	UWidget* ControlWidget)
{
	if (ParentContainer == nullptr || ControlWidget == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create a settings row due to an invalid parent or control widget."));
		return nullptr;
	}

	UHorizontalBox* const RowContainer = WidgetTree->ConstructWidget<UHorizontalBox>();
	if (RowContainer == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct a settings row container."));
		return nullptr;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(RowContainer, WidgetTree, RowName);

	ParentContainer->AddChildToVerticalBox(RowContainer);

	OutLabel = CreateRichTextBlock(LabelName, LabelText);
	if (OutLabel != nullptr)
	{
		RowContainer->AddChildToHorizontalBox(OutLabel);
	}

	RowContainer->AddChildToHorizontalBox(ControlWidget);
	return RowContainer;
}

URichTextBlock* UW_EscapeMenuSettings::CreateRichTextBlock(const FName WidgetName, const FText& TextToAssign)
{
	if (WidgetTree == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("WidgetTree was null while constructing a rich text block."));
		return nullptr;
	}

	URichTextBlock* const RichTextBlock = WidgetTree->ConstructWidget<URichTextBlock>();
	if (RichTextBlock == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct a rich text block for EscapeMenuSettings."));
		return nullptr;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(RichTextBlock, WidgetTree, WidgetName);

	RichTextBlock->SetText(TextToAssign);
	return RichTextBlock;
}

UComboBoxString* UW_EscapeMenuSettings::CreateComboBox(const FName WidgetName)
{
	if (WidgetTree == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("WidgetTree was null while constructing a combo box."));
		return nullptr;
	}

	UComboBoxString* const ComboBox = WidgetTree->ConstructWidget<UComboBoxString>();
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(ComboBox, WidgetTree, WidgetName);
	return ComboBox;
}

USlider* UW_EscapeMenuSettings::CreateSlider(const FName WidgetName)
{
	if (WidgetTree == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("WidgetTree was null while constructing a slider."));
		return nullptr;
	}

	USlider* const Slider = WidgetTree->ConstructWidget<USlider>();
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(Slider, WidgetTree, WidgetName);
	return Slider;
}

UCheckBox* UW_EscapeMenuSettings::CreateCheckBox(const FName WidgetName)
{
	if (WidgetTree == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("WidgetTree was null while constructing a checkbox."));
		return nullptr;
	}

	UCheckBox* const CheckBox = WidgetTree->ConstructWidget<UCheckBox>();
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(CheckBox, WidgetTree, WidgetName);
	return CheckBox;
}

UButton* UW_EscapeMenuSettings::CreateButton(
	const FName WidgetName,
	TObjectPtr<URichTextBlock>& OutButtonText,
	const FName ButtonTextName,
	const FText& ButtonText)
{
	if (WidgetTree == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("WidgetTree was null while constructing a button."));
		return nullptr;
	}

	UButton* const Button = WidgetTree->ConstructWidget<UButton>();
	if (Button == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to construct a button for EscapeMenuSettings."));
		return nullptr;
	}
	EscapeMenuSettingsOptionDefaults::AssignWidgetName(Button, WidgetTree, WidgetName);

	OutButtonText = CreateRichTextBlock(ButtonTextName, ButtonText);
	if (OutButtonText != nullptr)
	{
		Button->SetContent(OutButtonText);
	}

	return Button;
}

void UW_EscapeMenuSettings::CacheWidgetReferencesFromTree()
{
	CacheRootWidgetReferences();
	CacheGraphicsWidgetReferences();
	CacheAudioWidgetReferences();
	CacheControlsWidgetReferences();
	CacheFooterWidgetReferences();
}

void UW_EscapeMenuSettings::CacheRootWidgetReferences()
{
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_RootOverlay, EscapeMenuSettingsWidgetNames::RootOverlay);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_BackgroundBlurFullscreen, EscapeMenuSettingsWidgetNames::BackgroundBlur);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_SizeBoxMenuRoot, EscapeMenuSettingsWidgetNames::SizeBoxMenuRoot);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_VerticalBoxMenuRoot, EscapeMenuSettingsWidgetNames::VerticalBoxMenuRoot);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_VerticalBoxSections, EscapeMenuSettingsWidgetNames::VerticalBoxSections);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_HorizontalBoxFooterButtons, EscapeMenuSettingsWidgetNames::HorizontalBoxFooterButtons);
}

void UW_EscapeMenuSettings::CacheGraphicsWidgetReferences()
{
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_VerticalBoxGraphicsSection, EscapeMenuSettingsWidgetNames::VerticalBoxGraphicsSection);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextGraphicsHeader, EscapeMenuSettingsWidgetNames::TextGraphicsHeader);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextWindowModeLabel, EscapeMenuSettingsWidgetNames::TextWindowModeLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextResolutionLabel, EscapeMenuSettingsWidgetNames::TextResolutionLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextVSyncLabel, EscapeMenuSettingsWidgetNames::TextVSyncLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextOverallQualityLabel, EscapeMenuSettingsWidgetNames::TextOverallQualityLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextViewDistanceLabel, EscapeMenuSettingsWidgetNames::TextViewDistanceLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextShadowsLabel, EscapeMenuSettingsWidgetNames::TextShadowsLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextTexturesLabel, EscapeMenuSettingsWidgetNames::TextTexturesLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextEffectsLabel, EscapeMenuSettingsWidgetNames::TextEffectsLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextPostProcessingLabel, EscapeMenuSettingsWidgetNames::TextPostProcessingLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextFrameRateLimitLabel, EscapeMenuSettingsWidgetNames::TextFrameRateLimitLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ComboWindowMode, EscapeMenuSettingsWidgetNames::ComboWindowMode);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ComboResolution, EscapeMenuSettingsWidgetNames::ComboResolution);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_CheckVSync, EscapeMenuSettingsWidgetNames::CheckVSync);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ComboOverallQuality, EscapeMenuSettingsWidgetNames::ComboOverallQuality);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ComboViewDistanceQuality, EscapeMenuSettingsWidgetNames::ComboViewDistanceQuality);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ComboShadowsQuality, EscapeMenuSettingsWidgetNames::ComboShadowsQuality);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ComboTexturesQuality, EscapeMenuSettingsWidgetNames::ComboTexturesQuality);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ComboEffectsQuality, EscapeMenuSettingsWidgetNames::ComboEffectsQuality);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ComboPostProcessingQuality, EscapeMenuSettingsWidgetNames::ComboPostProcessingQuality);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_SliderFrameRateLimit, EscapeMenuSettingsWidgetNames::SliderFrameRateLimit);
}

void UW_EscapeMenuSettings::CacheAudioWidgetReferences()
{
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_VerticalBoxAudioSection, EscapeMenuSettingsWidgetNames::VerticalBoxAudioSection);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextAudioHeader, EscapeMenuSettingsWidgetNames::TextAudioHeader);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextMasterVolumeLabel, EscapeMenuSettingsWidgetNames::TextMasterVolumeLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextMusicVolumeLabel, EscapeMenuSettingsWidgetNames::TextMusicVolumeLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextSfxVolumeLabel, EscapeMenuSettingsWidgetNames::TextSfxVolumeLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_SliderMasterVolume, EscapeMenuSettingsWidgetNames::SliderMasterVolume);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_SliderMusicVolume, EscapeMenuSettingsWidgetNames::SliderMusicVolume);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_SliderSfxVolume, EscapeMenuSettingsWidgetNames::SliderSfxVolume);
}

void UW_EscapeMenuSettings::CacheControlsWidgetReferences()
{
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_VerticalBoxControlsSection, EscapeMenuSettingsWidgetNames::VerticalBoxControlsSection);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextControlsHeader, EscapeMenuSettingsWidgetNames::TextControlsHeader);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextMouseSensitivityLabel, EscapeMenuSettingsWidgetNames::TextMouseSensitivityLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextInvertYAxisLabel, EscapeMenuSettingsWidgetNames::TextInvertYAxisLabel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_SliderMouseSensitivity, EscapeMenuSettingsWidgetNames::SliderMouseSensitivity);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_CheckInvertYAxis, EscapeMenuSettingsWidgetNames::CheckInvertYAxis);
}

void UW_EscapeMenuSettings::CacheFooterWidgetReferences()
{
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ButtonApply, EscapeMenuSettingsWidgetNames::ButtonApply);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_ButtonBackOrCancel, EscapeMenuSettingsWidgetNames::ButtonBackOrCancel);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextApplyButton, EscapeMenuSettingsWidgetNames::TextApplyButton);
	EscapeMenuSettingsOptionDefaults::AssignWidgetIfFound(WidgetTree, M_TextBackOrCancelButton, EscapeMenuSettingsWidgetNames::TextBackOrCancelButton);
}

bool UW_EscapeMenuSettings::EnsureWidgetReferencesValid(const FString& FunctionName) const
{
	if (not EnsureRootWidgetReferencesValid(FunctionName))
	{
		return false;
	}

	if (not EnsureGraphicsWidgetReferencesValid(FunctionName))
	{
		return false;
	}

	if (not EnsureAudioWidgetReferencesValid(FunctionName))
	{
		return false;
	}

	if (not EnsureControlsWidgetReferencesValid(FunctionName))
	{
		return false;
	}

	return EnsureFooterWidgetReferencesValid(FunctionName);
}

bool UW_EscapeMenuSettings::EnsureRootWidgetReferencesValid(const FString& FunctionName) const
{
	bool bWidgetsValid = true;
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_RootOverlay, TEXT("M_RootOverlay"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_BackgroundBlurFullscreen, TEXT("M_BackgroundBlurFullscreen"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_SizeBoxMenuRoot, TEXT("M_SizeBoxMenuRoot"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_VerticalBoxMenuRoot, TEXT("M_VerticalBoxMenuRoot"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_VerticalBoxSections, TEXT("M_VerticalBoxSections"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_HorizontalBoxFooterButtons, TEXT("M_HorizontalBoxFooterButtons"), FunctionName);
	return bWidgetsValid;
}

bool UW_EscapeMenuSettings::EnsureGraphicsWidgetReferencesValid(const FString& FunctionName) const
{
	bool bWidgetsValid = true;
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_VerticalBoxGraphicsSection, TEXT("M_VerticalBoxGraphicsSection"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextGraphicsHeader, TEXT("M_TextGraphicsHeader"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextWindowModeLabel, TEXT("M_TextWindowModeLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextResolutionLabel, TEXT("M_TextResolutionLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextVSyncLabel, TEXT("M_TextVSyncLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextOverallQualityLabel, TEXT("M_TextOverallQualityLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextViewDistanceLabel, TEXT("M_TextViewDistanceLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextShadowsLabel, TEXT("M_TextShadowsLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextTexturesLabel, TEXT("M_TextTexturesLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextEffectsLabel, TEXT("M_TextEffectsLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextPostProcessingLabel, TEXT("M_TextPostProcessingLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextFrameRateLimitLabel, TEXT("M_TextFrameRateLimitLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ComboWindowMode, TEXT("M_ComboWindowMode"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ComboResolution, TEXT("M_ComboResolution"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_CheckVSync, TEXT("M_CheckVSync"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ComboOverallQuality, TEXT("M_ComboOverallQuality"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ComboViewDistanceQuality, TEXT("M_ComboViewDistanceQuality"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ComboShadowsQuality, TEXT("M_ComboShadowsQuality"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ComboTexturesQuality, TEXT("M_ComboTexturesQuality"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ComboEffectsQuality, TEXT("M_ComboEffectsQuality"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ComboPostProcessingQuality, TEXT("M_ComboPostProcessingQuality"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_SliderFrameRateLimit, TEXT("M_SliderFrameRateLimit"), FunctionName);
	return bWidgetsValid;
}

bool UW_EscapeMenuSettings::EnsureAudioWidgetReferencesValid(const FString& FunctionName) const
{
	bool bWidgetsValid = true;
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_VerticalBoxAudioSection, TEXT("M_VerticalBoxAudioSection"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextAudioHeader, TEXT("M_TextAudioHeader"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextMasterVolumeLabel, TEXT("M_TextMasterVolumeLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextMusicVolumeLabel, TEXT("M_TextMusicVolumeLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextSfxVolumeLabel, TEXT("M_TextSfxVolumeLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_SliderMasterVolume, TEXT("M_SliderMasterVolume"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_SliderMusicVolume, TEXT("M_SliderMusicVolume"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_SliderSfxVolume, TEXT("M_SliderSfxVolume"), FunctionName);
	return bWidgetsValid;
}

bool UW_EscapeMenuSettings::EnsureControlsWidgetReferencesValid(const FString& FunctionName) const
{
	bool bWidgetsValid = true;
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_VerticalBoxControlsSection, TEXT("M_VerticalBoxControlsSection"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextControlsHeader, TEXT("M_TextControlsHeader"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextMouseSensitivityLabel, TEXT("M_TextMouseSensitivityLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextInvertYAxisLabel, TEXT("M_TextInvertYAxisLabel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_SliderMouseSensitivity, TEXT("M_SliderMouseSensitivity"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_CheckInvertYAxis, TEXT("M_CheckInvertYAxis"), FunctionName);
	return bWidgetsValid;
}

bool UW_EscapeMenuSettings::EnsureFooterWidgetReferencesValid(const FString& FunctionName) const
{
	bool bWidgetsValid = true;
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_HorizontalBoxFooterButtons, TEXT("M_HorizontalBoxFooterButtons"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ButtonApply, TEXT("M_ButtonApply"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_ButtonBackOrCancel, TEXT("M_ButtonBackOrCancel"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextApplyButton, TEXT("M_TextApplyButton"), FunctionName);
	bWidgetsValid &= EnsureWidgetPointerIsValid(M_TextBackOrCancelButton, TEXT("M_TextBackOrCancelButton"), FunctionName);
	return bWidgetsValid;
}

void UW_EscapeMenuSettings::PopulateComboBoxOptions()
{
	EnsureWindowModeOptionTexts();
	EnsureQualityOptionTexts();
	PopulateWindowModeOptions();
	PopulateResolutionOptions();
	PopulateQualityOptions();

	SetSliderRange(M_SliderFrameRateLimit, M_SliderRanges.M_FrameRateLimitMin, M_SliderRanges.M_FrameRateLimitMax);
	SetSliderRange(M_SliderMouseSensitivity, M_SliderRanges.M_MouseSensitivityMin, M_SliderRanges.M_MouseSensitivityMax);
	SetSliderRange(M_SliderMasterVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	SetSliderRange(M_SliderMusicVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	SetSliderRange(M_SliderSfxVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
}

void UW_EscapeMenuSettings::PopulateWindowModeOptions()
{
	const FString FunctionName = TEXT("UW_EscapeMenuSettings::PopulateWindowModeOptions");
	if (not EnsureWidgetPointerIsValid(M_ComboWindowMode, TEXT("M_ComboWindowMode"), FunctionName))
	{
		return;
	}

	M_ComboWindowMode->ClearOptions();
	for (const FText& WindowModeOptionText : M_WindowModeOptionTexts)
	{
		M_ComboWindowMode->AddOption(WindowModeOptionText.ToString());
	}
}

void UW_EscapeMenuSettings::PopulateResolutionOptions()
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = M_SettingsSubsystem.Get();
	if (SettingsSubsystem == nullptr)
	{
		return;
	}

	const FString FunctionName = TEXT("UW_EscapeMenuSettings::PopulateResolutionOptions");
	if (not EnsureWidgetPointerIsValid(M_ComboResolution, TEXT("M_ComboResolution"), FunctionName))
	{
		return;
	}

	M_SupportedResolutions = SettingsSubsystem->GetSupportedResolutions();
	M_SupportedResolutionLabels = SettingsSubsystem->GetSupportedResolutionLabels();
	if (M_SupportedResolutions.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("No supported resolutions were available for the settings menu."));
		return;
	}

	if (M_SupportedResolutionLabels.Num() != M_SupportedResolutions.Num())
	{
		M_SupportedResolutionLabels.Empty();
		for (const FIntPoint& SupportedResolution : M_SupportedResolutions)
		{
			M_SupportedResolutionLabels.Add(BuildResolutionLabel(SupportedResolution));
		}
	}

	M_ComboResolution->ClearOptions();
	for (const FString& ResolutionLabel : M_SupportedResolutionLabels)
	{
		M_ComboResolution->AddOption(ResolutionLabel);
	}
}

void UW_EscapeMenuSettings::PopulateQualityOptions()
{
	PopulateQualityOptionsForComboBox(M_ComboOverallQuality);
	PopulateQualityOptionsForComboBox(M_ComboViewDistanceQuality);
	PopulateQualityOptionsForComboBox(M_ComboShadowsQuality);
	PopulateQualityOptionsForComboBox(M_ComboTexturesQuality);
	PopulateQualityOptionsForComboBox(M_ComboEffectsQuality);
	PopulateQualityOptionsForComboBox(M_ComboPostProcessingQuality);
}

void UW_EscapeMenuSettings::PopulateQualityOptionsForComboBox(UComboBoxString* ComboBoxToPopulate) const
{
	const FString FunctionName = TEXT("UW_EscapeMenuSettings::PopulateQualityOptionsForComboBox");
	if (not EnsureWidgetPointerIsValid(ComboBoxToPopulate, TEXT("ComboBoxToPopulate"), FunctionName))
	{
		return;
	}

	ComboBoxToPopulate->ClearOptions();
	for (const FText& QualityOptionText : M_QualityOptionTexts)
	{
		ComboBoxToPopulate->AddOption(QualityOptionText.ToString());
	}
}

void UW_EscapeMenuSettings::EnsureWindowModeOptionTexts()
{
	if (M_WindowModeOptionTexts.Num() >= EscapeMenuSettingsOptionDefaults::WindowModeOptionCount)
	{
		return;
	}

	for (int32 OptionIndex = M_WindowModeOptionTexts.Num(); OptionIndex < EscapeMenuSettingsOptionDefaults::WindowModeOptionCount; ++OptionIndex)
	{
		M_WindowModeOptionTexts.Add(FText::FromString(EscapeMenuSettingsOptionDefaults::WindowModeOptionKeys[OptionIndex]));
	}
}

void UW_EscapeMenuSettings::EnsureQualityOptionTexts()
{
	if (M_QualityOptionTexts.Num() >= EscapeMenuSettingsOptionDefaults::QualityOptionCount)
	{
		return;
	}

	for (int32 OptionIndex = M_QualityOptionTexts.Num(); OptionIndex < EscapeMenuSettingsOptionDefaults::QualityOptionCount; ++OptionIndex)
	{
		M_QualityOptionTexts.Add(FText::FromString(EscapeMenuSettingsOptionDefaults::QualityOptionKeys[OptionIndex]));
	}
}

void UW_EscapeMenuSettings::UpdateAllRichTextBlocks()
{
	const FString FunctionName = TEXT("UW_EscapeMenuSettings::UpdateAllRichTextBlocks");
	if (not EnsureWidgetReferencesValid(FunctionName))
	{
		return;
	}

	M_TextGraphicsHeader->SetText(M_GraphicsText.M_HeaderText);
	M_TextWindowModeLabel->SetText(M_GraphicsText.M_WindowModeLabelText);
	M_TextResolutionLabel->SetText(M_GraphicsText.M_ResolutionLabelText);
	M_TextVSyncLabel->SetText(M_GraphicsText.M_VSyncLabelText);
	M_TextOverallQualityLabel->SetText(M_GraphicsText.M_OverallQualityLabelText);
	M_TextViewDistanceLabel->SetText(M_GraphicsText.M_ViewDistanceLabelText);
	M_TextShadowsLabel->SetText(M_GraphicsText.M_ShadowsLabelText);
	M_TextTexturesLabel->SetText(M_GraphicsText.M_TexturesLabelText);
	M_TextEffectsLabel->SetText(M_GraphicsText.M_EffectsLabelText);
	M_TextPostProcessingLabel->SetText(M_GraphicsText.M_PostProcessingLabelText);
	M_TextFrameRateLimitLabel->SetText(M_GraphicsText.M_FrameRateLimitLabelText);

	M_TextAudioHeader->SetText(M_AudioText.M_HeaderText);
	M_TextMasterVolumeLabel->SetText(M_AudioText.M_MasterVolumeLabelText);
	M_TextMusicVolumeLabel->SetText(M_AudioText.M_MusicVolumeLabelText);
	M_TextSfxVolumeLabel->SetText(M_AudioText.M_SfxVolumeLabelText);

	M_TextControlsHeader->SetText(M_ControlsText.M_HeaderText);
	M_TextMouseSensitivityLabel->SetText(M_ControlsText.M_MouseSensitivityLabelText);
	M_TextInvertYAxisLabel->SetText(M_ControlsText.M_InvertYAxisLabelText);

	M_TextApplyButton->SetText(M_ButtonText.M_ApplyButtonText);
	M_TextBackOrCancelButton->SetText(M_ButtonText.M_BackOrCancelButtonText);
}

void UW_EscapeMenuSettings::InitialiseControlsFromSnapshot(const FRTSSettingsSnapshot& SettingsSnapshot)
{
	InitialiseGraphicsControls(SettingsSnapshot.M_GraphicsSettings);
	InitialiseAudioControls(SettingsSnapshot.M_AudioSettings);
	InitialiseControlSettingsControls(SettingsSnapshot.M_ControlSettings);
}

void UW_EscapeMenuSettings::InitialiseGraphicsControls(const FRTSGraphicsSettings& GraphicsSettings)
{
	InitialiseGraphicsDisplayControls(GraphicsSettings.M_DisplaySettings);
	InitialiseGraphicsScalabilityControls(GraphicsSettings.M_ScalabilityGroups);
	InitialiseGraphicsFrameRateControl(GraphicsSettings.M_DisplaySettings);
}

void UW_EscapeMenuSettings::InitialiseGraphicsDisplayControls(const FRTSDisplaySettings& DisplaySettings)
{
	const int32 WindowModeIndex = GetClampedOptionIndex(static_cast<int32>(DisplaySettings.M_WindowMode), M_WindowModeOptionTexts.Num());
	if (WindowModeIndex != INDEX_NONE)
	{
		M_ComboWindowMode->SetSelectedOption(M_WindowModeOptionTexts[WindowModeIndex].ToString());
	}

	int32 ResolutionIndex = FindResolutionIndex(DisplaySettings.M_Resolution);
	if (ResolutionIndex == INDEX_NONE)
	{
		const FString ResolutionLabel = BuildResolutionLabel(DisplaySettings.M_Resolution);
		M_SupportedResolutions.Add(DisplaySettings.M_Resolution);
		M_SupportedResolutionLabels.Add(ResolutionLabel);
		M_ComboResolution->AddOption(ResolutionLabel);
		ResolutionIndex = M_SupportedResolutionLabels.Num() - 1;
	}

	if (ResolutionIndex != INDEX_NONE)
	{
		M_ComboResolution->SetSelectedOption(M_SupportedResolutionLabels[ResolutionIndex]);
	}

	M_CheckVSync->SetIsChecked(DisplaySettings.bM_VSyncEnabled);

	const int32 OverallQualityIndex = GetClampedOptionIndex(static_cast<int32>(DisplaySettings.M_OverallQuality), M_QualityOptionTexts.Num());
	if (OverallQualityIndex != INDEX_NONE)
	{
		M_ComboOverallQuality->SetSelectedOption(M_QualityOptionTexts[OverallQualityIndex].ToString());
	}
}

void UW_EscapeMenuSettings::InitialiseGraphicsScalabilityControls(const FRTSScalabilityGroupSettings& ScalabilityGroups)
{
	SetQualityComboSelection(M_ComboViewDistanceQuality, ScalabilityGroups.M_ViewDistance);
	SetQualityComboSelection(M_ComboShadowsQuality, ScalabilityGroups.M_Shadows);
	SetQualityComboSelection(M_ComboTexturesQuality, ScalabilityGroups.M_Textures);
	SetQualityComboSelection(M_ComboEffectsQuality, ScalabilityGroups.M_Effects);
	SetQualityComboSelection(M_ComboPostProcessingQuality, ScalabilityGroups.M_PostProcessing);
}

void UW_EscapeMenuSettings::InitialiseGraphicsFrameRateControl(const FRTSDisplaySettings& DisplaySettings)
{
	const float ClampedFrameRate = FMath::Clamp(
		DisplaySettings.M_FrameRateLimit,
		M_SliderRanges.M_FrameRateLimitMin,
		M_SliderRanges.M_FrameRateLimitMax
	);
	M_SliderFrameRateLimit->SetValue(ClampedFrameRate);
}

void UW_EscapeMenuSettings::InitialiseAudioControls(const FRTSAudioSettings& AudioSettings)
{
	M_SliderMasterVolume->SetValue(AudioSettings.M_MasterVolume);
	M_SliderMusicVolume->SetValue(AudioSettings.M_MusicVolume);
	M_SliderSfxVolume->SetValue(AudioSettings.M_SfxVolume);
}

void UW_EscapeMenuSettings::InitialiseControlSettingsControls(const FRTSControlSettings& ControlSettings)
{
	const float ClampedSensitivity = FMath::Clamp(
		ControlSettings.M_MouseSensitivity,
		M_SliderRanges.M_MouseSensitivityMin,
		M_SliderRanges.M_MouseSensitivityMax
	);
	M_SliderMouseSensitivity->SetValue(ClampedSensitivity);
	M_CheckInvertYAxis->SetIsChecked(ControlSettings.bM_InvertYAxis);
}

void UW_EscapeMenuSettings::SetSliderRange(USlider* SliderToConfigure, const float MinValue, const float MaxValue) const
{
	if (SliderToConfigure == nullptr)
	{
		return;
	}

	SliderToConfigure->SetMinValue(MinValue);
	SliderToConfigure->SetMaxValue(MaxValue);
}

void UW_EscapeMenuSettings::SetQualityComboSelection(UComboBoxString* ComboBoxToSelect, const ERTSScalabilityQuality QualityToSelect) const
{
	if (ComboBoxToSelect == nullptr)
	{
		return;
	}

	const int32 QualityIndex = GetClampedOptionIndex(static_cast<int32>(QualityToSelect), M_QualityOptionTexts.Num());
	if (QualityIndex == INDEX_NONE)
	{
		return;
	}

	ComboBoxToSelect->SetSelectedOption(M_QualityOptionTexts[QualityIndex].ToString());
}

int32 UW_EscapeMenuSettings::GetClampedOptionIndex(const int32 DesiredIndex, const int32 OptionCount) const
{
	if (OptionCount <= 0)
	{
		return INDEX_NONE;
	}

	return FMath::Clamp(DesiredIndex, 0, OptionCount - 1);
}

FString UW_EscapeMenuSettings::BuildResolutionLabel(const FIntPoint& Resolution) const
{
	return FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y);
}

int32 UW_EscapeMenuSettings::FindResolutionIndex(const FIntPoint& Resolution) const
{
	for (int32 ResolutionIndex = 0; ResolutionIndex < M_SupportedResolutions.Num(); ++ResolutionIndex)
	{
		if (M_SupportedResolutions[ResolutionIndex] == Resolution)
		{
			return ResolutionIndex;
		}
	}

	return INDEX_NONE;
}

void UW_EscapeMenuSettings::BindButtonCallbacks()
{
	BindApplyButton();
	BindBackOrCancelButton();
}

void UW_EscapeMenuSettings::BindApplyButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenuSettings::BindApplyButton");
	if (not EnsureWidgetPointerIsValid(M_ButtonApply, TEXT("M_ButtonApply"), FunctionName))
	{
		return;
	}

	M_ButtonApply->OnClicked.RemoveAll(this);
	M_ButtonApply->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleApplyClicked);
}

void UW_EscapeMenuSettings::BindBackOrCancelButton()
{
	const FString FunctionName = TEXT("UW_EscapeMenuSettings::BindBackOrCancelButton");
	if (not EnsureWidgetPointerIsValid(M_ButtonBackOrCancel, TEXT("M_ButtonBackOrCancel"), FunctionName))
	{
		return;
	}

	M_ButtonBackOrCancel->OnClicked.RemoveAll(this);
	M_ButtonBackOrCancel->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleBackOrCancelClicked);
}

void UW_EscapeMenuSettings::BindSettingCallbacks()
{
	const FString FunctionName = TEXT("UW_EscapeMenuSettings::BindSettingCallbacks");
	if (not EnsureWidgetReferencesValid(FunctionName))
	{
		return;
	}

	M_ComboWindowMode->OnSelectionChanged.RemoveAll(this);
	M_ComboWindowMode->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleWindowModeSelectionChanged);

	M_ComboResolution->OnSelectionChanged.RemoveAll(this);
	M_ComboResolution->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleResolutionSelectionChanged);

	M_CheckVSync->OnCheckStateChanged.RemoveAll(this);
	M_CheckVSync->OnCheckStateChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleVSyncChanged);

	M_ComboOverallQuality->OnSelectionChanged.RemoveAll(this);
	M_ComboOverallQuality->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleOverallQualitySelectionChanged);

	M_ComboViewDistanceQuality->OnSelectionChanged.RemoveAll(this);
	M_ComboViewDistanceQuality->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleViewDistanceQualitySelectionChanged);

	M_ComboShadowsQuality->OnSelectionChanged.RemoveAll(this);
	M_ComboShadowsQuality->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleShadowsQualitySelectionChanged);

	M_ComboTexturesQuality->OnSelectionChanged.RemoveAll(this);
	M_ComboTexturesQuality->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleTexturesQualitySelectionChanged);

	M_ComboEffectsQuality->OnSelectionChanged.RemoveAll(this);
	M_ComboEffectsQuality->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleEffectsQualitySelectionChanged);

	M_ComboPostProcessingQuality->OnSelectionChanged.RemoveAll(this);
	M_ComboPostProcessingQuality->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandlePostProcessingQualitySelectionChanged);

	M_SliderFrameRateLimit->OnValueChanged.RemoveAll(this);
	M_SliderFrameRateLimit->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleFrameRateLimitChanged);

	M_SliderMasterVolume->OnValueChanged.RemoveAll(this);
	M_SliderMasterVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleMasterVolumeChanged);

	M_SliderMusicVolume->OnValueChanged.RemoveAll(this);
	M_SliderMusicVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleMusicVolumeChanged);

	M_SliderSfxVolume->OnValueChanged.RemoveAll(this);
	M_SliderSfxVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleSfxVolumeChanged);

	M_SliderMouseSensitivity->OnValueChanged.RemoveAll(this);
	M_SliderMouseSensitivity->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleMouseSensitivityChanged);

	M_CheckInvertYAxis->OnCheckStateChanged.RemoveAll(this);
	M_CheckInvertYAxis->OnCheckStateChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleInvertYAxisChanged);
}

bool UW_EscapeMenuSettings::GetIsValidMainGameUI() const
{
	if (not M_MainGameUI.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_MainGameUI"),
			TEXT("UW_EscapeMenuSettings::GetIsValidMainGameUI"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSettingsSubsystem() const
{
	if (not M_SettingsSubsystem.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SettingsSubsystem"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSettingsSubsystem"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::EnsureWidgetPointerIsValid(
	const UWidget* WidgetToCheck,
	const FString& WidgetName,
	const FString& FunctionName) const
{
	if (WidgetToCheck != nullptr)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, WidgetName, FunctionName, this);
	return false;
}

void UW_EscapeMenuSettings::HandleApplyClicked()
{
	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->ApplySettings();
	M_SettingsSubsystem->SaveSettings();
	RefreshControlsFromSubsystem();
}

void UW_EscapeMenuSettings::HandleBackOrCancelClicked()
{
	if (GetIsValidSettingsSubsystem())
	{
		M_SettingsSubsystem->RevertSettings();
		RefreshControlsFromSubsystem();
	}

	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuCloseSettings();
}

void UW_EscapeMenuSettings::HandleWindowModeSelectionChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboWindowMode->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Window mode selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingWindowMode(static_cast<ERTSWindowMode>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleResolutionSelectionChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const int32 SelectedIndex = M_SupportedResolutionLabels.IndexOfByKey(SelectedItem);
	if (SelectedIndex == INDEX_NONE || not M_SupportedResolutions.IsValidIndex(SelectedIndex))
	{
		RTSFunctionLibrary::ReportError(TEXT("Resolution selection did not map to a supported resolution."));
		return;
	}

	M_SettingsSubsystem->SetPendingResolution(M_SupportedResolutions[SelectedIndex]);
}

void UW_EscapeMenuSettings::HandleVSyncChanged(const bool bIsChecked)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingVSyncEnabled(bIsChecked);
}

void UW_EscapeMenuSettings::HandleOverallQualitySelectionChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboOverallQuality->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Overall quality selection was not found in the combo box options."));
		return;
	}

	const ERTSScalabilityQuality SelectedQuality = static_cast<ERTSScalabilityQuality>(SelectedIndex);
	M_SettingsSubsystem->SetPendingGraphicsQuality(SelectedQuality);

	bM_IsInitialisingControls = true;
	SetQualityComboSelection(M_ComboViewDistanceQuality, SelectedQuality);
	SetQualityComboSelection(M_ComboShadowsQuality, SelectedQuality);
	SetQualityComboSelection(M_ComboTexturesQuality, SelectedQuality);
	SetQualityComboSelection(M_ComboEffectsQuality, SelectedQuality);
	SetQualityComboSelection(M_ComboPostProcessingQuality, SelectedQuality);
	bM_IsInitialisingControls = false;
}

void UW_EscapeMenuSettings::HandleViewDistanceQualitySelectionChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboViewDistanceQuality->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("View distance quality selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingScalabilityGroupQuality(ERTSScalabilityGroup::ViewDistance, static_cast<ERTSScalabilityQuality>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleShadowsQualitySelectionChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboShadowsQuality->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Shadows quality selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingScalabilityGroupQuality(ERTSScalabilityGroup::Shadows, static_cast<ERTSScalabilityQuality>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleTexturesQualitySelectionChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboTexturesQuality->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Textures quality selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingScalabilityGroupQuality(ERTSScalabilityGroup::Textures, static_cast<ERTSScalabilityQuality>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleEffectsQualitySelectionChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboEffectsQuality->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Effects quality selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingScalabilityGroupQuality(ERTSScalabilityGroup::Effects, static_cast<ERTSScalabilityQuality>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandlePostProcessingQualitySelectionChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboPostProcessingQuality->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Post processing quality selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingScalabilityGroupQuality(ERTSScalabilityGroup::PostProcessing, static_cast<ERTSScalabilityQuality>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleFrameRateLimitChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingFrameRateLimit(NewValue);
}

void UW_EscapeMenuSettings::HandleMasterVolumeChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioChannel::Master, NewValue);
}

void UW_EscapeMenuSettings::HandleMusicVolumeChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioChannel::Music, NewValue);
}

void UW_EscapeMenuSettings::HandleSfxVolumeChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioChannel::Sfx, NewValue);
}

void UW_EscapeMenuSettings::HandleMouseSensitivityChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingMouseSensitivity(NewValue);
}

void UW_EscapeMenuSettings::HandleInvertYAxisChanged(const bool bIsChecked)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingInvertYAxis(bIsChecked);
}
