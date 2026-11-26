// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WerkWaveDirector.h"

#include "EngineUtils.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawner.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Enemy/Fallback/EnemyFallbackSubsystem.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Missions/DirectionFinding/DirectionFindingManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace WerkWaveDirectorConstants
{
	const FColor PlacementColor(235, 120, 40);
	constexpr float EditorDashSize = 400.f;
	// How often launched waves are checked for having been wiped out.
	constexpr float DefeatCheckSeconds = 1.f;
	const TCHAR* const LinePath = TEXT("/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/OderWarthe/");

	TSoftObjectPtr<USoundBase> MakeLine(const TCHAR* const LineName)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), LinePath, LineName,
		                                                                  LineName)));
	}
}

AWerkWaveDirector::AWerkWaveDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	LaterWaveWarnings.Add(WerkWaveDirectorConstants::MakeLine(TEXT("OderWarthe_A11_WaveLeaving_B")));
	LaterWaveWarnings.Add(WerkWaveDirectorConstants::MakeLine(TEXT("OderWarthe_A11_WaveLeaving_C")));
}

FColor AWerkWaveDirector::GetPlacementColor() const
{
	return WerkWaveDirectorConstants::PlacementColor;
}

FString AWerkWaveDirector::GetConfigurationProblem() const
{
	if (WerkSpawners.IsEmpty())
	{
		return TEXT("no Werk spawners");
	}
	for (int32 WerkIndex = 0; WerkIndex < WerkSpawners.Num(); ++WerkIndex)
	{
		if (not IsValid(WerkSpawners[WerkIndex]))
		{
			return FString::Printf(TEXT("spawner of Werk %d is not set"), WerkIndex + 1);
		}
	}
	if (not IsValid(DirectionFinding))
	{
		return TEXT("no direction-finding manager: no bursts, every Werk counts as standing");
	}
	return Super::GetConfigurationProblem();
}

FString AWerkWaveDirector::GetPlacementKindName() const
{
	return TEXT("WERK WAVES");
}

void AWerkWaveDirector::BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	for (const AEnemyWaveSpawner* const Spawner : WerkSpawners)
	{
		if (IsValid(Spawner))
		{
			OutDetail.DashedLines.Emplace(GetActorLocation(), Spawner->GetActorLocation(), GetPlacementColor(),
			                              WerkWaveDirectorConstants::EditorDashSize);
		}
	}
}

void AWerkWaveDirector::BeginPlay()
{
	Super::BeginPlay();
	M_Random.Initialize(RandomSeed != 0 ? RandomSeed : static_cast<int32>(FDateTime::Now().GetTicks() % MAX_int32));
	BindSpawners();
	BindTunnelMouths();
}

void AWerkWaveDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSpawners();
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimedWaveHandle);
		World->GetTimerManager().ClearTimer(M_DefeatCheckHandle);
	}
	Super::EndPlay(EndPlayReason);
}

AWerkWaveDirector* AWerkWaveDirector::FindWerkWaveDirector(const UObject* WorldContextObject)
{
	UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<AWerkWaveDirector> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

int32 AWerkWaveDirector::LaunchWave()
{
	const TArray<int32> StandingWerke = GetStandingWerke();
	if (StandingWerke.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Werk wave director " + GetName() + ": no standing Werk can send a wave.");
		return 0;
	}
	const int32 WerkIndex = StandingWerke[M_Random.RandRange(0, StandingWerke.Num() - 1)];
	FWerkWaveRecord& Wave = M_Waves.AddDefaulted_GetRef();
	Wave.WaveNumber = M_Waves.Num();
	Wave.WerkIndex = WerkIndex;
	// The spawner's timing (no start on begin play, no delay, one wave) is set by the map build.
	WerkSpawners[WerkIndex]->StartSpawner();
	return Wave.WaveNumber;
}

void AWerkWaveDirector::StartTimedWaves()
{
	if (bM_TimedWavesRunning)
	{
		return;
	}
	bM_TimedWavesRunning = true;
	ScheduleNextTimedWave();
}

void AWerkWaveDirector::StopTimedWaves()
{
	bM_TimedWavesRunning = false;
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimedWaveHandle);
	}
}

float AWerkWaveDirector::GetFirstWaveGraceSeconds() const
{
	return GetTiming().FirstWaveGraceSeconds;
}

