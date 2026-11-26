// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldMapFowTypes.generated.h"

UENUM(BlueprintType)
enum class EWorldMapFowState : uint8
{
	NotVisible UMETA(DisplayName = "Not Visible"),
	Explorable UMETA(DisplayName = "Explorable"),
	Visible UMETA(DisplayName = "Visible"),
	POIVisible UMETA(DisplayName = "POI Visible")
};

/** How a mission the fog fully hides is hinted at (UWorldFogHintSettings). */
UENUM(BlueprintType)
enum class EWorldUnknownMissionLook : uint8
{
	/** A plain grey puck above the fog; nothing of the piece shows. */
	PlainMarker UMETA(DisplayName = "Plain Marker"),
	/** A small opening in the fog cloud shows the real piece at its place on the map. */
	RealPiece UMETA(DisplayName = "Real Piece")
};

/** What a mission looks like for one fog pass; decided by WorldFogHintRules, applied by the fog manager. */
enum class EWorldMissionFogPresentation : uint8
{
	/** The piece as its fog state alone leaves it: seen where the fog is open, covered where it is closed. */
	AsFogState,
	/** Unidentified: the piece is hidden and the plain marker stands above the fog. */
	PlainMarker,
	/** Unidentified: the piece is seen through an opening in the fog that changes nothing else. */
	RealPieceThroughFog
};

USTRUCT(BlueprintType)
struct FWorldMapFowStateSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW")
	float M_RevealRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW")
	float M_RevealFalloff = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW")
	float M_ConnectionCorridorWidth = 450.f;
};

/**
 * @brief An extra circle the player can see that is not an anchor, such as a watch post on a build node.
 * Keyed by SourceId so setting the same source again replaces it.
 */
USTRUCT(BlueprintType)
struct FWorldFowRevealSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|FOW")
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|FOW")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|FOW", meta = (ClampMin = "0"))
	float RevealRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|FOW", meta = (ClampMin = "0"))
	float RevealFalloff = 0.f;
};
