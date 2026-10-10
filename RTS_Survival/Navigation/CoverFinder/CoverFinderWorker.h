#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"

struct FCoverSamplePlanResult
{
	uint64 Generation = 0;
	TArray<FVector> SampleLocations;
	float WorkerMilliseconds = 0.0f;
	bool bWasTruncated = false;
};

struct FCoverGenerationResult
{
	uint64 Generation = 0;
	TArray<FRTSCoverPoint> CoverPoints;
	int32 RawCandidateCount = 0;
	float WorkerMilliseconds = 0.0f;
};

/** Pure-data cover algorithms shared by the worker and automation tests. */
class FCoverFinderAlgorithms
{
public:
	/**
	 * @brief Converts one static provider entry without retaining any actor or component pointer.
	 * @param OwnerTransform Owning actor transform captured at BeginPlay.
	 * @param LocalCoverPoint Designer-authored transform and cover type relative to that actor.
	 * @return World-space cover point whose positive-X rotation axis becomes the cover normal.
	 */
	static FRTSCoverPoint BuildWorldAuthoredPoint(
		const FTransform& OwnerTransform,
		const FRTSLocalCoverPoint& LocalCoverPoint);

	/**
	 * @brief Creates a deterministic grid only where Character navigation tiles exist.
	 * @param TileBounds Copied bounds of populated navigation tiles.
	 * @param Settings Immutable settings for this generation.
	 * @param OutWasTruncated True when the safety limit prevented an unbounded plan.
	 * @return Candidate locations that still require game-thread navigation projection.
	 */
	static TArray<FVector> BuildSamplePlan(
		const TArray<FBox>& TileBounds,
		const FCoverFinderSettingsSnapshot& Settings,
		bool& OutWasTruncated);

	/**
	 * @brief Converts copied trace evidence to raw cover candidates without touching the world.
	 * @param Observations A game-thread-produced chunk of plain trace data.
	 * @param Settings Immutable settings for this generation.
	 * @param OutCandidates Accumulated generation candidates.
	 */
	static void AppendClassifiedCandidates(
		const TArray<FCoverProbeObservation>& Observations,
		const FCoverFinderSettingsSnapshot& Settings,
		TArray<FRTSCoverPoint>& OutCandidates);

	/**
	 * @brief Removes spatial duplicates while preserving distinct normals and standing sides.
	 * @param RawCandidates Candidates produced by all chunks in one generation.
	 * @param Settings Immutable settings for this generation.
	 * @param PreferredPronePointIds Prone points to keep when thinning them out; see AppendSpacedPronePoints.
	 * @return Deterministically ordered points with stable value-based IDs.
	 */
	static TArray<FRTSCoverPoint> FinalizeCandidates(
		TArray<FRTSCoverPoint>&& RawCandidates,
		const FCoverFinderSettingsSnapshot& Settings,
		const TSet<int64>* PreferredPronePointIds = nullptr);

	/**
	 * @brief Applies the same type, direction, and spacing rule used by final publication.
	 * @param FirstCandidate Existing or pending cover point.
	 * @param SecondCandidate Cover point being compared against it.
	 * @param MinimumSpacing Minimum allowed distance between equivalent points.
	 * @return True when only one of the two candidates may be published.
	 */
	static bool GetAreDuplicateCandidates(
		const FRTSCoverPoint& FirstCandidate,
		const FRTSCoverPoint& SecondCandidate,
		float MinimumSpacing);

	/** @return True when an obstacle with these collision bounds is probed from a ring instead of only by the grid. */
	static bool GetIsThinObstacle(const FBox& CollisionBounds, const FCoverFinderSettingsSnapshot& Settings);

	/** @return How many soldiers may take cover around a thin obstacle with these bounds at the same time. */
	static int32 GetThinObstacleSoldierCapacity(const FBox& ObstacleBounds, const FCoverFinderSettingsSnapshot& Settings);

	/** @return Horizontal distance from a thin obstacle's centre within which a cover point belongs to it. */
	static float GetThinObstacleCoverRadius(const FBox& ObstacleBounds, const FCoverFinderSettingsSnapshot& Settings);

	/** @return Distance from a probed surface at which a soldier's capsule stands just clear of it. */
	static float GetProbeStandOffDistance(const FCoverFinderSettingsSnapshot& Settings);

	/**
	 * @brief Surrounds one thin obstacle with probes aimed at its centre, so finding its cover does not depend
	 * on where the scan grid happens to fall.
	 * @param CollisionBounds World bounds of the obstacle's collision, not of its visual mesh.
	 * @param Settings Immutable settings for this generation.
	 * @param OutSamples Receives the ring of probes; untouched when the obstacle is too wide or too low.
	 * @return True when the obstacle was thin enough to get a ring.
	 */
	static bool AppendThinObstacleSamples(
		const FBox& CollisionBounds,
		const FCoverFinderSettingsSnapshot& Settings,
		TArray<FCoverFocusedSample>& OutSamples);

	/**
	 * @brief Builds a second probe straight in front of a surface that a grid probe only grazed.
	 * @param SampleLocation Navigable position the grazing probe was fired from.
	 * @param SearchDirection Direction of the grazing probe.
	 * @param SurfaceTrace What that probe hit.
	 * @param Settings Immutable settings for this generation.
	 * @param OutSample Probe that faces the hit surface squarely.
	 * @return False when the surface was missed or already hit squarely enough.
	 */
	static bool TryBuildReaimedSample(
		const FVector& SampleLocation,
		const FVector& SearchDirection,
		const FCoverTraceObservation& SurfaceTrace,
		const FCoverFinderSettingsSnapshot& Settings,
		FCoverFocusedSample& OutSample);

