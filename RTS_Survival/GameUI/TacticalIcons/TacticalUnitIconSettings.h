// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "TacticalUnitIconSettings.generated.h"

class UTexture2D;

/**
 * @brief What one tactical glass plate shows: the icon behind the glass and the colour of the light inside it.
 * A plate without an icon texture is hidden.
 */
USTRUCT(BlueprintType)
struct FTacticalUnitIconSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TacticalIcon")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	// Lights the glass from behind and tints its edges; linear colour (the light blue of the glass panes by default).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TacticalIcon")
	FLinearColor LightColor = FLinearColor(0.55f, 0.88f, 1.0f, 1.0f);
};
