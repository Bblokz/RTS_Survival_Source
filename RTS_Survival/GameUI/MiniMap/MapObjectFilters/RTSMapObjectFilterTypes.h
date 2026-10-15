// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSMapObjectFilterTypes.generated.h"

/** Map objects the tactical map can show once the player has discovered them. */
UENUM(BlueprintType)
enum class ERTSMapObjectFilter : uint8
{
	Radixite,
	Metal,
	Scavengeables,
	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(ERTSMapObjectFilter, ERTSMapObjectFilter::Count);

/** Where a map object filter is drawn; toggled per filter on the tactical map. */
UENUM(BlueprintType)
enum class ERTSMapObjectFilterMode : uint8
{
	Off,
	TacticalMapOnly UMETA(DisplayName = "Tactical Map Only"),
	MiniMapAndTacticalMap UMETA(DisplayName = "Minimap And Tactical Map"),
};

/** Colours and sizes of one filter's dots; set in Project Settings > Game > Map Object Filters. */
USTRUCT(BlueprintType)
struct FRTSMapObjectFilterLook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Look")
	FLinearColor Color = FLinearColor::White;

	/** Dot diameter on the small minimap, in UI units. */
	UPROPERTY(EditAnywhere, Category = "Look", meta = (ClampMin = "0.5", ClampMax = "32.0"))
	float MiniMapDiameter = 3.f;

	/** Dot diameter on the tactical (full screen) map, in UI units. */
	UPROPERTY(EditAnywhere, Category = "Look", meta = (ClampMin = "0.5", ClampMax = "64.0"))
	float TacticalMapDiameter = 5.f;

	/** Ring drawn around each dot so dark colours stay readable on the terrain (0 width hides it). */
	UPROPERTY(EditAnywhere, Category = "Look")
	FLinearColor OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.6f);

	UPROPERTY(EditAnywhere, Category = "Look", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float OutlineWidth = 0.f;

	/** Mode the filter starts in each game. */
	UPROPERTY(EditAnywhere, Category = "Look")
	ERTSMapObjectFilterMode DefaultMode = ERTSMapObjectFilterMode::Off;
};
