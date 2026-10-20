// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "CoreMinimal.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/FlareAimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/FlareProjectile.h"


/**
 * @brief Test-only access to the flare ability's internals (friend of AFlareProjectile and
 * UFlareAimAbilityComponent). Used by the automation tests and the -FlareAbilityTest map test; compiled out of
 * shipping builds together with them.
 */
struct FFlareAbilityTestAccess
{
	static void Tick(AFlareProjectile& Flare, const float DeltaSeconds) { Flare.Tick(DeltaSeconds); }
	static UStaticMeshComponent* GetParachute(const AFlareProjectile& Flare) { return Flare.M_ParachuteMesh; }
	static UPointLightComponent* GetLight(const AFlareProjectile& Flare) { return Flare.M_Light; }
	static UFowComp* GetFowComponent(const AFlareProjectile& Flare) { return Flare.M_FowComponent; }
	static bool GetIsGivingVision(const AFlareProjectile& Flare) { return Flare.M_FowComponent->IsActive(); }

	static AFlareProjectile* GetLastLaunchedFlare(const UFlareAimAbilityComponent& Component)
	{
		return Component.M_LastLaunchedFlare.Get();
	}

	static FFlareAimAbilitySettings& GetFlareSettings(UFlareAimAbilityComponent& Component)
	{
		return Component.M_FlareSettings;
	}

	static FAimAbilitySettings& GetAimSettings(UFlareAimAbilityComponent& Component)
	{
		return Component.M_AimAbilitySettings;
	}

	static UStaticMeshComponent* GetLauncherMesh(const UFlareAimAbilityComponent& Component)
	{
		return Component.M_LauncherMeshComponent;
	}

	static USceneComponent* FindLaunchSocketComponent(const UFlareAimAbilityComponent& Component)
	{
		return Component.FindLaunchSocketComponent();
	}
};

#endif
