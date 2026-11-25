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
struct FMapTestArmorHitReport;

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
	bool bRepeat = false;
	// Camera.bOrbitAroundUnit: the camera and its look-at turn round this point instead of round Pivot.
	bool bAroundUnit = false;
	FVector UnitPivot = FVector::ZeroVector;
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

/** One vehicle of a Segment.BattleAdvance line: where it drives, when, and whether it was released to fight. */
USTRUCT()
struct FRTSShortsAdvanceState
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Unit;
	FVector Destination = FVector::ZeroVector;
	float OrderAtSeconds = 0.f;
	bool bEnemy = false;
	bool bOrdered = false;
	bool bReleased = false;
	// Segment-local second it stopped short of its point; -1 while it is moving or has arrived.
	float StoppedSinceSeconds = -1.f;
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

/** One defender under Segment.BattleSoloEvasion: its stop, shoot, reverse cycle and the attacker it faces. */
USTRUCT()
struct FRTSShortsEvaderState
{
	GENERATED_BODY()

	ERTSShortsSoloEvasionPhase Phase = ERTSShortsSoloEvasionPhase::WaitingForFireOrder;
	float PhaseStartSeconds = 0.f;
	float LastTurnOrderSeconds = 0.f;
	TWeakObjectPtr<AActor> Unit;
	// Kept until it is dead: spreading shots over the attackers leaves all of them alive and firing.
	TWeakObjectPtr<AActor> Threat;
	// It gives ground at most RetreatRadiusCm from here.
	FVector StartLocation = FVector::ZeroVector;
};

/**
 * Segment.BattleSoloEvasion running: every defender's cycle (the segment's unit plus its tank allies, so a team of
 * defenders fights like the solo vehicle does) and the follow camera's offsets.
 */
USTRUCT()
struct FRTSShortsSoloEvasionState
{
	GENERATED_BODY()

