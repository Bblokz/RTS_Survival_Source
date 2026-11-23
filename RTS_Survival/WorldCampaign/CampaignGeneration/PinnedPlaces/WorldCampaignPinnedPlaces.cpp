// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignGeneration/PinnedPlaces/WorldCampaignPinnedPlaces.h"

namespace WorldCampaignPinnedPlacesPrivate
{
	const TCHAR* const PinnedAnchorKeyPrefix = TEXT("WorldCampaign.PinnedAnchorKey|");
	constexpr uint64 PinnedAnchorKeySeedSalt = 0x5D3A8E1F6C2B9047ull;

	bool GetIsSingleAnchorRole(const EWorldPinnedPlaceRole Role)
	{
		return Role == EWorldPinnedPlaceRole::PlayerHQ
			|| Role == EWorldPinnedPlaceRole::EnemyHQ
			|| Role == EWorldPinnedPlaceRole::EnemyWall;
	}

	FString GetRoleName(const EWorldPinnedPlaceRole Role)
	{
		if (const UEnum* RoleEnum = StaticEnum<EWorldPinnedPlaceRole>())
		{
			return RoleEnum->GetNameStringByValue(static_cast<int64>(Role));
		}

		return FString::FromInt(static_cast<int32>(Role));
	}

	bool GetIsPinnedPlaceUsable(const FWorldCampaignPinnedPlace& Place, FString& OutReason)
	{
		if (Place.PinId.IsNone())
		{
			OutReason = TEXT("pin has no PinId");
			return false;
		}

		if (Place.Role == EWorldPinnedPlaceRole::None)
		{
			OutReason = FString::Printf(TEXT("pin %s has no role"), *Place.PinId.ToString());
			return false;
		}

		if (Place.Role == EWorldPinnedPlaceRole::Mission && Place.Mission == EMapMission::None)
		{
			OutReason = FString::Printf(TEXT("mission pin %s has no mission type"), *Place.PinId.ToString());
			return false;
		}

		return true;
	}
}

void FWorldCampaignResolvedPins::Reset()
{
	M_Pins.Reset();
	M_PinnedAnchorKeys.Reset();
	M_PinnedMissionAnchorKeys.Reset();
}

void FWorldCampaignResolvedPins::AddPin(const FWorldCampaignResolvedPin& Pin)
{
	if (not Pin.AnchorKey.IsValid() || M_PinnedAnchorKeys.Contains(Pin.AnchorKey))
	{
		return;
	}

	M_Pins.Add(Pin);
	M_PinnedAnchorKeys.Add(Pin.AnchorKey);
	if (Pin.Role == EWorldPinnedPlaceRole::Mission && Pin.Mission != EMapMission::None)
	{
		M_PinnedMissionAnchorKeys.Add(Pin.AnchorKey);
	}
}

bool FWorldCampaignResolvedPins::GetIsPinnedAnchorKey(const FGuid& AnchorKey) const
{
	return M_PinnedAnchorKeys.Contains(AnchorKey);
}

bool FWorldCampaignResolvedPins::GetIsPinnedMissionAnchorKey(const FGuid& AnchorKey) const
{
	return M_PinnedMissionAnchorKeys.Contains(AnchorKey);
}

FName FWorldCampaignResolvedPins::GetPinIdForAnchorKey(const FGuid& AnchorKey) const
{
	for (const FWorldCampaignResolvedPin& Pin : M_Pins)
	{
		if (Pin.AnchorKey == AnchorKey)
		{
			return Pin.PinId;
		}
	}

	return NAME_None;
}

bool FWorldCampaignResolvedPins::TryGetAnchorKeyForPinId(const FName PinId, FGuid& OutAnchorKey) const
{
	OutAnchorKey = FGuid();
	if (PinId.IsNone())
	{
		return false;
	}

	for (const FWorldCampaignResolvedPin& Pin : M_Pins)
	{
		if (Pin.PinId == PinId)
		{
			OutAnchorKey = Pin.AnchorKey;
			return true;
		}
	}

	return false;
}

FGuid FWorldCampaignResolvedPins::GetAnchorKeyForRole(const EWorldPinnedPlaceRole Role) const
{
	for (const FWorldCampaignResolvedPin& Pin : M_Pins)
	{
		if (Pin.Role == Role)
		{
			return Pin.AnchorKey;
		}
	}

	return FGuid();
}

TArray<EMapMission> FWorldCampaignResolvedPins::GetPinnedMissionTypes() const
{
	TArray<EMapMission> MissionTypes;
	for (const FWorldCampaignResolvedPin& Pin : M_Pins)
	{
		if (Pin.Role != EWorldPinnedPlaceRole::Mission || Pin.Mission == EMapMission::None)
		{
			continue;
		}

		MissionTypes.Add(Pin.Mission);
	}

	return MissionTypes;
}

namespace WorldCampaignPinnedPlaces
{
	FGuid BuildPinnedAnchorKey(const FName PinId)
	{
		if (PinId.IsNone())
		{
			return FGuid();
		}

		/*
		 * FName compares case-insensitively but its display string keeps the first-registered casing, so the
		 * key source is lower-cased to stay identical across sessions and saves.
		 */
		const FString KeySource = WorldCampaignPinnedPlacesPrivate::PinnedAnchorKeyPrefix + PinId.ToString().ToLower();
		return FGuid::NewDeterministicGuid(KeySource, WorldCampaignPinnedPlacesPrivate::PinnedAnchorKeySeedSalt);
	}

