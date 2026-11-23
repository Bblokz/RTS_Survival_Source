// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "RTSShortsJobTypes.generated.h"

/**
 * @brief Camera placement relative to the unit on the stage for one segment of a short.
 * Distances are in centimetres in the unit's local frame (forward = firing direction).
 */
USTRUCT()
struct FRTSShortsCameraParams
{
	GENERATED_BODY()

	/** Distance behind the unit's bounds centre along its forward axis. */
	UPROPERTY()
	float BackDistance = 900.f;

	/** Offset along the unit's right axis; negative puts the camera on the left. */
	UPROPERTY()
	float SideOffset = -550.f;

	/**
	 * Moves the camera and its look-at point together along the unit's right axis (SideOffset moves only the
	 * camera): centres a battle line whose slots are not symmetric about the unit, e.g. six abreast.
	 */
	UPROPERTY()
	float LateralShift = 0.f;

	/** Height above the unit's ground contact. */
	UPROPERTY()
	float Height = 420.f;

	/** 0 looks at the unit centre, 1 at the impact point; values between frame both. */
	UPROPERTY()
	float LookAtBlend = 0.35f;

	/** Height added to the look-at point. */
	UPROPERTY()
	float LookAtHeight = 120.f;

	/** Cine camera focal length in mm (9:16 filmback 13.5 x 24 mm). */
	UPROPERTY()
	float FocalLength = 35.f;

	/** Extra pitch in degrees applied after the look-at, positive tilts up. */
	UPROPERTY()
	float ExtraPitch = 0.f;

	/** Auto-exposure bias applied on the camera for this cut (overrides the look volume). */
	UPROPERTY()
	float ExposureBias = 1.5f;

	/**
	 * > 0: ignore BackDistance and place the camera so the unit's front silhouette fills this fraction of the frame
	 * width (from the unit's bounds and the lens), never closer than MinDistance. Keeps every vehicle the same size.
	 */
	UPROPERTY()
	float FitWidthFraction = 0.f;

	UPROPERTY()
	float MinDistance = 400.f;

	/** With FitWidthFraction: Height = max(Height, HeightPerDistance x distance) and LookAtHeight = LookAtPerDistance x distance (0 = absolute values). */
	UPROPERTY()
	float HeightPerDistance = 0.f;

	UPROPERTY()
	float LookAtPerDistance = 0.f;

	/** With FitWidthFraction: camera pitch in degrees (> 0): the fitted distance is laid out as horizontal = d cos, height = LookAtHeight + d sin. */
	UPROPERTY()
	float FitPitch = 0.f;

	/** With FitPitch: yaw of the camera around the unit, degrees off the unit's front axis (sign of BackDistance picks front or back). */
	UPROPERTY()
	float FitAzimuth = 0.f;

	/** Post-process colour saturation multiplier on the cine camera (1 = untouched). */
	UPROPERTY()
	float ColorSaturation = 1.f;

	/** Post-process white balance temperature in Kelvin (6500 = untouched; lower warms the image). */
	UPROPERTY()
	float WhiteTemp = 6500.f;

	/** Centimetres the aim point is moved along the unit's forward axis (side profiles: puts the muzzle inside the frame). */
	UPROPERTY()
	float LookAtForward = 0.f;

	/** Degrees the camera travels around the unit during the segment (0 = static; 180 = from the front round to the rear). */
	UPROPERTY()
	float OrbitDegrees = 0.f;

	/** Segment-local second at which the orbit pan starts (after the shot, so the reload is seen from behind). */
	UPROPERTY()
	float OrbitStartSeconds = 0.f;

	/** Duration of the orbit pan in seconds (eased in and out). */
	UPROPERTY()
	float OrbitSeconds = 3.f;

	/**
	 * Opt-in descent during the orbit: the camera's height above the look-at point eases to this fraction of its
	 * framing height (1 keeps the height; 0.4 ends at 40 %), so a high overview can swing down to a low side view.
	 */
	UPROPERTY()
	float OrbitEndHeightScale = 1.f;

	/** Seconds the camera rests at the end of the orbit before the return sweep. */
	UPROPERTY()
	float OrbitHoldSeconds = 0.f;

	/**
	 * Opt-in second sweep after the hold, added to the orbit's angle: -2 x OrbitDegrees passes back over the starting view
	 * to the same distance on the other side. The height eases back to the framing height by the starting view.
	 */
	UPROPERTY()
	float OrbitReturnDegrees = 0.f;

	/** Duration of the return sweep in seconds (eased in and out). */
	UPROPERTY()
	float OrbitReturnSeconds = 3.f;

	/**
	 * Opt-in endless pan: after the return sweep the camera keeps sweeping back and forth over the same arc
	 * (ReturnDegrees, then -ReturnDegrees, ...), resting OrbitHoldSeconds at each end. For shorts with no fixed length.
	 */
	UPROPERTY()
	bool bOrbitRepeat = false;

	/**
	 * Opt-in: the pan turns round the framed unit's line (its location plus LateralShift) instead of round the look-at
	 * point, turning the view with it, so a close camera keeps the unit in frame while the view sweeps across.
	 */
	UPROPERTY()
	bool bOrbitAroundUnit = false;

	/**
	 * Team weapon battles: segment-local second at which the camera pushes in on the guns still standing, to show their
	 * crews at work (0 = no push-in). The focus is fixed when the push-in starts.
	 */
	UPROPERTY()
	float OrbitPushInStartSeconds = 0.f;

	/** Duration of the push-in in seconds (eased in and out). */
	UPROPERTY()
	float OrbitPushInSeconds = 6.f;

	/** The camera's distance from its focus at the end of the push-in, as a fraction of the orbit's distance. */
	UPROPERTY()
	float OrbitPushInDistanceScale = 0.4f;
};

/**
 * @brief Second camera that a segment cuts to once the shell is in the air: placed around the impact point,
 * looking back toward the unit so the impact is large and the vehicle sits behind it.
 */
USTRUCT()
struct FRTSShortsImpactCameraParams
{
	GENERATED_BODY()

	/** Distance from the impact point. */
	UPROPERTY()
	float Distance = 900.f;

	/** Angle around the impact point, 0 = on the far side looking back along the firing line toward the unit. */
	UPROPERTY()
	float AngleDeg = 30.f;

	UPROPERTY()
	float Height = 260.f;

	/** 0 looks at the impact point, 1 at the unit. */
	UPROPERTY()
	float UnitLookBlend = 0.25f;

	UPROPERTY()
	float LookAtHeight = 60.f;

	UPROPERTY()
	float FocalLength = 35.f;

	/**
	 * Over-the-shoulder mode: the camera sits BehindDistance behind the unit and SideOffset to its right
	 * (negative = left), looking along the firing line, so a shell that lands short or long stays in frame.
	 * Distance and AngleDeg are ignored in this mode.
	 */
	UPROPERTY()
	bool bOverShoulder = false;

	UPROPERTY()
	float BehindDistance = 350.f;

	UPROPERTY()
	float SideOffset = -380.f;

	/** Auto-exposure bias for this cut; the rear of the vehicle is backlit, so it sits above camera A's bias. */
	UPROPERTY()
	float ExposureBias = 2.7f;

	/** Post-process colour saturation multiplier (1 = untouched). */
	UPROPERTY()
	float ColorSaturation = 1.f;
};

/** @brief Extends a paratrooper camera move around the battle without restarting the fight. */
USTRUCT()
struct FRTSShortsParatrooperCameraWaypoint
{
	GENERATED_BODY()

	/** Travel time from the preceding viewpoint; nonpositive values skip this waypoint. */
	UPROPERTY()
	float DurationSeconds = 0.f;

	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	UPROPERTY()
	FVector LookOffset = FVector(0.f, 0.f, 150.f);

	UPROPERTY()
	float FocalLength = 14.f;
};

/** @brief Opt-in camera route from the landing overview, relative to the battle midpoint. */
USTRUCT()
struct FRTSShortsParatrooperCameraMove
{
	GENERATED_BODY()

	// Zero keeps the existing fixed battle camera for presets that have not requested a move.
	UPROPERTY()
	float DurationSeconds = 0.f;

	UPROPERTY()
	FVector EndOffset = FVector(-2400.f, -1100.f, 300.f);

	UPROPERTY()
	FVector EndLookOffset = FVector(500.f, -700.f, 240.f);

	UPROPERTY()
	float EndFocalLength = 14.f;

	/** Additional viewpoints continue after the original endpoint; empty preserves existing presets. */
	UPROPERTY()
	TArray<FRTSShortsParatrooperCameraWaypoint> ContinuationPoints;
};

/** @brief Places one aircraft pass independently of the battle's camera and unit origins. */
USTRUCT()
struct FRTSShortsParatrooperPass
{
	GENERATED_BODY()

	/** Ground target in stage-local centimetres. */
	UPROPERTY()
	FVector DropOffset = FVector::ZeroVector;

	/** Approach relative to DropOffset, including flight altitude. */
	UPROPERTY()
	FVector AircraftStartOffset = FVector(-6000.f, 0.f, 3500.f);
};

