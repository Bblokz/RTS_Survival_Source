#include "RTS_Survival/GameUI/EscapeMenu/EscapeMenuSettings/W_EscapeMenuSettings.h"

#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/RichTextBlock.h"
#include "Components/Slider.h"
#include "Components/BackgroundBlur.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"
#include "RTS_Survival/Game/UserSettings/GameplaySettings/HealthbarVisibilityStrategy/HealthBarVisibilityStrategy.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace EscapeMenuSettingsOptionDefaults
{
	constexpr int32 WindowModeOptionCount = 3;
	constexpr int32 QualityOptionCount = 4;
	constexpr int32 SettingsSectionGameplayIndex = 0;
	constexpr int32 SettingsSectionGraphicsIndex = 1;
	constexpr int32 SettingsSectionAudioIndex = 2;
	constexpr int32 SettingsSectionControlsIndex = 3;

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

void UW_EscapeMenuSettings::NotifyMenuOpened()
{
	BP_OnOpenMenu();
}

void UW_EscapeMenuSettings::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (not GetAreGraphicsTextWidgetsValid())
	{
		return;
	}

	if (not GetAreAudioTextWidgetsValid())
	{
		return;
	}

	if (not GetAreControlsTextWidgetsValid())
	{
		return;
	}

	if (not GetAreGameplayTextWidgetsValid())
	{
		return;
	}

	if (not GetAreFooterTextWidgetsValid())
	{
		return;
	}

	UpdateAllRichTextBlocks();
}

void UW_EscapeMenuSettings::NativeConstruct()
{
	Super::NativeConstruct();

	if (not GetAreAllWidgetsValid())
	{
		return;
	}

	CacheSettingsSubsystem();
	BindButtonCallbacks();
	BindSettingCallbacks();
	CacheSettingsSectionButtonPadding();
	ApplySettingsSectionSelection(EscapeMenuSettingsOptionDefaults::SettingsSectionGameplayIndex);
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

void UW_EscapeMenuSettings::PopulateComboBoxOptions()
{
	if (not GetAreGraphicsWidgetsValid())
	{
		return;
	}

	if (not GetAreAudioWidgetsValid())
	{
		return;
	}

	if (not GetAreControlsWidgetsValid())
	{
		return;
	}

	if (not GetAreGameplayWidgetsValid())
	{
		return;
	}

	EnsureWindowModeOptionTexts();
	EnsureQualityOptionTexts();
	PopulateWindowModeOptions();
	PopulateResolutionOptions();
	PopulateQualityOptions();
	PopulateGameplayOptions();

	SetSliderRange(M_SliderFrameRateLimit, M_SliderRanges.M_FrameRateLimitMin, M_SliderRanges.M_FrameRateLimitMax);
	SetSliderRange(M_SliderMouseSensitivity, M_SliderRanges.M_MouseSensitivityMin, M_SliderRanges.M_MouseSensitivityMax);
	SetSliderRange(
		M_SliderCameraMovementSpeedMultiplier,
		M_SliderRanges.M_CameraMovementSpeedMultiplierMin,
		M_SliderRanges.M_CameraMovementSpeedMultiplierMax
	);
	SetSliderRange(
		M_SliderCameraPanSpeedMultiplier,
		M_SliderRanges.M_CameraPanSpeedMultiplierMin,
		M_SliderRanges.M_CameraPanSpeedMultiplierMax
	);
	SetSliderRange(M_SliderMasterVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	SetSliderRange(M_SliderMusicVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	SetSliderRange(M_SliderSfxVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
	SetSliderRange(M_SliderTransmissionsAndCinematicsVolume, RTSGameUserSettingsRanges::MinVolume, RTSGameUserSettingsRanges::MaxVolume);
}

void UW_EscapeMenuSettings::PopulateWindowModeOptions()
{
	if (not GetIsValidComboWindowMode())
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
	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboResolution())
	{
		return;
	}

	M_SupportedResolutions = M_SettingsSubsystem->GetSupportedResolutions();
	M_SupportedResolutionLabels = M_SettingsSubsystem->GetSupportedResolutionLabels();
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
	if (ComboBoxToPopulate == nullptr)
	{
		return;
	}

	ComboBoxToPopulate->ClearOptions();
	for (const FText& QualityOptionText : M_QualityOptionTexts)
	{
		ComboBoxToPopulate->AddOption(QualityOptionText.ToString());
	}
}

void UW_EscapeMenuSettings::PopulateGameplayOptions()
{
	const TArray<ERTSPlayerHealthBarVisibilityStrategy> PlayerStrategies =
	{
		ERTSPlayerHealthBarVisibilityStrategy::NotInitialized,
		ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults,
		ERTSPlayerHealthBarVisibilityStrategy::AwaysVisible,
		ERTSPlayerHealthBarVisibilityStrategy::VisibleWhenDamagedOnly,
		ERTSPlayerHealthBarVisibilityStrategy::VisibleOnSelectionAndDamaged,
		ERTSPlayerHealthBarVisibilityStrategy::VisibleOnSelectionOnly,
		ERTSPlayerHealthBarVisibilityStrategy::NeverVisible
	};

	const TArray<ERTSEnemyHealthBarVisibilityStrategy> EnemyStrategies =
	{
		ERTSEnemyHealthBarVisibilityStrategy::NotInitialized,
		ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults,
		ERTSEnemyHealthBarVisibilityStrategy::AwaysVisible,
		ERTSEnemyHealthBarVisibilityStrategy::VisibleWhenDamagedOnly
	};

	TArray<FText> PlayerOptionTexts;
	PlayerOptionTexts.Reserve(PlayerStrategies.Num());
	for (const ERTSPlayerHealthBarVisibilityStrategy Strategy : PlayerStrategies)
	{
		PlayerOptionTexts.Add(global_GetPlayerVisibilityStrategyText(Strategy));
	}

	TArray<FText> EnemyOptionTexts;
	EnemyOptionTexts.Reserve(EnemyStrategies.Num());
	for (const ERTSEnemyHealthBarVisibilityStrategy Strategy : EnemyStrategies)
	{
		EnemyOptionTexts.Add(global_GetEnemyVisibilityStrategyText(Strategy));
	}

	PopulateVisibilityOptionsForComboBox(M_ComboOverwriteAllPlayerHpBarStrat, PlayerOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboPlayerTankHpBarStrat, PlayerOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboPlayerSquadHpBarStrat, PlayerOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboPlayerNomadicHpBarStrat, PlayerOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboPlayerBxpHpBarStrat, PlayerOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboPlayerAircraftHpBarStrat, PlayerOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboOverwriteAllEnemyHpBarStrat, EnemyOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboEnemyTankHpBarStrat, EnemyOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboEnemySquadHpBarStrat, EnemyOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboEnemyNomadicHpBarStrat, EnemyOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboEnemyBxpHpBarStrat, EnemyOptionTexts);
	PopulateVisibilityOptionsForComboBox(M_ComboEnemyAircraftHpBarStrat, EnemyOptionTexts);
}

void UW_EscapeMenuSettings::PopulateVisibilityOptionsForComboBox(
	UComboBoxString* ComboBoxToPopulate,
	const TArray<FText>& OptionTexts) const
{
	if (ComboBoxToPopulate == nullptr)
	{
		return;
	}

	ComboBoxToPopulate->ClearOptions();
	for (const FText& OptionText : OptionTexts)
	{
		ComboBoxToPopulate->AddOption(OptionText.ToString());
	}
}

void UW_EscapeMenuSettings::SetPlayerHealthBarStrategySelection(
	UComboBoxString* ComboBoxToSelect,
	const ERTSPlayerHealthBarVisibilityStrategy Strategy) const
{
	if (ComboBoxToSelect == nullptr)
	{
		return;
	}

	ComboBoxToSelect->SetSelectedOption(global_GetPlayerVisibilityStrategyText(Strategy).ToString());
}

void UW_EscapeMenuSettings::SetEnemyHealthBarStrategySelection(
	UComboBoxString* ComboBoxToSelect,
	const ERTSEnemyHealthBarVisibilityStrategy Strategy) const
{
	if (ComboBoxToSelect == nullptr)
	{
		return;
	}

	ComboBoxToSelect->SetSelectedOption(global_GetEnemyVisibilityStrategyText(Strategy).ToString());
}

void UW_EscapeMenuSettings::ApplyOverwriteAllPlayerHealthBarStrategy(const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	bM_IsInitialisingControls = true;
	SetPlayerHealthBarStrategySelection(M_ComboPlayerTankHpBarStrat, Strategy);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerSquadHpBarStrat, Strategy);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerNomadicHpBarStrat, Strategy);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerBxpHpBarStrat, Strategy);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerAircraftHpBarStrat, Strategy);
	bM_IsInitialisingControls = false;

	M_SettingsSubsystem->SetPendingPlayerTankHpBarStrat(Strategy);
	M_SettingsSubsystem->SetPendingPlayerSquadHpBarStrat(Strategy);
	M_SettingsSubsystem->SetPendingPlayerNomadicHpBarStrat(Strategy);
	M_SettingsSubsystem->SetPendingPlayerBxpHpBarStrat(Strategy);
	M_SettingsSubsystem->SetPendingPlayerAircraftHpBarStrat(Strategy);
}

