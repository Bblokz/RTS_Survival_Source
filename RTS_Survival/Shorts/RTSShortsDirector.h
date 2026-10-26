// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSShortsJobTypes.h"

#include "RTSShortsDirector.generated.h"

class ACineCameraActor;
class ACPPController;
class UMoviePipeline;
class USoundMix;

/** Camera orbit pan of the running segment: the start pose rotated around the framed unit's vertical axis. */
USTRUCT()
struct FRTSShortsOrbitState
{
	GENERATED_BODY()

	bool bIsActive = false;
	// Look-at point the camera circles and looks at.
	FVector Pivot = FVector::ZeroVector;
	// Camera location minus Pivot at the segment's framing; rotated about Z by the eased orbit angle.
	FVector StartOffset = FVector::ZeroVector;
	float Degrees = 0.f;
	float EndHeightScale = 1.f;
	float StartSeconds = 0.f;
	float DurationSeconds = 1.f;
};

/** 3v3 battle of the running segment: both lines, their current targets, the decision and the wrecks it left. */
USTRUCT()
struct FRTSShortsBattleState
{
	GENERATED_BODY()

	bool bIsActive = false;
	// The segment's own unit plus BattleAllyClassPaths.
	TArray<TWeakObjectPtr<AActor>> PlayerUnits;
	TArray<TWeakObjectPtr<AActor>> EnemyUnits;
	// Attack target last ordered per unit; a new order only goes out once that target is dead.
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>> OrderedTargets;
	// Wrecks spawned while the battle ran; destroyed with the battle so the next one starts on a clean field.
	TArray<TWeakObjectPtr<AActor>> SpawnedWrecks;
	// Squad controllers of either line: they take the attack orders, their soldiers join the lines above as they spawn.
	TArray<TWeakObjectPtr<AActor>> PlayerSquads;
	TArray<TWeakObjectPtr<AActor>> EnemySquads;
	int32 BeginFrame = 0;
	int32 DecidedFrame = -1;
	bool bPlayerWon = false;
	bool bSegmentShortened = false;
	FDelegateHandle ActorSpawnedHandle;
};

/** Convoy segment: the vehicles driving in a wedge, their pinned turrets and the follow camera's offsets. */
USTRUCT()
struct FRTSShortsConvoyState
{
	GENERATED_BODY()

	bool bIsActive = false;
	bool bDriving = false;
	// The segment's own unit first, then the wedge's side vehicles.
	TArray<TWeakObjectPtr<AActor>> Units;
	// Every turret mesh of the convoy with the relative yaw it is held at (its yaw when the convoy formed plus the
	// side's offset).
	TArray<TWeakObjectPtr<USceneComponent>> TurretMeshes;
	TArray<float> TurretYaws;
	// Smoothed lead position the camera follows, and the camera's and its look-at point's offsets from it.
	FVector Anchor = FVector::ZeroVector;
	FVector CameraOffset = FVector::ZeroVector;
	FVector LookOffset = FVector::ZeroVector;
	int32 StartFrame = 0;
	int32 SegmentFrames = 1;
};

/** Phases of one shorts run; the director advances through them in order. */
UENUM()
enum class ERTSShortsDirectorPhase : uint8
{
	WaitingForController,
	WaitingForStartDialog,
	SpawningUnits,
	WaitingForTurrets,
	Running,
	Finished
};

