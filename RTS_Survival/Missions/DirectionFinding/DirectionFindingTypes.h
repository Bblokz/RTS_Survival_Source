// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DirectionFindingTypes.generated.h"

class USoundBase;

class AActor;

/** The three kinds of command post transmission the player's stations can hear; each narrows the bearings. */
UENUM(BlueprintType)
enum class EDirectionFindingBurst : uint8
{
	// Routine traffic on a timer.
	Routine,
	// The command post answers a report that the player attacked one of its sites.
	Reply,
	// The command post orders an attack (a wave launches).
	AttackOrder
};

/** Bearing and burst rules of the direction-finding hunt (OderWartheMissionFlow.md sections 4.2 and 4.3). */
USTRUCT(BlueprintType)
struct FDirectionFindingSettings
{
	GENERATED_BODY()

	// Half-angle in degrees of a new wedge, per difficulty. NewToRTS uses Normal; Ironman uses Brutal.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wedges", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float StartHalfAngleNormal = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wedges", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float StartHalfAngleHard = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wedges", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float StartHalfAngleBrutal = 25.f;

	// Each wedge's centre is off the true bearing by up to this share of its half-angle (random per station), so
	// the command post is always inside the wedge but not on its centre line.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wedges", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float MaxErrorFraction = 0.35f;

	// Bursts never narrow a wedge below this half-angle (degrees).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wedges", meta = (ClampMin = "0.5", ClampMax = "30.0"))
	float MinHalfAngle = 2.f;

	// Half-angle multiplier per burst.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bursts", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float RoutineFactor = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bursts", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ReplyFactor = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bursts", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float AttackOrderFactor = 0.5f;

	// Seconds between routine bursts (random in X..Y) until the player holds two stations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bursts")
	FVector2D RoutineIntervalSeconds = FVector2D(60.f, 90.f);

	// Seconds between routine bursts once the command post noticed the player (two stations held).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bursts")
	FVector2D RoutineIntervalAfterNoticeSeconds = FVector2D(90.f, 120.f);

	// Delay between a reported attack on a site and the command post's reply (random in X..Y).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bursts")
	FVector2D ReplyDelaySeconds = FVector2D(10.f, 20.f);

	// A site that was reported cannot cause another reply for this long.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bursts", meta = (ClampMin = "0.0"))
	float ReplyCooldownPerSiteSeconds = 60.f;

	// Held wedges needed before their overlap can fix the command post.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fix", meta = (ClampMin = "1", ClampMax = "5"))
	int32 StationsNeededForFix = 3;

	// Holding every station at once fixes the command post whatever the wedge widths (the bonus objective).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fix")
	bool bAllStationsGiveFix = true;

	// Seconds a narrowing wedge takes to close on the map.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (ClampMin = "0.0"))
	float NarrowAnimationSeconds = 1.2f;
};

/** Colours of the bearing wedges on the minimap and the tactical map. */
/** The intercepted radio lines of one burst kind; a heard burst plays one of them at random on the intercept portrait. */
USTRUCT(BlueprintType)
struct FDirectionFindingIntercepts
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intercepts")
	TArray<TSoftObjectPtr<USoundBase>> Lines;
};

USTRUCT(BlueprintType)
struct FDirectionFindingLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor HeldFill = FLinearColor(1.f, 0.62f, 0.15f, 0.14f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor HeldEdge = FLinearColor(1.f, 0.7f, 0.25f, 0.85f);

	// A lost station keeps its wedge in these colours and stops narrowing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor LostFill = FLinearColor(0.6f, 0.6f, 0.6f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor LostEdge = FLinearColor(0.7f, 0.7f, 0.7f, 0.5f);

	// The area inside every held wedge (two or more held).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor OverlapFill = FLinearColor(1.f, 0.3f, 0.12f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor OverlapEdge = FLinearColor(1.f, 0.38f, 0.18f, 1.f);

	// Added to the fill alpha right after a wedge narrowed; fades over PulseSeconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PulseFillAlpha = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (ClampMin = "0.05"))
	float PulseSeconds = 1.5f;

	// Edge thickness on the minimap; the tactical map uses TacticalMapEdgeThickness.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (ClampMin = "0.5"))
	float EdgeThickness = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (ClampMin = "0.5"))
	float TacticalMapEdgeThickness = 2.f;
};

/**
 * One bunker group the command post can sit in. Filled by the map build (Source/Tools/OderWartheMap); the target
 * transforms are world transforms of the command post targets if this group is picked.
 */
USTRUCT(BlueprintType)
struct FDirectionFindingWerk
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Werk")
	FName WerkId = NAME_None;

	// World centre of the group; bearings aim here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Werk")
	FVector Centre = FVector::ZeroVector;

	// The group counts as inside a wedge when any point this close to its centre is.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Werk", meta = (ClampMin = "0.0"))
	float RadiusCm = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Werk|Targets")
	FTransform JammerTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Werk|Targets")
	FTransform CommandPorchTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Werk|Targets")
	FTransform RailGateTransform;
};

/** One wedge as the minimap draws it: world-space polygon already clipped to the map square. */
struct FDirectionFindingWedgeDraw
{
	TArray<FVector2D> Polygon;
	bool bHeld = false;
	// 1 right after the wedge narrowed, falling to 0.
	float Pulse = 0.f;
};

struct FDirectionFindingDrawData
{
	TArray<FDirectionFindingWedgeDraw> Wedges;
	// Inside every held wedge; empty with fewer than two held wedges.
	TArray<FVector2D> Overlap;
	// The decoy transmitter's wedges (in Wedges, drawn alike) overlap here; empty without a decoy or below two.
	TArray<FVector2D> DecoyOverlap;
};
