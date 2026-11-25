// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "GMRJobTypes.generated.h"

/**
 * @brief One camera key of a shot. Positions are given in the job's anchor frame (X forward along AnchorYaw,
 * Y right, Z up) relative to a pivot; the camera sits on a sphere around that pivot.
 * Values of 0 in the V* fields mean "derive the 9:16 value from the 16:9 one" (see FGMRJob vertical defaults).
 */
USTRUCT()
struct FGMRCamKey
{
	GENERATED_BODY()

	// Seconds from the shot start.
	UPROPERTY()
	float Time = 0.f;

	// Pivot source: empty = anchor, "tag:<Tag>" = centroid of live recorded units with that tag,
	// "actor:<LabelPrefix>" = first level actor whose name or label starts with the prefix.
	UPROPERTY()
	FString Pivot;

	// Added to the pivot, in the anchor frame (cm).
	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	// Compass direction from the pivot to the camera, degrees in the anchor frame (0 = in front along +X).
	UPROPERTY()
	float Yaw = 180.f;

	// Elevation of the camera above the pivot, degrees (positive looks down).
	UPROPERTY()
	float Pitch = 25.f;

	UPROPERTY()
	float Distance = 3000.f;

	UPROPERTY()
	float Focal = 35.f;

	UPROPERTY()
	float Roll = 0.f;

	// Look point = pivot + Offset + LookOffset (anchor frame); the camera is placed around pivot + Offset.
	UPROPERTY()
	FVector LookOffset = FVector(0.f, 0.f, 150.f);

	UPROPERTY()
	float VYaw = 0.f;

	UPROPERTY()
	float VPitch = 0.f;

	UPROPERTY()
	float VDistance = 0.f;

	UPROPERTY()
	float VFocal = 0.f;

	// 9:16 replacement for LookOffset (zero = same as 16:9); a tall frame often wants the look point higher.
	UPROPERTY()
	FVector VLookOffset = FVector::ZeroVector;
};

/**
 * @brief A continuous camera move between two cuts. Keys are splined (Catmull-Rom on every channel) over an eased
 * shot clock, so a move always starts and ends at rest unless Ease is Linear.
 */
USTRUCT()
struct FGMRShot
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	// Timeline seconds (0 = first recorded frame).
	UPROPERTY()
	float Start = 0.f;

	UPROPERTY()
	float End = 5.f;

	// "Orbit": camera on the sphere around the splined pivot. "Fixed": camera frozen where key 0 puts it at the
	// shot start, only the look point moves (a pan/tilt). "Follow": like Orbit but Yaw is relative to the heading
	// of the first key's tagged units.
	UPROPERTY()
	FString Rig = TEXT("Orbit");

	UPROPERTY()
	TArray<FGMRCamKey> Keys;

	// Fixed rig only: where the camera stands, independent of what it looks at. Used when CameraPivot is set or
	// CameraOffset is not zero: location = CameraPivot (empty = anchor) + CameraOffset (anchor frame); Z of the
	// offset is the height above the ground there.
	UPROPERTY()
	FString CameraPivot;

	UPROPERTY()
	FVector CameraOffset = FVector::ZeroVector;

	// 9:16 replacement for CameraOffset (zero = same as 16:9).
	UPROPERTY()
	FVector VCameraOffset = FVector::ZeroVector;

	// "InOut", "In", "Out" or "Linear".
	UPROPERTY()
	FString Ease = TEXT("InOut");

	// Half-life (s) of the spring that smooths tracked pivots; units dying or turning must not jerk the frame.
	UPROPERTY()
	float PivotLag = 0.6f;

	// F-stop for depth of field; 0 = everything sharp.
	UPROPERTY()
	float Aperture = 0.f;

	// Handheld drift amplitude in degrees (0 = locked off).
	UPROPERTY()
	float Shake = 0.f;

	UPROPERTY()
	bool bAvoidOcclusion = true;

	UPROPERTY()
	float MinGroundClearance = 300.f;
};

