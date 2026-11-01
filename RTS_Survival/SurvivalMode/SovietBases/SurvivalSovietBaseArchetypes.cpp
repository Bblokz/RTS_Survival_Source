// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseMasterPlan.h"

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePlanPen.h"

ESovietGatePreference GetSovietGatePreference(const ESurvivalSovietBaseArchetype Archetype, FRandomStream& Random)
{
	switch (Archetype)
	{
	case ESurvivalSovietBaseArchetype::Firebase:
	case ESurvivalSovietBaseArchetype::FortifiedDepot:
		// Supplied from the rear, walls towards the enemy.
		return ESovietGatePreference::AwayFromThreat;
	case ESurvivalSovietBaseArchetype::TankPark:
		// Tanks sortie straight at the enemy.
		return ESovietGatePreference::TowardThreat;
	case ESurvivalSovietBaseArchetype::Airstrip:
		// The strip runs towards the enemy's flank, so an attack cannot rake it end to end.
		return ESovietGatePreference::TowardThreat;
	case ESurvivalSovietBaseArchetype::CommandFortress:
	case ESurvivalSovietBaseArchetype::Cantonment:
	case ESurvivalSovietBaseArchetype::HillStronghold:
	default:
		return Random.FRand() < 0.5f ? ESovietGatePreference::FlankThreat : ESovietGatePreference::AwayFromThreat;
	}
}

FSovietMasterPlan BuildSovietMasterPlan(
	const ESurvivalSovietBaseArchetype Archetype,
	const int32 Strength,
	const double SizeFill,
	FRandomStream& Random,
	const FSurvivalSovietSiteMap* Site,
	const TFunction<FVector2D(const FVector2D&)>& ToLocal,
	const TFunction<FVector2D(const FVector2D&)>& ToWorld,
	const TArray<FVector2D>& DefensePointsLocal)
{
	FSovietMasterPlan Plan;
	const FSovietPlanPen Pen{Plan, Random, FMath::Clamp(Strength, 0, SovietBaseArchetypeConstants::MaxPlanStrength), SizeFill};
	switch (Archetype)
	{
	case ESurvivalSovietBaseArchetype::Cantonment:
		SovietPlan_RifleCantonment(Pen);
		break;
	case ESurvivalSovietBaseArchetype::TankPark:
		SovietPlan_TankPark(Pen);
		break;
	case ESurvivalSovietBaseArchetype::FortifiedDepot:
		SovietPlan_SupplyDepot(Pen);
		break;
	case ESurvivalSovietBaseArchetype::CommandFortress:
		SovietPlan_CommandFortress(Pen);
		break;
	case ESurvivalSovietBaseArchetype::HillStronghold:
		SovietPlan_HillStronghold(Pen, Site, ToLocal, ToWorld, DefensePointsLocal);
		break;
	case ESurvivalSovietBaseArchetype::Airstrip:
		SovietPlan_FieldAirstrip(Pen);
		break;
	case ESurvivalSovietBaseArchetype::Firebase:
	default:
		SovietPlan_ForwardFirebase(Pen);
		break;
	}
	return Plan;
}
