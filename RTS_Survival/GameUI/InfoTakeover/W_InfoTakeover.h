// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InfoTakeoverStyle.h"
#include "InfoTakeoverTypes.h"
#include "RTS_Survival/GameUI/IndicatorLamps/PanelSignalLamp.h"
#include "RTS_Survival/GameUI/TutorialOvertakeWidget/W_TutorialOvertake.h"

#include "W_InfoTakeover.generated.h"

class UAudioComponent;
class UBorder;
class UButton;
class UCheckBox;
class UHorizontalBox;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPauseSafeProtoRichTextBlock;
class UTextBlock;
class UVerticalBox;
class UW_InfoTakeover;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInfoTakeoverClosed, UW_InfoTakeover* /*ClosedTakeover*/);

/** Which typewritten text is printing; sounds follow it. */
UENUM()
enum class EInfoTakeoverTypingPhase : uint8
{
	NotStarted,
	Title,
	Description,
	Done
};

/** The sound that follows the typewriter and how many real seconds of it are left. */
USTRUCT()
struct FInfoTakeoverTypingSoundState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAudioComponent> ActiveSound = nullptr;

	UPROPERTY()
	float SecondsLeft = 0.f;

	// The ongoing loop has already been chained at least once.
	UPROPERTY()
	bool bIsOngoing = false;
};

/**
 * One glass element of a takeover (a body pane, the gauge glass over a picture, a footer button) with the main UI's
 * glass hover: its own dynamic materials, so only the element under the mouse lights up.
 */
USTRUCT()
struct FInfoTakeoverGlassElement
{
	GENERATED_BODY()

	// For the validation log, e.g. Pane_0, Picture_1, Button_Confirm.
	UPROPERTY()
	FName Name;

	// The widget whose hover lights the glass.
	UPROPERTY()
	TWeakObjectPtr<UWidget> HoverSource;

	// Picture glass inside a body pane: hovering anywhere on the pane (the card) lights the picture glass too.
	UPROPERTY()
	TWeakObjectPtr<UWidget> ParentHoverSource;

	// Buttons only: pressing darkens the glass.
	UPROPERTY()
	TWeakObjectPtr<UButton> PressSource;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	FGlassHoverAnimator Animator;

	// Tests and captures hover the element without a mouse.
	bool bIsForcedHover = false;
};

/**
 * @brief Base of the info takeovers (unit intel and game-mechanic field manuals): a paused, full-screen panel whose
 * title and description type in with the mission text sounds, with Understood and, on unit intel only (see
 * GetOffersNeverShowAgain), "never show these info widgets again". UInfoTakeoverSubsystem creates, queues and opens
 * them; subclasses fill the body.
 * @note Fill M_Style in the Blueprint child's class defaults (UISkins: the variant selection skin).
 */
UCLASS(Abstract)
class RTS_SURVIVAL_API UW_InfoTakeover : public UW_TutorialOvertake
{
	GENERATED_BODY()

public:
	FOnInfoTakeoverClosed OnTakeoverClosed;

	/** Pauses the game, takes the input and starts typing the title; the subclass has filled its content before. */
	void OpenTakeover();

	virtual void OnClickedContinue() override;

	EInfoTakeoverTypingPhase GetTypingPhase() const { return M_TypingPhase; }

	// For tests and tools: the same as the player ticking the box; does nothing on takeovers without the box.
	void SetNeverShowAgainChecked(const bool bIsChecked) const;

	/**
	 * @return True for unit intel (vehicles, squads), which shows "never show these info widgets again". Game-mechanic
	 * field manuals keep the default false: no box, and closing them never touches the setting.
	 */
	virtual bool GetOffersNeverShowAgain() const { return false; }

	// For tests and tools: the same as the player clicking the footer button (next page, or close on the last page).
	void PressConfirmButton();

	/** @return The typing sound playing now, empty when none; for the headless validation log. */
	FString GetActiveTypingSoundName() const;

	/** @return How many pages the takeover has; one-page takeovers keep the default 1. */
	virtual int32 GetPageCount() const { return 1; }

	/** @return The page that shows now, 0 based. */
	virtual int32 GetCurrentPageIndex() const { return 0; }

	// For tests and tools: the same as the player clicking PREVIOUS (does nothing on the first page).
	void PressPreviousButton();

