// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalWaveTypes.h"

#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyTrainingHelpers/EnemyTrainingHelpers.h"

FTrainingOption FSurvivalWaveUnitEntry::GetTrainingOption() const
{
	if (Kind == ESurvivalWaveUnitKind::Tank)
	{
		return FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtype));
	}
	return FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
}

int32 FSurvivalWaveUnitEntry::GetCost() const
{
	if (CostOverride > 0)
	{
		return CostOverride;
	}
	if (Kind == ESurvivalWaveUnitKind::Tank)
	{
		return EnemyTrainingHelpers::GetTankTrainingPointsCost(TankSubtype);
	}
	return EnemyTrainingHelpers::GetSquadTrainingPointCost(SquadSubtype);
}

bool FSurvivalWaveUnitEntry::GetIsValidEntry() const
{
	if (Weight <= 0.f)
	{
		return false;
	}
	if (Kind == ESurvivalWaveUnitKind::Tank)
	{
		return TankSubtype != ETankSubtype::Tank_None;
	}
	return SquadSubtype != ESquadSubtype::Squad_None;
}

FString FSurvivalWaveUnitEntry::GetDebugName() const
{
	if (Kind == ESurvivalWaveUnitKind::Tank)
	{
		return UEnum::GetValueAsString(TankSubtype);
	}
	return UEnum::GetValueAsString(SquadSubtype);
}

float FSurvivalWaveKindDefinition::GetRoleMultiplier(const ESurvivalWaveUnitRole Role) const
{
	for (const FSurvivalRoleWeightMultiplier& EachMultiplier : RoleWeightMultipliers)
	{
		if (EachMultiplier.Role == Role)
		{
			return EachMultiplier.Multiplier;
		}
	}
	return DefaultRoleMultiplier;
}
