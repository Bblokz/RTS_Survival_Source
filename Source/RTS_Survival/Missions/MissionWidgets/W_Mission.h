#pragma once
#include "ProtoRichTextBlock.h"
#include "Blueprint/UserWidget.h"
#include "MissionWidgetState/MissionWidgetState.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Game/GameState/HideGameUI/RTSUIElement.h" // <- add

#include "W_Mission.generated.h"

class UMissionBase;
class URichTextBlock;
class UWidgetSwitcher;
enum class EMissionWidgetState : uint8;

UCLASS()
class RTS_SURVIVAL_API UW_Mission : public UUserWidget, public IRTSUIElement // <- add interface
{
	GENERATED_BODY()

public:
	void InitMissionWidget(const FMissionWidgetState& WidgetState,
	                       UMissionBase* AssociatedMission,
	                       const bool bUseNextAsExpanded,
	                       const bool bStartAsCollapsedWidget);

	void MarkWidgetAsFree();

	inline bool GetIsFreeWidget() const { return not bM_IsInUse; };

	// IRTSUIElement
	virtual void OnHideAllGameUI(const bool bHide) override; 

protected:
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

	TWeakObjectPtr<UMissionBase> M_AssociatedMission;

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
};