	bool bIsActive = false;
	TArray<FRTSShortsEvaderState> Evaders;
	// The living defenders' centre when the battle began: the camera moves by how far that centre has moved since.
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
	// Segment.BattleAdvance: every vehicle of both lines, and the opening camera that follows the German line's vehicles.
	TArray<FRTSShortsAdvanceState> Advances;
	FVector AdvanceCameraStart = FVector::ZeroVector;
	FVector AdvanceAnchorStart = FVector::ZeroVector;
	FVector AdvanceAnchor = FVector::ZeroVector;
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

/** Segment.PenTest: one attacker of the running round and its single main-gun shot. */
struct FRTSShortsPenAttackerState
{
	TWeakObjectPtr<AActor> Unit;
	// Its largest-calibre weapon: the only one it may fire, once, from ReleaseSeconds on.
	TWeakObjectPtr<class UWeaponState> MainGun;
	TWeakObjectPtr<UObject> MainGunOwner;
	uint32 MainGunName = 0;
	float ReleaseSeconds = 0.f;
	bool bReleased = false;
	bool bFired = false;
	// The launch was seen in the projectile callback; the gun is blocked again on the next tick.
	bool bNeedsBlockAfterShot = false;
	bool bLost = false;
};

/** Segment.PenTest running: the attackers, the target and which manifest shot each launched shell belongs to. */
struct FRTSShortsPenTestState
{
	bool bIsActive = false;
	TWeakObjectPtr<AActor> Target;
	// Every weapon the director restricted (main guns between shots, all secondary weapons): freed when the round ends.
	TArray<TWeakObjectPtr<class UWeaponState>> RestrictedWeapons;
	TArray<TWeakObjectPtr<class UWeaponState>> TargetMainGuns;
	TArray<FRTSShortsPenAttackerState> Attackers;
	// Projectile launch id -> index into the manifest segment's PenShots.
	TMap<uint32, int32> ShotByLaunchId;
	int32 VolleyDoneFrame = -1;
	int32 LastShotFrame = -1;
	bool bTargetReleased = false;
	bool bRoundEnded = false;
	FDelegateHandle LaunchHandle;
	FDelegateHandle HitHandle;
	TSet<uint32> EffectIdsAtBegin;
};

/** Segment.Survival: one spawned wave unit until it is released to the battle's attack orders. */
struct FRTSShortsSurvivalPending
{
	TWeakObjectPtr<AActor> Unit;
	int32 SpawnFrame = 0;
	int32 WaveIndex = 0;
	// Settings.AdvanceOffsetX > 0: the world point it drives to; released when it gets there.
	bool bAdvancing = false;
	FVector AdvancePoint = FVector::ZeroVector;
};

/** Segment.Survival running: waves spawned so far, units on their way in, aircraft runs and the defenders' losses. */
struct FRTSShortsSurvivalState
{
	bool bIsActive = false;
	int32 NextWave = 0;
	// Round-robin index into Settings.SpawnOffsets and the frame each spot was last used.
	int32 NextSpawn = 0;
	TArray<int32> SpawnUsedFrame;
	TArray<FRTSShortsSurvivalPending> Pending;
	// Every wave unit per wave (for the first-seen frame) and the aircraft with their spawn frames.
	TArray<TArray<TWeakObjectPtr<AActor>>> WaveUnits;
	TArray<TWeakObjectPtr<AActor>> Aircraft;
	TArray<int32> AircraftFrames;
	// Defenders (the segment's unit, its allies and squads) and how many were alive last tick.
	TArray<TWeakObjectPtr<AActor>> Defenders;
	int32 DefendersAlive = 0;
	int32 EnemiesSpawned = 0;
	int32 BeginFrame = 0;
	// Diagnostics: the first shells of each side are logged with the distance to the nearest unit of the other side.
	FDelegateHandle LaunchHandle;
	int32 PlayerShellsLogged = 0;
	int32 EnemyShellsLogged = 0;
};

/** Segment.WallHoles running: the attackers, their aim points and the round's phase. */
struct FRTSShortsWallHolesState
{
	bool bIsActive = false;
	// The segment's own unit first, then the spawned copies (destroyed when the round ends).
	TArray<TWeakObjectPtr<AActor>> Attackers;
	TArray<TWeakObjectPtr<AActor>> SpawnedAttackers;
	// Where each attacker stands; one that drifts (drives toward its aim point) is put back.
	TArray<FVector> PlacedLocations;
	TArray<FRotator> PlacedRotations;
	TArray<int32> AimIndex;
	// Settings.AimGroups: each attacker's first and one-past-last index into AimPoints.
	TArray<int32> AimBlockStart;
	TArray<int32> AimBlockEnd;
	// Every weapon the round blocked (secondaries all round, main guns after the cease-fire): freed at the end.
	TArray<TWeakObjectPtr<class UWeaponState>> RestrictedWeapons;
	TSet<TWeakObjectPtr<UObject>> MainGunOwners;
	// The target's stage frame: origin on the ground at the target point.
	FVector TargetPoint = FVector::ZeroVector;
	float StageYaw = 0.f;
	float StartHealth = 0.f;
	// Every target (M_SegmentTarget first, then Settings.ExtraTargetOffsets) with its health when the round began, its
	// fraction last tick and its largest drop between two ticks (about one shell): the cease-fire looks one shell ahead.
	TArray<TWeakObjectPtr<AActor>> Targets;
	TArray<TWeakObjectPtr<AActor>> ExtraTargets;
	TArray<float> TargetStartHealth;
	TArray<float> TargetLastFraction;
	TArray<float> TargetLargestDrop;
	// Average over the targets, last tick.
	float LastHealthFraction = 1.f;
	int32 LastDamageFrame = -1;
	int32 BeginFrame = 0;
	int32 FireFrame = -1;
	int32 LastRetargetFrame = -1;
	int32 CeaseFrame = -1;
	int32 DeathFrame = -1;
	int32 NextShot = 0;
	// The shot being shown (index into Settings.Shots) and the frame it started, for glides.
	int32 ActiveShot = -1;
	int32 ActiveShotFrame = 0;
	bool bEnded = false;
	FDelegateHandle LaunchHandle;
};

/** Segment.ImpactShow running: the target, the main gun, its shells and the beat's camera. */
struct FRTSShortsImpactShowState
{
	bool bIsActive = false;
	TWeakObjectPtr<AActor> Unit;
	// The actor the unit is ordered to attack (a squad's soldier for a squad target), null for ground fire.
	TWeakObjectPtr<AActor> AttackTarget;
	TWeakObjectPtr<class UWeaponState> MainGun;
	TWeakObjectPtr<UObject> MainGunOwner;
	uint32 MainGunName = 0;
	TArray<TWeakObjectPtr<class UWeaponState>> RestrictedWeapons;
	// Launch id of every beat shell, mapped to its 0-based index.
	TMap<uint32, int32> ShellByLaunchId;
	TSet<int32> ShellsWithImpact;
	TMap<uint32, FString> VerdictByLaunchId;
	// Squad soldiers whose animation was forced to tick (offscreen renders froze them mid-stride, round 1).
	TSet<TWeakObjectPtr<AActor>> AnimForced;
	// The target's stage frame: origin on the ground at the target point.
	FVector TargetPoint = FVector::ZeroVector;
	float StageYaw = 0.f;
	int32 BeginFrame = 0;
	int32 FireFrame = -1;
	int32 FirstLaunchFrame = -1;
	int32 FirstImpactFrame = -1;
	FVector FirstImpactLocation = FVector::ZeroVector;
	int32 LastShellImpactFrame = -1;
	// The impact the close shot frames: the first one, then the first that showed the beat if that came later.
	int32 FramedImpactFrame = -1;
	FVector FramedImpactLocation = FVector::ZeroVector;
	// The settle glide onto the framed impact starts from the frame origin the shot had then.
	int32 SettleFromFrame = -1;
	FVector SettleFrom = FVector::ZeroVector;
	FVector LastFrameOrigin = FVector::ZeroVector;
	int32 NextShot = 0;
	int32 ActiveShot = -1;
	int32 ActiveShotFrame = 0;
	// Output frame the slow motion began (-1: none yet), and whether it has ended.
	int32 SlowMoFrame = -1;
	bool bSlowMoDone = false;
	bool bGunBlocked = false;
	// Settings.ReleaseGunAtSeconds passed (or no release time): the main gun may fire once ordered.
	bool bGunReleased = false;
	// 1-based attempt (see FRTSShortsImpactShow::MaxAttempts) and where the latest attempt aimed.
	int32 Attempt = 1;
	// Shells fired before the latest attempt, so a burst (Shells > 1) counts its own shells.
	int32 AttemptShellBase = 0;
	// Frame of the latest beat-shell launch and of the latest squad re-target (bursts only).
	int32 LastLaunchFrame = -1;
	int32 LastRetargetFrame = -1;
	FVector AimPoint = FVector::ZeroVector;
	bool bEnded = false;
	FDelegateHandle LaunchHandle;
	FDelegateHandle ImpactHandle;
	FDelegateHandle ArmorHandle;
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
enum class ERTSShortsShowcaseStep : uint8
{
	Boot,
	WaitForStart,
	StillsScrollKit,
	StillsKitEnd,
	OpenPreview,
	Running,
	BetweenPreviews,
	Done
};

/** Job.CommanderShowcase running (RTSShortsCommanderShowcase.cpp): where the walk through the kit is. */
struct FRTSShortsShowcaseState
{
	ERTSShortsShowcaseStep Step = ERTSShortsShowcaseStep::Boot;
	TWeakObjectPtr<class AFactionPlayerController> Controller;
	TWeakObjectPtr<class UW_CommanderSelectionMenu> Menu;
	TArray<int32> Slots;
	int32 SlotListIndex = 0;
	double NextActionWorldSeconds = 0.0;
	double StepStartWorldSeconds = 0.0;
	double FinishedWorldSeconds = -1.0;
	double StillsStartWorldSeconds = 0.0;
	bool bLaunchRequested = false;
	bool bVariantsConfirmed = false;
	bool bSoundMixPushed = false;
	bool bStillCardTaken = false;
	// Render only: the preview camera slid sideways so the preview's target units are in the middle of the 9:16 frame.
	TWeakObjectPtr<class UCameraComponent> FocusedCamera;
	FVector CameraRelativeBeforeFocus = FVector::ZeroVector;
	bool bCameraFocusDone = false;
	bool bWasAttacking = false;
};

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
	// Job.CommanderShowcase (RTSShortsCommanderShowcase.cpp): the commander screen and every kit ability's preview.
	bool GetIsCommanderShowcase() const { return M_Job.CommanderShowcase.Commander != ERTSCommander::NoCommander; }
	void TickCommanderShowcase();
	bool EnsureShowcaseCommanderScreen(double Now);
	void FillShowcaseManifest();
	void OpenShowcasePreview(int32 Frame, double Now);
	void TickShowcasePreview(int32 Frame, double Now);
	void CloseShowcasePreview(int32 Frame, double Now, const FString& Reason);
	void FinishCommanderShowcase(int32 Frame);
	int32 GetShowcaseFrame(double Now) const;
	void TakeShowcaseStill(const FString& Name) const;
	void ScrollShowcaseKitToEnd() const;
	void ApplyShowcaseCameraFieldOfView() const;
	void FocusShowcaseCameraOnTargets(double Now);
	void RestoreShowcaseCamera();
	void TrackShowcaseAttackRuns(int32 Frame);
	FRTSShortsShowcaseState M_Showcase;

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
	/**
	 * Size and rate-of-fire stacks (RTSShortsMainGunLog.cpp): logs the placed unit's hull and with-gun length
	 * (RTS_SHORTS_HULL) and its main-gun weapons (RTS_SHORTS_MAINGUN); Segment.bMainGunOnly blocks every smaller weapon.
	 */
	void BeginMainGunLog(const int32 SegmentIndex, AActor* Unit);
	/** One RTS_SHORTS_ROUND line per main-gun round (magazine drop), RTS_SHORTS_RELOAD on reload start and end. */
	void TickMainGunLog(const int32 Frame);
	void EndMainGunLog();
	void LogUnitHullSize(const int32 SegmentIndex, const AActor* Unit) const;
	/** Segment.bDisarmTarget: the spawned dummy target can be shot but never fires back. */
	void DisarmSegmentTarget() const;
	void PlaceSquadMembers(AActor* Unit, const FVector& Location, const FRotator& Rotation) const;
	void FrameCamera(const AActor* Unit, const FRTSShortsSegment& Segment, const FVector& TargetWorld) const;
	void FrameImpactCamera(const AActor* Unit, const FRTSShortsSegment& Segment) const;
	/** Segment.BattleEnemyClassPaths: spawns both lines (the segment's unit leads the player line) and makes them see each other. */
	void BeginBattle(const int32 SegmentIndex, AActor* MainUnit);
	/** Segment.BattleEnemyBehaviourClassPaths: adds each behaviour to one enemy vehicle (squads are skipped). */
	void AddBattleEnemyBehaviours(const FRTSShortsSegment& Segment, AActor* Enemy) const;
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
	/** Segment.BattleAdvance: holds every vehicle of both lines and works out where each one drives. */
	void BeginBattleAdvance(const FRTSShortsSegment& Segment);
	/** Orders the advance moves at their seconds and releases each vehicle to fight once it arrives or times out. */
	void TickBattleAdvance(int32 Frame);
	void OrderBattleAdvanceMove(FRTSShortsAdvanceState& Advance, int32 Frame);
	/**
	 * @brief Arrived, stopped short of its point, or timed out: from then on the vehicle fights like the rest of the line.
	 * @param Advance Updated with when it stopped short.
	 * @param Settings The segment's advance.
	 * @param SegmentSeconds Segment-local time now.
	 * @return True when it should be released to take and receive attack orders.
	 */
	bool GetHasBattleAdvanceFinished(FRTSShortsAdvanceState& Advance, const FRTSShortsBattleAdvance& Settings, float SegmentSeconds) const;
	void ReleaseBattleAdvanceUnit(FRTSShortsAdvanceState& Advance, int32 Frame, float SegmentSeconds);
	/** Keeps the opening camera with the German line as it drives forward and fights (until the first battle cut). */
	void UpdateAdvanceFollowCamera(const FRTSShortsBattleAdvance& Settings);
	bool GetIsBattleAdvanceUnit(const AActor* LineUnit) const;
	bool GetBattleAdvanceAnchor(FVector& OutAnchor) const;
	void BeginSoloEvasion(const FRTSShortsSegment& Segment, AActor* MainUnit);
	void TickSoloEvasion(int32 Frame);
	void TickSoloEvasionCycle(const FRTSShortsSoloEvasion& Settings, float SegmentSeconds);
	/**
	 * @brief Starts the short reverse that gives ground during a reload; reversing away from the threat also turns the
	 * front onto it, so one order does both.
	 * @param Settings The segment's evasion settings.
	 * @param SegmentSeconds Segment-local time, the start of the new phase.
	 */
	void EnterSoloReverse(FRTSShortsEvaderState& Evader, const FRTSShortsSoloEvasion& Settings, float SegmentSeconds);
	void TickEvaderCycle(FRTSShortsEvaderState& Evader, const FRTSShortsSoloEvasion& Settings, float SegmentSeconds);
	void EnterSoloHold(FRTSShortsEvaderState& Evader, float SegmentSeconds);
	void TurnSoloTowardsThreat(FRTSShortsEvaderState& Evader, float SegmentSeconds);
	void PickSoloThreat(FRTSShortsEvaderState& Evader);
	void AimSoloAtThreat(const FRTSShortsEvaderState& Evader) const;
	/** The defender's gun range for picking threats: its shortest turret's, else its largest weapon's. */
	float GetEvaderRange(const AActor* Unit) const;
	bool GetIsThreatInRange(const FRTSShortsEvaderState& Evader) const;
	/** The living defender nearest to the actor (null when every defender is dead). */
	AActor* GetNearestEvader(const AActor& From) const;
	bool GetIsEvader(const AActor* Unit) const;
	/** Centre of the living defenders; false when none is left. */
	bool GetEvadersCentre(FVector& OutCentre) const;
	void UpdateSoloFollowCamera(const FRTSShortsSoloEvasion& Settings);
	/** Sends attackers the evading unit has turned to face round its side again, and back to attacking once there. */
	void TickBattleReflanks(const FRTSShortsSoloEvasion& Settings, float SegmentSeconds);
	/** Job.bStopWhenUnitDestroyed: later rounds are not rendered once the segment's unit lost; they stay skipped in the manifest. */
	void SkipSegmentsAfterRunning(int32 Frame);
	// Segment.PenTest (RTSShortsPenTest.cpp): one main-gun shell per attacker at the segment unit's front.
	void BeginPenTest(const FRTSShortsSegment& Segment, AActor* MainUnit);
	void TickPenTest(int32 Frame);
	void EndPenTest();
	/** Restricts every weapon of the unit but its largest-calibre one; returns that one (null before the turrets are up). */
	class UWeaponState* RestrictSecondaryWeapons(AActor* Unit);
	void SetPenWeaponBlocked(class UWeaponState* Weapon, bool bBlocked);
	/** Projectile callbacks: only record, the round is advanced in TickPenTest. */
	void OnPenShellLaunched(uint32 LaunchId, const UObject* WeaponOwner, uint32 WeaponName);
	void OnPenArmorHit(const FMapTestArmorHitReport& Report);
	/** Where a world point lands on the rendered frame (0..1 from the top left), or negative when behind the camera. */
	FVector2D ProjectToFrame(const FVector& WorldLocation) const;
	// Segment.Survival (RTSShortsSurvival.cpp): defenders hold a hilltop against tiered waves until the last one dies.
	void BeginSurvival(int32 SegmentIndex);
	void TickSurvival(int32 Frame);
	void EndSurvival();
	void SpawnSurvivalWave(int32 WaveIndex, int32 Frame);
	AActor* SpawnSurvivalAircraft(const FString& ClassPath, int32 Frame);
	/** Next spawn spot (stage-local cm) the camera cannot see and no unit took in the last seconds; false if none. */
	bool PickSurvivalSpawn(FVector& OutWorldLocation, int32 Frame);
	void ReleaseSurvivalUnits(int32 Frame);
	void RecordSurvivalSightings(int32 Frame);
	/** Adds Segment.Survival's range behaviour to a unit or squad controller once. */
	void ApplySurvivalRange(AActor* Unit) const;
	/** True when the point is inside the render camera's view and no landscape stands between them. */
	bool GetIsPointInCameraView(const FVector& Point) const;
	/** Survival defenders take attack orders only on enemies within their weapons' reach, so they never leave the hill. */
	bool GetIsSurvivalTargetOutOfReach(const AActor* Attacker, const AActor* Target) const;

