// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/RTSCommanders/RTSCommander.h"
#include "CommanderSettings.generated.h"

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "CommanderSettings"))
class RTS_SURVIVAL_API UCommanderSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
	public:
	
	UCommanderSettings();
	
	FRTSCommanderSettings GetCommanderSettingsForType(const ERTSCommander CommanderType) const;
	
	
	/** Convenience accessor. */
	static const UCommanderSettings* Get()
	{
		return GetDefault<UCommanderSettings>();
	}
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Data")
	TMap<ERTSCommander, FRTSCommanderSettings> CommanderSettingsByType;
};

