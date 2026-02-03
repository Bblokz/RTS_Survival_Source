// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignGeneration/Debugging/WorldCampaignDebugger.h"

#include "Algo/Sort.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

namespace
{
	constexpr int32 PlacementReportMaxChunkChars = 650;
	constexpr int32 PlacementReportRateDivisorMin = 1;

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

	FString GetTopologyPreferenceName(ETopologySearchStrategy Preference)
	{
		const UEnum* PreferenceEnum = StaticEnum<ETopologySearchStrategy>();
		return PreferenceEnum
			       ? PreferenceEnum->GetNameStringByValue(static_cast<int64>(Preference))
			       : TEXT("Preference");
	}

	FString FormatRemovalRate(const int32 NumRemoved, const int32 NumPlaced)
	{
		const int32 SafePlacedCount = FMath::Max(PlacementReportRateDivisorMin, NumPlaced);
		const float RemovalRate = 100.f * static_cast<float>(NumRemoved) / static_cast<float>(SafePlacedCount);
		return FString::Printf(TEXT("%.0f%%"), RemovalRate);
	}

	FString FormatAverageAttempts(const int32 TotalPlacementAttempts, const int32 SuccessfulPlacements)
	{
		const int32 SafeSuccessfulPlacements = FMath::Max(PlacementReportRateDivisorMin, SuccessfulPlacements);
		const float AverageAttempts = static_cast<float>(TotalPlacementAttempts)
		                              / static_cast<float>(SafeSuccessfulPlacements);
		return FString::Printf(TEXT("%.1f"), AverageAttempts);
	}

	void AppendLineToChunks(const FString& Line, TArray<FString>& OutChunks)
	{
		if (OutChunks.Num() == 0)
		{
			OutChunks.Add(Line);
			return;
		}

		FString& CurrentChunk = OutChunks.Last();
		const int32 AdditionalCharacters = Line.Len() + 1;
		if (CurrentChunk.Len() + AdditionalCharacters > PlacementReportMaxChunkChars)
		{
			OutChunks.Add(Line);
			return;
		}

		CurrentChunk.Append(TEXT("\n"));
		CurrentChunk.Append(Line);
	}

	template <typename TStats>
	void IncrementRemovalBucket(TStats& Stats, const ECampaignGenerationStep FailureStep)
	{
		if (FailureStep == ECampaignGenerationStep::MissionsPlaced)
		{
			Stats.RemovedDueToMissions++;
			return;
		}

		if (FailureStep == ECampaignGenerationStep::NeutralObjectsPlaced)
		{
			Stats.RemovedDueToNeutralPlacement++;
			return;
		}

		if (FailureStep == ECampaignGenerationStep::EnemyObjectsPlaced
			|| FailureStep == ECampaignGenerationStep::EnemyWallPlaced)
		{
			Stats.RemovedDueToEnemyPlacement++;
			return;
		}

		if (FailureStep == ECampaignGenerationStep::EnemyHQPlaced
			|| FailureStep == ECampaignGenerationStep::PlayerHQPlaced)
		{
			Stats.RemovedDueToHQPlacement++;
		}
	}

	template <typename TKey, typename TStats>
	bool GetHasReportActivity(const TKey Key, const TMap<TKey, TStats>& ReportMap)
	{
		const TStats* StatsPtr = ReportMap.Find(Key);
		if (StatsPtr == nullptr)
		{
			return false;
		}

		return StatsPtr->NumPlaced > 0 || StatsPtr->NumRemoved > 0 || StatsPtr->TotalPlacementAttempts > 0;
	}

	int32 SumTotalAttempts(const TMap<EMapEnemyItem, FEnemyReportStats>& EnemyReport,
	                       const TMap<EMapMission, FMissionReportStats>& MissionReport)
	{
		int32 TotalPlacementAttempts = 0;
		for (const TPair<EMapEnemyItem, FEnemyReportStats>& Entry : EnemyReport)
		{
			TotalPlacementAttempts += Entry.Value.TotalPlacementAttempts;
		}

		for (const TPair<EMapMission, FMissionReportStats>& Entry : MissionReport)
		{
			TotalPlacementAttempts += Entry.Value.TotalPlacementAttempts;
		}

		return TotalPlacementAttempts;
	}

