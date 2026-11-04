// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AddOnArmorBreakTestTypes.h"

#include "AddOnArmorTestVehicle.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FChaosBreakEvent;

/** One shard of one skirt panel on the test vehicle. */
USTRUCT()
struct FAddOnArmorTestPiece
{
	GENERATED_BODY()

	int32 SideIndex = 0;
	int32 PanelIndex = 0;
	// Which of the shard meshes (and Niagara mesh slot) this piece is.
	int32 ShardIndex = 0;
	// The piece's pivot (its centroid) in vehicle space.
	FTransform VehicleTransform = FTransform::Identity;
	float Radius = 0.f;
	// Leaf of the side's geometry collection; INDEX_NONE for the Niagara technique.
	int32 CollectionTransformIndex = INDEX_NONE;
	bool bIsBroken = false;
	// Game seconds of the release; the geometry collection waits for its break event.
	float ReleaseSeconds = -1.f;
	bool bHasBreakEvent = false;
	// Spike: whether the landing check has looked at this released piece yet.
	bool bIsLandingChecked = false;
};

/** Counters the test subsystem reads to judge and compare the techniques. */
struct FAddOnArmorTestVehicleStats
{
	int32 HitCount = 0;
	int32 HitsThatMissed = 0;
	int32 PiecesReleased = 0;
	// Geometry collection only: break events of released pieces, and of pieces nobody released.
	int32 MatchingBreakEvents = 0;
	int32 UnexpectedBreakEvents = 0;
	int32 NiagaraBursts = 0;
};

