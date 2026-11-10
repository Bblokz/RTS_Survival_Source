// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "RTS_Survival/Buildings/FieldTrench/RTSFieldTrench.h"

#include "RTSFieldTrenchSpline.generated.h"

class USplineComponent;

/** What an end of a spline trench is joined to. */
UENUM(BlueprintType)
enum class EFieldTrenchSplineJoinKind : uint8
{
	None,
	// A TrenchPort_ socket of a pit or bunker (UFieldTrenchAnchorComponent).
	Anchor,
	// The free start of another spline trench; the two trenches run on as one.
	TrenchStart,
	// The free end of another spline trench.
	TrenchEnd,
	// The side of another spline trench, which puts a Tee there.
	TrenchSide
};

/** @brief A port in world space; the yaw points out of whatever the port belongs to. */
USTRUCT(BlueprintType)
struct FFieldTrenchSplinePort
{
	GENERATED_BODY()

	// Z is the height of the ground at the port.
	UPROPERTY(VisibleInstanceOnly, Category = "Field Trench|Spline")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, Category = "Field Trench|Spline")
	float Yaw = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Field Trench|Spline")
	bool bIsSet = false;

	FFieldTrenchOpenPort ToOpenPort() const { return FFieldTrenchOpenPort{FVector2D(Location), Yaw}; }
	static FFieldTrenchSplinePort Make(const FFieldTrenchOpenPort& Port, const float Height);
};

/** @brief What one end of a spline trench snapped to. The trench whose end it is owns the join. */
USTRUCT(BlueprintType)
struct FFieldTrenchSplineJoin
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, Category = "Field Trench|Spline")
	EFieldTrenchSplineJoinKind Kind = EFieldTrenchSplineJoinKind::None;

	// The pit or bunker of an anchor, or the other spline trench.
	UPROPERTY(VisibleInstanceOnly, Category = "Field Trench|Spline")
	TObjectPtr<AActor> Target;

	UPROPERTY(VisibleInstanceOnly, Category = "Field Trench|Spline")
	FName AnchorSocket;

	// Where the spline of this end is; a trench joined at its side puts its Tee nearest to it.
	UPROPERTY()
	FVector EndPoint = FVector::ZeroVector;

	bool IsSet() const { return Kind != EFieldTrenchSplineJoinKind::None && IsValid(Target); }
};

/** @brief A Tee this trench put in for another spline trench that joins it at the side. */
USTRUCT(BlueprintType)
struct FFieldTrenchSplineTee
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Joiner;

	// 0 for the joiner's start, 1 for its end.
	UPROPERTY()
	uint8 JoinerEnd = 0;

	UPROPERTY()
	FFieldTrenchSplinePort SidePort;
};