void UW_EscapeMenuSettings::ApplyOverwriteAllEnemyHealthBarStrategy(const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	bM_IsInitialisingControls = true;
	SetEnemyHealthBarStrategySelection(M_ComboEnemyTankHpBarStrat, Strategy);
	SetEnemyHealthBarStrategySelection(M_ComboEnemySquadHpBarStrat, Strategy);
	SetEnemyHealthBarStrategySelection(M_ComboEnemyNomadicHpBarStrat, Strategy);
	SetEnemyHealthBarStrategySelection(M_ComboEnemyBxpHpBarStrat, Strategy);
	SetEnemyHealthBarStrategySelection(M_ComboEnemyAircraftHpBarStrat, Strategy);
	bM_IsInitialisingControls = false;

	M_SettingsSubsystem->SetPendingEnemyTankHpBarStrat(Strategy);
	M_SettingsSubsystem->SetPendingEnemySquadHpBarStrat(Strategy);
	M_SettingsSubsystem->SetPendingEnemyNomadicHpBarStrat(Strategy);
	M_SettingsSubsystem->SetPendingEnemyBxpHpBarStrat(Strategy);
	M_SettingsSubsystem->SetPendingEnemyAircraftHpBarStrat(Strategy);
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
	if (not GetAreGraphicsTextWidgetsValid())
	{
		return;
	}

	if (not GetAreAudioTextWidgetsValid())
	{
		return;
	}

	if (not GetAreControlsTextWidgetsValid())
	{
		return;
	}

	if (not GetAreFooterTextWidgetsValid())
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
	M_TextVoicelinesVolumeLabel->SetText(M_AudioText.M_VoicelinesVolumeLabelText);
	M_TextAnnouncerVolumeLabel->SetText(M_AudioText.M_AnnouncerVolumeLabelText);
	M_TextTransmissionsAndCinematicsVolumeLabel->SetText(M_AudioText.M_TransmissionsAndCinematicsVolumeLabelText);
	M_TextUiVolumeLabel->SetText(M_AudioText.M_UiVolumeLabelText);

	M_TextControlsHeader->SetText(M_ControlsText.M_HeaderText);
	M_TextMouseSensitivityLabel->SetText(M_ControlsText.M_MouseSensitivityLabelText);
	M_TextCameraMovementSpeedMultiplierLabel->SetText(M_ControlsText.M_CameraMovementSpeedMultiplierLabelText);
	M_TextCameraPanSpeedMultiplierLabel->SetText(M_ControlsText.M_CameraPanSpeedMultiplierLabelText);
	M_TextInvertYAxisLabel->SetText(M_ControlsText.M_InvertYAxisLabelText);

	M_TextGameplayHeader->SetText(M_GameplayText.M_HeaderText);
	M_TextHideActionButtonHotkeysLabel->SetText(M_GameplayText.M_HideActionButtonHotkeysLabelText);
	M_TextOverwriteAllPlayerHpBarLabel->SetText(M_GameplayText.M_OverwriteAllPlayerHpBarLabelText);
	M_TextPlayerTankHpBarLabel->SetText(M_GameplayText.M_PlayerTankHpBarLabelText);
	M_TextPlayerSquadHpBarLabel->SetText(M_GameplayText.M_PlayerSquadHpBarLabelText);
	M_TextPlayerNomadicHpBarLabel->SetText(M_GameplayText.M_PlayerNomadicHpBarLabelText);
	M_TextPlayerBxpHpBarLabel->SetText(M_GameplayText.M_PlayerBxpHpBarLabelText);
	M_TextPlayerAircraftHpBarLabel->SetText(M_GameplayText.M_PlayerAircraftHpBarLabelText);
	M_TextOverwriteAllEnemyHpBarLabel->SetText(M_GameplayText.M_OverwriteAllEnemyHpBarLabelText);
	M_TextEnemyTankHpBarLabel->SetText(M_GameplayText.M_EnemyTankHpBarLabelText);
	M_TextEnemySquadHpBarLabel->SetText(M_GameplayText.M_EnemySquadHpBarLabelText);
	M_TextEnemyNomadicHpBarLabel->SetText(M_GameplayText.M_EnemyNomadicHpBarLabelText);
	M_TextEnemyBxpHpBarLabel->SetText(M_GameplayText.M_EnemyBxpHpBarLabelText);
	M_TextEnemyAircraftHpBarLabel->SetText(M_GameplayText.M_EnemyAircraftHpBarLabelText);

	M_TextApplyButton->SetText(M_ButtonText.M_ApplyButtonText);
	M_TextSetDefaultsButton->SetText(M_ButtonText.M_SetDefaultsButtonText);
	M_TextBackOrCancelButton->SetText(M_ButtonText.M_BackOrCancelButtonText);
}

void UW_EscapeMenuSettings::InitialiseControlsFromSnapshot(const FRTSSettingsSnapshot& SettingsSnapshot)
{
	InitialiseGraphicsControls(SettingsSnapshot.M_GraphicsSettings);
	InitialiseAudioControls(SettingsSnapshot.M_AudioSettings);
	InitialiseControlSettingsControls(SettingsSnapshot.M_ControlSettings);
	InitialiseGameplayControls(SettingsSnapshot.M_GameplaySettings);
}

void UW_EscapeMenuSettings::InitialiseGraphicsControls(const FRTSGraphicsSettings& GraphicsSettings)
{
	InitialiseGraphicsDisplayControls(GraphicsSettings.M_DisplaySettings);
	InitialiseGraphicsScalabilityControls(GraphicsSettings.M_ScalabilityGroups);
	InitialiseGraphicsFrameRateControl(GraphicsSettings.M_DisplaySettings);
}

void UW_EscapeMenuSettings::InitialiseGraphicsDisplayControls(const FRTSDisplaySettings& DisplaySettings)
{
	if (not GetAreGraphicsDisplayWidgetsValid())
	{
		return;
	}

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
	if (not GetAreGraphicsScalabilityWidgetsValid())
	{
		return;
	}

	SetQualityComboSelection(M_ComboViewDistanceQuality, ScalabilityGroups.M_ViewDistance);
	SetQualityComboSelection(M_ComboShadowsQuality, ScalabilityGroups.M_Shadows);
	SetQualityComboSelection(M_ComboTexturesQuality, ScalabilityGroups.M_Textures);
	SetQualityComboSelection(M_ComboEffectsQuality, ScalabilityGroups.M_Effects);
	SetQualityComboSelection(M_ComboPostProcessingQuality, ScalabilityGroups.M_PostProcessing);
}

void UW_EscapeMenuSettings::InitialiseGraphicsFrameRateControl(const FRTSDisplaySettings& DisplaySettings)
{
	if (not GetAreGraphicsFrameRateWidgetsValid())
	{
		return;
	}

	const float ClampedFrameRate = FMath::Clamp(
		DisplaySettings.M_FrameRateLimit,
		M_SliderRanges.M_FrameRateLimitMin,
		M_SliderRanges.M_FrameRateLimitMax
	);
	M_SliderFrameRateLimit->SetValue(ClampedFrameRate);
}

