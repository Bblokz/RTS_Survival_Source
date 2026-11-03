// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GlobalAbilityVisionBubble.generated.h"

class UFowComp;

/**
 * @brief An invisible fog of war vision bubble left behind by a global ability, gone after its time. Spawned from code
 * only: the player's bubbles light up the fog, the AI's bubbles give the AI vision.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AGlobalAbilityVisionBubble : public AActor
{
	GENERATED_BODY()

public:
	AGlobalAbilityVisionBubble();

	/**
	 * @brief Starts the vision; call once right after spawning.
	 * @param VisionRadius Radius of the bubble in cm.
	 * @param Seconds How long the bubble stays.
	 * @param OwningPlayer Player of the ability.
	 */
	void StartVision(const float VisionRadius, const float Seconds, const uint8 OwningPlayer);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<UFowComp> M_FowComponent;
};
