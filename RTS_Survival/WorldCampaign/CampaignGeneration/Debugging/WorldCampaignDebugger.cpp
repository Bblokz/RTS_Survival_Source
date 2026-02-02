// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignGeneration/Debugging/WorldCampaignDebugger.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

namespace
{
	FString GetEnemyItemName(EMapEnemyItem EnemyType)
	{
		const UEnum* EnemyEnum = StaticEnum<EMapEnemyItem>();
		return EnemyEnum ? EnemyEnum->GetNameStringByValue(static_cast<int64>(EnemyType)) : TEXT("EnemyItem");
	}

	FString GetNeutralItemName(EMapNeutralObjectType NeutralType)
	{
		const UEnum* NeutralEnum = StaticEnum<EMapNeutralObjectType>();
		return NeutralEnum ? NeutralEnum->GetNameStringByValue(static_cast<int64>(NeutralType)) : TEXT("NeutralItem");
	}

	FString GetMissionName(EMapMission MissionType)
	{
		const UEnum* MissionEnum = StaticEnum<EMapMission>();
		return MissionEnum ? MissionEnum->GetNameStringByValue(static_cast<int64>(MissionType)) : TEXT("Mission");
	}
}

UWorldCampaignDebugger::UWorldCampaignDebugger()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldCampaignDebugger::ClearAllDebugState()
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		M_DebugStateByAnchorKey.Reset();
		M_DebugStateByAnchorPointer.Reset();
	}
}

void UWorldCampaignDebugger::DrawAcceptedAtAnchor(AAnchorPoint* AnchorPoint, const FString& Text, FColor Color)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		DrawInfoAtAnchor(AnchorPoint, Text, DisplayTimeAcceptedLocation, Color);
	}
}

void UWorldCampaignDebugger::DrawRejectedAtAnchor(AAnchorPoint* AnchorPoint, const FString& Text, FColor Color)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		DrawInfoAtAnchor(AnchorPoint, Text, DisplayTimeRejectedLocation, Color);
	}
}

void UWorldCampaignDebugger::DrawInfoAtAnchor(AAnchorPoint* AnchorPoint, const FString& Text, float DurationOverride,
                                              FColor Color)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		if (not IsValid(AnchorPoint))
		{
			return;
		}

		UWorld* World = GetWorld();
		if (not IsValid(World))
		{
			return;
		}

		const float HeightOffset = GetStackedHeightOffset(AnchorPoint, DurationOverride);
		const FVector AnchorLocation = AnchorPoint->GetActorLocation();
		RTSFunctionLibrary::DrawDebugAtLocation(this, AnchorLocation + FVector(0.f, 0.f, HeightOffset), Text, Color,
			                                    DurationOverride);
	}
}

void UWorldCampaignDebugger::DebugEnemyPlacementAccepted(AAnchorPoint* AnchorPoint,
                                                         const FWorldCampaignEnemyPlacementDebugInfo& Info)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		TArray<FString> Parts;
		Parts.Add(FString::Printf(TEXT("Enemy %s"), *GetEnemyItemName(Info.EnemyType)));

		if (bDebugEnemyHQHops)
		{
			Parts.Add(Info.HopFromEnemyHQ == INDEX_NONE
				          ? TEXT("EnemyHQ:?")
				          : FString::Printf(TEXT("EnemyHQ:%d"), Info.HopFromEnemyHQ));
		}

		if (bDebugPlayerHQHops && Info.bSafeZoneRelevant)
		{
			Parts.Add(Info.HopFromPlayerHQ == INDEX_NONE
				          ? TEXT("PlayerHQ:?")
				          : FString::Printf(TEXT("PlayerHQ:%d"), Info.HopFromPlayerHQ));
		}

		if (bDebugAnchorDegree && Info.bIncludeAnchorDegree)
		{
			Parts.Add(Info.AnchorDegree == INDEX_NONE
				          ? TEXT("Degree:?")
				          : FString::Printf(TEXT("Degree:%d"), Info.AnchorDegree));
		}

		if (bDisplayVariationEnemyObjectPlacement && Info.bHasVariant)
		{
			Parts.Add(FString::Printf(TEXT("Variant %d/%d"), Info.VariantIndex + 1, Info.VariantCount));
		}

		if (bDisplayHopsFromSameEnemyItems && Info.MinSameTypeHopDistance != INDEX_NONE)
		{
			Parts.Add(FString::Printf(TEXT("MinSame:%d"), Info.MinSameTypeHopDistance));
		}

		DrawAcceptedAtAnchor(AnchorPoint, FString::Join(Parts, TEXT(" | ")));
	}
}

void UWorldCampaignDebugger::DebugEnemyPlacementRejected(AAnchorPoint* AnchorPoint, EMapEnemyItem EnemyType,
                                                         const FString& Reason)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		const FString Text = FString::Printf(TEXT("Enemy %s: %s"), *GetEnemyItemName(EnemyType), *Reason);
		DrawRejectedAtAnchor(AnchorPoint, Text, FColor::Red);
	}
}