/**
 * @brief A Panzer IV stand-in (hull, tracks and turret meshes, no gameplay) carrying test side skirts that break
 * off where a synthetic shell hits them, with either Niagara debris or a geometry collection. Spawned and driven
 * by UAddOnArmorBreakTestSubsystem; never used in the game. Its members are empty in builds without map tests.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class RTS_SURVIVAL_API AAddOnArmorTestVehicle : public AActor
{
	GENERATED_BODY()

public:
	AAddOnArmorTestVehicle();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * @brief Builds the skirts for one technique; call once right after spawning.
	 * @param Technique How the skirts break (None builds the vehicle without skirts).
	 * @param GcCollision How the geometry collection gets its collision.
	 * @param GroundHeight World Z of the ground under the vehicle, used by the Niagara debris.
	 * @return False when assets or the layout are missing.
	 */
	bool SetupSkirts(EAddOnArmorTestTechnique Technique, EAddOnArmorTestGcCollision GcCollision, float GroundHeight);

	/**
	 * @brief Lets the vehicle drive a circle so the test covers skirts that follow a moving hull.
	 * @param CircleCentre World point the vehicle circles around.
	 * @param CircleRadius Radius of the circle (cm).
	 * @param Speed Driving speed (cm/s).
	 * @param StartAngle Where on the circle the vehicle starts (radians).
	 */
	void StartDriving(const FVector& CircleCentre, float CircleRadius, float Speed, float StartAngle);

	/**
	 * @brief Breaks off the pieces around the shell's line and sends them flying.
	 * @param Hit The synthetic shell hit.
	 * @return The number of pieces released; zero when the shell missed every intact piece.
	 */
	int32 ApplyTestHit(const FAddOnArmorTestHit& Hit);

	/** @return The world centre of an intact piece on the given side, picked with the stream; false if none left. */
	bool PickIntactPieceLocation(int32 SideIndex, FRandomStream& Stream, FVector& OutLocation) const;

	/** @return The outward normal of a skirt side in world space. */
	FVector GetSideOutwardNormal(int32 SideIndex) const;

	const FAddOnArmorTestVehicleStats& GetStats() const { return M_Stats; }
	EAddOnArmorTestTechnique GetTechnique() const { return M_Technique; }
	int32 GetIntactPieceCount() const;

	/** @brief Logs released pieces whose geometry collection break event never came; returns their count. */
	int32 CountReleasesWithoutBreakEvent(float GameSecondsToWait) const;

	/**
	 * @brief Spike check: where released geometry collection pieces are once they had time to land, and whether
	 * each Niagara burst carries one particle per released piece.
	 * @param LandingAge Seconds after its release at which a piece is checked.
	 * @param InOutCounts Accumulates what was found.
	 */
	void CheckReleasedPieces(float LandingAge, FAddOnArmorTestSpikeCounts& InOutCounts);

	/** @return Intact geometry collection pieces that are not where they hang on the hull (they must follow it). */
	int32 CountDisplacedIntactPieces(float Tolerance) const;

	/** @return Pieces released less than the debris lifetime ago, the same measure for both techniques. */
	int32 CountLiveDebrisPieces() const;

	/** @brief Adds the components of this vehicle's recent, still active Niagara bursts (pooled components are shared). */
	void AppendActiveBursts(TSet<const UNiagaraComponent*>& InOutBursts) const;

	static int32 CountParticles(const UNiagaraComponent& Burst);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_Hull;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_Tracks;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_Turret;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_Rails;

	// Niagara technique: the intact shards, one instanced mesh component per shard mesh.
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> M_ShardInstances;

	// Geometry collection technique: one collection per skirt side.
	UPROPERTY()
	TArray<TObjectPtr<UGeometryCollectionComponent>> M_SideCollections;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> M_ShardMeshes;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_DebrisSystem;

	UPROPERTY()
	TArray<FAddOnArmorTestPiece> M_Pieces;

	// Per piece, its instance index in the instanced mesh component of its shard mesh (Niagara technique).
	TArray<int32> M_PieceInstanceIndices;

	// Baked techniques: broken pieces as bits, written to the hull's Custom Primitive Data for the skirt material.
	uint32 M_BrokenMask[AddOnArmorBreakTest::MaskWordCount] = {};

	EAddOnArmorTestTechnique M_Technique = EAddOnArmorTestTechnique::None;
	EAddOnArmorTestGcCollision M_GcCollision = EAddOnArmorTestGcCollision::FromStart;
	float M_GroundHeight = 0.f;
	FAddOnArmorTestVehicleStats M_Stats;
	// Geometry collection sides whose collision was switched on by a first break.
	TArray<bool> M_SideCollisionEnabled;

	struct FDriveState
	{
		bool bIsDriving = false;
		FVector CircleCentre = FVector::ZeroVector;
		float CircleRadius = 0.f;
		float AngularSpeed = 0.f;
		float Angle = 0.f;
	};
	FDriveState M_Drive;
	FVector M_Velocity = FVector::ZeroVector;

	// Push and spin of released geometry collection pieces, applied when their break event arrives.
	TMap<int32, FAddOnArmorTestPieceRelease> M_PendingCollectionReleases;

	struct FRecentBurst
	{
		TWeakObjectPtr<UNiagaraComponent> Component;
		int32 ExpectedParticles = 0;
		float SpawnSeconds = 0.f;
		bool bIsChecked = false;
	};
	// Bursts of the last debris lifetime; for the spike's particle check and the perf particle count.
	TArray<FRecentBurst> M_RecentBursts;

	void CreateVehicleMeshes();
	bool LoadPieceLayout();
	bool LoadSkirtAssets();
	void CreateRails();
	void CreateShardInstances();
	bool CreateSideCollections();
	UGeometryCollectionComponent* CreateSideCollection(int32 SideIndex, UGeometryCollection& Collection);
	void ConfigureCollectionCollision(UGeometryCollectionComponent& CollectionComponent, bool bEnable) const;
	bool MapCollectionLeavesToPieces(const UGeometryCollection& Collection);

	/**
	 * @brief Picks the intact pieces nearest the shell's line, within the hit's break radius.
	 * @param Hit The shell hit.
	 * @param OutPieceIndices Receives the picked pieces, nearest first.
	 */
	void SelectPiecesForHit(const FAddOnArmorTestHit& Hit, TArray<int32>& OutPieceIndices) const;

	/** @return The push and spin of each picked piece; deterministic for the hit's seed. */
	TArray<FAddOnArmorTestPieceRelease> MakeReleases(const FAddOnArmorTestHit& Hit, const TArray<int32>& PieceIndices) const;

	void ReleaseWithNiagara(const TArray<FAddOnArmorTestPieceRelease>& Releases);
	// Hides a broken piece on the intact skirts: an instance for the Niagara technique, a mask bit when baked.
	void HideIntactPiece(int32 PieceIndex);
	void HideShardInstance(int32 PieceIndex);
	void MarkPieceBrokenInMask(int32 PieceIndex);
	bool SetupBakedHull();
	void ReleaseWithGeometryCollection(const TArray<FAddOnArmorTestPieceRelease>& Releases);

	UFUNCTION()
	void OnRightSideBreak(const FChaosBreakEvent& BreakEvent);

	UFUNCTION()
	void OnLeftSideBreak(const FChaosBreakEvent& BreakEvent);

	void HandleBreakEvent(int32 SideIndex, const FChaosBreakEvent& BreakEvent);
	int32 FindPieceByCollectionLeaf(int32 SideIndex, int32 TransformIndex) const;

	FTransform GetPieceWorldTransform(const FAddOnArmorTestPiece& Piece) const;
	bool TryGetCollectionLeafWorldLocation(const FAddOnArmorTestPiece& Piece, FVector& OutLocation) const;
	static void LogFirstParticle(const UNiagaraComponent& Burst);
	float GetGameSeconds() const;
	bool GetIsValidDebrisSystem() const;
};
