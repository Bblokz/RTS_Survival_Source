// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "Mortar.generated.h"

UCLASS()
class RTS_SURVIVAL_API AMortar : public ATeamWeapon
{
	GENERATED_BODY()

public:
	AMortar();

protected:
	virtual void BeginPlay() override;

};
