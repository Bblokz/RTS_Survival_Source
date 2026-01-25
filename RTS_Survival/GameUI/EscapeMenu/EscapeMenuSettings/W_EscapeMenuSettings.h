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
	constexpr float FrameRateLimitMin = 35.f;
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
 * @brief Fullscreen escape settings menu that binds Blueprint-authored widgets to settings runtime logic.
 *
 * Designers own the widget tree in Blueprint while C++ populates options, binds callbacks, and stages settings changes.
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
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

private:
	void CacheSettingsSubsystem();
	void RefreshControlsFromSubsystem();

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
	void BindGraphicsSettingCallbacks();
	void BindAudioSettingCallbacks();
	void BindControlSettingCallbacks();

	bool GetIsValidPlayerController() const;
	bool GetIsValidMainGameUI() const;
	bool GetIsValidSettingsSubsystem() const;

	bool GetIsValidRootOverlay() const;
	bool GetIsValidBackgroundBlurFullscreen() const;
	bool GetIsValidSizeBoxMenuRoot() const;
	bool GetIsValidVerticalBoxMenuRoot() const;
	bool GetIsValidVerticalBoxSections() const;
	bool GetIsValidHorizontalBoxFooterButtons() const;

	bool GetIsValidVerticalBoxGraphicsSection() const;
	bool GetIsValidVerticalBoxAudioSection() const;
	bool GetIsValidVerticalBoxControlsSection() const;

	bool GetIsValidTextGraphicsHeader() const;
	bool GetIsValidTextWindowModeLabel() const;
	bool GetIsValidTextResolutionLabel() const;
	bool GetIsValidTextVSyncLabel() const;
	bool GetIsValidTextOverallQualityLabel() const;
	bool GetIsValidTextViewDistanceLabel() const;
	bool GetIsValidTextShadowsLabel() const;
	bool GetIsValidTextTexturesLabel() const;
	bool GetIsValidTextEffectsLabel() const;
	bool GetIsValidTextPostProcessingLabel() const;
	bool GetIsValidTextFrameRateLimitLabel() const;

	bool GetIsValidComboWindowMode() const;
	bool GetIsValidComboResolution() const;
	bool GetIsValidCheckVSync() const;
	bool GetIsValidComboOverallQuality() const;
	bool GetIsValidComboViewDistanceQuality() const;
	bool GetIsValidComboShadowsQuality() const;
	bool GetIsValidComboTexturesQuality() const;
	bool GetIsValidComboEffectsQuality() const;
	bool GetIsValidComboPostProcessingQuality() const;
	bool GetIsValidSliderFrameRateLimit() const;

	bool GetIsValidTextAudioHeader() const;
	bool GetIsValidTextMasterVolumeLabel() const;
	bool GetIsValidTextMusicVolumeLabel() const;
	bool GetIsValidTextSfxVolumeLabel() const;
	bool GetIsValidSliderMasterVolume() const;
	bool GetIsValidSliderMusicVolume() const;
	bool GetIsValidSliderSfxVolume() const;

	bool GetIsValidTextControlsHeader() const;
	bool GetIsValidTextMouseSensitivityLabel() const;
	bool GetIsValidTextInvertYAxisLabel() const;
	bool GetIsValidSliderMouseSensitivity() const;
	bool GetIsValidCheckInvertYAxis() const;

	bool GetIsValidButtonApply() const;
	bool GetIsValidButtonBackOrCancel() const;
	bool GetIsValidTextApplyButton() const;
	bool GetIsValidTextBackOrCancelButton() const;

	bool GetAreRootWidgetsValid() const;
	bool GetAreGraphicsWidgetsValid() const;
	bool GetAreAudioWidgetsValid() const;
	bool GetAreControlsWidgetsValid() const;
	bool GetAreFooterWidgetsValid() const;
	bool GetAreAllWidgetsValid() const;

	bool GetAreGraphicsTextWidgetsValid() const;
	bool GetAreGraphicsControlWidgetsValid() const;
	bool GetAreAudioTextWidgetsValid() const;
	bool GetAreControlsTextWidgetsValid() const;
	bool GetAreFooterTextWidgetsValid() const;
	bool GetAreGraphicsDisplayWidgetsValid() const;
	bool GetAreGraphicsScalabilityWidgetsValid() const;
	bool GetAreGraphicsFrameRateWidgetsValid() const;
	bool GetAreAudioSliderWidgetsValid() const;
	bool GetAreControlSettingsWidgetsValid() const;

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

	/**
	 * @brief BindWidget variable must reference an Overlay named M_RootOverlay in the Widget Blueprint.
	 * @note Designers may change layout, styling, padding, and add wrapper widgets around this overlay.
	 * @note The bound widget must exist, keep this name, and remain an Overlay for C++ validation.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UOverlay> M_RootOverlay = nullptr;

	/**
	 * @brief BindWidget variable must reference a BackgroundBlur named M_BackgroundBlurFullscreen in the Widget Blueprint.
	 * @note Designers may restyle the blur and adjust layout wrappers to fit the menu presentation.
	 * @note The bound widget must exist, keep this name, and remain a BackgroundBlur for runtime logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UBackgroundBlur> M_BackgroundBlurFullscreen = nullptr;

	/**
	 * @brief BindWidget variable must reference a SizeBox named M_SizeBoxMenuRoot in the Widget Blueprint.
	 * @note Designers may adjust sizing, padding, and wrappers to fit the screen or platform.
	 * @note The bound widget must exist, keep this name, and remain a SizeBox for validation.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<USizeBox> M_SizeBoxMenuRoot = nullptr;

	/**
	 * @brief BindWidget variable must reference a VerticalBox named M_VerticalBoxMenuRoot in the Widget Blueprint.
	 * @note Designers may reorder children and styling within the vertical box as long as it remains present.
	 * @note The bound widget must exist, keep this name, and remain a VerticalBox for validation.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UVerticalBox> M_VerticalBoxMenuRoot = nullptr;

	/**
	 * @brief BindWidget variable must reference a VerticalBox named M_VerticalBoxSections in the Widget Blueprint.
	 * @note Designers may style section spacing, padding, and wrappers around this container.
	 * @note The bound widget must exist, keep this name, and remain a VerticalBox for validation.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UVerticalBox> M_VerticalBoxSections = nullptr;

	/**
	 * @brief BindWidget variable must reference a HorizontalBox named M_HorizontalBoxFooterButtons in the Widget Blueprint.
	 * @note Designers may style spacing or add wrappers, but the horizontal box must remain the bound widget.
	 * @note The bound widget must exist, keep this name, and remain a HorizontalBox for C++ validation.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UHorizontalBox> M_HorizontalBoxFooterButtons = nullptr;

	/**
	 * @brief BindWidget variable must reference a VerticalBox named M_VerticalBoxGraphicsSection in the Widget Blueprint.
	 * @note Designers may style or wrap the section, but the bound vertical box must remain present.
	 * @note The bound widget must exist, keep this name, and remain a VerticalBox for validation.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UVerticalBox> M_VerticalBoxGraphicsSection = nullptr;

	/**
	 * @brief BindWidget variable must reference a VerticalBox named M_VerticalBoxAudioSection in the Widget Blueprint.
	 * @note Designers may adjust spacing or layout within the section as long as this vertical box remains bound.
	 * @note The bound widget must exist, keep this name, and remain a VerticalBox for validation.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UVerticalBox> M_VerticalBoxAudioSection = nullptr;

	/**
	 * @brief BindWidget variable must reference a VerticalBox named M_VerticalBoxControlsSection in the Widget Blueprint.
	 * @note Designers may style the section and add wrappers, but this vertical box must remain bound.
	 * @note The bound widget must exist, keep this name, and remain a VerticalBox for validation.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UVerticalBox> M_VerticalBoxControlsSection = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextGraphicsHeader in the Widget Blueprint.
	 * @note Designers may style the text, but C++ owns the runtime text value for localization.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextGraphicsHeader = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextWindowModeLabel in the Widget Blueprint.
	 * @note Designers may style the label or add wrappers, but C++ sets the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextWindowModeLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextResolutionLabel in the Widget Blueprint.
	 * @note Designers may style the label or adjust layout, but C++ sets the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextResolutionLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextVSyncLabel in the Widget Blueprint.
	 * @note Designers may style the label, but C++ sets the label text at runtime for localization.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextVSyncLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextOverallQualityLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextOverallQualityLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextViewDistanceLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextViewDistanceLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextShadowsLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextShadowsLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextTexturesLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextTexturesLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextEffectsLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextEffectsLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextPostProcessingLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextPostProcessingLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextFrameRateLimitLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextFrameRateLimitLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a ComboBoxString named M_ComboWindowMode in the Widget Blueprint.
	 * @note Designers may style and wrap the combo box, but C++ populates options and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a ComboBoxString for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UComboBoxString> M_ComboWindowMode = nullptr;

	/**
	 * @brief BindWidget variable must reference a ComboBoxString named M_ComboResolution in the Widget Blueprint.
	 * @note Designers may style and wrap the combo box, but C++ populates options and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a ComboBoxString for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UComboBoxString> M_ComboResolution = nullptr;

	/**
	 * @brief BindWidget variable must reference a CheckBox named M_CheckVSync in the Widget Blueprint.
	 * @note Designers may style and wrap the checkbox, but C++ binds callbacks and sets the checked state.
	 * @note The bound widget must exist, keep this name, and remain a CheckBox for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UCheckBox> M_CheckVSync = nullptr;

	/**
	 * @brief BindWidget variable must reference a ComboBoxString named M_ComboOverallQuality in the Widget Blueprint.
	 * @note Designers may style and wrap the combo box, but C++ populates options and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a ComboBoxString for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UComboBoxString> M_ComboOverallQuality = nullptr;

	/**
	 * @brief BindWidget variable must reference a ComboBoxString named M_ComboViewDistanceQuality in the Widget Blueprint.
	 * @note Designers may style and wrap the combo box, but C++ populates options and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a ComboBoxString for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UComboBoxString> M_ComboViewDistanceQuality = nullptr;

	/**
	 * @brief BindWidget variable must reference a ComboBoxString named M_ComboShadowsQuality in the Widget Blueprint.
	 * @note Designers may style and wrap the combo box, but C++ populates options and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a ComboBoxString for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UComboBoxString> M_ComboShadowsQuality = nullptr;

	/**
	 * @brief BindWidget variable must reference a ComboBoxString named M_ComboTexturesQuality in the Widget Blueprint.
	 * @note Designers may style and wrap the combo box, but C++ populates options and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a ComboBoxString for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UComboBoxString> M_ComboTexturesQuality = nullptr;

	/**
	 * @brief BindWidget variable must reference a ComboBoxString named M_ComboEffectsQuality in the Widget Blueprint.
	 * @note Designers may style and wrap the combo box, but C++ populates options and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a ComboBoxString for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UComboBoxString> M_ComboEffectsQuality = nullptr;

	/**
	 * @brief BindWidget variable must reference a ComboBoxString named M_ComboPostProcessingQuality in the Widget Blueprint.
	 * @note Designers may style and wrap the combo box, but C++ populates options and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a ComboBoxString for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UComboBoxString> M_ComboPostProcessingQuality = nullptr;

	/**
	 * @brief BindWidget variable must reference a Slider named M_SliderFrameRateLimit in the Widget Blueprint.
	 * @note Designers may style and wrap the slider, but C++ applies ranges and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a Slider for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<USlider> M_SliderFrameRateLimit = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextAudioHeader in the Widget Blueprint.
	 * @note Designers may style the text, but C++ owns the runtime text value for localization.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextAudioHeader = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextMasterVolumeLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextMasterVolumeLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextMusicVolumeLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextMusicVolumeLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextSfxVolumeLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextSfxVolumeLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a Slider named M_SliderMasterVolume in the Widget Blueprint.
	 * @note Designers may style and wrap the slider, but C++ applies ranges and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a Slider for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<USlider> M_SliderMasterVolume = nullptr;

	/**
	 * @brief BindWidget variable must reference a Slider named M_SliderMusicVolume in the Widget Blueprint.
	 * @note Designers may style and wrap the slider, but C++ applies ranges and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a Slider for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<USlider> M_SliderMusicVolume = nullptr;

	/**
	 * @brief BindWidget variable must reference a Slider named M_SliderSfxVolume in the Widget Blueprint.
	 * @note Designers may style and wrap the slider, but C++ applies ranges and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a Slider for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<USlider> M_SliderSfxVolume = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextControlsHeader in the Widget Blueprint.
	 * @note Designers may style the text, but C++ owns the runtime text value for localization.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextControlsHeader = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextMouseSensitivityLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextMouseSensitivityLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextInvertYAxisLabel in the Widget Blueprint.
	 * @note Designers may style or wrap the label, but C++ updates the label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextInvertYAxisLabel = nullptr;

	/**
	 * @brief BindWidget variable must reference a Slider named M_SliderMouseSensitivity in the Widget Blueprint.
	 * @note Designers may style and wrap the slider, but C++ applies ranges and binds callbacks.
	 * @note The bound widget must exist, keep this name, and remain a Slider for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<USlider> M_SliderMouseSensitivity = nullptr;

	/**
	 * @brief BindWidget variable must reference a CheckBox named M_CheckInvertYAxis in the Widget Blueprint.
	 * @note Designers may style and wrap the checkbox, but C++ binds callbacks and sets the checked state.
	 * @note The bound widget must exist, keep this name, and remain a CheckBox for logic.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UCheckBox> M_CheckInvertYAxis = nullptr;

	/**
	 * @brief BindWidget variable must reference a Button named M_ButtonApply in the Widget Blueprint.
	 * @note Designers may style and wrap the button, but it must contain a child RichTextBlock named M_TextApplyButton.
	 * @note The bound button and its label must exist, keep names, and remain the same widget types.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UButton> M_ButtonApply = nullptr;

	/**
	 * @brief BindWidget variable must reference a Button named M_ButtonBackOrCancel in the Widget Blueprint.
	 * @note Designers may style and wrap the button, but it must contain a child RichTextBlock named M_TextBackOrCancelButton.
	 * @note The bound button and its label must exist, keep names, and remain the same widget types.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<UButton> M_ButtonBackOrCancel = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextApplyButton in the Widget Blueprint.
	 * @note Designers may style the label, but C++ sets the button label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextApplyButton = nullptr;

	/**
	 * @brief BindWidget variable must reference a RichTextBlock named M_TextBackOrCancelButton in the Widget Blueprint.
	 * @note Designers may style the label, but C++ sets the button label text at runtime.
	 * @note The bound widget must exist, keep this name, and remain a RichTextBlock for updates.
	 */
	UPROPERTY(VisibleAnywhere, Transient, meta=(BindWidget, AllowPrivateAccess="true"), Category="Settings|Widgets")
	TObjectPtr<URichTextBlock> M_TextBackOrCancelButton = nullptr;

	UPROPERTY(Transient)
	TArray<FIntPoint> M_SupportedResolutions;

	UPROPERTY(Transient)
	TArray<FString> M_SupportedResolutionLabels;

	UPROPERTY(Transient)
	bool bM_IsInitialisingControls = false;
};