void UW_EscapeMenuSettings::InitialiseAudioControls(const FRTSAudioSettings& AudioSettings)
{
	if (not GetAreAudioSliderWidgetsValid())
	{
		return;
	}

	M_SliderMasterVolume->SetValue(AudioSettings.M_MasterVolume);
	M_SliderMusicVolume->SetValue(AudioSettings.M_MusicVolume);
	M_SliderSfxVolume->SetValue(AudioSettings.M_SfxAndWeaponsVolume);
	M_SliderVoicelinesVolume->SetValue(AudioSettings.M_VoicelinesVolume);
	M_SliderAnnouncerVolume->SetValue(AudioSettings.M_AnnouncerVolume);
	M_SliderTransmissionsAndCinematicsVolume->SetValue(AudioSettings.M_TransmissionsAndCinematicsVolume);
	M_SliderUiVolume->SetValue(AudioSettings.M_UiVolume);
}

void UW_EscapeMenuSettings::InitialiseControlSettingsControls(const FRTSControlSettings& ControlSettings)
{
	if (not GetAreControlSettingsWidgetsValid())
	{
		return;
	}

	const float ClampedSensitivity = FMath::Clamp(
		ControlSettings.M_MouseSensitivity,
		M_SliderRanges.M_MouseSensitivityMin,
		M_SliderRanges.M_MouseSensitivityMax
	);
	M_SliderMouseSensitivity->SetValue(ClampedSensitivity);

	const float ClampedCameraMovementSpeedMultiplier = FMath::Clamp(
		ControlSettings.M_CameraMovementSpeedMultiplier,
		M_SliderRanges.M_CameraMovementSpeedMultiplierMin,
		M_SliderRanges.M_CameraMovementSpeedMultiplierMax
	);
	M_SliderCameraMovementSpeedMultiplier->SetValue(ClampedCameraMovementSpeedMultiplier);

	const float ClampedCameraPanSpeedMultiplier = FMath::Clamp(
		ControlSettings.M_CameraPanSpeedMultiplier,
		M_SliderRanges.M_CameraPanSpeedMultiplierMin,
		M_SliderRanges.M_CameraPanSpeedMultiplierMax
	);
	M_SliderCameraPanSpeedMultiplier->SetValue(ClampedCameraPanSpeedMultiplier);
	M_CheckInvertYAxis->SetIsChecked(ControlSettings.bM_InvertYAxis);
}

void UW_EscapeMenuSettings::InitialiseGameplayControls(const FRTSGameplaySettings& GameplaySettings)
{
	if (not GetAreGameplayWidgetsValid())
	{
		return;
	}

	M_CheckHideActionButtonHotkeys->SetIsChecked(GameplaySettings.bM_HideActionButtonHotkeys);
	SetPlayerHealthBarStrategySelection(M_ComboOverwriteAllPlayerHpBarStrat, GameplaySettings.M_OverwriteAllPlayerHpBarStrat);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerTankHpBarStrat, GameplaySettings.M_PlayerTankHpBarStrat);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerSquadHpBarStrat, GameplaySettings.M_PlayerSquadHpBarStrat);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerNomadicHpBarStrat, GameplaySettings.M_PlayerNomadicHpBarStrat);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerBxpHpBarStrat, GameplaySettings.M_PlayerBxpHpBarStrat);
	SetPlayerHealthBarStrategySelection(M_ComboPlayerAircraftHpBarStrat, GameplaySettings.M_PlayerAircraftHpBarStrat);
	SetEnemyHealthBarStrategySelection(M_ComboOverwriteAllEnemyHpBarStrat, GameplaySettings.M_OverwriteAllEnemyHpBarStrat);
	SetEnemyHealthBarStrategySelection(M_ComboEnemyTankHpBarStrat, GameplaySettings.M_EnemyTankHpBarStrat);
	SetEnemyHealthBarStrategySelection(M_ComboEnemySquadHpBarStrat, GameplaySettings.M_EnemySquadHpBarStrat);
	SetEnemyHealthBarStrategySelection(M_ComboEnemyNomadicHpBarStrat, GameplaySettings.M_EnemyNomadicHpBarStrat);
	SetEnemyHealthBarStrategySelection(M_ComboEnemyBxpHpBarStrat, GameplaySettings.M_EnemyBxpHpBarStrat);
	SetEnemyHealthBarStrategySelection(M_ComboEnemyAircraftHpBarStrat, GameplaySettings.M_EnemyAircraftHpBarStrat);
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
	BindSetDefaultsButton();
	BindBackOrCancelButton();
	BindSettingsSectionButtons();
}

void UW_EscapeMenuSettings::BindApplyButton()
{
	if (not GetIsValidButtonApply())
	{
		return;
	}

	M_ButtonApply->OnClicked.RemoveAll(this);
	M_ButtonApply->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleApplyClicked);
}

void UW_EscapeMenuSettings::BindSetDefaultsButton()
{
	if (not GetIsValidButtonSetDefaults())
	{
		return;
	}

	M_ButtonSetDefaults->OnClicked.RemoveAll(this);
	M_ButtonSetDefaults->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleSetDefaultsClicked);
}

void UW_EscapeMenuSettings::BindBackOrCancelButton()
{
	if (not GetIsValidButtonBackOrCancel())
	{
		return;
	}

	M_ButtonBackOrCancel->OnClicked.RemoveAll(this);
	M_ButtonBackOrCancel->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleBackOrCancelClicked);
}

void UW_EscapeMenuSettings::BindSettingsSectionButtons()
{
	if (not GetIsValidButtonGameplaySettings())
	{
		return;
	}

	if (not GetIsValidButtonGraphicsSettings())
	{
		return;
	}

	if (not GetIsValidButtonAudioSettings())
	{
		return;
	}

	if (not GetIsValidButtonControlSettings())
	{
		return;
	}

	M_GameplaySettings->OnClicked.RemoveAll(this);
	M_GameplaySettings->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleGameplaySettingsClicked);

	M_GraphicsSettings->OnClicked.RemoveAll(this);
	M_GraphicsSettings->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleGraphicsSettingsClicked);

	M_AudioSettings->OnClicked.RemoveAll(this);
	M_AudioSettings->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleAudioSettingsClicked);

	M_ControlSettings->OnClicked.RemoveAll(this);
	M_ControlSettings->OnClicked.AddDynamic(this, &UW_EscapeMenuSettings::HandleControlSettingsClicked);
}

void UW_EscapeMenuSettings::CacheSettingsSectionButtonPadding()
{
	if (bM_HasCachedSettingsSectionPadding)
	{
		return;
	}

	if (not GetIsValidButtonGameplaySettings())
	{
		return;
	}

	if (not GetIsValidButtonGraphicsSettings())
	{
		return;
	}

	if (not GetIsValidButtonAudioSettings())
	{
		return;
	}

	if (not GetIsValidButtonControlSettings())
	{
		return;
	}

	M_SettingsSectionButtonPadding.M_GameplayPadding = GetButtonSlotPadding(M_GameplaySettings);
	M_SettingsSectionButtonPadding.M_GraphicsPadding = GetButtonSlotPadding(M_GraphicsSettings);
	M_SettingsSectionButtonPadding.M_AudioPadding = GetButtonSlotPadding(M_AudioSettings);
	M_SettingsSectionButtonPadding.M_ControlsPadding = GetButtonSlotPadding(M_ControlSettings);
	bM_HasCachedSettingsSectionPadding = true;
}

void UW_EscapeMenuSettings::ApplySettingsSectionSelection(const int32 NewIndex)
{
	if (not GetIsValidSettingsSwitch())
	{
		return;
	}

	const int32 ClampedIndex = FMath::Clamp(
		NewIndex,
		EscapeMenuSettingsOptionDefaults::SettingsSectionGameplayIndex,
		EscapeMenuSettingsOptionDefaults::SettingsSectionControlsIndex
	);

	CacheSettingsSectionButtonPadding();
	M_SettingsSwitch->SetActiveWidgetIndex(ClampedIndex);
	UpdateSettingsSectionButtonPadding(ClampedIndex);
	BP_OnSettingsMenuChange(ClampedIndex);
}

