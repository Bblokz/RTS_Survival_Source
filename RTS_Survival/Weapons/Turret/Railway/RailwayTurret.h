// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/Turret/Embedded/EmbeddedTurretsMaster.h"
#include "RailwayTurret.generated.h"

/**
 * @brief Embedded railway turret that accepts targets only within its configured minimum and weapon maximum range.
 * Targets that cross inside the minimum range are discarded so the turret can automatically acquire another target.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ARailwayTurret : public AEmbeddedTurretsMaster
{
	GENERATED_BODY()

protected:
	virtual float GetMinimumTargetRange() const override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="RailwayTurret|Targeting",
		meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0", Units="cm"))
	float M_MinimumRange = 0.0f;
};
