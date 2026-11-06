// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSShortsJobTypes.h"
#include "RTSShortsTeamWeaponState.h"
#include "RTSShortsAircraftCrashState.h"
#include "RTSShortsCookOffState.h"

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
	float HoldSeconds = 0.f;
	float ReturnDegrees = 0.f;
	float ReturnSeconds = 1.f;
	float PushInStartSeconds = 0.f;
	float PushInSeconds = 1.f;
	float PushInDistanceScale = 1.f;
	bool bHasPushInFocus = false;
	// The surviving guns' centre, taken when the push-in starts so a later loss does not jerk the camera.
	FVector PushInFocus = FVector::ZeroVector;
};

/** One Segment.FlyBys aircraft of the running segment: its spawned airframe and whether its sound has started. */
USTRUCT()
struct FRTSShortsFlyByState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Airframe;

	bool bSoundStarted = false;
};

/** A Segment.BattleFlankRoutes drive under way: the enemy on it, and whether it was ordered off and has finished. */
USTRUCT()
struct FRTSShortsBattleFlankState
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Unit;
	int32 RouteIndex = 0;
	bool bOrdered = false;
	bool bFinished = false;
	float OrderedAtSeconds = 0.f;
};

/** An attacker working round the evading unit (Segment.BattleSoloEvasion) after its flank route is done. */
USTRUCT()
struct FRTSShortsReflankState
{
	GENERATED_BODY()

	bool bDriving = false;
	float NextAllowedSeconds = 0.f;
	float OrderedAtSeconds = 0.f;
	FVector Destination = FVector::ZeroVector;
};

/** One enemy squad under Segment.BattleSquadTactics: where it was sent and when it may think again. */
USTRUCT()
struct FRTSShortsSquadTacticState
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Squad;
	float OrderAtSeconds = 0.f;
	bool bOrdered = false;
	// Walking to Destination: it takes no attack order from the director until it arrives or times out.
	bool bMoving = false;
	float MovedAtSeconds = 0.f;
	float NextCheckSeconds = 0.f;
	float ArrivedSeconds = 0.f;
	// Index into M_CoverTrees, or INDEX_NONE for an open flank spot.
	int32 TreeIndex = INDEX_NONE;
	FVector Destination = FVector::ZeroVector;
};

/** A Job.BattleCoverTrees tree standing on the stage and its cover points' registration with the cover finder. */
USTRUCT()
struct FRTSShortsCoverTreeState
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Tree;
	uint64 RegistrationId = 0;
	FVector TrunkLocation = FVector::ZeroVector;
};

/** Where Segment.BattleSoloEvasion's unit is in its stop, shoot, reverse cycle. */
UENUM()
enum class ERTSShortsSoloEvasionPhase : uint8
{
	WaitingForFireOrder,
	Holding,
	Reversing
};

/** Segment.BattleSoloEvasion running: the cycle, the attacker it faces and the follow camera's offsets. */
USTRUCT()
struct FRTSShortsSoloEvasionState
{
	GENERATED_BODY()

	bool bIsActive = false;
	ERTSShortsSoloEvasionPhase Phase = ERTSShortsSoloEvasionPhase::WaitingForFireOrder;
	float PhaseStartSeconds = 0.f;
	float LastTurnOrderSeconds = 0.f;
	TWeakObjectPtr<AActor> Unit;
	// Kept until it is dead: spreading shots over the attackers leaves all of them alive and firing.
	TWeakObjectPtr<AActor> Threat;
	FVector StartLocation = FVector::ZeroVector;
	FVector CameraStartLocation = FVector::ZeroVector;
	// Smoothed unit location the camera follows; its offset from StartLocation moves the camera.
	FVector CameraAnchor = FVector::ZeroVector;
	// Effects already running when the battle began: everything else stops with it, so the next round opens on a clean field.
	TSet<uint32> EffectIdsAtBegin;
};

/** How a Segment.BattleCuts shot places the camera round its unit. */
UENUM()
enum class ERTSShortsBattleShot : uint8
{
	// Behind the unit, off to one side, looking past it at what it shoots.
	OverShoulder,
	// Square to the line of fire, low: the unit's profile with its muzzle flash.
	Flank,
	// Ahead of the unit and off its line of fire, looking back at it.
	FrontQuarter,
	// Over the shoulder from twice as far and higher: the unit and the fight round it.
	Wide
};

/** Segment.BattleCuts running: the shot on screen, its unit and the smoothed points the camera follows. */
USTRUCT()
struct FRTSShortsBattleCutState
{
	GENERATED_BODY()

