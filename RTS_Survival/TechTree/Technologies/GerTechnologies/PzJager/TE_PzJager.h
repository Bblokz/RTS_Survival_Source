// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_PzJager.generated.h"

class ATankMaster;

/**
 * @brief PzJager technology hook; designers can add subtype targets and behaviour in the data asset.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_PzJager : public UTechnologyEffect
{
	GENERATED_BODY()

protected:
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;
};
