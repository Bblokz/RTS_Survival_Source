#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverCombatScoring.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverCombatTestScenario.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverCountTestScenario.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverTestScenario.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverTrenchTestScenario.h"
#include "Subsystems/WorldSubsystem.h"
#include "CoverFinderWorldSubsystem.generated.h"

class ANavigationData;
class ASquadUnit;
class IConsoleObject;
class UPrimitiveComponent;
struct FOverlapResult;
class UNavigationSystemV1;
class URTSCoverFinderDeveloperSettings;

enum class ECoverFinderScanState : uint8
{
	Idle,
	WaitingForPlan,
	SamplingWorld,
	WaitingForWorker
};

enum class ECoverFinderScanDomain : uint8
{
	Landscape,
	Environment
};

enum class ECoverFinderTraceDomain : uint8
{
	ActiveScan,
	AllCoverProviders
};

/** Identifies one obstacle; instances of an instanced mesh share a component and differ by their index. */
struct FCoverObstacleKey
{
	TWeakObjectPtr<const UPrimitiveComponent> Component;
	int32 ItemIndex = INDEX_NONE;

	bool operator==(const FCoverObstacleKey& Other) const
	{
		return Component == Other.Component && ItemIndex == Other.ItemIndex;
	}

	friend uint32 GetTypeHash(const FCoverObstacleKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Component), GetTypeHash(Key.ItemIndex));
	}
};

/**
 * @brief What the scanner remembers about one thin obstacle between scans, so a forest is not probed from
 * scratch every few seconds: how wide its trunk is, and the cover its ring of probes found last time.
 * Everything is dropped when the obstacle moves.
 */
/** A prone point beside a crouch or standing point, after its location was checked against the world. */
struct FCoverProneCompanionCacheEntry
{
	FRTSCoverPoint Companion;
	int32 LastUsedPublicationIndex = 0;

	// False when there is no room to lie down beside the point; kept so it is not checked again.
	bool bHasLyingSpace = false;
};

struct FCoverThinObstacleCache
{
	FVector BoundsCenter = FVector::ZeroVector;

	// Bounds of the trunk at crouch height, for objects whose whole collision is too wide to be thin.
	FBox TrunkBounds = FBox(ForceInit);

	// Unpublished cover candidates from the last time the ring was probed.
	TArray<FRTSCoverPoint> RingCandidates;

	// Where the obstacle's ring is and how far its cover points reach; zero radius while it has no ring.
	FVector RingCenter = FVector::ZeroVector;
	float CoverRadius = 0.0f;

	// Soldiers that may hold a point around this obstacle at the same time.
	int32 SoldierCapacity = 1;

	// Stable number that ring probes carry instead of the key, and that spreads refreshes over the scans.
	uint32 CacheId = 0;
	int32 LastSeenScanIndex = INDEX_NONE;
	int32 LastRingProbeScanIndex = INDEX_NONE;
	bool bHasMeasuredTrunk = false;
	bool bFoundTrunk = false;
};

/** A wide object whose trunk still has to be measured; done inside the scan's per-frame budget. */
struct FCoverPendingTrunkMeasurement
{
	FCoverObstacleKey Key;
	FBox CollisionBounds = FBox(ForceInit);
};

/** Keeps incremental game-thread sampling data together so no array element address survives a mutation. */
struct FCoverFinderSamplingState
{
	TArray<FVector> SampleLocations;

	// Aimed probes, sampled after the grid: first the rings around thin obstacles, then the re-aimed hits that
	// the grid samples of this scan asked for.
	TArray<FCoverFocusedSample> FocusedSamples;
	TArray<FCoverPendingTrunkMeasurement> PendingTrunkMeasurements;
	int32 ReaimedSampleCount = 0;
	int32 MeasuredTrunkCount = 0;
	int32 ReusedRingObstacleCount = 0;

	// Ring the aimed probe being sampled belongs to; zero for grid samples and re-aimed probes.
	uint32 CurrentObstacleCacheId = 0;
	bool bCurrentSampleMayFindOpenFrameCover = false;

	// Coarse position and direction of every re-aimed probe already queued, so one tree trunk grazed from
	// several grid positions is not probed again for each of them.
	TSet<FIntVector> QueuedReaimKeys;

	TArray<FCoverProbeObservation> ObservationChunk;
	FCoverProbeObservation CurrentObservation;

	// Search direction of the aimed probe being sampled; zero while a grid sample is sampled.
	FVector CurrentFocusDirection = FVector::ZeroVector;

	// Horizontal distance from the aimed probe's position to the spot it looks at.
	float CurrentFocusDistance = 0.0f;
	int32 NextSampleIndex = 0;
	int32 CurrentDirectionIndex = 0;
	bool bHasCurrentObservation = false;

	void Reset();
	int32 GetSampleCount() const { return SampleLocations.Num() + FocusedSamples.Num(); }
	bool GetIsSamplingFocusedSample() const { return not CurrentFocusDirection.IsNearlyZero(); }
	bool GetHasWorkLeft() const
	{
		return NextSampleIndex < GetSampleCount() || bHasCurrentObservation || not PendingTrunkMeasurements.IsEmpty();
	}
};

