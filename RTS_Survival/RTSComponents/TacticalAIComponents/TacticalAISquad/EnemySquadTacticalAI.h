// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/TacticalAIComponents/TacticalAIComp.h"
#include "EnemySquadTacticalAI.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemySquadTacticalAI : public UTacticalAIComp
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEnemySquadTacticalAI();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

};
