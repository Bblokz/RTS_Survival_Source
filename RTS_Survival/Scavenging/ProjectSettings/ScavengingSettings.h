// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ScavengingSettings.generated.h"

/**
 * 
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="ScavengingSettings"))
class RTS_SURVIVAL_API UScavengingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UScavengingSettings();

	/** Convenience accessor. */
	static const UScavengingSettings* Get()
	{
		return GetDefault<UScavengingSettings>();
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ScavReward")
	USoundBase* ScavRewardSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ScavReward")
	USoundAttenuation* ScavRewardAttenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ScavReward")
	USoundConcurrency* ScavRewardConcurrency = nullptr;
};
