// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/Units/Squads/SquadUnitHpComp/SquadUnitHealthComponent.h"
#include "HpCharacterObjectsMaster.generated.h"

class UGameUnitManager;
class RTS_SURVIVAL_API UHealthComponent;
class RTS_SURVIVAL_API URTSComponent;
class RTS_SURVIVAL_API USelectionComponent;

DECLARE_MULTICAST_DELEGATE(FOnUnitDies);


/**
 * @note SET IN BP
 * @note ON HEALTH COMPONENT : SetMaxHealth, SetArmor
 * @note ON RTS COMPONENT : SetOwningPlayer, SetUnitType
 * @note ON SELECTION COMPONENT : InitSelectionComponent
 */ 
UCLASS()
class RTS_SURVIVAL_API ACharacterObjectsMaster : public ACharacter, public IHealthBarOwner
{
	GENERATED_BODY()

public:
	ACharacterObjectsMaster(const FObjectInitializer& ObjectInitializer);

	// Delegate called when the unit dies.
	FOnUnitDies OnUnitDies;

	
	// Destroys the actor with death animation.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void TriggerDestroyActor(const ERTSDeathType DeathType);

	// Contains Health and primitive armor values.
	UFUNCTION(BlueprintCallable)
	inline UHealthComponent* GetHealthComponent() const {return HealthComponent;};

	// Contains OwningPlayer and UnitType.
	UFUNCTION(BlueprintCallable)
	inline URTSComponent* GetRTSComponent() const {return RTSComponent;};

	// Contains references to selectionBox, decals and associated flags.
	UFUNCTION(BlueprintCallable)
	inline USelectionComponent* GetSelectionComponent() const {return SelectionComponent;};
	
	/**
	 * @brief Used to damage this HpActorObject, can use different damage types
	 * @param DamageAmount: how much damage was dealt
	 * @param DamageEvent: the type of damage that was dealt
	 * @param EventInstigator: who the damage dealt
	 * @param DamageCauser: what the damage caused
	 * @return 0, if the unit died when taking damage, 1 otherwise
	 */ 
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/**
	 * Propagates the amount of health after reaching a certain threshold.
	 * @param PercentageLeft Enum level of percentage health that is left.
	 * @param bIsHealing
	 */
	virtual void OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) override;

	/**
	 * Propagates the amount of health after reaching a certain threshold.
	 * @param PercentageLeft Enum level of percentage health that is left.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="HealthComponent")
	void Bp_OnHealthChanged(const EHealthLevel PercentageLeft);


protected:
	// Contains Health and primitive armor values.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	USquadUnitHealthComponent* HealthComponent;

	// Contains OwningPlayer and UnitType.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	URTSComponent* RTSComponent;

	// Contains references to selectionBox, decals and associated flags. 
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	USelectionComponent* SelectionComponent;

	virtual void PostInitializeComponents() override;

	/** @brief Used to call death for this unit, override in childs*/
	virtual void UnitDies(const ERTSDeathType DeathType)
	{
		OnUnitDies.Broadcast();
		Destroy();
	}

	/** @brief Called when the unit dies, to implement logic in BP like death animation and giving up cover positions. */
	UFUNCTION(BlueprintImplementableEvent)
	void UnitDiesBP();
	
	/**
	 * @brief called when the unit is hit with a weapon or damaged otherwise
	 * @param damageEvent: determines the type of damage that was dealt
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void GotHit(FDamageEvent const damageEvent);

	bool GetIsValidRTSComponent()const;
	bool GetIsValidSelectionComponent() const;
	bool GetIsSelected() const;
	bool GetIsValidHealthComponent()const;
	
};