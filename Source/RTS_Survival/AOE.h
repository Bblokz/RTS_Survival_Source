// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AOE.generated.h"

class RTS_SURVIVAL_API AProjectileMaster;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UAOE : public UObject
{
	GENERATED_BODY()


public:
	/**
	 * @brief Create a sphere collision around the provided Location with radius equal to range.
	 * Targets all AActors of type OBJ_ENEMY and calls the HitActorFunction for each hit actor
	 * with that actor as argument.
	 * @param Instigator The actor creating the AOE.
	 * @param HitActorFunction Function-pointer to a void function that takes one AActor* as argument.
	 * @param Location The location of the AOE.
	 * @param Range The radius of the collision sweep generated.
	 */ 
	static void CreateAOEHitEnemy(
		AActor* Instigator,
		TFunction<void(AActor*)> HitActorFunction,
		const FVector Location,
		const float Range
		);

	/**
	 * @brief Create a sphere collision around the provided Location with radius equal to range.
	 * Targets all AActors of type OBJ_Player and calls the HitActorFunction for each hit actor
	 * with that actor as argument.
	 * @param Instigator The actor creating the AOE.
	 * @param HitActorFunction Function-pointer to a void function that takes one AActor* as argument.
	 * @param Location The location of the AOE.
	 * @param Range The radius of the collision sweep generated.
	 */ 
	static void CreateAOEHitPlayer(
		AActor* Instigator,
		TFunction<void(AActor*)> HitActorFunction,
		const FVector Location,
		const float Range
		);

	static void CreateTraceAOEHitEnemy(
	AActor* Instigator,
	TFunction<void(AActor*)> HitActorFunction,
	const FVector Location,
	const float Range,
	unsigned int AmountParticles,
	TArray<AActor*> ActorsToIgnore
	);

	

};
