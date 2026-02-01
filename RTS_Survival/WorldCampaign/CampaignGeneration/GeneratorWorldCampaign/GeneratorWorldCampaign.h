// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/GenerationStep/Enum_CampaignGenerationStep.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyHQPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/EnemyPlacementRules/EnemyPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/MissionPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/NeutralItemPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/PlayerHQPlacementRules.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignCountDifficultyTuning.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationRules/WorldCampaignPlacementFailurePolicy.h"
#include "GeneratorWorldCampaign.generated.h"

class AAnchorPoint;
class AConnection;

USTRUCT(BlueprintType)
struct FConnectionGenerationRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation")
	int32 MinConnections = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation")
	int32 MaxConnections = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Connection Generation")
	float MaxPreferredDistance = 5000.f;
};

USTRUCT(BlueprintType)
struct FWorldCampaignPlacementState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	int32 SeedUsed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TArray<TWeakObjectPtr<AAnchorPoint>> CachedAnchors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TArray<TWeakObjectPtr<AConnection>> CachedConnections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TWeakObjectPtr<AAnchorPoint> PlayerHQAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	FGuid PlayerHQAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TWeakObjectPtr<AAnchorPoint> EnemyHQAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	FGuid EnemyHQAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, EMapEnemyItem> EnemyItemsByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, EMapNeutralObjectType> NeutralItemsByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, EMapMission> MissionsByAnchorKey;
};

USTRUCT(BlueprintType)
struct FWorldCampaignDerivedData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> PlayerHQHopDistancesByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> EnemyHQHopDistancesByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, int32> AnchorConnectionDegreesByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<FGuid, float> ChokepointScoresByAnchorKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapEnemyItem, int32> EnemyItemPlacedCounts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapNeutralObjectType, int32> NeutralItemPlacedCounts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation")
	TMap<EMapMission, int32> MissionPlacedCounts;
};

USTRUCT(BlueprintType)
struct FCampaignGenerationStepTransaction
{
	GENERATED_BODY()

	UPROPERTY()
	ECampaignGenerationStep CompletedStep = ECampaignGenerationStep::NotStarted;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedActors;

	UPROPERTY()
	TArray<TObjectPtr<AConnection>> SpawnedConnections;

	UPROPERTY()
	FWorldCampaignPlacementState PreviousPlacementState;

	UPROPERTY()
	FWorldCampaignDerivedData PreviousDerivedData;
};

/**
 * @brief Generator actor that can be placed on a campaign map to spawn and configure world connections.
 */
UCLASS()
class RTS_SURVIVAL_API AGeneratorWorldCampaign : public AActor
{
	GENERATED_BODY()

public:
	AGeneratorWorldCampaign();

	UFUNCTION(CallInEditor, Category = "World Campaign|Connection Generation",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::NotStarted"))
	void CreateConnectionsStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|HQ",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::ConnectionsCreated"))
	void PlaceHQStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|HQ",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::PlayerHQPlaced"))
	void PlaceEnemyHQStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|Enemy",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::EnemyHQPlaced"))
	void PlaceEnemyObjectsStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|Neutral",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::EnemyObjectsPlaced"))
	void PlaceNeutralObjectsStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Placement|Missions",
		meta = (EditCondition = "M_GenerationStep == ECampaignGenerationStep::NeutralObjectsPlaced"))
	void PlaceMissionsStep();

	UFUNCTION(CallInEditor, Category = "World Campaign|Generation")
	void ExecuteAllSteps();

	UFUNCTION(CallInEditor, Category = "World Campaign|Generation")
	void EraseAllGeneration();

