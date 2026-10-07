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
	inline constexpr int32 MaxWorldQueriesPerFrame = 256;
	inline constexpr int32 MaxSampleLocations = 1000000;
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