	bool bIsActive = false;
	// Shots begun so far (the opening shot of the field not counted).
	int32 ShotIndex = 0;
	float ShotStartSeconds = 0.f;
	float ShotEndSeconds = 0.f;
	float LastSeconds = 0.f;
	// The line unit filmed (a vehicle or a squad controller) and what it fights.
	TWeakObjectPtr<AActor> Subject;
	TWeakObjectPtr<AActor> Target;
	bool bSubjectIsPlayer = false;
	// Segment-local second the unit died; the shot ends DeathHoldSeconds later.
	float DiedAtSeconds = -1.f;
	// Consecutive frames the view from the unit to the camera was blocked.
	int32 BlockedFrames = 0;
	ERTSShortsBattleShot Shot = ERTSShortsBattleShot::OverShoulder;
	// Bearing (degrees) of the camera round the unit at the shot's start; the pan runs PanSign x PanDegrees from it.
	float BaseYaw = 0.f;
	float PanSign = 1.f;
	float Distance = 0.f;
	float Height = 0.f;
	float FocalLength = 18.f;
	float LookBlend = 0.f;
	// Smoothed ground point of the unit and of its target.
	FVector Anchor = FVector::ZeroVector;
	FVector TargetPoint = FVector::ZeroVector;
	// The last units filmed, newest last.
	TArray<TWeakObjectPtr<AActor>> Recent;
	FRandomStream Random;
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
	// Segment.BattleReinforcements, same indices: spawned into their line when the battle begins (so they count as
	// alive and their soldiers get absorbed) but held, ordered in at their OrderAtSeconds and released on arrival.
	TArray<TWeakObjectPtr<AActor>> Reinforcements;
	// Held reinforcements: they take no attack orders and nobody is ordered to attack them.
	TSet<TWeakObjectPtr<AActor>> HeldUnits;
	TArray<bool> ReinforcementOrdered;
	TArray<bool> ReinforcementJoined;
	// Segment.BattleFlankRoutes under way: these vehicles fire on the move but take no attack order until they finish.
	TArray<FRTSShortsBattleFlankState> Flanks;
	FRTSShortsSoloEvasionState Evasion;
	TMap<TWeakObjectPtr<AActor>, FRTSShortsReflankState> Reflanks;
	// Segment.BattleSquadTactics: one entry per enemy squad.
	TArray<FRTSShortsSquadTacticState> SquadTactics;
	// The squads' target's health at the last tactics tick and when it last went down.
	float SquadTargetHealth = -1.f;
	float SquadTargetDamagedSeconds = 0.f;
	// Health fraction each battle unit had last tick; 0 once its Kill event went out.
	TMap<TWeakObjectPtr<AActor>, float> LastHealth;
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
	// Crushable props laid in the vehicles' lanes; the ones still standing are removed when the convoy ends.
	TArray<TWeakObjectPtr<AActor>> CrushProps;
	int32 CrushedProps = 0;
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
	void ApplyTrackTrailArea() const;

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
	void SpawnBattleReinforcements(const FRTSShortsSegment& Segment);
	/** Pairs each Segment.BattleFlankRoutes entry with the enemy vehicle that drives it. */
	void BeginBattleFlanks(const FRTSShortsSegment& Segment);
	/** Sends flanking vehicles off at their time and releases them to the normal attack orders on arrival. */
	void TickBattleFlanks(int32 Frame);
	void OrderBattleFlankRoute(FRTSShortsBattleFlankState& Flank, const FRTSShortsBattleFlankRoute& Route, int32 Frame,
	                           float SegmentSeconds);
	/** True for a unit that moves and picks its targets itself for now: the director's attack orders would cancel that. */
	bool GetIsBattleUnitSelfDirected(const AActor* Unit) const;
	void BeginSoloEvasion(const FRTSShortsSegment& Segment, AActor* MainUnit);
	void TickSoloEvasion(int32 Frame);
	void TickSoloEvasionCycle(const FRTSShortsSoloEvasion& Settings, float SegmentSeconds);
	/**
	 * @brief Starts the short reverse that gives ground during a reload; reversing away from the threat also turns the
	 * front onto it, so one order does both.
	 * @param Settings The segment's evasion settings.
	 * @param SegmentSeconds Segment-local time, the start of the new phase.
	 */
	void EnterSoloReverse(const FRTSShortsSoloEvasion& Settings, float SegmentSeconds);
	void EnterSoloHold(float SegmentSeconds);
	void TurnSoloTowardsThreat(float SegmentSeconds);
	void PickSoloThreat();
	void AimSoloAtThreat() const;
	void UpdateSoloFollowCamera(const FRTSShortsSoloEvasion& Settings);
	/** Sends attackers the evading unit has turned to face round its side again, and back to attacking once there. */
	void TickBattleReflanks(const FRTSShortsSoloEvasion& Settings, float SegmentSeconds);
	/** Job.bStopWhenUnitDestroyed: later rounds are not rendered once the segment's unit lost; they stay skipped in the manifest. */
	void SkipSegmentsAfterRunning(int32 Frame);
	void OrderBattleReflank(AActor& Attacker, FRTSShortsReflankState& Reflank, const FRTSShortsSoloEvasion& Settings, float SegmentSeconds);
	/** Plants Job.BattleCoverTrees once per job and publishes cover points round each trunk. */
	void SpawnBattleCoverTrees();
	/** Takes the cover of trees that were crushed or destroyed off the cover finder. */
	void DropFallenCoverTrees();
	void BeginSquadTactics(const FRTSShortsSegment& Segment);
	/** Sends each enemy squad to a flank tree or spot at its time, and moves it again when it can no longer engage. */
	void TickSquadTactics(int32 Frame);
	/**
	 * @brief Picks where a squad fights the unit from next and orders it there: the best flank tree it can shoot from,
	 * with its soldiers on the trunk's cover points, or an open flank spot when no tree will do.
	 * @param Tactic The squad's state; Destination, TreeIndex and the timers are updated.
	 * @param Settings The segment's squad tactics.
	 * @param Target The unit the squad fights.
	 * @param SegmentSeconds Segment-local time of the order.
	 */
	void OrderSquadTactic(FRTSShortsSquadTacticState& Tactic, const FRTSShortsSquadTactics& Settings, AActor& Target,
	                      float SegmentSeconds, int32 AvoidTreeIndex = INDEX_NONE);
	/** @return False when no tree on the stage gives this squad cover it can shoot the unit from. */
	bool TryPlanSquadTreeCover(const FRTSShortsSquadTacticState& Tactic, const FRTSShortsSquadTactics& Settings,
	                           const AActor& Target, const TArray<class ASquadUnit*>& Soldiers, float WeaponRange,
	                           int32 AvoidTreeIndex, int32& OutTreeIndex, FVector& OutDestination,
	                           TArray<struct FSquadUnitPlannedDestination>& OutPlan) const;
	/** The open flank spot a squad without a usable tree goes to: the first candidate its soldiers can walk to. */
	FVector FindSquadFlankSpot(const FRTSShortsSquadTactics& Settings, const AActor& Target, const TArray<class ASquadUnit*>& Soldiers,
	                           float WeaponRange) const;
	/** Outlines the battle's living enemy soldiers and takes it off the dead, so only the remaining enemies stand out. */
	void TickSoldierOutlines(const FRTSShortsSquadTactics& Settings);
	/** The enemy squads' target under Segment.BattleSquadTactics: the evading unit, else the segment's own unit. */
	AActor* GetSquadTacticsTarget() const;
	/** Writes hits and kills of both lines into the manifest, so post can cut a fight down to its moments. */
	void TrackBattleEvents(int32 Frame);
	void TrackBattleLineEvents(const TArray<TWeakObjectPtr<AActor>>& Line, bool bPlayerSide, int32 Frame);
	void AddBattleEvent(const TCHAR* Kind, bool bPlayerSide, float HealthFraction, int32 Frame);
	/** True when Segment.BattleOrderMaxDistance is set and the target lies farther than it from the attacker. */
	bool GetIsBattleTargetTooFar(const AActor* Attacker, const AActor* Target) const;
	/** Orders the waiting reinforcements in at their time and adds the arrived ones to their line. */
	void TickBattleReinforcements(int32 Frame);
	/** Held-position battles: logs every squad's soldiers' stage-local X span now and then (who left their cover line). */
	void LogBattleSquadPositions(int32 Frame) const;
	/** Gives a battle squad's soldier the squad controller's owner (and its weapons and collision with it). */
	static void MatchSoldierToSquadOwner(class ASquadUnit* Soldier, const uint8 SquadOwner);
	/** Segment.bConvoy: spawns the wedge's side vehicles, pins the turrets and starts the follow camera. */
	void BeginConvoy(const int32 SegmentIndex, AActor* MainUnit);
	/** Orders every convoy vehicle Segment.ConvoyDriveDistance ahead (issued at the segment's fire time). */
	void OrderConvoyDrive(const int32 SegmentIndex);
	/** Holds the convoy's turrets and moves the follow camera. */
	void TickConvoy(const int32 Frame);
	void EndConvoy();
	// Lays Segment.CrushableDistances props in front of every convoy vehicle, sized to what its weight can crush.
	void SpawnConvoyCrushProps(const FRTSShortsSegment& Segment);
	/** Segment.StageActorName: the segment plays on its own stage actor; otherwise on the job's stage. */
	void ApplySegmentStage(const int32 SegmentIndex);
	/** Keeps the fog of war out, re-targets units whose target died, and ends the segment once one side is gone. */
	void TickBattle(const int32 Frame);
	void EndBattle();
	bool GetShouldPreRollTeamWeaponIntro() const;
	void BeginTeamWeaponShot(int32 SegmentIndex);
	/**
	 * @brief Gives each independently loaded crew a stable place beside its cinematic transport.
	 * @param VehicleActor Existing convoy or battle vehicle.
	 * @param SquadClassPath Crew controller Blueprint, which owns spawning the gun.
	 * @param Settings Shot mode and relative deployment position.
	 */
	void SpawnTeamWeaponPair(AActor* VehicleActor, const FString& SquadClassPath,
		const FRTSShortsTeamWeaponSettings& Settings);
	void TickTeamWeaponShot(int32 Frame);
	/**
	 * @brief Waits for native loading and state transitions before permitting movement or combat.
	 * @param Pair Transport, crew and gun whose state is being advanced.
	 * @param Frame Output timeline frame used for capture verification.
	 */
	void TickTeamWeaponPair(FRTSShortsTowPair& Pair, int32 Frame);
	/**
	 * @brief Uses the game's tow/deploy actions once the crew and its gun have finished loading.
	 * @param Pair Ready pair to hook or deploy.
	 * @param Shot Determines whether transport or deployment is required.
	 * @param Frame Output timeline frame recorded in the verification log.
	 */
	void PrepareTeamWeaponPair(FRTSShortsTowPair& Pair, ERTSShortsTeamWeaponShot Shot, int32 Frame);
	void StartPreparedTeamWeaponConvoy(int32 Frame);
	void LogTeamWeaponConvoyMotion(int32 Frame) const;
	bool GetShouldHoldTeamWeaponOrder(const AActor* Unit) const;
	void EndTeamWeaponShot();
	static void DestroyTeamWeaponPair(const FRTSShortsTowPair& Pair);

