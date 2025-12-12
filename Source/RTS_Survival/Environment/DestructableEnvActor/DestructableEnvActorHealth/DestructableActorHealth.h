// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"

#include "DestructableActorHealth.generated.h"

class UHealthComponent;

UCLASS()
class RTS_SURVIVAL_API ADestructableActorHealth : public ADestructableEnvActor, public IHealthBarOwner
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADestructableActorHealth(const FObjectInitializer& ObjectInitializer);

	virtual void OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) override;

		/**
    	 * @note: DO NOT OVERRIDE IN CHILDS
    	 * @brief Used to damage this HpActorObject, can use different damage types. Calls UnitDies if hp <=0
    	 * and calls GotHit(). 
    	 * @param DamageAmount: how much damage was dealt
    	 * @param DamageEvent: the type of damage that was dealt
    	 * @param EventInstigator: who the damage dealt
    	 * @param DamageCauser: what the damage caused
    	 * @return 0, if the unit died when taking damage, 1 otherwise
    	 */
    	virtual float TakeDamage(
    		float DamageAmount,
    		FDamageEvent const& DamageEvent,
    		AController* EventInstigator,
    		AActor* DamageCauser) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintImplementableEvent)
	void BpOnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) ;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "References")
	UHealthComponent* HealthComponent;

	bool GetIsValidHealthComponent() const;
};
