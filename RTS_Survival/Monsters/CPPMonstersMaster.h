// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpCharacterObjectsMaster.h"

#include "CPPMonstersMaster.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API ACPPMonstersMaster : public ACharacterObjectsMaster
{
	GENERATED_BODY()

	ACPPMonstersMaster(const FObjectInitializer& ObjectInitializer);

	virtual void UnitDies(const ERTSDeathType DeathType) override;
};
