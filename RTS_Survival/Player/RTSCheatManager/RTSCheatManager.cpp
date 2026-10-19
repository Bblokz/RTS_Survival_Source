// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "RTSCheatManager.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Buildings/EnergyComponent/EnergyComp.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/EnemyControllerSubsystem/EnemyControllerSubsystem.h"

void URTSCheatManager::RTSDebugEnergy()
{
	const UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	const TArray<TObjectPtr<UEnergyComp>>& EnergyComponents = Prm->GetEnergyComponents();
	if (EnergyComponents.Num() == 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("No energy components registered."));
		return;
	}

	// Print the overview
	GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, TEXT("Debugging Energy Components:"));

	for (UEnergyComp* EnergyComp : EnergyComponents)
	{
		if (IsValid(EnergyComp))
		{
			FString ActorName = EnergyComp->GetOwner() ? EnergyComp->GetOwner()->GetName() : TEXT("Unknown Owner");
			int32 EnergySupplied = EnergyComp->GetEnergySupplied();

			FString Message = FString::Printf(TEXT("Component: %s, EnergySupplied: %d (%s)"),
			                                  *ActorName,
			                                  EnergySupplied,
			                                  EnergySupplied >= 0 ? TEXT("Supplies") : TEXT("Demands"));

			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::White, Message);
		}
	}
}

void URTSCheatManager::RTSCheatEnergy(const int32 Amount)
{
	ANomadicVehicle* HQ = FRTS_Statics::GetPlayerHq(this);
	if (not HQ)
	{
		return;
	}
	UEnergyComp* EnergyComp = HQ->GetEnergyComp();
	if (not EnergyComp)
	{
		return;
	}
	EnergyComp->UpgradeEnergy(Amount);
}

void URTSCheatManager::DebugResource()
{
}

void URTSCheatManager::RTSCheatRadixite(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	Prm->AddResource(ERTSResourceType::Resource_Radixite, Amount);
}

void URTSCheatManager::RTSCheatMetal(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	Prm->AddResource(ERTSResourceType::Resource_Metal, Amount);
}

void URTSCheatManager::RTSCheatVehicleParts(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	Prm->AddResource(ERTSResourceType::Resource_Fuel, Amount);
}

void URTSCheatManager::RTSCheatFuel(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	Prm->AddResource(ERTSResourceType::Resource_Fuel, Amount);
}

void URTSCheatManager::RTSCheatAmmo(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	Prm->AddResource(ERTSResourceType::Resource_Ammo, Amount);
}

void URTSCheatManager::RTSCheatResource(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	TArray<ERTSResourceType> Resources = {
		ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal, ERTSResourceType::Resource_VehicleParts
	};
	for (ERTSResourceType Resource : Resources)
	{
		Prm->AddResource(Resource, FMath::Abs(Amount));
	}
}

void URTSCheatManager::RTSCheatResources(const int32 Amount)
{
	RTSCheatResource(Amount);
}

void URTSCheatManager::RTSCheatStorage(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	TArray<ERTSResourceType> Resources = {
		ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal, ERTSResourceType::Resource_VehicleParts
	};
	for (ERTSResourceType Resource : Resources)
	{
		Prm->CheatAddStorage(Resource, FMath::Abs(Amount));
	}
}

void URTSCheatManager::RTSSubtractResource(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	TArray<ERTSResourceType> Resources = {
		ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal, ERTSResourceType::Resource_VehicleParts
	};

	for (ERTSResourceType Resource : Resources)
	{
		Prm->SubtractResource(Resource, FMath::Abs(Amount));
	}
}


void URTSCheatManager::RTSCheatBlueprints(const int32 Amount)
{
	UPlayerResourceManager* Prm = FRTS_Statics::GetPlayerResourceManager(this);
	if (!Prm)
	{
		return;
	}
	TArray<ERTSResourceType> Blueprints = {
		ERTSResourceType::Blueprint_Vehicle, ERTSResourceType::Blueprint_Energy,
		ERTSResourceType::Blueprint_Construction, ERTSResourceType::Blueprint_Weapon, ERTSResourceType::Blueprint_Energy
	};
	for (ERTSResourceType Blueprint : Blueprints)
	{
		Prm->AddResource(Blueprint, Amount);
	}
}

void URTSCheatManager::RTSCheatTraining(const int32 Time)
{
	UTrainingMenuManager* TrainingMenuManager = FRTS_Statics::GetTrainingMenuManager(this);
	if (TrainingMenuManager)
	{
		TrainingMenuManager->SetTrainingTimeForEachOption(Time);
	}
}

void URTSCheatManager::RTSEnableErrors(const bool bEnable)
{
	if (bEnable)
	{
		RTSFunctionLibrary::ReportError("Reset error suppression", true);
	}
}

void URTSCheatManager::RTSHideUI(const bool bEnable)
{
	UWorld* const World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	int32 ToggledCount = 0;

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* const Obj = *It;
		if (!IsValid(Obj))                 { continue; }
		if (Obj->IsTemplate())             { continue; } // skip CDOs/archetypes
		if (Obj->GetWorld() != World)      { continue; }
		if (!Obj->GetClass()->ImplementsInterface(URTSUIElement::StaticClass()))
		{
			continue;
		}

		// C++-only interface call
		if (IRTSUIElement* const UIElem = Cast<IRTSUIElement>(Obj))
		{
			UIElem->OnHideAllGameUI(bEnable);
			++ToggledCount;
		}
	}

#if !(UE_BUILD_SHIPPING)
	UE_LOG(LogTemp, Log, TEXT("RTSHideUI(%s): toggled %d IRTSUIElement(s)."),
	       bEnable ? TEXT("true") : TEXT("false"), ToggledCount);
#endif
}


