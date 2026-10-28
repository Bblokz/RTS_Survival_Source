// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GroundSurfaceMap.generated.h"

/**
 * @brief The ground surface of one map per grid cell, baked in the editor with the console command
 * RTS.GroundSurface.BakeSurfaceMap from the landscape paint layers and the surface meshes (roads, paths, tagged actors).
 * UGroundSurfaceSubsystem loads SurfaceMap_<MapName> from the folder in the RTS Ground Surfaces settings.
 */
UCLASS()
class RTS_SURVIVAL_API UGroundSurfaceMap : public UDataAsset
{
	GENERATED_BODY()

public:
	// World XY of the corner of cell 0.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Surface Map")
	FVector2D Origin = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Surface Map")
	FIntPoint CellCount = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Surface Map")
	float CellSize = 0.f;

	// Row-major, CellCount.X per row: the EGroundSurfaceType that covers most of the cell.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Surface Map")
	TArray<uint8> SurfaceCells;

	bool GetIsValidMap() const
	{
		return CellSize > 0.f && SurfaceCells.Num() == CellCount.X * CellCount.Y && not SurfaceCells.IsEmpty();
	}
};
