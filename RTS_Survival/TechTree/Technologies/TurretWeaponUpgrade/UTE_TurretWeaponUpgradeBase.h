#pragma once
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"

#include "UTE_TurretWeaponUpgradeBase.generated.h"

/**
 * @brief Base for weapon upgrade technologies that now rely on configured behaviours to alter unit weapons.
 * The behaviour can inspect mounted weapons/calibres, so this tech only selects eligible units and adds behaviour.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_TurretWeaponUpgradeBase : public UTechnologyEffect
{
	GENERATED_BODY()
};
