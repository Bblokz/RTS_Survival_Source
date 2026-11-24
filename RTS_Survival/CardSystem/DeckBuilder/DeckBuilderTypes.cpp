// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DeckBuilderTypes.h"

namespace DeckBuilderTypesPrivate
{
	const EDeckSection SectionsInRailOrder[DeckBuilder::SectionCount] = {
		EDeckSection::Technologies, EDeckSection::Barracks, EDeckSection::MechanicalDepot, EDeckSection::Forge,
		EDeckSection::T2Factory, EDeckSection::Airbase, EDeckSection::Experimental
	};
}

TConstArrayView<EDeckSection> DeckBuilder::GetAllSections()
{
	return TConstArrayView<EDeckSection>(DeckBuilderTypesPrivate::SectionsInRailOrder, SectionCount);
}

ECardType DeckBuilder::GetCardTypeOfSection(const EDeckSection Section)
{
	switch (Section)
	{
	case EDeckSection::Technologies:
		return ECardType::Technology;
	case EDeckSection::Barracks:
		return ECardType::BarracksTrain;
	case EDeckSection::MechanicalDepot:
		return ECardType::MechanicalDepotTrain;
	case EDeckSection::Forge:
		return ECardType::ForgeTrain;
	case EDeckSection::T2Factory:
		return ECardType::T2FactoryTrain;
	case EDeckSection::Airbase:
		return ECardType::AirbaseTrain;
	case EDeckSection::Experimental:
		return ECardType::ExperimentalFactoryTrain;
	}
	return ECardType::Invalid;
}

ENomadicLayoutBuildingType DeckBuilder::GetBuildingTypeOfSection(const EDeckSection Section)
{
	switch (Section)
	{
	case EDeckSection::Technologies:
		return ENomadicLayoutBuildingType::Building_None;
	case EDeckSection::Barracks:
		return ENomadicLayoutBuildingType::Building_Barracks;
	case EDeckSection::MechanicalDepot:
		return ENomadicLayoutBuildingType::Building_MechanicalDepot;
	case EDeckSection::Forge:
		return ENomadicLayoutBuildingType::Building_Forge;
	case EDeckSection::T2Factory:
		return ENomadicLayoutBuildingType::Building_T2Factory;
	case EDeckSection::Airbase:
		return ENomadicLayoutBuildingType::Building_Airbase;
	case EDeckSection::Experimental:
		return ENomadicLayoutBuildingType::Building_Experimental;
	}
	return ENomadicLayoutBuildingType::Building_None;
}

bool DeckBuilder::GetSectionOfCardType(const ECardType CardType, EDeckSection& OutSection)
{
	for (const EDeckSection Section : GetAllSections())
	{
		if (GetCardTypeOfSection(Section) == CardType)
		{
			OutSection = Section;
			return true;
		}
	}
	return false;
}

bool DeckBuilder::GetSectionOfBuildingType(const ENomadicLayoutBuildingType BuildingType, EDeckSection& OutSection)
{
	if (BuildingType == ENomadicLayoutBuildingType::Building_None)
	{
		return false;
	}
	for (const EDeckSection Section : GetAllSections())
	{
		if (GetBuildingTypeOfSection(Section) == BuildingType)
		{
			OutSection = Section;
			return true;
		}
	}
	return false;
}

FText DeckBuilder::GetSectionDisplayName(const EDeckSection Section)
{
	switch (Section)
	{
	case EDeckSection::Technologies:
		return NSLOCTEXT("DeckBuilder", "SectionTechnologies", "Technologies");
	case EDeckSection::Barracks:
		return NSLOCTEXT("DeckBuilder", "SectionBarracks", "Barracks");
	case EDeckSection::MechanicalDepot:
		return NSLOCTEXT("DeckBuilder", "SectionMechanicalDepot", "Mech. Depot");
	case EDeckSection::Forge:
		return NSLOCTEXT("DeckBuilder", "SectionForge", "Forge");
	case EDeckSection::T2Factory:
		return NSLOCTEXT("DeckBuilder", "SectionT2Factory", "T2 Factory");
	case EDeckSection::Airbase:
		return NSLOCTEXT("DeckBuilder", "SectionAirbase", "Airbase");
	case EDeckSection::Experimental:
		return NSLOCTEXT("DeckBuilder", "SectionExperimental", "Experimental");
	}
	return FText::GetEmpty();
}

FText DeckBuilder::GetSectionSubtitle(const EDeckSection Section)
{
	switch (Section)
	{
	case EDeckSection::Technologies:
		return NSLOCTEXT("DeckBuilder", "SubtitleTechnologies", "researched when the mission starts");
	case EDeckSection::Barracks:
		return NSLOCTEXT("DeckBuilder", "SubtitleBarracks", "infantry and team weapons");
	case EDeckSection::MechanicalDepot:
		return NSLOCTEXT("DeckBuilder", "SubtitleMechanicalDepot", "armoured cars and half-tracks");
	case EDeckSection::Forge:
		return NSLOCTEXT("DeckBuilder", "SubtitleForge", "light vehicles");
	case EDeckSection::T2Factory:
		return NSLOCTEXT("DeckBuilder", "SubtitleT2Factory", "medium vehicles");
	case EDeckSection::Airbase:
		return NSLOCTEXT("DeckBuilder", "SubtitleAirbase", "aircraft");
	case EDeckSection::Experimental:
		return NSLOCTEXT("DeckBuilder", "SubtitleExperimental", "heavy and experimental vehicles");
	}
	return FText::GetEmpty();
}