/** @brief Overrides only this short's copied ability when the drop belongs to another side or spans several passes. */
USTRUCT()
struct FRTSShortsParatrooperFormation
{
	GENERATED_BODY()

	/** Zero inherits the job's owning player. */
	UPROPERTY()
	int32 OwningPlayer = 0;

	/** Zero retains the template's circular landing zone; positive values spread landings along the flight. */
	UPROPERTY()
	float LandingTrailLength = 0.f;

	/** Zero inherits the ability template's timing between individual jumps. */
	UPROPERTY()
	float JumpIntervalSeconds = 0.f;

	/** Empty inherits the template's squads; each pass drops one copy of this list. */
	UPROPERTY()
	TArray<ESquadSubtype> SquadSubtypes;

	/** Empty retains the original single pass over the battle midpoint. */
	UPROPERTY()
	TArray<FRTSShortsParatrooperPass> Passes;
};

/** @brief Selects the two reusable shots without changing other short categories. */
UENUM()
enum class ERTSShortsTeamWeaponShot : uint8
{
	None,
	TowConvoy,
	DeployBattle
};

/** @brief Pairs the shot's transports with real crew controllers for transport or deployment. */
USTRUCT()
struct FRTSShortsTeamWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY()
	ERTSShortsTeamWeaponShot Shot = ERTSShortsTeamWeaponShot::None;

	/** One crew-controller class per transport, in the same order as its convoy or battle line. */
	UPROPERTY()
	TArray<FString> SquadClassPaths;

	/** Ground offset beside each half-track after the cut to the deployment yard. */
	UPROPERTY()
	FVector DeploymentOffset = FVector(-250.f, 650.f, 0.f);
};

/** @brief A cinematic aircraft (an animated airframe, no unit or AI) that flies one straight line over the stage. */
USTRUCT()
struct FRTSShortsFlyBy
{
	GENERATED_BODY()

	/** Skeletal mesh of the airframe. */
	UPROPERTY()
	FString MeshPath;

	/** Looping animation on the airframe (the propeller spin); empty plays none. */
	UPROPERTY()
	FString AnimPath;

	/** Optional static mesh attached at the airframe's origin: the Soviet aircraft are a static frame plus a
	 *  skeletal rotor mesh (MeshPath) authored in the same space. */
	UPROPERTY()
	FString FrameMeshPath;

	/** Path start and end in stage-local centimetres (X forward, Y right, Z above the stage origin). */
	UPROPERTY()
	FVector StartOffset = FVector(-25000.f, 2500.f, 3000.f);

	UPROPERTY()
	FVector EndOffset = FVector(25000.f, 2500.f, 3000.f);

	/** Segment-local seconds at which the aircraft is at StartOffset; it is hidden outside the flight. */
	UPROPERTY()
	float StartSeconds = 0.f;

	UPROPERTY()
	float DurationSeconds = 8.f;

	/** Yaw added to the flight direction so the nose leads (a mesh modelled along +Y needs -90). */
	UPROPERTY()
	float MeshYawOffset = 0.f;

	/** Roll in degrees for a banking pass. */
	UPROPERTY()
	float BankDegrees = 0.f;

	UPROPERTY()
	float Scale = 1.f;

	/** Bones hidden with their children (the landing gear, which the propeller animations leave lowered). */
	UPROPERTY()
	TArray<FName> HiddenBones;

	/** One-shot 2D sound started SoundAtSeconds into the segment (a recorded fly-by peaks mid-file); empty = silent. */
	UPROPERTY()
	FString SoundPath;

	UPROPERTY()
	float SoundAtSeconds = 0.f;

	UPROPERTY()
	float SoundVolume = 1.f;
};

/**
 * @brief Segment Kind "AircraftCrash": the segment's aircraft (ClassPath, an *_Airborne blueprint) spawns in flight,
 * flies a straight run, is shot down at KillAtSeconds and is filmed by a chase camera, then from a high side camera
 * that follows the dive and holds on the wreck. Offsets are in centimetres.
 */
USTRUCT()
struct FRTSShortsAircraftCrash
{
	GENERATED_BODY()

	/** Spawn point in stage-local space (X forward, Y right, Z above the stage origin). */
	UPROPERTY()
	FVector StartOffset = FVector(-9000.f, 0.f, 2600.f);

	/** Direction of the run in degrees from the stage forward. */
	UPROPERTY()
	float RunYaw = 0.f;

	/** Segment-local seconds at which the aircraft takes fatal damage. */
	UPROPERTY()
	float KillAtSeconds = 3.f;

	/** Seconds after the kill when the chase camera cuts to the crash view (the fire and the nose drop read first). */
	UPROPERTY()
	float CrashViewAfterKillSeconds = 1.2f;

	/** Chase camera offset from the aircraft in the run's frame (X along the run, Y right, Z up). */
	UPROPERTY()
	FVector FollowOffset = FVector(-2400.f, -1300.f, 450.f);

	/** The chase camera aims this far ahead of the aircraft along the run. */
	UPROPERTY()
	float FollowLookAhead = 900.f;

	UPROPERTY()
	float FollowFocalLength = 14.f;

	/** Crash camera offset in the run's frame from the ground point below the aircraft at the cut; Z is the height
	 *  above that ground. Positive X stands it ahead, where the dive comes down. */
	UPROPERTY()
	FVector CrashViewOffset = FVector(2600.f, -5200.f, 2600.f);

	UPROPERTY()
	float CrashViewFocalLength = 12.f;

	/** How fast (1/s) the crash camera's aim catches up with the falling aircraft; lower lags more. */
	UPROPERTY()
	float CrashViewTrackSpeed = 5.f;

	/** Seconds the crash camera holds on the burning wreck before the next segment starts. */
	UPROPERTY()
	float HoldAfterImpactSeconds = 3.f;
};

/**
 * @brief Ammo cook-off death of the segment's tank: its ammunition starts cooking off at StartAtSeconds, it dies
 * DieAfterSeconds later (the cook-off death throws the wreck turret), and the segment ends HoldAfterLandSeconds after
 * the turret has come down and settled. StartAtSeconds < 0 leaves the segment a normal one.
 */
USTRUCT()
struct FRTSShortsCookOff
{
	GENERATED_BODY()

	UPROPERTY()
	float StartAtSeconds = -1.f;

	UPROPERTY()
	float DieAfterSeconds = 1.f;

	UPROPERTY()
	float HoldAfterLandSeconds = 1.f;

	/** Seconds after the death when the segment ends even if no thrown turret was seen landing (hull-only wrecks). */
	UPROPERTY()
	float MaxAfterDeathSeconds = 7.f;

	/** Seconds after the death without a tossed turret before the segment treats the wreck as hull-only. */
	UPROPERTY()
	float HullOnlyHoldSeconds = 3.f;

	/** How far the camera's aim moves from the tank toward the thrown turret (0 = never, 1 = onto the turret). */
	UPROPERTY()
	float TurretAimBlend = 0.55f;

	/** How fast (1/s) the aim follows; lower lags more and keeps the motion calm. */
	UPROPERTY()
	float AimTrackSpeed = 2.5f;

	/**
	 * Fuel cook-off instead: the engine catches fire at StartAtSeconds (it leaks burning fuel), and the death that follows
	 * is the fuel explosion; heavy tanks with an engine deck throw it, which the camera follows like a turret.
	 */
	UPROPERTY()
	bool bEngineFire = false;
};

/** @brief A battle unit that waits far from the lines and drives in later, joining its side when it arrives. */
USTRUCT()
struct FRTSShortsBattleReinforcement
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassPath;

	/** True joins the enemy line (Segment.TargetOwningPlayer), false the player line (Job.UnitOwningPlayer). */
	UPROPERTY()
	bool bEnemy = false;

	/** Where it waits and where it drives to, in stage-local centimetres. */
	UPROPERTY()
	FVector StartOffset = FVector::ZeroVector;

	UPROPERTY()
	FVector DriveOffset = FVector::ZeroVector;

	/** Segment-local second of the move order. */
	UPROPERTY()
	float OrderAtSeconds = 0.f;
};

/**
 * @brief The drive one battle vehicle makes before it takes attack orders: it fires on the move with the game's own
 * auto-engage and can be shot the whole way, which is what a flank looks like to the unit being flanked.
 */
USTRUCT()
struct FRTSShortsBattleFlankRoute
{
	GENERATED_BODY()

	/** Index among the enemy line's vehicles (Segment.BattleEnemyClassPaths without its squads). */
	UPROPERTY()
	int32 EnemyIndex = 0;

	/** Stage-local centimetres, driven in order; the last one is where it stops and takes attack orders. */
	UPROPERTY()
	TArray<FVector> Waypoints;

	/** Segment-local second of the move order. */
	UPROPERTY()
	float OrderAtSeconds = 0.f;

	/** Seconds after the order when it takes attack orders even if it has not arrived. */
	UPROPERTY()
	float TimeoutSeconds = 30.f;
};

