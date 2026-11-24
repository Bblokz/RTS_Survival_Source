// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleInfoTakeoverContent.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantProbes.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "Slate/SlateBrushAsset.h"

namespace VehicleInfoTakeoverContentPrivate
{
	/**
	 * Plate names without left or right: hulls are symmetric, so "the weakest side" is the same plate on both.
	 * Turret_SidesAndRear counts for the sides and the rear.
	 */
	FText GetNeutralPlateName(const EArmorPlate Plate)
	{
		switch (Plate)
		{
		case EArmorPlate::Plate_Front:
			return NSLOCTEXT("InfoTakeover", "PlateFront", "Hull front");
		case EArmorPlate::Plate_FrontUpperGlacis:
			return NSLOCTEXT("InfoTakeover", "PlateUpperGlacis", "Upper glacis");
		case EArmorPlate::Plate_FrontLowerGlacis:
			return NSLOCTEXT("InfoTakeover", "PlateLowerGlacis", "Lower glacis");
		case EArmorPlate::Plate_SideLeft:
		case EArmorPlate::Plate_SideRight:
			return NSLOCTEXT("InfoTakeover", "PlateSide", "Hull side");
		case EArmorPlate::Plate_SideLowerLeft:
		case EArmorPlate::Plate_SideLowerRight:
			return NSLOCTEXT("InfoTakeover", "PlateLowerSide", "Lower hull side");
		case EArmorPlate::Plate_Rear:
			return NSLOCTEXT("InfoTakeover", "PlateRear", "Hull rear");
		case EArmorPlate::Plate_RearLowerGlacis:
			return NSLOCTEXT("InfoTakeover", "PlateLowerRear", "Lower hull rear");
		case EArmorPlate::Plate_RearUpperGlacis:
			return NSLOCTEXT("InfoTakeover", "PlateUpperRear", "Upper hull rear");
		case EArmorPlate::Turret_Front:
			return NSLOCTEXT("InfoTakeover", "PlateTurretFront", "Turret front");
		case EArmorPlate::Turret_SideLeft:
		case EArmorPlate::Turret_SideRight:
			return NSLOCTEXT("InfoTakeover", "PlateTurretSide", "Turret side");
		case EArmorPlate::Turret_Rear:
			return NSLOCTEXT("InfoTakeover", "PlateTurretRear", "Turret rear");
		case EArmorPlate::Turret_SidesAndRear:
			return NSLOCTEXT("InfoTakeover", "PlateTurretSidesRear", "Turret sides/rear");
		case EArmorPlate::Turret_Cupola:
			return NSLOCTEXT("InfoTakeover", "PlateCupola", "Cupola");
		case EArmorPlate::Turret_Mantlet:
			return NSLOCTEXT("InfoTakeover", "PlateMantlet", "Gun mantlet");
		}
		return FText::GetEmpty();
	}

	bool GetCountsForSides(const EArmorPlate Plate)
	{
		return Global_GetDamageTypeFromPlate(Plate) == EArmorPlateDamageType::DamageSides;
	}

	bool GetCountsForRear(const EArmorPlate Plate)
	{
		return Global_GetDamageTypeFromPlate(Plate) == EArmorPlateDamageType::DamageRear
			|| Plate == EArmorPlate::Turret_SidesAndRear;
	}

	// The small cupola would make every frontal range start at a few mm; players read the range off the big plates.
	bool GetCountsForFrontRange(const EArmorPlate Plate)
	{
		return Global_GetDamageTypeFromPlate(Plate) == EArmorPlateDamageType::DamageFront
			&& Plate != EArmorPlate::Turret_Cupola;
	}

	void ConsiderWeakestPlate(const FArmorSettings& Plate, FInfoTakeoverWeakestPlate& InOutWeakest)
	{
		const float EffectiveMm = Plate.ArmorValue + Plate.AddOnArmorValue;
		if (InOutWeakest.bIsValid && EffectiveMm >= InOutWeakest.ArmorMm + InOutWeakest.AddOnArmorMm)
		{
			return;
		}
		InOutWeakest.bIsValid = true;
		InOutWeakest.PlateName = GetNeutralPlateName(Plate.ArmorType);
		InOutWeakest.ArmorMm = Plate.ArmorValue;
		InOutWeakest.AddOnArmorMm = Plate.AddOnArmorValue;
	}

