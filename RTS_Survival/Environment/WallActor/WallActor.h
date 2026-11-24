// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "WallActor.generated.h"

class URTSComponent;
class UWallHoleDamageComponent;
class UWallTankBreachComponent;

UCLASS()
class RTS_SURVIVAL_API AWallActor : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWallActor(const FObjectInitializer& ObjectInitializer);

	
	// Contains owning player.
	UFUNCTION(BlueprintCallable)
	inline URTSComponent* GetRTSComponent() const { return RTSComponent; };
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupWallCollision(UStaticMeshComponent* WallMeshComponent,
		const bool bAffectNavMesh);

	virtual void PostInitializeComponents() override;

	/**
	 * @brief Walls tanks can break (UWallTankBreachComponent) only take damage from the other side: the Soviet walls
	 * from the human player, never from Soviet shells, splash, bombs or abilities. A shot into a breach flies through.
	 */
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	// Contains OwningPlayer and UnitType.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "References")
	URTSComponent* RTSComponent;
private:
	bool GetIsValidRTSComponent()const;

	/**
	 * @brief The player behind a hit: the projectile's shooter, else the causer's (or its owner's) RTS component, else
	 * the instigating controller's pawn.
	 * @param DamageCauser Projectile, bomb or ability actor; may be null.
	 * @param EventInstigator Controller of the attacker; may be null.
	 * @param OutPlayer Receives the attacking player.
	 * @return False when no player can be found (scripted or environment damage).
	 */
	static bool TryGetAttackingPlayer(const AActor* DamageCauser, const AController* EventInstigator, uint8& OutPlayer);
	bool GetIsDamageFromOwnSide(const AActor* DamageCauser, const AController* EventInstigator) const;
	bool GetIsShotThroughBreach(const FDamageEvent& DamageEvent) const;

	// Set on walls tanks can break: they are protected from their own side and let shots through their breaches.
	UPROPERTY()
	TObjectPtr<UWallTankBreachComponent> M_TankBreach;

	UPROPERTY()
	TObjectPtr<UWallHoleDamageComponent> M_HoleDamageForBreach;

};
