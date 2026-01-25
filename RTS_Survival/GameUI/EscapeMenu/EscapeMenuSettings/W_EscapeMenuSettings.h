#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "Types/SlateEnums.h"

#include "W_EscapeMenuSettings.generated.h"

class ACPPController;
class UBackgroundBlur;
class UButton;
class UCheckBox;
class UComboBoxString;
class UHorizontalBox;
class UMainGameUI;
class UOverlay;
class URichTextBlock;
class USizeBox;
class USlider;
class UVerticalBox;
class UWidget;

/**
 * @brief Holds designer-facing text keys for the graphics settings section.
 *
 * Populate these with localization keys so the menu can replace labels without editing layout code.
 */
USTRUCT(BlueprintType)
struct FEscapeMenuSettingsGraphicsText
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_HeaderText = FText::FromString(TEXT("SETTINGS_GRAPHICS_HEADER"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_WindowModeLabelText = FText::FromString(TEXT("SETTINGS_WINDOW_MODE_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_ResolutionLabelText = FText::FromString(TEXT("SETTINGS_RESOLUTION_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_VSyncLabelText = FText::FromString(TEXT("SETTINGS_VSYNC_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_OverallQualityLabelText = FText::FromString(TEXT("SETTINGS_OVERALL_QUALITY_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_ViewDistanceLabelText = FText::FromString(TEXT("SETTINGS_VIEW_DISTANCE_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_ShadowsLabelText = FText::FromString(TEXT("SETTINGS_SHADOWS_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_TexturesLabelText = FText::FromString(TEXT("SETTINGS_TEXTURES_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_EffectsLabelText = FText::FromString(TEXT("SETTINGS_EFFECTS_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_PostProcessingLabelText = FText::FromString(TEXT("SETTINGS_POST_PROCESSING_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_FrameRateLimitLabelText = FText::FromString(TEXT("SETTINGS_FRAME_RATE_LIMIT_LABEL"));
};

/**
 * @brief Holds designer-facing text keys for the audio settings section.
 *
 * Update these values in Blueprint to localize or rename audio labels without touching C++ logic.
 */
USTRUCT(BlueprintType)
struct FEscapeMenuSettingsAudioText
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_HeaderText = FText::FromString(TEXT("SETTINGS_AUDIO_HEADER"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_MasterVolumeLabelText = FText::FromString(TEXT("SETTINGS_MASTER_VOLUME_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_MusicVolumeLabelText = FText::FromString(TEXT("SETTINGS_MUSIC_VOLUME_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_SfxVolumeLabelText = FText::FromString(TEXT("SETTINGS_SFX_VOLUME_LABEL"));
};

/**
 * @brief Holds designer-facing text keys for the controls settings section.
 *
 * Keeping these in a struct makes it easy to swap label wording for different control schemes.
 */
USTRUCT(BlueprintType)
struct FEscapeMenuSettingsControlsText
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_HeaderText = FText::FromString(TEXT("SETTINGS_CONTROLS_HEADER"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_MouseSensitivityLabelText = FText::FromString(TEXT("SETTINGS_MOUSE_SENSITIVITY_LABEL"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_InvertYAxisLabelText = FText::FromString(TEXT("SETTINGS_INVERT_Y_AXIS_LABEL"));
};

/**
 * @brief Holds the apply/back button text so designers can relabel the footer actions.
 */
USTRUCT(BlueprintType)
struct FEscapeMenuSettingsButtonText
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_ApplyButtonText = FText::FromString(TEXT("SETTINGS_APPLY_BUTTON"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Text")
	FText M_BackOrCancelButtonText = FText::FromString(TEXT("SETTINGS_BACK_OR_CANCEL_BUTTON"));
};

namespace EscapeMenuSettingsDefaults
{
	constexpr float FrameRateLimitMin = 0.0f;
	constexpr float FrameRateLimitMax = 240.0f;
}

/**
 * @brief Defines the slider ranges so designers can tune min/max limits without code edits.
 */
USTRUCT(BlueprintType)
struct FEscapeMenuSettingsSliderRanges
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Sliders")
	float M_FrameRateLimitMin = EscapeMenuSettingsDefaults::FrameRateLimitMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Sliders")
	float M_FrameRateLimitMax = EscapeMenuSettingsDefaults::FrameRateLimitMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Sliders")
	float M_MouseSensitivityMin = RTSGameUserSettingsRanges::MinMouseSensitivity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Sliders")
	float M_MouseSensitivityMax = RTSGameUserSettingsRanges::MaxMouseSensitivity;
};

/**
 * @brief Fullscreen escape settings menu that stages changes through URTSSettingsMenuSubsystem before applying or reverting.
 *
 * The widget builds a neutral layout in C++ so designers can focus on styling in Blueprint without changing the settings logic.
 */
UCLASS()
class RTS_SURVIVAL_API UW_EscapeMenuSettings : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Provides the owning controller so the menu can drive gameplay state changes.
	 * @param NewPlayerController Player controller that owns the settings menu.
	 */
	void SetPlayerController(ACPPController* NewPlayerController);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	void CacheSettingsSubsystem();
	void RefreshControlsFromSubsystem();

	void InitWidgetTree();
	void InitRootLayout();
	void InitGraphicsSection(UVerticalBox* SectionsContainer);
	void InitGraphicsDisplaySettings(UVerticalBox* GraphicsSection);
	void InitGraphicsScalabilitySettings(UVerticalBox* GraphicsSection);
	void InitGraphicsFrameRateSetting(UVerticalBox* GraphicsSection);
	void InitAudioSection(UVerticalBox* SectionsContainer);
	void InitControlsSection(UVerticalBox* SectionsContainer);
	void InitFooterButtons();

	/**
	 * @brief Builds a section wrapper with a header to keep the menu grouped without relying on styling data.
	 * @param ParentContainer The vertical box that owns the section.
	 * @param SectionName Name assigned to the section widget for Blueprint lookup.
	 * @param OutSectionHeader Rich text header created for the section.
	 * @param HeaderName Name assigned to the header widget for Blueprint lookup.
	 * @param HeaderText Placeholder header text that designers can replace.
	 * @return The created section container or nullptr when creation fails.
	 */
	UVerticalBox* CreateSectionContainer(UVerticalBox* ParentContainer, const FName SectionName, TObjectPtr<URichTextBlock>& OutSectionHeader, const FName HeaderName, const FText& HeaderText);

	/**
	 * @brief Creates a neutral label/control row so each setting can be bound without depending on final styling.
	 * @param ParentContainer The section that receives the row.
	 * @param RowName Name assigned to the row widget for Blueprint lookup.
	 * @param OutLabel Rich text label created for the row.
	 * @param LabelName Name assigned to the label widget for Blueprint lookup.
	 * @param LabelText Placeholder label text that designers can replace.
	 * @param ControlWidget The input widget that represents the setting value.
	 * @return The created row container or nullptr when creation fails.
	 */
	UHorizontalBox* CreateSettingRow(UVerticalBox* ParentContainer, const FName RowName, TObjectPtr<URichTextBlock>& OutLabel, const FName LabelName, const FText& LabelText, UWidget* ControlWidget);
	URichTextBlock* CreateRichTextBlock(const FName WidgetName, const FText& TextToAssign);
	UComboBoxString* CreateComboBox(const FName WidgetName);
	USlider* CreateSlider(const FName WidgetName);
	UCheckBox* CreateCheckBox(const FName WidgetName);

	/**
	 * @brief Creates a button with rich text content so the apply/cancel labels stay fully designer-editable.
	 * @param WidgetName Name assigned to the button widget for Blueprint lookup.
	 * @param OutButtonText Rich text block created for the button content.
	 * @param ButtonTextName Name assigned to the button text widget for Blueprint lookup.
	 * @param ButtonText Placeholder button text that designers can replace.
	 * @return The created button or nullptr when creation fails.
	 */
	UButton* CreateButton(const FName WidgetName, TObjectPtr<URichTextBlock>& OutButtonText, const FName ButtonTextName, const FText& ButtonText);

	void CacheWidgetReferencesFromTree();
	void CacheRootWidgetReferences();
	void CacheGraphicsWidgetReferences();
	void CacheAudioWidgetReferences();
	void CacheControlsWidgetReferences();
	void CacheFooterWidgetReferences();

	bool EnsureWidgetReferencesValid(const FString& FunctionName) const;
	bool EnsureRootWidgetReferencesValid(const FString& FunctionName) const;
	bool EnsureGraphicsWidgetReferencesValid(const FString& FunctionName) const;
	bool EnsureAudioWidgetReferencesValid(const FString& FunctionName) const;
	bool EnsureControlsWidgetReferencesValid(const FString& FunctionName) const;
	bool EnsureFooterWidgetReferencesValid(const FString& FunctionName) const;

	void PopulateComboBoxOptions();
	void PopulateWindowModeOptions();
	void PopulateResolutionOptions();
	void PopulateQualityOptions();
	void PopulateQualityOptionsForComboBox(UComboBoxString* ComboBoxToPopulate) const;

	void EnsureWindowModeOptionTexts();
	void EnsureQualityOptionTexts();
	void UpdateAllRichTextBlocks();

	void InitialiseControlsFromSnapshot(const FRTSSettingsSnapshot& SettingsSnapshot);
	void InitialiseGraphicsControls(const FRTSGraphicsSettings& GraphicsSettings);
	void InitialiseGraphicsDisplayControls(const FRTSDisplaySettings& DisplaySettings);
	void InitialiseGraphicsScalabilityControls(const FRTSScalabilityGroupSettings& ScalabilityGroups);
	void InitialiseGraphicsFrameRateControl(const FRTSDisplaySettings& DisplaySettings);
	void InitialiseAudioControls(const FRTSAudioSettings& AudioSettings);
	void InitialiseControlSettingsControls(const FRTSControlSettings& ControlSettings);

	/**
	 * @brief Applies slider min/max bounds so designer ranges stay enforced at runtime.
	 * @param SliderToConfigure Slider widget that receives the bounds.
	 * @param MinValue Minimum value to expose to the player.
	 * @param MaxValue Maximum value to expose to the player.
	 */
	void SetSliderRange(USlider* SliderToConfigure, float MinValue, float MaxValue) const;

	/**
	 * @brief Selects a combo option that matches the provided quality enum.
	 * @param ComboBoxToSelect Combo box that hosts the quality options.
	 * @param QualityToSelect Quality enum to map into the combo selection.
	 */
	void SetQualityComboSelection(UComboBoxString* ComboBoxToSelect, ERTSScalabilityQuality QualityToSelect) const;

	/**
	 * @brief Clamps the requested index so UI queries never access an out-of-range option.
	 * @param DesiredIndex Index the caller wants to use.
	 * @param OptionCount Total available options in the list.
	 * @return A safe index that is guaranteed to be within the option array.
	 */
	int32 GetClampedOptionIndex(int32 DesiredIndex, int32 OptionCount) const;

	/**
	 * @brief Formats a resolution label for combo box display.
	 * @param Resolution Resolution pair used to build the label.
	 * @return Text label for the resolution, e.g. 1920x1080.
	 */
	FString BuildResolutionLabel(const FIntPoint& Resolution) const;

	/**
	 * @brief Finds a resolution in the cached list so the UI can sync the current selection.
	 * @param Resolution Resolution value to search for.
	 * @return Index into the supported resolutions array, or INDEX_NONE when missing.
	 */
	int32 FindResolutionIndex(const FIntPoint& Resolution) const;

	void BindButtonCallbacks();
	void BindApplyButton();
	void BindBackOrCancelButton();
	void BindSettingCallbacks();

	bool GetIsValidMainGameUI() const;
	bool GetIsValidSettingsSubsystem() const;

	/**
	 * @brief Validates a widget pointer to protect against missing Blueprint bindings.
	 * @param WidgetToCheck Widget pointer to validate.
	 * @param WidgetName Name to include in error output for designers.
	 * @param FunctionName Function name to include in error output.
	 * @return True when the widget pointer is valid.
	 */
	bool EnsureWidgetPointerIsValid(const UWidget* WidgetToCheck, const FString& WidgetName, const FString& FunctionName) const;

	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleBackOrCancelClicked();

	UFUNCTION()
	void HandleWindowModeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleResolutionSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleVSyncChanged(bool bIsChecked);

	UFUNCTION()
	void HandleOverallQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleViewDistanceQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleShadowsQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleTexturesQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleEffectsQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandlePostProcessingQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleFrameRateLimitChanged(float NewValue);

	UFUNCTION()
	void HandleMasterVolumeChanged(float NewValue);

	UFUNCTION()
	void HandleMusicVolumeChanged(float NewValue);

	UFUNCTION()
	void HandleSfxVolumeChanged(float NewValue);

	UFUNCTION()
	void HandleMouseSensitivityChanged(float NewValue);

	UFUNCTION()
	void HandleInvertYAxisChanged(bool bIsChecked);

	UPROPERTY(Transient)
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	UPROPERTY(Transient)
	TWeakObjectPtr<URTSSettingsMenuSubsystem> M_SettingsSubsystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings|Text", meta=(AllowPrivateAccess="true"))
	FEscapeMenuSettingsGraphicsText M_GraphicsText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings|Text", meta=(AllowPrivateAccess="true"))
	FEscapeMenuSettingsAudioText M_AudioText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings|Text", meta=(AllowPrivateAccess="true"))
	FEscapeMenuSettingsControlsText M_ControlsText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings|Text", meta=(AllowPrivateAccess="true"))
	FEscapeMenuSettingsButtonText M_ButtonText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings|Sliders", meta=(AllowPrivateAccess="true"))
	FEscapeMenuSettingsSliderRanges M_SliderRanges;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings|Text", meta=(AllowPrivateAccess="true"))
	TArray<FText> M_WindowModeOptionTexts =
	{
		FText::FromString(TEXT("SETTINGS_WINDOW_MODE_FULLSCREEN_OPTION")),
		FText::FromString(TEXT("SETTINGS_WINDOW_MODE_WINDOWED_FULLSCREEN_OPTION")),
		FText::FromString(TEXT("SETTINGS_WINDOW_MODE_WINDOWED_OPTION"))
	};

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings|Text", meta=(AllowPrivateAccess="true"))
	TArray<FText> M_QualityOptionTexts =
	{
		FText::FromString(TEXT("SETTINGS_QUALITY_LOW_OPTION")),
		FText::FromString(TEXT("SETTINGS_QUALITY_MEDIUM_OPTION")),
		FText::FromString(TEXT("SETTINGS_QUALITY_HIGH_OPTION")),
		FText::FromString(TEXT("SETTINGS_QUALITY_EPIC_OPTION"))
	};

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UOverlay> M_RootOverlay = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBackgroundBlur> M_BackgroundBlurFullscreen = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USizeBox> M_SizeBoxMenuRoot = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UVerticalBox> M_VerticalBoxMenuRoot = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UVerticalBox> M_VerticalBoxSections = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UHorizontalBox> M_HorizontalBoxFooterButtons = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UVerticalBox> M_VerticalBoxGraphicsSection = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UVerticalBox> M_VerticalBoxAudioSection = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UVerticalBox> M_VerticalBoxControlsSection = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextGraphicsHeader = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextWindowModeLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextResolutionLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextVSyncLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextOverallQualityLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextViewDistanceLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextShadowsLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextTexturesLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextEffectsLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextPostProcessingLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextFrameRateLimitLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> M_ComboWindowMode = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> M_ComboResolution = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCheckBox> M_CheckVSync = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> M_ComboOverallQuality = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> M_ComboViewDistanceQuality = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> M_ComboShadowsQuality = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> M_ComboTexturesQuality = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> M_ComboEffectsQuality = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UComboBoxString> M_ComboPostProcessingQuality = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USlider> M_SliderFrameRateLimit = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextAudioHeader = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextMasterVolumeLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextMusicVolumeLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextSfxVolumeLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USlider> M_SliderMasterVolume = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USlider> M_SliderMusicVolume = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USlider> M_SliderSfxVolume = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextControlsHeader = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextMouseSensitivityLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextInvertYAxisLabel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USlider> M_SliderMouseSensitivity = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCheckBox> M_CheckInvertYAxis = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UButton> M_ButtonApply = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UButton> M_ButtonBackOrCancel = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextApplyButton = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Settings|Widgets", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URichTextBlock> M_TextBackOrCancelButton = nullptr;

	UPROPERTY(Transient)
	TArray<FIntPoint> M_SupportedResolutions;

	UPROPERTY(Transient)
	TArray<FString> M_SupportedResolutionLabels;

	UPROPERTY(Transient)
	bool bM_IsInitialisingControls = false;
};
