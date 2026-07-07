// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldPlayerAudioController.generated.h"

class USoundBase;

/**
 * @brief Used by the world player controller to play campaign-map UI sounds.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldPlayerAudioController : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldPlayerAudioController();

	UFUNCTION(BlueprintCallable, Category = "World Campaign|Audio")
	void PlayUISound(USoundBase* Sound) const;
};
