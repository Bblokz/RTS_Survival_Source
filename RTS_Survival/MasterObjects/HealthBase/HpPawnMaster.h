// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "HpPawnMaster.generated.h"

enum class ERTSDeathType : uint8;
struct FCollapseFX;
struct FDestroySpawnActorsParameters;
class UFowComp;
enum class ETargetPreference : uint8;
class RTS_SURVIVAL_API UHealthComponent;
class RTS_SURVIVAL_API URTSComponent;

DECLARE_MULTICAST_DELEGATE(FOnUnitDies);
/**
 * @note SET IN BP
 * @note ON HEALTH COMPONENT : SetMaxHealth, SetArmor
 * @note ON RTS COMPONENT : SetOwningPlayer, SetUnitType
 */
UCLASS()
class RTS_SURVIVAL_API AHpPawnMaster : public APawn, public IHealthBarOwner
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AHpPawnMaster(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void TakeFatalDamage();
	// Destroys the actor with death animation.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void TriggerDestroyActor(const ERTSDeathType DeathType);

	// Delegate called when the unit dies.
	FOnUnitDies OnUnitDies;

	// Contains Health and primitive armor values.
	UFUNCTION(BlueprintCallable)
	inline UHealthComponent* GetHealthComponent() const { return HealthComponent; };

	// Contains OwningPlayer and UnitType.
	UFUNCTION(BlueprintCallable)
	inline URTSComponent* GetRTSComponent() const { return RTSComponent; };

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

	// todo remove
	ETargetPreference GetTargetPreference() const { return ThisTarget; }

protected:
	// Called to initialize components
	virtual void PostInitializeComponents() override;

	// Contains Health and primitive armor values.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	UHealthComponent* HealthComponent;

	// Contains OwningPlayer and UnitType.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	URTSComponent* RTSComponent;


	/**
	 * @brief Destroy the bxp and spawn actors around the location.
	 * @param SpawnParams Defines what actors to spawn and where.
	 * @param CollapseFX Optional FX to play.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	virtual void DestroyAndSpawnActors(
		const FDestroySpawnActorsParameters& SpawnParams,
		FCollapseFX CollapseFX);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnUnitDies();


	bool GetIsValidHealthComponent() const;
	bool GetIsValidRTSComponent() const;


	// todo remove
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	ETargetPreference ThisTarget;


	/** @brief Used to call death for this unit, override in childs*/
	virtual void UnitDies(const ERTSDeathType DeathType)
	{
		SetUnitDying();
		OnUnitDies.Broadcast();
		Destroy();
	}

	/**
	* @brief called when the unit is hit with a weapon or damaged otherwise
	* @param damageEvent: determines the type of damage that was dealt
	*/
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void GotHit(FDamageEvent const damageEvent);

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Checks if all references are set.
	UFUNCTION(BlueprintCallable, Category="UnitTest")
	virtual void CheckReferences();
};
