// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FieldConstructionAbilityComponent.generated.h"

class AFieldConstruction;
enum class EFieldConstructionType : uint8;

USTRUCT(Blueprintable)
struct FFieldConstructionAbilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EFieldConstructionType FieldConstructionType;
	
	// Attempts to add the abilty to this index of the Unit's Ability Array.
	// Reverts to first empty index if negative or already occupied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;

	// How long the ability is on cooldown after use. (does not affect behaviour duration)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cooldown = 5;

	// To spawn to actor to construct.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AFieldConstruction> FieldConstructionClass;
	
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UFieldConstructionAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UFieldConstructionAbilityComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

};