void UW_EscapeMenuSettings::UpdateSettingsSectionButtonPadding(const int32 SelectedIndex)
{
	if (not bM_HasCachedSettingsSectionPadding)
	{
		return;
	}

	SetButtonSlotPadding(M_GameplaySettings, M_SettingsSectionButtonPadding.M_GameplayPadding);
	SetButtonSlotPadding(M_GraphicsSettings, M_SettingsSectionButtonPadding.M_GraphicsPadding);
	SetButtonSlotPadding(M_AudioSettings, M_SettingsSectionButtonPadding.M_AudioPadding);
	SetButtonSlotPadding(M_ControlSettings, M_SettingsSectionButtonPadding.M_ControlsPadding);

	if (SelectedIndex == EscapeMenuSettingsOptionDefaults::SettingsSectionGameplayIndex)
	{
		SetButtonSlotPadding(
			M_GameplaySettings,
			BuildSelectedButtonPadding(M_SettingsSectionButtonPadding.M_GameplayPadding)
		);
		return;
	}

	if (SelectedIndex == EscapeMenuSettingsOptionDefaults::SettingsSectionGraphicsIndex)
	{
		SetButtonSlotPadding(
			M_GraphicsSettings,
			BuildSelectedButtonPadding(M_SettingsSectionButtonPadding.M_GraphicsPadding)
		);
		return;
	}

	if (SelectedIndex == EscapeMenuSettingsOptionDefaults::SettingsSectionAudioIndex)
	{
		SetButtonSlotPadding(
			M_AudioSettings,
			BuildSelectedButtonPadding(M_SettingsSectionButtonPadding.M_AudioPadding)
		);
		return;
	}

	if (SelectedIndex == EscapeMenuSettingsOptionDefaults::SettingsSectionControlsIndex)
	{
		SetButtonSlotPadding(
			M_ControlSettings,
			BuildSelectedButtonPadding(M_SettingsSectionButtonPadding.M_ControlsPadding)
		);
	}
}

FMargin UW_EscapeMenuSettings::BuildSelectedButtonPadding(const FMargin& BasePadding) const
{
	return FMargin(
		BasePadding.Left + M_ExtraLeftPadding,
		BasePadding.Top,
		BasePadding.Right,
		BasePadding.Bottom
	);
}

FMargin UW_EscapeMenuSettings::GetButtonSlotPadding(const UButton* Button) const
{
	if (not IsValid(Button))
	{
		return FMargin();
	}

	if (const UHorizontalBoxSlot* const HorizontalSlot = Cast<UHorizontalBoxSlot>(Button->Slot))
	{
		return HorizontalSlot->GetPadding();
	}

	if (const UVerticalBoxSlot* const VerticalSlot = Cast<UVerticalBoxSlot>(Button->Slot))
	{
		return VerticalSlot->GetPadding();
	}

	return FMargin();
}

void UW_EscapeMenuSettings::SetButtonSlotPadding(UButton* Button, const FMargin& NewPadding) const
{
	if (not IsValid(Button))
	{
		return;
	}

	if (UHorizontalBoxSlot* const HorizontalSlot = Cast<UHorizontalBoxSlot>(Button->Slot))
	{
		HorizontalSlot->SetPadding(NewPadding);
		return;
	}

	if (UVerticalBoxSlot* const VerticalSlot = Cast<UVerticalBoxSlot>(Button->Slot))
	{
		VerticalSlot->SetPadding(NewPadding);
	}
}

void UW_EscapeMenuSettings::BindSettingCallbacks()
{
	if (not GetAreGraphicsWidgetsValid())
	{
		return;
	}

	if (not GetAreAudioWidgetsValid())
	{
		return;
	}

	if (not GetAreControlsWidgetsValid())
	{
		return;
	}

	if (not GetAreGameplayWidgetsValid())
	{
		return;
	}

	BindGraphicsSettingCallbacks();
	BindAudioSettingCallbacks();
	BindControlSettingCallbacks();
	BindGameplaySettingCallbacks();
}

void UW_EscapeMenuSettings::BindGraphicsSettingCallbacks()
{
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
}

void UW_EscapeMenuSettings::BindAudioSettingCallbacks()
{
	M_SliderMasterVolume->OnValueChanged.RemoveAll(this);
	M_SliderMasterVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleMasterVolumeChanged);

	M_SliderMusicVolume->OnValueChanged.RemoveAll(this);
	M_SliderMusicVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleMusicVolumeChanged);

	M_SliderSfxVolume->OnValueChanged.RemoveAll(this);
	M_SliderSfxVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleSfxVolumeChanged);

	M_SliderVoicelinesVolume->OnValueChanged.RemoveAll(this);
	M_SliderVoicelinesVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleVoicelinesVolumeChanged);

	M_SliderAnnouncerVolume->OnValueChanged.RemoveAll(this);
	M_SliderAnnouncerVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleAnnouncerVolumeChanged);

	M_SliderTransmissionsAndCinematicsVolume->OnValueChanged.RemoveAll(this);
	M_SliderTransmissionsAndCinematicsVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleTransmissionsAndCinematicsVolumeChanged);

	M_SliderUiVolume->OnValueChanged.RemoveAll(this);
	M_SliderUiVolume->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleUiVolumeChanged);
}

void UW_EscapeMenuSettings::BindControlSettingCallbacks()
{
	M_SliderMouseSensitivity->OnValueChanged.RemoveAll(this);
	M_SliderMouseSensitivity->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleMouseSensitivityChanged);

	M_SliderCameraMovementSpeedMultiplier->OnValueChanged.RemoveAll(this);
	M_SliderCameraMovementSpeedMultiplier->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleCameraMovementSpeedMultiplierChanged);

	M_SliderCameraPanSpeedMultiplier->OnValueChanged.RemoveAll(this);
	M_SliderCameraPanSpeedMultiplier->OnValueChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleCameraPanSpeedMultiplierChanged);

	M_CheckInvertYAxis->OnCheckStateChanged.RemoveAll(this);
	M_CheckInvertYAxis->OnCheckStateChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleInvertYAxisChanged);
}

void UW_EscapeMenuSettings::BindGameplaySettingCallbacks()
{
	M_CheckHideActionButtonHotkeys->OnCheckStateChanged.RemoveAll(this);
	M_CheckHideActionButtonHotkeys->OnCheckStateChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleHideActionButtonHotkeysChanged);

	M_ComboOverwriteAllPlayerHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboOverwriteAllPlayerHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleOverwriteAllPlayerHpBarStratChanged);

	M_ComboPlayerTankHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboPlayerTankHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandlePlayerTankHpBarStratChanged);

	M_ComboPlayerSquadHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboPlayerSquadHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandlePlayerSquadHpBarStratChanged);

	M_ComboPlayerNomadicHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboPlayerNomadicHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandlePlayerNomadicHpBarStratChanged);

	M_ComboPlayerBxpHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboPlayerBxpHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandlePlayerBxpHpBarStratChanged);

	M_ComboPlayerAircraftHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboPlayerAircraftHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandlePlayerAircraftHpBarStratChanged);

	M_ComboOverwriteAllEnemyHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboOverwriteAllEnemyHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleOverwriteAllEnemyHpBarStratChanged);

	M_ComboEnemyTankHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboEnemyTankHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleEnemyTankHpBarStratChanged);

	M_ComboEnemySquadHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboEnemySquadHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleEnemySquadHpBarStratChanged);

	M_ComboEnemyNomadicHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboEnemyNomadicHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleEnemyNomadicHpBarStratChanged);

	M_ComboEnemyBxpHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboEnemyBxpHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleEnemyBxpHpBarStratChanged);

	M_ComboEnemyAircraftHpBarStrat->OnSelectionChanged.RemoveAll(this);
	M_ComboEnemyAircraftHpBarStrat->OnSelectionChanged.AddDynamic(this, &UW_EscapeMenuSettings::HandleEnemyAircraftHpBarStratChanged);
}

bool UW_EscapeMenuSettings::GetIsValidPlayerController() const
{
	if (not M_PlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_PlayerController"),
			TEXT("UW_EscapeMenuSettings::GetIsValidPlayerController"),
			this
		);
		return false;
	}

	return true;
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

