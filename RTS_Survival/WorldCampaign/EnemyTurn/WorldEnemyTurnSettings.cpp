// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnSettings.h"

namespace WorldEnemyTurnSettingsDefaults
{
	constexpr int32 ConvoyFallbackStrength = 5;
	constexpr int32 LightDivisionFallbackStrength = 10;
	constexpr int32 MediumDivisionFallbackStrength = 15;
	constexpr int32 HeavyDivisionFallbackStrength = 20;
	constexpr int32 InfantryDivisionFallbackStrength = 10;

	FWorldEnemyReactionPieceSetup MakeTankSetup(const TArray<ETankSubtype>& TankSubtypes, const int32 FallbackStrength)
	{
		FWorldEnemyReactionPieceSetup PieceSetup;
		PieceSetup.TankSubtypes = TankSubtypes;
		PieceSetup.FallbackMaxStrengthPercentage = FallbackStrength;
		return PieceSetup;
	}

	FWorldEnemyReactionPieceSetup MakeSquadSetup(const TArray<ESquadSubtype>& SquadSubtypes,
	                                             const int32 FallbackStrength)
	{
		FWorldEnemyReactionPieceSetup PieceSetup;
		PieceSetup.SquadSubtypes = SquadSubtypes;
		PieceSetup.FallbackMaxStrengthPercentage = FallbackStrength;
		return PieceSetup;
	}
}

FWorldEnemyTurnNumbers::FWorldEnemyTurnNumbers()
{
	namespace Defaults = WorldEnemyTurnSettingsDefaults;
	// Only the medium armour division has a Blueprint class today; add the others once theirs exist.
	ReactionDivisionTypes = {EWorldFieldDivisions::SovietMediumArmorDivision};
	ReactionSourceItems = {EMapEnemyItem::Factory, EMapEnemyItem::Barracks, EMapEnemyItem::SupplyDepo};

	PieceSetups.Add(EWorldFieldDivisions::SovietLightArmorDivision, Defaults::MakeTankSetup(
		                {ETankSubtype::Tank_BT7, ETankSubtype::Tank_BT7, ETankSubtype::Tank_T26, ETankSubtype::Tank_T26},
		                Defaults::LightDivisionFallbackStrength));
	PieceSetups.Add(EWorldFieldDivisions::SovietMediumArmorDivision, Defaults::MakeTankSetup(
		                {
			                ETankSubtype::Tank_T34_76, ETankSubtype::Tank_T34_76, ETankSubtype::Tank_T34_85,
			                ETankSubtype::Tank_T34_85
		                },
		                Defaults::MediumDivisionFallbackStrength));
	PieceSetups.Add(EWorldFieldDivisions::SovietHeavyArmorDivision, Defaults::MakeTankSetup(
		                {ETankSubtype::Tank_KV_1, ETankSubtype::Tank_KV_1, ETankSubtype::Tank_IS_2},
		                Defaults::HeavyDivisionFallbackStrength));
	PieceSetups.Add(EWorldFieldDivisions::SovietInfantryDivision, Defaults::MakeSquadSetup(
		                {
			                ESquadSubtype::Squad_Rus_Mosin, ESquadSubtype::Squad_Rus_Mosin,
			                ESquadSubtype::Squad_Rus_Mosin, ESquadSubtype::Squad_Rus_Maxim
		                },
		                Defaults::InfantryDivisionFallbackStrength));

	FWorldEnemyReactionPieceSetup ConvoySetup;
	ConvoySetup.FallbackMaxStrengthPercentage = Defaults::ConvoyFallbackStrength;
	PieceSetups.Add(EWorldFieldDivisions::SovietSupplyConvoy, ConvoySetup);
}

const FWorldEnemyTurnNumbers& UWorldEnemyTurnSettings::GetNumbers()
{
	return GetDefault<UWorldEnemyTurnSettings>()->Numbers;
}

const FWorldEnemyReactionPieceSetup* UWorldEnemyTurnSettings::FindPieceSetup(const EWorldFieldDivisions PieceType)
{
	return GetNumbers().PieceSetups.Find(PieceType);
}
