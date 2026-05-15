// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MutatorSettings.generated.h"

class UBehaviour;

UENUM(BlueprintType)
enum class EMutatorClass : uint8
{
	Squad,
	ArmoredCar,
	LightTank,
	MediumTank,
	HeavyTank,
	TankDestroyer
};

USTRUCT(BlueprintType)
struct FMutatorClassSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Mutations")
	TArray<TSubclassOf<UBehaviour>> SquadMutators;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Mutations")
	TArray<TSubclassOf<UBehaviour>> ArmoredCarMutators;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Mutations")
	TArray<TSubclassOf<UBehaviour>> LightTankMutators;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Mutations")
	TArray<TSubclassOf<UBehaviour>> MediumTankMutators;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Mutations")
	TArray<TSubclassOf<UBehaviour>> HeavyTankMutators;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Mutations")
	TArray<TSubclassOf<UBehaviour>> TankDestroyerMutators;
};

/**
 * @brief Project settings used by enemy behaviour components to pick optional mutation behaviours at spawn time.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Mutators"))
class RTS_SURVIVAL_API UMutatorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMutatorSettings();

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Mutations")
	FMutatorClassSettings Mutators;

	static const UMutatorSettings* Get()
	{
		return GetDefault<UMutatorSettings>();
	}

	const TArray<TSubclassOf<UBehaviour>>& GetMutatorsForClass(const EMutatorClass MutatorClass) const;
};
