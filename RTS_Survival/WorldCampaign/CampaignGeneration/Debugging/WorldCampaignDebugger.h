// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/GenerationStep/Enum_CampaignGenerationStep.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"
#include "WorldCampaignDebugger.generated.h"

class AAnchorPoint;

struct FWorldCampaignEnemyPlacementDebugInfo
{
	EMapEnemyItem EnemyType = EMapEnemyItem::EnemyHQ;
	int32 HopFromEnemyHQ = INDEX_NONE;
	int32 HopFromPlayerHQ = INDEX_NONE;
	int32 AnchorDegree = INDEX_NONE;
	int32 MinSameTypeHopDistance = INDEX_NONE;
	int32 VariantIndex = INDEX_NONE;
	int32 VariantCount = 0;
	bool bSafeZoneRelevant = false;
	bool bIncludeAnchorDegree = false;
	bool bHasVariant = false;
};

struct FWorldCampaignNeutralPlacementDebugInfo
{
	EMapNeutralObjectType NeutralType = EMapNeutralObjectType::None;
	int32 HopFromPlayerHQ = INDEX_NONE;
	int32 MinHopFromOtherNeutral = INDEX_NONE;
};

struct FWorldCampaignMissionPlacementDebugInfo
{
	EMapMission MissionType = EMapMission::None;
	int32 HopFromHQ = INDEX_NONE;
	float NearestMissionHopDistance = -1.f;
	int32 MinConnections = 0;
	int32 MaxConnections = 0;
	int32 OverrideMinConnections = 0;
	int32 OverrideMaxConnections = 0;
	int32 OverrideMinHopsFromHQ = 0;
	int32 OverrideMaxHopsFromHQ = 0;
	bool bUsesConnectionRules = false;
	bool bUsesMissionSpacingHops = false;
	bool bHasAdjacencyRequirement = false;
	bool bHasNeutralRequirement = false;
	bool bUsesOverrideArray = false;
	bool bOverrideArrayUsesConnectionBounds = false;
	bool bOverrideArrayUsesHopsBounds = false;
	ETopologySearchStrategy OverrideConnectionPreference = ETopologySearchStrategy::NotSet;
	ETopologySearchStrategy OverrideHopsPreference = ETopologySearchStrategy::NotSet;
	FString AdjacencySummary;
	EMapNeutralObjectType RequiredNeutralType = EMapNeutralObjectType::None;
};

