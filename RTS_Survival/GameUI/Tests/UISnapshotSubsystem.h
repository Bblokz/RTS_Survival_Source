// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "UISnapshotSubsystem.generated.h"

class ACPPController;

enum class EUISnapshotPhase : uint8
{
	Inactive,
	WaitingForHud,
	WaitingAfterStart,
	ApplyingStep,
	Settling,
	WaitingForCapture,
	Done
};

enum class EUISnapshotSelection : uint8
{
	PlayerUnits,
	Trainer
};

/**
 * @brief Captures the in-game HUD with UI at several resolutions so skin and pixel-snapping changes can be reviewed
 * without driving a PIE session. Started with -UISnapshot (best with -game -RenderOffscreen -unattended); writes
 * Saved/UISnapshot/<selection>_<height>p.png plus a widget reflector snapshot per capture, then quits.
 * Not created in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UUISnapshotSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	struct FSnapshotStep
	{
		EUISnapshotSelection Selection = EUISnapshotSelection::PlayerUnits;
		FIntPoint Resolution = FIntPoint::ZeroValue;
	};

	TArray<FSnapshotStep> M_Steps;
	int32 M_StepIndex = 0;
	double M_PhaseStartSeconds = 0.0;
	EUISnapshotPhase M_Phase = EUISnapshotPhase::Inactive;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	bool bM_HasAddedGallery = false;
	bool bM_HasFinishedTruckAnims = false;

	void TickWaitingForHud();
	void TickWaitingAfterStart();
	void TickSettling();
	void TickWaitingForCapture();

	void EnterPhase(EUISnapshotPhase NewPhase);
	double GetSecondsInPhase() const;

	void BuildSteps();
	bool TryStartGame();
	// Writes the minimap terrain capture next to the screenshots so its framing and exposure can be reviewed.
	void ExportMinimapTerrain() const;
	// Maps that open on a difficulty picker (the game start tutorial) wait for it; picks Normal.
	void DismissDifficultyPicker() const;
	// With -UISnapshotEnergyPreview the energy bar is held at a fixed fill so its look can be reviewed.
	void HoldEnergyPreview() const;
	// -UISnapshotHoverPortrait / -UISnapshotHoverAction / -UISnapshotHoverTraining hover the portrait, first action button or first training item.
	void HoverPortrait() const;
	void HoverFirstVisibleWidget(UClass* const UserWidgetClass, const FName InnerWidgetName) const;
	static void MoveSlateCursorTo(const FVector2D& AbsolutePosition);
	// -UISnapshotGallery=/Game/A+/Game/B adds those widgets with their designer content for review.
	void AddGalleryWidgetsOnce();
	static void ShowCollapsedGalleryParts(UUserWidget& GalleryWidget);
	void ApplyCurrentStep();
	void CaptureCurrentStep() const;
	void AdvanceStepOrFinish();
	void Finish();

	void SelectPlayerUnits() const;
	void SelectTrainer() const;
	AActor* FindPlayerTrainer() const;
	// -UISnapshotFocusTrainer keeps the camera on the trainer so a converting nomadic truck finishes its building.
	void FocusCameraOnTrainer() const;
	// -UISnapshotTickPoses: lets offscreen nomadic conversion montages play (meshes never count as rendered).
	void ForceNomadicMeshesToTickPose() const;
	// -UISnapshotFinishTruckAnims: ends the truck-unfold montage step of converting nomadic trucks.
	void FinishTruckUnfoldAnimations() const;
	// -UISnapshotHideGlass collapses every M_GlassPane so the same scene can be captured with and without glass.
	void HideGlassPanes() const;

	FString GetCurrentStepName() const;
	bool GetIsValidPlayerController() const;
};
