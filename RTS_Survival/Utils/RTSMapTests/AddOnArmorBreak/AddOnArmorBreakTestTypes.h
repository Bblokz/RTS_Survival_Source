// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "AddOnArmorBreakTestTypes.generated.h"

/**
 * How the test vehicle's add-on armour (side skirts) breaks off. None carries no skirts at all and is the baseline
 * the other two are measured against.
 */
UENUM()
enum class EAddOnArmorTestTechnique : uint8
{
	None,
	// Intact skirts are instanced shard meshes; broken shards are hidden and fly off as pooled Niagara mesh particles.
	Niagara,
	// Each skirt side is a geometry collection with anchored pieces; broken pieces are released to Chaos.
	GeometryCollection,
	// The skirts are baked into the hull mesh; broken shards fly as pooled Niagara debris. Reference material that
	// cannot hide shards (prices the merged geometry alone).
	BakedOpaque,
	// Baked skirts; the skirt material clips broken shards away (opacity mask on a Custom Primitive Data bitmask).
	BakedMask,
	// Baked skirts; the skirt material folds broken shards onto the object origin (world position offset), opaque.
	BakedWpo
};

/** @return Whether the technique bakes the skirts into the hull mesh. */
inline bool GetIsBakedTechnique(const EAddOnArmorTestTechnique Technique)
{
	return Technique == EAddOnArmorTestTechnique::BakedOpaque || Technique == EAddOnArmorTestTechnique::BakedMask
		|| Technique == EAddOnArmorTestTechnique::BakedWpo;
}

/** @return Whether broken pieces fly as Niagara debris (everything but the geometry collection and None). */
inline bool GetUsesNiagaraDebris(const EAddOnArmorTestTechnique Technique)
{
	return Technique == EAddOnArmorTestTechnique::Niagara || GetIsBakedTechnique(Technique);
}

// How the geometry collection gets its collision; the spike decides which one production would need.
UENUM()
enum class EAddOnArmorTestGcCollision : uint8
{
	// No collision while intact; switched on when the first piece breaks.
	OnFirstBreak,
	// Collision from the start, blocking only the ground; anchored pieces are kinematic and touch nothing.
	FromStart
};

/** One synthetic shell hit on the add-on armour, in world space. */
struct FAddOnArmorTestHit
{
	FVector WorldLocation = FVector::ZeroVector;
	// Normalised direction the shell travels in.
	FVector WorldDirection = FVector::ForwardVector;
	// Pieces whose centre lies within this distance of the shell's line break off.
	float BreakRadius = 20.f;
	// Speed the pieces are pushed with along the shell's direction (cm/s).
	float PushSpeed = 450.f;
	int32 MaxPieces = 6;
	// Seeds the per-piece spread so both techniques get the same push for the same hit.
	int32 Seed = 0;
};

/** The release of one piece: identical for both techniques so their pictures and costs can be compared. */
struct FAddOnArmorTestPieceRelease
{
	int32 PieceIndex = INDEX_NONE;
	// Velocity added on top of the vehicle's own (cm/s) and spin (rad/s) around SpinAxis.
	FVector PushVelocity = FVector::ZeroVector;
	FVector SpinAxis = FVector::UpVector;
	float SpinSpeed = 0.f;
};

/** What the spike's checks found; a piece counts once, when it is old enough to have landed. */
struct FAddOnArmorTestSpikeCounts
{
	int32 Landed = 0;
	int32 StillAirborne = 0;
	int32 FellThroughGround = 0;
	// Released but still sitting where it hung on the hull.
	int32 StuckOnHull = 0;
	int32 BurstsWithAllPieces = 0;
	int32 BurstsWithWrongCount = 0;
};

