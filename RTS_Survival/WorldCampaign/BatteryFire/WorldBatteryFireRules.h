// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeTypes.h"
#include "WorldBatteryFireRules.generated.h"

/**
 * @brief Why a battery fire mission was accepted or refused, so the UI can explain a refusal.
 */
UENUM(BlueprintType)
enum class EWorldBatteryFireResult : uint8
{
	Success,
	UnknownBattery,
	/** The node holds no built battery. */
	NotABattery,
	/** The battery already fired its missions of this player turn. */
	NoMissionsLeft,
	UnknownTarget,
	TargetNotEnemy,
	TargetOutOfRange,
	TargetHiddenByFog,
	/** No enemy division the battery could fire on; only returned for the battery as a whole. */
	NoTargetInRange
};

/**
 * @brief The numbers the fire mission rules need, copied from UWorldBatteryFireSettings so tests can set their own.
 */
struct RTS_SURVIVAL_API FWorldBatteryFireNumbers
{
	float Range = 3000.f;
	int32 MissionsPerTurn = 1;
	int32 EntriesDestroyedPerMission = 2;
};

/**
 * @brief What the rules need to know about the build node that wants to fire.
 */
struct RTS_SURVIVAL_API FWorldBatteryFireBattery
{
	bool bIsKnownNode = false;
	EWorldBuildNodeState NodeState = EWorldBuildNodeState::Locked;
	EWorldCampaignBuilding Building = EWorldCampaignBuilding::None;
	FVector Location = FVector::ZeroVector;
	int32 MissionsFiredThisTurn = 0;
};

/**
 * @brief What the rules need to know about one division or convoy on the map.
 */
struct RTS_SURVIVAL_API FWorldBatteryFireTarget
{
	FGuid DivisionKey;
	bool bExists = false;
	int32 OwningPlayer = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	// False when the fog mask hides the place the piece stands on.
	bool bIsVisibleThroughFog = false;
};

/**
 * @brief Which roster entries a fire mission removes. Damage takes the last entries, so the casualties are the last
 * formation slots.
 */
struct RTS_SURVIVAL_API FWorldBatteryFireCasualties
{
	int32 FirstDestroyedSlot = 0;
	int32 DestroyedCount = 0;
	bool bIsDivisionEmptied = false;
};

/**
 * @brief Battery fire mission rules as pure functions of plain data, so every refusal is unit tested and the
 * director, the build site panel and the target prompt cannot disagree.
 */
namespace WorldBatteryFireRules
{
	/** @return Success when the node is a built battery with a mission left this turn, else the first refusal. */
	RTS_SURVIVAL_API EWorldBatteryFireResult GetBatteryResult(const FWorldBatteryFireBattery& Battery,
	                                                         const FWorldBatteryFireNumbers& Numbers);

	/**
	 * @brief Decides whether a battery may fire on one division, in the order the player would want to hear about it.
	 * @param Battery The firing node.
	 * @param Target The clicked division or convoy.
	 * @param Numbers Range and missions per turn.
	 * @return Success, or the first reason the mission is refused.
	 */
	RTS_SURVIVAL_API EWorldBatteryFireResult GetFireMissionResult(const FWorldBatteryFireBattery& Battery,
	                                                             const FWorldBatteryFireTarget& Target,
	                                                             const FWorldBatteryFireNumbers& Numbers);

	/** @return Keys of every division the battery may fire on now, in the order given. */
	RTS_SURVIVAL_API TArray<FGuid> GetValidTargetKeys(const FWorldBatteryFireBattery& Battery,
	                                                  const TArray<FWorldBatteryFireTarget>& Targets,
	                                                  const FWorldBatteryFireNumbers& Numbers);

	/**
	 * @brief Whether the fire mission button of a battery can be pressed.
	 * @param Battery The node whose panel is open.
	 * @param Targets Every division and convoy on the map.
	 * @param Numbers Range and missions per turn.
	 * @return Success, a refusal of the battery itself, or NoTargetInRange.
	 */
	RTS_SURVIVAL_API EWorldBatteryFireResult GetBatteryReadiness(const FWorldBatteryFireBattery& Battery,
	                                                            const TArray<FWorldBatteryFireTarget>& Targets,
	                                                            const FWorldBatteryFireNumbers& Numbers);

	/** @return Player-facing line for a refusal; empty for Success. */
	RTS_SURVIVAL_API FText GetFireResultText(EWorldBatteryFireResult Result);

	RTS_SURVIVAL_API bool GetIsWithinRangeXY(const FVector& BatteryLocation, const FVector& TargetLocation,
	                                         float Range);

	/** @return Missions the node fired in CurrentTurn; records of another turn count as zero. */
	RTS_SURVIVAL_API int32 GetMissionsFiredThisTurn(const FWorldBatteryFireTurnState& TurnState, FName BuildNodeId,
	                                                int32 CurrentTurn);

	/** Counts one mission for the node in CurrentTurn; records of an older turn are dropped first. */
	RTS_SURVIVAL_API void RecordMissionFired(FWorldBatteryFireTurnState& TurnState, FName BuildNodeId,
	                                         int32 CurrentTurn);

	/** Empties the records and stamps them with the new turn. */
	RTS_SURVIVAL_API void ResetForNewTurn(FWorldBatteryFireTurnState& TurnState, int32 NewTurn);

	/**
	 * @brief The slots a mission destroys in a roster of RemainingEntryCount entries.
	 * @param RemainingEntryCount Entries before the mission; negative when the piece keeps no roster.
	 * @param EntriesDestroyedPerMission Whole entries one mission removes.
	 * @return The last slots of the roster; emptied when nothing is left afterwards.
	 */
	RTS_SURVIVAL_API FWorldBatteryFireCasualties GetCasualties(int32 RemainingEntryCount,
	                                                          int32 EntriesDestroyedPerMission);

	/**
	 * @brief Spreads the casualties over the salvo so they die one by one and the last shell takes the last one.
	 * @param CasualtyIndex 0 = the first casualty to die.
	 * @param CasualtyCount Number of casualties of this mission.
	 * @param ShellCount Shells in the salvo.
	 * @return Index of the shell that kills this casualty, or INDEX_NONE without shells or casualties.
	 */
	RTS_SURVIVAL_API int32 GetShellIndexForCasualty(int32 CasualtyIndex, int32 CasualtyCount, int32 ShellCount);

	/**
	 * @brief A point on the shell's arc: a straight line with a parabola added on top.
	 * @param Start Muzzle location.
	 * @param End Impact location.
	 * @param Alpha 0 at the muzzle, 1 at the impact; clamped.
	 * @param ArcHeightFactor Peak height as a share of the XY distance.
	 * @return World location of the shell.
	 */
	RTS_SURVIVAL_API FVector GetShellLocation(const FVector& Start, const FVector& End, float Alpha,
	                                          float ArcHeightFactor);

	/** @return Seconds after the show started at which a shell leaves the gun. */
	RTS_SURVIVAL_API float GetShellLaunchSeconds(int32 ShellIndex, float LeadInSeconds, float StaggerSeconds);

	/**
	 * @brief Length of the whole show: lead-in, the staggered salvo, the last flight and the linger on the result.
	 * @return Seconds from the start of the show until input is given back.
	 */
	RTS_SURVIVAL_API float GetShowDurationSeconds(int32 ShellCount, float LeadInSeconds, float StaggerSeconds,
	                                              float FlightSeconds, float LingerSeconds);
}