	void ConsiderFrontPlate(const FArmorSettings& Plate, FInfoTakeoverArmorSummary& InOutSummary)
	{
		InOutSummary.FrontMinMm = InOutSummary.bHasFront
			                          ? FMath::Min(InOutSummary.FrontMinMm, Plate.ArmorValue)
			                          : Plate.ArmorValue;
		InOutSummary.FrontMaxMm = InOutSummary.bHasFront
			                          ? FMath::Max(InOutSummary.FrontMaxMm, Plate.ArmorValue)
			                          : Plate.ArmorValue;
		InOutSummary.FrontAddOnMaxMm = FMath::Max(InOutSummary.FrontAddOnMaxMm, Plate.AddOnArmorValue);
		InOutSummary.bHasFront = true;
	}

	void ReadPlates(const FArmorSettings* const Plates, const int32 PlateCount, FInfoTakeoverArmorSummary& InOutSummary)
	{
		for (int32 PlateIndex = 0; PlateIndex < FMath::Min(PlateCount, MaxArmorPlatesPerMesh); ++PlateIndex)
		{
			const FArmorSettings& Plate = Plates[PlateIndex];
			if (GetCountsForFrontRange(Plate.ArmorType))
			{
				ConsiderFrontPlate(Plate, InOutSummary);
			}
			if (GetCountsForSides(Plate.ArmorType))
			{
				ConsiderWeakestPlate(Plate, InOutSummary.WeakestSide);
			}
			if (GetCountsForRear(Plate.ArmorType))
			{
				ConsiderWeakestPlate(Plate, InOutSummary.WeakestRear);
			}
		}
	}

	FText GetDefaultDisplayName(const FTrainingOption& TrainingOption, const UDataTable* TrainingOptionsTable)
	{
		const FText TableTitle = UUnitVariantPresentationLibrary::FindTrainingTableTitle(
			TrainingOptionsTable, TrainingOption);
		return TableTitle.IsEmpty()
			       ? UUnitVariantPresentationLibrary::GetUnitDisplayNameFallback(TrainingOption)
			       : TableTitle;
	}

	void FillImage(const FVehicleInfoTakeoverEntry& Entry, const FTrainingOption& TrainingOption,
	               const UDataTable* TrainingOptionsTable, FVehicleInfoTakeoverContent& OutContent)
	{
		if (UTexture2D* const EntryTexture = Entry.Image.LoadSynchronous())
		{
			OutContent.ImageTexture = EntryTexture;
			OutContent.bHasImage = true;
			return;
		}
		const USlateBrushAsset* const TableBrush = UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(
			TrainingOptionsTable, TrainingOption);
		if (not IsValid(TableBrush))
		{
			return;
		}
		OutContent.ImageBrush = TableBrush->Brush;
		OutContent.bHasImage = true;
	}
}

FVehicleInfoTakeoverContent VehicleInfoTakeoverContent::BuildContent(const ATankMaster& Vehicle,
                                                                     const FVehicleInfoTakeoverEntry& Entry,
                                                                     const UDataTable* TrainingOptionsTable)
{
	using namespace VehicleInfoTakeoverContentPrivate;
	FVehicleInfoTakeoverContent Content;
	const URTSComponent* const RTSComponent = Vehicle.FindComponentByClass<URTSComponent>();
	const FTrainingOption TrainingOption = IsValid(RTSComponent) ? RTSComponent->GetUnitTrainingOption()
		                                       : FTrainingOption();
	Content.DisplayName = Entry.DisplayName.IsEmpty()
		                      ? GetDefaultDisplayName(TrainingOption, TrainingOptionsTable)
		                      : Entry.DisplayName;
	Content.Description = Entry.Description;
	Content.ProductionYear = Entry.ProductionYear;
	FillImage(Entry, TrainingOption, TrainingOptionsTable, Content);
	Content.Weapons = UUnitVariantProbes::ReadWeaponStats(Vehicle);
	Content.Armor = ReadArmorSummary(Vehicle);
	return Content;
}

FInfoTakeoverArmorSummary VehicleInfoTakeoverContent::ReadArmorSummary(const AActor& Vehicle)
{
	using namespace VehicleInfoTakeoverContentPrivate;
	FInfoTakeoverArmorSummary Summary;
	const UArmorCalculation* const ArmorCalculation = Vehicle.FindComponentByClass<UArmorCalculation>();
	if (not IsValid(ArmorCalculation))
	{
		return Summary;
	}
	const FArmorSetup Setup = ArmorCalculation->GetArmorSetup();
	ReadPlates(Setup.ArmorSettings0, Setup.NumArmorPlates0, Summary);
	ReadPlates(Setup.ArmorSettings1, Setup.NumArmorPlates1, Summary);
	ReadPlates(Setup.ArmorSettings2, Setup.NumArmorPlates2, Summary);
	return Summary;
}
