// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"

/**
 * Impact effects are sized by the weapon's ImpactScale, applied as the Niagara component scale. Positions and
 * velocities of local-space emitters follow that scale, but sprite sizes do not: the howitzer impacts' stateless
 * volume layers (Tools/ImpactVfx/make_recipe.py) multiply their size curve by the User.ImpactSpriteScale parameter
 * instead, so every caller that scales an impact also sets that parameter. Systems without it ignore the value.
 */
namespace ImpactVfxSpriteScale
{
	const FName UserParameterName = TEXT("ImpactSpriteScale");

	inline void ApplyImpactScale(UNiagaraComponent& NiagaraComponent, const FVector& ImpactScale)
	{
		NiagaraComponent.SetWorldScale3D(ImpactScale);
		const double UniformScale = ImpactScale.GetAbsMax();
		NiagaraComponent.SetVariableVec2(UserParameterName, FVector2D(UniformScale, UniformScale));
	}
}