private:
	bool GetIsValidPlayerHQAnchor() const;
	bool GetIsValidEnemyHQAnchor() const;

	/**
	 * @brief Wraps step execution so undo data is recorded for backtracking.
	 * @param CompletedStep Step state that will be marked as completed on success.
	 * @param StepFunction Function that performs the step work.
	 * @return true if the step executed successfully.
	 */
	bool ExecuteStepWithTransaction(ECampaignGenerationStep CompletedStep,
		bool (AGeneratorWorldCampaign::*StepFunction)(FCampaignGenerationStepTransaction&));
	bool ExecuteCreateConnections(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceHQ(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyHQ(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceEnemyObjects(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceNeutralObjects(FCampaignGenerationStepTransaction& OutTransaction);
	bool ExecutePlaceMissions(FCampaignGenerationStepTransaction& OutTransaction);

	bool ExecuteAllStepsWithBacktracking();
	bool CanExecuteStep(ECampaignGenerationStep CompletedStep) const;
	ECampaignGenerationStep GetPrerequisiteStep(ECampaignGenerationStep CompletedStep) const;
	ECampaignGenerationStep GetNextStep(ECampaignGenerationStep CurrentStep) const;
	int32 GetStepAttemptIndex(ECampaignGenerationStep CompletedStep) const;
	void IncrementStepAttempt(ECampaignGenerationStep CompletedStep);
	void ResetStepAttemptsFrom(ECampaignGenerationStep CompletedStep);
	EPlacementFailurePolicy GetFailurePolicyForStep(ECampaignGenerationStep FailedStep) const;
	/**
	 * @brief Handles failed steps by undoing transactions and advancing the retry index.
	 * @param FailedStep Step that failed to execute.
	 * @param InOutStepIndex Step index to resume from after backtracking.
	 * @param StepOrder Ordered list of completed step states.
	 * @return true if backtracking can continue.
	 */
	bool HandleStepFailure(ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
		const TArray<ECampaignGenerationStep>& StepOrder);
	void UndoLastTransaction();
	void UndoConnections(const FCampaignGenerationStepTransaction& Transaction);
	void ClearPlacementState();
	void ClearDerivedData();
	void CacheAnchorConnectionDegrees();
	void DebugNotifyAnchorPicked(const AAnchorPoint* AnchorPoint, const FString& Label, const FColor& Color) const;

	/**
	 * @brief Prepares anchors for a deterministic connection pass so retries start cleanly.
	 * @param OutAnchorPoints Output list of sorted, valid anchors.
	 * @return true when there are enough valid anchors to continue.
	 */
	bool PrepareAnchorsForConnectionGeneration(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints);

	/**
	 * @brief Runs the connection generation phases with deterministic shuffle per attempt.
	 * @param AnchorPoints Sorted anchors to process.
	 * @param RandomStream Stream seeded for deterministic attempts.
	 * @param OutDesiredConnections Desired connection counts per anchor.
	 */
	void GenerateConnectionsForAnchors(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints, FRandomStream& RandomStream,
		TMap<TObjectPtr<AAnchorPoint>, int32>& OutDesiredConnections);

	/**
	 * @brief Stores derived cached data so designers can inspect intermediate results.
	 * @param AnchorPoints Anchors used for keying cached state.
	 */
	void CacheGeneratedState(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints);

	bool ValidateGenerationRules() const;
	void ClearExistingConnections();
	void GatherAnchorPoints(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints) const;

	/**
	 * @brief Assigns stable desired connection counts so the generator can target a per-node degree.
	 * @param AnchorPoints Anchors to receive desired connection counts.
	 * @param RandomStream Stream used to keep the assignment deterministic.
	 * @param OutDesiredConnections Output mapping of anchors to desired counts.
	 */
	void AssignDesiredConnections(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
		FRandomStream& RandomStream, TMap<TObjectPtr<AAnchorPoint>, int32>& OutDesiredConnections) const;

	/**
	 * @brief Attempts to create preferred-distance connections for a single anchor.
	 * @param AnchorPoint Anchor currently being processed.
	 * @param AnchorPoints Sorted list of all anchors for deterministic ordering.
	 * @param DesiredConnections Desired connection counts per anchor.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void GeneratePhasePreferredConnections(AAnchorPoint* AnchorPoint,
		const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
		const TMap<TObjectPtr<AAnchorPoint>, int32>& DesiredConnections,
		TArray<struct FConnectionSegment>& ExistingSegments);

	/**
	 * @brief Extends the search beyond distance limits to satisfy minimum connections.
	 * @param AnchorPoint Anchor currently being processed.
	 * @param AnchorPoints Sorted list of all anchors for deterministic ordering.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void GeneratePhaseExtendedConnections(AAnchorPoint* AnchorPoint,
		const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
		TArray<struct FConnectionSegment>& ExistingSegments);

	/**
	 * @brief Attempts to attach the anchor to an existing connection segment as a junction.
	 * @param AnchorPoint Anchor currently being processed.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void GeneratePhaseThreeWayConnections(AAnchorPoint* AnchorPoint,
		TArray<struct FConnectionSegment>& ExistingSegments);

	/**
	 * @brief Spawns and registers a new connection between two anchors if allowed.
	 * @param AnchorPoint First endpoint anchor.
	 * @param CandidateAnchor Second endpoint anchor.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 * @param DebugColor Debug draw color if enabled at compile time.
	 * @return true if the connection was created and registered.
	 */
	bool TryCreateConnection(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor,
		TArray<struct FConnectionSegment>& ExistingSegments, const FColor& DebugColor);

	/**
	 * @brief Validates a potential anchor-to-anchor segment against max counts and intersections.
	 * @param AnchorPoint First endpoint anchor.
	 * @param CandidateAnchor Second endpoint anchor.
	 * @param StartPoint Segment start in XY.
	 * @param EndPoint Segment end in XY.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 * @param ConnectionToIgnore Optional connection to ignore for intersection tests.
	 * @return true if the connection is allowed.
	 */
	bool IsConnectionAllowed(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor,
		const FVector2D& StartPoint, const FVector2D& EndPoint, const TArray<struct FConnectionSegment>& ExistingSegments,
		const AConnection* ConnectionToIgnore) const;

	/**
	 * @brief Finds and adds a valid three-way junction connection to satisfy minimum connections.
	 * @param AnchorPoint Anchor currently being processed.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 * @return true if a three-way connection was created.
	 */
	bool TryAddThreeWayConnection(AAnchorPoint* AnchorPoint, TArray<struct FConnectionSegment>& ExistingSegments);

	/**
	 * @brief Registers the base connection relationship on both anchors.
	 * @param Connection Connection actor to register.
	 * @param AnchorA First endpoint anchor.
	 * @param AnchorB Second endpoint anchor.
	 */
	void RegisterConnectionOnAnchors(AConnection* Connection, AAnchorPoint* AnchorA, AAnchorPoint* AnchorB) const;

	/**
	 * @brief Registers a third anchor on an existing connection without duplicating the actor.
	 * @param Connection Connection actor to update.
	 * @param ThirdAnchor The new third anchor to register.
	 */
	void RegisterThirdAnchorOnConnection(AConnection* Connection, AAnchorPoint* ThirdAnchor) const;

	/**
	 * @brief Adds a 2D segment entry to the list for intersection checks.
	 * @param Connection Connection actor owning the segment.
	 * @param AnchorA First endpoint anchor.
	 * @param AnchorB Second endpoint anchor.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void AddConnectionSegment(AConnection* Connection, AAnchorPoint* AnchorA, AAnchorPoint* AnchorB,
		TArray<struct FConnectionSegment>& ExistingSegments) const;

	/**
	 * @brief Adds the third-branch segment entry for intersection checks.
	 * @param Connection Connection actor owning the segment.
	 * @param ThirdAnchor Anchor that is attached to the junction.
	 * @param JunctionLocation World-space location on the base segment.
	 * @param ExistingSegments Current list of generated connection segments for intersection checks.
	 */
	void AddThirdConnectionSegment(AConnection* Connection, AAnchorPoint* ThirdAnchor, const FVector& JunctionLocation,
		TArray<struct FConnectionSegment>& ExistingSegments) const;

	void DebugNotifyAnchorProcessing(const AAnchorPoint* AnchorPoint, const FString& Label, const FColor& Color) const;
	void DebugDrawConnection(const AConnection* Connection, const FColor& Color) const;
	void DebugDrawThreeWay(const AConnection* Connection, const FColor& Color) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation", meta = (AllowPrivateAccess = "true"))
	ECampaignGenerationStep M_GenerationStep = ECampaignGenerationStep::NotStarted;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation", meta = (AllowPrivateAccess = "true"))
	FWorldCampaignPlacementState M_PlacementState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation", meta = (AllowPrivateAccess = "true"))
	FWorldCampaignDerivedData M_DerivedData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation", meta = (AllowPrivateAccess = "true"))
	TArray<FCampaignGenerationStepTransaction> M_StepTransactions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation", meta = (AllowPrivateAccess = "true"))
	TMap<ECampaignGenerationStep, int32> M_StepAttemptIndices;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Generation", meta = (AllowPrivateAccess = "true"))
	int32 M_TotalAttemptCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation", meta = (AllowPrivateAccess = "true"))
	FConnectionGenerationRules ConnectionGenerationRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AConnection> M_ConnectionClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AConnection>> M_GeneratedConnections;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Player HQ", meta = (AllowPrivateAccess = "true"))
	FPlayerHQPlacementRules M_PlayerHQPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Enemy HQ", meta = (AllowPrivateAccess = "true"))
	FEnemyHQPlacementRules M_EnemyHQPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Enemy Items", meta = (AllowPrivateAccess = "true"))
	FEnemyPlacementRules M_EnemyPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Neutral Items", meta = (AllowPrivateAccess = "true"))
	FNeutralItemPlacementRules M_NeutralItemPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Missions", meta = (AllowPrivateAccess = "true"))
	FMissionPlacement M_MissionPlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Counts & Difficulty", meta = (AllowPrivateAccess = "true"))
	FWorldCampaignCountDifficultyTuning M_CountAndDifficultyTuning;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Placement Rules|Failure Policy", meta = (AllowPrivateAccess = "true"))
	FWorldCampaignPlacementFailurePolicy M_PlacementFailurePolicy;
};
