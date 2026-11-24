// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "GMRCameraRig.h"
#include "GMRJobTypes.h"

#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorComponent.h"
#include "GMRDirector.generated.h"

class ACineCameraActor;
class ACPPController;
class FGMRAudioCapture;
class UMoviePipeline;
class USoundMix;

UENUM()
enum class EGMRDirectorPhase : uint8
{
	WaitingForController,
	WaitingForStart,
	WaitingForTimeline,
	Running,
	Finished
};

/** @brief Units created or claimed by the director under one job tag, in spawn order. */
USTRUCT()
struct FGMRTaggedUnits
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Units;
};

/** @brief A building expansion order; the controller's preview flow takes several ticks per expansion. */
USTRUCT()
struct FGMRPendingExpansion
{
	GENERATED_BODY()

	FGMREvent Event;
	double QueuedWorldSeconds = 0.0;
	bool bPreviewStarted = false;
	bool bSocket = false;
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<AActor> Building;
};

/** @brief New actors of a class get the tag while the claim is open (aircraft a global ability spawns). */
USTRUCT()
struct FGMRPendingClaim
{
	GENERATED_BODY()

	FString Tag;

	UPROPERTY()
	TObjectPtr<UClass> ClaimClass;

	double SinceWorldSeconds = 0.0;
	double UntilWorldSeconds = 0.0;

	// Child classes are skipped: the survival drop ships derive from the Bf 110 Blueprint.
	bool bExactClass = false;
};

/**
 * @brief Runs one recording take inside the MRQ render: starts the mission without UI, neutralises the map's own
 * scripting in memory only, plays the job's events on the timeline and drives a cine camera through its shots.
 * Spawned by UGMRRunner when Movie Render Queue has loaded the job's map; everything it spawns is destroyed when
 * the render ends and nothing is ever saved, so the map asset is untouched.
 */
UCLASS(NotPlaceable, Transient)
class AGMRDirector : public AActor
{
	GENERATED_BODY()

public:
	AGMRDirector();

	void InitDirector(const FGMRJob& Job);
	void NotifyRenderFinished();

	// Event handlers (GMRDirectorEvents.cpp) need the tag table and the anchor transform.
	TArray<AActor*> GetLiveTaggedUnits(const FString& Tag) const;
	FVector AnchorToWorld(const FVector& AnchorFrameOffset) const;
	float AnchorYawToWorld(float AnchorFrameYaw) const;
	void RegisterUnit(const FString& Tag, AActor* Unit);
	ACPPController* GetPlayerController() const;
	void AddManifestWarning(const FString& Warning);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void TickWaitingForController();
	void TickWaitingForStart();
	void TickWaitingForTimeline();
	void TickRunning();

	void PrepareScene();
	void PrepareScene_ResolveAnchor();
	void PrepareScene_NeutraliseMap();
	void PrepareScene_SpawnCamera();
	void PrepareScene_SpawnSky();
	FString Event_Sky(const FGMREvent& Event);
	void HideGameUI() const;
	void PushSoundMix();
	void KeepGameRunning() const;
	void DisableFogOfWar() const;

	void RunDueEvents(float TimelineSeconds, int32 Frame);
	void ExecuteEvent(const FGMREvent& Event, int32 Frame);
	FString ExecuteEventByType(const FGMREvent& Event);

	void UpdateCamera(float TimelineSeconds, float DeltaSeconds, int32 Frame);
	void BeginShot(int32 ShotIndex, int32 Frame);
	bool ResolvePivot(const FString& Pivot, FVector& OutLocation, FVector& OutHeading);
	bool ResolveLivePivot(const FString& Tag, FVector& OutLocation, FVector& OutHeading);
	bool GetLastKnownLocation(const FString& Tag, FVector& OutLocation) const;
	void RecordUnitLocations();
	bool GetTaggedCentroid(const FString& Tag, int32 Index, FVector& OutLocation, FVector& OutHeading) const;
	TArray<const AActor*> GetRigIgnoredActors() const;
	void FollowCameraWithPawn(const FVector& LookAt) const;

	int32 GetTimelineFrame() const;
	UMoviePipeline* GetActivePipeline() const;

	void StartSubmixCapture();
	void StopSubmixCapture();
	void UpdateSubmixCaptureWindow(int32 Frame);
	void WriteSubmixCaptureWav();
	void KeepAudioUnmuted() const;
	void ForceSoundsNonSpatialized() const;
	void SampleVoiceLines(int32 Frame);

	void DestroySpawnedActors();
	void WriteManifest() const;

	bool GetIsValidCamera() const;
	bool GetIsValidPlayerController() const;