bool UW_EscapeMenuSettings::GetIsValidRootOverlay() const
{
	if (not IsValid(M_RootOverlay))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_RootOverlay"),
			TEXT("UW_EscapeMenuSettings::GetIsValidRootOverlay"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidBackgroundBlurFullscreen() const
{
	if (not IsValid(M_BackgroundBlurFullscreen))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_BackgroundBlurFullscreen"),
			TEXT("UW_EscapeMenuSettings::GetIsValidBackgroundBlurFullscreen"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSizeBoxMenuRoot() const
{
	if (not IsValid(M_SizeBoxMenuRoot))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SizeBoxMenuRoot"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSizeBoxMenuRoot"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidVerticalBoxMenuRoot() const
{
	if (not IsValid(M_VerticalBoxMenuRoot))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_VerticalBoxMenuRoot"),
			TEXT("UW_EscapeMenuSettings::GetIsValidVerticalBoxMenuRoot"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidVerticalBoxSections() const
{
	if (not IsValid(M_VerticalBoxSections))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_VerticalBoxSections"),
			TEXT("UW_EscapeMenuSettings::GetIsValidVerticalBoxSections"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidHorizontalBoxFooterButtons() const
{
	if (not IsValid(M_HorizontalBoxFooterButtons))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_HorizontalBoxFooterButtons"),
			TEXT("UW_EscapeMenuSettings::GetIsValidHorizontalBoxFooterButtons"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSettingsSwitch() const
{
	if (not IsValid(M_SettingsSwitch))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SettingsSwitch"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSettingsSwitch"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidButtonGameplaySettings() const
{
	if (not IsValid(M_GameplaySettings))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GameplaySettings"),
			TEXT("UW_EscapeMenuSettings::GetIsValidButtonGameplaySettings"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidButtonGraphicsSettings() const
{
	if (not IsValid(M_GraphicsSettings))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_GraphicsSettings"),
			TEXT("UW_EscapeMenuSettings::GetIsValidButtonGraphicsSettings"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidButtonAudioSettings() const
{
	if (not IsValid(M_AudioSettings))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_AudioSettings"),
			TEXT("UW_EscapeMenuSettings::GetIsValidButtonAudioSettings"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidButtonControlSettings() const
{
	if (not IsValid(M_ControlSettings))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ControlSettings"),
			TEXT("UW_EscapeMenuSettings::GetIsValidButtonControlSettings"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidVerticalBoxGraphicsSection() const
{
	if (not IsValid(M_VerticalBoxGraphicsSection))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_VerticalBoxGraphicsSection"),
			TEXT("UW_EscapeMenuSettings::GetIsValidVerticalBoxGraphicsSection"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidVerticalBoxAudioSection() const
{
	if (not IsValid(M_VerticalBoxAudioSection))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_VerticalBoxAudioSection"),
			TEXT("UW_EscapeMenuSettings::GetIsValidVerticalBoxAudioSection"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidVerticalBoxControlsSection() const
{
	if (not IsValid(M_VerticalBoxControlsSection))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_VerticalBoxControlsSection"),
			TEXT("UW_EscapeMenuSettings::GetIsValidVerticalBoxControlsSection"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidVerticalBoxGameplaySection() const
{
	if (not IsValid(M_VerticalBoxGameplaySection))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_VerticalBoxGameplaySection"),
			TEXT("UW_EscapeMenuSettings::GetIsValidVerticalBoxGameplaySection"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextGraphicsHeader() const
{
	if (not IsValid(M_TextGraphicsHeader))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextGraphicsHeader"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextGraphicsHeader"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextWindowModeLabel() const
{
	if (not IsValid(M_TextWindowModeLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextWindowModeLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextWindowModeLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextResolutionLabel() const
{
	if (not IsValid(M_TextResolutionLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextResolutionLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextResolutionLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextVSyncLabel() const
{
	if (not IsValid(M_TextVSyncLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextVSyncLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextVSyncLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextOverallQualityLabel() const
{
	if (not IsValid(M_TextOverallQualityLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextOverallQualityLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextOverallQualityLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextViewDistanceLabel() const
{
	if (not IsValid(M_TextViewDistanceLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextViewDistanceLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextViewDistanceLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextShadowsLabel() const
{
	if (not IsValid(M_TextShadowsLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextShadowsLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextShadowsLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextTexturesLabel() const
{
	if (not IsValid(M_TextTexturesLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextTexturesLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextTexturesLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextEffectsLabel() const
{
	if (not IsValid(M_TextEffectsLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextEffectsLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextEffectsLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextPostProcessingLabel() const
{
	if (not IsValid(M_TextPostProcessingLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextPostProcessingLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextPostProcessingLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextFrameRateLimitLabel() const
{
	if (not IsValid(M_TextFrameRateLimitLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextFrameRateLimitLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextFrameRateLimitLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboWindowMode() const
{
	if (not IsValid(M_ComboWindowMode))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboWindowMode"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboWindowMode"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboResolution() const
{
	if (not IsValid(M_ComboResolution))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboResolution"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboResolution"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidCheckVSync() const
{
	if (not IsValid(M_CheckVSync))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_CheckVSync"),
			TEXT("UW_EscapeMenuSettings::GetIsValidCheckVSync"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboOverallQuality() const
{
	if (not IsValid(M_ComboOverallQuality))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboOverallQuality"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboOverallQuality"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboViewDistanceQuality() const
{
	if (not IsValid(M_ComboViewDistanceQuality))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboViewDistanceQuality"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboViewDistanceQuality"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboShadowsQuality() const
{
	if (not IsValid(M_ComboShadowsQuality))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboShadowsQuality"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboShadowsQuality"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboTexturesQuality() const
{
	if (not IsValid(M_ComboTexturesQuality))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboTexturesQuality"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboTexturesQuality"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboEffectsQuality() const
{
	if (not IsValid(M_ComboEffectsQuality))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboEffectsQuality"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboEffectsQuality"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboPostProcessingQuality() const
{
	if (not IsValid(M_ComboPostProcessingQuality))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboPostProcessingQuality"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboPostProcessingQuality"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderFrameRateLimit() const
{
	if (not IsValid(M_SliderFrameRateLimit))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderFrameRateLimit"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderFrameRateLimit"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextAudioHeader() const
{
	if (not IsValid(M_TextAudioHeader))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextAudioHeader"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextAudioHeader"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextMasterVolumeLabel() const
{
	if (not IsValid(M_TextMasterVolumeLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextMasterVolumeLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextMasterVolumeLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextMusicVolumeLabel() const
{
	if (not IsValid(M_TextMusicVolumeLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextMusicVolumeLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextMusicVolumeLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextSfxVolumeLabel() const
{
	if (not IsValid(M_TextSfxVolumeLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextSfxVolumeLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextSfxVolumeLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextVoicelinesVolumeLabel() const
{
	if (not IsValid(M_TextVoicelinesVolumeLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextVoicelinesVolumeLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextVoicelinesVolumeLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextAnnouncerVolumeLabel() const
{
	if (not IsValid(M_TextAnnouncerVolumeLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextAnnouncerVolumeLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextAnnouncerVolumeLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextTransmissionsAndCinematicsVolumeLabel() const
{
	if (not IsValid(M_TextTransmissionsAndCinematicsVolumeLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextTransmissionsAndCinematicsVolumeLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextTransmissionsAndCinematicsVolumeLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextUiVolumeLabel() const
{
	if (not IsValid(M_TextUiVolumeLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextUiVolumeLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextUiVolumeLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderMasterVolume() const
{
	if (not IsValid(M_SliderMasterVolume))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderMasterVolume"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderMasterVolume"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderMusicVolume() const
{
	if (not IsValid(M_SliderMusicVolume))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderMusicVolume"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderMusicVolume"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderSfxVolume() const
{
	if (not IsValid(M_SliderSfxVolume))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderSfxVolume"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderSfxVolume"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderVoicelinesVolume() const
{
	if (not IsValid(M_SliderVoicelinesVolume))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderVoicelinesVolume"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderVoicelinesVolume"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderAnnouncerVolume() const
{
	if (not IsValid(M_SliderAnnouncerVolume))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderAnnouncerVolume"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderAnnouncerVolume"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderTransmissionsAndCinematicsVolume() const
{
	if (not IsValid(M_SliderTransmissionsAndCinematicsVolume))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderTransmissionsAndCinematicsVolume"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderTransmissionsAndCinematicsVolume"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderUiVolume() const
{
	if (not IsValid(M_SliderUiVolume))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderUiVolume"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderUiVolume"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextControlsHeader() const
{
	if (not IsValid(M_TextControlsHeader))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextControlsHeader"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextControlsHeader"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextMouseSensitivityLabel() const
{
	if (not IsValid(M_TextMouseSensitivityLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextMouseSensitivityLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextMouseSensitivityLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextCameraMovementSpeedMultiplierLabel() const
{
	if (not IsValid(M_TextCameraMovementSpeedMultiplierLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextCameraMovementSpeedMultiplierLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextCameraMovementSpeedMultiplierLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextCameraPanSpeedMultiplierLabel() const
{
	if (not IsValid(M_TextCameraPanSpeedMultiplierLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextCameraPanSpeedMultiplierLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextCameraPanSpeedMultiplierLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextInvertYAxisLabel() const
{
	if (not IsValid(M_TextInvertYAxisLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextInvertYAxisLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextInvertYAxisLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextGameplayHeader() const
{
	if (not IsValid(M_TextGameplayHeader))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextGameplayHeader"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextGameplayHeader"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextHideActionButtonHotkeysLabel() const
{
	if (not IsValid(M_TextHideActionButtonHotkeysLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextHideActionButtonHotkeysLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextHideActionButtonHotkeysLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextOverwriteAllPlayerHpBarLabel() const
{
	if (not IsValid(M_TextOverwriteAllPlayerHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextOverwriteAllPlayerHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextOverwriteAllPlayerHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextPlayerTankHpBarLabel() const
{
	if (not IsValid(M_TextPlayerTankHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextPlayerTankHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextPlayerTankHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextPlayerSquadHpBarLabel() const
{
	if (not IsValid(M_TextPlayerSquadHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextPlayerSquadHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextPlayerSquadHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextPlayerNomadicHpBarLabel() const
{
	if (not IsValid(M_TextPlayerNomadicHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextPlayerNomadicHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextPlayerNomadicHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextPlayerBxpHpBarLabel() const
{
	if (not IsValid(M_TextPlayerBxpHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextPlayerBxpHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextPlayerBxpHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextPlayerAircraftHpBarLabel() const
{
	if (not IsValid(M_TextPlayerAircraftHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextPlayerAircraftHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextPlayerAircraftHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextOverwriteAllEnemyHpBarLabel() const
{
	if (not IsValid(M_TextOverwriteAllEnemyHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextOverwriteAllEnemyHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextOverwriteAllEnemyHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextEnemyTankHpBarLabel() const
{
	if (not IsValid(M_TextEnemyTankHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextEnemyTankHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextEnemyTankHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextEnemySquadHpBarLabel() const
{
	if (not IsValid(M_TextEnemySquadHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextEnemySquadHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextEnemySquadHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextEnemyNomadicHpBarLabel() const
{
	if (not IsValid(M_TextEnemyNomadicHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextEnemyNomadicHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextEnemyNomadicHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextEnemyBxpHpBarLabel() const
{
	if (not IsValid(M_TextEnemyBxpHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextEnemyBxpHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextEnemyBxpHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextEnemyAircraftHpBarLabel() const
{
	if (not IsValid(M_TextEnemyAircraftHpBarLabel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextEnemyAircraftHpBarLabel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextEnemyAircraftHpBarLabel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderMouseSensitivity() const
{
	if (not IsValid(M_SliderMouseSensitivity))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderMouseSensitivity"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderMouseSensitivity"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderCameraMovementSpeedMultiplier() const
{
	if (not IsValid(M_SliderCameraMovementSpeedMultiplier))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderCameraMovementSpeedMultiplier"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderCameraMovementSpeedMultiplier"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidSliderCameraPanSpeedMultiplier() const
{
	if (not IsValid(M_SliderCameraPanSpeedMultiplier))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_SliderCameraPanSpeedMultiplier"),
			TEXT("UW_EscapeMenuSettings::GetIsValidSliderCameraPanSpeedMultiplier"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidCheckInvertYAxis() const
{
	if (not IsValid(M_CheckInvertYAxis))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_CheckInvertYAxis"),
			TEXT("UW_EscapeMenuSettings::GetIsValidCheckInvertYAxis"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidCheckHideActionButtonHotkeys() const
{
	if (not IsValid(M_CheckHideActionButtonHotkeys))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_CheckHideActionButtonHotkeys"),
			TEXT("UW_EscapeMenuSettings::GetIsValidCheckHideActionButtonHotkeys"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboOverwriteAllPlayerHpBarStrat() const
{
	if (not IsValid(M_ComboOverwriteAllPlayerHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboOverwriteAllPlayerHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboOverwriteAllPlayerHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboPlayerTankHpBarStrat() const
{
	if (not IsValid(M_ComboPlayerTankHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboPlayerTankHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboPlayerTankHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboPlayerSquadHpBarStrat() const
{
	if (not IsValid(M_ComboPlayerSquadHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboPlayerSquadHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboPlayerSquadHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboPlayerNomadicHpBarStrat() const
{
	if (not IsValid(M_ComboPlayerNomadicHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboPlayerNomadicHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboPlayerNomadicHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboPlayerBxpHpBarStrat() const
{
	if (not IsValid(M_ComboPlayerBxpHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboPlayerBxpHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboPlayerBxpHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboPlayerAircraftHpBarStrat() const
{
	if (not IsValid(M_ComboPlayerAircraftHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboPlayerAircraftHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboPlayerAircraftHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboOverwriteAllEnemyHpBarStrat() const
{
	if (not IsValid(M_ComboOverwriteAllEnemyHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboOverwriteAllEnemyHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboOverwriteAllEnemyHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboEnemyTankHpBarStrat() const
{
	if (not IsValid(M_ComboEnemyTankHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboEnemyTankHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboEnemyTankHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboEnemySquadHpBarStrat() const
{
	if (not IsValid(M_ComboEnemySquadHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboEnemySquadHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboEnemySquadHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboEnemyNomadicHpBarStrat() const
{
	if (not IsValid(M_ComboEnemyNomadicHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboEnemyNomadicHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboEnemyNomadicHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboEnemyBxpHpBarStrat() const
{
	if (not IsValid(M_ComboEnemyBxpHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboEnemyBxpHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboEnemyBxpHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidComboEnemyAircraftHpBarStrat() const
{
	if (not IsValid(M_ComboEnemyAircraftHpBarStrat))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ComboEnemyAircraftHpBarStrat"),
			TEXT("UW_EscapeMenuSettings::GetIsValidComboEnemyAircraftHpBarStrat"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidButtonApply() const
{
	if (not IsValid(M_ButtonApply))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ButtonApply"),
			TEXT("UW_EscapeMenuSettings::GetIsValidButtonApply"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidButtonBackOrCancel() const
{
	if (not IsValid(M_ButtonBackOrCancel))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ButtonBackOrCancel"),
			TEXT("UW_EscapeMenuSettings::GetIsValidButtonBackOrCancel"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidButtonSetDefaults() const
{
	if (not IsValid(M_ButtonSetDefaults))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ButtonSetDefaults"),
			TEXT("UW_EscapeMenuSettings::GetIsValidButtonSetDefaults"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextApplyButton() const
{
	if (not IsValid(M_TextApplyButton))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextApplyButton"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextApplyButton"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextSetDefaultsButton() const
{
	if (not IsValid(M_TextSetDefaultsButton))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextSetDefaultsButton"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextSetDefaultsButton"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetIsValidTextBackOrCancelButton() const
{
	if (not IsValid(M_TextBackOrCancelButton))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_TextBackOrCancelButton"),
			TEXT("UW_EscapeMenuSettings::GetIsValidTextBackOrCancelButton"),
			this
		);
		return false;
	}

	return true;
}

bool UW_EscapeMenuSettings::GetAreRootWidgetsValid() const
{
	if (not GetIsValidRootOverlay())
	{
		return false;
	}

	if (not GetIsValidBackgroundBlurFullscreen())
	{
		return false;
	}

	if (not GetIsValidSizeBoxMenuRoot())
	{
		return false;
	}

	if (not GetIsValidVerticalBoxMenuRoot())
	{
		return false;
	}

	if (not GetIsValidVerticalBoxSections())
	{
		return false;
	}

	return GetIsValidHorizontalBoxFooterButtons();
}

bool UW_EscapeMenuSettings::GetAreSettingsNavigationWidgetsValid() const
{
	if (not GetIsValidSettingsSwitch())
	{
		return false;
	}

	if (not GetIsValidButtonGameplaySettings())
	{
		return false;
	}

	if (not GetIsValidButtonGraphicsSettings())
	{
		return false;
	}

	if (not GetIsValidButtonAudioSettings())
	{
		return false;
	}

	return GetIsValidButtonControlSettings();
}

bool UW_EscapeMenuSettings::GetAreGraphicsWidgetsValid() const
{
	if (not GetIsValidVerticalBoxGraphicsSection())
	{
		return false;
	}

	if (not GetAreGraphicsTextWidgetsValid())
	{
		return false;
	}

	return GetAreGraphicsControlWidgetsValid();
}

bool UW_EscapeMenuSettings::GetAreAudioWidgetsValid() const
{
	if (not GetIsValidVerticalBoxAudioSection())
	{
		return false;
	}

	if (not GetAreAudioTextWidgetsValid())
	{
		return false;
	}

	return GetAreAudioSliderWidgetsValid();
}

bool UW_EscapeMenuSettings::GetAreControlsWidgetsValid() const
{
	if (not GetIsValidVerticalBoxControlsSection())
	{
		return false;
	}

	if (not GetAreControlsTextWidgetsValid())
	{
		return false;
	}

	return GetAreControlSettingsWidgetsValid();
}

bool UW_EscapeMenuSettings::GetAreGameplayWidgetsValid() const
{
	if (not GetIsValidVerticalBoxGameplaySection())
	{
		return false;
	}

	if (not GetAreGameplayTextWidgetsValid())
	{
		return false;
	}

	if (not GetIsValidCheckHideActionButtonHotkeys())
	{
		return false;
	}

	return GetAreGameplayComboWidgetsValid();
}

bool UW_EscapeMenuSettings::GetAreFooterWidgetsValid() const
{
	if (not GetIsValidHorizontalBoxFooterButtons())
	{
		return false;
	}

	if (not GetIsValidButtonApply())
	{
		return false;
	}

	if (not GetIsValidButtonSetDefaults())
	{
		return false;
	}

	if (not GetIsValidButtonBackOrCancel())
	{
		return false;
	}

	if (not GetIsValidTextApplyButton())
	{
		return false;
	}

	if (not GetIsValidTextSetDefaultsButton())
	{
		return false;
	}

	return GetIsValidTextBackOrCancelButton();
}

bool UW_EscapeMenuSettings::GetAreAllWidgetsValid() const
{
	if (not GetAreRootWidgetsValid())
	{
		return false;
	}

	if (not GetAreSettingsNavigationWidgetsValid())
	{
		return false;
	}

	if (not GetAreGraphicsWidgetsValid())
	{
		return false;
	}

	if (not GetAreAudioWidgetsValid())
	{
		return false;
	}

	if (not GetAreControlsWidgetsValid())
	{
		return false;
	}

	if (not GetAreGameplayWidgetsValid())
	{
		return false;
	}

	return GetAreFooterWidgetsValid();
}

bool UW_EscapeMenuSettings::GetAreGraphicsTextWidgetsValid() const
{
	if (not GetIsValidTextGraphicsHeader())
	{
		return false;
	}

	if (not GetIsValidTextWindowModeLabel())
	{
		return false;
	}

	if (not GetIsValidTextResolutionLabel())
	{
		return false;
	}

	if (not GetIsValidTextVSyncLabel())
	{
		return false;
	}

	if (not GetIsValidTextOverallQualityLabel())
	{
		return false;
	}

	if (not GetIsValidTextViewDistanceLabel())
	{
		return false;
	}

	if (not GetIsValidTextShadowsLabel())
	{
		return false;
	}

	if (not GetIsValidTextTexturesLabel())
	{
		return false;
	}

	if (not GetIsValidTextEffectsLabel())
	{
		return false;
	}

	if (not GetIsValidTextPostProcessingLabel())
	{
		return false;
	}

	return GetIsValidTextFrameRateLimitLabel();
}

bool UW_EscapeMenuSettings::GetAreGraphicsControlWidgetsValid() const
{
	if (not GetIsValidComboWindowMode())
	{
		return false;
	}

	if (not GetIsValidComboResolution())
	{
		return false;
	}

	if (not GetIsValidCheckVSync())
	{
		return false;
	}

	if (not GetIsValidComboOverallQuality())
	{
		return false;
	}

	if (not GetIsValidComboViewDistanceQuality())
	{
		return false;
	}

	if (not GetIsValidComboShadowsQuality())
	{
		return false;
	}

	if (not GetIsValidComboTexturesQuality())
	{
		return false;
	}

	if (not GetIsValidComboEffectsQuality())
	{
		return false;
	}

	if (not GetIsValidComboPostProcessingQuality())
	{
		return false;
	}

	return GetIsValidSliderFrameRateLimit();
}

bool UW_EscapeMenuSettings::GetAreAudioTextWidgetsValid() const
{
	if (not GetIsValidTextAudioHeader())
	{
		return false;
	}

	if (not GetIsValidTextMasterVolumeLabel())
	{
		return false;
	}

	if (not GetIsValidTextMusicVolumeLabel())
	{
		return false;
	}

	if (not GetIsValidTextSfxVolumeLabel())
	{
		return false;
	}

	if (not GetIsValidTextVoicelinesVolumeLabel())
	{
		return false;
	}

	if (not GetIsValidTextAnnouncerVolumeLabel())
	{
		return false;
	}

	if (not GetIsValidTextTransmissionsAndCinematicsVolumeLabel())
	{
		return false;
	}

	return GetIsValidTextUiVolumeLabel();
}

bool UW_EscapeMenuSettings::GetAreControlsTextWidgetsValid() const
{
	if (not GetIsValidTextControlsHeader())
	{
		return false;
	}

	if (not GetIsValidTextMouseSensitivityLabel())
	{
		return false;
	}

	if (not GetIsValidTextCameraMovementSpeedMultiplierLabel())
	{
		return false;
	}

	if (not GetIsValidTextCameraPanSpeedMultiplierLabel())
	{
		return false;
	}

	return GetIsValidTextInvertYAxisLabel();
}

bool UW_EscapeMenuSettings::GetAreGameplayTextWidgetsValid() const
{
	if (not GetIsValidTextGameplayHeader())
	{
		return false;
	}

	if (not GetIsValidTextHideActionButtonHotkeysLabel())
	{
		return false;
	}

	if (not GetIsValidTextOverwriteAllPlayerHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextPlayerTankHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextPlayerSquadHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextPlayerNomadicHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextPlayerBxpHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextPlayerAircraftHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextOverwriteAllEnemyHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextEnemyTankHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextEnemySquadHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextEnemyNomadicHpBarLabel())
	{
		return false;
	}

	if (not GetIsValidTextEnemyBxpHpBarLabel())
	{
		return false;
	}

	return GetIsValidTextEnemyAircraftHpBarLabel();
}

bool UW_EscapeMenuSettings::GetAreFooterTextWidgetsValid() const
{
	if (not GetIsValidTextApplyButton())
	{
		return false;
	}

	if (not GetIsValidTextSetDefaultsButton())
	{
		return false;
	}

	return GetIsValidTextBackOrCancelButton();
}

bool UW_EscapeMenuSettings::GetAreGraphicsDisplayWidgetsValid() const
{
	if (not GetIsValidComboWindowMode())
	{
		return false;
	}

	if (not GetIsValidComboResolution())
	{
		return false;
	}

	if (not GetIsValidCheckVSync())
	{
		return false;
	}

	return GetIsValidComboOverallQuality();
}

bool UW_EscapeMenuSettings::GetAreGraphicsScalabilityWidgetsValid() const
{
	if (not GetIsValidComboViewDistanceQuality())
	{
		return false;
	}

	if (not GetIsValidComboShadowsQuality())
	{
		return false;
	}

	if (not GetIsValidComboTexturesQuality())
	{
		return false;
	}

	if (not GetIsValidComboEffectsQuality())
	{
		return false;
	}

	return GetIsValidComboPostProcessingQuality();
}

bool UW_EscapeMenuSettings::GetAreGraphicsFrameRateWidgetsValid() const
{
	return GetIsValidSliderFrameRateLimit();
}

bool UW_EscapeMenuSettings::GetAreAudioSliderWidgetsValid() const
{
	if (not GetIsValidSliderMasterVolume())
	{
		return false;
	}

	if (not GetIsValidSliderMusicVolume())
	{
		return false;
	}

	if (not GetIsValidSliderSfxVolume())
	{
		return false;
	}

	if (not GetIsValidSliderVoicelinesVolume())
	{
		return false;
	}

	if (not GetIsValidSliderAnnouncerVolume())
	{
		return false;
	}

	if (not GetIsValidSliderTransmissionsAndCinematicsVolume())
	{
		return false;
	}

	return GetIsValidSliderUiVolume();
}

bool UW_EscapeMenuSettings::GetAreControlSettingsWidgetsValid() const
{
	if (not GetIsValidSliderMouseSensitivity())
	{
		return false;
	}

	if (not GetIsValidSliderCameraMovementSpeedMultiplier())
	{
		return false;
	}

	if (not GetIsValidSliderCameraPanSpeedMultiplier())
	{
		return false;
	}

	return GetIsValidCheckInvertYAxis();
}

bool UW_EscapeMenuSettings::GetAreGameplayComboWidgetsValid() const
{
	if (not GetIsValidComboOverwriteAllPlayerHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboPlayerTankHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboPlayerSquadHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboPlayerNomadicHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboPlayerBxpHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboPlayerAircraftHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboOverwriteAllEnemyHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboEnemyTankHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboEnemySquadHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboEnemyNomadicHpBarStrat())
	{
		return false;
	}

	if (not GetIsValidComboEnemyBxpHpBarStrat())
	{
		return false;
	}

	return GetIsValidComboEnemyAircraftHpBarStrat();
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

	if (not GetIsValidMainGameUI())
	{
		return;
	}

	M_MainGameUI->OnEscapeMenuCloseSettings();
}

void UW_EscapeMenuSettings::HandleSetDefaultsClicked()
{
	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingSettingsToDefaults();
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

void UW_EscapeMenuSettings::HandleGameplaySettingsClicked()
{
	ApplySettingsSectionSelection(EscapeMenuSettingsOptionDefaults::SettingsSectionGameplayIndex);
}

void UW_EscapeMenuSettings::HandleGraphicsSettingsClicked()
{
	ApplySettingsSectionSelection(EscapeMenuSettingsOptionDefaults::SettingsSectionGraphicsIndex);
}

void UW_EscapeMenuSettings::HandleAudioSettingsClicked()
{
	ApplySettingsSectionSelection(EscapeMenuSettingsOptionDefaults::SettingsSectionAudioIndex);
}

void UW_EscapeMenuSettings::HandleControlSettingsClicked()
{
	ApplySettingsSectionSelection(EscapeMenuSettingsOptionDefaults::SettingsSectionControlsIndex);
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

	if (not GetIsValidComboWindowMode())
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

	if (not GetIsValidComboResolution())
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

	if (not GetIsValidCheckVSync())
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

	if (not GetIsValidComboOverallQuality())
	{
		return;
	}

	if (not GetAreGraphicsScalabilityWidgetsValid())
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

	if (not GetIsValidComboViewDistanceQuality())
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

	if (not GetIsValidComboShadowsQuality())
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

	if (not GetIsValidComboTexturesQuality())
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

	if (not GetIsValidComboEffectsQuality())
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

	if (not GetIsValidComboPostProcessingQuality())
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

	if (not GetIsValidSliderFrameRateLimit())
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

	if (not GetIsValidSliderMasterVolume())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingMasterVolume(NewValue);
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

	if (not GetIsValidSliderMusicVolume())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioType::Music, NewValue);
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

	if (not GetIsValidSliderSfxVolume())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioType::SFXAndWeapons, NewValue);
}

void UW_EscapeMenuSettings::HandleVoicelinesVolumeChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidSliderVoicelinesVolume())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioType::Voicelines, NewValue);
}

void UW_EscapeMenuSettings::HandleAnnouncerVolumeChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidSliderAnnouncerVolume())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioType::Announcer, NewValue);
}

void UW_EscapeMenuSettings::HandleTransmissionsAndCinematicsVolumeChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidSliderTransmissionsAndCinematicsVolume())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioType::TransmissionsAndCinematics, NewValue);
}

void UW_EscapeMenuSettings::HandleUiVolumeChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidSliderUiVolume())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingAudioVolume(ERTSAudioType::UI, NewValue);
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

	if (not GetIsValidSliderMouseSensitivity())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingMouseSensitivity(NewValue);
}

void UW_EscapeMenuSettings::HandleCameraMovementSpeedMultiplierChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidSliderCameraMovementSpeedMultiplier())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingCameraMovementSpeedMultiplier(NewValue);

	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_PlayerController->SetCameraMovementSpeedMultiplier(NewValue);
}

void UW_EscapeMenuSettings::HandleCameraPanSpeedMultiplierChanged(const float NewValue)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidSliderCameraPanSpeedMultiplier())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingCameraPanSpeedMultiplier(NewValue);

	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_PlayerController->SetCameraPanSpeedMultiplier(NewValue);
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

	if (not GetIsValidCheckInvertYAxis())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingInvertYAxis(bIsChecked);
}

void UW_EscapeMenuSettings::HandleHideActionButtonHotkeysChanged(const bool bIsChecked)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidCheckHideActionButtonHotkeys())
	{
		return;
	}

	M_SettingsSubsystem->SetPendingHideActionButtonHotkeys(bIsChecked);
}

void UW_EscapeMenuSettings::HandleOverwriteAllPlayerHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboOverwriteAllPlayerHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboOverwriteAllPlayerHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player health bar overwrite selection was not found in the combo box options."));
		return;
	}

	const ERTSPlayerHealthBarVisibilityStrategy Strategy = static_cast<ERTSPlayerHealthBarVisibilityStrategy>(SelectedIndex);
	M_SettingsSubsystem->SetPendingOverwriteAllPlayerHpBarStrat(Strategy);
	ApplyOverwriteAllPlayerHealthBarStrategy(Strategy);
}

void UW_EscapeMenuSettings::HandlePlayerTankHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboPlayerTankHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboPlayerTankHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player tank health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingPlayerTankHpBarStrat(static_cast<ERTSPlayerHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandlePlayerSquadHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboPlayerSquadHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboPlayerSquadHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player squad health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingPlayerSquadHpBarStrat(static_cast<ERTSPlayerHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandlePlayerNomadicHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboPlayerNomadicHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboPlayerNomadicHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player nomadic health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingPlayerNomadicHpBarStrat(static_cast<ERTSPlayerHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandlePlayerBxpHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboPlayerBxpHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboPlayerBxpHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player bxp health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingPlayerBxpHpBarStrat(static_cast<ERTSPlayerHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandlePlayerAircraftHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboPlayerAircraftHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboPlayerAircraftHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player aircraft health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingPlayerAircraftHpBarStrat(static_cast<ERTSPlayerHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleOverwriteAllEnemyHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboOverwriteAllEnemyHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboOverwriteAllEnemyHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy health bar overwrite selection was not found in the combo box options."));
		return;
	}

	const ERTSEnemyHealthBarVisibilityStrategy Strategy = static_cast<ERTSEnemyHealthBarVisibilityStrategy>(SelectedIndex);
	M_SettingsSubsystem->SetPendingOverwriteAllEnemyHpBarStrat(Strategy);
	ApplyOverwriteAllEnemyHealthBarStrategy(Strategy);
}

void UW_EscapeMenuSettings::HandleEnemyTankHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboEnemyTankHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboEnemyTankHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy tank health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingEnemyTankHpBarStrat(static_cast<ERTSEnemyHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleEnemySquadHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboEnemySquadHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboEnemySquadHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy squad health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingEnemySquadHpBarStrat(static_cast<ERTSEnemyHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleEnemyNomadicHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboEnemyNomadicHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboEnemyNomadicHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy nomadic health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingEnemyNomadicHpBarStrat(static_cast<ERTSEnemyHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleEnemyBxpHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboEnemyBxpHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboEnemyBxpHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy bxp health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingEnemyBxpHpBarStrat(static_cast<ERTSEnemyHealthBarVisibilityStrategy>(SelectedIndex));
}

void UW_EscapeMenuSettings::HandleEnemyAircraftHpBarStratChanged(FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bM_IsInitialisingControls)
	{
		return;
	}

	if (not GetIsValidSettingsSubsystem())
	{
		return;
	}

	if (not GetIsValidComboEnemyAircraftHpBarStrat())
	{
		return;
	}

	const int32 SelectedIndex = M_ComboEnemyAircraftHpBarStrat->FindOptionIndex(SelectedItem);
	if (SelectedIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy aircraft health bar selection was not found in the combo box options."));
		return;
	}

	M_SettingsSubsystem->SetPendingEnemyAircraftHpBarStrat(static_cast<ERTSEnemyHealthBarVisibilityStrategy>(SelectedIndex));
}
