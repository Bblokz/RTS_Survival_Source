// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "MinimapIconDataAsset.generated.h"

/**
 * @brief Designers use this asset to map minimap icon types to their texture and draw size.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UMinimapIconDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	const FMinimapIcon* FindMinimapIcon(const EMinimapIconType IconType) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MiniMap")
	TMap<EMinimapIconType, FMinimapIcon> M_MinimapIcons;
};
