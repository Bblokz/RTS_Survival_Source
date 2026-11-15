// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadInfoTakeoverContent.h"

#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantProbes.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "Slate/SlateBrushAsset.h"

namespace SquadInfoTakeoverContentPrivate
{
	FText GetDefaultDisplayName(const FTrainingOption& TrainingOption, const UDataTable* TrainingOptionsTable)
	{
		const FText TableTitle = UUnitVariantPresentationLibrary::FindTrainingTableTitle(
			TrainingOptionsTable, TrainingOption);
		return TableTitle.IsEmpty()
			       ? UUnitVariantPresentationLibrary::GetUnitDisplayNameFallback(TrainingOption)
			       : TableTitle;
	}

	void FillImage(const FSquadInfoTakeoverEntry& Entry, const FTrainingOption& TrainingOption,
	               const UDataTable* TrainingOptionsTable, FSquadInfoTakeoverContent& OutContent)
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

	// The squad's own training option sits on the controller; a solo unit carries its own.
	FTrainingOption GetTrainingOption(const AActor& Infantry)
	{
		const URTSComponent* const RTSComponent = Infantry.FindComponentByClass<URTSComponent>();
		return IsValid(RTSComponent) ? RTSComponent->GetUnitTrainingOption() : FTrainingOption();
	}

	void ReadSoldiers(const TArray<ASquadUnit*>& Soldiers, FSquadInfoTakeoverContent& OutContent)
	{
		TArray<const UWeaponState*> WeaponStates;
		for (const ASquadUnit* const Soldier : Soldiers)
		{
			if (not IsValid(Soldier))
			{
				continue;
			}
			++OutContent.SoldierCount;
			WeaponStates.Add(Soldier->GetWeaponState());
			const UHealthComponent* const Health = Soldier->FindComponentByClass<UHealthComponent>();
			if (IsValid(Health))
			{
				OutContent.HealthPerSoldier = FMath::Max(OutContent.HealthPerSoldier, Health->GetMaxHealth());
			}
		}
		OutContent.Weapons = UUnitVariantProbes::ReadWeaponStatsFromStates(WeaponStates);
	}
}

FSquadInfoTakeoverContent SquadInfoTakeoverContent::BuildContent(AActor& Infantry,
                                                                 const FSquadInfoTakeoverEntry& Entry,
                                                                 const UDataTable* TrainingOptionsTable)
{
	using namespace SquadInfoTakeoverContentPrivate;
	FSquadInfoTakeoverContent Content;
	const FTrainingOption TrainingOption = GetTrainingOption(Infantry);
	Content.DisplayName = Entry.DisplayName.IsEmpty()
		                      ? GetDefaultDisplayName(TrainingOption, TrainingOptionsTable)
		                      : Entry.DisplayName;
	Content.Description = Entry.Description;
	Content.ServiceYear = Entry.ServiceYear;
	FillImage(Entry, TrainingOption, TrainingOptionsTable, Content);
	ReadSoldiers(GetSoldiers(Infantry), Content);
	return Content;
}

TArray<ASquadUnit*> SquadInfoTakeoverContent::GetSoldiers(AActor& Infantry)
{
	if (ASquadController* const SquadController = Cast<ASquadController>(&Infantry))
	{
		return SquadController->GetSquadUnitsChecked();
	}
	if (ASquadUnit* const SquadUnit = Cast<ASquadUnit>(&Infantry))
	{
		return {SquadUnit};
	}
	return {};
}

bool SquadInfoTakeoverContent::GetIsInfantry(const AActor& Actor)
{
	return Actor.IsA<ASquadController>() || Actor.IsA<ASquadUnit>();
}
