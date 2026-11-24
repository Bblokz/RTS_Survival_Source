// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldCountryLookCaptureSubsystem.generated.h"

class AWorldPlayerController;
class UWorldCountryOccupationRegulator;

/**
 * @brief Offscreen pictures of the country look. Start the world campaign map with -WorldCountryLookCapture
 * (-game -RenderOffscreen -windowed -ResX=1920 -ResY=1080): it generates a seeded campaign on its own save slot,
 * looks straight down at central Europe without fog or UI, clears targets one after another and writes one picture
 * per step to Saved/WorldCountryLookCapture, then quits. Not created in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UWorldCountryLookCaptureSubsystem : public UTickableWorldSubsystem
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
	AWorldPlayerController* FindReadyWorldController() const;
	UWorldCountryOccupationRegulator* FindCountryRegulator() const;
	void PrepareScene();

	/**
	 * @brief Runs one step of the tour.
	 * @param StepIndex Step to run.
	 * @param OutSettleSeconds How long to wait before the picture; short for a front caught while it spreads.
	 * @return The step's name for its picture; empty when there is no step left.
	 */
	FString ApplyStep(int32 StepIndex, double& OutSettleSeconds);

	/** Marks the mission or enemy item on an anchor as won, the way a returning operation does. */
	bool ClearAnchor(const FGuid& AnchorKey) const;
	bool ClearPinnedPlace(FName PinId) const;
	int32 ClearEveryTargetInCountry(FName CountryId) const;

	/** Puts one building after the other on the country's open build sites, for the look only. */
	int32 BuildOnEveryOpenSiteInCountry(FName CountryId) const;
	void MoveCamera(const FVector& CameraLocation) const;

	/** Traces down onto the site the way the cursor trace does and hovers what it hits; false when it misses. */
	bool HoverBuildSiteByTrace(FName BuildNodeId) const;

	void CaptureStep() const;
	void EnterPhase(EPhase NewPhase);
	double GetSecondsInPhase() const;
	void Finish();

	EPhase M_Phase = EPhase::Inactive;
	double M_PhaseStartSeconds = 0.0;
	double M_SettleSeconds = 0.0;
	int32 M_StepIndex = 0;
	FString M_CurrentStepName;
};
