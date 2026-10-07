#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverTestScenario.h"
#include "Subsystems/WorldSubsystem.h"
#include "CoverFinderWorldSubsystem.generated.h"

class ANavigationData;
class ASquadUnit;
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

/** Keeps incremental game-thread sampling data together so no array element address survives a mutation. */
struct FCoverFinderSamplingState
{
	TArray<FVector> SampleLocations;
	TArray<FCoverProbeObservation> ObservationChunk;
	FCoverProbeObservation CurrentObservation;
	int32 NextSampleIndex = 0;
	int32 CurrentDirectionIndex = 0;
	bool bHasCurrentObservation = false;

	void Reset();
};

/** Holds per-generation timing before it is converted into the public performance snapshot. */
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

	void ReleaseCoverReservation(const ASquadUnit& SquadUnit, int64 PointId);

	// Called when a unit gave up walking to a point, so nobody else is sent to the same blocked slot for a while.
	void ReportCoverPointUnreachable(int64 PointId);
	bool GetIsCoverPointPublished(int64 PointId) const;
	AActor* ResolveBlockingProvider(const FRTSCoverPoint& CoverPoint) const;

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
};
