// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "WorldConvoyDivision.generated.h"

class USphereComponent;

/**
 * @brief Enemy supply convoy: a cheap reaction piece the enemy turn sends toward a target. On arrival the enemy turn
 * consumes it and fortifies the target. Damage removes whole trucks. Concrete, with its own root and a
 * Visibility-blocking sphere, so it can be spawned from C++ and clicked when no Blueprint class is configured.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AWorldConvoyDivision : public AWorldDivisionBase
{
	GENERATED_BODY()

public:
	AWorldConvoyDivision();

	virtual void PostInitializeComponents() override;

	/**
	 * @brief Removes whole trucks and scales the convoy's strength by the trucks left.
	 * @param DamageEntryCount Number of trucks to remove.
	 */
	virtual void ApplyWorldDivisionDamage(int32 DamageEntryCount) override;

	virtual int32 GetRemainingEntryCount() const override { return M_RemainingTruckCount; }

protected:
	/**
	 * @brief Rebuilds the truck count from saved strength; the save data has no convoy roster.
	 * @param DivisionSaveData Save data loaded for this convoy.
	 */
	virtual void RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> M_ConvoyCollision = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="World Campaign|World Divisions",
		meta=(AllowPrivateAccess="true", ClampMin="1"))
	int32 M_StartingTruckCount = 4;

	int32 M_RemainingTruckCount = 0;
};
