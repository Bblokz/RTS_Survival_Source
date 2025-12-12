// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Game/GameSettings/RTSGameSettings.h"
#include "RTSGameSettingsHandler.generated.h"


class USelectionComponent;

/**
 * This class is responsible for storing and updating the game settings across units.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSGameSettingsHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URTSGameSettingsHandler();

	const FRTSGameSettings& GetGameSettings() const { return M_GameSettings; }

	/**
	 * @brief Updates all selection components in the game with this new setting.
	 * @param bIsUsed Whether to use decals.
	 */
	void OnDeselectedDecalUpdate(const bool bIsUsed);

	template<typename SettingType>
	SettingType GetGameSetting(const ERTSGameSetting Setting) const;

	/**
	 * @brief Updates the provided gamesetting. Non templated as blueprints do not support templates
	 * @param Setting Enum value of the setting to update.
	 */
	void UpdateGameSetting(ERTSGameSetting Setting, const bool bValue=false, float fValue=0, int iValue =0);


private:
	/**
	 * Updates all provided components with the new setting.
	 * @param SelectionComponents The selection components in the game to update.
	 * @param SettingUpdated The setting to update.
	 * @param bValue the value of that updated setting.
	 */
	void UpdateSelectionComponentsCallBack(
		const TArray<USelectionComponent*>& SelectionComponents,
		ERTSGameSetting SettingUpdated,
		bool bValue);
	
	/**
	 * @brief Obtains all components of a specific type and updates them asynchronously.
	 * @tparam ComponentType The type of component to update.
	 * @tparam SettingValue The value to update the component with.
	 * @param WorldContextObject To get the world.
	 * @param Callback The function to call with the components.
	 * @param SettingUpdated The setting to update.
	 * @param ValueUpdatedSetting The value to update the setting with.
	 */
	template<typename ComponentType, typename SettingValue>
	static void UpdateComponentsOfTypeAsync(
		UObject* WorldContextObject,
		TFunction<void(const TArray<ComponentType*>&, ERTSGameSetting, SettingValue)> Callback,
		ERTSGameSetting SettingUpdated,
		SettingValue ValueUpdatedSetting);

	UPROPERTY()
	FRTSGameSettings M_GameSettings;
};

