// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "AircraftDeathCookReferences.generated.h"

/**
 * @brief Lists the default effects and sounds of UAircraftDeathComponent, which C++ finds by path. One asset of this
 * class lives in the always-cooked CrashCollections folder (DA_AircraftDeathCookReferences) so the cook packs them.
 */
UCLASS()
class RTS_SURVIVAL_API UAircraftDeathCookReferences : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Cook")
	TArray<TSoftObjectPtr<UObject>> AssetsToCook;
};