	// Segment.WallHoles (RTSShortsWallHoles.cpp): four attackers of one type shoot holes into a fresh building.
	/** Spawns the round's target as a plain actor, turned and centred as Segment.WallHoles asks. */
	AActor* SpawnWallHolesTarget(const FRTSShortsSegment& Segment, const FVector& TargetWorld);
	void BeginWallHoles(int32 SegmentIndex, AActor* MainUnit);
	void TickWallHoles(int32 Frame);
	void EndWallHoles();
	void OrderWallHolesFire(int32 SegmentIndex);
	void AimWallHolesAttackers(const FRTSShortsWallHoles& Settings);
	void CeaseWallHolesFire(int32 Frame, const FString& Reason);
	/** Blocks every weapon of the unit but its largest-calibre one; returns that one (null before the turrets are up). */
	class UWeaponState* BlockWallHolesSecondaries(AActor* Unit);
	void SetWallHolesWeaponBlocked(class UWeaponState* Weapon, bool bBlocked);
	/** The target's health over the health it had when the round began (1 when unknown, 0 when gone or dead). */
	float GetWallHolesHealthFraction() const;
	/** One target's health over its health when the round began (1 when unknown, 0 when gone or dead). */
	float GetWallHolesTargetFraction(int32 TargetIndex) const;
	void RecordWallHolesDamage();
	FVector WallHolesFramePoint(const FVector& Offset) const;
	/** Job.StageProps: spawned once, when the first segment begins. */
	void SpawnStageProps();
	/** Applies Shots[Index]'s pose for the frame (cut on its first frame, glide after). */
	void ApplyWallHolesShot(int32 Index, int32 Frame);