/**
 * @brief A field trench drawn in the level editor: the trench follows the spline and is rebuilt from the kit
 * every time the spline, the actor or one of its settings changes, while points are still being dragged.
 * The pieces are planned with the same planner as the trenches players drag out (ARTSFieldTrench::PlanOnGround)
 * and are laid onto the ground under the spline. They are built: in game they are trench cover from the start.
 *
 * Joining: an end of the spline within SnapRadius of a pit's TrenchPort_ socket, of a free end of another spline
 * trench, or of the side of another spline trench joins it. The trench then leaves from, or closes onto, that
 * port exactly: its last straight is stretched to land on it. Joined to another trench's end, the two run on
 * as one; joined to its side, the other trench puts a Tee there. The spline point itself is not moved.
 * In game a joined anchor is claimed (its neck shows) and when its owner goes the trench caps that end.
 *
 * @note Place it as BP_FieldTrenchSpline (Environment/Trenches/FieldTrench): the editor only reruns the
 * construction of Blueprint actors, so a plain C++ instance does not follow its spline.
 * @note The pieces are construction script components: they are saved with the level and cannot be edited by
 * hand; edit the spline instead.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ARTSFieldTrenchSpline : public ARTSFieldTrench
{
	GENERATED_BODY()

public:
	ARTSFieldTrenchSpline();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;
	virtual void OnStartAnchorLost(const FFieldTrenchOpenPort& AnchorPort, const float AnchorHeight) override;

	USplineComponent* GetTrenchPath() const { return TrenchPath; }

	/**
	 * @brief Looks for what the spline's ends snap to, plans the trench along the spline again and lays its
	 * pieces onto the ground; spline trenches joined to this one follow.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Field Trench|Spline")
	void RebuildTrench();

	/** @brief Moves every spline point onto the ground under it, so the drawn line lies where the trench is. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Field Trench|Spline")
	void SnapSplinePointsToGround();

	/** @param End 0 for the start of the trench, 1 for its end. */
	const FFieldTrenchSplineJoin& GetJoin(const int32 End) const { return End == 0 ? StartJoin : EndJoin; }

	/** @return The open port at an end of the trench, pointing out of it; unset before the trench is planned. */
	const FFieldTrenchSplinePort& GetOutwardPort(const int32 End) const
	{
		return End == 0 ? M_StartOutwardPort : M_EndOutwardPort;
	}

	/** @return The side port of the Tee put in for that end of the joiner; unset when there is none. */
	FFieldTrenchSplinePort GetTeePortFor(const AActor* Joiner, const int32 JoinerEnd) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The line the trench follows. Its points can be dragged, added (alt-drag) and removed in the viewport.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Field Trench|Spline")
	TObjectPtr<USplineComponent> TrenchPath;

	// Longest straight piece the trench may use; shorter straights follow the spline more closely.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field Trench|Spline",
		meta = (ClampMin = "125.0", ClampMax = "500.0", Units = "cm"))
	float MaximumStraightLength = 500.0f;

	// The trench stops once it is this long, end cap included.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field Trench|Spline",
		meta = (ClampMin = "250.0", Units = "cm"))
	float MaximumTrenchLength = 100000.0f;

	// How far apart the spline is sampled for the planner; it aims about 3 m ahead along these points.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field Trench|Spline",
		meta = (ClampMin = "10.0", ClampMax = "200.0", Units = "cm"))
	float PathSampleSpacing = 50.0f;

	// Ends of the spline join trench anchors and other spline trenches near them.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field Trench|Spline|Joining")
	bool bSnapEnds = true;

	// How near an end of the spline has to be to what it joins.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field Trench|Spline|Joining",
		meta = (ClampMin = "50.0", ClampMax = "1500.0", Units = "cm", EditCondition = "bSnapEnds"))
	float SnapRadius = 350.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Field Trench|Spline|Joining")
	FFieldTrenchSplineJoin StartJoin;

	UPROPERTY(VisibleInstanceOnly, Category = "Field Trench|Spline|Joining")
	FFieldTrenchSplineJoin EndJoin;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Field Trench|Spline|Result")
	int32 PieceCount = 0;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Field Trench|Spline|Result", meta = (Units = "cm"))
	float TrenchLength = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Field Trench|Spline|Result", meta = (Units = "deg"))
	float SteepestPieceSlope = 0.0f;

private:
	// Saved with the level: a trench joined to this one plans against them, also before this one is rebuilt.
	UPROPERTY()
	FFieldTrenchSplinePort M_StartOutwardPort;

	UPROPERTY()
	FFieldTrenchSplinePort M_EndOutwardPort;

	UPROPERTY()
	TArray<FFieldTrenchSplineTee> M_Tees;

	// The anchors this trench holds in game, per end.
	FFieldTrenchAnchorHandle M_ClaimedAnchors[2];
	int32 M_AnchorClaimTriesLeft = 0;
	FTimerHandle M_AnchorClaimTimer;
	bool bM_IsBeingRemoved = false;

	/** @return The spline sampled in world space, the last point included. */
	TArray<FVector> SampleTrenchPath() const;

	/** @return What the end of the spline at this point joins; nothing when it snaps to nothing that is free. */
	FFieldTrenchSplineJoin FindJoin(const int32 End, const FVector& EndPoint, const FFieldTrenchSplineJoin& OtherEnd) const;

	/** @return The port the end joins, pointing out of what it joins; unset when it is gone or not ready. */
	FFieldTrenchSplinePort GetJoinedPort(const FFieldTrenchSplineJoin& Join, const int32 End) const;

	/** @return Whether another spline trench (not this one) joins the given end of the target. */
	bool GetIsTrenchEndTaken(const ARTSFieldTrenchSpline& Target, const int32 TargetEnd) const;

	/** @return Whether a spline trench other than this one joins this anchor socket. */
	bool GetIsAnchorTaken(const AActor& AnchorOwner, const FName SocketName) const;

	/** @return The spline trenches with a join onto this one. */
	TArray<ARTSFieldTrenchSpline*> FindJoiners() const;

	/** @brief Plans the pieces from the joins that are resolved and builds them. */
	void BuildPieces(const TArray<FVector>& PathSamples);

	/** @brief Puts straights from an open end onto the port, stretched to land on it. */
	void CloseOntoPort(
		TArray<FFieldTrenchPiecePlacement>& Pieces,
		const int32 FirstTrenchPiece,
		const FFieldTrenchOpenPort& OpenEnd,
		const FFieldTrenchOpenPort& TargetPort) const;

	void RefreshGroundIgnoredActors();
	void MarkPackageDirtyInEditor();

	void TryClaimJoinedAnchors();
	void ReleaseClaimedAnchors();

	static bool GetIsSplineTrenchUsable(const ARTSFieldTrenchSpline* Trench);
};
