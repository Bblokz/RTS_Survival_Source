// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "RTSBarbedWireLine.generated.h"

class AFieldConstruction;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EBarbedWirePieceState : uint8
{
	// Laid out by the player; shown as a ghost until its turn comes and it is paid for.
	Planned,
	// Paid for; rises out of the ground while it is strung.
	Building,
	// Stands as its own scavenger barbed wire actor: destroyed, crushed and blocking infantry on its own.
	Built,
	// Its wire actor died or is gone.
	Destroyed
};

/** One straight piece of wire on the ground plan, from where it starts to where it ends. */
USTRUCT()
struct FBarbedWireSpan
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D Start = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D End = FVector2D::ZeroVector;

	float GetLength() const { return FVector2D::Distance(Start, End); }
	FVector2D GetCentre() const { return (Start + End) * 0.5f; }
	bool operator==(const FBarbedWireSpan& Other) const { return Start.Equals(Other.Start) && End.Equals(Other.End); }
};

USTRUCT()
struct FBarbedWirePieceRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	FBarbedWireSpan Span;

	// The span laid onto the ground: its centre on the ground, its X along the span, leaning with the slope.
	UPROPERTY()
	FTransform GroundTransform = FTransform::Identity;

	// How far the piece leans from upright to lie on the ground, in degrees.
	UPROPERTY()
	float SlopeDegrees = 0.0f;

	// Whether ground was found under the piece; a piece without ground cannot be built.
	UPROPERTY()
	bool bHasGround = false;

	// Shows the piece while it is planned and while it is strung; removed once the wire actor stands.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GhostComponent;

	// The scavenger barbed wire actor that is this piece once it is built.
	UPROPERTY()
	TWeakObjectPtr<AFieldConstruction> WireActor;

	UPROPERTY()
	EBarbedWirePieceState State = EBarbedWirePieceState::Planned;

	// 0 when stringing starts, 1 when the piece stands.
	UPROPERTY()
	float BuildProgress = 0.0f;

	bool GetIsStanding() const { return State != EBarbedWirePieceState::Destroyed; }
};

/**
 * @brief A barbed wire line the player dragged out from the HQ's Barriers tab: a chain of straight wire pieces along
 * the dragged path. Pieces are strung one after another from the start of the line; each is paid for, per metre, when
 * stringing starts and waits while the player cannot pay or a unit stands on its spot. A finished piece becomes its
 * own scavenger barbed wire actor (PieceClass), so every piece is destroyed by weapons, crushed by any vehicle (its
 * mesh's convex hull overlaps vehicles; the Blueprint binds the crush) and blocks infantry navigation on its own,
 * exactly like the wire the scavengers build. The line keeps the plan, the
 * building order and the open ends new lines snap to, and removes itself once none of its pieces is left.
 * Spawned and laid out by UBarbedWirePlacementComponent.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSBarbedWireLine : public AActor
{
	GENERATED_BODY()

public:
	ARTSBarbedWireLine();

	/**
	 * @brief Lays straight pieces along a path, one piece length apart along it. A free end lands within half a piece
	 * of the end of the path; a closing end lands on it exactly, its last piece then overlapping the one before it.
	 * @param Path Ground points from the start of the line to its end.
	 * @param PieceLength Distance between the two ends of a piece.
	 * @param MaximumLength The line is not planned longer than this.
	 * @param bEndOnPathEnd The last piece ends exactly at the end of the path, as when the end snapped to other wire.
	 * @return The pieces, from the start of the path; empty for an empty path.
	 */
	static TArray<FBarbedWireSpan> PlanAlongPath(
		const TArray<FVector2D>& Path,
		const float PieceLength,
		const float MaximumLength,
		const bool bEndOnPathEnd);

	/**
	 * @brief Shows a plan as ghosts while the player lays the line out; nothing is paid or built yet. Ghosts are reused,
	 * so a plan can be replaced every frame.
	 * @param Spans The pieces; each is laid onto the ground under it.
	 * @param GroundHeight Roughly the height of the ground along the line; the ground is looked for around it.
	 */
	void SetPlannedPieces(const TArray<FBarbedWireSpan>& Spans, const float GroundHeight);

	/** @brief Colours the ghosts as a plan that can be built or one that cannot. */
	void SetPlanIsValid(const bool bIsValid);

	/**
	 * @brief Starts stringing the planned pieces in order.
	 * @param OwningPlayer The player that pays for the wire and owns its pieces.
	 * @param CostPerMetre What one metre of wire costs; empty builds for free.
	 */
	void StartConstruction(const int32 OwningPlayer, const TMap<ERTSResourceType, int32>& CostPerMetre);

	const TArray<FBarbedWirePieceRuntime>& GetPieces() const { return M_Pieces; }
	int32 GetPieceCount() const { return M_Pieces.Num(); }
	int32 GetBuiltPieceCount() const;
	bool GetIsUnderConstruction() const { return bM_IsConstructionStarted; }
	bool GetIsWaitingForResources() const { return bM_IsWaitingForResources; }
	bool GetIsWaitingForClearSpot() const { return bM_IsWaitingForClearSpot; }
	int32 GetOwningPlayer() const { return M_OwningPlayer; }
	float GetPieceLength() const { return PieceLength; }

	/** @return The steepest lean of any piece that is not built yet, in degrees. */
	float GetSteepestPlannedSlopeDegrees() const;

	/**
	 * @return Whether something buildings may not be placed on (a building, a pit, a wall) stands where the piece goes,
	 * on the building placement channel the grid uses. Units and other barbed wire do not count.
	 */
	bool GetIsPieceOnObstacle(const FBarbedWirePieceRuntime& Piece) const;

	/**
	 * @brief Finds the open end of the player's wire nearest to a location, for a new line to start or end on: an end
	 * of a standing or coming piece that no other piece continues from, also the ends a destroyed piece left open.
	 * @param World The world to look in.
	 * @param Location Where the click or the mouse is.
	 * @param Radius Ends further away are not found.
	 * @param OwningPlayer Only this player's lines count.
	 * @param IgnoredLine A line left out, such as the one being laid out.
	 * @param OutEnd The open end, on the ground.
	 * @return Whether an open end lies within the radius.
	 */
	static bool FindOpenEndNear(
		UWorld* World,
		const FVector2D& Location,
		const float Radius,
		const int32 OwningPlayer,
		const ARTSBarbedWireLine* IgnoredLine,
		FVector& OutEnd);

	/** @return What a piece of this length costs at the given price per metre; at least 1 of every priced resource. */
	static TMap<ERTSResourceType, int32> GetCostOfLength(
		const TMap<ERTSResourceType, int32>& CostPerMetre,
		const float LengthCm);

	/** @brief Collects the ends of standing or coming pieces of this line that no other piece of this line continues from. */
	void GetOpenEndsWithinLine(TArray<FVector>& OutOpenEnds) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// The actor every finished piece becomes: the scavengers' barbed wire, with its health, nav blocker and crush.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire")
	TSoftClassPtr<AFieldConstruction> PieceClass;

	// The mesh of PieceClass, shown as the ghost while a piece is planned and strung; centred on its origin along X.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire")
	TSoftObjectPtr<UStaticMesh> PieceMesh;

	// Shown on every slot of a planned piece; its "PlacementOkay" parameter turns it red.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire")
	TSoftObjectPtr<UMaterialInterface> PlannedPieceMaterial;

	// Distance between the two ends of a piece; a little less than the mesh's 4.14 m so neighbouring coils overlap.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire", meta = (ClampMin = "100.0", Units = "cm"))
	float PieceLength = 400.0f;

	// Seconds of stringing per metre of wire.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire", meta = (ClampMin = "0.0"))
	float BuildSecondsPerMetre = 1.0f;

	// How deep a piece starts below its place when stringing begins; it rises to the ground as it is strung.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire", meta = (ClampMin = "0.0", Units = "cm"))
	float BuildStartDepth = 130.0f;

	// How often a piece that waits for resources or a clear spot checks again.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire", meta = (ClampMin = "0.1"))
	float RetrySeconds = 0.5f;

	// A piece whose spot stays taken this long is strung anyway, so a wreck or an enemy cannot stall the line forever.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire", meta = (ClampMin = "0.0"))
	float MaximumClearSpotWaitSeconds = 10.0f;

	// Half the depth of the spot a piece takes; a unit this close beside the wire stands on it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Barbed Wire", meta = (ClampMin = "0.0", Units = "cm"))
	float SpotHalfDepth = 75.0f;

