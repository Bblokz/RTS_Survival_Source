// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSCorpseSubsystem.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class URTSCorpsePoseLibrary;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FRTSCorpsePose;

/** A settled body, soldier or thrown crew, that should stay on the battlefield. */
struct FRTSCorpseRequest
{
	// The body's mesh; it picks the pose library.
	const USkeletalMesh* SourceMesh = nullptr;
	// What the body wore, per material slot of SourceMesh; a missing or null slot keeps the pose mesh's material.
	TArray<UMaterialInterface*> Materials;
	FTransform Pelvis = FTransform::Identity;
	// False after a death montage: the pelvis then shows how the animation ended, not how a body lies, so any pose fits.
	bool bMatchPelvis = true;
};

/** The instances of one pose in one look; hidden instances wait in FreeInstances for the next corpse. */
USTRUCT()
struct FRTSCorpseBatch
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Component;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	TArray<int32> FreeInstances;
	int32 LibraryIndex = 0;
	int32 PoseIndex = 0;
};

/** Where one visible corpse lives. */
struct FRTSCorpseSlot
{
	int32 BatchIndex = 0;
	int32 InstanceIndex = 0;
};

/**
 * @brief Keeps settled infantry and crew ragdolls on the battlefield as instances of baked poses, one library per
 * soldier mesh found in the corpse folder, up to the player's Battlefield Corpses setting (rts.Corpses); the oldest
 * corpse makes way for a new one. Soldiers and the crew pool hand it each body as it freezes.
 */
UCLASS()
class RTS_SURVIVAL_API URTSCorpseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/**
	 * @brief Leaves a corpse in the pose closest to the body's, lying on the ground under its pelvis.
	 * @param Request The settled body.
	 * @return False when corpses are switched off or the mesh has no poses; the body should then go as before.
	 */
	bool TryAddCorpse(const FRTSCorpseRequest& Request);

	/** @return How many corpses the player allows at once; 0 switches them off. */
	static int32 GetCorpseCap();

	/**
	 * @return The pelvis in world space, from its physics body while one exists: bones only refresh while the mesh is
	 * rendered, so a body that fell out of view would report where it was last seen.
	 */
	static FTransform GetPelvisTransform(const USkeletalMeshComponent* Mesh);

	/** @return Whether the pelvis has a physics body, so GetPelvisTransform shows how a ragdoll came to lie. */
	static bool GetHasPelvisBody(const USkeletalMeshComponent* Mesh);

	/** @return The baked poses of this soldier mesh, or nullptr when it has none. */
	const URTSCorpsePoseLibrary* GetLibraryFor(const USkeletalMesh* SourceMesh) const;

	int32 GetLibraryCount() const { return M_Libraries.Num(); }
	int32 GetCorpseCount() const { return M_Corpses.Num(); }

	/** @return Instances across all batches, hidden ones included: it never grows beyond twice the cap. */
	int32 GetInstanceCount() const;

	int32 GetBatchCount() const { return M_Batches.Num(); }

	/**
	 * @brief Reads one visible corpse, oldest first; for tests and debugging.
	 * @param CorpseIndex 0 is the oldest corpse.
	 * @param OutLibrary The pose library of the corpse.
	 * @param OutPoseIndex Its pose in that library.
	 * @param OutTransform Its world transform.
	 * @return False when no corpse has that index.
	 */
	bool GetCorpse(int32 CorpseIndex, const URTSCorpsePoseLibrary*& OutLibrary, int32& OutPoseIndex,
	               FTransform& OutTransform) const;

private:
	void LoadLibraries();
	int32 GetLibraryIndex(const USkeletalMesh* SourceMesh) const;

	/** @return A pose close to how the body lies, preferring one whose batch has a slot an evicted corpse freed. */
	int32 PickPose(int32 LibraryIndex, const FRTSCorpseRequest& Request) const;

	FTransform GetCorpseTransform(const FRTSCorpsePose& Pose, const FTransform& Pelvis) const;
	FQuat GetGroundTilt(const FVector& GroundNormal) const;
	int32 FindBatch(int32 LibraryIndex, int32 PoseIndex, const TArray<UMaterialInterface*>& Materials) const;
	int32 FindOrCreateBatch(int32 LibraryIndex, int32 PoseIndex, const TArray<UMaterialInterface*>& Materials);
	bool EnsureHolder();
	void RemoveOldestCorpse();
	void TrimToCap();
	void CompactBatches();
	void CompactBatch(int32 BatchIndex);

	UPROPERTY()
	TArray<TObjectPtr<URTSCorpsePoseLibrary>> M_Libraries;

	// Soldier mesh path to its index in M_Libraries.
	TMap<FSoftObjectPath, int32> M_LibraryByMesh;

	UPROPERTY()
	TArray<FRTSCorpseBatch> M_Batches;

	// Owns every batch component; spawned with the first corpse.
	UPROPERTY()
	TObjectPtr<AActor> M_Holder;

	// Oldest first.
	TArray<FRTSCorpseSlot> M_Corpses;

	// Lowering the setting mid-game removes the oldest corpses on the next check.
	FTimerHandle M_TrimTimer;
};
