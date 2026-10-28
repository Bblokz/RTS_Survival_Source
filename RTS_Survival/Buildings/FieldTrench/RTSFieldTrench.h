// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Buildings/FieldTrench/FieldTrenchLayout.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverSocketSource.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "RTSFieldTrench.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

UENUM()
enum class EFieldTrenchPieceState : uint8
{
	// Laid out by the player; shown as a ghost until its turn to be dug comes and it is paid for.
	Planned,
	// Paid for; rises out of the ground while it is dug.
	Digging,
	// Finished; its cargo sockets are trench cover.
	Built
};

USTRUCT()
struct FFieldTrenchPieceRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	FFieldTrenchPiecePlacement Placement;

	// The placement laid onto the ground: on the ground under the piece origin, tilted with the slope.
	UPROPERTY()
	FTransform GroundTransform = FTransform::Identity;

	// How far the piece leans from upright to lie on the ground, in degrees.
	UPROPERTY()
	float SlopeDegrees = 0.0f;

	// How far the ground under the piece strays from it, in centimetres: a long piece on a curved slope floats
	// at one spot and sinks at another.
	UPROPERTY()
	float GroundGapCm = 0.0f;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Component;

	UPROPERTY()
	EFieldTrenchPieceState State = EFieldTrenchPieceState::Planned;

	// 0 when digging starts, 1 when the piece is built.
	UPROPERTY()
	float DigProgress = 0.0f;
};

/**
 * @brief A trench the player dragged out at runtime: one actor holding a chain of field trench kit pieces, each
 * its own static mesh component laid onto the ground. Pieces are dug one after another from the start of the
 * trench; every piece is paid for, per metre of trench, when its digging starts, and digging waits while the
 * player cannot pay.
 * Built pieces give trench cover: the cargo sockets of their meshes are handed to the cover finder through
 * IRTSCoverSocketSource, so soldiers are placed on the sockets 60 cm below the ground the same way they are in a
 * hand-placed BP_TrenchMaster trench.
 * @note PieceCollisionTemplate holds the collision every piece copies; its C++ defaults are BP_CargoTrench's.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSFieldTrench : public AActor, public IRTSCoverSocketSource
{
	GENERATED_BODY()

public:
	ARTSFieldTrench();

	/**
	 * @brief Shows a plan as ghosts while the player lays the trench out; nothing is paid or dug yet. Pieces that
	 * stay the same keep their component, so a plan can be replaced every frame.
	 * @param Pieces Pieces in world space; they are laid onto the ground under them.
	 */
	void SetPlannedPieces(const TArray<FFieldTrenchPiecePlacement>& Pieces);

	/** @brief Colours the ghosts as a plan that can be built or one that cannot. */
	void SetPlanIsValid(const bool bIsValid);

	/**
	 * @brief Starts digging the planned pieces in order; replaces the plan first when pieces are given.
	 * @param Pieces The pieces to dig; empty digs the plan that is shown.
	 * @param OwningPlayer The player that pays for the digging.
	 * @param CostPerMetre What one metre of trench costs; empty digs for free.
	 */
	void StartConstruction(
		const TArray<FFieldTrenchPiecePlacement>& Pieces,
		const int32 OwningPlayer,
		const TMap<ERTSResourceType, int32>& CostPerMetre);

	/**
	 * @brief Adds a finished piece at once, without paying; for trenches set up by script and for tests.
	 * @return Index of the new piece.
	 */
	int32 AddBuiltPiece(const FFieldTrenchPiecePlacement& Placement);

	/** @brief Removes a piece, built or not; its cover goes with it. */
	void RemovePiece(const int32 PieceIndex);

	/** @brief Stops digging; pieces that are not built yet are removed and the one being dug is refunded. */
	void CancelUnbuiltPieces();

	int32 GetPieceCount() const { return M_Pieces.Num(); }
	int32 GetBuiltPieceCount() const;
	bool GetIsWaitingForResources() const { return bM_IsWaitingForResources; }
	const TArray<FFieldTrenchPieceRuntime>& GetPieces() const { return M_Pieces; }

	/** @return The steepest lean of any piece that is not built yet, in degrees. */
	float GetSteepestPlannedSlopeDegrees() const;

	/**
	 * @brief Plans a trench along a dragged path on the ground of this world: with the longest straights that
	 * still lie on the ground, so a trench over uneven ground is made of shorter pieces.
	 * @param DragPath Ground points the cursor passed, see FieldTrenchLayout::PlanAlongPath.
	 * @param MaximumLengthCm The trench is not planned longer than this.
	 * @param GroundHeight Roughly the height of the ground along the path; the ground is looked for around it.
	 * @return The pieces, with this height; empty for a drag that is too short.
	 */
	TArray<FFieldTrenchPiecePlacement> PlanOnGround(
		const TArray<FVector2D>& DragPath,
		const float MaximumLengthCm,
		const float GroundHeight) const;

	/** @return What a piece of this length costs at the given price per metre; at least 1 of every priced resource. */
	static TMap<ERTSResourceType, int32> GetCostOfLength(
		const TMap<ERTSResourceType, int32>& CostPerMetre,
		const float LengthCm);

	// IRTSCoverSocketSource
	virtual void GetUsableCoverSockets(TArray<FRTSActorCoverSocket>& OutCoverSockets) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// Its collision settings are copied onto every piece; it has no mesh itself.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Trench")
	TObjectPtr<UStaticMeshComponent> PieceCollisionTemplate;

	// Shown on every slot of a piece that is planned but not dug yet; its "PlacementOkay" parameter turns it red.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Trench")
	TSoftObjectPtr<UMaterialInterface> PlannedPieceMaterial;

	// Seconds of digging per metre of trench.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Trench", meta = (ClampMin = "0.0"))
	float DigSecondsPerMetre = 1.0f;

	// How deep a piece starts below its place when digging begins; it rises to the ground as it is dug.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Trench", meta = (ClampMin = "0.0"))
	float DigStartDepth = 80.0f;

	// How often a trench that waits for resources checks again whether the player can pay.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Field Trench", meta = (ClampMin = "0.1"))
	float ResourceRetrySeconds = 0.5f;