const FWerkWaveTiming& AWerkWaveDirector::GetTiming() const
{
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	EnemyPlacement::TryGetGameDifficulty(this, Difficulty);
	switch (Difficulty)
	{
	case ERTSGameDifficulty::Hard:
		return TimingHard;
	case ERTSGameDifficulty::Brutal:
	case ERTSGameDifficulty::Ironman:
		return TimingBrutal;
	default:
		return TimingNormal;
	}
}

bool AWerkWaveDirector::GetIsWerkStanding(const int32 WerkIndex) const
{
	if (not WerkSpawners.IsValidIndex(WerkIndex) || not IsValid(WerkSpawners[WerkIndex]))
	{
		return false;
	}
	// A spawner still sending the previous wave cannot start another one.
	if (WerkSpawners[WerkIndex]->GetIsSpawnerRunning())
	{
		return false;
	}
	if (M_TunnelMouthCollapsed.IsValidIndex(WerkIndex) && M_TunnelMouthCollapsed[WerkIndex])
	{
		return false;
	}
	return not IsValid(DirectionFinding) || not DirectionFinding->GetIsWerkEliminated(WerkIndex);
}

int32 AWerkWaveDirector::GetCollapsedTunnelMouthCount() const
{
	int32 CollapsedCount = 0;
	for (const bool bCollapsed : M_TunnelMouthCollapsed)
	{
		CollapsedCount += bCollapsed ? 1 : 0;
	}
	return CollapsedCount;
}

void AWerkWaveDirector::BindTunnelMouths()
{
	M_TunnelMouthCollapsed.Init(false, TunnelMouths.Num());
	for (int32 WerkIndex = 0; WerkIndex < TunnelMouths.Num(); ++WerkIndex)
	{
		ADestructableEnvActor* const TunnelMouth = TunnelMouths[WerkIndex];
		if (not IsValid(TunnelMouth))
		{
			continue;
		}
		TunnelMouth->OnDestructableEnvActorDied.AddWeakLambda(this, [this, WerkIndex]()
		{
			HandleTunnelMouthDied(WerkIndex);
		});
	}
}

void AWerkWaveDirector::HandleTunnelMouthDied(const int32 WerkIndex)
{
	if (not M_TunnelMouthCollapsed.IsValidIndex(WerkIndex) || M_TunnelMouthCollapsed[WerkIndex])
	{
		return;
	}
	M_TunnelMouthCollapsed[WerkIndex] = true;
	OnTunnelMouthCollapsed.Broadcast(WerkIndex);
}

void AWerkWaveDirector::PlayLaterWaveWarning(const int32 WaveNumber)
{
	// The first wave's warning belongs to M04 (with "Hear that?" when DF 1 listens).
	constexpr int32 FirstWaveNumber = 1;
	if (WaveNumber <= FirstWaveNumber || LaterWaveWarnings.IsEmpty())
	{
		return;
	}
	UPlayerPortraitManager* const PortraitManager = FRTS_Statics::GetPlayerPortraitManager(this);
	USoundBase* const Line = LaterWaveWarnings[M_LaterWarningsPlayed % LaterWaveWarnings.Num()].LoadSynchronous();
	++M_LaterWarningsPlayed;
	if (IsValid(PortraitManager) && Line != nullptr)
	{
		PortraitManager->PlayPortrait(WarningPortrait, Line);
	}
}

TArray<int32> AWerkWaveDirector::GetStandingWerke() const
{
	TArray<int32> StandingWerke;
	for (int32 WerkIndex = 0; WerkIndex < WerkSpawners.Num(); ++WerkIndex)
	{
		if (GetIsWerkStanding(WerkIndex))
		{
			StandingWerke.Add(WerkIndex);
		}
	}
	return StandingWerke;
}

bool AWerkWaveDirector::GetIsValidDirectionFinding() const
{
	if (IsValid(DirectionFinding))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "DirectionFinding",
	                                                       "AWerkWaveDirector::GetIsValidDirectionFinding", this);
	return false;
}

void AWerkWaveDirector::BindSpawners()
{
	for (AEnemyWaveSpawner* const Spawner : WerkSpawners)
	{
		if (IsValid(Spawner))
		{
			Spawner->OnWaveUnitsSpawned.AddUniqueDynamic(this, &AWerkWaveDirector::HandleSpawnerUnitsSpawned);
		}
	}
}

