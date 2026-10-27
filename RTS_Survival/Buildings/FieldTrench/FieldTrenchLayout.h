// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"

#include "FieldTrenchLayout.generated.h"

class UStaticMesh;

/** The pieces of the field trench kit (GerDefenses/FieldTrenches/Exports/field_trench_kit.json). */
UENUM(BlueprintType)
enum class EFieldTrenchPiece : uint8
{
	None,
	Straight_125,
	Straight_250_A,
	Straight_250_B,
	Straight_250_C,
	Straight_500_A,
	Straight_500_B,
	Bend_15,
	Bend_30,
	Bend_45,
	Corner_90,
	Tee,
	Cross,
	End
};

/**
 * @brief Where two kit pieces join: a point on the centreline at ground level and the yaw pointing out of the
 * piece, in the piece's own Unreal space. Two pieces join where a port of each coincides with opposite yaws.
 */
struct FFieldTrenchPort
{
	FVector2D Location = FVector2D::ZeroVector;
	float Yaw = 0.0f;
};

/** @brief One kit piece as the layout planner sees it. */
struct FFieldTrenchPieceDefinition
{
	EFieldTrenchPiece Piece = EFieldTrenchPiece::None;
	const TCHAR* MeshName = TEXT("");
	TArray<FFieldTrenchPort, TInlineAllocator<4>> Ports;
	// Length of trench the piece adds, along its centreline; what the player pays for.
	float LengthCm = 0.0f;
};

/** @brief One piece of a planned trench, in world space. */
USTRUCT(BlueprintType)
struct FFieldTrenchPiecePlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EFieldTrenchPiece Piece = EFieldTrenchPiece::None;

	// The ground point under the piece origin; the piece only turns about the vertical.
	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float Yaw = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float LengthCm = 0.0f;

	FTransform GetTransform() const { return FTransform(FRotator(0.0f, Yaw, 0.0f), Location); }
};

/** @brief The open end of a trench under construction: where the next piece joins and which way it points. */
struct FFieldTrenchOpenPort
{
	FVector2D Location = FVector2D::ZeroVector;
	float Yaw = 0.0f;
};

/**
 * @brief Lays a chain of kit pieces along the path the player dragged with the mouse: an End cap at the start,
 * straights and bends that follow the path, and an End cap where the drag stopped.
 * Pure geometry in the ground plane; the caller puts each piece on the ground.
 */
namespace FieldTrenchLayout
{
	RTS_SURVIVAL_API const FFieldTrenchPieceDefinition& GetPieceDefinition(EFieldTrenchPiece Piece);

	/** @return The mesh asset of the piece under /Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Meshes. */
	RTS_SURVIVAL_API FSoftObjectPath GetPieceMeshPath(EFieldTrenchPiece Piece);

	/**
	 * @brief Puts a piece so that one of its ports joins an open port.
	 * @param Piece The piece to place.
	 * @param EntryPortIndex Which port of the piece joins; entering a bend from its second port turns it the other way.
	 * @param OpenPort The open end the piece joins.
	 * @param OutPlacement Receives the piece's ground-plane location and yaw; Z is left at zero.
	 * @param OutExitPort Receives the open end of the trench after the piece; unchanged for an End.
	 */
	RTS_SURVIVAL_API void PlacePieceOnPort(
		EFieldTrenchPiece Piece,
		int32 EntryPortIndex,
		const FFieldTrenchOpenPort& OpenPort,
		FFieldTrenchPiecePlacement& OutPlacement,
		FFieldTrenchOpenPort& OutExitPort);

	/**
	 * @brief Plans a trench along a dragged path.
	 * @param DragPath Ground points the cursor passed, from the press to where it is now; at least two apart.
	 * @param MaximumLengthCm The trench is not planned longer than this.
	 * @param MaximumStraightLengthCm Longer straights are not used; shorter ones follow uneven ground better.
	 * @return The pieces in building order: the start cap first and the end cap last; empty for a drag that is
	 * too short for a single straight.
	 */
	RTS_SURVIVAL_API TArray<FFieldTrenchPiecePlacement> PlanAlongPath(
		const TArray<FVector2D>& DragPath,
		float MaximumLengthCm,
		float MaximumStraightLengthCm = 500.0f);

	/** @return The summed length of the pieces, in centimetres. */
	RTS_SURVIVAL_API float GetTotalLengthCm(const TArray<FFieldTrenchPiecePlacement>& Pieces);
}
