// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignMapContentSettings.h"

/**
 * @brief One configured pinned place after the generator matched it to an anchor key.
 */
struct RTS_SURVIVAL_API FWorldCampaignResolvedPin
{
	FName PinId = NAME_None;
	EWorldPinnedPlaceRole Role = EWorldPinnedPlaceRole::None;
	EMapMission Mission = EMapMission::None;
	FGuid AnchorKey;
};

/**
 * @brief Plain-data pin set shared by the synchronous generator and the async solver snapshot.
 * The generator fills it in the anchor step (or from config on restore); placement code only reads it, so it
 * is safe to copy to the worker thread. An empty set means placement behaves exactly as without pins.
 */
struct RTS_SURVIVAL_API FWorldCampaignResolvedPins
{
	void Reset();

	/** Adds a pin; ignored when the anchor key is invalid or already used by another pin. */
	void AddPin(const FWorldCampaignResolvedPin& Pin);

	bool HasAnyPins() const { return M_Pins.Num() > 0; }
	const TArray<FWorldCampaignResolvedPin>& GetPins() const { return M_Pins; }
	const TSet<FGuid>& GetPinnedAnchorKeys() const { return M_PinnedAnchorKeys; }
	const TSet<FGuid>& GetPinnedMissionAnchorKeys() const { return M_PinnedMissionAnchorKeys; }

	bool GetIsPinnedAnchorKey(const FGuid& AnchorKey) const;
	bool GetIsPinnedMissionAnchorKey(const FGuid& AnchorKey) const;
	FName GetPinIdForAnchorKey(const FGuid& AnchorKey) const;
	bool TryGetAnchorKeyForPinId(FName PinId, FGuid& OutAnchorKey) const;

	/**
	 * @brief Single-anchor roles (player HQ, enemy HQ, wall) are restricted to this key during placement.
	 * @param Role Role to look up.
	 * @return Anchor key of the first pin with that role, or an invalid key when the role is not pinned.
	 */
	FGuid GetAnchorKeyForRole(EWorldPinnedPlaceRole Role) const;

	/** Mission types of every pinned mission in pin order; used to remove them from the seeded plan. */
	TArray<EMapMission> GetPinnedMissionTypes() const;

private:
	TArray<FWorldCampaignResolvedPin> M_Pins;

	// Lookup caches kept next to M_Pins because placement checks them inside hot candidate loops.
	TSet<FGuid> M_PinnedAnchorKeys;
	TSet<FGuid> M_PinnedMissionAnchorKeys;
};

/**
 * @brief Pure pin logic used by the generator, the async solver and the automation tests.
 */
namespace WorldCampaignPinnedPlaces
{
	/**
	 * @brief Derives the anchor key of a pin from its id so save/restore can find the pin without a saved pin id.
	 * @param PinId Configured pin id; compared case-insensitively like FName.
	 * @return Stable key, or an invalid key for NAME_None.
	 */
	RTS_SURVIVAL_API FGuid BuildPinnedAnchorKey(FName PinId);

	/**
	 * @brief Drops config entries that cannot be placed so later code can trust every pin.
	 * @param bUsePinnedPlaces Settings switch; false returns an empty list.
	 * @param ConfiguredPlaces Pins from UWorldCampaignMapContentSettings.
	 * @param OutWarnings One readable line per dropped entry.
	 * @return Usable pins in config order: unique ids, one pin per HQ/wall role, missions with a type.
	 */
	RTS_SURVIVAL_API TArray<FWorldCampaignPinnedPlace> BuildUsablePinnedPlaces(
		bool bUsePinnedPlaces,
		const TArray<FWorldCampaignPinnedPlace>& ConfiguredPlaces,
		TArray<FString>& OutWarnings);

	/**
	 * @brief Removes one plan entry per pinned mission so the seeded solver never places a story mission twice.
	 * @param Pins Resolved pins.
	 * @param InOutPlan Sorted mission placement plan; order of the remaining entries is kept.
	 */
	RTS_SURVIVAL_API void RemovePinnedMissionsFromPlan(const FWorldCampaignResolvedPins& Pins,
	                                                   TArray<EMapMission>& InOutPlan);

	/**
	 * @brief Writes pinned missions into a mission occupancy map before seeded placement starts.
	 * @param Pins Resolved pins.
	 * @param GetIsAnchorAvailable Returns true when the key belongs to an anchor of the current graph.
	 * @param InOutMissionsByAnchorKey Mission map of the placement state; existing entries are never replaced.
	 * @return Number of pinned missions written.
	 */
	RTS_SURVIVAL_API int32 ApplyPinnedMissionPreSeed(const FWorldCampaignResolvedPins& Pins,
	                                                 TFunctionRef<bool(const FGuid&)> GetIsAnchorAvailable,
	                                                 TMap<FGuid, EMapMission>& InOutMissionsByAnchorKey);

	/**
	 * @brief Candidate restriction for single-anchor roles, shared by both solvers so they pick the same anchor.
	 * @param SourceKeys Candidate anchor keys in source order.
	 * @param RolePinKey Pin key of the role being placed; invalid when the role is not pinned.
	 * @param PinnedAnchorKeys Keys reserved by any pin.
	 * @return Positions into SourceKeys to keep: only the first match of RolePinKey when the role is pinned,
	 *         otherwise every position whose key is not reserved by a pin.
	 */
	RTS_SURVIVAL_API TArray<int32> SelectCandidatePositionsForRole(const TArray<FGuid>& SourceKeys,
	                                                               const FGuid& RolePinKey,
	                                                               const TSet<FGuid>& PinnedAnchorKeys);

	/**
	 * @brief Mission hop spacing band; pinned missions only enforce the minimum so far story places never make
	 * seeded missions impossible.
	 * @param HopDistance Hops between the candidate and the other mission, INDEX_NONE when unreachable.
	 * @param MinHops Minimum spacing.
	 * @param MaxHopsClamped Maximum spacing, already clamped to at least MinHops.
	 * @param bOtherMissionIsPinned Whether the other mission is a pinned story mission.
	 * @return true when the candidate passes the band for this other mission.
	 */
	RTS_SURVIVAL_API bool PassesMissionHopSpacingBand(int32 HopDistance, int32 MinHops, int32 MaxHopsClamped,
	                                                  bool bOtherMissionIsPinned);

	/**
	 * @brief XY version of PassesMissionHopSpacingBand.
	 * @param XYDistance XY distance between the candidate and the other mission.
	 * @param MinXY Minimum spacing.
	 * @param MaxXY Maximum spacing.
	 * @param bOtherMissionIsPinned Whether the other mission is a pinned story mission.
	 * @return true when the candidate passes the band for this other mission.
	 */
	RTS_SURVIVAL_API bool PassesMissionXYSpacingBand(float XYDistance, float MinXY, float MaxXY,
	                                                 bool bOtherMissionIsPinned);
}
