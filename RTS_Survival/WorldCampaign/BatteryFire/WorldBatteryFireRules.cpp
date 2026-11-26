// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireRules.h"

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionOwners.h"

#define LOCTEXT_NAMESPACE "WorldBatteryFireRules"

namespace WorldBatteryFireRulesConstants
{
	// Peak of the unit parabola 4a(1-a) is 1 at a = 0.5.
	constexpr float ParabolaScale = 4.f;
}

EWorldBatteryFireResult WorldBatteryFireRules::GetBatteryResult(const FWorldBatteryFireBattery& Battery,
                                                               const FWorldBatteryFireNumbers& Numbers)
{
	if (not Battery.bIsKnownNode)
	{
		return EWorldBatteryFireResult::UnknownBattery;
	}

	const bool bIsBuiltBattery = Battery.NodeState == EWorldBuildNodeState::Built
		&& Battery.Building == EWorldCampaignBuilding::Battery;
	if (not bIsBuiltBattery)
	{
		return EWorldBatteryFireResult::NotABattery;
	}

	if (Battery.MissionsFiredThisTurn >= Numbers.MissionsPerTurn)
	{
		return EWorldBatteryFireResult::NoMissionsLeft;
	}

	return EWorldBatteryFireResult::Success;
}

EWorldBatteryFireResult WorldBatteryFireRules::GetFireMissionResult(const FWorldBatteryFireBattery& Battery,
                                                                   const FWorldBatteryFireTarget& Target,
                                                                   const FWorldBatteryFireNumbers& Numbers)
{
	const EWorldBatteryFireResult BatteryResult = GetBatteryResult(Battery, Numbers);
	if (BatteryResult != EWorldBatteryFireResult::Success)
	{
		return BatteryResult;
	}

	if (not Target.bExists || not Target.DivisionKey.IsValid())
	{
		return EWorldBatteryFireResult::UnknownTarget;
	}

	if (Target.OwningPlayer != WorldDivisionOwners::EnemyOwner)
	{
		return EWorldBatteryFireResult::TargetNotEnemy;
	}

	if (not GetIsWithinRangeXY(Battery.Location, Target.Location, Numbers.Range))
	{
		return EWorldBatteryFireResult::TargetOutOfRange;
	}

	if (not Target.bIsVisibleThroughFog)
	{
		return EWorldBatteryFireResult::TargetHiddenByFog;
	}

	return EWorldBatteryFireResult::Success;
}

TArray<FGuid> WorldBatteryFireRules::GetValidTargetKeys(const FWorldBatteryFireBattery& Battery,
                                                        const TArray<FWorldBatteryFireTarget>& Targets,
                                                        const FWorldBatteryFireNumbers& Numbers)
{
	TArray<FGuid> ValidTargetKeys;
	for (const FWorldBatteryFireTarget& Target : Targets)
	{
		if (GetFireMissionResult(Battery, Target, Numbers) == EWorldBatteryFireResult::Success)
		{
			ValidTargetKeys.Add(Target.DivisionKey);
		}
	}
	return ValidTargetKeys;
}

EWorldBatteryFireResult WorldBatteryFireRules::GetBatteryReadiness(const FWorldBatteryFireBattery& Battery,
                                                                  const TArray<FWorldBatteryFireTarget>& Targets,
                                                                  const FWorldBatteryFireNumbers& Numbers)
{
	const EWorldBatteryFireResult BatteryResult = GetBatteryResult(Battery, Numbers);
	if (BatteryResult != EWorldBatteryFireResult::Success)
	{
		return BatteryResult;
	}

	return GetValidTargetKeys(Battery, Targets, Numbers).Num() > 0
		       ? EWorldBatteryFireResult::Success
		       : EWorldBatteryFireResult::NoTargetInRange;
}

FText WorldBatteryFireRules::GetFireResultText(const EWorldBatteryFireResult Result)
{
	switch (Result)
	{
	case EWorldBatteryFireResult::Success:
		return FText::GetEmpty();
	case EWorldBatteryFireResult::UnknownBattery:
	case EWorldBatteryFireResult::NotABattery:
		return LOCTEXT("NotABattery", "No battery stands here.");
	case EWorldBatteryFireResult::NoMissionsLeft:
		return LOCTEXT("NoMissionsLeft", "Already fired this turn.");
	case EWorldBatteryFireResult::UnknownTarget:
		return LOCTEXT("UnknownTarget", "Pick an enemy division.");
	case EWorldBatteryFireResult::TargetNotEnemy:
		return LOCTEXT("TargetNotEnemy", "Not an enemy division.");
	case EWorldBatteryFireResult::TargetOutOfRange:
		return LOCTEXT("TargetOutOfRange", "Out of range.");
	case EWorldBatteryFireResult::TargetHiddenByFog:
		return LOCTEXT("TargetHiddenByFog", "The target is not in sight.");
	case EWorldBatteryFireResult::NoTargetInRange:
		return LOCTEXT("NoTargetInRange", "No enemy division in range.");
	}
	return FText::GetEmpty();
}

