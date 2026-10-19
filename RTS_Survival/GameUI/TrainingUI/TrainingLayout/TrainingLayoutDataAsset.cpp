// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TrainingLayoutDataAsset.h"

#include "Engine/DataTable.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TrainingLayout"

namespace TrainingLayoutDefaults
{
	const TCHAR* const TrainingOptionIconTablePath =
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions.DT_TrainingOptions");

	FTrainingLayoutSubSection MakeSubSection(const FText& DisplayName, const TArray<FTrainingLayoutSlot>& Slots)
	{
		FTrainingLayoutSubSection SubSection;
		SubSection.DisplayName = DisplayName;
		SubSection.Slots = Slots;
		// Always a full grid, so that a designer can fill in any slot without first adding the ones before it.
		while (SubSection.Slots.Num() < TrainingLayoutConstants::SlotsPerSubSection)
		{
			SubSection.Slots.Add(FTrainingLayoutSlot::Empty());
		}
		return SubSection;
	}

	FTrainingLayoutSubSection MakeSubSection(const FText& DisplayName,
	                                         std::initializer_list<FTrainingLayoutSlot> Slots)
	{
		return MakeSubSection(DisplayName, TArray<FTrainingLayoutSlot>(Slots));
	}

	FTrainingLayoutBuilding MakeBuilding(std::initializer_list<FTrainingLayoutSubSection> SubSections,
	                                     const bool bRequiresUnitCards = true)
	{
		FTrainingLayoutBuilding Building;
		Building.bRequiresUnitCards = bRequiresUnitCards;
		Building.SubSections = SubSections;
		return Building;
	}
}

FTrainingOption FTrainingLayoutSlot::ToTrainingOption() const
{
	switch (UnitType)
	{
	case EAllUnitType::UNType_Tank:
	case EAllUnitType::UNType_Harvester:
		return FTrainingOption(UnitType, static_cast<uint8>(TankSubtype));
	case EAllUnitType::UNType_Squad:
		return FTrainingOption(UnitType, static_cast<uint8>(SquadSubtype));
	case EAllUnitType::UNType_Nomadic:
		return FTrainingOption(UnitType, static_cast<uint8>(NomadicSubtype));
	case EAllUnitType::UNType_Aircraft:
		return FTrainingOption(UnitType, static_cast<uint8>(AircraftSubtype));
	case EAllUnitType::UNType_BuildingExpansion:
		return FTrainingOption(UnitType, static_cast<uint8>(FieldDefenseType));
	default:
		return FTrainingOption();
	}
}

FTrainingLayoutSlot FTrainingLayoutSlot::Empty()
{
	return FTrainingLayoutSlot();
}

FTrainingLayoutSlot FTrainingLayoutSlot::Tank(const ETankSubtype InTankSubtype)
{
	FTrainingLayoutSlot LayoutSlot;
	LayoutSlot.UnitType = EAllUnitType::UNType_Tank;
	LayoutSlot.TankSubtype = InTankSubtype;
	return LayoutSlot;
}

FTrainingLayoutSlot FTrainingLayoutSlot::Squad(const ESquadSubtype InSquadSubtype)
{
	FTrainingLayoutSlot LayoutSlot;
	LayoutSlot.UnitType = EAllUnitType::UNType_Squad;
	LayoutSlot.SquadSubtype = InSquadSubtype;
	return LayoutSlot;
}

FTrainingLayoutSlot FTrainingLayoutSlot::Nomadic(const ENomadicSubtype InNomadicSubtype)
{
	FTrainingLayoutSlot LayoutSlot;
	LayoutSlot.UnitType = EAllUnitType::UNType_Nomadic;
	LayoutSlot.NomadicSubtype = InNomadicSubtype;
	return LayoutSlot;
}

FTrainingLayoutSlot FTrainingLayoutSlot::Aircraft(const EAircraftSubtype InAircraftSubtype)
{
	FTrainingLayoutSlot LayoutSlot;
	LayoutSlot.UnitType = EAllUnitType::UNType_Aircraft;
	LayoutSlot.AircraftSubtype = InAircraftSubtype;
	return LayoutSlot;
}

