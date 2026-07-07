// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FPlayerTurnContext.generated.h"

/**
 * @brief Passed to Blueprint when the player starts a world campaign turn.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FPlayerTurnContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "World Campaign|Turns")
	int32 CurrentTurn = 0;
};