/**
 * @brief How the segment's unit fights a swarm: it keeps its front on the attacker flanking it most and shoots that one,
 * reverses a short way while reloading and stops to fire, and the attackers keep working round it. Commands only, no stats.
 */
USTRUCT()
struct FRTSShortsSoloEvasion
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = false;

	/** Seconds it stands still for a shot, once the gun is loaded. */
	UPROPERTY()
	float HoldSeconds = 1.f;

	/** It reverses again after this long even if the gun found no shot. */
	UPROPERTY()
	float MaxHoldSeconds = 2.5f;

	/** Length of one reverse leg (cm): one per reload, so it can never simply outrun the attackers. */
	UPROPERTY()
	float ReverseLegCm = 650.f;

	/** It stops giving ground this far (cm) from where it started and only turns from then on. */
	UPROPERTY()
	float RetreatRadiusCm = 3500.f;

	/** Attackers drive to a point this far (cm) off the unit's side when it has turned to face them. */
	UPROPERTY()
	float ReflankRadiusCm = 1500.f;

	/** Seconds an attacker fights from a spot before it may work round again. */
	UPROPERTY()
	float ReflankIntervalSeconds = 5.f;

	/** How quickly the camera catches up with the unit (1/s); 0 leaves the camera where it was framed. */
	UPROPERTY()
	float CameraFollowSpeed = 2.5f;
};

/**
 * @brief How enemy squads fight the segment's unit: each one works round its side to a cover tree (Job.BattleCoverTrees)
 * from which it can still shoot, its soldiers sent onto the trunk's cover points, or to an open flank spot when no tree
 * will do. A squad stays while it can engage from where it is and moves again when it cannot. Commands only, no stats.
 */
USTRUCT()
struct FRTSShortsSquadTactics
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = false;

	/** Segment-local second of the first squad's order; the rest follow OrderStaggerSeconds apart. */
	UPROPERTY()
	float OrderAtSeconds = 1.f;

	UPROPERTY()
	float OrderStaggerSeconds = 0.5f;

	/** Without a usable tree a squad goes to a spot this far (degrees) off the unit's nose, on the side it is on. */
	UPROPERTY()
	float FlankBearingDegrees = 110.f;

	/** ...and this far (cm) from the unit, or less when its weapon does not reach that far. */
	UPROPERTY()
	float FlankRadiusCm = 1800.f;

	/** Cover is only taken this far inside the squad's weapon range, so the unit backing off does not put it out of range at once. */
	UPROPERTY()
	float RangeFraction = 0.85f;

	/** A cover tree closer to the unit than this (cm) is not used: the squad would be under its tracks. */
	UPROPERTY()
	float MinimumCoverDistanceCm = 700.f;

	/** Seconds a squad fights from a spot before it may move to a better one (out of range moves it at once). */
	UPROPERTY()
	float HoldSeconds = 8.f;

	/** Seconds after a move order when the squad attacks from wherever it got to. */
	UPROPERTY()
	float MoveTimeoutSeconds = 25.f;

	/** A squad in the open that the unit has turned to within this many degrees moves to a flank again. */
	UPROPERTY()
	float FacedDegrees = 40.f;

	/** Holding squads move to another spot when nobody has damaged the unit for this long: from here they are not hurting it. */
	UPROPERTY()
	float NoEffectSeconds = 12.f;

	/** Living enemy soldiers get an outline (the game's outliner post-process), so viewers can find them in cover. */
	UPROPERTY()
	bool bOutlineSoldiers = false;

	UPROPERTY()
	FLinearColor OutlineColor = FLinearColor(1.f, 0.08f, 0.05f, 1.f);

	UPROPERTY()
	float OutlineWidth = 3.f;
};

/**
 * @brief Standoff battles: both lines spawn far apart, then every vehicle of both lines (the segment's unit too) drives
 * straight ahead toward the other line and only takes attack orders once it is there, firing on the move with the game's
 * own auto-engage. Commands only, no stats.
 */
USTRUCT()
struct FRTSShortsBattleAdvance
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = false;

	/** Segment-local second of the first move order; the vehicles of each line follow OrderStaggerSeconds apart. */
	UPROPERTY()
	float OrderAtSeconds = 1.f;

	UPROPERTY()
	float OrderStaggerSeconds = 0.15f;

	/** How far (cm) the player line drives toward the enemy line, and the enemy line toward the player line. */
	UPROPERTY()
	float PlayerAdvanceCm = 1200.f;

	UPROPERTY()
	float EnemyAdvanceCm = 2000.f;

	/** A vehicle closer than this (cm) to its drive point has arrived; one that stopped short of it counts as arrived too.
	 *  Keep it small: the attack order that follows the release cancels the rest of the drive. */
	UPROPERTY()
	float ArrivedCm = 200.f;

	/** Seconds after its order when a vehicle takes attack orders even if it has not arrived. */
	UPROPERTY()
	float TimeoutSeconds = 20.f;

	/** How quickly the opening camera follows the German line's living vehicles (1/s); 0 leaves it where it was framed. */
	UPROPERTY()
	float CameraFollowSpeed = 1.2f;

	/** Every vehicle's weapon ranges and vision radius times these for the battle (user 2026-11-18: 2 and 2, so the lines
	 *  fight from where they spawn). At the start every turret also faces its hull's front with its idle sweeps off. */
	UPROPERTY()
	float RangeScale = 1.f;

	UPROPERTY()
	float VisionScale = 1.f;
};

/**
 * @brief Large battles filmed up close: after the opening shot (the segment's Camera) the view cuts between single
 * fighters of both lines, each shot a slow pan round one unit while it fights, so the hits and the gun sounds are near.
 */
USTRUCT()
struct FRTSShortsBattleCuts
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = false;

	/** Segment-local second of the first cut; until then the segment's Camera (and its orbit) films the field. */
	UPROPERTY()
	float FirstCutSeconds = 6.f;

	/** Length of one shot; each shot varies by up to ShotJitterSeconds either way. */
	UPROPERTY()
	float ShotSeconds = 5.f;

	UPROPERTY()
	float ShotJitterSeconds = 1.f;

	/** A shot whose unit dies holds on the explosion this long and then cuts (never past its planned end). */
	UPROPERTY()
	float DeathHoldSeconds = 2.2f;

	/** Camera distance from the unit (cm) for the over-the-shoulder shot; the other shots scale from it. */
	UPROPERTY()
	float Distance = 1300.f;

	UPROPERTY()
	float Height = 380.f;

	UPROPERTY()
	float FocalLength = 18.f;

	/** Degrees the camera pans round the unit during one shot (the direction alternates). */
	UPROPERTY()
	float PanDegrees = 24.f;

	/** How quickly the camera follows a driving unit (1/s). */
	UPROPERTY()
	float FollowSpeed = 3.f;

	/** Over-the-shoulder shots look this far (0..1) along the line to the unit's target. */
	UPROPERTY()
	float TargetLookBlend = 0.4f;

	/** Units that were filmed within the last this many shots are not picked again while others are left. */
	UPROPERTY()
	int32 RepeatGap = 6;

	/** Every this-many-th shot is a wide one (twice the distance, higher, wider lens); 0 = never. */
	UPROPERTY()
	int32 WideEvery = 5;

	UPROPERTY()
	float ExposureBias = 0.25f;

	UPROPERTY()
	int32 RandomSeed = 1;
};

/** @brief A tree the director plants for squad battles, with cover points round its trunk. */
/**
 * @brief "What can pen the ...?" rounds: the segment's unit stands while the enemy line, all one attacker type, fires one
 * main-gun shell each at its front, one after another. The round ends once every attacker fired or died and the last
 * shell had time to bounce and be read; a round that got through the armour ends the job.
 */
USTRUCT()
struct FRTSShortsPenTest
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = false;

	/** Segment-local second the first attacker may fire; the others follow FireStaggerSeconds apart. */
	UPROPERTY()
	float FirstShotSeconds = 2.f;

	UPROPERTY()
	float FireStaggerSeconds = 1.2f;

	/** After the last attacker fired (or the last one died), the round holds this long: flight, bounce, reading time. */
	UPROPERTY()
	float HoldAfterLastShotSeconds = 6.f;

	/** A round with an attacker that never fires ends this many seconds after FirstShotSeconds anyway. */
	UPROPERTY()
	float RoundTimeoutSeconds = 20.f;

	/** The segment's unit never fires: its return shot's dust and the attacker it kills hid the bounce being read. */
	UPROPERTY()
	bool bTargetHoldsFire = true;

	/** Without bTargetHoldsFire: the segment's unit holds its main gun until every attacker has fired, then may answer. */
	UPROPERTY()
	bool bTargetAnswersAfterVolley = true;

	/** A shell that got through (pen, over-pen, armour destroyed) ends the job after this round. */
	UPROPERTY()
	bool bStopJobOnPenetration = true;
};

/**
 * @brief One camera shot of a wall-holes round, in the target's stage frame: origin at the target point on the ground,
 * X along the stage forward (from the attackers toward the target), Y to the stage's right, Z up. The shot starts
 * AtSeconds after its anchor and, with MoveSeconds > 0, glides from Offset/LookOffset/FocalLength to the End values.
 */