/**
 * @brief A scripted gameplay action at a timeline second. One struct carries every action type's parameters so the
 * job JSON stays flat; unused fields are ignored. Times below 0 run during setup, before the first frame.
 */
USTRUCT()
struct FGMREvent
{
	GENERATED_BODY()

	UPROPERTY()
	float Time = 0.f;

	// Spawn, Move, AttackMove, Attack, AttackGround, Deploy, Expand, Ability, Kill, Destroy, Hide, Resources,
	// VoiceLine, Stop, Formation... (see GMRDirectorEvents.cpp).
	UPROPERTY()
	FString Type;

	// Units created by Spawn get this tag; other events act on all live units with it.
	UPROPERTY()
	FString Tag;

	UPROPERTY()
	FString ClassPath;

	UPROPERTY()
	int32 Player = 1;

	UPROPERTY()
	int32 Count = 1;

	// Spawn grid: Columns per row, Spacing between units (cm), rows go back along -X of the event yaw.
	UPROPERTY()
	int32 Columns = 0;

	UPROPERTY()
	float Spacing = 1200.f;

	// Anchor frame location (cm) and yaw (degrees).
	UPROPERTY()
	FVector Offset = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.f;

	UPROPERTY()
	FString TargetTag;

	UPROPERTY()
	FVector TargetOffset = FVector::ZeroVector;

	UPROPERTY()
	float TargetYaw = 0.f;

	UPROPERTY()
	float Radius = 0.f;

	// Generic numeric / text parameters (health fraction, ability enum name, expansion slot, sound path...).
	UPROPERTY()
	float Value = 0.f;

	UPROPERTY()
	FString Text;

	// ERTSVoiceLine name played (forced) on the first unit when an event whose Text holds something else, such as
	// Grenade, carries out its order; the command line a player would hear.
	UPROPERTY()
	FString Voice;

	// Index-based selection inside the tag (-1 = all units of the tag).
	UPROPERTY()
	int32 Index = -1;

	// Named overrides for events with many knobs (supply point timings, strafe overrides...).
	UPROPERTY()
	TMap<FString, float> Settings;

	float GetSetting(const TCHAR* Name, const float Default) const
	{
		const float* Found = Settings.Find(Name);
		return Found != nullptr ? *Found : Default;
	}
};

/**
 * @brief The whole recording: which map, where (anchor), what happens (events) and how it is filmed (shots).
 * Written by Tools/GameMechanicsRecs/gmr.py per take; the launcher fills OutputDir, resolution and Aspect.
 */
USTRUCT()
struct FGMRJob
{
	GENERATED_BODY()

	UPROPERTY()
	FString JobName;

	UPROPERTY()
	FString MapPath;

	// Empty = the map's own game mode.
	UPROPERTY()
	FString GameModeClassPath;

	UPROPERTY()
	FString OutputDir;

	// "16x9" or "9x16".
	UPROPERTY()
	FString Aspect = TEXT("16x9");

	UPROPERTY()
	int32 ResolutionX = 1920;

	UPROPERTY()
	int32 ResolutionY = 1080;

	UPROPERTY()
	int32 Fps = 30;

	// Rendered but cut away in post: the director sets up during these seconds.
	UPROPERTY()
	float WarmupSeconds = 6.f;

	UPROPERTY()
	float DurationSeconds = 20.f;

	UPROPERTY()
	int32 EngineWarmUpFrames = 32;

	UPROPERTY()
	int32 SpatialSamples = 1;

	UPROPERTY()
	int32 TemporalSamples = 1;

	// World anchor of the scene; empty AnchorActor uses Anchor as given, "PlayerStart" uses the first player start.
	UPROPERTY()
	FVector Anchor = FVector::ZeroVector;

	UPROPERTY()
	float AnchorYaw = 0.f;

	UPROPERTY()
	FString AnchorActor;

	// Level actors whose class name starts with one of these are destroyed at setup (mission managers, spawners).
	UPROPERTY()
	TArray<FString> DestroyClassPrefixes;

