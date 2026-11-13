// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_InfoTakeover.h"

#include "W_CameraControlsTakeover.generated.h"

class UUserWidget;
class UVerticalBox;
class UWidget;
class UWidgetSwitcher;

/** One card of the camera controls takeover: the looping picture and the words under it. */
USTRUCT()
struct FCameraControlsTakeoverSection
{
	GENERATED_BODY()

	// Small accent label over the title, e.g. "KEYBOARD".
	UPROPERTY()
	FText Label;

	UPROPERTY()
	FText Title;

	UPROPERTY()
	FText Body;

	UPROPERTY()
	TSoftClassPtr<UUserWidget> IllustrationClass;
};

/** The pages of the camera controls takeover, in the order CONTINUE shows them. */
UENUM()
enum class ECameraControlsTakeoverPage : uint8
{
	// Arrow keys, edge panning and zoom.
	CameraControls,
	// Turning and tilting the view with the middle mouse button (IA_PanCamera), and the reset (IA_PanReset).
	CameraPanning,
	// Drag-box selection with the left mouse button, and Shift.
	MarqueeSelection,
	// The number of pages; not a page.
	Count UMETA(Hidden)
};

/**
 * @brief The field manual on moving the camera and selecting units, in pages like the tutorial overtakes: camera
 * controls (arrow keys, edge panning, zoom) and camera panning end in CONTINUE, marquee selection in UNDERSTOOD. Each
 * picture is a looping illustration widget (plain UserWidgets whose UMG animations all loop; widget animations tick on
 * real time, so they run while the takeover pauses the game). Opened by UInfoTakeoverSubsystem::QueueCameraControlsTakeover.
 * @note Fill M_Style in the Blueprint child's class defaults (WBP_CameraControlsTakeover copies the vehicle takeover).
 */
UCLASS()
class RTS_SURVIVAL_API UW_CameraControlsTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	UW_CameraControlsTakeover(const FObjectInitializer& ObjectInitializer);

	/** Sets the texts and builds every page; call before OpenTakeover. */
	void SetupCameraControls();

	static FText GetCameraControlsTitle();

	/** @return Every control of every page in one plain text, for the archive entry. */
	static FText GetCameraControlsSummary();

	ECameraControlsTakeoverPage GetActivePage() const { return M_ActivePage; }

	virtual int32 GetPageCount() const override { return static_cast<int32>(ECameraControlsTakeoverPage::Count); }
	virtual int32 GetCurrentPageIndex() const override { return static_cast<int32>(M_ActivePage); }

	/** @return The illustrations of one page, for the headless validation. */
	TArray<UUserWidget*> GetPageIllustrations(const ECameraControlsTakeoverPage Page) const;

protected:
	virtual bool TryShowPage(const int32 PageIndex) override;

	// Looping picture of the arrow keys moving a small map view.
	UPROPERTY(EditDefaultsOnly, Category = "Camera Controls")
	TSoftClassPtr<UUserWidget> M_KeyboardPanIllustration;

	// Looping picture of the cursor pushing against the screen edge while the map scrolls.
	UPROPERTY(EditDefaultsOnly, Category = "Camera Controls")
	TSoftClassPtr<UUserWidget> M_EdgePanIllustration;

	// Looping picture of the mouse wheel rolling while the map view zooms.
	UPROPERTY(EditDefaultsOnly, Category = "Camera Controls")
	TSoftClassPtr<UUserWidget> M_ZoomIllustration;

	// Looping picture of the middle mouse button held while the map turns and tilts, then the reset (camera panning page).
	UPROPERTY(EditDefaultsOnly, Category = "Camera Controls")
	TSoftClassPtr<UUserWidget> M_PanningIllustration;

	// Looping picture of a drag box growing over units that light up as selected (marquee page).
	UPROPERTY(EditDefaultsOnly, Category = "Camera Controls")
	TSoftClassPtr<UUserWidget> M_MarqueeIllustration;

private:
	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> M_CameraPageIllustrations;

	UPROPERTY()
	TObjectPtr<UUserWidget> M_PanningPageIllustration;

	UPROPERTY()
	TObjectPtr<UUserWidget> M_MarqueePageIllustration;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> M_PageSwitcher;

	UPROPERTY()
	ECameraControlsTakeoverPage M_ActivePage = ECameraControlsTakeoverPage::CameraControls;

	TArray<FCameraControlsTakeoverSection> MakeCameraSections() const;
	TArray<FCameraControlsTakeoverSection> MakePanningSections() const;
	TArray<FCameraControlsTakeoverSection> MakeMarqueeSections() const;
	void BuildBody();
	UWidget* BuildCameraPage();

	/**
	 * @brief A page with one large illustration on the left and its cards' texts beside it (panning, marquee).
	 * @param IllustrationClass The illustration Blueprint, designed at the side-page size.
	 * @param Sections The texts beside the picture, top to bottom.
	 * @param OutIllustration The created illustration; null when its class could not be loaded.
	 * @return The page in its glass pane.
	 */
	UWidget* BuildSideIllustrationPage(const TSoftClassPtr<UUserWidget>& IllustrationClass,
	                                   const TArray<FCameraControlsTakeoverSection>& Sections,
	                                   TObjectPtr<UUserWidget>& OutIllustration);

	UUserWidget* GetSidePageIllustration(const ECameraControlsTakeoverPage Page) const;
	UWidget* BuildSectionCard(const FCameraControlsTakeoverSection& Section);
	UWidget* BuildGlassPane(UWidget* Content);

	/**
	 * @brief The framed, clipped well an illustration widget loops in.
	 * @param IllustrationClass The illustration Blueprint.
	 * @param IllustrationSize The size the Blueprint was designed at.
	 * @param OutIllustration The created widget; null when the class could not be loaded.
	 * @return The well, also when it stays empty.
	 */
	UWidget* BuildIllustrationWell(const TSoftClassPtr<UUserWidget>& IllustrationClass, const FVector2D& IllustrationSize,
	                               UUserWidget*& OutIllustration);

	void AddCardText(UVerticalBox& CardColumn, const FCameraControlsTakeoverSection& Section,
	                 const float WrapWidth) const;
	void ApplyPageTexts(const ECameraControlsTakeoverPage Page);

	/** Plays every UMG animation of the illustration on an endless loop, from the start. */
	static void PlayAllAnimationsLooped(UUserWidget& Illustration);
};