USTRUCT()
struct FRTSShortsWallHolesShot
{
	GENERATED_BODY()

	/** "Begin" (segment start), "Fire" (the fire order) or "Cease" (the cease-fire, or the target's death). */
	UPROPERTY()
	FString Anchor = TEXT("Begin");

	UPROPERTY()
	float AtSeconds = 0.f;

	UPROPERTY()
	FVector Offset = FVector(-4200.f, 900.f, 900.f);

	UPROPERTY()
	FVector LookOffset = FVector(0.f, 0.f, 250.f);

	UPROPERTY()
	float FocalLength = 18.f;

	/** 0 keeps the shot still; otherwise it glides to the End values over this many seconds (smoothstep). */
	UPROPERTY()
	float MoveSeconds = 0.f;

	UPROPERTY()
	FVector EndOffset = FVector::ZeroVector;

	UPROPERTY()
	FVector EndLookOffset = FVector::ZeroVector;

	/** <= 0 keeps FocalLength. */
	UPROPERTY()
	float EndFocalLength = 0.f;

	/** Segment.ImpactShow only: "Impact" puts the offsets' origin on the beat's first impact point (same axes) instead
	 *  of the target point, so a close shot frames the hit wherever the shell landed. "AimThenImpact" starts on the aim
	 *  point (Settings.AimOffset on the target) for a cut before the hit, and glides onto the real impact point in the
	 *  0.2 s after it lands. */
	UPROPERTY()
	FString Frame = TEXT("Target");
};

/**
 * @brief Wall-holes rounds ("what do four X do to a wall?"): the segment's unit and BattleAllyClassPaths (all one type)
 * fire their main guns at a fresh TargetClassPath building, which shows the shell holes of UWallHoleDamageComponent.
 * The volley stops after FireSeconds or once the target is down to StopAtHealthFraction (unless bAllowKill), the
 * camera cuts to the result, and the round ends ResultHoldSeconds after the cease-fire.
 */
USTRUCT()
struct FRTSShortsWallHoles
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = false;

	/** Yaw of the target relative to the stage forward (0: its local X along the stage forward). */
	UPROPERTY()
	float TargetYaw = 90.f;

	/** Move the target so its bounds centre (XY) stands on the target point; walls have their origin at a mesh end. */
	UPROPERTY()
	bool bCentreTargetOnBounds = true;

	/** Where the (first) target stands, in the target's stage frame (see FRTSShortsWallHolesShot); 0 = on the target point. */
	UPROPERTY()
	FVector PrimaryOffset = FVector::ZeroVector;

	/**
	 * More targets of the same class (e.g. a second tree), in the target's stage frame, each turned ExtraTargetYaws[i]
	 * (stage-relative, TargetYaw when missing). Health, the cease-fire and the result count all targets: the bar shows
	 * their average, each target is spared on its own, and the round counts as a kill once every target is down.
	 */
	UPROPERTY()
	TArray<FVector> ExtraTargetOffsets;

	UPROPERTY()
	TArray<float> ExtraTargetYaws;

	/** Seconds of fire from the fire order (Segment.FireAtSeconds). */
	UPROPERTY()
	float FireSeconds = 7.f;

	/**
	 * After MinFireSeconds, the volley also stops once the wall took no damage for CeaseAfterIdleSeconds (single-shot guns
	 * reload for 5-6 s: the critic saw 3.5 s of nothing after their volley). <= 0 turns this off.
	 */
	UPROPERTY()
	float CeaseAfterIdleSeconds = 1.5f;

	UPROPERTY()
	float MinFireSeconds = 3.f;

	/** The volley stops early once the target's health fraction is at or below this (ignored with bAllowKill). */
	UPROPERTY()
	float StopAtHealthFraction = 0.3f;

	/** The finale: the target may die; the volley runs until it does or FireSeconds pass. */
	UPROPERTY()
	bool bAllowKill = false;

	/** Seconds the round runs on after the cease-fire (the result shot). */
	UPROPERTY()
	float ResultHoldSeconds = 4.f;

	/** With bAllowKill: seconds after the target died before the round ends (the collapse). */
	UPROPERTY()
	float DeathHoldSeconds = 4.5f;

	/**
	 * Aim points in the target's stage frame (see FRTSShortsWallHolesShot), spread over the face the attackers see.
	 * Attacker i starts on point i and steps to the next point every RetargetSeconds, so the holes spread.
	 */
	UPROPERTY()
	TArray<FVector> AimPoints;

	/**
	 * Splits the attackers by their side of the line (stage Y) into this many groups and AimPoints into as many
	 * consecutive blocks; each attacker cycles only its group's block (two trees: the left pair keeps to the left trunk,
	 * the right pair to the right one, instead of all four firing across the line at one tree). <= 1: one group.
	 */
	UPROPERTY()
	int32 AimGroups = 1;

	/** <= 0: every attacker keeps its first aim point. */
	UPROPERTY()
	float RetargetSeconds = 1.6f;

	/** Only each attacker's largest-calibre gun fires (MG hits would use up the wall's 6 hole slots with pin holes). */
	UPROPERTY()
	bool bMainGunOnly = true;

	/** Camera shots in order; a shot replaces the one before when its time comes. */
	UPROPERTY()
	TArray<FRTSShortsWallHolesShot> Shots;
};

/**
 * @brief Impact showcase beat ("every impact of the X"): the segment's unit fires Shells main-gun shells at one target
 * picked so its weapon plays one surface's impact effect (sand ground, a soldier, a concrete wall, a vehicle it
 * penetrates, armour it bounces off). Every impact is recorded with the surface and effect the projectile resolved, so
 * the post step labels the beat from what really played. The camera is driven by Shots, poses in the target's stage
 * frame (origin on the ground at the target point, X along the stage forward, away from the shooter) anchored on
 * "Begin" (segment start), "Fire" (first shell launched) or "Impact" (first impact of the beat).
 */
USTRUCT()
struct FRTSShortsImpactShow
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = false;

	/** The effect the beat is meant to show: "Sand", "Flesh", "Stone", "Metal" or "Bounce" (checked against the impacts). */
	UPROPERTY()
	FString Beat;

	/** "Ground" (attack-ground on the target point), "Actor" (a plain actor, fired at as ground) or "Unit" (an enemy
	 *  vehicle or squad for TargetOwningPlayer, attacked as a unit; a squad's soldier nearest the point is the target). */
	UPROPERTY()
	FString TargetKind = TEXT("Ground");

	UPROPERTY()
	FString TargetClassPath;

	/** Target yaw relative to the stage forward (180: a vehicle faces the shooter). */
	UPROPERTY()
	float TargetYaw = 180.f;

	/** Actor targets: move it so its bounds centre (XY) stands on the target point (walls have their origin at an end). */
	UPROPERTY()
	bool bCentreTargetOnBounds = true;

	/** Ground and Actor targets: the aim point in the target frame. */
	UPROPERTY()
	FVector AimOffset = FVector::ZeroVector;

	/** Unit targets: fire at the AimOffset point on the unit (attack ground) instead of letting the gun pick one of the
	 *  unit's aim points; one T-70 aim point let three 75 mm shells fly past it (Pz IV G retake). */
	UPROPERTY()
	bool bFireAtAimPoint = false;

	/** Main-gun shells fired in the beat (per attempt); the gun is blocked after the last one launched. */
	UPROPERTY()
	int32 Shells = 1;

	/** Shells > 1 (an autocannon burst): the burst counts as over once every launched shell landed and the gun has not
	 *  fired for this long (the soldier it aimed at died, or the magazine ran dry). Squad targets: after
	 *  BurstRetargetSeconds without a launch the gun is ordered onto the nearest soldier still standing. */
	UPROPERTY()
	float BurstIdleSeconds = 3.0f;

	UPROPERTY()
	float BurstRetargetSeconds = 1.2f;

	/** When a shell's first impact is not the beat's surface (a glacis ricochet in a metal beat), the beat fires again,
	 *  up to this many attempts in all; the camera restarts with each attempt and the post cuts to the one that worked. */
	UPROPERTY()
	int32 MaxAttempts = 3;

	/** >= 0: the main gun stays blocked until this many seconds into the beat (the turret aims meanwhile), so the shell
	 *  leaves at a known time and the shots can be timed round it (the launch came 0.6 to 8.7 s after the fire order,
	 *  round 4). Retries fire as soon as they can. */
	UPROPERTY()
	float ReleaseGunAtSeconds = -1.f;

	/** Seconds after a failed attempt's impact before the next one is ordered. */
	UPROPERTY()
	float RetryDelaySeconds = 0.5f;

	/** Unit targets: max health scaled by this (a penetrated tank must not burn out over the impact). */
	UPROPERTY()
	float TargetHealthScale = 1.f;

	/** The target takes no damage at all (no kill, no cook-off, no collapse covering the impact). */
	UPROPERTY()
	bool bTargetInvulnerable = false;

	/**
	 * Non-empty: the landscape under the target point gets this physical material and blocks both projectile trace
	 * channels for the beat, like a game map's landscape (the Shorts desert stage's landscape let shells through).
	 */
	UPROPERTY()
	FString GroundPhysMaterialPath;

	/** The beat ends this long after the last shell's first impact. */
	UPROPERTY()
	float HoldAfterImpactSeconds = 3.f;

	/** The beat ends this long after the fire order when the shells never landed. */
	UPROPERTY()
	float TimeoutSeconds = 12.f;

	/** < 1: world time runs this fast from the beat's first impact for SlowMoSeconds of output time. Do not use in
	 *  renders: MRQ switches to relative frame numbers under time dilation and the take's frames no longer match the
	 *  manifest (frame gaps, 2026-11-21); impactshow_post.py slows the impact instead. */
	UPROPERTY()
	float ImpactTimeDilation = 1.f;

	UPROPERTY()
	float SlowMoSeconds = 0.f;

	/** Seconds over which time eases back to normal after the slow motion. */
	UPROPERTY()
	float SlowMoEaseOutSeconds = 0.4f;

	/** Camera shots in order; a shot replaces the one before when its time comes. Anchors: Begin, Fire, Impact. */
	UPROPERTY()
	TArray<FRTSShortsWallHolesShot> Shots;
};

