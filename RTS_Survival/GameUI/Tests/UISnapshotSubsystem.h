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

	void TickWaitingForHud();
	void TickWaitingAfterStart();
	void TickSettling();
	void TickWaitingForCapture();

	void EnterPhase(EUISnapshotPhase NewPhase);
	double GetSecondsInPhase() const;

	void BuildSteps();
	bool TryStartGame();
	void ApplyCurrentStep();
	void CaptureCurrentStep() const;
	void AdvanceStepOrFinish();
	void Finish();

	void SelectPlayerUnits() const;
	void SelectTrainer() const;

	FString GetCurrentStepName() const;
	bool GetIsValidPlayerController() const;
};