void AWerkWaveDirector::UnbindSpawners()
{
	for (AEnemyWaveSpawner* const Spawner : WerkSpawners)
	{
		if (IsValid(Spawner))
		{
			Spawner->OnWaveUnitsSpawned.RemoveDynamic(this, &AWerkWaveDirector::HandleSpawnerUnitsSpawned);
		}
	}
}

void AWerkWaveDirector::HandleSpawnerUnitsSpawned(AEnemyWaveSpawner* Spawner, const TArray<AActor*>& Units)
{
	const int32 WerkIndex = WerkSpawners.IndexOfByKey(Spawner);
	const int32 WaveIndex = FindWaitingWave(WerkIndex);
	if (not M_Waves.IsValidIndex(WaveIndex))
	{
		// A spawner that started on its own (not through this director); not a Werk wave.
		return;
	}
	FWerkWaveRecord& Wave = M_Waves[WaveIndex];
	Wave.bHasSpawned = true;
	for (AActor* const Unit : Units)
	{
		Wave.Units.Add(Unit);
	}
	if (GetIsValidDirectionFinding())
	{
		DirectionFinding->NotifyAttackOrder();
	}
	EnsureDefeatCheck();
	// Copied: a listener may launch the next wave, which grows M_Waves.
	const int32 WaveNumber = Wave.WaveNumber;
	PlayLaterWaveWarning(WaveNumber);
	OnWaveLaunched.Broadcast(WaveNumber, WerkIndex);
}

int32 AWerkWaveDirector::FindWaitingWave(const int32 WerkIndex) const
{
	for (int32 WaveIndex = 0; WaveIndex < M_Waves.Num(); ++WaveIndex)
	{
		if (M_Waves[WaveIndex].WerkIndex == WerkIndex && not M_Waves[WaveIndex].bHasSpawned)
		{
			return WaveIndex;
		}
	}
	return INDEX_NONE;
}

void AWerkWaveDirector::ScheduleNextTimedWave()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || not bM_TimedWavesRunning)
	{
		return;
	}
	const FWerkWaveTiming& Timing = GetTiming();
	const bool bNoticed = IsValid(DirectionFinding) && DirectionFinding->GetHasNoticedPlayer();
	const float Interval = bNoticed ? Timing.IntervalAfterNoticeSeconds : Timing.IntervalSeconds;
	World->GetTimerManager().SetTimer(M_TimedWaveHandle, this, &AWerkWaveDirector::HandleTimedWave, Interval, false);
}

void AWerkWaveDirector::HandleTimedWave()
{
	LaunchWave();
	ScheduleNextTimedWave();
}

void AWerkWaveDirector::EnsureDefeatCheck()
{
	UWorld* const World = GetWorld();
	if (World == nullptr || World->GetTimerManager().IsTimerActive(M_DefeatCheckHandle))
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_DefeatCheckHandle, this, &AWerkWaveDirector::CheckDefeatedWaves,
	                                  WerkWaveDirectorConstants::DefeatCheckSeconds, true);
}

void AWerkWaveDirector::CheckDefeatedWaves()
{
	TArray<TPair<int32, int32>> NewlyDefeated;
	bool bAnyAlive = false;
	for (FWerkWaveRecord& Wave : M_Waves)
	{
		if (not Wave.bHasSpawned || Wave.bIsDefeated)
		{
			continue;
		}
		if (GetIsWaveAlive(Wave))
		{
			bAnyAlive = true;
			continue;
		}
		Wave.bIsDefeated = true;
		NewlyDefeated.Emplace(Wave.WaveNumber, Wave.WerkIndex);
	}
	if (not bAnyAlive)
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_DefeatCheckHandle);
		}
	}
	// Broadcast after the loop: a listener may launch a wave, which grows M_Waves.
	for (const TPair<int32, int32>& Defeated : NewlyDefeated)
	{
		OnWaveDefeated.Broadcast(Defeated.Key, Defeated.Value);
	}
}

bool AWerkWaveDirector::GetIsWaveAlive(const FWerkWaveRecord& Wave)
{
	for (const TWeakObjectPtr<AActor>& Unit : Wave.Units)
	{
		if (UEnemyFallbackSubsystem::GetIsUnitActorAlive(Unit.Get()))
		{
			return true;
		}
	}
	return false;
}