FTrainingLayoutSlot FTrainingLayoutSlot::FieldDefense(const EBuildingExpansionType InFieldDefenseType)
{
	FTrainingLayoutSlot LayoutSlot;
	LayoutSlot.UnitType = EAllUnitType::UNType_BuildingExpansion;
	LayoutSlot.FieldDefenseType = InFieldDefenseType;
	return LayoutSlot;
}

UTrainingLayoutDataAsset::UTrainingLayoutDataAsset()
{
	TrainingOptionIconTable = FSoftObjectPath(TrainingLayoutDefaults::TrainingOptionIconTablePath);
	InitDefaultQuickNavBuildings();
	InitDefaultStrikeDivisionLayout();
	InitDefaultBreakthroughLayout();
}

const FTrainingLayoutBuilding* UTrainingLayoutDataAsset::FindBuildingLayout(
	const ERTSFaction Faction,
	const ENomadicSubtype Building) const
{
	if (const FTrainingLayoutFaction* FactionLayout = FactionLayouts.Find(Faction))
	{
		if (const FTrainingLayoutBuilding* BuildingLayout = FactionLayout->Buildings.Find(Building))
		{
			return BuildingLayout;
		}
	}

	const FTrainingLayoutFaction* FallbackLayout = FactionLayouts.Find(ERTSFaction::GerStrikeDivision);
	if (not FallbackLayout)
	{
		return nullptr;
	}
	return FallbackLayout->Buildings.Find(Building);
}

#if WITH_EDITOR
EDataValidationResult UTrainingLayoutDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TMap<ENomadicSubtype, int32> SubSectionCountPerBuilding;

	for (const TPair<ERTSFaction, FTrainingLayoutFaction>& FactionPair : FactionLayouts)
	{
		const FString FactionName = UEnum::GetValueAsString(FactionPair.Key);
		for (const TPair<ENomadicSubtype, FTrainingLayoutBuilding>& BuildingPair : FactionPair.Value.Buildings)
		{
			const FString BuildingName = FactionName + TEXT(" / ") + UEnum::GetValueAsString(BuildingPair.Key);
			const int32 SubSectionCount = BuildingPair.Value.SubSections.Num();

			if (SubSectionCount == 0 || SubSectionCount > TrainingLayoutConstants::MaxSubSectionsPerBuilding)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("%s has %d sub sections; the training UI supports 1 to %d."),
					*BuildingName, SubSectionCount, TrainingLayoutConstants::MaxSubSectionsPerBuilding)));
				Result = EDataValidationResult::Invalid;
			}

			const int32* CountInOtherFaction = SubSectionCountPerBuilding.Find(BuildingPair.Key);
			if (CountInOtherFaction && *CountInOtherFaction != SubSectionCount)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("%s has %d sub sections but another faction has %d for the same building."),
					*BuildingName, SubSectionCount, *CountInOtherFaction)));
				Result = EDataValidationResult::Invalid;
			}
			SubSectionCountPerBuilding.Add(BuildingPair.Key, SubSectionCount);

			for (const FTrainingLayoutSubSection& SubSection : BuildingPair.Value.SubSections)
			{
				if (SubSection.Slots.Num() <= TrainingLayoutConstants::SlotsPerSubSection)
				{
					continue;
				}
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("%s / %s has %d slots; a sub section shows at most %d."),
					*BuildingName, *SubSection.DisplayName.ToString(), SubSection.Slots.Num(),
					TrainingLayoutConstants::SlotsPerSubSection)));
				Result = EDataValidationResult::Invalid;
			}
		}
	}
	return Result;
}
#endif

void UTrainingLayoutDataAsset::InitDefaultQuickNavBuildings()
{
	const TArray<ENomadicSubtype> QuickNavOrder = {
		ENomadicSubtype::Nomadic_GerHq,
		ENomadicSubtype::Nomadic_GerBarracks,
		ENomadicSubtype::Nomadic_GerMechanizedDepot,
		ENomadicSubtype::Nomadic_GerLightSteelForge,
		ENomadicSubtype::Nomadic_GerMedTankFactory,
		ENomadicSubtype::Nomadic_GerAirbase,
		ENomadicSubtype::Nomadic_GerExperimentalUnitsFactory
	};

	for (const ENomadicSubtype Building : QuickNavOrder)
	{
		FTrainingQuickNavEntry QuickNavEntry;
		QuickNavEntry.Building = Building;
		QuickNavBuildings.Add(QuickNavEntry);
	}
}

