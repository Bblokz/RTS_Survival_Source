#pragma once

#include "CoreMinimal.h"

#include "RTS_PCG_Parameters.generated.h"


USTRUCT(BlueprintType)
struct FRTSPCGParameters
{
	GENERATED_BODY()
	// Name needs to match PCG Graph parameter. Ensure minimal zero.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 PlayerStartSmallResourcePoints = 2;

	// How many of the small resource points are Radixite in the player starting area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 PlayerStartSmallResourceRadixitePoints = 1;

	// How many of the small resource points are Vehicle Parts in the player starting area.	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 PlayerStartSmallResourceVehiclePartsPoints = 1;

	// Name needs to match PCG Graph parameter. Ensure minimal zero.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 PlayerStartMediumResourcePoints = 0;

	// How many of the medium resource points are Radixite in the player starting area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 PlayerStartMediumResourceRadixitePoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 PlayerStartMediumScavengeObjects = 3;
};
