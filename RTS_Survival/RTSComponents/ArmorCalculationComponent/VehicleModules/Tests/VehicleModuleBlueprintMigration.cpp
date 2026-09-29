// Temporary editor migration helper for the tracked tank Blueprint.

#if WITH_EDITOR

#include "K2Node_MakeStruct.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleTypes.h"
#include "UObject/SavePackage.h"

namespace VehicleModuleBlueprintMigration
{
	void RefreshTrackedTankSetupNodes()
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr,
			TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/RTS_BP_TrackedTankMaster"));
		if (not IsValid(Blueprint))
		{
			UE_LOG(LogTemp, Error, TEXT("VehicleModuleBlueprintMigration: tracked tank Blueprint failed to load."));
			return;
		}

		TArray<UK2Node_MakeStruct*> MakeStructNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, MakeStructNodes);
		int32 RemovedModuleIdPins = 0;
		for (UK2Node_MakeStruct* Node : MakeStructNodes)
		{
			if (Node->StructType != FVehicleModuleSetup::StaticStruct())
			{
				continue;
			}
			for (int32 PinIndex = Node->Pins.Num() - 1; PinIndex >= 0; --PinIndex)
			{
				UEdGraphPin* Pin = Node->Pins[PinIndex];
				if (Pin->PinName != TEXT("ModuleId"))
				{
					continue;
				}
				if (not Pin->LinkedTo.IsEmpty())
				{
					UE_LOG(LogTemp, Error, TEXT("VehicleModuleBlueprintMigration: ModuleId pin is connected; asset unchanged."));
					return;
				}
				Node->RemovePin(Pin);
				++RemovedModuleIdPins;
			}
		}

		if (RemovedModuleIdPins == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("VehicleModuleBlueprintMigration: no obsolete ModuleId pins found; asset unchanged."));
			return;
		}

		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status != BS_UpToDate)
		{
			UE_LOG(LogTemp, Error, TEXT("VehicleModuleBlueprintMigration: Blueprint has compile warnings or errors; asset unchanged."));
			return;
		}

		UPackage* Package = Blueprint->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (not UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs))
		{
			UE_LOG(LogTemp, Error, TEXT("VehicleModuleBlueprintMigration: failed to save tracked tank Blueprint."));
			return;
		}

		UE_LOG(LogTemp, Display, TEXT("VehicleModuleBlueprintMigration: removed %d obsolete ID pins and saved %s."),
			RemovedModuleIdPins, *Filename);
	}

	FAutoConsoleCommand RefreshTrackedTankSetupCommand(
		TEXT("Codex.RefreshTrackedTankModuleSetup"),
		TEXT("Refresh obsolete module setup pins on the tracked tank Blueprint."),
		FConsoleCommandDelegate::CreateStatic(&RefreshTrackedTankSetupNodes));
}

#endif