/** Holds per-generation timing before it is converted into the public performance snapshot. *//** Holds per-generation timing before it is converted into the public performance snapshot. */
struct FCoverFinderPerformanceAccumulator
{
	double ScanStartSeconds = 0.0;
	double TotalGameThreadSeconds = 0.0;
	double MaximumGameThreadFrameSeconds = 0.0;
	int32 SamplingFrameCount = 0;
	int32 ProjectedSampleCount = 0;
	int32 WorldQueryCount = 0;
	int32 StandingSurfaceDirectionCount = 0;
	int32 StandingOpeningCount = 0;
	int32 ValidatedStandingGapCount = 0;

	void Reset(double StartSeconds);
};

/**
 * @brief Scans Character navigation incrementally and publishes directional infantry cover found by a worker thread.
 * Callers query immutable snapshots; world collision and navigation never leave the game thread.
 */
UCLASS()
class RTS_SURVIVAL_API URTSCoverFinderWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

	const TArray<FRTSCoverPoint>& GetCoverPointsView() const { return M_CoverPoints; }

	UFUNCTION(BlueprintPure, Category="Cover Finder")
	TArray<FRTSCoverPoint> GetCoverPoints() const { return M_CoverPoints; }

	/**
	 * @brief Copies nearby points for later squad scoring without exposing mutable subsystem storage.
	 * @param SearchLocation Center of the requested region.
	 * @param SearchRadius Maximum three-dimensional distance in centimeters.
	 * @return Cover points currently published inside the region.
	 */
	UFUNCTION(BlueprintPure, Category="Cover Finder")
	TArray<FRTSCoverPoint> FindCoverPointsInRadius(
		const FVector& SearchLocation,
		float SearchRadius) const;

	UFUNCTION(BlueprintPure, Category="Cover Finder")
	FRTSCoverFinderPerformance GetLastPerformanceSnapshot() const { return M_LastPerformanceSnapshot; }

	UFUNCTION(BlueprintPure, Category="Cover Finder")
	FRTSTacticalCoverPerformance GetTacticalPerformanceSnapshot() const { return M_TacticalPerformanceSnapshot; }

	UFUNCTION(BlueprintCallable, Category="Cover Finder")
	void ForceRescan();

	/**
	 * @brief Adds a static actor's authored points to the same spacing-controlled publication used by scanned cover.
	 * @param CoverPoints World-space points copied from the provider component at BeginPlay.
	 * @return Non-zero registration ID used to remove exactly this provider when its actor ends play.
	 */
	uint64 RegisterAuthoredCoverProvider(AActor* ProviderActor, TArray<FRTSCoverPoint>&& CoverPoints);

	uint64 RegisterAuthoredCoverProvider(TArray<FRTSCoverPoint>&& CoverPoints)
	{
		return RegisterAuthoredCoverProvider(nullptr, MoveTemp(CoverPoints));
	}

	/**
	 * @brief Removes one provider immediately and invalidates equivalent cached scan points until the next rescan.
	 * @param RegistrationId ID returned by RegisterAuthoredCoverProvider.
	 */
	void UnregisterAuthoredCoverProvider(uint64 RegistrationId);

	void RegisterSquadUnit(ASquadUnit* SquadUnit);
	void UnregisterSquadUnit(ASquadUnit* SquadUnit);

	/**
	 * @brief Reserves the best nearby point so squads can choose cover without sharing one physical slot.
	 * @param SquadUnit Unit requesting a point; ownership is also used for target-channel selection.
	 * @param TargetActor Optional target whose direction and firing lane constrain the result.
	 * @param TargetLocation Current aim location for the target-specific firing-lane trace.
	 * @param OutCoverPoint Reserved point when successful.
	 * @param MaximumDistanceToTarget Keeps the point inside weapon range of the target; zero disables the limit.
	 * @return True when a currently published and unclaimed point was reserved.
	 */
	bool TryReserveBestCoverPoint(
		ASquadUnit& SquadUnit,
		AActor* TargetActor,
		const FVector& TargetLocation,
		FRTSCoverPoint& OutCoverPoint,
		float MaximumDistanceToTarget = 0.0f);

	/**
	 * @brief Reserves the point that best shields a soldier from the enemies it is fighting.
	 * Used instead of the nearest point as soon as the soldier has a target, and again while it is in cover to
	 * find out whether the enemy has moved around that cover.
	 * @param SquadUnit Unit requesting a point.
	 * @param TargetActor The enemy the unit shoots at; it must be reachable by a firing lane from the point.
	 * @param Threats That target's location plus the other enemies the unit's squad is engaging.
	 * @param MaximumDistanceToTarget Keeps the point inside weapon range of the target; zero disables the limit.
	 * @param OccupiedCoverPoint The point the unit holds now, or nullptr; the result must be clearly better.
	 * @param OutCoverPoint Reserved point when successful.
	 * @return True when a point was reserved; false keeps the unit where it is.
	 */
	bool TryReserveCombatCoverPoint(
		ASquadUnit& SquadUnit,
		AActor& TargetActor,
		const FRTSCombatCoverThreats& Threats,
		float MaximumDistanceToTarget,
		const FRTSCoverPoint* OccupiedCoverPoint,
		FRTSCoverPoint& OutCoverPoint);

	/** @return The designer's combat cover values, copied for the scoring functions. */
	FRTSCombatCoverSettings BuildCombatCoverSettings() const;

	/**
	 * @brief Reserves one specific point, for a soldier whose player-planned move ended on it.
	 * @param SquadUnit Unit that was sent to the point.
	 * @param PointId Point shown to the player in the move preview.
	 * @param OutCoverPoint Current published data of that point when successful.
	 * @return False when the point is gone, taken by another unit, or currently reported unreachable.
	 */
	bool TryReserveCoverPointById(ASquadUnit& SquadUnit, int64 PointId, FRTSCoverPoint& OutCoverPoint);

	/**
	 * @brief Counts the soldiers holding cover around one thin obstacle.
	 * @param ThinObstacleId Obstacle id carried by its cover points.
	 * @param IgnoredUnit A unit whose own reservation does not count, or nullptr.
	 * @param IgnoredUnits More units whose reservations do not count, or nullptr.
	 * @return Number of other soldiers with a reserved point around that obstacle.
	 */
	int32 GetThinObstacleReservationCount(
		uint32 ThinObstacleId,
		const ASquadUnit* IgnoredUnit,
		const TSet<const ASquadUnit*>* IgnoredUnits = nullptr) const;

	// Actors whose cover was read from their sockets at map start, for tests and reports.
	int32 GetSocketCoverActorCount() const { return M_SocketCoverActorCount; }

	// Number of thin obstacles that hold more soldiers than they have room for; zero when the limit works.
	int32 GetOverCapacityThinObstacleCount() const;

	/** @return Unit holding the reservation on a point, or nullptr when it is free. */
	const ASquadUnit* GetCoverReservationOwner(int64 PointId) const;

	void ReleaseCoverReservation(const ASquadUnit& SquadUnit, int64 PointId);

	// Called when a unit gave up walking to a point, so nobody else is sent to the same blocked slot for a while.
	void ReportCoverPointUnreachable(int64 PointId);
	bool GetIsCoverPointPublished(int64 PointId) const;
	AActor* ResolveBlockingProvider(const FRTSCoverPoint& CoverPoint) const;

	/**
	 * @brief Lists the objects a soldier in this cover has directly in front of him.
	 * The scan remembers one object per point, but a crystal cluster or a wreck with loose parts is several
	 * actors, and a shot that clears the first still hits the next.
	 * @param CoverPoint Point the soldier occupies.
	 * @param OutObstacleActors Receives the point's own cover object and every other object in the same spot.
	 */
	void GatherCoverObstacleActors(const FRTSCoverPoint& CoverPoint, TArray<AActor*>& OutObstacleActors) const;

	/**
	 * @brief Validates a single target lane on the target team's collision trace channel.
	 * @param SquadUnit Unit occupying the point and choosing the enemy channel.
	 * @param CoverPoint Point whose posture determines the approximate muzzle origin.
	 * @param TargetActor Actor that must be the first blocking target-channel hit.
	 * @param TargetLocation Current world-space aim location.
	 * @return True only when this posture has a direct lane to this target.
	 */
	bool GetHasTargetSpecificFiringLane(
		const ASquadUnit& SquadUnit,
		const FRTSCoverPoint& CoverPoint,
		const AActor& TargetActor,
		const FVector& TargetLocation);

	/**
	 * @brief Decides whether an occupied point still serves the unit against its current target.
	 * @param SquadUnit Unit occupying the point and choosing the enemy channel.
	 * @param CoverPoint Point whose normal and posture are tested.
	 * @param TargetActor Actor that must be the first blocking target-channel hit.
	 * @param TargetLocation Current world-space aim location.
	 * @return True when the point faces the target and has a direct firing lane to it.
	 */
	bool GetIsCoverPointValidAgainstTarget(
		const ASquadUnit& SquadUnit,
		const FRTSCoverPoint& CoverPoint,
		const AActor& TargetActor,
		const FVector& TargetLocation);

	/**
	 * @return Default horizontal step from a standing point to its exposed firing position; zero for crouch cover.
	 * Units with an expose montage use that animation set's designer offset instead.
	 */
	FVector GetStandingPeekOffset(const FRTSCoverPoint& CoverPoint) const;

	// False until landscape and environment geometry have both been published at least once.
	bool GetHasCompletedFullScan() const { return bM_EnvironmentScanComplete; }

	void LogPerformanceReport() const;

	/**
	 * @brief Debug tool: logs and draws why the scanner does or does not publish cover at a location.
	 * Does nothing unless cover debug symbols are compiled in.
	 * @param GroundLocation Location to explain, normally the ground under the cursor.
	 */
	void ExplainCoverAtLocation(const FVector& GroundLocation);

	void RequestDebugCapture(const FString& ScreenshotName = FString());
	void RequestTestCoverValidation(float DelaySeconds, bool bCaptureScreenshot);