void UWorldCampaignDebugger::DebugNeutralPlacementAccepted(AAnchorPoint* AnchorPoint,
                                                           const FWorldCampaignNeutralPlacementDebugInfo& Info)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		TArray<FString> Parts;
		Parts.Add(FString::Printf(TEXT("Neutral %s"), *GetNeutralItemName(Info.NeutralType)));

		if (bDebugPlayerHQHops)
		{
			Parts.Add(Info.HopFromPlayerHQ == INDEX_NONE
				          ? TEXT("PlayerHQ:?")
				          : FString::Printf(TEXT("PlayerHQ:%d"), Info.HopFromPlayerHQ));
		}

		if (bDisplayHopsFromOtherNeutralItems && Info.MinHopFromOtherNeutral != INDEX_NONE)
		{
			Parts.Add(FString::Printf(TEXT("MinNeutral:%d"), Info.MinHopFromOtherNeutral));
		}

		DrawAcceptedAtAnchor(AnchorPoint, FString::Join(Parts, TEXT(" | ")));
	}
}

void UWorldCampaignDebugger::DebugNeutralPlacementRejected(AAnchorPoint* AnchorPoint,
                                                           EMapNeutralObjectType NeutralType,
                                                           const FString& Reason)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		const FString Text = FString::Printf(TEXT("Neutral %s: %s"), *GetNeutralItemName(NeutralType), *Reason);
		DrawRejectedAtAnchor(AnchorPoint, Text, FColor::Red);
	}
}

void UWorldCampaignDebugger::DebugMissionPlacementAccepted(AAnchorPoint* AnchorPoint,
                                                           const FWorldCampaignMissionPlacementDebugInfo& Info)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		TArray<FString> Parts;
		Parts.Add(FString::Printf(TEXT("Mission %s"), *GetMissionName(Info.MissionType)));

		if (bDisplayHopsFromHQForMissions)
		{
			Parts.Add(Info.HopFromHQ == INDEX_NONE
				          ? TEXT("HQ:?")
				          : FString::Printf(TEXT("HQ:%d"), Info.HopFromHQ));
		}

		if (bDebugMissionSpacingHops && Info.bUsesMissionSpacingHops)
		{
			Parts.Add(Info.NearestMissionHopDistance < 0.f
				          ? TEXT("NearestMission:?")
				          : FString::Printf(TEXT("NearestMission:%.0f"), Info.NearestMissionHopDistance));
		}

		if (bDisplayMinMaxConnectionsForMissionPlacement && Info.bUsesConnectionRules)
		{
			Parts.Add(FString::Printf(TEXT("Connections:%d-%d"), Info.MinConnections, Info.MaxConnections));
		}

		if (bDisplayMissionAdjacencyRequirements && Info.bHasAdjacencyRequirement)
		{
			Parts.Add(Info.AdjacencySummary);
		}

		if (bDisplayNeutralItemRequirementForMission && Info.bHasNeutralRequirement)
		{
			Parts.Add(FString::Printf(TEXT("NeedsNeutral:%s"), *GetNeutralItemName(Info.RequiredNeutralType)));
		}

		DrawAcceptedAtAnchor(AnchorPoint, FString::Join(Parts, TEXT(" | ")));
	}
}

void UWorldCampaignDebugger::DebugMissionPlacementFailed(AAnchorPoint* AnchorPoint, EMapMission MissionType,
                                                         const FString& Reason)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		if (not bDebugFailedMissionPlacement)
		{
			return;
		}

		const FString Text = FString::Printf(TEXT("Mission %s: %s"), *GetMissionName(MissionType), *Reason);
		DrawRejectedAtAnchor(AnchorPoint, Text, FColor::Red);
	}
}

float UWorldCampaignDebugger::GetStackedHeightOffset(AAnchorPoint* AnchorPoint, float Duration)
{
	if (not IsValid(AnchorPoint))
	{
		return DefaultDebugHeightOffset;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return DefaultDebugHeightOffset;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
	if (AnchorKey.IsValid())
	{
		FWorldCampaignAnchorDebugStackState& State = M_DebugStateByAnchorKey.FindOrAdd(AnchorKey);
		return UpdateStackState(State, CurrentTimeSeconds, Duration);
	}

	const TWeakObjectPtr<AAnchorPoint> AnchorPointer = AnchorPoint;
	FWorldCampaignAnchorDebugStackState& State = M_DebugStateByAnchorPointer.FindOrAdd(AnchorPointer);
	return UpdateStackState(State, CurrentTimeSeconds, Duration);
}

float UWorldCampaignDebugger::UpdateStackState(FWorldCampaignAnchorDebugStackState& State, float CurrentTimeSeconds,
                                               float Duration)
{
	if (CurrentTimeSeconds < State.LastEndTimeSeconds)
	{
		State.StackIndex++;
	}
	else
	{
		State.StackIndex = 0;
	}

	State.LastEndTimeSeconds = CurrentTimeSeconds + Duration;
	return DefaultDebugHeightOffset + AddedHeightIfStillDisplaying * State.StackIndex;
}
