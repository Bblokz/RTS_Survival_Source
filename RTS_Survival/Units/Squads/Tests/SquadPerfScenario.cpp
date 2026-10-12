#include "SquadPerfScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSquadPerf, Log, All);

namespace SquadPerfScenarioPrivate
{
	constexpr int32 WarmUpFrames = 300;
	constexpr int32 FightFrames = 1500;
	constexpr int32 MoveFrames = 1500;
	// Long enough for a squad to get going and arrive, short enough that nobody stands still for long.
	constexpr int32 FramesBetweenMoveOrders = 300;
	constexpr float MoveDistance = 2500.0f;
	// How often the soldiers are counted; counting every frame would itself show up in the frame time.
	constexpr int32 FramesBetweenSoldierCounts = 30;
	constexpr double MillisecondsPerSecond = 1000.0;
	// The stat dump of one phase has to be written before the next phase asks for its own.
	constexpr int32 StatDumpFrameMargin = 20;

	float GetPercentile(const TArray<float>& SortedValues, const float Percentile)
	{
		if (SortedValues.IsEmpty())
		{
			return 0.0f;
		}
		const int32 Index = FMath::Clamp(
			FMath::FloorToInt(Percentile * (SortedValues.Num() - 1)),
			0,
			SortedValues.Num() - 1);
		return SortedValues[Index];
	}
}

void FSquadPerfScenario::Start()
{
	M_Phase = ESquadPerfPhase::WaitingForCoverScan;
	bM_DumpStats = FParse::Param(FCommandLine::Get(), TEXT("SquadPerfStatDump"));
	bM_DumpHitches = FParse::Param(FCommandLine::Get(), TEXT("SquadPerfHitches"));
	if (FParse::Param(FCommandLine::Get(), TEXT("SquadPerfNoErrorDialogs")))
	{
		// An error dialog costs milliseconds in an editor build; a repeating error would be measured instead of the game.
		RTSFunctionLibrary::PushModalDialogSuppression();
	}
	UE_LOG(LogRTSSquadPerf, Display, TEXT("RTS_SQUAD_PERF scheduled"));
}

void FSquadPerfScenario::EnterPhase(UWorld& World, const ESquadPerfPhase NewPhase, const int32 FrameCount)
{
	M_Phase = NewPhase;
	M_FramesLeftInPhase = FrameCount;
	M_Samples = FSquadPerfPhaseSamples();
	M_Samples.FrameMilliseconds.Reserve(FrameCount);
	int32 MovingCount = 0;
	int32 FiringCount = 0;
	M_Samples.LivingSoldiersAtStart = CountLivingSoldiers(World, MovingCount, FiringCount);
	M_LastTickSeconds = FPlatformTime::Seconds();
	const bool bIsMeasuredPhase = NewPhase == ESquadPerfPhase::Fighting || NewPhase == ESquadPerfPhase::Moving;
	if (bM_DumpStats && bIsMeasuredPhase && GEngine != nullptr)
	{
		// The engine averages its stat tree over the phase and writes it to the log when the frames are up.
		GEngine->Exec(&World, *FString::Printf(TEXT("stat dumpave -num=%d -ms=0.02"), FrameCount - SquadPerfScenarioPrivate::StatDumpFrameMargin));
	}
	if (bM_DumpHitches && NewPhase == ESquadPerfPhase::Fighting && GEngine != nullptr)
	{
		// From here on the engine writes the stat tree of every frame that takes longer than the threshold.
		GEngine->Exec(&World, TEXT("t.HitchFrameTimeThreshold 14"));
		GEngine->Exec(&World, TEXT("stat dumphitches"));
	}
}

int32 FSquadPerfScenario::CountLivingSoldiers(UWorld& World, int32& OutMovingCount, int32& OutFiringCount) const
{
	int32 LivingCount = 0;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		if (not IsValid(SquadUnit) || not SquadUnit->IsUnitAlive())
		{
			continue;
		}
		++LivingCount;
		OutMovingCount += SquadUnit->GetVelocity().SizeSquared2D() > 1.0f ? 1 : 0;
		AInfantryWeaponMaster* Weapon = SquadUnit->GetInfantryWeapon();
		OutFiringCount += IsValid(Weapon) && IsValid(Weapon->GetCurrentTargetActor()) &&
			Weapon->GetIsCurrentTargetInRange() ? 1 : 0;
	}
	return LivingCount;
}

void FSquadPerfScenario::CollectSquads(UWorld& World)
{
	M_Squads.Reset();
	M_SquadHomeLocations.Reset();
	for (TActorIterator<ASquadController> SquadIterator(&World); SquadIterator; ++SquadIterator)
	{
		ASquadController* Squad = *SquadIterator;
		if (IsValid(Squad) && Squad->GetSquadUnitsCount() > 0)
		{
			M_Squads.Add(Squad);
			M_SquadHomeLocations.Add(Squad->GetActorLocation());
		}
	}
}

