#pragma once
#include "ProtoRichTextBlock.h"
#include "Blueprint/UserWidget.h"
#include "MissionWidgetState/MissionWidgetState.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Game/GameState/HideGameUI/RTSUIElement.h" // <- add
#include "RTS_Survival/Missions/MissionWidgets/ObjectivePlate/MissionObjectivePlateWidgets.h"

#include "W_Mission.generated.h"

class UMissionBase;
class UMissionObjectivePlateSettings;
class URichTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
enum class EMissionWidgetState : uint8;

/**
 * @brief Mission widget instances are pooled and reconfigured by missions through init/refresh calls.
 * With UMissionObjectivePlateSettings::bUseObjectivePlates the widget replaces W_BP_Mission's designer layout
 * at initialisation with a C++ built objective plate (major / bonus); the Blueprint graph keeps driving the
 * title, description, typewriter and sounds. See Source/Docs/UI/MissionObjectivePlates.md.
 */
UCLASS()
class RTS_SURVIVAL_API UW_Mission : public UUserWidget, public IRTSUIElement 
{
	GENERATED_BODY()

public:
	void InitMissionWidget(const FMissionWidgetState& WidgetState,
	                       UMissionBase* AssociatedMission,
	                       const bool bUseNextAsExpanded,
	                       const bool bStartAsCollapsedWidget);

	void RefreshMissionWidget(const FMissionWidgetState& WidgetState);
	void EnsureMissionShowsTitleOnly();

	void MarkWidgetAsFree();

	inline bool GetIsFreeWidget() const { return not bM_IsInUse; };

	/** The title and description last shown; the tactical map lists in-use missions with them. */
	const FMissionWidgetState& GetShownWidgetState() const { return M_ShownWidgetState; }

	/** The text blocks whose styles the tactical map's mission cards copy, so rich text tags look the same. */
	URichTextBlock* GetRichTitleBlock() const { return M_RichTitle; }
	URichTextBlock* GetRichDescriptionBlock() const;

	EMissionObjectiveType GetObjectiveType() const { return M_ShownWidgetState.ObjectiveType; }

	/** True while the objective plate shows the description (expanded or expanded with OK / Next). */
	bool GetIsShowingDescription() const { return GetIsExpandedState(); }

	/**
	 * Broadcasts the plate's knob (or, while it is hidden, title) button click, as a player click would.
	 * @return False without an objective plate.
	 */
	bool SimulateClickObjectiveToggle();

	/** True when the plate's knob, title and OK / Next buttons all have their click handlers bound. */
	bool GetAreObjectivePlateButtonsBound() const;

	// IRTSUIElement
	virtual void OnHideAllGameUI(const bool bHide) override;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Objective")
	void OnChangeWidgetState(const EMissionWidgetState NewState, EMissionTextSpeed NewTextSpeed);

	UFUNCTION(BlueprintCallable)
	void OnClickedNextMission();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetWidgetState(const EMissionWidgetState NewState);

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UWidgetSwitcher* M_Switcher;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* M_RichTitle;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UProtoRichTextBlock* M_RichDescription;

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable)
	bool GetIsNextButtonExpandedWidget() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, NotBlueprintable)
	EMissionWidgetNextButton GetNextButtonType() const;

private:
	UPROPERTY()
	EMissionWidgetState M_WidgetState;

	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_AssociatedMission;

	// Title and description of the last init or refresh.
	UPROPERTY()
	FMissionWidgetState M_ShownWidgetState;

	// In-use (allocated in list) vs free (pooled).
	UPROPERTY()
	bool bM_IsInUse = false;

	// Global “hide all UI” latch.
	UPROPERTY()
	bool bM_WasHiddenByAllGameUI = false;

	bool GetIsValidRichTextBlocks() const;
	bool EnsureAssociatedMissionIsValid(const TWeakObjectPtr<UMissionBase> MissionBase) const;

	// Special type of widget with next button that immediately completes the mission.
	UPROPERTY()
	bool bM_UseNextAsExpandedWidget = false;

	// The button used in the expanded version of the widget that will complete the current mission when clicked.
	UPROPERTY()
	EMissionWidgetNextButton M_NextButtonType = EMissionWidgetNextButton::Btn_Next;

	// Final visibility gate: global-hide wins; otherwise show if in-use.
	void ApplyVisibilityPolicy();
	bool GetIsExpandedState() const;

	// ---- Objective plate (W_MissionObjectivePlate.cpp) ----

	// Built in NativeOnInitialized when the settings ask for it; empty keeps W_BP_Mission's own layout.
	UPROPERTY()
	FMissionObjectivePlateWidgets M_ObjectivePlate;

	/** Moves the title / description into a new C++ plate and makes the plate the widget tree's root. */
	void BuildObjectivePlate();
	bool BuildObjectivePlate_LoadMaterials(const UMissionObjectivePlateSettings& Settings);
	/** Builds indent, stack, plate frame and shield; returns the column the header and body go into. */
	UVerticalBox* BuildObjectivePlate_Frame(const UMissionObjectivePlateSettings& Settings);
	UWidget* BuildObjectivePlate_Shield(const UMissionObjectivePlateSettings& Settings);
	UWidget* BuildObjectivePlate_Header(const UMissionObjectivePlateSettings& Settings);
	UWidget* BuildObjectivePlate_TitleColumn(const UMissionObjectivePlateSettings& Settings);
	UWidget* BuildObjectivePlate_Knob();
	UWidget* BuildObjectivePlate_Body(const UMissionObjectivePlateSettings& Settings);
	UWidget* BuildObjectivePlate_ConfirmRow(const UMissionObjectivePlateSettings& Settings);
	void BuildObjectivePlate_KnobMaterials(const UMissionObjectivePlateSettings& Settings);

	/** Shows / hides and resizes the plate's parts for the current widget state and objective type. */
	void ApplyObjectivePlateState();
	bool GetIsValidObjectivePlate() const;
	void ApplyObjectivePlate_TypeVisuals(const bool bIsBonus, const bool bIsExpanded);
	void ApplyObjectivePlate_Sizes(const bool bIsBonus, const bool bIsExpanded);

	/**
	 * @brief The plate's buttons route through C++ (unlike W_BP_Mission's own buttons) so the plate follows the
	 * state, then fire the Blueprint event that drives the switcher, typewriter and button text.
	 * @param NewState The state to show.
	 */
	void RequestWidgetStateFromPlate(const EMissionWidgetState NewState);

	UFUNCTION()
	void OnClickedObjectiveToggle();

	UFUNCTION()
	void OnClickedObjectiveConfirm();
};