namespace AddOnArmorBreakTest
{
	// Test content: nothing that ships references this folder, so it is never cooked.
	inline const TCHAR* ContentFolder = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak");
	inline const TCHAR* RailMeshPath = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/Meshes/SM_TestSkirtRail.SM_TestSkirtRail");
	inline const TCHAR* CollectionPath = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/GC_TestSkirtSide.GC_TestSkirtSide");
	inline const TCHAR* CollectionPackage = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/GC_TestSkirtSide");
	inline const TCHAR* NiagaraSystemPath = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/NS_TestSkirtDebris.NS_TestSkirtDebris");
	inline const TCHAR* NiagaraSystemPackage = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/NS_TestSkirtDebris");
	// The Panzer IV hull with both skirt sides and rails merged in; shard ids in UV channel ShardIdUvChannel.
	inline const TCHAR* BakedHullMeshPath = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/Meshes/SM_TestPzIVHull_Skirted.SM_TestPzIVHull_Skirted");
	inline const TCHAR* BakedHullMeshPackage = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/Meshes/SM_TestPzIVHull_Skirted");
	inline const TCHAR* SourceHullMeshPath = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Meshes/SM_PanzerIV_Hull.SM_PanzerIV_Hull");
	inline const TCHAR* OpaqueSkirtMaterialPath = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/M_TestSkirtSteel.M_TestSkirtSteel");
	inline const TCHAR* MaskSkirtMaterialPath = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/M_TestSkirtSteelBakedMask.M_TestSkirtSteelBakedMask");
	inline const TCHAR* WpoSkirtMaterialPath = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/M_TestSkirtSteelBakedWpo.M_TestSkirtSteelBakedWpo");
	inline const TCHAR* WpoSkirtMaterialPackage = TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/M_TestSkirtSteelBakedWpo");
	inline const FName SkirtMaterialSlot(TEXT("Skirt"));
	constexpr int32 ShardIdUvChannel = 2;
	// Rails are part of the skirt material slot but never break: their id lies beyond every mask word.
	constexpr float RailShardId = 999.f;
	// Broken shards as a bitmask in Custom Primitive Data 0..3; 20 bits per float stay exact in a float.
	constexpr int32 ShardsPerMaskWord = 20;
	constexpr int32 MaskWordCount = 4;

	// Written by Tools/AddOnArmorBreak/make_skirt_pieces.py: shard centroids in panel space.
	inline const TCHAR* LayoutFile = TEXT("Source/Tools/AddOnArmorBreak/skirt_layout.json");

	constexpr int32 ShardCount = 8;
	constexpr int32 PanelsPerSide = 5;
	constexpr int32 SideCount = 2;
	// Panel width 90 cm plus a 2 cm gap.
	constexpr float PanelPitch = 92.f;
	// Where the skirts hang on the Panzer IV hull (vehicle space): rail centre, outboard of the tracks, top of panels.
	constexpr float RailCentreX = -20.f;
	constexpr float SideOffsetY = 160.f;
	constexpr float PanelTopZ = 150.f;
	// The rail's vertical web sits just inside the plates.
	constexpr float RailInsetY = 0.4f;

	// Both techniques remove debris on the same schedule: whole for BreakToRemoval, then shrinking for Removal.
	constexpr float DebrisSecondsBeforeRemoval = 3.f;
	constexpr float DebrisRemovalSeconds = 0.6f;

	// User parameters of NS_TestSkirtDebris.
	inline const FName PieceCountParameter(TEXT("PieceCount"));
	inline const FName PieceStartParameter(TEXT("PieceStart"));
	inline const FName PieceVelocityParameter(TEXT("PieceVelocity"));
	inline const FName PieceSpinParameter(TEXT("PieceSpin"));
	inline const FName PieceOrientationParameter(TEXT("PieceOrientation"));
	inline const FName GroundHeightParameter(TEXT("GroundHeight"));
	/** @return The user parameter (without the User. prefix) holding the mesh of one debris piece kind. */
	inline FName GetDebrisMeshParameter(const int32 ShardIndex)
	{
		return FName(*FString::Printf(TEXT("DebrisMesh%d"), ShardIndex));
	}

	inline FString GetShardMeshPath(const int32 ShardIndex)
	{
		return FString::Printf(TEXT("/Game/RTS_Survival/Tests/AddOnArmorBreak/Meshes/SM_TestSkirtShard_%d.SM_TestSkirtShard_%d"),
		                       ShardIndex, ShardIndex);
	}
	// Most pieces one Niagara burst carries; the spawn burst is bound to PieceCount.
	constexpr int32 MaxPiecesPerBurst = 16;

	/** @return The transform from skirt-side space to vehicle space; the left side is the right one turned around. */
	inline FTransform GetSideTransform(const int32 SideIndex)
	{
		const bool bIsRightSide = SideIndex == 0;
		const FVector Location(RailCentreX, bIsRightSide ? SideOffsetY : -SideOffsetY, PanelTopZ);
		const FRotator Rotation(0.f, bIsRightSide ? 0.f : 180.f, 0.f);
		return FTransform(Rotation, Location);
	}

	/** @return Where a panel's top centre sits in skirt-side space. */
	inline FVector GetPanelTopCentre(const int32 PanelIndex)
	{
		const float FirstPanelX = -0.5f * (PanelsPerSide - 1) * PanelPitch;
		return FVector(FirstPanelX + PanelIndex * PanelPitch, 0.f, 0.f);
	}
}
