// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/GunPit/GunPitCrewComponent.h"
#include "B4HowitzerPitCrewComponent.generated.h"

/**
 * @brief The B-4 pit's crew component: the generic gun pit drill with the B-4's own tags. The gun plays its crane
 * reload montage and the four crew their drills; crew meshes are tagged "B4PitCrew", props that ride on the gun
 * "B4PitSocket:<GunSocket>", and the crew ride on the gun's PitOrigin socket.
 * @note InitPitCrew: call in the turret blueprint's BeginPlay with the gun mesh.
 * @note OnReloadStarted: call from the turret blueprint's ReloadWeapon event.
 * @note OnGunFired: call from the turret blueprint's PlaySingleFireAnimation.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UB4HowitzerPitCrewComponent : public UGunPitCrewComponent
{
	GENERATED_BODY()

public:
	UB4HowitzerPitCrewComponent();
};
