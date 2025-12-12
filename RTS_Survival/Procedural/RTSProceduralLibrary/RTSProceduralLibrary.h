// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGPrimitiveData.h"
#include "RTSProceduralLibrary.generated.h"

/**
 * A library of procedural functions.
 */
UCLASS()
class RTS_SURVIVAL_API URTSProceduralLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Filters point data based on whether they lie within given spatial data.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "RTSCard")
	static FPCGDataCollection GetPointDataNotOnVolumes(TArray<UPCGPointData*> InPointData,
	                                                   TArray<UPCGSpatialData*> InSpatialData,
	                                                   const FName& OutputPinName);

	/**
	 * For each point in the provided point data, generates a volume.
	 * @param InPointData      Array of point data objects.
	 * @param VolumeBounds     The dimensions for the volume:
	 *                           X: length along the point’s local X (forward) direction,
	 *                           Y: length along the point’s local Y (right) direction,
	 *                           Z: height of the volume.
	 * @param OutputPinName    The name of the output pin to tag the generated volume data.
	 * @return A data collection with one tagged entry per generated volume.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "RTSCard")
	static FPCGDataCollection GenerateVolumesAtPoints(TArray<UPCGPointData*> InPointData,
	                                                    const FVector& VolumeBounds,
	                                                    const FName& OutputPinName);
};
