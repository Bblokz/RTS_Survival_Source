// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSRagdollSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSCorpseSubsystem.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class URTSCorpsePoseLibrary;
struct FRTSCorpsePose;

/** A settled crew ragdoll that should stay on the battlefield. */
struct FRTSCorpseRequest
{
	ERTSCrewSide Side = ERTSCrewSide::German;
	// The look the body wore; nullptr keeps the pose mesh's own material.
	UMaterialInterface* Look = nullptr;
	FTransform Pelvis = FTransform::Identity;
};

/** The instances of one pose in one look; hidden instances wait in FreeInstances for the next corpse. */
USTRUCT()
struct FRTSCorpseBatch
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Component;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Look;

	TArray<int32> FreeInstances;
	int32 SideIndex = 0;
	int32 PoseIndex = 0;
};

/** Where one visible corpse lives. */
struct FRTSCorpseSlot
{
	int32 BatchIndex = 0;
	int32 InstanceIndex = 0;
};

/**
 * @brief Keeps settled crew ragdolls on the battlefield as instances of baked poses, up to the player's Battlefield
 * Corpses setting (rts.Corpses); the oldest corpse makes way for a new one. URTSRagdollSubsystem hands it each body
 * as it freezes and parks the body at once when a corpse took its place.
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
	 * @return False when corpses are switched off or no poses exist; the body should then sink away as before.
	 */
	bool TryAddCorpse(const FRTSCorpseRequest& Request);

	/** @return How many corpses the player allows at once; 0 switches them off. */
	static int32 GetCorpseCap();

	int32 GetCorpseCount() const { return M_Corpses.Num(); }

	/** @return Instances across all batches, hidden ones included: it never grows beyond the most corpses ever shown. */
	int32 GetInstanceCount() const;

	int32 GetBatchCount() const { return M_Batches.Num(); }

	/**
	 * @brief Reads one visible corpse, oldest first; for tests and debugging.
	 * @param CorpseIndex 0 is the oldest corpse.
	 * @param OutSideIndex ERTSCrewSide of the corpse.
	 * @param OutPoseIndex Its pose in that side's library.
	 * @param OutTransform Its world transform.
	 * @return False when no corpse has that index.
	 */
	bool GetCorpse(int32 CorpseIndex, int32& OutSideIndex, int32& OutPoseIndex, FTransform& OutTransform) const;

private:
	const URTSCorpsePoseLibrary* GetLibrary(int32 SideIndex) const;

	/** @return A pose close to how the body lies, preferring one whose batch has a slot an evicted corpse freed. */
	int32 PickPose(const URTSCorpsePoseLibrary& Library, const FTransform& Pelvis, int32 SideIndex,
	               const UMaterialInterface* Look) const;

	bool GetHasFreeInstance(int32 SideIndex, int32 PoseIndex, const UMaterialInterface* Look) const;
	void CompactBatches();
	void CompactBatch(int32 BatchIndex);
	FTransform GetCorpseTransform(const FRTSCorpsePose& Pose, const FTransform& Pelvis) const;
	FQuat GetGroundTilt(const FVector& GroundNormal) const;
	int32 FindOrCreateBatch(int32 SideIndex, int32 PoseIndex, UMaterialInterface* Look);
	bool EnsureHolder();
	void RemoveOldestCorpse();
	void TrimToCap();

	// One per ERTSCrewSide, in enum order; null where no library is set.
	UPROPERTY()
	TArray<TObjectPtr<URTSCorpsePoseLibrary>> M_Libraries;

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
