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
