// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "CommanderSettings.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UCommanderSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
	public:
	
	UCommanderSettings();
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Data")
	TMap<ERTSCommander, FRTSCommanderSettings> CommanderSettingsByType;
};