	// --- Event handlers, GMRDirectorEvents.cpp ---
	FString Event_Spawn(const FGMREvent& Event);
	FString Event_Order(const FGMREvent& Event);
	FString Event_Destroy(const FGMREvent& Event);
	FString Event_Hide(const FGMREvent& Event);
	FString Event_Resources(const FGMREvent& Event);
	FString Event_Deploy(const FGMREvent& Event);
	FString Event_Expand(const FGMREvent& Event);
	FString Event_Ability(const FGMREvent& Event);
	FString Event_Claim(const FGMREvent& Event);
	FString Event_Voice(const FGMREvent& Event);
	FString Event_Trench(const FGMREvent& Event);
	FString Event_Garrison(const FGMREvent& Event);
	FString Event_Capture(const FGMREvent& Event);
	FString Event_Clear(const FGMREvent& Event);
	FString Event_Stance(const FGMREvent& Event);
	FString Event_Marker(const FGMREvent& Event);

	// Per-frame upkeep of recorded units (fog, owners of late soldiers, health bars), GMRDirectorEvents.cpp.
	void KeepUnitsReady();
	// Shows pre-placed actors the fog of war hid (enemy base buildings), every few frames.
	void RevealFogHiddenActors() const;
	void PrepareNewUnit(AActor* Unit);
	void TickPendingExpansions();
	bool TryStartExpansion(FGMRPendingExpansion& Pending);
	bool TryPlaceExpansion(FGMRPendingExpansion& Pending);
	void TickPendingClaims();
	// Logs GMR_TRENCH built/total pieces of every spawned trench twice a second (dig timing for shot planning).
	void LogTrenchProgress();
	void ForceTrenchPiecesLodZero(const class ARTSFieldTrench& Trench) const;
	void TickInstantDeploys();
	void SuppressUnwantedActors();
	void DisableUnitOptimisation() const;
	int32 ClearPreplacedUnits() const;
	TArray<AActor*> GetEventUnits(const FGMREvent& Event) const;
	FVector GetGroundLocation(const FVector& Location, bool bProjectToNav) const;
	void PlayUnitVoiceLine(AActor* Unit, const FString& VoiceLineName, bool bForce) const;

	FGMRJob M_Job;
	FGMRManifest M_Manifest;
	EGMRDirectorPhase M_Phase = EGMRDirectorPhase::WaitingForController;
	double M_PhaseStartWorldSeconds = 0.0;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TObjectPtr<ACineCameraActor> M_Camera;

	UPROPERTY()
	TObjectPtr<USoundMix> M_SoundMix;

	UPROPERTY()
	TObjectPtr<AActor> M_Sky;

	UPROPERTY()
	TMap<FString, FGMRTaggedUnits> M_TaggedUnits;

	// Everything the director spawned, destroyed when the render ends.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedActors;

	double M_NextTrenchLogSeconds = 0.0;

	// Ports earlier Trench events started or ended on; later trenches never start on them.
	TArray<FFieldTrenchAnchorHandle> M_UsedTrenchAnchors;

	// Last centroid seen per pivot string, so a shot keeps framing the spot where its subject died.
	TMap<FString, FVector> M_LastPivotLocations;

	// Where each tagged unit was last seen alive (same order as the tag's units), for pivots on dead units.
	TMap<FString, TArray<FVector>> M_LastUnitLocations;

	// live:<Tag> pivots: the unit picked when the current shot began.
	TMap<FString, TWeakObjectPtr<AActor>> M_ShotLiveChoices;

	TArray<FGMRPendingExpansion> M_PendingExpansions;

	UPROPERTY()
	TArray<FGMRPendingClaim> M_PendingClaims;

	// Global abilities fired by the job; kept alive until the render ends (their aircraft report to them).
	UPROPERTY()
	TArray<TObjectPtr<UObject>> M_ActiveAbilities;

	// Units whose fog participation, animation ticking and owner were already set up.
	TSet<TWeakObjectPtr<AActor>> M_PreparedUnits;

	// Trucks ordered to deploy with Settings.Instant: finished as buildings as soon as their conversion starts
	// (a base that must already stand before the take, e.g. under building expansions).
	TArray<TWeakObjectPtr<AActor>> M_InstantDeploys;

	FGMRCameraRig M_Rig;
	int32 M_CurrentShot = -1;
	int32 M_NextEvent = 0;
	TArray<FGMREvent> M_SortedEvents;
	FVector M_AnchorLocation = FVector::ZeroVector;
	float M_AnchorYaw = 0.f;
	int32 M_TimelineStartFrame = -1;
	int32 M_TimelineEndFrame = -1;
	int32 M_LastFrame = -1;
	// Frames left with motion blur off after a cut.
	int32 M_CutBlurHoldFrames = 0;
	bool bM_HasSuppressedDialogs = false;
	bool bM_SetupEventsDone = false;

	TSharedPtr<FGMRAudioCapture, ESPMode::ThreadSafe> M_AudioCapture;
	double M_CaptureStartClock = 0.0;
	double M_CaptureEndClock = 0.0;
	bool bM_CaptureStartClockSet = false;
	bool bM_CaptureWritten = false;

	// Voice line sound names heard while running, filled on the audio thread.
	TSharedPtr<TMap<FString, float>, ESPMode::ThreadSafe> M_VoiceLinesHeard;
	TSharedPtr<FCriticalSection, ESPMode::ThreadSafe> M_VoiceLinesLock;
};