	int32 SumTotalBacktracks(const TMap<EMapEnemyItem, FEnemyReportStats>& EnemyReport,
	                         const TMap<EMapMission, FMissionReportStats>& MissionReport)
	{
		int32 TotalBacktracks = 0;
		for (const TPair<EMapEnemyItem, FEnemyReportStats>& Entry : EnemyReport)
		{
			TotalBacktracks += Entry.Value.NumTimesCausedBacktrack;
		}

		for (const TPair<EMapMission, FMissionReportStats>& Entry : MissionReport)
		{
			TotalBacktracks += Entry.Value.NumTimesCausedBacktrack;
		}

		return TotalBacktracks;
	}
}

UWorldCampaignDebugger::UWorldCampaignDebugger()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldCampaignDebugger::ApplySettingsFromGenerator(const AGeneratorWorldCampaign& Generator)
{
	DefaultDebugHeightOffset = Generator.M_DefaultDebugHeightOffset;
	AddedHeightIfStillDisplaying = Generator.M_AddedHeightIfStillDisplaying;
	DisplayTimeRejectedLocation = Generator.M_DisplayTimeRejectedLocation;
	DisplayTimeAcceptedLocation = Generator.M_DisplayTimeAcceptedLocation;
	MaxRejectionDrawsPerAttempt = Generator.M_MaxRejectionDrawsPerAttempt;
	MaxRejectionDrawsPerReason = Generator.M_MaxRejectionDrawsPerReason;
	bDebugAnchorDegree = Generator.bM_DebugAnchorDegree;
	bDebugPlayerHQHops = Generator.bM_DebugPlayerHQHops;
	bDebugEnemyHQHops = Generator.bM_DebugEnemyHQHops;
	bDisplayVariationEnemyObjectPlacement = Generator.bM_DisplayVariationEnemyObjectPlacement;
	bDisplayHopsFromSameEnemyItems = Generator.bM_DisplayHopsFromSameEnemyItems;
	bDisplayHopsFromOtherNeutralItems = Generator.bM_DisplayHopsFromOtherNeutralItems;
	bDebugFailedMissionPlacement = Generator.bM_DebugFailedMissionPlacement;
	bDebugMissionCandidateRejections = Generator.bM_DebugMissionCandidateRejections;
	bDebugEnemyCandidateRejections = Generator.bM_DebugEnemyCandidateRejections;
	bDebugNeutralCandidateRejections = Generator.bM_DebugNeutralCandidateRejections;
	bDisplayHopsFromHQForMissions = Generator.bM_DisplayHopsFromHQForMissions;
	bDebugMissionSpacingHops = Generator.bM_DebugMissionSpacingHops;
	bDisplayMinMaxConnectionsForMissionPlacement = Generator.bM_DisplayMinMaxConnectionsForMissionPlacement;
	bDisplayMissionAdjacencyRequirements = Generator.bM_DisplayMissionAdjacencyRequirements;
	bDisplayNeutralItemRequirementForMission = Generator.bM_DisplayNeutralItemRequirementForMission;
}

