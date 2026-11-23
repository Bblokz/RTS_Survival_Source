// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKit.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitRules.h"

#define LOCTEXT_NAMESPACE "WorldCommanderMapKit"

namespace WorldCommanderMapKitNeutral
{
	constexpr float Multiplier = 1.f;
	constexpr float Range = 0.f;
}

UWorldCommanderMapKit::UWorldCommanderMapKit()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldCommanderMapKit::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	M_ProgressManager = Owner->FindComponentByClass<UWorldCampaignProgressManager>();
}

ERTSCommander UWorldCommanderMapKit::GetCommander() const
{
	if (not GetIsValidProgressManager())
	{
		return ERTSCommander::NoCommander;
	}
	return M_ProgressManager->GetProgressState().CommanderMap.Commander;
}

bool UWorldCommanderMapKit::GetCommanderKit(FWorldCommanderMapKitDefinition& OutKit) const
{
	const FWorldCommanderMapKitDefinition* Kit = FindCommanderKit();
	if (Kit == nullptr)
	{
		return false;
	}
	OutKit = *Kit;
	return true;
}

FWorldMapResources UWorldCommanderMapKit::GetPassiveScaledBuildingCost(const EWorldCampaignBuilding Building,
                                                                       const FWorldMapResources& BaseCost) const
{
	const float Multiplier = WorldCommanderMapKitRules::GetPassiveBuildingValue(
		GetCommanderPassive(), EWorldCommanderPassive::BuildingCostMultiplier, Building,
		WorldCommanderMapKitNeutral::Multiplier);
	return WorldCommanderMapKitRules::ScaleResources(BaseCost, Multiplier, EWorldMapResourceScope::Both);
}

float UWorldCommanderMapKit::GetPassiveBuildingRangeMultiplier(const EWorldCampaignBuilding Building) const
{
	return WorldCommanderMapKitRules::GetPassiveBuildingValue(
		GetCommanderPassive(), EWorldCommanderPassive::BuildingRangeMultiplier, Building,
		WorldCommanderMapKitNeutral::Multiplier);
}

float UWorldCommanderMapKit::GetPassiveStrengthReductionMultiplier(const EWorldCampaignBuilding Building) const
{
	return WorldCommanderMapKitRules::GetPassiveBuildingValue(
		GetCommanderPassive(), EWorldCommanderPassive::BuildingStrengthReductionMultiplier, Building,
		WorldCommanderMapKitNeutral::Multiplier);
}

FWorldMapResources UWorldCommanderMapKit::GetPassiveScaledBuildingIncome(const EWorldCampaignBuilding Building,
                                                                         const FWorldMapResources& BaseIncome) const
{
	const FWorldCommanderPassiveDefinition Passive = GetCommanderPassive();
	const float Multiplier = WorldCommanderMapKitRules::GetPassiveBuildingValue(
		Passive, EWorldCommanderPassive::BuildingIncomeMultiplier, Building, WorldCommanderMapKitNeutral::Multiplier);
	return WorldCommanderMapKitRules::ScaleResources(BaseIncome, Multiplier, Passive.ResourceScope);
}

float UWorldCommanderMapKit::GetPassiveMissionSupportRange(const EWorldCampaignBuilding Building) const
{
	return WorldCommanderMapKitRules::GetPassiveBuildingValue(
		GetCommanderPassive(), EWorldCommanderPassive::BuildingSupportsMissionsInRange, Building,
		WorldCommanderMapKitNeutral::Range);
}

FWorldMapResources UWorldCommanderMapKit::GetPassiveScaledOperationReward(const FWorldMapResources& BaseReward,
                                                                          const EWorldOperationTarget Target,
                                                                          const bool bIsStoryMission) const
{
	return WorldCommanderMapKitRules::GetScaledOperationReward(GetCommanderPassive(), BaseReward, Target,
	                                                           bIsStoryMission);
}

float UWorldCommanderMapKit::GetPassiveEnemyItemStrengthMultiplier(const EMapEnemyItem EnemyItem) const
{
	const FWorldCommanderPassiveDefinition Passive = GetCommanderPassive();
	if (Passive.Type != EWorldCommanderPassive::EnemyItemStrengthMultiplier)
	{
		return WorldCommanderMapKitNeutral::Multiplier;
	}
	if (Passive.EnemyItem != EMapEnemyItem::None && Passive.EnemyItem != EnemyItem)
	{
		return WorldCommanderMapKitNeutral::Multiplier;
	}
	return Passive.Magnitude;
}

int32 UWorldCommanderMapKit::GetPassiveIgnoredFortificationCount() const
{
	const FWorldCommanderPassiveDefinition Passive = GetCommanderPassive();
	if (Passive.Type != EWorldCommanderPassive::IgnoreFortifications)
	{
		return 0;
	}
	return FMath::Max(0, FMath::RoundToInt(Passive.Magnitude));
}