/**
 * @brief Add to the world campaign generator blueprint to control in-world text debugging.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldCampaignDebugger : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldCampaignDebugger();

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "Base Z offset for debug text drawn at anchors. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	float DefaultDebugHeightOffset = 300.0f;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "Additional Z offset per stacked message while previous debug text is still displaying. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	float AddedHeightIfStillDisplaying = 200.0f;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "Display time for rejected placement debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	float DisplayTimeRejectedLocation = 3.0f;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "Display time for accepted placement debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	float DisplayTimeAcceptedLocation = 5.0f;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "Maximum total rejection debug draws per placement attempt. Use <= 0 for unlimited. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	int32 MaxRejectionDrawsPerAttempt = 200;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "Maximum rejection debug draws per reason per placement attempt. Use <= 0 for unlimited. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	int32 MaxRejectionDrawsPerReason = 25;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, show connection degree details where relevant. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDebugAnchorDegree = false;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, include player HQ hop distances in debug strings where applicable. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDebugPlayerHQHops = false;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, include enemy HQ hop distances in enemy placement debug strings. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDebugEnemyHQHops = false;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, show variant selection info for enemy placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDisplayVariationEnemyObjectPlacement = true;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, show hop distance to nearest same enemy type after placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDisplayHopsFromSameEnemyItems = true;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, show hop distance to other neutral items. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDisplayHopsFromOtherNeutralItems = true;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, display mission placement failure reasons. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDebugFailedMissionPlacement = false;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, display mission candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDebugMissionCandidateRejections = false;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, display enemy candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDebugEnemyCandidateRejections = false;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, display neutral candidate rejection debug text. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDebugNeutralCandidateRejections = false;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, include hop distance from HQ in mission placement debug. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDisplayHopsFromHQForMissions = true;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, include mission spacing hop distances when spacing rules are active. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDebugMissionSpacingHops = true;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, show min/max connection requirements for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDisplayMinMaxConnectionsForMissionPlacement = true;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, show adjacency requirement summaries for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDisplayMissionAdjacencyRequirements = true;

	UPROPERTY(EditAnywhere, Category = "World Campaign|Debug",
		meta = (ToolTip = "When enabled, show required neutral item type for mission placement. Compile-guarded by DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols."))
	bool bDisplayNeutralItemRequirementForMission = true;

	/**
	 * @brief Clears stacked debug state so new text starts at the default height.
	 */
	void ClearAllDebugState();

	/**
	 * @brief Draws a short-lived accepted placement message at an anchor.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param Text Debug string to draw.
	 * @param Color Optional color override for the text.
	 */
	void DrawAcceptedAtAnchor(AAnchorPoint* AnchorPoint, const FString& Text, FColor Color = FColor::Green);

	/**
	 * @brief Draws a short-lived rejected placement message at an anchor.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param Text Debug string to draw.
	 * @param Color Optional color override for the text.
	 */
	void DrawRejectedAtAnchor(AAnchorPoint* AnchorPoint, const FString& Text, FColor Color = FColor::Red);

	/**
	 * @brief Draws an informational debug message with a custom duration.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param Text Debug string to draw.
	 * @param DurationOverride Duration override in seconds.
	 * @param Color Optional color override for the text.
	 */
	void DrawInfoAtAnchor(AAnchorPoint* AnchorPoint, const FString& Text, float DurationOverride,
	                      FColor Color = FColor::White);

	/**
	 * @brief Builds and draws enemy placement acceptance details.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param Info Structured info for the placement summary.
	 */
	void DebugEnemyPlacementAccepted(AAnchorPoint* AnchorPoint, const FWorldCampaignEnemyPlacementDebugInfo& Info);

	/**
	 * @brief Builds and draws enemy placement rejection details.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param EnemyType Enemy type being placed.
	 * @param Reason Concise rejection reason.
	 */
	void DebugEnemyPlacementRejected(AAnchorPoint* AnchorPoint, EMapEnemyItem EnemyType, const FString& Reason);

	/**
	 * @brief Builds and draws neutral placement acceptance details.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param Info Structured info for the placement summary.
	 */
	void DebugNeutralPlacementAccepted(AAnchorPoint* AnchorPoint, const FWorldCampaignNeutralPlacementDebugInfo& Info);

	/**
	 * @brief Builds and draws neutral placement rejection details.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param NeutralType Neutral type being placed.
	 * @param Reason Concise rejection reason.
	 */
	void DebugNeutralPlacementRejected(AAnchorPoint* AnchorPoint, EMapNeutralObjectType NeutralType,
	                                  const FString& Reason);

	/**
	 * @brief Builds and draws mission placement acceptance details.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param Info Structured info for the placement summary.
	 */
	void DebugMissionPlacementAccepted(AAnchorPoint* AnchorPoint, const FWorldCampaignMissionPlacementDebugInfo& Info);

	/**
	 * @brief Builds and draws mission placement failure details.
	 * @param AnchorPoint Anchor used as the debug draw location.
	 * @param MissionType Mission type being placed.
	 * @param Reason Concise failure reason.
	 */
	void DebugMissionPlacementFailed(AAnchorPoint* AnchorPoint, EMapMission MissionType, const FString& Reason);

private:
	enum class EWorldCampaignRejectionItemType : uint8
	{
		Enemy,
		Neutral,
		Mission,
		Unknown
	};

	struct FWorldCampaignRejectionKey
	{
		EWorldCampaignRejectionItemType ItemType = EWorldCampaignRejectionItemType::Unknown;
		FString Reason;

		bool operator==(const FWorldCampaignRejectionKey& Other) const
		{
			return ItemType == Other.ItemType && Reason == Other.Reason;
		}

		friend uint32 GetTypeHash(const FWorldCampaignRejectionKey& Key)
		{
			return HashCombine(GetTypeHash(static_cast<uint8>(Key.ItemType)), GetTypeHash(Key.Reason));
		}
	};

	struct FWorldCampaignRejectionAttemptState
	{
		int32 AttemptIndex = INDEX_NONE;
		int32 TotalRejections = 0;
		TMap<FWorldCampaignRejectionKey, int32> RejectionsByReason;
	};

	struct FWorldCampaignAnchorDebugStackState
	{
		float LastEndTimeSeconds = 0.f;
		int32 StackIndex = 0;
	};

	bool CanDrawRejectionForAttempt(ECampaignGenerationStep Step, EWorldCampaignRejectionItemType ItemType,
	                                const FString& Reason);
	int32 GetAttemptIndexForStep(ECampaignGenerationStep Step) const;
	float GetStackedHeightOffset(AAnchorPoint* AnchorPoint, float Duration);
	float UpdateStackState(FWorldCampaignAnchorDebugStackState& State, float CurrentTimeSeconds, float Duration);

	// Tracks per-step rejection counts and attempt indices for debug throttling.
	TMap<ECampaignGenerationStep, FWorldCampaignRejectionAttemptState> M_RejectionStateByStep;

	// Tracks stack height and expiry times for anchors that have valid GUID keys.
	TMap<FGuid, FWorldCampaignAnchorDebugStackState> M_DebugStateByAnchorKey;

	// Tracks stack height and expiry times for anchors without GUID keys.
	TMap<TWeakObjectPtr<AAnchorPoint>, FWorldCampaignAnchorDebugStackState> M_DebugStateByAnchorPointer;
};
