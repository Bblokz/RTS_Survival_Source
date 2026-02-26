// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "AntiTankGun.generated.h"

UCLASS()
class RTS_SURVIVAL_API AAntiTankGun : public ATeamWeapon
{
	GENERATED_BODY()

public:
	AAntiTankGun();

protected:
	virtual void BeginPlay() override;

};