int32 UWorldCommanderMapKit::GetPassiveExtraBuildNodesPerCountry() const
{
	const FWorldCommanderPassiveDefinition Passive = GetCommanderPassive();
	if (Passive.Type != EWorldCommanderPassive::ExtraBuildNodesPerCountry)
	{
		return 0;
	}
	return FMath::Max(0, FMath::RoundToInt(Passive.Magnitude));
}

bool UWorldCommanderMapKit::GetPassiveBuildingsFinishInstantly() const
{
	return GetCommanderPassive().Type == EWorldCommanderPassive::InstantBuildings;
}

bool UWorldCommanderMapKit::TryUseMapAbility(const FWorldCommanderMapAbilityTarget& Target, FText& OutFailReason)
{
	if (not GetCanUseMapAbility(Target, OutFailReason))
	{
		return false;
	}

	// Copied: the effect below broadcasts into other systems, which may change the progress state.
	const FWorldCommanderMapAbilityDefinition Ability = FindCommanderKit()->Ability;
	// Cooldown and timed effect are stored first, so a listener of the effect request already sees them.
	WorldCommanderMapKitRules::RecordAbilityUse(M_ProgressManager->GetMutableProgressState().CommanderMap, Ability,
	                                            Target);
	ApplyMapAbilityEffect(Ability, Target);

	OnMapAbilityUsed.Broadcast(Ability.Type, Target);
	OnMapAbilityCooldownChanged.Broadcast(GetMapAbilityCooldownTurnsRemaining());
	return true;
}

bool UWorldCommanderMapKit::GetCanUseMapAbility(const FWorldCommanderMapAbilityTarget& Target,
                                                FText& OutFailReason) const
{
	OutFailReason = FText::GetEmpty();
	if (not GetIsValidProgressManager())
	{
		OutFailReason = LOCTEXT("NoCampaign", "No campaign is running.");
		return false;
	}

	const FWorldCampaignProgressState& ProgressState = M_ProgressManager->GetProgressState();
	const EWorldCommanderAbilityBlock Block = WorldCommanderMapKitRules::GetAbilityBlock(
		ProgressState.CommanderMap, FindCommanderKit(), Target, ProgressState.Resources);
	if (Block == EWorldCommanderAbilityBlock::None)
	{
		return true;
	}
	OutFailReason = WorldCommanderMapKitRules::GetAbilityBlockText(Block);
	return false;
}

EWorldCommanderAbilityTargetKind UWorldCommanderMapKit::GetMapAbilityTargetKind() const
{
	const FWorldCommanderMapKitDefinition* Kit = FindCommanderKit();
	if (Kit == nullptr)
	{
		return EWorldCommanderAbilityTargetKind::None;
	}
	return WorldCommanderMapKitRules::GetRequiredTargetKind(Kit->Ability.Type);
}

int32 UWorldCommanderMapKit::GetMapAbilityCooldownTurnsRemaining() const
{
	if (not GetIsValidProgressManager())
	{
		return 0;
	}
	return M_ProgressManager->GetProgressState().CommanderMap.MapAbilityCooldownTurnsRemaining;
}

TArray<FWorldCommanderTimedEffect> UWorldCommanderMapKit::GetActiveTimedEffects() const
{
	if (not GetIsValidProgressManager())
	{
		return TArray<FWorldCommanderTimedEffect>();
	}
	return M_ProgressManager->GetProgressState().CommanderMap.ActiveTimedEffects;
}

void UWorldCommanderMapKit::TickCooldownAtStartOfPlayerTurn()
{
	if (not GetIsValidProgressManager())
	{
		return;
	}

	FWorldCommanderMapState& MapState = M_ProgressManager->GetMutableProgressState().CommanderMap;
	const int32 CooldownBefore = MapState.MapAbilityCooldownTurnsRemaining;
	const TArray<FWorldCommanderTimedEffect> ExpiredEffects = WorldCommanderMapKitRules::TickStartOfPlayerTurn(
		MapState);
	const int32 CooldownAfter = MapState.MapAbilityCooldownTurnsRemaining;

	// State is final before anything is broadcast, so a listener sees the new turn's values.
	for (const FWorldCommanderTimedEffect& ExpiredEffect : ExpiredEffects)
	{
		OnTimedEffectExpired.Broadcast(ExpiredEffect);
	}
	if (CooldownAfter != CooldownBefore)
	{
		OnMapAbilityCooldownChanged.Broadcast(CooldownAfter);
	}
}