	// Segment.ImpactShow (RTSShortsImpactShow.cpp): one beat of "every impact of the X".
	/** Spawns the beat's target (plain actor or enemy unit) on the target point; null for a ground beat. */
	AActor* SpawnImpactShowTarget(const FRTSShortsSegment& Segment, const FVector& TargetWorld);
	void BeginImpactShow(int32 SegmentIndex, AActor* MainUnit);
	void TickImpactShow(int32 Frame);
	void EndImpactShow();
	void OrderImpactShowFire(int32 SegmentIndex);
	void OnImpactShowImpact(const struct FMapTestImpactReport& Report);
	void SetImpactShowWeaponBlocked(class UWeaponState* Weapon, bool bBlocked);
	FVector ImpactShowFramePoint(const FVector& Offset) const;
	void ApplyImpactShowShot(int32 Index, int32 Frame);
	void SetImpactShowTimeDilation(float Dilation) const;
	/** The landscape under the beat's target point gets the physical material and blocks both projectile channels. */
	void MakeImpactShowGroundLikeGame(const FString& PhysMaterialPath);
	void OrderBattleReflank(AActor& Attacker, AActor& Defender, FRTSShortsReflankState& Reflank, const FRTSShortsSoloEvasion& Settings,
	                        float SegmentSeconds);
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
	/** @return How many looping fires a dead tank left burning at the stage were stopped before the next tank. */
	int32 StopLingeringStageFires(int32 SegmentIndex) const;
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
	/** Shortest weapon range of any turret on the unit (cupola MGs and second turrets included); 0 = no turret range. */
	float ReadShortestTurretRange(const AActor* Unit) const;
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
	/** The segment unit's largest-calibre weapons (every barrel of a multi-gun mount) and their last magazine reading. */
	struct FMainGunLogState
	{
		int32 Segment = -1;
		TArray<TWeakObjectPtr<class UWeaponState>> Weapons;
		TArray<int32> LastMag;
		/** Projectiles one magazine round sends out: a fire socket each on multi-barrel weapons (Flakvierling), else 1. */
		TArray<int32> ProjectilesPerRound;
		TArray<bool> bWasReloading;
		/** Smaller weapons blocked by Segment.bMainGunOnly; released at the segment's end. */
		TArray<TWeakObjectPtr<class UWeaponState>> Blocked;
		int32 Rounds = 0;
	};
	FMainGunLogState M_MainGunLog;
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
	FRTSShortsPenTestState M_PenTest;
	FRTSShortsWallHolesState M_WallHoles;
	FRTSShortsImpactShowState M_ImpactShow;
	FRTSShortsSurvivalState M_Survival;
	bool bM_StagePropsSpawned = false;
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
