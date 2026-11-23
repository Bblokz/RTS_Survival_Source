// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitRules.h"

#define LOCTEXT_NAMESPACE "WorldCommanderMapKitRules"

namespace WorldCommanderMapKitRulesPrivate
{
	bool GetIsBuildingMatch(const EWorldCampaignBuilding PassiveBuilding, const EWorldCampaignBuilding Building)
	{
		return PassiveBuilding == EWorldCampaignBuilding::None || PassiveBuilding == Building;
	}

	bool GetDoesRewardPassiveApply(const FWorldCommanderPassiveDefinition& Passive,
	                               const EWorldOperationTarget Target, const bool bIsStoryMission)
	{
		if (Passive.Type != EWorldCommanderPassive::OperationRewardMultiplier)
		{
			return false;
		}
		if (Passive.OperationTarget != EWorldOperationTarget::None && Passive.OperationTarget != Target)
		{
			return false;
		}
		return not(Passive.bExcludeStoryMissions && bIsStoryMission);
	}
}

EWorldCommanderAbilityTargetKind WorldCommanderMapKitRules::GetRequiredTargetKind(
	const EWorldCommanderMapAbility Ability)
{
	switch (Ability)
	{
	case EWorldCommanderMapAbility::WeakenTarget:
	case EWorldCommanderMapAbility::RemoveFortification:
	case EWorldCommanderMapAbility::RevealTargetStrength:
	case EWorldCommanderMapAbility::StallStrengthGrowth:
		return EWorldCommanderAbilityTargetKind::Anchor;
	case EWorldCommanderMapAbility::RevealRegion:
		return EWorldCommanderAbilityTargetKind::WorldLocation;
	case EWorldCommanderMapAbility::DamageDivision:
		return EWorldCommanderAbilityTargetKind::Division;
	case EWorldCommanderMapAbility::ProtectBuilding:
		return EWorldCommanderAbilityTargetKind::BuildNode;
	case EWorldCommanderMapAbility::None:
	case EWorldCommanderMapAbility::ConvertRadixiteToBlueprints:
	case EWorldCommanderMapAbility::GrantResources:
	case EWorldCommanderMapAbility::ForcedMarch:
	case EWorldCommanderMapAbility::ReinforceNextMission:
	case EWorldCommanderMapAbility::StallFreeze:
		return EWorldCommanderAbilityTargetKind::None;
	}
	return EWorldCommanderAbilityTargetKind::None;
}

bool WorldCommanderMapKitRules::GetIsTargetValidForAbility(const EWorldCommanderMapAbility Ability,
                                                           const FWorldCommanderMapAbilityTarget& Target)
{
	switch (GetRequiredTargetKind(Ability))
	{
	case EWorldCommanderAbilityTargetKind::None:
		return true;
	case EWorldCommanderAbilityTargetKind::Anchor:
		return Target.AnchorKey.IsValid();
	case EWorldCommanderAbilityTargetKind::Division:
		return Target.DivisionKey.IsValid();
	case EWorldCommanderAbilityTargetKind::BuildNode:
		return not Target.BuildNodeId.IsNone();
	case EWorldCommanderAbilityTargetKind::WorldLocation:
		return Target.bHasWorldLocation;
	}
	return false;
}

FWorldCommanderConversion WorldCommanderMapKitRules::GetConversion(const FWorldCommanderMapAbilityDefinition& Ability,
                                                                   const int32 RadixiteHeld)
{
	FWorldCommanderConversion Conversion;
	const int32 RadixitePerBlueprint = FMath::CeilToInt(Ability.Magnitude);
	if (RadixitePerBlueprint <= 0 || Ability.Resources.Blueprints <= 0 || RadixiteHeld <= 0)
	{
		return Conversion;
	}

	Conversion.BlueprintsGained = FMath::Min(RadixiteHeld / RadixitePerBlueprint, Ability.Resources.Blueprints);
	Conversion.RadixiteSpent = Conversion.BlueprintsGained * RadixitePerBlueprint;
	return Conversion;
}

EWorldCommanderAbilityBlock WorldCommanderMapKitRules::GetAbilityBlock(const FWorldCommanderMapState& MapState,
                                                                       const FWorldCommanderMapKitDefinition* Kit,
                                                                       const FWorldCommanderMapAbilityTarget& Target,
                                                                       const FWorldMapResources& Resources)
{
	if (MapState.Commander == ERTSCommander::NoCommander)
	{
		return EWorldCommanderAbilityBlock::NoCommander;
	}
	if (Kit == nullptr || Kit->Ability.Type == EWorldCommanderMapAbility::None)
	{
		return EWorldCommanderAbilityBlock::NoAbility;
	}
	if (MapState.MapAbilityCooldownTurnsRemaining > 0)
	{
		return EWorldCommanderAbilityBlock::OnCooldown;
	}
	if (not GetIsTargetValidForAbility(Kit->Ability.Type, Target))
	{
		return EWorldCommanderAbilityBlock::MissingTarget;
	}
	const bool bIsConversion = Kit->Ability.Type == EWorldCommanderMapAbility::ConvertRadixiteToBlueprints;
	if (bIsConversion && GetConversion(Kit->Ability, Resources.Radixite).BlueprintsGained <= 0)
	{
		return EWorldCommanderAbilityBlock::CannotAfford;
	}
	return EWorldCommanderAbilityBlock::None;
}

