// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

/**
 * @brief Generator actor that can be placed on a campaign map to spawn and configure world connections.
 */
UCLASS()
class RTS_SURVIVAL_API AGeneratorWorldCampaign : public AActor
{
	GENERATED_BODY()

public:
	AGeneratorWorldCampaign();

	UFUNCTION(CallInEditor, Category = "World Campaign|Connection Generation")
	void GenerateConnections();

private:
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation", meta = (AllowPrivateAccess = "true"))
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation", meta = (AllowPrivateAccess = "true"))
	FConnectionGenerationRules ConnectionGenerationRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AConnection> M_ConnectionClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Generation", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AConnection>> M_GeneratedConnections;
};