FTrainingLayoutBuilding UTrainingLayoutDataAsset::MakeDefaultHeadquartersLayout(
	const TArray<FTrainingLayoutSlot>& DoctrineFieldDefenses)
{
	using namespace TrainingLayoutDefaults;
	using Slot = FTrainingLayoutSlot;
	constexpr bool bRequiresUnitCards = false;
	TArray<FTrainingLayoutSlot> FieldDefenses = {
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerPak40Pit),
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerFlak37Pit),
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerMortarPit),
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerPuppchenPit),
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerTwinMG42Pit),
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerLeFH18Pit),
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerSFH18Pit),
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerPanzerschreckPit),
		Slot::FieldDefense(EBuildingExpansionType::BTX_GerFlak88Pit)
	};
	FieldDefenses.Append(DoctrineFieldDefenses);

	return MakeBuilding({
		                    MakeSubSection(LOCTEXT("Economy", "Economy"), {
			                                   Slot::Tank(ETankSubtype::Tank_PzI_Harvester),
			                                   Slot::Squad(ESquadSubtype::Squad_Ger_Scavengers),
			                                   Slot::Empty(),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerGammaFacility),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerRefinery),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerMetalVault),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerArmory),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerCommunicationCenter),
			                                   Slot::Empty()
		                                   }),
		                    // The trainer builds these at a spot the player places, see UTrainerComponent.
		                    MakeSubSection(LOCTEXT("FieldDefenses", "Field Defenses"), FieldDefenses),
		                    MakeSubSection(LOCTEXT("Factories", "Factories"), {
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerBarracks),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerMechanizedDepot),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerLightSteelForge),
			                                   // Not in the design notes, but without it the medium tank factory
			                                   // could not be built at all.
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerMedTankFactory),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerAirbase),
			                                   Slot::Empty(),
			                                   Slot::Empty(),
			                                   Slot::Nomadic(ENomadicSubtype::Nomadic_GerExperimentalUnitsFactory),
			                                   Slot::Empty()
		                                   })
	                    }, bRequiresUnitCards);
}

FTrainingLayoutBuilding UTrainingLayoutDataAsset::MakeDefaultAirbaseLayout()
{
	using namespace TrainingLayoutDefaults;
	using Slot = FTrainingLayoutSlot;

	return MakeBuilding({
		MakeSubSection(LOCTEXT("Aircraft", "Aircraft"), {
			               Slot::Aircraft(EAircraftSubtype::Aircraft_Bf109),
			               Slot::Aircraft(EAircraftSubtype::Aircraft_Ju87),
			               Slot::Aircraft(EAircraftSubtype::Aircraft_Me410)
		               })
	});
}

