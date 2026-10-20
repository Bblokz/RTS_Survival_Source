// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_ArmoredTroopBay.generated.h"

class ATankMaster;

/**
 * @brief Thickens the hull plates of the SdKfz 250/251 half-tracks and lets their passengers bail out
 * (keeping half their health) when the half-track is destroyed instead of dying with it.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_ArmoredTroopBay : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_ArmoredTroopBay();

protected:
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Troop Bay")
	float M_HullArmorMultiplier;

	// Share of their current health passengers keep when bailing out of a destroyed half-track.
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Troop Bay")
	float M_PassengerHealthFractionKept;
};