FText WorldCommanderMapKitRules::GetAbilityBlockText(const EWorldCommanderAbilityBlock Block)
{
	switch (Block)
	{
	case EWorldCommanderAbilityBlock::None:
		return FText::GetEmpty();
	case EWorldCommanderAbilityBlock::NoCommander:
		return LOCTEXT("NoCommander", "No commander leads this campaign.");
	case EWorldCommanderAbilityBlock::NoAbility:
		return LOCTEXT("NoAbility", "This commander has no map ability.");
	case EWorldCommanderAbilityBlock::OnCooldown:
		return LOCTEXT("OnCooldown", "The map ability is not ready yet.");
	case EWorldCommanderAbilityBlock::MissingTarget:
		return LOCTEXT("MissingTarget", "Choose a valid target for the map ability.");
	case EWorldCommanderAbilityBlock::CannotAfford:
		return LOCTEXT("CannotAfford", "Not enough radixite.");
	}
	return FText::GetEmpty();
}

void WorldCommanderMapKitRules::RecordAbilityUse(FWorldCommanderMapState& MapState,
                                                 const FWorldCommanderMapAbilityDefinition& Ability,
                                                 const FWorldCommanderMapAbilityTarget& Target)
{
	MapState.MapAbilityCooldownTurnsRemaining = FMath::Max(0, Ability.CooldownTurns);
	MapState.MapAbilityUses++;
	if (Ability.DurationTurns <= 0)
	{
		return;
	}

	FWorldCommanderTimedEffect TimedEffect;
	TimedEffect.Ability = Ability.Type;
	TimedEffect.Target = Target;
	TimedEffect.Magnitude = Ability.Magnitude;
	TimedEffect.Radius = Ability.Radius;
	TimedEffect.TurnsRemaining = Ability.DurationTurns;
	MapState.ActiveTimedEffects.Add(TimedEffect);
}

TArray<FWorldCommanderTimedEffect> WorldCommanderMapKitRules::TickStartOfPlayerTurn(FWorldCommanderMapState& MapState)
{
	MapState.MapAbilityCooldownTurnsRemaining = FMath::Max(0, MapState.MapAbilityCooldownTurnsRemaining - 1);

	TArray<FWorldCommanderTimedEffect> ExpiredEffects;
	for (int32 EffectIndex = MapState.ActiveTimedEffects.Num() - 1; EffectIndex >= 0; --EffectIndex)
	{
		FWorldCommanderTimedEffect& TimedEffect = MapState.ActiveTimedEffects[EffectIndex];
		TimedEffect.TurnsRemaining--;
		if (TimedEffect.TurnsRemaining > 0)
		{
			continue;
		}
		ExpiredEffects.Add(TimedEffect);
		MapState.ActiveTimedEffects.RemoveAt(EffectIndex);
	}
	return ExpiredEffects;
}

float WorldCommanderMapKitRules::GetPassiveBuildingValue(const FWorldCommanderPassiveDefinition& Passive,
                                                         const EWorldCommanderPassive Type,
                                                         const EWorldCampaignBuilding Building, const float Neutral)
{
	if (Passive.Type != Type || not WorldCommanderMapKitRulesPrivate::GetIsBuildingMatch(Passive.Building, Building))
	{
		return Neutral;
	}
	return Passive.Magnitude;
}

FWorldMapResources WorldCommanderMapKitRules::ScaleResources(const FWorldMapResources& Resources,
                                                             const float Multiplier,
                                                             const EWorldMapResourceScope Scope)
{
	const float SafeMultiplier = FMath::Max(0.f, Multiplier);
	FWorldMapResources Scaled = Resources;
	if (Scope != EWorldMapResourceScope::BlueprintsOnly)
	{
		Scaled.Radixite = FMath::RoundToInt(Resources.Radixite * SafeMultiplier);
	}
	if (Scope != EWorldMapResourceScope::RadixiteOnly)
	{
		Scaled.Blueprints = FMath::RoundToInt(Resources.Blueprints * SafeMultiplier);
	}
	return Scaled;
}

FWorldMapResources WorldCommanderMapKitRules::GetScaledOperationReward(
	const FWorldCommanderPassiveDefinition& Passive, const FWorldMapResources& BaseReward,
	const EWorldOperationTarget Target, const bool bIsStoryMission)
{
	if (not WorldCommanderMapKitRulesPrivate::GetDoesRewardPassiveApply(Passive, Target, bIsStoryMission))
	{
		return BaseReward;
	}
	return ScaleResources(BaseReward, Passive.Magnitude, Passive.ResourceScope);
}

#undef LOCTEXT_NAMESPACE