/** One impact or bounce effect a beat's shell played (Segment.ImpactShow), as the projectile resolved it. */
USTRUCT()
struct FRTSShortsImpactRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Frame = -1;

	UPROPERTY()
	int32 ShellIndex = 0;

	/** "Sand", "Metal", "Flesh", "Air", "Stone" or "Bounce". */
	UPROPERTY()
	FString Surface;

	UPROPERTY()
	FString EffectName;

	UPROPERTY()
	FString HitActor;

	/** Armour verdict of the shell's hit on a vehicle (Bounce, Penetration, ...), empty for other targets. */
	UPROPERTY()
	FString Verdict;

	UPROPERTY()
	float ScreenX = -1.f;

	UPROPERTY()
	float ScreenY = -1.f;
};

USTRUCT()
struct FRTSShortsCoverTree
{
	GENERATED_BODY()

	UPROPERTY()
	FString ClassPath;

	/** Stage-local centimetres; the tree stands on the ground below. */
	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	UPROPERTY()
	float Scale = 1.f;

	UPROPERTY()
	float Yaw = 0.f;
};

/** @brief A static mesh the director places for one job only (set dressing, e.g. the industrial plant round a wall-holes
 * target), so a stage can be dressed per job without saving the map (which drops its baked navmesh). */
USTRUCT()
struct FRTSShortsStageProp
{
	GENERATED_BODY()

	/** Static mesh object path, e.g. /Game/RTS_Survival/Meshes/Industrial/Zone/Meshes/Warehouses/SM_Warehouse_03. */
	UPROPERTY()
	FString MeshPath;

	/** Stage-local centimetres (X stage forward, Y right); the mesh pivot is put on the ground found below, minus SinkCm. */
	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.f;

	UPROPERTY()
	FVector Scale = FVector::OneVector;

	UPROPERTY()
	float SinkCm = 5.f;

	/** Off by default: a prop with collision would stop shells that missed the target and blow up beside it. */
	UPROPERTY()
	bool bCollision = false;
};

/** @brief One wave of Segment.Survival: what spawns, when, and at which tier (the job generator rolls the units). */
USTRUCT()
struct FRTSShortsSurvivalWave
{
	GENERATED_BODY()

	/** Segment-local second the wave spawns. */
	UPROPERTY()
	float AtSeconds = 0.f;

	UPROPERTY()
	int32 Tier = 0;

	/** 1-based wave number shown on screen. */
	UPROPERTY()
	int32 Number = 1;

	/** Ground units and squads; they spawn hidden behind the ridge and come into view over its crest. */
	UPROPERTY()
	TArray<FString> ClassPaths;

	/** Aircraft flying one strafing (no bombs) or carpet-bombing (bombs) run over the defenders, then leaving. */
	UPROPERTY()
	TArray<FString> AircraftClassPaths;
};

/**
 * @brief "How long can they survive?": the segment's unit and BattleAllyClassPaths hold a hilltop while Soviet waves of
 * rising tiers come over a ridge in front of them. The segment ends when the last defender dies (or at SlotSeconds).
 */
USTRUCT()
struct FRTSShortsSurvival
{
	GENERATED_BODY()

	UPROPERTY()
	bool bEnabled = false;

	/** Stage-local spawn points (cm) hidden from the camera; each unit takes the next free one, round-robin. */
	UPROPERTY()
	TArray<FVector> SpawnOffsets;

	/** Stage-local point (cm) the waves drive to first (the ramp's foot); they are released to attack on the way. */
	UPROPERTY()
	FVector RallyOffset = FVector(4500.f, 0.f, 0.f);

	/**
	 * Stage-local X (cm) of the line the waves drive straight to (a plain move, no stopping) before they fight, so they
	 * close to where every defender reaches them instead of parking at the edge of their own range. 0: attack-move
	 * to RallyOffset and release at ReleaseRangeFraction, as before.
	 */
	UPROPERTY()
	float AdvanceOffsetX = 0.f;

	/** Each unit's advance point keeps its spawn's stage-local Y, clamped to this (cm), so the wave arrives spread out. */
	UPROPERTY()
	float AdvanceHalfWidthCm = 2500.f;

	/** A wave unit is released to attack when a defender is within this fraction of its longest weapon's range (attack-move halts it at about 1.0). */
	UPROPERTY()
	float ReleaseRangeFraction = 1.05f;

	/** Released anyway this long after it spawned. */
	UPROPERTY()
	float ReleaseAfterSeconds = 45.f;

	/** Weapon range multiplier on every unit of the short, both sides (3 = 300 %). */
	UPROPERTY()
	float RangeMultiplier = 3.f;

	/** Aircraft spawn this far out past the defenders (cm, along the stage's +X), at AircraftHeightCm over them. */
	UPROPERTY()
	float AircraftStartDistanceCm = 70000.f;

	UPROPERTY()
	float AircraftHeightCm = 4500.f;

	/** Waves in spawn order. */
	UPROPERTY()
	TArray<FRTSShortsSurvivalWave> Waves;
};

/**
 * @brief One unit of a short: which Blueprint to spawn, how long it is on stage and when it fires.
 */
USTRUCT()
struct FRTSShortsSegment
{
	GENERATED_BODY()

	/** Subtype enum name, informational (e.g. Tank_PzII_F). */
	UPROPERTY()
	FString Subtype;

	/** Blueprint class path of the unit, e.g. /Game/.../Ger_Vehicle_PanzerII_F.Ger_Vehicle_PanzerII_F_C. */
	UPROPERTY()
	FString ClassPath;

	/** "Unit" (tank or squad, spawned for the player) or "Pit" (a building expansion spawned as a level designer places it). */
	UPROPERTY()
	FString Kind = TEXT("Unit");

	UPROPERTY()
	FString Caption1;

	UPROPERTY()
	FString Caption2;

	/** Seconds this unit stays on stage. */
	UPROPERTY()
	float SlotSeconds = 7.f;

	/** Seconds after the cut when the attack-ground order is given. */
	UPROPERTY()
	float FireAtSeconds = 0.5f;

	/** Seconds after the cut when the stop order is given; negative means half a second before the slot ends. */
	UPROPERTY()
	float StopAtSeconds = -1.f;

	/** Impact point relative to the stage spawn point, in stage space (X forward). */
	UPROPERTY()
	FVector TargetOffset = FVector(5000.f, 300.f, 0.f);

	/** Yaw added to the stage forward when the unit is placed. */
	UPROPERTY()
	float UnitYawOffset = 0.f;

	/** Seconds after the cut of a second attack-ground order; negative means none. */
	UPROPERTY()
	float FireAgainAtSeconds = -1.f;

	/** Seconds after each fire order when a stop order follows (single shots); negative keeps StopAtSeconds only. */
	UPROPERTY()
	float StopAfterFireSeconds = -1.f;

	/** Squads: how many soldiers stay at the mark (0 = all); the rest are parked 30 m behind the mark, out of frame. */
	UPROPERTY()
	int32 MaxSquadVisible = 0;

	/** Team weapons: distance in cm of the crew arc behind the gun. */
	UPROPERTY()
	float CrewBackCm = 280.f;

	/** Blueprint class of a dummy enemy spawned at the target point for an attack-unit order (infantry only fire
	 *  their rifles at an actor; an attack-ground order makes them throw Panzerfausts and grenades). Empty = attack ground. */
	UPROPERTY()
	FString TargetClassPath;

	/** Fire an attack-ground order at the spawned target instead of an attack-unit order (targets that are not
	 *  units, such as a destructible apartment), aimed this high above the target's origin. */
	UPROPERTY()
	bool bAttackTargetAsGround = false;

	/** The spawned target never fires back (its turrets and soldiers' weapons stop ticking). */
	UPROPERTY()
	bool bDisarmTarget = false;

	UPROPERTY()
	float TargetAimHeight = 400.f;

