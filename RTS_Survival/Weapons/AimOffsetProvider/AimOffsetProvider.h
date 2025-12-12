// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AimOffsetProvider.generated.h"

/**
 * @brief C++-only interface for supplying up to 4 local-space aim offsets.
 * Implement this on any actor class in C++ (NotBlueprintable). The offsets are local to actor origin.
 * The targeting struct will cache and use them; if fewer than 4 are provided, it will fill with fallbacks.
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UAimOffsetProvider : public UInterface
{
	GENERATED_BODY()
};

class IAimOffsetProvider
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fill OutLocalOffsets with up to 4 local-space offsets (front/muzzle/chest/head etc).
	 * @note Do NOT transform to world here; the targeting struct will transform.
	 */
	virtual void GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const = 0;
};
