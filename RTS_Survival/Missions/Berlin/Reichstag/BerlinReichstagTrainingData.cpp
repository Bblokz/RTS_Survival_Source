// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BerlinReichstagTrainingData.h"

namespace BerlinReichstagTrainingDefaults
{
	// Patrol guarding, as the placed patrol wave spawners use it.
	constexpr int32 IterationsAtPatrolPoint = 2;
	constexpr float GuardSecondsPerIteration = 12.f;
	constexpr float GuardSphereRadius = 800.f;
	constexpr float HelpOffsetRadiusMltMax = 1.2f;
	constexpr int32 MaxTriesFindNavPointForHelpOffset = 3;
	// Attack groups linger this long in combat at a waypoint before they push on.
	constexpr float AttackLingerSeconds = 30.f;
}

UBerlinReichstagTrainingData::UBerlinReichstagTrainingData()
{
	using namespace BerlinReichstagTrainingDefaults;
	PatrolSettings.AmountIterationsAtPatrolPoint = IterationsAtPatrolPoint;
	PatrolSettings.GuardTimePerPatrolPointIteration = GuardSecondsPerIteration;
	PatrolSettings.GuardSphereRadius = GuardSphereRadius;
	PatrolSettings.AttackMoveSettings.HelpOffsetRadiusMltMax = HelpOffsetRadiusMltMax;
	PatrolSettings.AttackMoveSettings.MaxTriesFindNavPointForHelpOffset = MaxTriesFindNavPointForHelpOffset;
	AttackSettings.HelpOffsetRadiusMltMax = HelpOffsetRadiusMltMax;
	AttackSettings.MaxTriesFindNavPointForHelpOffset = MaxTriesFindNavPointForHelpOffset;
	AttackSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint = AttackLingerSeconds;
}

FBerlinReichstagTrainingLevel UBerlinReichstagTrainingData::GetLevelForDifficulty(
	const ERTSGameDifficulty Difficulty) const
{
	switch (Difficulty)
	{
	case ERTSGameDifficulty::NewToRTS:
		return NewToRTS;
	case ERTSGameDifficulty::Hard:
		return Hard;
	case ERTSGameDifficulty::Brutal:
	case ERTSGameDifficulty::Ironman:
		return Brutal;
	case ERTSGameDifficulty::Normal:
	default:
		return Normal;
	}
}