private:
	TUniquePtr<FCoverFinderWorker> M_Worker;
	FCoverFinderSamplingState M_SamplingState;
	FCoverFinderPerformanceAccumulator M_PerformanceAccumulator;
	FCoverFinderSettingsSnapshot M_ActiveSettings;
	FRTSCoverFinderPerformance M_LastPerformanceSnapshot;
	FRTSTacticalCoverPerformance M_TacticalPerformanceSnapshot;
	TArray<FRTSCoverPoint> M_CoverPoints;
	TArray<FRTSCoverPoint> M_LandscapeCoverPoints;
	TArray<FRTSCoverPoint> M_EnvironmentCoverPoints;
	TMap<uint64, TArray<FRTSCoverPoint>> M_AuthoredCoverProviders;
	TMap<uint64, TWeakObjectPtr<AActor>> M_BlockingProviderActors;
	TMap<TWeakObjectPtr<AActor>, uint64> M_BlockingProviderHandles;
	TMap<int64, int32> M_CoverPointIndices;
	TMap<FIntPoint, TArray<int32>> M_CoverSpatialGrid;
	TMap<int64, TWeakObjectPtr<ASquadUnit>> M_CoverReservations;
	// Point ID to the world time at which a slot that could not be walked to may be offered again.
	TMap<int64, float> M_UnreachableCoverPointExpiry;
	TArray<TWeakObjectPtr<ASquadUnit>> M_RegisteredSquadUnits;
	TArray<FBox> M_CachedNavigationTileBounds;
	FBox M_LastScanBounds = FBox(ForceInit);
	uint64 M_ActiveGeneration = 0;
	uint64 M_NextProviderRegistrationId = 1;
	uint64 M_NextBlockingProviderHandle = 1;
	int32 M_NextTacticalUnitIndex = 0;
	float M_CachedAgentRadius = 0.0f;
	float M_CachedAgentHeight = 0.0f;
	float M_TimeUntilNextScan = 0.0f;
	ECoverFinderScanState M_ScanState = ECoverFinderScanState::Idle;
	ECoverFinderScanDomain M_ActiveScanDomain = ECoverFinderScanDomain::Landscape;
	bool bM_LandscapeScanComplete = false;
	bool bM_EnvironmentScanComplete = false;
	bool bM_ForceRescanAfterCurrent = false;
	bool bM_CaptureAfterScan = false;
	bool bM_RestoreCaptureLighting = false;
	bool bM_CaptureLightingWasEnabled = true;
	int32 M_CaptureFramesRemaining = 0;
	FString M_PendingScreenshotPath;
	FCoverTestScenario M_TestCoverScenario;
	FCoverCombatTestScenario M_CombatCoverScenario;
	FCoverCountTestScenario M_CoverCountScenario;
	FCoverTrenchTestScenario M_TrenchCoverScenario;

	// Cover that actors carry on their sockets is registered once, on the first tick of the map.
	bool bM_HasRegisteredSocketCover = false;
	int32 M_SocketCoverActorCount = 0;

	// The actor behind each socket cover registration; when it is destroyed its points are taken out again.
	TMap<uint64, TWeakObjectPtr<AActor>> M_SocketCoverProviders;

	// Rings around the thin obstacles found while gathering the bounds of the next environment scan.
	TArray<FCoverFocusedSample> M_PendingThinObstacleSamples;

	TMap<FCoverObstacleKey, FCoverThinObstacleCache> M_ThinObstacleCaches;
	TMap<uint32, FCoverObstacleKey> M_ObstacleKeysByCacheId;
	uint32 M_NextObstacleCacheId = 1;
	int32 M_EnvironmentScanIndex = 0;

	// Gathered with the bounds of the next environment scan and handed over when that scan begins.
	TArray<FRTSCoverPoint> M_PendingReusedRingCandidates;
	TArray<FCoverPendingTrunkMeasurement> M_PendingTrunkMeasurements;
	int32 M_PendingReusedRingObstacleCount = 0;

	// Prone points placed beside crouch and standing points, by the ID of the point they belong to. Checking
	// one against the world costs queries, so each is checked once and kept for as long as its point exists.
	TMap<int64, FCoverProneCompanionCacheEntry> M_ProneCompanionCache;
	int32 M_PublicationIndex = 0;

	// Prone points of the previous publication that the latest one no longer has; reported with the scan timings.
	int32 M_PronePointsLostAtLastPublication = 0;

	// RTS.CoverFinder.ExplainAtCursor; only registered when cover debug symbols are compiled in.
	IConsoleObject* M_ExplainConsoleCommand = nullptr;

	// -CoverFinderExplainAt="X Y Z" explains one location as soon as the first scan is published.
	bool bM_HasRunCommandLineExplain = false;

	const URTSCoverFinderDeveloperSettings* GetCoverFinderSettings() const;
	const ANavigationData* GetCharacterNavigationData() const;

	/**
	 * @brief Copies valid Character-nav tile bounds and agent dimensions before worker planning begins.
	 * @param OutTileBounds Populated Recast tile bounds for this world.
	 * @param OutAgentRadius Character navigation radius used by side-clearance checks.
	 * @param OutAgentHeight Character navigation height used by capsule checks.
	 * @return True when usable Character Recast navigation data was found.
	 */
	bool GatherNavigationScanInputs(
		TArray<FBox>& OutTileBounds,
		float& OutAgentRadius,
		float& OutAgentHeight);

	/**
	 * @brief Bounds recurring scans to non-landscape collision components so static terrain is not reprocessed.
	 * @param OutEnvironmentBounds Expanded component bounds used by the worker to build its navigation sample grid.
	 * @return True when at least one non-landscape physical cover provider overlaps the navigable world bounds.
	 */
	bool GatherEnvironmentScanBounds(TArray<FBox>& OutEnvironmentBounds);

	/**
	 * @brief Adds the scan area of one very large object as the navigation tiles it overlaps, not as its box.
	 * A backdrop mesh kilometres wide would otherwise fill the whole scan with samples nobody can stand on and
	 * push real objects past the sample limit.
	 * @param ExpandedObjectBounds The object's collision bounds, widened by the cover search distance.
	 * @param OutEnvironmentBounds Receives one box per overlapped navigation tile.
	 */
	void AppendLargeObjectScanBounds(const FBox& ExpandedObjectBounds, TArray<FBox>& OutEnvironmentBounds) const;

	/**
	 * @brief Registers the cover of every actor of the configured socket cover classes from the sockets of its
	 * first mesh that has sockets. Used for trenches, which traces cannot find, and for hand-placed cover such as sandbag walls.
	 */
	void RegisterSocketCoverOnce();

	/**
	 * @brief Builds one actor's cover points from the sockets of its first mesh that has sockets.
	 * @param CoverActor Actor of one of the configured socket cover classes.
	 * @param NameParts Part of a socket name that marks each kind of cover.
	 * @return One point per matching socket, facing along the socket's forward axis.
	 */
	TArray<FRTSCoverPoint> BuildSocketCoverPoints(const AActor& CoverActor, const FCoverSocketNameParts& NameParts) const;

	// Takes out the socket cover of actors that no longer exist, such as a sandbag wall that was destroyed.
	void RemoveSocketCoverOfDestroyedActors();

	// Landscape is scanned once on its own, and soldiers stand on cover points without being cover themselves.
	static bool GetIsScannableEnvironmentComponent(const UPrimitiveComponent* PrimitiveComponent);

	/**
	 * @return World bounds of the collision that was overlapped. A tree's visual bounds include its crown and an
	 * instanced mesh's bounds include every instance, so neither says how wide one trunk is.
	 */
	static FBox GetOverlapCollisionBounds(const FOverlapResult& Overlap);

	/**
	 * @brief Decides what one overlapped object needs this scan: nothing, a trunk measurement, a fresh ring of
	 * probes, or only the cover its ring found earlier.
	 * @param Overlap The overlapped component or instance.
	 * @param CollisionBounds World bounds of its collision.
	 */
	void PlanThinObstacleProbing(const FOverlapResult& Overlap, const FBox& CollisionBounds);

	/**
	 * @brief Finds an obstacle's cache entry, creating it or clearing it when the obstacle has moved.
	 * @param Key Component and instance of the obstacle.
	 * @param CollisionBounds Its current collision bounds.
	 * @return The entry; only valid until the cache map is next added to.
	 */
	FCoverThinObstacleCache& FindOrAddThinObstacleCache(const FCoverObstacleKey& Key, const FBox& CollisionBounds);

	/**
	 * @brief Queues a ring of probes whose results replace what the cache holds for this obstacle.
	 * @param InOutCache The obstacle's cache entry.
	 * @param RingBounds Collision or trunk bounds the ring goes around.
	 * @param OutSamples Receives the ring.
	 */
	void ScheduleRingProbe(
		FCoverThinObstacleCache& InOutCache,
		const FBox& RingBounds,
		TArray<FCoverFocusedSample>& OutSamples) const;

	// Measures one waiting trunk inside the sampling budget and queues its ring when it turns out thin.
	void ProcessNextTrunkMeasurement(int32& InOutFrameWorldQueries);

	// Keeps what a ring probe found, so the following scans can publish it without probing again.
	void StoreRingCandidates(const FCoverProbeObservation& Observation);
	void RemoveUnseenThinObstacleCaches();

	// Marks every published point around a thin obstacle with that obstacle's id and capacity, whichever probe
	// found it, so the capacity also covers points the grid happened to find on the same pole.
	void TagThinObstacleCoverPoints();

	/**
	 * @brief Finds the bounds of an object's trunk at crouch height when its whole collision is too wide to tell.
	 * @param PrimitiveComponent Component to measure; its pivot is taken as the foot of the trunk.
	 * @param CollisionBounds World bounds of its collision.
	 * @param OutTrunkBounds Box around what the rays hit, from the ground to the top of the collision.
	 * @return False when nothing solid surrounds the pivot at crouch height.
	 */
	bool TryMeasureTrunk(
		const UPrimitiveComponent& PrimitiveComponent,
		const FBox& CollisionBounds,
		FBox& OutTrunkBounds) const;

		FCoverFinderSettingsSnapshot BuildSettingsSnapshot(float AgentRadius, float AgentHeight) const;
	FCoverFinderSettingsSnapshot BuildPublicationSettings() const;
	void BeginScan();
	bool PrepareScanInputs(TArray<FBox>& OutScanBounds);
	void PollWorkerResults();
	void AcceptSamplePlan(struct FCoverSamplePlanResult&& PlanResult);
	void AcceptGenerationResult(struct FCoverGenerationResult&& GenerationResult);
	void ProcessWorldSampling();
	bool StartNextObservation(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		int32& InOutFrameWorldQueries);
	void SampleCurrentDirection(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Looks up the next sample to probe, whether it is a grid position or an aimed probe.
	 * @param OutSample Planned position, not yet projected onto the navmesh; a grid position only fills Location.
	 * @return True when the sample is an aimed probe.
	 */
	bool GetNextPlannedSample(FCoverFocusedSample& OutSample) const;

	// Grid samples look in the eight fixed directions; an aimed probe only in its own.
	int32 GetCurrentSampleDirectionCount() const;
	FVector GetCurrentSampleDirection() const;

	/**
	 * @brief Looks for crouch cover behind an open frame of beams, which the regular height probes cannot see.
	 * @param NavigationSystem Current world navigation system.
	 * @param NavigationData Character nav data selected by the registry.
	 * @param ProbeLocation Navigable position on the obstacle's ring.
	 * @param DistanceToObstacleCentre Horizontal distance from that position to the middle of the obstacle.
	 * @param InOutDirectionObservation Receives the cover location when enough of the frame is in the way.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 */
	void SampleOpenFrameCover(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProbeLocation,
		float DistanceToObstacleCentre,
		FCoverDirectionalObservation& InOutDirectionObservation,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Probes crouch and standing height where the lower probe hit, and the sides of a standing surface.
	 * @param ProtectedLocation Navigable position the probes start from.
	 * @param InOutDirectionObservation Holds the lower trace; receives the other traces and side openings.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @return True when lower and crouch probe hit the same surface, so regular cover rules apply.
	 */
	bool SampleRegularCoverHeights(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProtectedLocation,
		FCoverDirectionalObservation& InOutDirectionObservation,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Checks whether a soldier can lie behind what the lower probe hit: low enough to fire over, and with
	 * navigable room to lie down at the designer's stand-off from it.
	 * @param ProbeLocation Navigable position the lower probe started from.
	 * @param InOutDirectionObservation Holds the lower and crouch traces; receives the prone evidence.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 */
	void SampleProneCover(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProbeLocation,
		FCoverDirectionalObservation& InOutDirectionObservation,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Tests the room a lying soldier's body takes up, which is long and low instead of a standing column.
	 * @param GroundLocation Navigable position of the middle of the soldier.
	 * @param FacingDirection Horizontal direction the soldier's head points in.
	 * @return True when no object other than the ground and other soldiers is in that room.
	 */
	bool GetCanInfantryLieAt(const FVector& GroundLocation, const FVector& FacingDirection) const;

	/**
	 * @brief Adds the prone points that belong beside some of the published crouch and standing points.
	 * @param PublishedPronePointIds Prone points of the previous publication, which keep their place.
	 */
	void AppendProneCompanionPoints(const TSet<int64>& PublishedPronePointIds);

	/**
	 * @brief Moves a proposed prone companion onto the navmesh and checks there is room to lie there.
	 * @param InOutCompanion Proposed point; its location is replaced by the navigable one.
	 * @return False when the spot is off the navmesh or blocked.
	 */
	bool TryPlaceProneCompanion(FRTSCoverPoint& InOutCompanion) const;

	/**
	 * @brief Queues one straight-on probe for a surface that a grid probe only grazed.
	 * @param SampleLocation Navigable position the grazing probe was fired from.
	 * @param DirectionObservation The grazing probe's direction and what it hit.
	 */
	void QueueReaimedSample(const FVector& SampleLocation, const FCoverDirectionalObservation& DirectionObservation);

	/**
	 * @brief Evaluates endpoint clearance only after all three height probes identify a continuous high surface.
	 * @param NavigationSystem Current world navigation system.
	 * @param NavigationData Character nav data selected by the registry.
	 * @param ProtectedLocation Navigable position behind the high cover.
	 * @param DirectionObservation Height evidence and resulting left/right endpoint data.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 */
	void SampleStandingSides(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProtectedLocation,
		FCoverDirectionalObservation& DirectionObservation,
		int32& InOutFrameWorldQueries);
	void CompleteCurrentObservation();
	void FlushObservationChunk();
	void FinishWorldSampling();

	/**
	 * @brief Samples whether a full standing capsule can occupy the requested peek side and see past the cover.
	 * @param NavigationSystem Current world navigation system.
	 * @param NavigationData Character nav data selected by the registry.
	 * @param ProtectedLocation Navigable position behind the high cover.
	 * @param SearchDirection Direction from the infantry position toward the cover surface.
	 * @param SideDirection Left or right movement direction relative to the cover-facing orientation.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @param OutCoverLocation Last protected nav position immediately before the verified opening.
	 * @return True when the configured standing gap contains a navigable, collision-free firing position.
	 */
	bool SampleStandingGap(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProtectedLocation,
		const FVector& SearchDirection,
		const FVector& SideDirection,
		int32& InOutFrameWorldQueries,
		FVector& OutCoverLocation);

	/**
	 * @brief Finds the transition from protecting high cover to open lateral space without assuming an actor class.
	 * @param ProtectedLocation Navigable position currently protected by the surface.
	 * @param SearchDirection Direction from the infantry position toward the surface.
	 * @param SideDirection Lateral direction in which the unit would step.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @param OutLastCoveredOffset Last sampled lateral offset still protected by high cover.
	 * @param OutFirstOpenOffset First sampled lateral offset beyond the high-cover edge.
	 * @return True when an edge is found within the bounded endpoint search distance.
	 */
	bool FindStandingOpening(
		const FVector& ProtectedLocation,
		const FVector& SearchDirection,
		const FVector& SideDirection,
		int32& InOutFrameWorldQueries,
		float& OutLastCoveredOffset,
		float& OutFirstOpenOffset);

	/**
	 * @brief Narrows the coarse opening search down to the wall edge so standing points sit a known distance from it.
	 * @param ProtectedLocation Navigable position currently protected by the surface.
	 * @param SearchDirection Direction from the infantry position toward the surface.
	 * @param SideDirection Lateral direction in which the unit would step.
	 * @param LastCoveredOffset Sampled lateral offset still protected by high cover.
	 * @param FirstOpenOffset Sampled lateral offset beyond the high-cover edge.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @return Largest lateral offset found that is still covered, within a few centimeters of the edge.
	 */
	float RefineStandingEdgeOffset(
		const FVector& ProtectedLocation,
		const FVector& SearchDirection,
		const FVector& SideDirection,
		float LastCoveredOffset,
		float FirstOpenOffset,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Rejects narrow seams by sweeping standing-capsule positions across the configured physical opening.
	 * @param NavigationSystem Current world navigation system.
	 * @param NavigationData Character nav data selected by the registry.
	 * @param ProtectedLocation Navigable position currently protected by the surface.
	 * @param SearchDirection Direction from the infantry position toward the surface.
	 * @param SideDirection Lateral direction in which the unit would step.
	 * @param FirstOpenOffset First sampled offset beyond the high-cover edge.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @return True when a standing capsule can traverse the full required opening.
	 */
	bool GetIsStandingGapIntervalOpen(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProtectedLocation,
		const FVector& SearchDirection,
		const FVector& SideDirection,
		float FirstOpenOffset,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Samples the complete physical opening so tiny collision seams cannot masquerade as firing gaps.
	 * @param ProtectedLocation Navigable position currently protected by the surface.
	 * @param SearchDirection Direction from the infantry position toward the surface.
	 * @param SideDirection Lateral direction in which the unit would step.
	 * @param FirstOpenOffset First sampled offset beyond the high-cover edge.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @return True when no crouch-height obstruction exists anywhere across the required opening width.
	 */
	bool GetIsStandingGapTraceIntervalClear(
		const FVector& ProtectedLocation,
		const FVector& SearchDirection,
		const FVector& SideDirection,
		float FirstOpenOffset,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Confirms one candidate firing position is navigable, capsule-clear, and has an open firing lane.
	 * @param NavigationSystem Current world navigation system.
	 * @param NavigationData Character nav data selected by the registry.
	 * @param RequestedLocation World position requested for the standing capsule center path.
	 * @param SearchDirection Direction from the infantry position toward the surface.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @return True when the unit can occupy the position and see past the cover.
	 */
	bool GetIsStandingGapPositionOpen(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& RequestedLocation,
		const FVector& SearchDirection,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Rejects nav projections inside non-nav-affecting walls, props, buildings, or vehicles.
	 * @param GroundLocation Projected floor position beneath the standing capsule.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @return True when the infantry capsule does not overlap any physical cover-provider geometry.
	 */
	bool GetCanInfantryOccupyLocation(
		const FVector& GroundLocation,
		int32& InOutFrameWorldQueries);

	/**
	 * @brief Builds the column a soldier's body needs at a position, as configured by the designer.
	 * @param GroundLocation Floor position the soldier would stand on.
	 * @param OutShapeCenter World location to test the shape at.
	 * @return The capsule that must be free of obstacles.
	 */
	FCollisionShape BuildStandingSpaceShape(const FVector& GroundLocation, FVector& OutShapeCenter) const;

	/**
	 * @brief Routes height probes to the cached landscape domain, recurring environment domain, or combined gap check.
	 * @param GroundLocation Projected infantry floor position.
	 * @param SearchDirection Horizontal direction toward potential cover.
	 * @param Height Probe height above the floor in centimeters.
	 * @param InOutFrameWorldQueries Running query count used to enforce the hard frame cap.
	 * @param TraceDomain Chooses current scan geometry or every physical cover provider.
	 * @return Plain trace evidence safe to copy to the worker thread.
	 */
	FCoverTraceObservation TraceCoverHeight(
		const FVector& GroundLocation,
		const FVector& SearchDirection,
		float Height,
		int32& InOutFrameWorldQueries,
		ECoverFinderTraceDomain TraceDomain = ECoverFinderTraceDomain::ActiveScan);
	uint64 FindOrAddBlockingProviderHandle(AActor* ProviderActor);
	void RebuildCoverSpatialGrid();
	void TickTacticalCoverUnits();
	void RemoveInvalidTacticalReferences();

	/**
	 * @brief Limits expensive firing-lane traces to nearby points that already satisfy cheap tactical checks.
	 * @param SquadUnit Unit whose location and reservation ownership are evaluated.
	 * @param TargetActor Optional target; when valid the point must face away from it.
	 * @param TargetLocation Current target position used by the directional protection test.
	 * @param SearchRadius Maximum distance from the unit in centimeters.
	 * @param MaximumDistanceToTarget Weapon-range limit between point and target; zero disables the limit.
	 * @return Nearest candidates, capped to the small number that may receive line traces.
	 */
	TArray<FRTSCoverPoint> GatherBestTacticalCoverCandidates(
		const ASquadUnit& SquadUnit,
		const AActor* TargetActor,
		const FVector& TargetLocation,
		float SearchRadius,
		float MaximumDistanceToTarget);

	FIntPoint GetCoverSpatialCell(const FVector& Location) const;
	FVector BuildFiringLaneStart(const ASquadUnit& SquadUnit, const FRTSCoverPoint& CoverPoint) const;
	bool GetIsCandidateProtectedFromTarget(
		const FRTSCoverPoint& CoverPoint,
		const FVector& TargetLocation) const;

	// Free, reachable, not just given up by this unit, and its cover object still exists.
	bool GetIsCoverPointAvailableToUnit(const FRTSCoverPoint& CoverPoint, const ASquadUnit& SquadUnit) const;
	bool GetIsPointReservedByAnotherUnit(int64 PointId, const ASquadUnit& SquadUnit) const;
	bool GetIsCoverPointTemporarilyUnreachable(int64 PointId) const;
	void RebuildPublishedCoverPoints();
	void RemoveGeneratedDuplicates(const TArray<FRTSCoverPoint>& RemovedProviderPoints);
	void RecordSamplingFrame(double FrameStartSeconds);
	void PublishPerformanceSnapshot(const struct FCoverGenerationResult& GenerationResult);
	void DrawPublishedCover() const;
	void PrepareDebugCapture();
	void TickDebugCapture();
	void RestoreCaptureLighting();

	// Everything below is the "explain at cursor" debug tool; each of these returns at once unless cover debug
	// symbols are compiled in.
	void TickCommandLineExplain();
	void RegisterExplainConsoleCommand();
	void UnregisterExplainConsoleCommand();
	static void RunExplainConsoleCommand(const TArray<FString>& Arguments, UWorld* World);
	void Explain_LogPublishedCoverNear(const FVector& GroundLocation) const;

	/** @return Names of the objects a soldier's capsule standing on GroundLocation overlaps, and how. */
	FString Explain_DescribeCapsuleOverlaps(const FVector& GroundLocation) const;

	/** @return The heights above the ground, in slices, at which an object intrudes on a soldier's position. */
	FString Explain_DescribeBlockedHeights(const FVector& GroundLocation) const;

	/**
	 * @brief Builds the ring a scan would probe around an obstacle, measuring its trunk on the spot.
	 * @param Overlap The overlapped component or instance.
	 * @param CollisionBounds World bounds of its collision.
	 * @param OutSamples Receives the ring.
	 * @param OutTrunkBounds The measured trunk, invalid when the collision itself was thin or nothing was found.
	 * @return False when the obstacle is wide where a soldier stands against it.
	 */
	bool Explain_TryBuildObstacleRing(
		const FOverlapResult& Overlap,
		const FBox& CollisionBounds,
		TArray<FCoverFocusedSample>& OutSamples,
		FBox& OutTrunkBounds) const;

	/**
	 * @brief Lists the obstacles around a location and how the scanner treats each of them.
	 * @param GroundLocation Location being explained.
	 * @param OutThinObstacleSamples Ring of probes of the nearest thin obstacle, empty when there is none.
	 */
	void Explain_LogObstaclesNear(
		const FVector& GroundLocation,
		TArray<FCoverFocusedSample>& OutThinObstacleSamples);

	// Explains why the prone rule does or does not give cover in one direction.
	FString Explain_ProneCover(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProtectedLocation,
		const FVector& SearchDirection,
		const FCoverTraceObservation& LowerTrace,
		const FCoverTraceObservation& CrouchTrace);

	/**
	 * @brief Probes one position in one direction the way a scan does and says what came of it.
	 * @param NavigationSystem Current world navigation system.
	 * @param NavigationData Character nav data selected by the registry.
	 * @param ProtectedLocation Navigable position the probe is fired from.
	 * @param SearchDirection Direction of the probe.
	 * @return Human-readable result: the cover type found or the check that rejected it.
	 */
	FString Explain_ProbeDirection(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProtectedLocation,
		const FVector& SearchDirection);

	/**
	 * @brief Runs the standing-side checks of a scan one by one to find the first that fails.
	 * @param NavigationSystem Current world navigation system.
	 * @param NavigationData Character nav data selected by the registry.
	 * @param ProtectedLocation Navigable position behind the high cover.
	 * @param CoverFacingDirection Direction from that position toward the cover.
	 * @param SideDirection Left or right of that direction.
	 * @return "ok" or the reason this side gets no standing point.
	 */
	FString Explain_ProbeStandingSide(
		const UNavigationSystemV1& NavigationSystem,
		const ANavigationData& NavigationData,
		const FVector& ProtectedLocation,
		const FVector& CoverFacingDirection,
		const FVector& SideDirection);

	/**
	 * @brief Projects a sample the way a scan does and explains every direction it would probe.
	 * @param Label Prefix for the log lines of this sample.
	 * @param PlannedLocation Position before navmesh projection.
	 * @param AimLocation Spot an aimed probe looks at; ignored when bAimed is false.
	 * @param bAimed False probes the eight grid directions, true only toward AimLocation.
	 */
	void Explain_ProbeSample(const FString& Label, const FVector& PlannedLocation, const FVector& AimLocation, bool bAimed);
};