void UWorldCampaignDebugger::ClearAllDebugState()
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		M_RejectionStateByStep.Reset();
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
		const bool bHasEnemyPrefix = Text.StartsWith(TEXT("Enemy "));
		const bool bHasNeutralPrefix = Text.StartsWith(TEXT("Neutral "));
		const bool bHasMissionPrefix = Text.StartsWith(TEXT("Mission "));

		EWorldCampaignRejectionItemType ItemType = EWorldCampaignRejectionItemType::Mission;
		ECampaignGenerationStep Step = ECampaignGenerationStep::MissionsPlaced;
		bool bShouldDrawByToggle = true;

		if (bHasEnemyPrefix)
		{
			ItemType = EWorldCampaignRejectionItemType::Enemy;
			Step = ECampaignGenerationStep::EnemyObjectsPlaced;
			bShouldDrawByToggle = bDebugEnemyCandidateRejections;
		}
		else if (bHasNeutralPrefix)
		{
			ItemType = EWorldCampaignRejectionItemType::Neutral;
			Step = ECampaignGenerationStep::NeutralObjectsPlaced;
			bShouldDrawByToggle = bDebugNeutralCandidateRejections;
		}
		else if (bHasMissionPrefix)
		{
			ItemType = EWorldCampaignRejectionItemType::Mission;
			Step = ECampaignGenerationStep::MissionsPlaced;
			bShouldDrawByToggle = true;
		}
		else
		{
			ItemType = EWorldCampaignRejectionItemType::Mission;
			Step = ECampaignGenerationStep::MissionsPlaced;
			bShouldDrawByToggle = bDebugMissionCandidateRejections;
		}

		if (not bShouldDrawByToggle)
		{
			return;
		}

		if (not CanDrawRejectionForAttempt(Step, ItemType, Text))
		{
			return;
		}

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
		if (not bDebugEnemyCandidateRejections)
		{
			return;
		}

		if (not CanDrawRejectionForAttempt(ECampaignGenerationStep::EnemyObjectsPlaced,
		                                   EWorldCampaignRejectionItemType::Enemy, Reason))
		{
			return;
		}

		const FString Text = FString::Printf(TEXT("Enemy %s: %s"), *GetEnemyItemName(EnemyType), *Reason);
		DrawInfoAtAnchor(AnchorPoint, Text, DisplayTimeRejectedLocation, FColor::Red);
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
		if (not bDebugNeutralCandidateRejections)
		{
			return;
		}

		if (not CanDrawRejectionForAttempt(ECampaignGenerationStep::NeutralObjectsPlaced,
		                                   EWorldCampaignRejectionItemType::Neutral, Reason))
		{
			return;
		}

		const FString Text = FString::Printf(TEXT("Neutral %s: %s"), *GetNeutralItemName(NeutralType), *Reason);
		DrawInfoAtAnchor(AnchorPoint, Text, DisplayTimeRejectedLocation, FColor::Red);
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

		if (Info.bUsesOverrideArray)
		{
			Parts.Add(TEXT("OverrideArray"));

			if (Info.bOverrideArrayUsesConnectionBounds)
			{
				Parts.Add(FString::Printf(TEXT("Conn:%d-%d"), Info.OverrideMinConnections,
				                          Info.OverrideMaxConnections));
			}

			if (Info.bOverrideArrayUsesHopsBounds)
			{
				Parts.Add(FString::Printf(TEXT("Hops:%d-%d"), Info.OverrideMinHopsFromHQ, Info.OverrideMaxHopsFromHQ));
			}

			if (Info.OverrideConnectionPreference != ETopologySearchStrategy::NotSet)
			{
				Parts.Add(FString::Printf(TEXT("PrefConn:%s"),
				                          *GetTopologyPreferenceName(Info.OverrideConnectionPreference)));
			}

			if (Info.OverrideHopsPreference != ETopologySearchStrategy::NotSet)
			{
				Parts.Add(FString::Printf(TEXT("PrefHops:%s"),
				                          *GetTopologyPreferenceName(Info.OverrideHopsPreference)));
			}
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

		if (not CanDrawRejectionForAttempt(ECampaignGenerationStep::MissionsPlaced,
		                                   EWorldCampaignRejectionItemType::Mission, Reason))
		{
			return;
		}

		const FString Text = FString::Printf(TEXT("Mission %s: %s"), *GetMissionName(MissionType), *Reason);
		DrawInfoAtAnchor(AnchorPoint, Text, DisplayTimeRejectedLocation, FColor::Red);
	}
}

void UWorldCampaignDebugger::Report_ResetPlacementReport()
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		M_EnemyReport.Reset();
		M_MissionReport.Reset();
		M_NeutralReport.Reset();
	}
}

void UWorldCampaignDebugger::Report_OnAttemptEnemy(const EMapEnemyItem Type)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		FEnemyReportStats& Stats = M_EnemyReport.FindOrAdd(Type);
		Stats.TotalPlacementAttempts++;
		Stats.AttemptsSinceLastSuccess++;
	}
}

void UWorldCampaignDebugger::Report_OnPlacedEnemy(const EMapEnemyItem Type)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		FEnemyReportStats& Stats = M_EnemyReport.FindOrAdd(Type);
		const int32 AttemptsUsed = FMath::Max(PlacementReportRateDivisorMin, Stats.AttemptsSinceLastSuccess);
		Stats.SuccessfulPlacements++;
		Stats.NumPlaced++;
		Stats.MaxAttemptsForSuccess = FMath::Max(Stats.MaxAttemptsForSuccess, AttemptsUsed);
		Stats.AttemptsSinceLastSuccess = 0;
	}
}

void UWorldCampaignDebugger::Report_OnAttemptMission(const EMapMission Type)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		FMissionReportStats& Stats = M_MissionReport.FindOrAdd(Type);
		Stats.TotalPlacementAttempts++;
		Stats.AttemptsSinceLastSuccess++;
	}
}