	/** A destructible apartment target: its part health is scaled by this and its splash radius by
	 *  TargetSplashScale before it is set up (a short slot only has room for a few rounds). */
	UPROPERTY()
	float TargetHealthScale = 1.f;

	UPROPERTY()
	float TargetSplashScale = 1.f;

	/** Squads: issue a switch-weapons command right after placement (rifle squads otherwise fight with their Panzerfausts). */
	UPROPERTY()
	bool bSwitchWeapons = false;

	/** Rate-of-fire stacks: only the largest-calibre weapons fire (MGs and secondary guns are blocked for the segment). */
	UPROPERTY()
	bool bMainGunOnly = false;

	/** Squads with MaxSquadVisible: members whose weapon class name contains this are shown first (e.g. "Kar98_C"). */
	UPROPERTY()
	FString PreferredWeapon;

	/**
	 * Runtime effects whose asset name contains any of these (e.g. "Bounce", "MuzzleFlash", "LaunchEffect") are scaled
	 * by EffectBoostScale every time they play, pooled ones included: small ricochets and MG flashes read from far away.
	 */
	UPROPERTY()
	TArray<FString> EffectBoostNames;

	UPROPERTY()
	float EffectBoostScale = 1.f;

	/** Non-empty: the segment plays on the stage actor "Stage_<StageActorName>" instead of the job's stage. */
	UPROPERTY()
	FString StageActorName;

	/**
	 * Convoy: the segment's unit and BattleAllyClassPaths drive forward in a wedge from the segment's fire time
	 * (nobody fires). The side vehicles hold their turrets ConvoyTurretYawDegrees outward and the camera follows the
	 * lead vehicle, pulling back to ConvoyZoomOut times its framing offset by the end of the segment (SlotSeconds).
	 */
	UPROPERTY()
	bool bConvoy = false;

	UPROPERTY()
	FRTSShortsTeamWeaponSettings TeamWeapons;

	/** Convoy: sideways distance between the wedge's ranks (cm). */
	UPROPERTY()
	float ConvoySpacing = 650.f;

	/** Convoy: how far each rank of side vehicles drives behind the one before (cm). */
	UPROPERTY()
	float ConvoySideLag = 350.f;

	UPROPERTY()
	float ConvoyTurretYawDegrees = 20.f;

	/**
	 * Convoy rows instead of the wedge: vehicles per row, front to back (e.g. [3, 4]: the segment's unit and two
	 * allies in front, four allies behind). The front row fans out from the segment's unit (centre, right, left, ...),
	 * the rows behind are centred; rows are ConvoyRowGap apart and ConvoySpacing sideways. Empty keeps the wedge.
	 */
	UPROPERTY()
	TArray<int32> ConvoyRows;

	UPROPERTY()
	float ConvoyRowGap = 1100.f;

	/** Convoy: each vehicle's move order goes this far ahead of it along the stage's forward axis (cm). */
	UPROPERTY()
	float ConvoyDriveDistance = 20000.f;

	UPROPERTY()
	float ConvoyZoomOut = 1.35f;

	/**
	 * Convoy: crushable props laid in every vehicle's lane before it drives, this far ahead of its hull front (cm).
	 * Empty lays none. Each vehicle gets props it can crush, picked by its nav agent weight from the lists below.
	 */
	UPROPERTY()
	TArray<float> CrushableDistances;

	/** Convoy: props for heavy tanks (crush rule HeavyTank or lighter). */
	UPROPERTY()
	TArray<FString> CrushableHeavyClassPaths;

	/** Convoy: props for medium tanks (crush rule MediumOrHeavyTank or AnyTank). */
	UPROPERTY()
	TArray<FString> CrushableMediumClassPaths;

	/** Convoy: props for light tanks and other vehicles (crush rule AnyTank). */
	UPROPERTY()
	TArray<FString> CrushableLightClassPaths;

	/** Convoy: the props stand up to this far left or right of the lane's centre line (cm). */
	UPROPERTY()
	float CrushableLaneJitter = 60.f;

	/**
	 * Battle: further player units spawned in line abreast beside the segment's unit (BattleSpacing apart). Squads
	 * may be mixed in here and in BattleEnemyClassPaths: the squad takes the orders, its soldiers are the targets.
	 */
	UPROPERTY()
	TArray<FString> BattleAllyClassPaths;

	/**
	 * Battle: enemy units (TargetOwningPlayer) spawned in a facing line TargetOffset.X ahead. A segment with enemies
	 * plays until one side is destroyed (or its SlotSeconds run out) and then ends BattleEndHoldSeconds later.
	 */
	UPROPERTY()
	TArray<FString> BattleEnemyClassPaths;

	/** Optional stage-axis offsets from the player line origin, indexed like BattleAllyClassPaths (main unit excluded). */
	UPROPERTY()
	TArray<FVector> BattleAllyOffsets;

	/** Optional stage-axis offsets from the enemy line origin, indexed like BattleEnemyClassPaths. */
	UPROPERTY()
	TArray<FVector> BattleEnemyOffsets;

	/**
	 * Battle: behaviour Blueprints (/Game/..._C) added to every enemy vehicle as it spawns, for example
	 * BP_Beh_Permanent_SetHEATShells so a weak gun in the enemy line fires HEAT. Squads are skipped.
	 */
	UPROPERTY()
	TArray<FString> BattleEnemyBehaviourClassPaths;

	UPROPERTY()
	float BattleSpacing = 700.f;

	UPROPERTY()
	float BattleEndHoldSeconds = 3.f;

	/** Battle: every unit's max health (both lines) is scaled by this, so heavily armoured lines still reach a winner. */
	UPROPERTY()
	float BattleHealthScale = 1.f;

	/** Battle: the player line's max health is scaled by this on top of BattleHealthScale, so a showcased unit
	 *  survives the match-up that shows its strength (a variant short's one unit against a group). */
	UPROPERTY()
	float BattlePlayerHealthScale = 1.f;

	/** Opt-in real paratrooper ability; GA_None preserves the ordinary battle/showcase camera. */
	UPROPERTY()
	EGlobalAbility ParatrooperAbility = EGlobalAbility::GA_None;

	/** Aircraft is framed for exactly this many seconds before cutting to the drop zone. */
	UPROPERTY()
	float ParatrooperIntroSeconds = 2.f;

	/** Approach in stage-local cm, relative to the midpoint between the two battle lines. */
	UPROPERTY()
	FVector ParatrooperAircraftStartOffset = FVector(-6000.f, 0.f, 3500.f);

	UPROPERTY()
	FRTSShortsParatrooperFormation ParatrooperFormation;

	UPROPERTY()
	FRTSShortsParatrooperCameraMove ParatrooperCameraMove;

	/** Squads: the soldiers' cooldown between shots in seconds (a bolt rifle otherwise fires once per 3.7 s slot); negative keeps the weapon's own. */
	UPROPERTY()
	float InfantryFireIntervalSeconds = -1.f;

	/** Owning player of the dummy target (0 = the AI enemy). */
	UPROPERTY()
	int32 TargetOwningPlayer = 0;

	/** Seconds a runtime-spawned particle effect (muzzle dust, smoke, debris) may live before it is cut; negative
	 *  keeps the game's own lifetimes. Short slots: keeps the hull readable after the shot. */
	UPROPERTY()
	float EffectMaxSeconds = -1.f;

	/** World-spawned effects (dust, smoke; not those attached to the unit, like its muzzle flash) are scaled by
	 *  this when they appear (1 = untouched). */
	UPROPERTY()
	float EffectScale = 1.f;

	/** Effects attached to the unit itself (muzzle flash and smoke) are scaled by this when they appear. */
	UPROPERTY()
	float EffectUnitScale = 1.f;

	/** > 0: the unit's own effects are split, their smoke emitters play from a copy scaled by this (relative to
	 *  the original size) while EffectUnitScale sizes the flash. */
	UPROPERTY()
	float EffectUnitSmokeScale = 0.f;

	/** The unit's own effects whose asset name contains one of these keep their original size (no EffectUnitScale,
	 *  no smoke split), e.g. machine-gun flashes next to a scaled-up cannon blast. */
	UPROPERTY()
	TArray<FString> EffectUnitExemptNames;

	/** Smoke copy: seconds it keeps spawning, its fade time (-1 = the effect values) and how far (cm) in front of
	 *  the muzzle socket it plays. */
	UPROPERTY()
	float EffectSmokeMaxSeconds = -1.f;

	UPROPERTY()
	float EffectSmokeFadeSeconds = -1.f;

	UPROPERTY()
	float EffectSmokeForwardOffset = 0.f;

	/** Number of stacked smoke copies (1-4): more copies, denser smoke at the same size. */
	UPROPERTY()
	int32 EffectSmokeCopies = 1;

	/** Extra smoke copies (after the first) are scaled by this relative to EffectUnitSmokeScale. */
	UPROPERTY()
	float EffectSmokeExtraCopyScale = 1.f;

	/** Smoke copies are pushed this far (cm, horizontally) away from the camera, behind the barrel's plane. */
	UPROPERTY()
	float EffectSmokeAwayOffset = 0.f;

