// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyItemSiteTypes.h"

#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"
#include "RTS_Survival/Game/RTSGameInstance/WorldMissionContext/WorldMissionContext.h"

UEnemyItemSlotContentSet::UEnemyItemSlotContentSet()
{
	// Filler is dressing for item types the slot does not serve; it should not hold up the victory.
	FillerContent.bIsSignatureTarget = false;
}

FEnemyDivisionArmySettings::FEnemyDivisionArmySettings()
{
	ConvoyTruck = FEnemyPlacementUnit::MakeVehicle(ETankSubtype::Tank_ZilAmmo);
	// Unarmed trucks under the AI's control would be sent into attacks; a convoy waits at its start.
	OrdersByDivision.Add(EWorldFieldDivisions::SovietSupplyConvoy, EEnemyDivisionArmyOrders::HoldStart);
}

EEnemyDivisionArmyOrders FEnemyDivisionArmySettings::GetOrdersForDivision(const EWorldFieldDivisions DivisionType) const
{
	const EEnemyDivisionArmyOrders* Orders = OrdersByDivision.Find(DivisionType);
	return Orders != nullptr ? *Orders : DefaultOrders;
}

int32 FEnemyItemSiteContext::GetTotalStrengthPercentage() const
{
	int32 TotalStrengthPercentage = 0;
	for (const FWorldStrengthContribution& StrengthContribution : StrengthContributions)
	{
		TotalStrengthPercentage += StrengthContribution.StrengthPercentage;
	}

	return TotalStrengthPercentage;
}

bool FEnemyItemSiteContext::GetHasFortification(const EWorldFortificationStrength Fortification) const
{
	for (const FWorldStrengthContribution& StrengthContribution : StrengthContributions)
	{
		if (StrengthContribution.FortificationModification == Fortification)
		{
			return true;
		}
	}

	return false;
}

namespace EnemyItemSiteRules
{
	namespace
	{
		bool TryGetEntryWithClass(const TMap<EMapEnemyItem, FEnemyItemSlotContent>& ContentByItem,
		                          const EMapEnemyItem ItemType,
		                          FEnemyItemSlotContent& OutContent)
		{
			const FEnemyItemSlotContent* Entry = ContentByItem.Find(ItemType);
			if (Entry == nullptr || not Entry->GetHasActorClass())
			{
				return false;
			}

			OutContent = *Entry;
			return true;
		}
	}

	bool TryResolveSlotContent(const EMapEnemyItem ItemType,
	                           const TMap<EMapEnemyItem, FEnemyItemSlotContent>& SlotContentByItem,
	                           const FEnemyItemSlotContent& SlotFiller,
	                           const UEnemyItemSlotContentSet* ContentSet,
	                           FEnemyItemSlotContent& OutContent)
	{
		if (ItemType == EMapEnemyItem::None)
		{
			return false;
		}

		if (TryGetEntryWithClass(SlotContentByItem, ItemType, OutContent))
		{
			return true;
		}

		if (IsValid(ContentSet) && TryGetEntryWithClass(ContentSet->ContentByItem, ItemType, OutContent))
		{
			return true;
		}

		if (SlotFiller.GetHasActorClass())
		{
			OutContent = SlotFiller;
			return true;
		}

		if (IsValid(ContentSet) && ContentSet->FillerContent.GetHasActorClass())
		{
			OutContent = ContentSet->FillerContent;
			return true;
		}

		return false;
	}

	bool GetShouldActivateStrengthSlot(const FEnemyItemStrengthSlotCondition& Condition,
	                                   const FEnemyItemSiteContext& SiteContext)
	{
		if (not Condition.OnlyForItems.IsEmpty() && not Condition.OnlyForItems.Contains(SiteContext.ItemType))
		{
			return false;
		}

		if (Condition.RequiredFortification != EWorldFortificationStrength::None
			&& not SiteContext.GetHasFortification(Condition.RequiredFortification))
		{
			return false;
		}

		return SiteContext.GetTotalStrengthPercentage() >= Condition.MinimumTotalStrengthPercentage;
	}

	FEnemyItemSiteContext BuildSiteContext(const FWorldMissionContext& MissionContext,
	                                       const FEnemyItemSiteContext& TestContext)
	{
		// Only a launch from the world map counts: PIE startup settings fill the context too, and the director's
		// own test values must win over those on a handmade map.
		const bool bIsEnemyOperation = MissionContext.GetIsCampaignOperation()
			&& MissionContext.GetIsValid()
			&& MissionContext.OperationType == EMapItemType::EnemyItem;
		if (not bIsEnemyOperation)
		{
			FEnemyItemSiteContext SiteContext = TestContext;
			SiteContext.bFromMissionContext = false;
			return SiteContext;
		}

		FEnemyItemSiteContext SiteContext;
		// A division arrives as an enemy item operation with a stand-in item type; the key tells them apart.
		SiteContext.bIsDivisionOperation = MissionContext.SourceDivisionKey.IsValid();
		SiteContext.ItemType = SiteContext.bIsDivisionOperation
			                       ? EMapEnemyItem::None
			                       : MissionContext.EnemyObjectType;
		SiteContext.Army = MissionContext.EnemyArmy;
		SiteContext.OperationSeed = MissionContext.OperationSeed;
		SiteContext.StrengthContributions = MissionContext.StrengthContributions;
		SiteContext.bFromMissionContext = true;
		return SiteContext;
	}

	EEnemyItemSiteObjectiveState GetObjectiveState(const bool bIsSiteBuilt,
	                                               const int32 SpawnedTargetCount,
	                                               const int32 LivingTargetCount)
	{
		if (not bIsSiteBuilt)
		{
			return EEnemyItemSiteObjectiveState::WaitingForSite;
		}

		if (SpawnedTargetCount <= 0)
		{
			return EEnemyItemSiteObjectiveState::NoTargets;
		}

		return LivingTargetCount > 0
			       ? EEnemyItemSiteObjectiveState::InProgress
			       : EEnemyItemSiteObjectiveState::AllTargetsDestroyed;
	}

	int32 BuildArmySpawnList(const FWorldMissionEnemyArmy& Army,
	                         const FEnemyPlacementUnit& ConvoyTruck,
	                         TArray<FEnemyPlacementUnit>& OutUnits)
	{
		int32 SkippedEntryCount = 0;
		for (const ETankSubtype VehicleSubtype : Army.Vehicles)
		{
			const FEnemyPlacementUnit Vehicle = FEnemyPlacementUnit::MakeVehicle(VehicleSubtype);
			if (not Vehicle.GetIsSet())
			{
				SkippedEntryCount++;
				continue;
			}

			OutUnits.Add(Vehicle);
		}

		for (const ESquadSubtype SquadSubtype : Army.Squads)
		{
			const FEnemyPlacementUnit Squad = FEnemyPlacementUnit::MakeSquad(SquadSubtype);
			if (not Squad.GetIsSet())
			{
				SkippedEntryCount++;
				continue;
			}

			OutUnits.Add(Squad);
		}

		const int32 TruckCount = FMath::Max(0, Army.ConvoyTruckCount);
		if (not ConvoyTruck.GetIsSet())
		{
			return SkippedEntryCount + TruckCount;
		}

		for (int32 TruckIndex = 0; TruckIndex < TruckCount; ++TruckIndex)
		{
			OutUnits.Add(ConvoyTruck);
		}

		return SkippedEntryCount;
	}
}
