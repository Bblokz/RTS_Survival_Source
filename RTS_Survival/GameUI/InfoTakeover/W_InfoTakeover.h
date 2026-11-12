// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "InfoTakeoverStyle.h"
#include "InfoTakeoverTypes.h"
#include "RTS_Survival/GameUI/TutorialOvertakeWidget/W_TutorialOvertake.h"

#include "W_InfoTakeover.generated.h"

class UAudioComponent;
class UButton;
class UCheckBox;
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
 * @brief Base of the info takeovers (vehicles now, buildings and squads later): a paused, full-screen panel whose
 * title and description type in with the mission text sounds, with "never show these info widgets again" and
 * Understood. UInfoTakeoverSubsystem creates, queues and opens them; subclasses fill the body.
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

	// For tests and tools: the same as the player ticking the box.
	void SetNeverShowAgainChecked(const bool bIsChecked) const;

	// For tests and tools: the same as the player clicking the footer button (next page, or close on the last page).
	void PressConfirmButton();

	/** @return The typing sound playing now, empty when none; for the headless validation log. */
	FString GetActiveTypingSoundName() const;

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
	 * @brief Multi-page takeovers (like the tutorial overtakes' Continue button) show their next page here; the game
	 * stays paused and locked between pages.
	 * @return True when another page was shown, so the button press must not close the takeover.
	 */
	virtual bool TryShowNextPage() { return false; }

	// The footer button's text, e.g. "CONTINUE" while more pages follow; UNDERSTOOD by default.
	void SetConfirmButtonLabel(const FText& Label) const;

	/** Types the title and description again, for a page whose texts were just set. */
	void RestartTyping();

	/** @return The column the subclass builds its body in, between the title and the footer; fills the height. */
	UVerticalBox* GetBodyColumn() const { return M_BodyColumn; }

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

	void TickTypingSound(const float RealDeltaSeconds);
	void PlayTypingSound(USoundBase* Sound, const bool bIsOngoing);
	void StopTypingSound();

	/** @return Seconds per letter that types Text in TargetSeconds, never slower than MaxInterval. */
	static float GetLetterIntervalForDuration(const FText& Text, const float TargetSeconds, const float MaxInterval);

	void ApplyNeverShowAgainChoice() const;

	bool GetIsValidTitleText() const;
	bool GetIsValidDescriptionText() const;
	bool GetIsValidNeverShowAgainCheckBox() const;
};