void UTrainingLayoutDataAsset::InitDefaultStrikeDivisionLayout()
{
	using namespace TrainingLayoutDefaults;
	using Slot = FTrainingLayoutSlot;

	TMap<ENomadicSubtype, FTrainingLayoutBuilding>& Buildings =
		FactionLayouts.FindOrAdd(ERTSFaction::GerStrikeDivision).Buildings;

	Buildings.Add(ENomadicSubtype::Nomadic_GerHq, MakeDefaultHeadquartersLayout());
	Buildings.Add(ENomadicSubtype::Nomadic_GerAirbase, MakeDefaultAirbaseLayout());

	Buildings.Add(ENomadicSubtype::Nomadic_GerBarracks, MakeBuilding({
		              MakeSubSection(LOCTEXT("AssaultSquads", "Assault Squads"), {
			                             Slot::Squad(ESquadSubtype::Squad_Ger_Scavengers),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_JagerTruppKar98k),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SturmPionieren),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_Vultures),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_IronStorm),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SturmKommandos),
			                             Slot::Empty()
		                             }),
		              MakeSubSection(LOCTEXT("SupportWeapons", "Support Weapons"), {
			                             Slot::Squad(ESquadSubtype::Squad_Ger_37mmFlak),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_PaK38),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_GrW42_80mm),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_Nebelwerfer),
			                             Slot::Empty()
		                             }),
		              MakeSubSection(LOCTEXT("Specialists", "Specialists"), {
			                             Slot::Squad(ESquadSubtype::Squad_Ger_Gebirgsjagerin),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SteelFistAssaultSquad),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_PanzerGrenadiere),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SniperTeam),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_LightBringers),
			                             Slot::Empty()
		                             })
	              }));

	Buildings.Add(ENomadicSubtype::Nomadic_GerMechanizedDepot, MakeBuilding({
		              MakeSubSection(LOCTEXT("Recons", "Recons"), {
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz_231),
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz251_Mortar),
			                             Slot::Tank(ETankSubtype::Tank_Puma),
			                             Slot::Tank(ETankSubtype::Tank_FlamePuma),
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz9_37mm),
			                             Slot::Empty()
		                             }),
		              MakeSubSection(LOCTEXT("SupportWeapons", "Support Weapons"), {
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz251_22),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SFH18_150mm),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_88mmFlak),
			                             Slot::Empty()
		                             })
	              }));

	Buildings.Add(ENomadicSubtype::Nomadic_GerLightSteelForge, MakeBuilding({
		              MakeSubSection(LOCTEXT("LightTanks", "Light Tanks"), {
			                             Slot::Tank(ETankSubtype::Tank_PzII_F),
			                             Slot::Tank(ETankSubtype::Tank_Pz38t),
			                             Slot::Tank(ETankSubtype::Tank_PzJagerLaser),
			                             Slot::Tank(ETankSubtype::Tank_PzI_15cm),
			                             Slot::Tank(ETankSubtype::Tank_PzIII_J),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Tank(ETankSubtype::Tank_Pz38t_R),
			                             Slot::Empty()
		                             })
	              }));

	Buildings.Add(ENomadicSubtype::Nomadic_GerMedTankFactory, MakeBuilding({
		              MakeSubSection(LOCTEXT("MediumTanks", "Medium Tanks"), {
			                             Slot::Tank(ETankSubtype::Tank_Stug),
			                             Slot::Tank(ETankSubtype::Tank_Jaguar),
			                             Slot::Tank(ETankSubtype::Tank_Hetzer),
			                             Slot::Tank(ETankSubtype::Tank_Brumbar),
			                             Slot::Tank(ETankSubtype::Tank_PzIII_M),
			                             Slot::Tank(ETankSubtype::Tank_PzIII_FLamm)
		                             })
	              }));

	Buildings.Add(ENomadicSubtype::Nomadic_GerExperimentalUnitsFactory, MakeBuilding({
		              MakeSubSection(LOCTEXT("HeavyTanks", "Heavy Tanks"), {
			                             Slot::Tank(ETankSubtype::Tank_PantherD),
			                             Slot::Tank(ETankSubtype::Tank_JagdPanther),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Tank(ETankSubtype::Tank_PantherII),
			                             Slot::Empty()
		                             }),
		              MakeSubSection(LOCTEXT("Experimental", "Experimental"), {
			                             Slot::Tank(ETankSubtype::Tank_E25),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Tank(ETankSubtype::Tank_SturmTiger),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_210MM_Morser),
			                             Slot::Tank(ETankSubtype::Tank_E100),
			                             Slot::Empty()
		                             })
	              }));
}