private:
	UPROPERTY()
	TArray<FFieldTrenchPieceRuntime> M_Pieces;

	UPROPERTY()
	TMap<ERTSResourceType, int32> M_CostPerMetre;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_PlannedMaterial;

	int32 M_OwningPlayer = INDEX_NONE;
	float M_TimeUntilResourceRetry = 0.0f;
	bool bM_IsWaitingForResources = false;
	bool bM_CoverRefreshScheduled = false;

	UStaticMeshComponent* CreatePieceComponent(const EFieldTrenchPiece Piece);
	void SetPieceMesh(UStaticMeshComponent& PieceComponent, const EFieldTrenchPiece Piece) const;
	// Lays the piece onto the ground: its origin on the ground, its length and width along the slope.
	void FitPieceToGround(FFieldTrenchPieceRuntime& Piece) const;
	// Straights whose ground strays further from them than this are replaced by shorter ones.
	UPROPERTY(EditDefaultsOnly, Category = "Field Trench", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumGroundGap = 20.0f;
	bool TryTraceGround(const FVector2D& GroundPoint, const float AroundHeight, FVector& OutGround) const;
	UMaterialInterface* GetPlannedMaterial();
	void ShowPieceState(FFieldTrenchPieceRuntime& Piece);

	/** @return Index of the first piece that is not built, or INDEX_NONE when the trench is finished. */
	int32 FindPieceToDig() const;
	void TickDigging(const float DeltaSeconds);
	bool TryPayForPiece(const FFieldTrenchPieceRuntime& Piece) const;
	void RefundPiece(const FFieldTrenchPieceRuntime& Piece) const;
	void OnPieceBuilt(FFieldTrenchPieceRuntime& Piece);

	// Pieces often finish or go in the same frame; the cover finder reads the trench once at the next tick.
	void ScheduleCoverRefresh();
	void RefreshCover();
};
