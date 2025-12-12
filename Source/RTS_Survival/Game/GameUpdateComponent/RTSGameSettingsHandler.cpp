// Copyright (C) Bas Blokzijl - All rights reserved.
#include "RTSGameSettingsHandler.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

URTSGameSettingsHandler::URTSGameSettingsHandler()
{
}

void URTSGameSettingsHandler::OnDeselectedDecalUpdate(bool bIsUsed)
{
	// We use a lambda to capter the this pointer to bind our member function as a callaback.
	UpdateComponentsOfTypeAsync<USelectionComponent, bool>(
		this,
		// capture this to call the member function as TFUNCTION cannot be used directly (without the class instance).
		[this](const TArray<USelectionComponent*>& Components, ERTSGameSetting SettingUpdated, bool ValueUpdatedSetting)
		{
			this->UpdateSelectionComponentsCallBack(Components, SettingUpdated, ValueUpdatedSetting);
		},
		ERTSGameSetting::UseDeselectedDecals,
		bIsUsed
	);
}

void URTSGameSettingsHandler::UpdateGameSetting(ERTSGameSetting Setting, const bool bValue, float fValue, int iValue)
{
	switch (Setting)
	{
	case ERTSGameSetting::None:
		break;
	case ERTSGameSetting::UseDeselectedDecals:
		M_GameSettings.bUseDeselectedDecals = bValue;
		break;
	case ERTSGameSetting::StartRadixite:
		M_GameSettings.StartRadixite = iValue;
		break;
	case ERTSGameSetting::StartMetal:
		M_GameSettings.StartMetal = iValue;
		break;
	case ERTSGameSetting::StartVehicleParts:
		M_GameSettings.StartVehicleParts = iValue;
		break;
	case ERTSGameSetting::StartFuel:
		M_GameSettings.StartFuel = iValue;
		break;
	case ERTSGameSetting::StartAmmo:
		M_GameSettings.StartAmmo = iValue;
		break;
	default:
		RTSFunctionLibrary::ReportError("Unsupported setting type requested!"
			"\n See URTSGameSettingsHandler::UpdateGameSetting for supported types.");
		break;
	}
}


void URTSGameSettingsHandler::UpdateSelectionComponentsCallBack(
	const TArray<USelectionComponent*>& SelectionComponents,
	ERTSGameSetting SettingUpdated, bool bValue)
{
	for (auto& SelectionComponent : SelectionComponents)
	{
		if (IsValid(SelectionComponent))
		{
			SelectionComponent->SetDeselectedDecalSetting(bValue);
		}
	}
}


template <typename SettingType>
SettingType URTSGameSettingsHandler::GetGameSetting(const ERTSGameSetting Setting) const
{
	RTSFunctionLibrary::ReportError(
		"Unsupported setting type requested! \n See URTSGameSettingsHandler::GetGameSetting for supported types.");
	return SettingType();
}

// Specialization for bool
template <>
bool URTSGameSettingsHandler::GetGameSetting<bool>(const ERTSGameSetting Setting) const
{
	switch (Setting)
	{
	case ERTSGameSetting::UseDeselectedDecals:
		return M_GameSettings.bUseDeselectedDecals;
	// Add other bool settings here
	default:
		RTSFunctionLibrary::ReportError("Requested setting has no bool value!"
			"\n at function URTSGameSettingsHandler::GetGameSetting --> bool specialization");
		return false;
	}
}

// specialization for int32
template <>
int32 URTSGameSettingsHandler::GetGameSetting<int32>(const ERTSGameSetting Setting) const
{
	switch (Setting)
	{
	case ERTSGameSetting::StartRadixite:
		return M_GameSettings.StartRadixite;
	case ERTSGameSetting::StartMetal:
		return M_GameSettings.StartMetal;
	case ERTSGameSetting::StartVehicleParts:
		return M_GameSettings.StartVehicleParts;
	case ERTSGameSetting::StartFuel:
		return M_GameSettings.StartFuel;
	case ERTSGameSetting::StartAmmo:
		return M_GameSettings.StartAmmo;
	case ERTSGameSetting::StartWeaponBlueprints:
		return M_GameSettings.StartWeaponBlueprints;
	case ERTSGameSetting::StartVehicleBlueprints:
		return M_GameSettings.StartVehicleBlueprints;
	case ERTSGameSetting::StartBlueprintsStorage:
		return M_GameSettings.StartBlueprintsStorage;
		case ERTSGameSetting::StartEnergyBlueprints:
			return M_GameSettings.StartEnergyBlueprints;
	case ERTSGameSetting::StartConstructionBlueprints:
		return M_GameSettings.StartConstructionBlueprints;
	default:
		RTSFunctionLibrary::ReportError("Requested setting has no int32 value!"
			"\n at function URTSGameSettingsHandler::GetGameSetting --> int32 specialization");
		break;
	}
	// Return a default value if the setting is not found
	return 0;
}


template <typename ComponentType, typename SettingValue>
void URTSGameSettingsHandler::UpdateComponentsOfTypeAsync(
	UObject* WorldContextObject,
	TFunction<void(const TArray<ComponentType*>&, ERTSGameSetting, SettingValue)> Callback,
	ERTSGameSetting SettingUpdated,
	SettingValue ValueUpdatedSetting)
{
	if(IsValid(WorldContextObject))
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		  [WorldContextObject, Callback, SettingUpdated, ValueUpdatedSetting]()
		  {
			  TArray<ComponentType*> Components;
			  TArray<AActor*> AllActors;
			  UGameplayStatics::GetAllActorsOfClass(WorldContextObject->GetWorld(), AActor::StaticClass(),
													AllActors);

			  for (AActor* Actor : AllActors)
			  {
				  if (RTSFunctionLibrary::RTSIsValid(Actor))
				  {
					  TArray<ComponentType*> ActorComponents;
					  Actor->GetComponents<ComponentType>(ActorComponents);
					  Components.Append(ActorComponents);
				  }
			  }

			  // Ensure updates occur on the game thread
			  AsyncTask(ENamedThreads::GameThread, [Components, Callback, SettingUpdated, ValueUpdatedSetting]()
			  {
				  Callback(Components, SettingUpdated, ValueUpdatedSetting);
			  });
		  });
	}
	else
	{
		RTSFunctionLibrary::ReportError("Invalid WorldContextObject provided in UpdateComponentsOfTypeAsync");
	}

}
