// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "DeckBuilderCaptureSubsystem.generated.h"

class UW_DeckBuilderMenu;
class UW_WorldMenu;

/**
 * @brief Offscreen check of the deck builder card menu. Start the world campaign map with -DeckBuilderCapture
 * (-game -RenderOffscreen -windowed -ResX=1920 -ResY=1080): it opens the menu on a seeded new campaign, makes the
 * edits a player makes (slot a card, clip on a variant, switch section and filter, save and reopen), writes one
 * picture per step to Saved/DeckBuilderCapture and logs the deck after each step, then quits.
 * Never writes the campaign save itself: the save step only updates the save manager's cache.
 * Not created in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UDeckBuilderCaptureSubsystem : public UTickableWorldSubsystem
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
	enum class EPhase : uint8
	{
		Inactive,
		WaitingForCampaign,
		ApplyingStep,
		Settling,
		WaitingForCapture,
		Done
	};

	void SeedGameInstanceForWorldCampaign() const;
	UW_WorldMenu* FindReadyWorldMenu() const;
	UW_DeckBuilderMenu* FindDeckBuilderMenu() const;

	/** @return The step's name for its picture; empty when there is no step left. */
	FString ApplyStep(const int32 StepIndex);
	FString ApplyStep_SaveAndReopen(UW_DeckBuilderMenu& Menu) const;
	FString ApplyStep_SaveToDisk(UW_DeckBuilderMenu& Menu) const;

	void CaptureStep() const;
	void EnterPhase(const EPhase NewPhase);
	double GetSecondsInPhase() const;
	void Finish();

	EPhase M_Phase = EPhase::Inactive;
	double M_PhaseStartSeconds = 0.0;
	int32 M_StepIndex = 0;
	FString M_CurrentStepName;
};
