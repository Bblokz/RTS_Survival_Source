// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/GameUI/Healthbar/HealthBarSettings/HealthBarVisibilitySettings.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoOwner/CargoOwner.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "GarrisonableBuilding.generated.h"

class UCargo;
class URTSComponent;
class UHealthComponent;

// Collision is setup in the bp of this class automatically.
/**
 * @brief Environmental building that allows squads to garrison and provides cargo management for them.
 * Handles visibility changes when squads enter or leave and forwards destruction effects to garrisoned squads.
 */
UCLASS()
class RTS_SURVIVAL_API AGarrisonableBuilding : public ADestructableEnvActor, public IHealthBarOwner,
public ICargoOwner
{
        GENERATED_BODY()

public:
        // Sets default values for this actor's properties
        AGarrisonableBuilding(const FObjectInitializer& ObjectInitializer);

        /** Percentage of max health dealt as damage to garrisoned units when this building dies. */
        static const int32 DamagePercentage;

	virtual void OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) override;

	// -----Start overwrite cargo interface-----
	virtual void OnSquadRegistered(ASquadController* SquadController) override;
	virtual void OnCargoEmpty() override;
	// -----End overwrite cargo interface-----
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;

	// If set to true the garrison will ignore allied damage from allied units to the garrison.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	bool IgnoreGarrisonAlliedDamage = false;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing);

		/**
    	* @brief Used to damage this HpActorObject, can use different damage types
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
    		AActor* DamageCauser) override final;

        virtual void OnUnitDies(const ERTSDeathType DeathType) override;

	
	// Contains Health and primitive armor values.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	UHealthComponent* HealthComponent;

	bool EnsureHealthComponentIsValid() const;

	// Contains OwningPlayer and UnitType.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	URTSComponent* RTSComponent;

	bool EnsureRTSComponentIsValid() const;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	UCargo* CargoComponent;

	bool EnsureCargoComponentIsValid() const;


private:

        void OnUnitDies_HandleGarrisonState();

        // What the health bar visibility settings were before squads entered.
        FHealthBarVisibilitySettings M_CachedVisibilityPreGarrison;
	
	void HandleAlliedDamage(const int32 PlayerAlliedTo);
	void GarissonEmptyHandleAlliedDamage() const;
	int32 LastAlliedToPlayer = -1;
	
};
