// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UPackage;
class USkeletalMeshComponent;
class UStaticMesh;

/** What one bake produced, for the log. */
struct FCorpsePoseBakeResult
{
	UStaticMesh* Mesh = nullptr;
	int32 Triangles = 0;
	// Lowest vertex relative to the pivot; near zero when the body lies on the ground.
	float LowestZ = 0.f;
	// The bone that weighs most on the lowest vertex: where a body sinks into the ground.
	FName LowestBone;
	bool bSaved = false;
};

/**
 * @brief Turns a settled ragdoll into a static mesh asset for the battlefield corpse library. The pose is skinned on
 * the CPU from the imported LOD0, so it needs no render state and runs in any editor build, -game included.
 */
namespace CorpsePoseBaker
{
#if WITH_EDITOR
	/**
	 * @brief Bakes the current pose; the materials are the ones the component shows now.
	 * @param CrewMesh The frozen ragdoll.
	 * @param Pivot World point that becomes the mesh origin: the pelvis over the ground.
	 * @param PackagePath Long package name of the new asset, such as /Game/Folder/SM_Name.
	 * @return The mesh, its size and whether the package reached disk.
	 */
	FCorpsePoseBakeResult BakeAndSave(const USkeletalMeshComponent* CrewMesh, const FVector& Pivot,
	                                  const FString& PackagePath);

	/** @return A package for a new asset that replaces whatever an earlier bake saved there. */
	UPackage* CreateAssetPackage(const FString& PackagePath);

	/** @return Whether the asset reached disk; works outside the editor too, as the bake runs with -game. */
	bool SaveAsset(UPackage* Package, UObject* Asset);
#endif
}