void URTSCheatManager::RTSDebugEnemy() const
{
	// Get the enemy controller from the world subsystem
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	const UEnemyControllerSubsystem* Subsystem = World->GetSubsystem<UEnemyControllerSubsystem>();
	if (not IsValid(Subsystem))
	{
		return;
	}
	// Get the enemy controller
	const AEnemyController* EnemyController = Subsystem->GetEnemyController();
	if (!IsValid(EnemyController))
	{
		return;
	}
	EnemyController->DebugAllActiveFormations();
}

void URTSCheatManager::RTSDebugFormation() const
{
	
}

void URTSCheatManager::RTSDestroyBxps() const
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	// Get all actors of bxp class.
	TArray<AActor*> BxpActors;
	UGameplayStatics::GetAllActorsOfClass(World, ABuildingExpansion::StaticClass(), BxpActors);
	for (const auto EachBxp : BxpActors)
	{
		if (IsValid(EachBxp))
		{
			EachBxp->Destroy();
		}
	}
}

void URTSCheatManager::RTSOptimizeUnits(const bool bEnable) const
{
	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (!GameUnitManager)
	{
		return;
	}
	GameUnitManager->SetAllUnitOptimizationEnabled(bEnable);
}

namespace RTSCheatUnitVariants
{
	constexpr float MessageDurationSeconds = 10.0f;
	constexpr int32 MaxSubtypeValue = MAX_uint8;

	void PrintUnitVariantMessage(const FString& Message, const FColor Color)
	{
		UE_LOG(LogTemp, Log, TEXT("%s"), *Message);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, MessageDurationSeconds, Color, Message);
		}
	}

	bool GetAreUnitVariantCheatArgumentsValid(const int32 UnitType, const int32 Subtype, const int32 Variant)
	{
		if (not StaticEnum<EAllUnitType>()->IsValidEnumValue(UnitType))
		{
			PrintUnitVariantMessage(FString::Printf(TEXT("RTSCheatSetUnitVariant: %d is not an EAllUnitType."),
			                                        UnitType), FColor::Red);
			return false;
		}
		if (Subtype < 0 || Subtype > MaxSubtypeValue)
		{
			PrintUnitVariantMessage(FString::Printf(TEXT("RTSCheatSetUnitVariant: subtype %d is out of range 0..%d."),
			                                        Subtype, MaxSubtypeValue), FColor::Red);
			return false;
		}
		if (Variant < 0 || Variant >= UnitVariants::MaxVariantsPerUnit)
		{
			PrintUnitVariantMessage(FString::Printf(TEXT("RTSCheatSetUnitVariant: variant %d is out of range 0..%d."),
			                                        Variant, UnitVariants::MaxVariantsPerUnit - 1), FColor::Red);
			return false;
		}
		return true;
	}
}

void URTSCheatManager::RTSCheatSetUnitVariant(const int32 UnitType, const int32 Subtype, const int32 Variant)
{
	if (not RTSCheatUnitVariants::GetAreUnitVariantCheatArgumentsValid(UnitType, Subtype, Variant))
	{
		return;
	}
	UPlayerUnitVariantManager* PlayerUnitVariantManager = FRTS_Statics::GetPlayerUnitVariantManager(this);
	if (not PlayerUnitVariantManager)
	{
		RTSCheatUnitVariants::PrintUnitVariantMessage(
			TEXT("RTSCheatSetUnitVariant: no player unit variant manager in this world."), FColor::Red);
		return;
	}
	const FTrainingOption Unit(static_cast<EAllUnitType>(UnitType), static_cast<uint8>(Subtype));
	const EUnitVariant NewVariant = static_cast<EUnitVariant>(Variant);
	const bool bWasSet = PlayerUnitVariantManager->SetSelectedVariant(Unit, NewVariant);
	RTSCheatUnitVariants::PrintUnitVariantMessage(FString::Printf(
		TEXT("RTSCheatSetUnitVariant: %s -> %s %s (applies to units that spawn from now on)."),
		*Unit.GetTrainingName(), *Global_GetUnitVariantAsString(NewVariant),
		bWasSet ? TEXT("SET") : TEXT("REJECTED, unit type unsupported or variant not implemented")),
	                                              bWasSet ? FColor::Green : FColor::Red);
}

void URTSCheatManager::RTSCheatPrintUnitVariants()
{
	const UPlayerUnitVariantManager* PlayerUnitVariantManager = FRTS_Statics::GetPlayerUnitVariantManager(this);
	if (not PlayerUnitVariantManager)
	{
		RTSCheatUnitVariants::PrintUnitVariantMessage(
			TEXT("RTSCheatPrintUnitVariants: no player unit variant manager in this world."), FColor::Red);
		return;
	}
	FString Report = TEXT("Selected unit variants (missing units are Base):\n");
	for (const FUnitVariantSelection& Selection : PlayerUnitVariantManager->GetSelectionsForSave())
	{
		Report += FString::Printf(TEXT(" - %s -> %s\n"), *Selection.Unit.GetTrainingName(),
		                          *Global_GetUnitVariantAsString(Selection.Variant));
	}
	Report += TEXT("Units with a variant choice in the data asset:\n");
	const UUnitVariantDataAsset* VariantDataAsset = PlayerUnitVariantManager->GetVariantDataAsset();
	if (IsValid(VariantDataAsset))
	{
		for (const FTrainingOption& Unit : VariantDataAsset->GetUnitsWithChoice())
		{
			Report += FString::Printf(TEXT(" - %s: %d variants\n"), *Unit.GetTrainingName(),
			                          VariantDataAsset->GetImplementedVariantCount(Unit));
		}
	}
	RTSCheatUnitVariants::PrintUnitVariantMessage(Report, FColor::Yellow);
}
