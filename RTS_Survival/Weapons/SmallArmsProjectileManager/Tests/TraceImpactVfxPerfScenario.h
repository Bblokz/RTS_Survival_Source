#pragma once

#include "CoreMinimal.h"

class ACameraActor;
class ASquadController;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class ETraceImpactVfxPerfPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	WaitingForSquads,
	Fighting
};

/** Frame times and impact drawing cost of the measured fight. */
struct FTraceImpactVfxPerfSamples
{
	TArray<float> FrameMilliseconds;
	int64 ActiveNiagaraComponentSamples = 0;
	// Draw calls of the whole frame as the renderer counted them, sampled together with the other counts.
	int64 DrawCallSamples = 0;
	int32 FiringSoldierSamples = 0;
	int32 CountSamples = 0;
};

/**
 * @brief Spawns a line of German squads against a line of Soviet squads, keeps them alive and shooting, and
 * measures what the fight costs per frame and what drawing the trace impacts costs on the game thread.
 * Logs as RTS_TRACE_IMPACT_VFX. Started with -TraceImpactVfxMeasure; add -TraceImpactVfxLegacy to measure
 * the one-Niagara-component-per-impact drawing instead. Not part of a shipping build.
 */
struct FTraceImpactVfxPerfScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	ETraceImpactVfxPerfPhase M_Phase = ETraceImpactVfxPerfPhase::Inactive;
	int32 M_FramesLeftInPhase = 0;
	int32 M_SquadsPerSide = 14;
	int32 M_FightFrames = 1800;
	float M_LineDistance = 1800.0f;
	// Tanks put behind each line so hull and turret MGs fire in the measured fight as well.
	int32 M_TanksPerSide = 3;
	bool bM_DumpStats = false;
	// Moves the camera close to the fight and saves a picture every few seconds; not for measuring runs.
	bool bM_TakeScreenshots = false;
	int32 M_ScreenshotCount = 0;
	// Draws one impact of every type in a row in front of the screenshot camera, over and over.
	bool bM_ShowcaseImpactTypes = false;
	double M_LastTickSeconds = 0.0;
	FVector M_FightCenter = FVector::ZeroVector;
	FTraceImpactVfxPerfSamples M_Samples;

	TArray<TWeakObjectPtr<ASquadController>> M_GermanSquads;
	TArray<TWeakObjectPtr<ASquadController>> M_SovietSquads;
	TWeakObjectPtr<ACameraActor> M_OverviewCamera;

	void SpawnSquads(UWorld& World);
	void SpawnSquadLine(
		UWorld& World,
		const TArray<const TCHAR*>& SquadClassPaths,
		const float LineOffset,
		TArray<TWeakObjectPtr<ASquadController>>& OutSquads) const;
	void SpawnTanksBehindLine(UWorld& World, const uint8 OwningPlayer, const float LineOffset) const;
	bool GetHaveAllSquadsLoaded() const;
	void StartFight(UWorld& World);
	void OrderSquadsToAttack(
		const TArray<TWeakObjectPtr<ASquadController>>& Attackers,
		const TArray<TWeakObjectPtr<ASquadController>>& Targets) const;
	void MakeSoldiersHardToKill() const;
	void HealSoldiers() const;
	void KeepCameraOnFight(UWorld& World);
	void RecordFrame(UWorld& World);
	void SampleCounts(UWorld& World);
	void DrawShowcaseImpacts(UWorld& World) const;
	void ReportFight(UWorld& World) const;
};
