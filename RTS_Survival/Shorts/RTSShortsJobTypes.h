// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

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

	UPROPERTY()
	float BattleSpacing = 700.f;

	UPROPERTY()
	float BattleEndHoldSeconds = 3.f;

	/** Battle: every unit's max health (both lines) is scaled by this, so heavily armoured lines still reach a winner. */
	UPROPERTY()
	float BattleHealthScale = 1.f;

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

	/** Actors whose name or label starts with any of these are hidden for this job (e.g. a backdrop hill over a second mark). */
	UPROPERTY()
	TArray<FString> HiddenActorPrefixes;

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