void UWorldCampaignDebugger::Report_OnPlacedMission(const EMapMission Type)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		FMissionReportStats& Stats = M_MissionReport.FindOrAdd(Type);
		const int32 AttemptsUsed = FMath::Max(PlacementReportRateDivisorMin, Stats.AttemptsSinceLastSuccess);
		Stats.SuccessfulPlacements++;
		Stats.NumPlaced++;
		Stats.MaxAttemptsForSuccess = FMath::Max(Stats.MaxAttemptsForSuccess, AttemptsUsed);
		Stats.AttemptsSinceLastSuccess = 0;
	}
}

void UWorldCampaignDebugger::Report_OnUndoneMicro(const FCampaignGenerationStepTransaction& Transaction,
                                                  const ECampaignGenerationStep FailureStep)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (not Transaction.bIsMicroTransaction)
		{
			return;
		}

		if (Transaction.MicroItemType == EMapItemType::EnemyItem)
		{
			const EMapEnemyItem EnemyType = Transaction.MicroEnemyItemType;
			if (EnemyType == EMapEnemyItem::None)
			{
				return;
			}

			FEnemyReportStats& Stats = M_EnemyReport.FindOrAdd(EnemyType);
			Stats.NumRemoved++;
			Stats.NumTimesCausedBacktrack++;
			IncrementRemovalBucket(Stats, FailureStep);
			return;
		}

		if (Transaction.MicroItemType == EMapItemType::Mission)
		{
			const EMapMission MissionType = Transaction.MicroMissionType;
			if (MissionType == EMapMission::None)
			{
				return;
			}

			FMissionReportStats& Stats = M_MissionReport.FindOrAdd(MissionType);
			Stats.NumRemoved++;
			Stats.NumTimesCausedBacktrack++;
			IncrementRemovalBucket(Stats, FailureStep);
		}
	}
}