bool UWorldCommanderMapKit::GetIsValidProgressManager() const
{
	if (M_ProgressManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ProgressManager",
		"GetIsValidProgressManager",
		GetOwner()
	);
	return false;
}

const FWorldCommanderMapKitDefinition* UWorldCommanderMapKit::FindCommanderKit() const
{
	if (not GetIsValidProgressManager())
	{
		return nullptr;
	}
	const ERTSCommander Commander = M_ProgressManager->GetProgressState().CommanderMap.Commander;
	return UWorldCommanderMapKitSettings::GetKitTable().FindDefinition(Commander);
}

FWorldCommanderPassiveDefinition UWorldCommanderMapKit::GetCommanderPassive() const
{
	const FWorldCommanderMapKitDefinition* Kit = FindCommanderKit();
	if (Kit == nullptr)
	{
		return FWorldCommanderPassiveDefinition();
	}
	return Kit->Passive;
}

void UWorldCommanderMapKit::ApplyMapAbilityEffect(const FWorldCommanderMapAbilityDefinition& Ability,
                                                  const FWorldCommanderMapAbilityTarget& Target)
{
	const int32 Amount = FMath::RoundToInt(Ability.Magnitude);
	switch (Ability.Type)
	{
	case EWorldCommanderMapAbility::None:
		return;
	case EWorldCommanderMapAbility::WeakenTarget:
		OnWeakenTargetRequested.Broadcast(Target.AnchorKey, Amount, Ability.DurationTurns);
		return;
	case EWorldCommanderMapAbility::RevealRegion:
		RequestRevealRegion(Ability, Target);
		return;
	case EWorldCommanderMapAbility::RemoveFortification:
		OnRemoveFortificationRequested.Broadcast(Target.AnchorKey, Amount);
		return;
	case EWorldCommanderMapAbility::ConvertRadixiteToBlueprints:
		ApplyConvertRadixiteToBlueprints(Ability);
		return;
	case EWorldCommanderMapAbility::GrantResources:
		ApplyGrantResources(Ability);
		return;
	case EWorldCommanderMapAbility::DamageDivision:
		OnDamageDivisionRequested.Broadcast(Target.DivisionKey, Amount);
		return;
	case EWorldCommanderMapAbility::RevealTargetStrength:
		OnRevealTargetStrengthRequested.Broadcast(Target.AnchorKey, Ability.DurationTurns);
		return;
	case EWorldCommanderMapAbility::StallStrengthGrowth:
		OnStallStrengthGrowthRequested.Broadcast(Target.AnchorKey, Ability.Radius, Ability.DurationTurns);
		return;
	case EWorldCommanderMapAbility::ForcedMarch:
		OnForcedMarchRequested.Broadcast(Amount, Ability.DurationTurns);
		return;
	case EWorldCommanderMapAbility::ProtectBuilding:
		OnProtectBuildingRequested.Broadcast(Target.BuildNodeId, Ability.DurationTurns);
		return;
	case EWorldCommanderMapAbility::ReinforceNextMission:
		OnReinforceNextMissionRequested.Broadcast(Amount);
		return;
	case EWorldCommanderMapAbility::StallFreeze:
		OnStallFreezeRequested.Broadcast(Amount);
		return;
	}
}

void UWorldCommanderMapKit::ApplyConvertRadixiteToBlueprints(const FWorldCommanderMapAbilityDefinition& Ability)
{
	const FWorldCommanderConversion Conversion = WorldCommanderMapKitRules::GetConversion(
		Ability, M_ProgressManager->GetResources().Radixite);
	FWorldMapResources RadixiteCost;
	RadixiteCost.Radixite = Conversion.RadixiteSpent;
	if (Conversion.BlueprintsGained <= 0 || not M_ProgressManager->TrySpendResources(RadixiteCost))
	{
		return;
	}

	FWorldMapResources BlueprintsGained;
	BlueprintsGained.Blueprints = Conversion.BlueprintsGained;
	M_ProgressManager->AddResources(BlueprintsGained);
}

void UWorldCommanderMapKit::ApplyGrantResources(const FWorldCommanderMapAbilityDefinition& Ability)
{
	FWorldMapResources Granted;
	Granted.Radixite = FMath::Max(0, Ability.Resources.Radixite);
	Granted.Blueprints = FMath::Max(0, Ability.Resources.Blueprints);
	M_ProgressManager->AddResources(Granted);
}

void UWorldCommanderMapKit::RequestRevealRegion(const FWorldCommanderMapAbilityDefinition& Ability,
                                                const FWorldCommanderMapAbilityTarget& Target) const
{
	OnRevealRegionRequested.Broadcast(Target.WorldLocation, Ability.Radius, Ability.DurationTurns);
}

#undef LOCTEXT_NAMESPACE
