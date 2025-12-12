// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"

#include "CPPWeaponsMaster.generated.h"

class UGameUnitManager;
// forward declaration
class RTS_SURVIVAL_API ACPPController;

/** todo specific damage types
UCLASS()
class UBulletDamageType : public UDamageType
{
	GENERATED_UCLASS_BODY()

	public:
	int ArmorPen;
};
*/


UCLASS()
class RTS_SURVIVAL_API ACPPWeaponsMaster : public AActorObjectsMaster
{
	GENERATED_BODY()


	/*--------------------------------- OBJECT REFS ---------------------------------*/
	
	public:
	// Index in WeaponData array of the controller
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite)
	int OldWeaponIndex;
	
	// ptr used to ref weapon stats in array in controller
	// Set in BP
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	ACPPController* PlayerController;

	protected:
	virtual void BeginPlay() override;
	
	
	UPROPERTY()
	TSubclassOf<UDamageType> DamageType;
	FDamageEvent DamageEvent;
	

	
	/*--------------------------------- Target ---------------------------------*/
	
	public:
	UPROPERTY(BlueprintReadWrite)
	/**
	 * @brief the actor currently targeted
	 * @note: This target is usually set by the weapon owner before engaging an actor
	 */
	AActor* ActorTarget;
	

};
