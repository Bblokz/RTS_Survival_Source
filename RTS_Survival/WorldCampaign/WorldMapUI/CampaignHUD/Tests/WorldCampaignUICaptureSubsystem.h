// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldCampaignUICaptureSubsystem.generated.h"

class UW_WorldTechWheel;
enum class ETechnology : uint8;

class AWorldBuildNode;
class AWorldPlayerController;
class UW_WorldCampaignHUD;
class UW_WorldMenu;

/**
 * @brief Offscreen check of the campaign HUD. Start the world campaign map with -WorldCampaignUICapture and
 * -RTSWorldCampaignSaveSlot=<name> (-game -RenderOffscreen -windowed -ResX=1920 -ResY=1080): on a seeded new
 * campaign it shows the top strip, opens a build site and builds on it, shows the commander tooltip, opens the tech
 * wheel, selects and buys nodes and plays one enemy turn; one picture per step goes to Saved/WorldCampaignUICapture.
 * Refuses to run without the save slot switch, so it can never write the real campaign save. Not in shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API UWorldCampaignUICaptureSubsystem : public UTickableWorldSubsystem
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
	AWorldPlayerController* GetWorldController() const;
	UW_WorldMenu* FindReadyWorldMenu() const;

	/**
	 * Switches the player's camera input off. An offscreen run still reads the desktop's real cursor, and a cursor
	 * resting at a screen edge scrolls the camera off the map; the steps move the camera themselves.
	 */
	void HoldCameraAgainstEdgeScroll() const;

	/** @return The step's name for its picture; empty when there is no step left. */
	FString ApplyStep(int32 StepIndex);
	void ApplyStep_OpenBuildSite(AWorldPlayerController& Controller, UW_WorldCampaignHUD& CampaignHUD);
	void ApplyStep_BuildOnSite(UW_WorldCampaignHUD& CampaignHUD) const;
	void ApplyStep_BuyTechNodes(UW_WorldMenu& WorldMenu) const;
	static void SelectTechnologyNode(UW_WorldTechWheel* TechWheel, ETechnology Technology);
	void ApplyStep_PlayEnemyTurn(AWorldPlayerController& Controller) const;
	void ApplyStep_ShowFreezeAndCooldown(AWorldPlayerController& Controller, UW_WorldCampaignHUD& CampaignHUD) const;

	/** The open build site nearest the player HQ, where the camera rests after setup. */
	AWorldBuildNode* FindOpenBuildNodeNearHQ(AWorldPlayerController& Controller) const;

	/** Writes every mission piece's name and whether the fog lets it show, to read next to the first picture. */
	void LogMissionPieces() const;

	void CaptureStep() const;
	void EnterPhase(EPhase NewPhase);
	double GetSecondsInPhase() const;
	void Finish();

	EPhase M_Phase = EPhase::Inactive;
	double M_PhaseStartSeconds = 0.0;
	int32 M_StepIndex = 0;
	FString M_CurrentStepName;

	UPROPERTY()
	TWeakObjectPtr<AWorldBuildNode> M_CaptureBuildNode;
};
