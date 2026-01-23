// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourSquadMovement/BehaviourSquadMovement.h"

#include "SquadRetreatBehaviour.generated.h"

/**
 * @brief Behaviour applied during retreat to temporarily boost squad movement speed.
 */
UCLASS()
class RTS_SURVIVAL_API USquadRetreatBehaviour : public UBehaviourSquadMovement
{
	GENERATED_BODY()

public:
	USquadRetreatBehaviour();
};