	bool GetIsPreviousButtonVisible() const;

	int32 GetPageLampCount() const { return M_PageLamps.Num(); }

	/** @return The index of the one lit page lamp; INDEX_NONE when none or more than one is lit. */
	int32 GetLitPageLampIndex() const;

	/** @return True when the page lamps draw the signal lamp material with halos (not the flat fallback brushes). */
	bool GetHasPageLampHalos() const;

	/**
	 * @brief For tests: where the page lamp row and the lit lamp's halo sit, in units of the panel plate.
	 * @param OutRowRect Lamp row: left, top, width, height from the plate's top left.
	 * @param OutLitLampCentre Centre of the lit lamp in the same units; (-1, -1) when none is lit.
	 * @return False before the first layout pass.
	 */
	bool GetPageLampLayout(FBox2D& OutRowRect, FVector2D& OutLitLampCentre) const;

	/** @return 0 dark .. 1 fully lit; -1 for an invalid index. */
	float GetPageLampPower(const int32 LampIndex) const;

	int32 GetGlassElementCount() const { return M_GlassElements.Num(); }

	/** @return The element's name for logs, NAME_None for an invalid index. */
	FName GetGlassElementName(const int32 ElementIndex) const;

	/** @return The Hover value the element's first glass material has now; -1 for an invalid index. */
	float GetGlassElementHover(const int32 ElementIndex) const;

	/**
	 * @brief Tests and captures hover a glass element without a mouse; it ramps like a real hover.
	 * @param ElementIndex Index into the takeover's glass elements.
	 * @param bIsHovered Whether the element counts as hovered.
	 * @param FrozenSweepProgress Holds the light band at this progress (0..1); negative lets it run.
	 */
	void SetGlassElementForcedHover(const int32 ElementIndex, const bool bIsHovered,
	                                const float FrozenSweepProgress = -1.f);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Sets the small label over the title, e.g. "VEHICLE INTEL".
	void SetKindLabel(const FText& KindLabel);
	// Collapses the label over the title, so the title sits at the top of the panel.
	void HideKindLabel();
	void SetTitleText(const FText& Title);
	void SetDescriptionText(const FText& Description);

	/**
	 * @brief Multi-page takeovers show a page here (switcher, texts, button label, illustrations, typing); CONTINUE
	 * and PREVIOUS call it and then update the page lamps and PREVIOUS. The game stays paused between pages.
	 * @param PageIndex The page to show, 0 based and within GetPageCount.
	 * @return True when the page was shown.
	 */
	virtual bool TryShowPage(const int32 PageIndex) { return false; }

	/** Updates the page lamps and PREVIOUS to GetCurrentPageIndex; call after a page change outside TryShowPage. */
	void RefreshPageControls();

	/**
	 * @brief Gives a body pane the glass hover: its own dynamic material (quieter than on buttons, the pane carries
	 * text), and the picture glass inside it lights up with it. Call after the pane's content is set.
	 * @param GlassPane The border drawn with M_Style.Skin.VariantSectionGlass.
	 */
	void RegisterGlassPane(UBorder& GlassPane);

	// The footer button's text, e.g. "CONTINUE" while more pages follow; UNDERSTOOD by default.
	void SetConfirmButtonLabel(const FText& Label) const;

	/** Types the title and description again, for a page whose texts were just set. */
	void RestartTyping();

	/** @return The column the subclass builds its body in, between the title and the footer; fills the height. */
	UVerticalBox* GetBodyColumn() const { return M_BodyColumn; }

	/**
	 * @brief Every takeover picture sits behind the same gauge glass (M_Style.PictureGlass), so subclasses put their
	 * picture in through here instead of placing it directly.
	 * @param Picture The picture widget (image, scale box or illustration) to cover.
	 * @return An overlay of the picture with the glass on top, or the picture itself when the style has no glass.
	 */
	UWidget* MakePictureWithGlass(UWidget* Picture);

	/** @return The typewritten description; the subclass places it in its body. */
	UPauseSafeProtoRichTextBlock* GetDescriptionBlock() const { return M_DescriptionText; }

	UPROPERTY(EditDefaultsOnly, Category = "Info Takeover")
	FInfoTakeoverStyle M_Style;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> M_KindLabelText;