/**
 * @brief Runs one shorts job on the RTS_Shorts stage: confirms the start dialog, hides the UI, spawns and parks
 * the units, then shows them one per segment and orders attack-ground while Movie Render Queue renders.
 * Timing is keyed on the rendered output frame so the manifest maps every segment to exact frames.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSShortsDirector : public AActor
{
	GENERATED_BODY()

public:
	ARTSShortsDirector();

	/** Hands the job to the director; must be called before BeginPlay runs its first tick. */
	void InitShortsDirector(const FRTSShortsJob& Job);

	const FRTSShortsManifest& GetManifest() const { return M_Manifest; }
	bool GetIsFinished() const { return M_Phase == ERTSShortsDirectorPhase::Finished; }

	/** Writes Manifest.json into the job output directory; safe to call repeatedly. */
	void WriteManifest();

	/** Called by the render subsystem when Movie Render Queue finished; ends the open segment and seals the manifest. */
	void NotifyRenderFinished();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void TickWaitingForController();
	void TickWaitingForStartDialog();
	void TickSpawningUnits();
	void TickWaitingForTurrets();
	void TickRunning();

	bool FindStageActors();
	void ConsiderStageActor(AActor* Actor, const FString& WantedActorName, const FName WantedStage);
	void HideGameUI() const;
	void PushShortsSoundMix();
	void LockViewOnCamera() const;
	void ApplyStageVisibility() const;

	AActor* SpawnSegmentUnit(const int32 SegmentIndex);
	AActor* SpawnPitAsPlaced(const FString& ClassPath, const FTransform& Transform) const;
	static TArray<class ACPPTurretsMaster*> GetUnitTurrets(const AActor* Unit);
	static bool GetHasTurretOwner(const AActor* Unit);
	FTransform GetPenTransform(const int32 SegmentIndex) const;

	/** @return The current timeline frame: rendered output frame when rendering, else world time * fps. */
	int32 GetTimelineFrame() const;
	UMoviePipeline* GetActivePipeline() const;

	void FlushStageEffects(const int32 SegmentIndex) const;
	void BeginSegment(const int32 SegmentIndex);
	void EndSegment(const int32 SegmentIndex);
	void PlaceUnitOnStage(AActor* Unit, const FRTSShortsSegment& Segment) const;
	void ParkUnit(AActor* Unit, const int32 SegmentIndex) const;

	/** The actor the camera frames: the unit itself, or for squads the team weapon or the leader soldier. */
	const AActor* GetFramingSubject(const AActor* Unit) const;

	/** Cuts runtime-spawned particle effects after Segment.EffectMaxSeconds and keeps exhaust emitters off. */
	void LimitStageEffects(AActor* Unit, const struct FRTSShortsSegment& Segment, const int32 Frame);

	/** Segment.EffectBoostNames: enlarges a matching effect, again each time its pool resets its scale. */
	void BoostEffectScale(class UFXSystemComponent* Component, const struct FRTSShortsSegment& Segment);

	/** True when an effect first seen at FirstSeenFrame appeared after Segment.EffectLimitUntilSeconds. */
	bool GetIsEffectExemptFromLimits(const struct FRTSShortsSegment& Segment, const int32 FirstSeenFrame) const;

	/** Frame at which each active runtime effect was first seen (for EffectMaxSeconds). */
	TMap<TWeakObjectPtr<class UFXSystemComponent>, int32> M_EffectFirstSeen;

	/** Scale each tracked runtime effect had when it was first seen (the fade-out shrinks from it). */
	TMap<TWeakObjectPtr<class UFXSystemComponent>, FVector> M_EffectBaseScale;
	/** Scale this director last gave each boosted effect (EffectBoostNames); a different scale means its pool reset it. */
	TMap<TWeakObjectPtr<class UFXSystemComponent>, FVector> M_EffectBoostedScale;
	/** Effects this director deactivated after EffectMaxSeconds; one that turns active again is a new shot. */
	TSet<TWeakObjectPtr<class UFXSystemComponent>> M_EffectStopped;
	/** Smoke-only copies spawned by EffectUnitSmokeScale (own hold and fade). */
	TSet<TWeakObjectPtr<class UFXSystemComponent>> M_SmokeCopies;

	/** Squads: stops projected decals from painting the soldiers' skeletal meshes. */
	void SuppressUnitDecals(AActor* Unit, const int32 SegmentIndex) const;
	/** Segment.InfantryFireIntervalSeconds: shortens the squad's weapon cooldown so a slot shows several shots. */
	void ApplyInfantryFireInterval(AActor* Unit, const FRTSShortsSegment& Segment) const;
	/** Logs RTS_SHORTS_SHOT when the framed soldier's magazine count drops (one line per round). */
	void LogVisibleSoldierShots(const int32 Frame);
	/** Segment.bDisarmTarget: the spawned dummy target can be shot but never fires back. */
	void DisarmSegmentTarget() const;
	void PlaceSquadMembers(AActor* Unit, const FVector& Location, const FRotator& Rotation) const;
	void FrameCamera(const AActor* Unit, const FRTSShortsSegment& Segment, const FVector& TargetWorld) const;
	void FrameImpactCamera(const AActor* Unit, const FRTSShortsSegment& Segment) const;
	/** Segment.BattleEnemyClassPaths: spawns both lines (the segment's unit leads the player line) and makes them see each other. */
	void BeginBattle(const int32 SegmentIndex, AActor* MainUnit);
	/**
	 * @brief Allows support weapons to stand behind a battle line while retaining the default line layout elsewhere.
	 * @param Segment Layout and spacing for the current battle.
	 * @param UnitIndex Index in the chosen faction's additional unit list.
	 * @param bEnemy Uses the enemy origin and layout instead of the player origin and layout.
	 * @return Grounded world position for this participant.
	 */
	FVector GetBattleSpawnLocation(const FRTSShortsSegment& Segment, const int32 UnitIndex, const bool bEnemy) const;
	/**
	 * @brief Grounds fixed emplacements without the vehicle spawner's suspension clearance.
	 * @param ClassPath Participant Blueprint class.
	 * @param Transform Facing and grounded placement for this battle.
	 * @param OwningPlayer Faction owner propagated before participant initialization.
	 * @return Spawned participant, or null when its class cannot spawn.
	 */
	AActor* SpawnBattleParticipant(const FString& ClassPath, const FTransform& Transform, const uint8 OwningPlayer) const;
	/** Battle squads: their soldiers join the battle lines (targets, visibility, alive count) as they spawn. */
	void AbsorbBattleSquadMembers();
	/** Gives a battle squad's soldier the squad controller's owner (and its weapons and collision with it). */
	static void MatchSoldierToSquadOwner(class ASquadUnit* Soldier, const uint8 SquadOwner);
	/** Segment.bConvoy: spawns the wedge's side vehicles, pins the turrets and starts the follow camera. */
	void BeginConvoy(const int32 SegmentIndex, AActor* MainUnit);
	/** Orders every convoy vehicle Segment.ConvoyDriveDistance ahead (issued at the segment's fire time). */
	void OrderConvoyDrive(const int32 SegmentIndex);
	/** Holds the convoy's turrets and moves the follow camera. */
	void TickConvoy(const int32 Frame);
	void EndConvoy();
	/** Segment.StageActorName: the segment plays on its own stage actor; otherwise on the job's stage. */
	void ApplySegmentStage(const int32 SegmentIndex);
	/** Keeps the fog of war out, re-targets units whose target died, and ends the segment once one side is gone. */
	void TickBattle(const int32 Frame);
	void EndBattle();
	void BeginParatrooperBattle();
	/**
	 * @brief Allows independent passes to share the real ability lifecycle without moving either battle line.
	 * @param Pass Ground target and aircraft approach in stage-local coordinates.
	 * @param PassIndex Stable pass index recorded in render verification logs.
	 */
	void StartParatrooperPass(const FRTSShortsParatrooperPass& Pass, int32 PassIndex) const;
	int32 GetParatrooperOwningPlayer() const;
	void UpdateParatrooperAircraftCamera(int32 LocalFrame) const;
	void KeepParatrooperAircraftVisible() const;
	void PrepareParatrooperAircraftForCapture(class AAircraftMaster* Aircraft) const;
	/**
	 * @brief Keeps the flight pose updating offscreen and prevents a missing propeller material hiding the airframe.
	 * @param Mesh Airframe component on the spawned cinematic aircraft.
	 * @param bFirstPreparation Repairs material slots once before the placeholder cleanup sees them.
	 */
	void PrepareParatrooperAircraftMesh(class USkeletalMeshComponent* Mesh, bool bFirstPreparation) const;
	void RepairParatrooperAircraftPropellerMaterial(USkeletalMeshComponent* Mesh) const;
	void TickParatrooperBattle(const int32 LocalFrame);
	/** Preserves the landing cut, then follows the configured route across both faction viewpoints. */
	void UpdateParatrooperBattleCamera(const int32 BattleLocalFrame) const;
	void TrackParatrooperSquads();
	/** Adds the meadow's private range behaviour to vehicles and whole squads, including late arrivals. */
	void UpdateParatrooperRangeBehaviour(const bool bRemove) const;
	/**
	 * @brief Keeps the meadow modifier on its owning component so squad weapon loading is handled by the behaviour.
	 * @param Unit Vehicle or squad controller participating in this battle.
	 * @param bRemove Removes the modifier before a survivor can be reused by another segment.
	 */
	void UpdateParatrooperUnitRangeBehaviour(AActor* Unit, const bool bRemove) const;
	void LogParatrooperUnitRanges(AActor* Unit) const;
	void EndParatrooperBattle();
	bool GetIsValidParatrooperAbility() const;

	// A private copy of the designer template owns the drop; no mission ability or cooldown is modified.
	UPROPERTY()
	TObjectPtr<class UGA_Paratroopers> M_ParatrooperAbility;
	FTransform M_ParatrooperBattleCamera;
	bool bM_ParatrooperCameraCut = false;
	void IssueBattleOrders();
	void KeepBattleUnitsVisible() const;
	void OnBattleActorSpawned(AActor* SpawnedActor);
	/** Ends the running segment at Frame and pulls every later segment forward by the time saved. */
	void ShortenRunningSegment(const int32 Frame);
	FVector GetBattleGroundPoint(const FVector& Location) const;
	/** Job.CombatTextWorldScale: floating combat texts become camera-facing world widgets the 3D pass can film. */
	void KeepCombatTextsInWorld() const;
	/**
	 * A grey checker sphere (a mesh drawn with the engine's default material) shows in battle and convoy frames.
	 * Hides any visible engine sphere or default-material mesh a gameplay actor owns (level dressing is skipped)
	 * and logs each one once.
	 */
	void HideEngineSpheres();
	/** Effects of these owners are activated once and never restarted: the shorts flush and caps must leave them alone. */
	static bool GetIsPersistentEffectOwner(const AActor* ComponentOwner);
	/** Moves the camera along the running segment's orbit pan (Camera.OrbitDegrees) for this segment-local time. */
	void UpdateOrbitCamera(const float SegmentSeconds) const;
	void ApplyCameraCut(const FVector& CameraLocation) const;
	void ForceAnimationTicking(AActor* Unit) const;
	void DisableUnitOptimisation() const;
	void ParkStartingUnits() const;
	void KeepAudioUnmuted() const;
	void ForceSoundsNonSpatialized() const;
	void RunAudioProbe(const int32 Local) const;
	void LogActiveSounds(const int32 Frame) const;
	void BeginRealtimeAudioRecording();
	void EndRealtimeAudioRecordingAndExit();
	void StartSubmixCapture();
	void StopSubmixCapture();
	void UpdateSubmixCaptureWindow(const int32 Frame);
	void WriteSubmixCaptureWav();
	FVector GetTargetWorldLocation(const FRTSShortsSegment& Segment, const AActor* Unit) const;
	float ReadLargestWeaponRange(const AActor* Unit) const;
	void OrderAttackGround(const int32 SegmentIndex);
	void ReleaseOutOfRangeHullWeapons(AActor* Unit, const FVector& Target) const;
	void OrderStop(const int32 SegmentIndex);
	int32 ReadLargestCaliberMm(const AActor* Unit) const;
	void MarkSegmentSkipped(const int32 SegmentIndex, const FString& Reason);

	bool GetIsValidPlayerController() const;
	bool GetIsValidStage() const;
	bool GetIsValidCamera() const;

	FRTSShortsJob M_Job;
	FRTSShortsManifest M_Manifest;

	ERTSShortsDirectorPhase M_Phase = ERTSShortsDirectorPhase::WaitingForController;
	double M_PhaseStartRealSeconds = 0.0;
	double M_RunStartWorldSeconds = 0.0;
	double M_AudioRecordStartWorldSeconds = 0.0;

	/** Frame at which each segment starts; one extra entry holds the end of the last segment. */
	TArray<int32> M_SegmentStartFrames;
	int32 M_CurrentSegment = -1;
	bool bM_FireOrdered = false;
	// Rounds left in the framed soldier's weapons last tick (-1 = not read yet this segment).
	int32 M_LastVisibleRounds = -1;
	bool bM_StopOrdered = false;
	bool bM_SecondFireOrdered = false;
	/** Per segment: squad members hidden by MaxSquadVisible (their attack orders are cancelled every tick). */
	mutable TArray<TArray<TWeakObjectPtr<class ASquadUnit>>> M_HiddenSoldiers;
	int32 M_StopAfterFireFrame = -1;
	int32 M_HoldFramesLeft = 0;
	FVector M_PlacedLocation = FVector::ZeroVector;
	// Set by FrameCamera (const) when the segment pans; read every tick by UpdateOrbitCamera.
	mutable FRTSShortsOrbitState M_Orbit;
	FRTSShortsBattleState M_Battle;
	FRTSShortsConvoyState M_Convoy;
	// The job's stage; segments with their own StageActorName switch M_Stage away from it for their duration.
	UPROPERTY()
	TObjectPtr<AActor> M_JobStage;
	// Diagnostics for the world-space combat texts (converted widgets, ticks with a visible one).
	struct FCombatTextStats
	{
		int32 Converted = 0;
		int32 VisibleTicks = 0;
	};
	mutable FCombatTextStats M_CombatTextStats;
	// Owners already logged by HideEngineSpheres.
	TSet<FString> M_LoggedSphereOwners;
	// A battle ended early: the movie render is stopped once the last segment is done instead of filming empty time.
	bool bM_TimelineShortened = false;
	bool bM_ImpactCutDone = false;

	/** Own capture of the main submix (see FRTSShortsAudioCapture); clock window of frames 0..TotalFrames. */
	TSharedPtr<class FRTSShortsAudioCapture, ESPMode::ThreadSafe> M_AudioCapture;
	bool bM_CaptureStartClockSet = false;
	double M_CaptureStartClock = 0.0;
	double M_CaptureEndClock = 0.0;
	bool bM_CaptureWritten = false;
	bool bM_StageMatchedByName = false;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> M_Units;

	/** Team weapon actor spawned by each segment's squad (null for tanks and plain infantry). */
	TArray<TWeakObjectPtr<AActor>> M_SegmentTeamWeapons;
	/** Dummy enemy actor of the running segment (TargetClassPath), destroyed at the segment's end. */
	TWeakObjectPtr<AActor> M_SegmentTarget;

	UPROPERTY()
	TObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TObjectPtr<AActor> M_Stage;

	UPROPERTY()
	TObjectPtr<AActor> M_Pen;

	UPROPERTY()
	TObjectPtr<ACineCameraActor> M_Camera;

	UPROPERTY()
	TObjectPtr<USoundMix> M_SoundMix;

	bool bM_HasSuppressedDialogs = false;
};