bool WorldBatteryFireRules::GetIsWithinRangeXY(const FVector& BatteryLocation, const FVector& TargetLocation,
                                               const float Range)
{
	if (Range <= 0.f)
	{
		return false;
	}

	return FVector::DistSquared2D(BatteryLocation, TargetLocation) <= FMath::Square(Range);
}

int32 WorldBatteryFireRules::GetMissionsFiredThisTurn(const FWorldBatteryFireTurnState& TurnState,
                                                      const FName BuildNodeId, const int32 CurrentTurn)
{
	if (TurnState.Turn != CurrentTurn)
	{
		return 0;
	}

	for (const FWorldBatteryFireRecord& Record : TurnState.Records)
	{
		if (Record.BuildNodeId == BuildNodeId)
		{
			return Record.MissionsFired;
		}
	}
	return 0;
}

void WorldBatteryFireRules::RecordMissionFired(FWorldBatteryFireTurnState& TurnState, const FName BuildNodeId,
                                               const int32 CurrentTurn)
{
	if (TurnState.Turn != CurrentTurn)
	{
		ResetForNewTurn(TurnState, CurrentTurn);
	}

	for (FWorldBatteryFireRecord& Record : TurnState.Records)
	{
		if (Record.BuildNodeId == BuildNodeId)
		{
			Record.MissionsFired++;
			return;
		}
	}

	FWorldBatteryFireRecord NewRecord;
	NewRecord.BuildNodeId = BuildNodeId;
	NewRecord.MissionsFired = 1;
	TurnState.Records.Add(NewRecord);
}

void WorldBatteryFireRules::ResetForNewTurn(FWorldBatteryFireTurnState& TurnState, const int32 NewTurn)
{
	TurnState.Turn = NewTurn;
	TurnState.Records.Reset();
}

FWorldBatteryFireCasualties WorldBatteryFireRules::GetCasualties(const int32 RemainingEntryCount,
                                                                const int32 EntriesDestroyedPerMission)
{
	FWorldBatteryFireCasualties Casualties;
	if (RemainingEntryCount < 0 || EntriesDestroyedPerMission <= 0)
	{
		return Casualties;
	}

	Casualties.DestroyedCount = FMath::Min(RemainingEntryCount, EntriesDestroyedPerMission);
	Casualties.FirstDestroyedSlot = RemainingEntryCount - Casualties.DestroyedCount;
	Casualties.bIsDivisionEmptied = Casualties.FirstDestroyedSlot == 0;
	return Casualties;
}

int32 WorldBatteryFireRules::GetShellIndexForCasualty(const int32 CasualtyIndex, const int32 CasualtyCount,
                                                      const int32 ShellCount)
{
	if (ShellCount <= 0 || CasualtyCount <= 0 || CasualtyIndex < 0 || CasualtyIndex >= CasualtyCount)
	{
		return INDEX_NONE;
	}

	// Casualty j dies at the end of its share of the salvo: with 5 shells and 2 casualties, the second and
	// the fifth shell (indices 1 and 4).
	const int32 ShellIndex = ((CasualtyIndex + 1) * ShellCount) / CasualtyCount - 1;
	return FMath::Clamp(ShellIndex, 0, ShellCount - 1);
}

FVector WorldBatteryFireRules::GetShellLocation(const FVector& Start, const FVector& End, const float Alpha,
                                                const float ArcHeightFactor)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	const float PeakHeight = FVector::Dist2D(Start, End) * FMath::Max(0.f, ArcHeightFactor);
	const float ArcHeight = PeakHeight * WorldBatteryFireRulesConstants::ParabolaScale * ClampedAlpha
		* (1.f - ClampedAlpha);
	return FMath::Lerp(Start, End, ClampedAlpha) + FVector(0.f, 0.f, ArcHeight);
}

float WorldBatteryFireRules::GetShellLaunchSeconds(const int32 ShellIndex, const float LeadInSeconds,
                                                   const float StaggerSeconds)
{
	return FMath::Max(0.f, LeadInSeconds) + FMath::Max(0, ShellIndex) * FMath::Max(0.f, StaggerSeconds);
}

float WorldBatteryFireRules::GetShowDurationSeconds(const int32 ShellCount, const float LeadInSeconds,
                                                    const float StaggerSeconds, const float FlightSeconds,
                                                    const float LingerSeconds)
{
	const int32 LastShellIndex = FMath::Max(0, ShellCount - 1);
	return GetShellLaunchSeconds(LastShellIndex, LeadInSeconds, StaggerSeconds) + FMath::Max(0.f, FlightSeconds)
		+ FMath::Max(0.f, LingerSeconds);
}

#undef LOCTEXT_NAMESPACE
