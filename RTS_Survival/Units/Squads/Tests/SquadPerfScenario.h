#pragma once

#include "CoreMinimal.h"

class ASquadController;
class URTSCoverFinderWorldSubsystem;
class UWorld;

enum class ESquadPerfPhase : uint8
{
	Inactive,
	WaitingForCoverScan,
	WarmingUp,
	Fighting,
	Moving
};

/** Frame times of one measured phase, in milliseconds of wall time per game frame. */
struct FSquadPerfPhaseSamples
{
	TArray<float> FrameMilliseconds;
	int32 LivingSoldiersAtStart = 0;
	int32 MovingSoldierSamples = 0;
	int32 FiringSoldierSamples = 0;
};

/**
 * @brief Measures what a map full of squads costs per frame, first while they fight where they stand and then
 * while every squad is kept on the move. Logs frame time percentiles as RTS_SQUAD_PERF and, with
 * -SquadPerfStatDump, lets the engine dump its averaged stat tree over each phase to show where the time goes.
 * Started with -SquadPerfMeasure. Not part of a shipping build.
 * @note -SquadPerfNoErrorDialogs logs reported errors instead of opening their dialog, which costs frame time.
 */
struct FSquadPerfScenario
{
	void Start();
	void Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem);

private:
	ESquadPerfPhase M_Phase = ESquadPerfPhase::Inactive;
	int32 M_FramesLeftInPhase = 0;
	int32 M_FramesUntilNextMoveOrder = 0;
	bool bM_MoveAwayNext = true;
	bool bM_DumpStats = false;
	bool bM_DumpHitches = false;
	double M_LastTickSeconds = 0.0;
	FSquadPerfPhaseSamples M_Samples;

	// Where every squad stood when the measuring started; the move phase sends them out from there and back.
	TArray<TWeakObjectPtr<ASquadController>> M_Squads;
	TArray<FVector> M_SquadHomeLocations;

	void EnterPhase(UWorld& World, ESquadPerfPhase NewPhase, int32 FrameCount);
	void RecordFrame(UWorld& World);
	void ReportPhase(const TCHAR* PhaseName) const;
	void CollectSquads(UWorld& World);
	void OrderSquadsToMove();
	int32 CountLivingSoldiers(UWorld& World, int32& OutMovingCount, int32& OutFiringCount) const;
};
