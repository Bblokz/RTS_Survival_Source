// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"

#include "HPActorObjectsMaster.generated.h"

struct FCollapseFX;
struct FDestroySpawnActorsParameters;
class RTS_SURVIVAL_API UHealthComponent;
class RTS_SURVIVAL_API URTSComponent;

DECLARE_MULTICAST_DELEGATE(FOnUnitDies);
/**
 * @note SET IN BP
 * @note ON HEALTH COMPONENT : SetMaxHealth, SetArmor
 * @note ON RTS COMPONENT : SetOwningPlayer, SetUnitType
 */
UCLASS()
class RTS_SURVIVAL_API AHPActorObjectsMaster : public AActorObjectsMaster, public IHealthBarOwner
{
	GENERATED_BODY()

public:
	AHPActorObjectsMaster(const FObjectInitializer& ObjectInitializer);

	// Delegate called when the unit dies.
	FOnUnitDies OnUnitDies;
UFUNCTION(BlueprintCallable , NotBlueprintable)
	void TakeFatalDamage();

	// Destroys the actor with death animation.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void TriggerDestroyActor(const ERTSDeathType DeathType);

	// Contains Health and primitive armor values.
	UFUNCTION(BlueprintCallable)
	inline UHealthComponent* GetHealthComponent() const { return HealthComponent; };

	// Contains OwningPlayer and UnitType.
	UFUNCTION(BlueprintCallable)
	inline URTSComponent* GetRTSComponent() const { return RTSComponent; };


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
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "References")
	UHealthComponent* HealthComponent;
	bool GetIsValidHealthComponent() const;

	// Contains OwningPlayer and UnitType.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "References")
	URTSComponent* RTSComponent;
	bool GetIsValidRTSComponent() const;

	virtual void PostInitializeComponents() override;

	/** @brief Used to call death for this unit, override in childs*/
	virtual void UnitDies(const ERTSDeathType DeathType)
	{
		SetUnitDying();
		OnUnitDies.Broadcast();
		Destroy();
	}


	/**
	 * @brief Destroy the bxp and spawn actors around the location.
	 * @param SpawnParams Defines what actors to spawn and where.
	 * @param CollapseFX Optional FX to play.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	virtual void DestroyAndSpawnActors(
		const FDestroySpawnActorsParameters& SpawnParams,
		FCollapseFX CollapseFX);

	/**
	 * @brief called when the unit is hit with a weapon or damaged otherwise
	 * @param damageEvent: determines the type of damage that was dealt
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void GotHit(FDamageEvent const damageEvent);

	// Set on derived actors that do not use RTSComponent.
	bool bDoNotUseRTSComponent = false;
};
