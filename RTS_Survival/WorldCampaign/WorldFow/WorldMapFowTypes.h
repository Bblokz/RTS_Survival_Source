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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW")
	bool bM_WritesVisibleMask = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW")
	bool bM_WritesExplorableMask = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW")
	bool bM_WritesPOIMask = false;
};
