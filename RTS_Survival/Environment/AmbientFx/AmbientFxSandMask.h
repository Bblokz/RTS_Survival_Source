// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AmbientFxSandMask.generated.h"

/**
 * @brief Which grid cells of one map are sand, baked in the editor from the landscape paint layers with the console
 * command RTS.AmbientFx.BakeSandMask. ULandscapeAmbientFxSubsystem loads SandMask_<MapName> from the folder in the
 * RTS Ambient Effects settings; dust devils only form on its sand cells.
 */
UCLASS()
class RTS_SURVIVAL_API UAmbientFxSandMask : public UDataAsset
{
	GENERATED_BODY()

public:
	// World XY of the corner of cell 0.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sand Mask")
	FVector2D Origin = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sand Mask")
	FIntPoint CellCount = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sand Mask")
	float CellSize = 0.f;

	// Row-major, CellCount.X per row: 1 where the sand layers cover at least half of the cell.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sand Mask")
	TArray<uint8> SandCells;

	bool GetIsValidMask() const
	{
		return CellSize > 0.f && SandCells.Num() == CellCount.X * CellCount.Y && not SandCells.IsEmpty();
	}
};