	/** After EffectMaxSeconds the effect stops spawning; its live particles keep fading naturally and whatever is
	 *  left is removed this many seconds later. */
	UPROPERTY()
	float EffectFadeSeconds = 2.0f;

	/** After EffectMaxSeconds, Niagara effects run this many times faster so their particles fade out on their own
	 *  curves (1 = normal speed). */
	UPROPERTY()
	float EffectFadeSpeed = 1.f;

	/** Niagara emitters whose name contains any of these are switched off when their effect appears. */
	UPROPERTY()
	TArray<FString> EffectDisableEmitters;

	/** Runtime effects whose asset name contains any of these are removed the moment they appear. */
	UPROPERTY()
	TArray<FString> EffectKillNames;

	/** Effects that first appear this many seconds into the slot or later (a slow shell landing in the impact shot)
	 *  keep the game's own look: no cap, scale, emitter switch-off or kill. Negative = every effect is limited. */
	UPROPERTY()
	float EffectLimitUntilSeconds = -1.f;

	/** Keep the staged unit's engine exhaust emitters switched off. */
	UPROPERTY()
	bool bSuppressExhaust = false;

	/** Turret rotation speed (deg/s) forced on the unit's turrets at placement; 0 keeps the unit's own. */
	UPROPERTY()
	float TurretRotationSpeed = 0.f;

	/** Scale on the hull's fire feedback (recoil kick) of the staged unit; 1 keeps the unit's own (user 2026-11-20:
	 *  caliber stacks at 0.5). */
	UPROPERTY()
	float HullRecoilScale = 1.f;

	UPROPERTY()
	FRTSShortsCameraParams Camera;

	/** Seconds after the cut when the view switches to the impact camera; negative keeps the main camera. */
	UPROPERTY()
	float ImpactCutAtSeconds = -1.f;

	UPROPERTY()
	FRTSShortsImpactCameraParams ImpactCamera;

	/** Battle: > 0 orders a unit to attack only enemies within this distance (cm) and puts the battle squads on hold
	 *  position, so they keep their cover (a trench) and fire at what is in range instead of walking at far targets. */
	UPROPERTY()
	float BattleOrderMaxDistance = 0.f;

	/** Battle: units that drive in from far off during the battle (they take no attack orders until they arrive). */
	UPROPERTY()
	TArray<FRTSShortsBattleReinforcement> BattleReinforcements;

	/** Battle: enemy vehicles that work round the segment's unit instead of attacking from where they spawned. */
	UPROPERTY()
	TArray<FRTSShortsBattleFlankRoute> BattleFlankRoutes;

	/** Battle: the segment's unit fights the enemy line by itself instead of taking the director's attack orders. */
	UPROPERTY()
	FRTSShortsSoloEvasion BattleSoloEvasion;

	/** Battle: both lines spawn far apart and drive toward each other before they take attack orders. */
	UPROPERTY()
	FRTSShortsBattleAdvance BattleAdvance;

	/** Battle: the enemy squads flank and take cover behind Job.BattleCoverTrees instead of walking straight in. */
	UPROPERTY()
	FRTSShortsSquadTactics BattleSquadTactics;

	/** Battle: after the opening shot the camera cuts between close shots of single fighters. */
	UPROPERTY()
	FRTSShortsBattleCuts BattleCuts;

	/** Battle: one shell per attacker at the segment unit's front, recorded shot by shot ("What can pen the ...?"). */
	UPROPERTY()
	FRTSShortsPenTest PenTest;

	/** Four attackers of one type shoot holes into a fresh building, then the camera shows the result. */
	UPROPERTY()
	FRTSShortsWallHoles WallHoles;

	/** "How long can they survive?": defenders on a hilltop against tiered Soviet waves. */
	UPROPERTY()
	FRTSShortsSurvival Survival;

	/** One beat of an impact showcase: the unit's main gun on a target that shows one impact effect. */
	UPROPERTY()
	FRTSShortsImpactShow ImpactShow;

	/** Aircraft passes flown during this segment. */
	UPROPERTY()
	TArray<FRTSShortsFlyBy> FlyBys;

	/** Kind "AircraftCrash": the flight, kill and cameras of the shot-down aircraft. */
	UPROPERTY()
	FRTSShortsAircraftCrash AircraftCrash;

	/** Ammo (turret toss) or fuel cook-off death of the segment's tank; off unless CookOff.StartAtSeconds >= 0. */
	UPROPERTY()
	FRTSShortsCookOff CookOff;
};

/**
 * @brief A whole short: output location, render settings and the ordered segments.
 */
USTRUCT()
struct FRTSShortsJob
{
	GENERATED_BODY()

	UPROPERTY()
	FString JobName;

	/** Absolute directory the JPG sequence, WAV and Manifest.json are written to. */
	UPROPERTY()
	FString OutputDir;

	/** Package path of the shorts map. */
	UPROPERTY()
	FString MapPath = TEXT("/Game/RTS_Survival/Maps/Shorts/RTS_Shorts");

	/** Class path of the game mode the Movie Render Queue game override uses. */
	UPROPERTY()
	FString GameModeClassPath = TEXT("/Game/RTS_Survival/Shorts/BP_ShortsGameMode.BP_ShortsGameMode_C");

	/** Infantry skeletal meshes do not receive projected decals (ground ruts, craters, dirt). */
	UPROPERTY()
	bool bSuppressUnitDecals = true;

	/** Owning player the staged units are spawned for (team paint, 1 = the shorts player). */
	UPROPERTY()
	int32 UnitOwningPlayer = 1;

	/** Where the waiting units are parked, in stage-local cm (X along the firing direction, Y to the right). */
	UPROPERTY()
	FVector PenLocalOffset = FVector(15000.f, 8000.f, 0.f);

	/** StageName variable value of the BP_ShortsStage instance to use. */
	UPROPERTY()
	FString StageName = TEXT("Vehicle");

	/**
	 * > 0: the floating combat texts ("engine damaged", "shell shattered") are turned into world-space widgets facing the
	 * camera at this scale, so the 3D pass films them (screen-space widgets never reach Movie Render Queue frames).
	 */
	UPROPERTY()
	float CombatTextWorldScale = 0.f;

	/** Render the game's screen-space widgets (floating combat texts) into the frames via the MRQ UI renderer. */
	UPROPERTY()
	bool bRenderGameWidgets = false;

	/** > 0: track trails only cover this far (cm) round the stage, which keeps them sharp on a map of far-apart landscapes. */
	UPROPERTY()
	float TrackTrailAreaHalfSizeCm = 0.f;

	/** A battle the segment's unit lost is the last one rendered: the rounds of an escalating job stop there. */
	UPROPERTY()
	bool bStopWhenUnitDestroyed = false;

	/** Planted when the first battle with Segment.BattleSquadTactics begins and kept for the rest of the job. */
	UPROPERTY()
	TArray<FRTSShortsCoverTree> BattleCoverTrees;

	/** Static meshes placed round the job's stage when its first segment begins and kept for the whole job. */
	UPROPERTY()
	TArray<FRTSShortsStageProp> StageProps;

	/** Actors whose name or label starts with any of these are hidden for this job (e.g. a backdrop hill over a second mark). */
	UPROPERTY()
	TArray<FString> HiddenActorPrefixes;

	/**
	 * Keeps the crews' spoken lines in the capture. The shorts mute RTS_SClass_VoiceLine by default, which silences most
	 * of them; the medium tanks' fire lines have no sound class and always played.
	 */
	UPROPERTY()
	bool bKeepVoiceLines = false;

	/** Stage actor label to place units on (default "Stage_" + StageName): a second mark inside an existing stage's dressing. */
	UPROPERTY()
	FString StageActorName;

	UPROPERTY()
	int32 Fps = 60;

	UPROPERTY()
	int32 ResolutionX = 1080;

	UPROPERTY()
	int32 ResolutionY = 1920;

	/** Seconds of empty stage at the start, dropped in post; the director prepares during it. */
	UPROPERTY()
	float WarmupSeconds = 6.f;

	UPROPERTY()
	int32 EngineWarmUpFrames = 30;

	UPROPERTY()
	int32 RenderWarmUpFrames = 16;

	UPROPERTY()
	int32 TemporalSamples = 1;

	UPROPERTY()
	int32 SpatialSamples = 1;

	/** False runs the director on world time without Movie Render Queue (PIE preview or the audio pass). */
	UPROPERTY()
	bool bRender = true;

	/**
	 * True records the master submix in real time while the director runs on world time and writes
	 * <OutputDir>/<JobName>_realtime.wav; used because the offline audio renderer drops longer sounds.
	 */
	UPROPERTY()
	bool bRecordAudio = false;

	/**
	 * Size and rate-of-fire stacks: every sound plays at full volume whatever its distance to the camera. Their cameras
	 * stand up to ~80 m back and the Flakvierling's attenuation silenced it there (RateOfFire01 take 1, 2026-11-11).
	 */
	UPROPERTY()
	bool bNoDistanceAttenuation = false;

