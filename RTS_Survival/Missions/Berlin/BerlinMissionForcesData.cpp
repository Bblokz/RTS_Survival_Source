// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BerlinMissionForcesData.h"

UBerlinMissionForcesData* UBerlinMissionForcesData::LoadForDifficulty(
	const TMap<ERTSGameDifficulty, TSoftObjectPtr<UBerlinMissionForcesData>>& ForcesPerDifficulty,
	const ERTSGameDifficulty Difficulty)
{
	TArray<ERTSGameDifficulty> Candidates = {Difficulty};
	if (Difficulty == ERTSGameDifficulty::Ironman)
	{
		Candidates.Add(ERTSGameDifficulty::Brutal);
	}
	Candidates.Add(ERTSGameDifficulty::Normal);
	for (const ERTSGameDifficulty Candidate : Candidates)
	{
		const TSoftObjectPtr<UBerlinMissionForcesData>* Entry = ForcesPerDifficulty.Find(Candidate);
		if (Entry == nullptr || Entry->IsNull())
		{
			continue;
		}
		if (UBerlinMissionForcesData* Loaded = Entry->LoadSynchronous())
		{
			return Loaded;
		}
	}
	return nullptr;
}

int32 UBerlinMissionForcesData::GetPatrolUnitCount() const
{
	int32 UnitCount = 0;
	for (const FEnemyWaveSpawnerUnitEntry& Entry : Patrols.PatrolUnits)
	{
		UnitCount += Entry.Count;
	}
	return UnitCount;
}
