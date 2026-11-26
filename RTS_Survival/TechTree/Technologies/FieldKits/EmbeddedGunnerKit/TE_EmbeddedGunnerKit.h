// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_EmbeddedGunnerKit.generated.h"

class ATankMaster;
class UEmbeddedTurretMountComponent;

/**
 * @brief Field modification kit that puts an infantry gunner in the hatch of every eligible tank: researched in a
 * building expansion during a mission, never on the campaign tech tree. The tank Blueprints carry the gunner as a
 * UEmbeddedTurretMountComponent whose RequiredTechnology is this kit; this effect only tells those mounts to mount.
 * Mounts gated by UnmountOnTechnology (a variant's own gunner the kit replaces) are removed first.
 * The constructor sets the MG 42 hatch gunner kit; a Blueprint child can set another kit technology and tank list.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_EmbeddedGunnerKit : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_EmbeddedGunnerKit();

protected:
	virtual void ApplyOnTank_Internal(ATankMaster* Tank) override;

private:
	/** @return How many mounts of the tank were asked to mount the kit's gunner. */
	int32 MountKitGunners(const TArray<UEmbeddedTurretMountComponent*>& Mounts) const;
	void UnmountReplacedGunners(const TArray<UEmbeddedTurretMountComponent*>& Mounts) const;
};