	/** Diagnostic: play known cues through the different playback paths at fixed frames of segment 0 (see director). */
	UPROPERTY()
	bool bAudioProbe = false;

	UPROPERTY()
	TArray<FRTSShortsSegment> Segments;
};

/**
 * @brief One moment of a battle for the post step, which cuts a long fight down to what mattered.
 */
USTRUCT()
struct FRTSShortsBattleEvent
{
	GENERATED_BODY()

	/** Output frame of the single rendered shot. */
	UPROPERTY()
	int32 Frame = 0;

	/** "Hit", "Kill" or "FlankArrived". */
	UPROPERTY()
	FString Kind;

	UPROPERTY()
	bool bPlayerSide = false;

	/** Health left on the unit, 0..1. */
	UPROPERTY()
	float HealthFraction = 1.f;

	UPROPERTY()
	int32 PlayersAlive = 0;

	UPROPERTY()
	int32 EnemiesAlive = 0;
};

/** Segment.PenTest: one attacker's main-gun shell, from launch to what it did on the target's armour. */
USTRUCT()
struct FRTSShortsPenShot
{
	GENERATED_BODY()

	/** Index into the segment's BattleEnemyClassPaths. */
	UPROPERTY()
	int32 AttackerIndex = 0;

	UPROPERTY()
	int32 LaunchFrame = -1;

	/** -1 when the shell hit nothing on the target (a miss, or it hit something else first). */
	UPROPERTY()
	int32 HitFrame = -1;

	/** "Bounce", "Penetration", "OverPenetration", "ArmorDestroyed", "FlankAmmoDetonation" or "Miss". */
	UPROPERTY()
	FString Verdict;

	UPROPERTY()
	FString Plate;

	UPROPERTY()
	int32 CalibreMm = 0;

	UPROPERTY()
	FString ShellType;

	UPROPERTY()
	float RawArmorMm = 0.f;

	UPROPERTY()
	float EffectiveArmorMm = 0.f;

	UPROPERTY()
	float ImpactAngleDegrees = 0.f;

	/** Penetration at the distance flown, and after the impact angle reduced it (what was compared to the armour). */
	UPROPERTY()
	float PenAtRangeMm = 0.f;

	UPROPERTY()
	float PenAfterAngleMm = 0.f;

	UPROPERTY()
	float DistanceM = 0.f;

	UPROPERTY()
	float HealthBefore = 0.f;

	UPROPERTY()
	float HealthAfter = 0.f;

	/** Where the hit was on the rendered frame, 0..1 from the top left; negative when off screen. */
	UPROPERTY()
	float ScreenX = -1.f;

	UPROPERTY()
	float ScreenY = -1.f;
};

/**
 * @brief What the director recorded about one segment, in output frames of the single rendered shot.
 */
USTRUCT()
struct FRTSShortsManifestSegment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Index = 0;

	UPROPERTY()
	FString Subtype;

	UPROPERTY()
	FString Caption1;

	UPROPERTY()
	FString Caption2;

	UPROPERTY()
	int32 CaliberMm = 0;

	UPROPERTY()
	int32 StartFrame = 0;

	UPROPERTY()
	int32 EndFrame = 0;

	UPROPERTY()
	int32 FireOrderFrame = -1;

	UPROPERTY()
	int32 StopOrderFrame = -1;

	UPROPERTY()
	bool bSkipped = false;

	UPROPERTY()
	FString SkipReason;

	/** Battles: output frame one side was gone; -1 when the slot ran out first. */
	UPROPERTY()
	int32 BattleDecidedFrame = -1;

	/** Battles: "player", "enemy", or empty when undecided. */
	UPROPERTY()
	FString BattleWinner;

	UPROPERTY()
	int32 BattleSurvivors = 0;

	UPROPERTY()
	TArray<FRTSShortsBattleEvent> BattleEvents;

	/** Segment.BattleCuts: output frames of the camera cuts inside the segment (post must not treat them as glitches). */
	UPROPERTY()
	TArray<int32> CameraCutFrames;

	/** Segment.PenTest: every attacker's shell in launch order, the attackers' main-gun calibre and the round's outcome. */
	UPROPERTY()
	TArray<FRTSShortsPenShot> PenShots;

	UPROPERTY()
	int32 PenAttackerCalibreMm = 0;

	/** Output frame the last attacker fired or died; -1 while the volley is not complete. */
	UPROPERTY()
	int32 PenVolleyDoneFrame = -1;

	UPROPERTY()
	bool bPenetrated = false;

	/** Attackers destroyed before they fired. */
	UPROPERTY()
	int32 PenAttackersLostUnfired = 0;

	/** Segment.WallHoles: output frames of the fire order, the cease-fire and the target's death (-1: none). */
	UPROPERTY()
	int32 WallFireFrame = -1;

	UPROPERTY()
	int32 WallCeaseFrame = -1;

	UPROPERTY()
	int32 WallDeathFrame = -1;

	/** "time", "health", "killed" or empty while firing. */
	UPROPERTY()
	FString WallCeaseReason;

	UPROPERTY()
	int32 WallAttackers = 0;

	/** Main-gun shells the attackers launched during the volley. */
	UPROPERTY()
	int32 WallShells = 0;

	UPROPERTY()
	int32 WallAttackerCalibreMm = 0;

	/** Target health fraction (0..1) every WallHealthSampleFrames output frames from WallFireFrame on. */
	UPROPERTY()
	TArray<float> WallHealthSamples;

	UPROPERTY()
	int32 WallHealthSampleFrames = 6;

	UPROPERTY()
	float WallHealthStart = 0.f;

	UPROPERTY()
	float WallHealthEnd = 0.f;

	/** Holes and knocked-out grid cells on the target at the cease-fire. */
	UPROPERTY()
	int32 WallHoleCount = 0;

	UPROPERTY()
	int32 WallHiddenCells = 0;

	/** Camera shots applied in this segment (output frame each started). */
	UPROPERTY()
	TArray<int32> WallShotFrames;

	/** Segment.Survival: output frame, tier and number of every wave that spawned. */
	UPROPERTY()
	TArray<int32> SurvivalWaveFrames;

	UPROPERTY()
	TArray<int32> SurvivalWaveTiers;

	UPROPERTY()
	TArray<int32> SurvivalWaveNumbers;

	/** Output frame each wave's first unit came into the camera's view (-1: never seen). */
	UPROPERTY()
	TArray<int32> SurvivalWaveSeenFrames;

	/** Output frames a shell was launched (either side, one entry per frame): the cut keeps the shooting at normal speed. */
	UPROPERTY()
	TArray<int32> SurvivalShotFrames;

	/** Output frame each defender died, in death order. */
	UPROPERTY()
	TArray<int32> SurvivalDefenderDeathFrames;

	UPROPERTY()
	int32 SurvivalDefenders = 0;

	/** Output frame the last defender died (-1: they lasted to the end of the segment). */
	UPROPERTY()
	int32 SurvivalEndFrame = -1;

	UPROPERTY()
	int32 SurvivalEnemiesSpawned = 0;

	UPROPERTY()
	int32 SurvivalEnemiesKilled = 0;

	/** Segment.ImpactShow: the beat, the main gun, the fire order, first impact and every impact effect played. */
	UPROPERTY()
	FString ImpactBeat;

	UPROPERTY()
	FString ImpactWeaponName;

	UPROPERTY()
	int32 ImpactShellsFired = 0;

	UPROPERTY()
	int32 ImpactLaunchFrame = -1;

	UPROPERTY()
	int32 ImpactFirstFrame = -1;

	UPROPERTY()
	int32 ImpactEndFrame = -1;

	/** The expected effect played (an impact on the beat's surface, or a bounce for "Bounce"). */
	UPROPERTY()
	bool bImpactBeatShown = false;

	/** Index of the shell whose first impact showed the beat's surface (-1: none); launch and first-impact frames above
	 *  belong to the latest attempt, which is this shell's when the beat was shown. */
	UPROPERTY()
	int32 ImpactShownShell = -1;

	UPROPERTY()
	int32 ImpactAttempts = 0;

	/** Distance (cm) from the aim point to the shown impact; a sand shell that overflew its aim landed 34 m off (round 2). */
	UPROPERTY()
	float ImpactMissCm = -1.f;

	UPROPERTY()
	TArray<FRTSShortsImpactRecord> Impacts;

	/** Weapon's own effect for the beat's surface (or its bounce effect); empty when it has none. */
	UPROPERTY()
	FString ImpactExpectedEffect;
};

/**
 * @brief Manifest written next to the render for the post step.
 */
USTRUCT()
struct FRTSShortsManifest
{
	GENERATED_BODY()

	UPROPERTY()
	FString JobName;

	UPROPERTY()
	int32 Fps = 60;

	UPROPERTY()
	int32 WarmupFrames = 0;

	UPROPERTY()
	int32 TotalFrames = 0;

	UPROPERTY()
	FString SequenceName;

	UPROPERTY()
	bool bComplete = false;

	UPROPERTY()
	TArray<FRTSShortsManifestSegment> Segments;
};
