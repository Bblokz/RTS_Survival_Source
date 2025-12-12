// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoA_SmallArmsProjectiles/SoA_SmallArmsProjectilesPool.h"
#include "SoA_TankProjectilesPool/SoA_TankProjectilesPool.h"
#include "SmallArmsProjectileManager.generated.h"

struct FSoA_TankProjectilesPool;
class AProjectile;
class UNiagaraSystem;
class UNiagaraComponent;
struct FSoA_SmallArmsProjectilesPool;

UCLASS()
class RTS_SURVIVAL_API ASmallArmsProjectileManager : public AActor
{
	GENERATED_BODY()

	friend class AProjectile;

public:
	// Sets default values for this actor's properties
	ASmallArmsProjectileManager();

	/**
	 * @brief Updates the projetile pool with new data will render the projectile with niagara in the next tick.
	 * @param Position Start position of the projectile.
	 * @param EndPosition Final destination of the projectile.
	 * @param Type Type of projectile
	 * @param StartTime When the projectile was fired.
	 */
	void FireProjectile(
		const FVector& Position,
		const FVector& EndPosition,
		const int32 Type,
		const float StartTime);

	AProjectile* GetDormantTankProjectile();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitProjectileManager(UNiagaraSystem* ProjectileSystem, TSubclassOf<AProjectile> TankProjectileClass);

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString NiagaraPositionArrayName;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString NiagaraEndPositionArrayName;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString NiagaraTypeArrayName;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString NiagaraProjectileSpeedVarName;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	// Contains all the projectile data in a DOP style.
	FSoA_SmallArmsProjectilesPool M_SmallArmsProjectilePool;

	// The index of the last activated projectile.
	int32 M_LastActivatedProjectileIndex;

	// Cached World pointer.
	TWeakObjectPtr<UWorld> M_World;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_NiagaraComponent;

	bool EnsureSystemIsValid(TObjectPtr<UNiagaraSystem> System) const;
	bool EnsureWorldIsValid();
	bool GetIsValidNiagaraComponent();
	void OnPoolSaturated();

	/** @return The first available projectile index.
	 * @param bOutIsPoolSaturated if true then no projectiles are left!*/
	int32 FindDormantProjectileIndex(bool& bOutIsPoolSaturated) const;

	void SetProjectileValues(const FVector& Position,
	                         const FVector& EndPosition,
	                         const int32 Type,
	                         const float StartTime,
	                         const double LifeTime,
	                         const int32 ProjectileIndex);

	/** @brief Update the LifeTime calculations of projectiles and updates niagara with the data
	 * @see UpdateNiagaraWithData */
	void UpdateProjectiles();

	/**
	 * @brief Propagates the data needed for bullet simulation to the Niagara system.
	 * @post Set the Position and EndPosition arrays to keep track between where the bullets travel.
	 * @post Set the combined array of {Type, StartTime, LifeTime} to keep track of the type of bullet,
	 * when it was fired and how long it will live.
	 * @note Ideally would use only one aggregated array to keep track of all data,
	 * but currently unreal engine only supports certain arrays of their derived data interfaces.
	 */
	inline void UpdateNiagaraWithData();

	void Debug_ProjectilePooling(const FString& Message) const;
	void Debug_AmountProjectilesActive() const;


	// --------------------- Tank Projectile --------------------- //

	// Determines the type of projectile to spawn.
	UPROPERTY()
	TSubclassOf<AProjectile> M_TankProjectileClass;

	FSoA_TankProjectilesPool M_TankProjectilesPool;

	void OnTankProjectileDormant(const int32 Index);
};
