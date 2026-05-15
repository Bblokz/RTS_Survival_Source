// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MutatorSettings.h"

#include "RTS_Survival/Behaviours/Behaviour.h"

namespace MutatorSettingsConstants
{
	const TArray<TSubclassOf<UBehaviour>> EmptyMutators;
}

UMutatorSettings::UMutatorSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Mutators");
}

const TArray<TSubclassOf<UBehaviour>>& UMutatorSettings::GetMutatorsForClass(const EMutatorClass MutatorClass) const
{
	switch (MutatorClass)
	{
	case EMutatorClass::Squad:
		return Mutators.SquadMutators;
	case EMutatorClass::ArmoredCar:
		return Mutators.ArmoredCarMutators;
	case EMutatorClass::LightTank:
		return Mutators.LightTankMutators;
	case EMutatorClass::MediumTank:
		return Mutators.MediumTankMutators;
	case EMutatorClass::HeavyTank:
		return Mutators.HeavyTankMutators;
	case EMutatorClass::TankDestroyer:
		return Mutators.TankDestroyerMutators;
	default:
		return MutatorSettingsConstants::EmptyMutators;
	}
}