	/**
	 * @brief Decides from the probes of one direction whether a soldier can lie behind what the lower probe hit.
	 * Regular cover always wins: this only passes where the hit is no crouch cover.
	 * @param DirectionObservation Lower, crouch and fire-over probes of one direction.
	 * @param Settings Immutable settings for this generation.
	 * @return True for a steep enough face that tops out below the height a prone soldier fires over.
	 */
	static bool GetIsProneCoverEvidence(
		const FCoverDirectionalObservation& DirectionObservation,
		const FCoverFinderSettingsSnapshot& Settings);

	/**
	 * @brief Adds prone points sparsely: none on top of another cover point, none close to another prone point.
	 * @param PronePoints Candidates in the order of preference.
	 * @param Settings Immutable settings; ProneCoverPointSpacing sets how sparse the result is.
	 * @param InOutAcceptedPoints Published points so far; receives the prone points that fit, with their IDs.
	 * @param PreferredPointIds Points that go first, normally the ones published before. Which of two close
	 * candidates survives would otherwise change whenever a third one comes or goes, and a soldier lying on a
	 * point would lose it to its neighbour from one scan to the next.
	 */
	static void AppendSpacedPronePoints(
		TArray<FRTSCoverPoint>&& PronePoints,
		const FCoverFinderSettingsSnapshot& Settings,
		TArray<FRTSCoverPoint>& InOutAcceptedPoints,
		const TSet<int64>* PreferredPointIds = nullptr);

	/**
	 * @brief Proposes a prone point beside some of the crouch and standing points, for variety in a squad's cover.
	 * Which points get one is decided by their ID, so the choice is the same on every scan.
	 * @param SourcePoint Published crouch or standing point.
	 * @param Settings Immutable settings; chance and sideways offset come from here.
	 * @param OutCompanion Prone point facing roughly the same way; its location still needs a navigation check.
	 * @return False when this point gets no companion.
	 */
	static bool TryBuildProneCompanion(
		const FRTSCoverPoint& SourcePoint,
		const FCoverFinderSettingsSnapshot& Settings,
		FRTSCoverPoint& OutCompanion);

	/**
	 * @brief Reads the kind of cover a mesh socket stands for from its name.
	 * @param SocketName Name of the socket.
	 * @param NameParts Designer's name part per kind of cover; empty parts match nothing.
	 * @param OutCoverType Kind of the longest name part the socket name contains, whatever the letter case.
	 * @return False when the name contains none of the parts, so the socket is no cover point.
	 */
	static bool TryGetSocketCoverType(
		const FString& SocketName,
		const FCoverSocketNameParts& NameParts,
		ERTSCoverType& OutCoverType);

	static bool GetIsSameSurface(
		const FCoverTraceObservation& FirstTrace,
		const FCoverTraceObservation& SecondTrace,
		const FCoverFinderSettingsSnapshot& Settings);

	static FVector BuildCoverNormal(
		const FCoverTraceObservation& SurfaceTrace,
		const FVector& SearchDirection);
};

/**
 * @brief Owns the persistent cover-analysis thread and exchanges only copied value data with the world subsystem.
 * Requests are ordered, allowing chunks to be accumulated and finalized without cross-thread UObject access.
 */
class FCoverFinderWorker final : public FRunnable
{
public:
	FCoverFinderWorker() = default;
	virtual ~FCoverFinderWorker() override;

	bool Start();
	void StopAndWait();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

	void EnqueuePlanRequest(
		uint64 Generation,
		TArray<FBox>&& TileBounds,
		const FCoverFinderSettingsSnapshot& Settings);
	void EnqueueObservationChunk(uint64 Generation, TArray<FCoverProbeObservation>&& Observations);

	// Adds candidates that were classified in an earlier generation and are known to be unchanged.
	void EnqueueCandidateChunk(uint64 Generation, TArray<FRTSCoverPoint>&& Candidates);
	void EnqueueFinalizeRequest(uint64 Generation);

	bool DequeueSamplePlan(FCoverSamplePlanResult& OutResult);
	bool DequeueGenerationResult(FCoverGenerationResult& OutResult);

private:
	enum class EWorkerRequestType : uint8
	{
		BuildPlan,
		ProcessObservations,
		AppendCandidates,
		Finalize
	};

	struct FWorkerRequest
	{
		EWorkerRequestType Type = EWorkerRequestType::BuildPlan;
		uint64 Generation = 0;
		FCoverFinderSettingsSnapshot Settings;
		TArray<FBox> TileBounds;
		TArray<FCoverProbeObservation> Observations;
		TArray<FRTSCoverPoint> Candidates;
	};

	TQueue<FWorkerRequest, EQueueMode::Spsc> M_RequestQueue;
	TQueue<FCoverSamplePlanResult, EQueueMode::Spsc> M_SamplePlanQueue;
	TQueue<FCoverGenerationResult, EQueueMode::Spsc> M_GenerationResultQueue;
	FRunnableThread* M_Thread = nullptr;
	FEvent* M_WorkAvailableEvent = nullptr;
	FThreadSafeBool bM_StopRequested = false;

	uint64 M_ActiveGeneration = 0;
	FCoverFinderSettingsSnapshot M_ActiveSettings;
	TArray<FRTSCoverPoint> M_ActiveRawCandidates;
	double M_ActiveWorkerSeconds = 0.0;

	void TriggerWorker();
	void ProcessRequest(FWorkerRequest&& Request);
	void ProcessBuildPlan(FWorkerRequest&& Request);
	void ProcessObservations(FWorkerRequest&& Request);
	void ProcessAppendCandidates(FWorkerRequest&& Request);
	void ProcessFinalize(const FWorkerRequest& Request);
};