void UWorldCampaignDebugger::Report_PrintPlacementReport(const AGeneratorWorldCampaign& Generator) const
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		const bool bWasSuccessful = Generator.M_GenerationStep == ECampaignGenerationStep::Finished;
		const int32 TotalPlacementAttempts = SumTotalAttempts(M_EnemyReport, M_MissionReport);
		const int32 TotalBacktracks = SumTotalBacktracks(M_EnemyReport, M_MissionReport);
		const FString ResultLabel = bWasSuccessful ? TEXT("FINISHED") : TEXT("FAILED");
		const FString HeaderLine = FString::Printf(
			TEXT("Placement Report %s | Seed=%d | TotalAttempts=%d | Backtracks=%d"),
			*ResultLabel,
			Generator.M_PlacementState.SeedUsed,
			TotalPlacementAttempts,
			TotalBacktracks);

		TArray<FString> Lines;
		Lines.Add(HeaderLine);

		TArray<EMapEnemyItem> EnemyKeys;
		M_EnemyReport.GetKeys(EnemyKeys);
		Algo::Sort(EnemyKeys, [](const EMapEnemyItem Left, const EMapEnemyItem Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		});

		Lines.Add(TEXT("Enemies:"));
		bool bHasEnemyLines = false;
		for (const EMapEnemyItem EnemyType : EnemyKeys)
		{
			if (not GetHasReportActivity(EnemyType, M_EnemyReport))
			{
				continue;
			}

			const FEnemyReportStats& Stats = M_EnemyReport.FindChecked(EnemyType);
			const int32 NetPlaced = Stats.NumPlaced - Stats.NumRemoved;
			const FString RemovalRateText = FormatRemovalRate(Stats.NumRemoved, Stats.NumPlaced);
			const FString AverageAttemptsText = FormatAverageAttempts(Stats.TotalPlacementAttempts,
			                                                         Stats.SuccessfulPlacements);
			const FString Line = FString::Printf(
				TEXT("Enemy %s: placed=%d removed=%d net=%d rate=%s caused=%d avgTry=%s maxTry=%d ")
				TEXT("removedBy(Mis/Neu/En/HQ)=%d/%d/%d/%d"),
				*GetEnemyItemName(EnemyType),
				Stats.NumPlaced,
				Stats.NumRemoved,
				NetPlaced,
				*RemovalRateText,
				Stats.NumTimesCausedBacktrack,
				*AverageAttemptsText,
				Stats.MaxAttemptsForSuccess,
				Stats.RemovedDueToMissions,
				Stats.RemovedDueToNeutralPlacement,
				Stats.RemovedDueToEnemyPlacement,
				Stats.RemovedDueToHQPlacement);
			Lines.Add(Line);
			bHasEnemyLines = true;
		}

		if (not bHasEnemyLines)
		{
			Lines.Add(TEXT("Enemy items: no activity recorded."));
		}

		TArray<EMapMission> MissionKeys;
		M_MissionReport.GetKeys(MissionKeys);
		Algo::Sort(MissionKeys, [](const EMapMission Left, const EMapMission Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		});

		Lines.Add(TEXT("Missions:"));
		bool bHasMissionLines = false;
		for (const EMapMission MissionType : MissionKeys)
		{
			if (not GetHasReportActivity(MissionType, M_MissionReport))
			{
				continue;
			}

			const FMissionReportStats& Stats = M_MissionReport.FindChecked(MissionType);
			const int32 NetPlaced = Stats.NumPlaced - Stats.NumRemoved;
			const FString RemovalRateText = FormatRemovalRate(Stats.NumRemoved, Stats.NumPlaced);
			const FString AverageAttemptsText = FormatAverageAttempts(Stats.TotalPlacementAttempts,
			                                                         Stats.SuccessfulPlacements);
			const FString Line = FString::Printf(
				TEXT("Mission %s: placed=%d removed=%d net=%d rate=%s caused=%d avgTry=%s maxTry=%d ")
				TEXT("removedBy(Mis/Neu/En/HQ)=%d/%d/%d/%d"),
				*GetMissionName(MissionType),
				Stats.NumPlaced,
				Stats.NumRemoved,
				NetPlaced,
				*RemovalRateText,
				Stats.NumTimesCausedBacktrack,
				*AverageAttemptsText,
				Stats.MaxAttemptsForSuccess,
				Stats.RemovedDueToMissions,
				Stats.RemovedDueToNeutralPlacement,
				Stats.RemovedDueToEnemyPlacement,
				Stats.RemovedDueToHQPlacement);
			Lines.Add(Line);
			bHasMissionLines = true;
		}

		if (not bHasMissionLines)
		{
			Lines.Add(TEXT("Missions: no activity recorded."));
		}

		Lines.Add(TEXT("Neutrals: not tracked in this report."));

		TArray<FString> Chunks;
		for (const FString& Line : Lines)
		{
			AppendLineToChunks(Line, Chunks);
		}

		const int32 TotalPages = Chunks.Num();
		if (TotalPages > 1)
		{
			for (int32 PageIndex = 0; PageIndex < TotalPages; PageIndex++)
			{
				Chunks[PageIndex].Append(TEXT("\n"));
				Chunks[PageIndex].Append(
					FString::Printf(TEXT("Page (%d/%d)"), PageIndex + 1, TotalPages));
			}
		}

		for (const FString& Chunk : Chunks)
		{
			RTSFunctionLibrary::DisplayNotification(Chunk);
		}
	}
}

bool UWorldCampaignDebugger::CanDrawRejectionForAttempt(ECampaignGenerationStep Step,
                                                        EWorldCampaignRejectionItemType ItemType,
                                                        const FString& Reason)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		const int32 AttemptIndex = GetAttemptIndexForStep(Step);
		FWorldCampaignRejectionAttemptState& AttemptState = M_RejectionStateByStep.FindOrAdd(Step);
		if (AttemptState.AttemptIndex != AttemptIndex)
		{
			AttemptState.AttemptIndex = AttemptIndex;
			AttemptState.TotalRejections = 0;
			AttemptState.RejectionsByReason.Reset();
		}

		if (MaxRejectionDrawsPerAttempt > 0 && AttemptState.TotalRejections >= MaxRejectionDrawsPerAttempt)
		{
			return false;
		}

		if (MaxRejectionDrawsPerReason > 0)
		{
			const FWorldCampaignRejectionKey Key{ItemType, Reason};
			int32& ReasonCount = AttemptState.RejectionsByReason.FindOrAdd(Key);
			if (ReasonCount >= MaxRejectionDrawsPerReason)
			{
				return false;
			}

			ReasonCount++;
		}

		AttemptState.TotalRejections++;
		return true;
	}
	else
	{
		return true;
	}
}

int32 UWorldCampaignDebugger::GetAttemptIndexForStep(ECampaignGenerationStep Step) const
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		const AGeneratorWorldCampaign* Generator = Cast<AGeneratorWorldCampaign>(GetOwner());
		if (not IsValid(Generator))
		{
			return INDEX_NONE;
		}

		return Generator->GetStepAttemptIndex(Step);
	}
	else
	{
		return INDEX_NONE;
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