void UTrainingLayoutDataAsset::InitDefaultBreakthroughLayout()
{
	using namespace TrainingLayoutDefaults;
	using Slot = FTrainingLayoutSlot;

	TMap<ENomadicSubtype, FTrainingLayoutBuilding>& Buildings =
		FactionLayouts.FindOrAdd(ERTSFaction::GerBreakthroughDoctrine).Buildings;

	// The rail Pak bunker is the breakthrough doctrine's own field defense.
	Buildings.Add(ENomadicSubtype::Nomadic_GerHq, MakeDefaultHeadquartersLayout({
		              Slot::FieldDefense(EBuildingExpansionType::BTX_GerRailPakBunker)
	              }));
	Buildings.Add(ENomadicSubtype::Nomadic_GerAirbase, MakeDefaultAirbaseLayout());

	Buildings.Add(ENomadicSubtype::Nomadic_GerBarracks, MakeBuilding({
		              MakeSubSection(LOCTEXT("AssaultSquads", "Assault Squads"), {
			                             Slot::Squad(ESquadSubtype::Squad_Ger_Scavengers),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_JagerTruppKar98k),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SturmPionieren),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_Vultures),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_FeuerSturm),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SturmKommandos),
			                             Slot::Empty()
		                             }),
		              MakeSubSection(LOCTEXT("SupportWeapons", "Support Weapons"), {
			                             Slot::Squad(ESquadSubtype::Squad_Ger_37mmFlak),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_PaK40),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_GrW42_80mm),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_Nebelwerfer),
			                             Slot::Empty()
		                             }),
		              MakeSubSection(LOCTEXT("Specialists", "Specialists"), {
			                             Slot::Squad(ESquadSubtype::Squad_Ger_Gebirgsjagerin),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SteelFistAssaultSquad),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_PanzerGrenadiere),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_SniperTeam),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_LMGSquad),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_LightBringers),
			                             Slot::Empty()
		                             })
	              }));

	Buildings.Add(ENomadicSubtype::Nomadic_GerMechanizedDepot, MakeBuilding({
		              MakeSubSection(LOCTEXT("Recons", "Recons"), {
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz250),
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz250_37mm),
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz_232_3),
			                             Slot::Tank(ETankSubtype::Tank_Panzerwerfer),
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz_140),
			                             Slot::Empty()
		                             }),
		              MakeSubSection(LOCTEXT("SupportWeapons", "Support Weapons"), {
			                             Slot::Tank(ETankSubtype::Tank_Sdkfz251_PZIV),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_LefH18),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_88mmFlak),
			                             Slot::Empty()
		                             })
	              }));

	Buildings.Add(ENomadicSubtype::Nomadic_GerLightSteelForge, MakeBuilding({
		              MakeSubSection(LOCTEXT("LightTanks", "Light Tanks"), {
			                             Slot::Tank(ETankSubtype::Tank_PzII_F),
			                             Slot::Tank(ETankSubtype::Tank_Pz38t),
			                             Slot::Tank(ETankSubtype::Tank_PzJager),
			                             Slot::Tank(ETankSubtype::Tank_PzI_15cm),
			                             Slot::Tank(ETankSubtype::Tank_PzIV_F1),
			                             Slot::Empty(),
			                             Slot::Tank(ETankSubtype::Tank_Marder),
			                             Slot::Tank(ETankSubtype::Tank_Pz38t_RailGun),
			                             Slot::Empty()
		                             })
	              }));

	Buildings.Add(ENomadicSubtype::Nomadic_GerMedTankFactory, MakeBuilding({
		              MakeSubSection(LOCTEXT("MediumTanks", "Medium Tanks"), {
			                             Slot::Tank(ETankSubtype::Tank_Stug),
			                             Slot::Tank(ETankSubtype::Tank_PzIV_70),
			                             Slot::Tank(ETankSubtype::Tank_Hetzer),
			                             Slot::Tank(ETankSubtype::Tank_Brumbar),
			                             Slot::Tank(ETankSubtype::Tank_PzIV_G),
			                             Slot::Tank(ETankSubtype::Tank_PzIV_H),
			                             Slot::Empty(),
			                             Slot::Tank(ETankSubtype::Tank_PZIV_Rockets),
			                             Slot::Empty()
		                             })
	              }));

	Buildings.Add(ENomadicSubtype::Nomadic_GerExperimentalUnitsFactory, MakeBuilding({
		              MakeSubSection(LOCTEXT("HeavyTanks", "Heavy Tanks"), {
			                             Slot::Tank(ETankSubtype::Tank_TigerH1),
			                             Slot::Tank(ETankSubtype::Tank_JagdTiger),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Tank(ETankSubtype::Tank_KingTiger),
			                             Slot::Empty()
		                             }),
		              MakeSubSection(LOCTEXT("Experimental", "Experimental"), {
			                             Slot::Tank(ETankSubtype::Tank_TigerRail),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Empty(),
			                             Slot::Tank(ETankSubtype::Tank_Tiger105),
			                             Slot::Empty(),
			                             Slot::Squad(ESquadSubtype::Squad_Ger_210MM_Morser),
			                             Slot::Tank(ETankSubtype::Tank_Maus),
			                             Slot::Empty()
		                             })
	              }));
}

#undef LOCTEXT_NAMESPACE