	// Level actors whose name or label starts with one of these are hidden and lose collision at setup.
	UPROPERTY()
	TArray<FString> HideActorPrefixes;

	// Units and squads already on the map within this radius of the anchor are removed (0 = none).
	UPROPERTY()
	float ClearUnitsRadius = 0.f;

	// Remove every pre-placed unit of these players (e.g. [2] removes the map's enemy garrison).
	UPROPERTY()
	TArray<int32> ClearPlayers;

	UPROPERTY()
	bool bDisableFogOfWar = true;

	// Actors of these class-name prefixes that appear during the take are removed (survival drop ships).
	UPROPERTY()
	TArray<FString> SuppressClassPrefixes;

	UPROPERTY()
	bool bKeepVoiceLines = true;

	UPROPERTY()
	bool bKeepAnnouncer = false;

	UPROPERTY()
	bool bKeepMusic = false;

	// Every spatialized sound renders silent in -deterministicaudio renders (see headless-render-audio-mute);
	// they are forced 2D. With this flag distance attenuation is dropped too.
	UPROPERTY()
	bool bNoDistanceAttenuation = false;

	UPROPERTY()
	float VerticalFocalScale = 0.85f;

	UPROPERTY()
	float VerticalDistanceScale = 1.f;

	UPROPERTY()
	float VerticalPitchAdd = 4.f;

	// Post-process motion blur on the cine camera; negative leaves the map's setting.
	UPROPERTY()
	float MotionBlurAmount = 0.35f;

	// Campaign maps have no sky (the RTS camera never sees the horizon); cinematic angles would show black.
	// A render-only sky sphere with this material is put around the anchor (empty = none).
	UPROPERTY()
	FString SkyMaterial;

	UPROPERTY()
	FString SkyMesh = TEXT("/Game/SkyboxPack_X/Meshes/SM_Skybox_Mesh.SM_Skybox_Mesh");

	UPROPERTY()
	float SkyRadius = 250000.f;

	UPROPERTY()
	float SkyYaw = 0.f;

	// Lowers the sky sphere so its horizon band sits below distant ridges (cm).
	UPROPERTY()
	float SkyDrop = 0.f;

	UPROPERTY()
	TArray<FGMREvent> Events;

	UPROPERTY()
	TArray<FGMRShot> Shots;
};

USTRUCT()
struct FGMRManifestShot
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Rig;

	UPROPERTY()
	int32 StartFrame = 0;

	UPROPERTY()
	int32 EndFrame = 0;

	UPROPERTY()
	float OcclusionPitchBoost = 0.f;

	UPROPERTY()
	FVector FirstCameraLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector FirstLookAt = FVector::ZeroVector;

	UPROPERTY()
	float FirstFocal = 0.f;
};

USTRUCT()
struct FGMRManifestEvent
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Frame = 0;

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FString Tag;

	UPROPERTY()
	FString Result;
};

/** @brief Written next to the frames; post cuts [StartFrame, EndFrame) and the critic reads the shot list. */
USTRUCT()
struct FGMRManifest
{
	GENERATED_BODY()

	UPROPERTY()
	FString JobName;

	UPROPERTY()
	FString Aspect;

	UPROPERTY()
	int32 Fps = 30;

	UPROPERTY()
	int32 StartFrame = -1;

	UPROPERTY()
	int32 EndFrame = -1;

	UPROPERTY()
	int32 TotalFrames = 0;

	UPROPERTY()
	bool bComplete = false;

	UPROPERTY()
	bool bRealisticBalance = false;

	UPROPERTY()
	FVector Anchor = FVector::ZeroVector;

	UPROPERTY()
	TArray<FGMRManifestShot> Shots;

	UPROPERTY()
	TArray<FGMRManifestEvent> Events;

	UPROPERTY()
	TArray<FString> Warnings;

	UPROPERTY()
	int32 VoiceLinesHeard = 0;
};