	TArray<FWorldCampaignPinnedPlace> BuildUsablePinnedPlaces(const bool bUsePinnedPlaces,
	                                                          const TArray<FWorldCampaignPinnedPlace>& ConfiguredPlaces,
	                                                          TArray<FString>& OutWarnings)
	{
		TArray<FWorldCampaignPinnedPlace> UsablePlaces;
		if (not bUsePinnedPlaces)
		{
			return UsablePlaces;
		}

		TSet<FName> UsedPinIds;
		TSet<EWorldPinnedPlaceRole> UsedSingleRoles;
		for (const FWorldCampaignPinnedPlace& Place : ConfiguredPlaces)
		{
			FString Reason;
			if (not WorldCampaignPinnedPlacesPrivate::GetIsPinnedPlaceUsable(Place, Reason))
			{
				OutWarnings.Add(FString::Printf(TEXT("Pinned place ignored: %s."), *Reason));
				continue;
			}

			if (UsedPinIds.Contains(Place.PinId))
			{
				OutWarnings.Add(FString::Printf(TEXT("Pinned place ignored: duplicate PinId %s."),
				                                *Place.PinId.ToString()));
				continue;
			}

			if (WorldCampaignPinnedPlacesPrivate::GetIsSingleAnchorRole(Place.Role)
				&& UsedSingleRoles.Contains(Place.Role))
			{
				OutWarnings.Add(FString::Printf(TEXT("Pinned place ignored: %s is a second %s pin."),
				                                *Place.PinId.ToString(),
				                                *WorldCampaignPinnedPlacesPrivate::GetRoleName(Place.Role)));
				continue;
			}

			UsedPinIds.Add(Place.PinId);
			if (WorldCampaignPinnedPlacesPrivate::GetIsSingleAnchorRole(Place.Role))
			{
				UsedSingleRoles.Add(Place.Role);
			}

			UsablePlaces.Add(Place);
		}

		return UsablePlaces;
	}

	void RemovePinnedMissionsFromPlan(const FWorldCampaignResolvedPins& Pins, TArray<EMapMission>& InOutPlan)
	{
		if (not Pins.HasAnyPins())
		{
			return;
		}

		for (const EMapMission PinnedMissionType : Pins.GetPinnedMissionTypes())
		{
			InOutPlan.RemoveSingle(PinnedMissionType);
		}
	}

	int32 ApplyPinnedMissionPreSeed(const FWorldCampaignResolvedPins& Pins,
	                                const TFunctionRef<bool(const FGuid&)> GetIsAnchorAvailable,
	                                TMap<FGuid, EMapMission>& InOutMissionsByAnchorKey)
	{
		int32 WrittenCount = 0;
		for (const FWorldCampaignResolvedPin& Pin : Pins.GetPins())
		{
			if (Pin.Role != EWorldPinnedPlaceRole::Mission || Pin.Mission == EMapMission::None)
			{
				continue;
			}

			if (InOutMissionsByAnchorKey.Contains(Pin.AnchorKey) || not GetIsAnchorAvailable(Pin.AnchorKey))
			{
				continue;
			}

			InOutMissionsByAnchorKey.Add(Pin.AnchorKey, Pin.Mission);
			WrittenCount++;
		}

		return WrittenCount;
	}

	TArray<int32> SelectCandidatePositionsForRole(const TArray<FGuid>& SourceKeys,
	                                              const FGuid& RolePinKey,
	                                              const TSet<FGuid>& PinnedAnchorKeys)
	{
		TArray<int32> KeptPositions;
		if (RolePinKey.IsValid())
		{
			const int32 PinPosition = SourceKeys.IndexOfByKey(RolePinKey);
			if (PinPosition != INDEX_NONE)
			{
				KeptPositions.Add(PinPosition);
			}

			return KeptPositions;
		}

		KeptPositions.Reserve(SourceKeys.Num());
		for (int32 Position = 0; Position < SourceKeys.Num(); Position++)
		{
			if (PinnedAnchorKeys.Contains(SourceKeys[Position]))
			{
				continue;
			}

			KeptPositions.Add(Position);
		}

		return KeptPositions;
	}

	bool PassesMissionHopSpacingBand(const int32 HopDistance, const int32 MinHops, const int32 MaxHopsClamped,
	                                 const bool bOtherMissionIsPinned)
	{
		if (HopDistance == INDEX_NONE)
		{
			return bOtherMissionIsPinned;
		}

		if (HopDistance < MinHops)
		{
			return false;
		}

		return bOtherMissionIsPinned || HopDistance <= MaxHopsClamped;
	}

	bool PassesMissionXYSpacingBand(const float XYDistance, const float MinXY, const float MaxXY,
	                                const bool bOtherMissionIsPinned)
	{
		if (XYDistance < MinXY)
		{
			return false;
		}

		return bOtherMissionIsPinned || XYDistance <= MaxXY;
	}
}