void FSquadPerfScenario::OrderSquadsToMove()
{
	using namespace SquadPerfScenarioPrivate;
	for (int32 SquadIndex = 0; SquadIndex < M_Squads.Num(); ++SquadIndex)
	{
		ASquadController* Squad = M_Squads[SquadIndex].Get();
		if (not IsValid(Squad) || Squad->GetSquadUnitsCount() <= 0)
		{
			continue;
		}
		// Every squad its own direction, so they cross each other like an army on the move does.
		const float DirectionDegrees = static_cast<float>(SquadIndex) * 67.0f;
		const FVector AwayLocation = M_SquadHomeLocations[SquadIndex] +
			FRotator(0.0f, DirectionDegrees, 0.0f).Vector() * MoveDistance;
		Squad->MoveToLocation(
			bM_MoveAwayNext ? AwayLocation : M_SquadHomeLocations[SquadIndex],
			true,
			FRotator::ZeroRotator);
	}
	bM_MoveAwayNext = not bM_MoveAwayNext;
}

void FSquadPerfScenario::RecordFrame(UWorld& World)
{
	const double NowSeconds = FPlatformTime::Seconds();
	M_Samples.FrameMilliseconds.Add(static_cast<float>(
		(NowSeconds - M_LastTickSeconds) * SquadPerfScenarioPrivate::MillisecondsPerSecond));
	if (M_FramesLeftInPhase % SquadPerfScenarioPrivate::FramesBetweenSoldierCounts == 0)
	{
		CountLivingSoldiers(World, M_Samples.MovingSoldierSamples, M_Samples.FiringSoldierSamples);
	}
	// Taken after the counting, so the test's own work is not part of the next frame's time.
	M_LastTickSeconds = FPlatformTime::Seconds();
}

void FSquadPerfScenario::ReportPhase(const TCHAR* PhaseName) const
{
	using namespace SquadPerfScenarioPrivate;
	TArray<float> SortedFrames = M_Samples.FrameMilliseconds;
	SortedFrames.Sort();
	double TotalMilliseconds = 0.0;
	for (const float FrameMilliseconds : SortedFrames)
	{
		TotalMilliseconds += FrameMilliseconds;
	}
	const int32 CountSamples = FMath::Max(1, SortedFrames.Num() / FramesBetweenSoldierCounts);
	UE_LOG(
		LogRTSSquadPerf,
		Display,
		TEXT("RTS_SQUAD_PERF phase=%s frames=%d avg_ms=%.3f p50_ms=%.3f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f soldiers=%d squads=%d avg_moving=%d avg_with_target=%d"),
		PhaseName,
		SortedFrames.Num(),
		SortedFrames.IsEmpty() ? 0.0 : TotalMilliseconds / SortedFrames.Num(),
		GetPercentile(SortedFrames, 0.5f),
		GetPercentile(SortedFrames, 0.95f),
		GetPercentile(SortedFrames, 0.99f),
		SortedFrames.IsEmpty() ? 0.0f : SortedFrames.Last(),
		M_Samples.LivingSoldiersAtStart,
		M_Squads.Num(),
		M_Samples.MovingSoldierSamples / CountSamples,
		M_Samples.FiringSoldierSamples / CountSamples);
}

void FSquadPerfScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	using namespace SquadPerfScenarioPrivate;
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == ESquadPerfPhase::Inactive || not IsValid(World))
	{
		return;
	}
	if (M_Phase == ESquadPerfPhase::WaitingForCoverScan)
	{
		// The map boots paused behind the start-game widget.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
		if (CoverSubsystem.GetHasCompletedFullScan())
		{
			EnterPhase(*World, ESquadPerfPhase::WarmingUp, WarmUpFrames);
		}
		return;
	}
	RecordFrame(*World);
	if (M_Phase == ESquadPerfPhase::Moving && --M_FramesUntilNextMoveOrder <= 0)
	{
		OrderSquadsToMove();
		M_FramesUntilNextMoveOrder = FramesBetweenMoveOrders;
		M_LastTickSeconds = FPlatformTime::Seconds();
	}
	if (--M_FramesLeftInPhase > 0)
	{
		return;
	}
	if (M_Phase == ESquadPerfPhase::WarmingUp)
	{
		CollectSquads(*World);
		EnterPhase(*World, ESquadPerfPhase::Fighting, FightFrames);
		return;
	}
	if (M_Phase == ESquadPerfPhase::Fighting)
	{
		ReportPhase(TEXT("fight"));
		M_FramesUntilNextMoveOrder = 0;
		EnterPhase(*World, ESquadPerfPhase::Moving, MoveFrames);
		return;
	}
	ReportPhase(TEXT("move"));
	M_Phase = ESquadPerfPhase::Inactive;
	UE_LOG(LogRTSSquadPerf, Display, TEXT("RTS_SQUAD_PERF RESULT DONE map=%s"), *World->GetName());
}

#endif // RTS_WITH_MAP_TESTS