	UPROPERTY()
	TObjectPtr<UPauseSafeProtoRichTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UPauseSafeProtoRichTextBlock> M_DescriptionText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_BodyColumn;

	UPROPERTY()
	TObjectPtr<UCheckBox> M_NeverShowAgainCheckBox;

	UPROPERTY()
	TObjectPtr<UButton> M_UnderstoodButton;

	// PREVIOUS left of the footer button; collapsed on the first page.
	UPROPERTY()
	TObjectPtr<UButton> M_PreviousButton;

	// The sized box around PREVIOUS that is shown and collapsed.
	UPROPERTY()
	TObjectPtr<UWidget> M_PreviousButtonBox;

	// The panel plate; tests measure the page lamps against it.
	UPROPERTY()
	TObjectPtr<UBorder> M_Plate;

	// Top right of the title placard; one lamp per page.
	UPROPERTY()
	TObjectPtr<UHorizontalBox> M_PageLampRow;

	// Same layout as M_PageLampRow, drawn under it: each lamp's halo. Every halo lies under every lamp body, so the
	// light on the plate never depends on which lamp draws first.
	UPROPERTY()
	TObjectPtr<UHorizontalBox> M_PageLampHaloRow;

	UPROPERTY()
	TArray<FPanelSignalLamp> M_PageLamps;

	// Loaded once from UIndicatorLampSettings::PageLampMaterial; null keeps the style's flat lamp brushes.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> M_PageLampMaterial;

	UPROPERTY()
	TArray<FInfoTakeoverGlassElement> M_GlassElements;

	UPROPERTY()
	EInfoTakeoverTypingPhase M_TypingPhase = EInfoTakeoverTypingPhase::NotStarted;

	UPROPERTY()
	FInfoTakeoverTypingSoundState M_TypingSound;

	// Loaded once from M_Style so the typewriter never stalls on a load.
	UPROPERTY()
	TObjectPtr<USoundBase> M_TitleTypingSound;

	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> M_DescriptionStartSounds;

	UPROPERTY()
	TObjectPtr<USoundBase> M_DescriptionOngoingSound;

	bool bM_IsClosing = false;

	void BuildWidgetTree();
	UWidget* BuildHeader();
	UWidget* BuildFooter();
	UPauseSafeProtoRichTextBlock* BuildTypewriterBlock(UDataTable* TextStyleSet);
	void LoadSounds();

	void StartTitleTyping();
	void StartDescriptionTyping();
	void FinishTyping();

	UFUNCTION()
	void HandleTitlePrintFinished(bool bStoppedEarly);

	UFUNCTION()
	void HandleDescriptionPrintFinished(bool bStoppedEarly);

	UFUNCTION()
	void HandleUnderstoodClicked();

	UFUNCTION()
	void HandlePreviousClicked();

	/** @return True when the next page showed, so the footer button must not close the takeover. */
	bool TryShowNextPage();
	bool TryShowPreviousPage();
	UWidget* BuildPageLampRow();
	void RebuildPageLamps(const int32 LampCount);
	void AddPageLamp(const int32 LampIndex);
	void TickPageLamps(const float DeltaSeconds);

	/** Gives a footer button the glass hover on its own dynamic instances of its n / h / p materials. */
	void RegisterGlassButton(UButton& Button, const FName ElementName);

	/** @return A dynamic instance of the brush's material put back into the brush; null for a non-material brush. */
	UMaterialInstanceDynamic* MakeDynamicGlassBrush(FSlateBrush& Brush);
	void LinkPicturesToPane(UBorder& GlassPane);
	void TickGlassHover(const float DeltaSeconds);
	static bool GetIsWidgetInside(const UWidget* Widget, const UWidget* Ancestor);

	void TickTypingSound(const float RealDeltaSeconds);
	void PlayTypingSound(USoundBase* Sound, const bool bIsOngoing);
	void StopTypingSound();

	/** @return Seconds per letter that types Text in TargetSeconds, never slower than MaxInterval. */
	static float GetLetterIntervalForDuration(const FText& Text, const float TargetSeconds, const float MaxInterval);

	void ApplyNeverShowAgainChoice() const;

	bool GetIsValidPlate() const;
	bool GetIsValidPageLampRows() const;
	bool GetIsValidTitleText() const;
	bool GetIsValidDescriptionText() const;
	bool GetIsValidNeverShowAgainCheckBox() const;
};
