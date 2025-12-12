// Fill out your copyright notice in the Description page of Project Settings.


#include "AOE.h"


#include "RTSCollisionTraceChannels.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Weapons/WeaponData/WeaponSystems.h"


void UAOE::CreateAOEHitEnemy(
	AActor* Instigator,
	TFunction<void(AActor*)> HitActorFunction,
	const FVector Location,
	const float Range)
{
	TArray<FHitResult> hitResults;
	const FVector startLocation = FVector(Location.X, Location.Y, Location.Z - 0.5 * Range);
	const FVector endLocation = FVector(Location.X, Location.Y, Location.Z + 0.5 * Range);
	const FQuat rot = FQuat::Identity;
	FCollisionObjectQueryParams queryObjectParams; queryObjectParams.AddObjectTypesToQuery(COLLISION_TRACE_ENEMY);
	queryObjectParams.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	const FCollisionShape shape = FCollisionShape::MakeSphere(Range);
	const FCollisionQueryParams queryCollisionParams;
	
	Instigator->GetWorld()->SweepMultiByObjectType(hitResults, startLocation, endLocation, rot, queryObjectParams,
		shape, queryCollisionParams);
	for (uint8 i = 0; i<hitResults.Num(); ++i)
	{
		// Execute received method with the hit actor as argument.
		HitActorFunction(hitResults[i].GetActor());
	}

}

void UAOE::CreateAOEHitPlayer(
	AActor* Instigator,
	TFunction<void(AActor*)> HitActorFunction,
	const FVector Location,
	const float Range)
{
	TArray<FHitResult> hitResults;
	const FVector startLocation = FVector(Location.X, Location.Y, Location.Z - 0.5 * Range);
	const FVector endLocation = FVector(Location.X, Location.Y, Location.Z + 0.5 * Range);
	const FQuat rot = FQuat::Identity;
	FCollisionObjectQueryParams queryObjectParams; queryObjectParams.AddObjectTypesToQuery(COLLISION_TRACE_PLAYER);
	const FCollisionShape shape = FCollisionShape::MakeSphere(Range);
	const FCollisionQueryParams queryCollisionParams;
	
	Instigator->GetWorld()->SweepMultiByObjectType(hitResults, startLocation, endLocation, rot, queryObjectParams,
		shape, queryCollisionParams);
	for (uint8 i = 0; i<hitResults.Num(); ++i)
	{
		// Execute received method with the hit actor as argument.
		HitActorFunction(hitResults[i].GetActor());
	}

}

void UAOE::CreateTraceAOEHitEnemy(
	AActor* Instigator,
	TFunction<void(AActor*)> HitActorFunction,
	const FVector Location,
	const float Range,
	const unsigned int AmountParticles,
	TArray<AActor*> ActorsToIgnore)
{

	FHitResult traceHit(ForceInit);
	FVector endRay; AActor* hitActor;
	FCollisionQueryParams traceParams = FCollisionQueryParams(FName(TEXT("Fire_Trace")),
	true, nullptr);
	traceParams.AddIgnoredActors(ActorsToIgnore);
	UWorld* world = Instigator->GetWorld();
	for (uint8 i =0; i< AmountParticles; ++i)
	{
		endRay = Location + UKismetMathLibrary::RandomUnitVector() * Range;
		world->LineTraceSingleByChannel(traceHit, Location, endRay,
				COLLISION_TRACE_ENEMY, traceParams);
		hitActor = traceHit.GetActor();
		if(hitActor)
		{
			HitActorFunction(hitActor);
		}
	}

}