	UPROPERTY()
	TArray<FRTSShortsTowPair> M_TeamWeaponPairs;

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

	// Aircraft crash segments (RTSShortsAircraftCrash.cpp): the aircraft spawns when its segment begins, not in a pen.
	bool GetIsAircraftCrashSegment(int32 SegmentIndex) const;
	void BeginAircraftCrash(int32 SegmentIndex);
	class AAircraftMaster* SpawnCrashAircraft(const FRTSShortsSegment& Segment, const FTransform& SpawnTransform) const;
	void ApplyCrashCameraGrade(const FRTSShortsCameraParams& Params) const;
	void TickAircraftCrash(int32 LocalFrame);
	void KillCrashAircraft(AAircraftMaster& Aircraft, int32 LocalFrame);
	void UpdateCrashFollowCamera(const AAircraftMaster& Aircraft, const FRTSShortsAircraftCrash& Crash) const;
	/**
	 * @brief Stands the crash camera high beside the dive's path, so the fall and the impact are both in frame.
	 * @param Aircraft The falling aircraft; the camera is placed relative to the ground below it.
	 * @param Crash The segment's crash settings (offset in the run's frame, lens).
	 */
	void CutToCrashView(const AAircraftMaster& Aircraft, const FRTSShortsAircraftCrash& Crash);
	void UpdateCrashViewCamera(const FRTSShortsAircraftCrash& Crash);
	/** Records the impact and ends the segment once the crash camera has held on the wreck long enough. */
	void TrackCrashImpact(int32 LocalFrame, const FRTSShortsAircraftCrash& Crash);
	void EndAircraftCrash();
	/** Unique ids of the Niagara components active now; StopEffectsStartedSince stops every other active one later. */
	TSet<uint32> CollectActiveEffectIds() const;
	/**
	 * @brief Effects spawned at a location are owned by the world settings, a level actor FlushStageEffects keeps
	 * (crash smoke, cook-off explosions): stops every active one that was not active when the segment began.
	 * @param EffectIdsAtBegin What CollectActiveEffectIds returned at the segment's start.
	 * @return How many components were stopped.
	 */
	int32 StopEffectsStartedSince(const TSet<uint32>& EffectIdsAtBegin) const;

