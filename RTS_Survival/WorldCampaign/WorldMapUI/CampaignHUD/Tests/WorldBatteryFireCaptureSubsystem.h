// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldBatteryFireCaptureSubsystem.generated.h"

class AWorldBuildNode;
class AWorldDivisionBase;
class AWorldPlayerController;
class UW_WorldMenu;

/**
 * @brief Offscreen check of the battery fire mission. Start the world campaign map with
 * -WorldCampaignBatteryFireCapture and -RTSWorldCampaignSaveSlot=<name> (-game -RenderOffscreen -windowed
 * -ResX=1920 -ResY=1080): on a seeded new campaign it builds a battery, puts an enemy tank division in its range,
 * opens the fog there, frames both, fires, and writes Saved/WorldCampaignUICapture/batteryfire_*.png before firing
 * and 0.6, 1.5, 2.5 and 4.5 seconds after. Refuses to run without the save slot switch. Not in shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API UWorldBatteryFireCaptureSubsystem : public UTickableWorldSubsystem
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
		SettlingBeforeFire,
		WaitingToFire,
		Firing,
		WaitingToQuit,
		Done
	};

	void SeedGameInstanceForWorldCampaign() const;
	AWorldPlayerController* GetWorldController() const;
	UW_WorldMenu* FindReadyWorldMenu() const;

	/** An offscreen run reads the desktop's real cursor; a cursor at a screen edge would scroll the camera away. */
	void HoldCameraAgainstEdgeScroll() const;

	/** @return false when the scene could not be set up; the run then ends. */
	bool SetUpScene(AWorldPlayerController& Controller);
	AWorldBuildNode* BuildBatteryNearHQ(AWorldPlayerController& Controller) const;
	AWorldDivisionBase* FindOrSpawnTargetDivision(AWorldPlayerController& Controller,
	                                             const AWorldBuildNode& BatteryNode) const;
	void RevealAndFrame(AWorldPlayerController& Controller, const AWorldBuildNode& BatteryNode,
	                    const AWorldDivisionBase& TargetDivision) const;

	/** @return false when the mission was refused or plays no show; the run then ends. */
	bool Fire(AWorldPlayerController& Controller);

	/** Takes every picture whose time has come, one per frame. */
	void Tick_Firing(float DeltaTime);

	void RequestPicture(const TCHAR* PictureName) const;
	void EnterPhase(EPhase NewPhase);
	void Finish();

	EPhase M_Phase = EPhase::Inactive;

	// Seconds of world time in the current phase.
	float M_SecondsInPhase = 0.f;

	// Index of the next picture after firing.
	int32 M_NextShotIndex = 0;

	FName M_BatteryNodeId = NAME_None;

	FGuid M_TargetDivisionKey;
};
