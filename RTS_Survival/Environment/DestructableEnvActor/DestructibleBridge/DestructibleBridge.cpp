// Copyright (C) Bas Blokzijl - All rights reserved.


#include "DestructibleBridge.h"

#include "Engine/World.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"


ADestructibleBridge::ADestructibleBridge(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bCanEverTick = false;
}

void ADestructibleBridge::BeginPlay()
{
	Super::BeginPlay();
}

void ADestructibleBridge::KillActorsOnBridge(UMeshComponent* BridgeMesh)
{
	if (not IsValid(BridgeMesh))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const FBoxSphereBounds MeshBounds = BridgeMesh->Bounds;
	const FVector TraceStart = MeshBounds.Origin;
	const FQuat TraceRotation = BridgeMesh->GetComponentQuat();
	const FCollisionShape CollisionShape = FCollisionShape::MakeBox(MeshBounds.BoxExtent);
	constexpr ECollisionChannel TraceChannel = ECC_WorldDynamic;

	FCollisionQueryParams QueryParams(SCENEQUERY_STAT(KillActorsOnBridge), false, this);
	QueryParams.AddIgnoredActor(this);

	FTraceDelegate TraceDelegate;
	TraceDelegate.BindUObject(this, &ADestructibleBridge::HandleAsyncBridgeSweepComplete);

	World->AsyncSweepByChannel(
		EAsyncTraceType::Multi,
		TraceStart,
		TraceStart,
		TraceRotation,
		TraceChannel,
		CollisionShape,
		QueryParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);
}

void ADestructibleBridge::HandleAsyncBridgeSweepComplete(const FTraceHandle& /*TraceHandle*/,
                                                         FTraceDatum& TraceDatum)
{
	if (TraceDatum.OutHits.Num() == 0)
	{
		return;
	}

	TSet<AActor*> ActorsToKill;
	for (const FHitResult& HitResult : TraceDatum.OutHits)
	{
		UPrimitiveComponent* HitComponent = HitResult.GetComponent();
		AActor* HitActor = HitResult.GetActor();
		if (not IsValid(HitComponent) || not IsValid(HitActor) || HitActor == this)
		{
			continue;
		}

		const ECollisionChannel ObjectChannel = HitComponent->GetCollisionObjectType();
		const bool bIsRelevantChannel =
			   ObjectChannel == ECC_Pawn
			|| ObjectChannel == ECC_PhysicsBody
			|| ObjectChannel == ECC_Destructible
			|| ObjectChannel == COLLISION_OBJ_PLAYER
			|| ObjectChannel == COLLISION_OBJ_ENEMY;

		if (not bIsRelevantChannel)
		{
			continue;
		}

		ActorsToKill.Add(HitActor);
	}

	for (AActor* ActorToKill : ActorsToKill)
	{
		TriggerKillOnActor(ActorToKill);
	}
}

void ADestructibleBridge::TriggerKillOnActor(AActor* ActorToKill) const
{
	if (not IsValid(ActorToKill))
	{
		return;
	}

	if (AHPActorObjectsMaster* ActorWithHealth = Cast<AHPActorObjectsMaster>(ActorToKill))
	{
		ActorWithHealth->TriggerDestroyActor(ERTSDeathType::Kinetic);
		return;
	}

	ActorToKill->Destroy();
}