	// Ammo cook-off segments (RTSShortsCookOff.cpp): cook-off, death a second later, turret toss, cut once it landed.
	void BeginCookOff(int32 SegmentIndex);
	void TickCookOff(int32 LocalFrame);
	void TrackTossedTurret(int32 LocalFrame);
	void AimCookOffCamera(const FRTSShortsCookOff& CookOff);
	void OnCookOffActorSpawned(AActor* SpawnedActor);
	void EndCookOff();

	// A private copy of the designer template owns the drop; no mission ability or cooldown is modified.
	UPROPERTY()
	TObjectPtr<class UGA_Paratroopers> M_ParatrooperAbility;
	FTransform M_ParatrooperBattleCamera;
	bool bM_ParatrooperCameraCut = false;
	// Segment.BattleCuts (RTSShortsBattleCuts.cpp): close shots of single fighters after the opening shot.
	void BeginBattleCuts(const FRTSShortsSegment& Segment);
	void TickBattleCuts(int32 Frame);
	/** Picks the next unit (alternating the lines, engaged units first, no recent repeats) and cuts to it. */
	void StartBattleCutShot(const FRTSShortsBattleCuts& Settings, float SegmentSeconds, int32 Frame);
	/** @return False when a camera at this bearing would be hidden behind another unit or a prop. */
	bool GetIsBattleCutViewClear(const FVector& Anchor, float Yaw, float Distance, float Height, const AActor* Subject) const;
	/**
	 * @brief Moves the camera along the running shot: the pan angle at this second round the smoothed unit, kept above the ground.
	 * @param SegmentSeconds Segment-local time.
	 * @param bSnap The shot's first frame: the follow points jump to the unit instead of easing towards it.
	 */
	void PlaceBattleCutCamera(float SegmentSeconds, bool bSnap);
	void ApplyBattleCutLens() const;
	/**
	 * @brief Where a line unit stands: a vehicle's location, or the centre of a squad's living soldiers (with a team
	 * weapon's crew that is round the gun).
	 * @return False once the unit is gone (a squad without soldiers).
	 */
	bool GetBattleCutUnitPoint(const AActor* LineUnit, FVector& OutPoint) const;
	bool GetIsBattleCutUnitAlive(const AActor* LineUnit) const;
	/** Every line unit but a reinforcement still waiting for its move order. */
	bool GetIsBattleCutUnitFilmable(const AActor* LineUnit) const;
	/** A reinforcement ordered in that has not joined the fight yet. */
	bool GetIsBattleCutUnitDrivingIn(const AActor* LineUnit) const;
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
	/**
	 * @brief Lets a team weapon battle end close on the crews, once the pan has shown the whole field.
	 * @param OutFocus Centre of the guns whose crews are still alive.
	 * @return False when no gun is left to look at.
	 */
	bool GetSurvivingTeamWeaponsCentre(FVector& OutFocus) const;
	/** Spawns the running segment's Segment.FlyBys airframes, flies them for this segment-local time and removes
	 *  the previous segment's. */
	void UpdateFlyBys(const float SegmentSeconds);
	AActor* SpawnFlyByAirframe(const FRTSShortsFlyBy& FlyBy) const;
	/**
	 * The batched small-arms tracers and trace impacts are two Niagara systems on the projectile manager, which the
	 * game spawns at the world origin; their fixed bounds sit there too, so on a stage far from the origin both were
	 * culled and no tracer or impact rendered. Widens their bounds over the whole level and turns off their
	 * scalability culling.
	 */
	void KeepSmallArmsEffectsVisible();
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
	UPROPERTY()
	TArray<FRTSShortsFlyByState> M_FlyBys;
	// Segment whose Segment.FlyBys M_FlyBys holds (INDEX_NONE before the first update).
	int32 M_FlyBySegment = INDEX_NONE;
	// Niagara components of the projectile manager whose bounds were already widened.
	TSet<TWeakObjectPtr<class UNiagaraComponent>> M_WidenedSmallArmsSystems;
	FRTSShortsBattleState M_Battle;
	FRTSShortsBattleCutState M_BattleCuts;
	// Segment.BattleSquadTactics.bOutlineSoldiers: the post-process volume that draws the outlines, made once per job.
	UPROPERTY()
	TObjectPtr<class APostProcessVolume> M_SoldierOutlineVolume;
	// Job.BattleCoverTrees once planted: they stay for every later round.
	TArray<FRTSShortsCoverTreeState> M_CoverTrees;
	bool bM_CoverTreesSpawned = false;
	FRTSShortsConvoyState M_Convoy;
	UPROPERTY()
	FRTSShortsAircraftCrashState M_AircraftCrash;
	UPROPERTY()
	FRTSShortsCookOffState M_CookOff;
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
	// The running segment's unit lost and the job ends with this segment: the later segments never begin.
	bool bM_LaterSegmentsSkipped = false;
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
