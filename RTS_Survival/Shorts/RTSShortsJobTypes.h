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

/** @brief A tree the director plants for squad battles, with cover points round its trunk. */
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

	/** Battle: the enemy squads flank and take cover behind Job.BattleCoverTrees instead of walking straight in. */
	UPROPERTY()
	FRTSShortsSquadTactics BattleSquadTactics;

	/** Aircraft passes flown during this segment. */
	UPROPERTY()
	TArray<FRTSShortsFlyBy> FlyBys;

	/** Kind "AircraftCrash": the flight, kill and cameras of the shot-down aircraft. */
	UPROPERTY()
	FRTSShortsAircraftCrash AircraftCrash;

	/** Ammo cook-off death of the segment's tank (turret toss); off unless CookOff.StartAtSeconds >= 0. */
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
