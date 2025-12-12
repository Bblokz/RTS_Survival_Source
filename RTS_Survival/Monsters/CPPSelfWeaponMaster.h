// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DamageEvents.h"
#include "CPPSelfWeaponMaster.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RTS_SURVIVAL_API UCPPSelfWeaponMaster : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCPPSelfWeaponMaster();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	/*--------------------------------- DAMAGE ---------------------------------*/
	
	protected:
	/** @brief average damage dealt by this creature's weapon */
	UPROPERTY()
	float Damage;

	/** @brief amount of millimeters in armor that this creature's weapon can penetrate */
	UPROPERTY()
	int ArmorPen;
	
	UPROPERTY()
	TSubclassOf<UDamageType> DamageType;
	FDamageEvent DamageEvent;
	
	/**
	* @brief calculates how much damage will be dealt
	* @param AverageDamage: the average damage dealt by the weapon
	* @param DamageFluxPercentage: percentage of damage flux, determines how much randomness is added
	* @return: The amount of damage to deal
	*/ 
	float CalculateDamageDealt(float AverageDamage, int DamageFluxPercentage);
	
	/*--------------------------------- Target ---------------------------------*/
	
	public:
	UPROPERTY(BlueprintReadWrite)
	/**
	* @brief the actor currently targeted
	* @note: This target is usually set by the weapon owner before engaging an actor
	*/
	AActor* ActorTarget;

	UFUNCTION(BlueprintCallable)
	/** @brief sets the weapon's values
	 * @param averageDamage: the average damage dealt by this weapon
	 * @param armorPen: the amount of armor in mm this weapon can punch through
	 */ 
	void SetWeaponProperties(float averageDamage, int armorPenetration);
	
};
