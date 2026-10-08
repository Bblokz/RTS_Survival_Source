#pragma once

#include "CoreMinimal.h"
#include "CoverFinderTypes.generated.h"

namespace RTSCoverFinderConstants
{
	inline constexpr float InfantryHeight = 180.0f;
	inline constexpr float StandingCoverHeightRatio = 0.8f;
	inline constexpr float StandingCoverHeight = InfantryHeight * StandingCoverHeightRatio;
	inline constexpr float LowerSupportProbeHeight = 30.0f;
	inline constexpr int32 SearchDirectionCount = 8;
	inline constexpr int32 ObservationChunkSize = 128;
	// Safety cap only: the millisecond budget is what paces a scan. A low cap made a large map's first scan take
	// minutes, during which no cover existed at all.
	inline constexpr int32 MaxWorldQueriesPerFrame = 16384;
	inline constexpr int32 MaxSampleLocations = 1000000;
	// A standing surface must be hit this squarely; a more slanted hit is probed again straight on.
	inline constexpr float MinimumStandingFaceAlignment = 0.8f;
	// Room left between a probing soldier's capsule and the surface it is aimed at.
	inline constexpr float ProbeSurfaceClearance = 15.0f;
	// Safety cap on aimed probes in one scan; thin obstacles and re-aimed hits share it.
	inline constexpr int32 MaxFocusedSamples = 200000;
	inline constexpr float SurfaceNormalSimilarity = 0.75f;
	inline constexpr float DuplicateNormalSimilarity = 0.85f;
}

/** Cover postures/actions produced by the geometry search. */
UENUM(BlueprintType)
enum class ERTSCoverType : uint8
{
	Crouch UMETA(DisplayName="Crouch"),
	StandingLeft UMETA(DisplayName="Standing - Peek Left"),
	StandingRight UMETA(DisplayName="Standing - Peek Right")
};

/**
 * @brief A class-agnostic infantry cover position published by the world cover service.
 * The normal points away from the protecting surface and toward the infantry position.
 */
USTRUCT(BlueprintType)
struct FRTSCoverPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Cover")
	int64 PointId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Cover")
	FVector CoverNormal = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category="Cover")
	ERTSCoverType CoverType = ERTSCoverType::Crouch;

	// Zero identifies geometry-discovered cover; authored providers use their subsystem registration ID.
	uint64 ProviderRegistrationId = 0;

	// Game-thread registry handle for the actor whose collision produced this point; zero is terrain or unknown.
	uint64 BlockingProviderHandle = 0;

	// Non-zero when the point lies around a thin obstacle such as a pole or a tree trunk. Such an obstacle has
	// points on every side, but only room for ThinObstacleCapacity soldiers at a time.
	uint32 ThinObstacleId = 0;
	uint8 ThinObstacleCapacity = 0;
};

/**
 * @brief Defines one designer-authored point relative to the owning cover-provider actor.
 * Local rotation's positive X axis is the cover normal pointing from the surface toward the infantry.
 */
USTRUCT(BlueprintType)
struct FRTSLocalCoverPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cover")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cover")
	ERTSCoverType CoverType = ERTSCoverType::Crouch;
};

/** Performance totals for the latest completed cover scan. */
USTRUCT(BlueprintType)
struct FRTSCoverFinderPerformance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 PlannedSampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 ProjectedSampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 WorldQueryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 CoverPointCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 RawCandidateCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 StandingSurfaceDirectionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 StandingOpeningCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 ValidatedStandingGapCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	float TotalGameThreadMilliseconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	float AverageGameThreadMillisecondsPerSamplingFrame = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	float MaximumGameThreadMillisecondsInOneSamplingFrame = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	float WorkerMilliseconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	float WallClockMilliseconds = 0.0f;
};

/** Lightweight game-thread totals for staggered squad cover decisions. */
USTRUCT(BlueprintType)
struct FRTSTacticalCoverPerformance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 RegisteredUnitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 ReservedPointCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 UnitUpdatesLastFrame = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 CandidateChecksLastFrame = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 FiringLaneTracesLastFrame = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	int32 FiringLaneRejectionsLastFrame = 0;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	float GameThreadMillisecondsLastFrame = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Cover|Performance")
	float MaximumGameThreadMilliseconds = 0.0f;
};

/** Plain copied settings used by both the game thread and worker. */
struct FCoverFinderSettingsSnapshot
{
	float SearchGridSpacing = 200.0f;
	float MinimumCrouchCoverHeight = 90.0f;
	float MaximumCoverSearchDistance = 175.0f;
	float StandingPeekGapWidth = 110.0f;
	float StandingPeekEdgeInset = 25.0f;
	float CoverPointSpacing = 90.0f;
	float GameThreadBudgetMilliseconds = 0.35f;
	float AgentRadius = 42.0f;
	float AgentHeight = RTSCoverFinderConstants::InfantryHeight;
	float SurfaceDistanceTolerance = 100.0f;
	float StandingSpaceRadius = 25.0f;
	float StandingSpaceHeight = 150.0f;
	float StandingSpaceFloorClearance = 60.0f;
	float ThinObstacleMaximumWidth = 300.0f;
	int32 ThinObstacleRingSampleCount = 8;
	int32 ThinObstacleRefreshScanCount = 6;
	float ThinObstacleWidthPerSoldier = 70.0f;
	bool bProbeThinObstacles = true;
	bool bReaimSlantedHits = true;
};

/**
 * @brief A probe position that looks at one chosen spot instead of in the eight fixed grid directions.
 * Used where the grid cannot be trusted to hit a surface squarely: around thin obstacles such as tree trunks,
 * and in front of a surface that a grid probe only grazed.
 */
struct FCoverFocusedSample
{
	// Where the soldier would stand; still has to be projected onto the navmesh.
	FVector Location = FVector::ZeroVector;

	// The probe looks from the projected location toward this spot.
	FVector AimLocation = FVector::ZeroVector;

	// Non-zero for a probe of a thin obstacle's ring: its result is kept for that obstacle and reused by the
	// following scans instead of probing the ring again.
	uint32 ObstacleCacheId = 0;
};

/** One horizontal trace result copied without any UObject or physics-scene ownership. */
struct FCoverTraceObservation
{
	FVector ImpactNormal = FVector::ZeroVector;
	float Distance = 0.0f;
	uint64 BlockingProviderHandle = 0;
	bool bBlockingHit = false;
};

/** All height and side-clearance evidence collected in one search direction. */
struct FCoverDirectionalObservation
{
	FVector SearchDirection = FVector::ForwardVector;
	FCoverTraceObservation LowerTrace;
	FCoverTraceObservation CrouchTrace;
	FCoverTraceObservation StandingTrace;
	FVector LeftCoverLocation = FVector::ZeroVector;
	FVector RightCoverLocation = FVector::ZeroVector;
	bool bLeftGapOpen = false;
	bool bRightGapOpen = false;
};

/** World-query evidence for one navigable infantry position. */
struct FCoverProbeObservation
{
	FVector ProjectedLocation = FVector::ZeroVector;
	TArray<FCoverDirectionalObservation> DirectionalObservations;
};