private:
	UPROPERTY()
	TArray<FBarbedWirePieceRuntime> M_Pieces;

	UPROPERTY()
	TMap<ERTSResourceType, int32> M_CostPerMetre;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_PlannedMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> M_LoadedPieceMesh;

	int32 M_OwningPlayer = INDEX_NONE;
	float M_TimeUntilRetry = 0.0f;
	// How long the piece to string has waited for its spot to clear.
	float M_ClearSpotWaitSeconds = 0.0f;
	bool bM_IsConstructionStarted = false;
	bool bM_IsWaitingForResources = false;
	bool bM_IsWaitingForClearSpot = false;
	bool bM_IsRemovalScheduled = false;

	UStaticMeshComponent* CreateGhostComponent();
	UStaticMesh* GetPieceMesh();
	UMaterialInterface* GetPlannedMaterial();

	// Lays the piece onto the ground: its centre on the ground, its length and depth along the slope.
	void FitPieceToGround(FBarbedWirePieceRuntime& Piece, const float AroundHeight) const;
	bool TryTraceGround(const FVector2D& GroundPoint, const float AroundHeight, FVector& OutGround) const;
	void ShowPieceState(FBarbedWirePieceRuntime& Piece);

	/** @return Index of the first piece that is planned or being strung, or INDEX_NONE when none is left. */
	int32 FindPieceToBuild() const;
	void TickBuilding(const float DeltaSeconds);

	/**
	 * @brief Starts stringing the piece once its spot is clear and it is paid for.
	 * @return Whether stringing started.
	 */
	bool TryStartPiece(FBarbedWirePieceRuntime& Piece, const float DeltaSeconds);

	/** @return Whether a unit stands where the piece goes; the player's own units there are ordered away. */
	bool GetIsSpotTakenAndClearOwnUnits(const FBarbedWirePieceRuntime& Piece) const;
	FBox GetPieceWorldBounds(const FBarbedWirePieceRuntime& Piece) const;
	bool TryPayForPiece() const;

	/** @brief The piece stands: spawns its wire actor and listens for its death. */
	void FinishPiece(const int32 PieceIndex);
	void BindToWireActor(const int32 PieceIndex, AFieldConstruction& WireActor);
	void OnWireActorDied(const int32 PieceIndex);

	UFUNCTION()
	void OnWireActorDestroyed(AActor* DestroyedActor);

	void MarkPieceDestroyed(const int32 PieceIndex);
	void DestroyGhost(FBarbedWirePieceRuntime& Piece) const;

	// The line goes once nothing of it stands or is still to come; checked a tick later, outside the death broadcast.
	void ScheduleRemovalIfEmpty();
	void RemoveIfEmpty();
	bool GetHasStandingPiece() const;
};
