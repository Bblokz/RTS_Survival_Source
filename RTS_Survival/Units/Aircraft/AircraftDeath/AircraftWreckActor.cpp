// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftWreckActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace AircraftWreckPrivate
{
	// The explosion kicks the pieces outward once Chaos has released them from their clusters.
	constexpr float ScatterDelaySeconds = 0.05f;
	// Pieces leave the ground a little: a crash throws them up and forward, not down into the ground.
	constexpr float UpwardScatterShare = 0.3f;
	constexpr float ScatterRadius = 2500.f;
	// Pieces tumble; a wreck sliding without spin looks like one solid object.
	constexpr float TumbleRadiansPerSecond = 2.5f;
	// Heavy pieces piled on a thin wing panel squeeze it through the landscape with the default iterations.
	constexpr uint8 PiecePositionIterations = 16;
	constexpr uint8 PieceVelocityIterations = 2;
	constexpr uint8 PieceProjectionIterations = 4;
	// Settled pieces stop jittering on each other instead of creeping into the ground.
	constexpr float PieceAngularDamping = 0.5f;
}

AAircraftWreckActor::AAircraftWreckActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WreckRoot"));
}

int32 AAircraftWreckActor::BreakUp(UGeometryCollection* CrashCollection, const FVector& ImpactVelocity,
                                   const float VelocityRetained, const float ScatterSpeed, const float LifeSeconds)
{
	using namespace AircraftWreckPrivate;
	SetLifeSpan(LifeSeconds);
	if (not IsValid(CrashCollection) || not CrashCollection->GetGeometryCollection().IsValid())
	{
		return 0;
	}
	M_GeometryCollection = NewObject<UGeometryCollectionComponent>(this, TEXT("WreckPieces"));
	M_GeometryCollection->SetRestCollection(CrashCollection);
	M_GeometryCollection->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	M_GeometryCollection->SetEnableDamageFromCollision(false);
	// Pieces keep the direction of flight; the downward part of the speed is spent on the ground.
	FVector PieceVelocity = ImpactVelocity * VelocityRetained;
	PieceVelocity.Z = ScatterSpeed * UpwardScatterShare;
	M_GeometryCollection->InitialVelocityType = EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined;
	M_GeometryCollection->InitialLinearVelocity = PieceVelocity;
	M_GeometryCollection->InitialAngularVelocity = FVector(TumbleRadiansPerSecond, TumbleRadiansPerSecond * 0.5f, 0.f);
	M_GeometryCollection->SetupAttachment(RootComponent);
	M_GeometryCollection->SetRelativeTransform(FTransform::Identity);
	SetupPieceCollision();
	M_GeometryCollection->RegisterComponent();
	AddInstanceComponent(M_GeometryCollection);
	// Every cluster lets go at once: the aircraft comes apart in all its pieces on impact.
	M_GeometryCollection->CrumbleActiveClusters();

	const FGeometryCollection& Collection = *CrashCollection->GetGeometryCollection();
	M_PieceCount = 0;
	for (int32 TransformIndex = 0; TransformIndex < Collection.Children.Num(); ++TransformIndex)
	{
		// Leaves with geometry; the bone transforms of a skeletal aircraft are leaves without any.
		M_PieceCount += Collection.Children[TransformIndex].IsEmpty()
			&& Collection.TransformToGeometryIndex[TransformIndex] != INDEX_NONE ? 1 : 0;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return M_PieceCount;
	}
	TWeakObjectPtr<AAircraftWreckActor> WeakThis(this);
	const float Strength = ScatterSpeed;
	FTimerHandle ScatterHandle;
	World->GetTimerManager().SetTimer(ScatterHandle, FTimerDelegate::CreateLambda([WeakThis, Strength]()
	{
		if (not WeakThis.IsValid() || not WeakThis->GetIsValidGeometryCollection())
		{
			return;
		}
		WeakThis->M_GeometryCollection->AddRadialImpulse(WeakThis->GetActorLocation(), ScatterRadius, Strength,
		                                                RIF_Linear, true);
	}), ScatterDelaySeconds, false);
	return M_PieceCount;
}

bool AAircraftWreckActor::GetIsValidGeometryCollection() const
{
	if (IsValid(M_GeometryCollection))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_GeometryCollection",
	                                                      "AAircraftWreckActor::GetIsValidGeometryCollection", this);
	return false;
}

void AAircraftWreckActor::SetupPieceCollision() const
{
	using namespace AircraftWreckPrivate;
	if (not IsValid(M_GeometryCollection))
	{
		return;
	}
	// Pieces land on the ground and on each other; units, weapons, the cursor and building previews pass through.
	M_GeometryCollection->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	M_GeometryCollection->SetCollisionObjectType(ECC_PhysicsBody);
	M_GeometryCollection->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_GeometryCollection->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	M_GeometryCollection->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	// Thin panels (wing skin) tunnel through the ground at crash speed without it.
	M_GeometryCollection->BodyInstance.bUseCCD = true;
	M_GeometryCollection->BodyInstance.SetPositionSolverIterationCount(PiecePositionIterations);
	M_GeometryCollection->BodyInstance.SetVelocitySolverIterationCount(PieceVelocityIterations);
	M_GeometryCollection->BodyInstance.SetProjectionSolverIterationCount(PieceProjectionIterations);
	M_GeometryCollection->BodyInstance.AngularDamping = PieceAngularDamping;
	M_GeometryCollection->SetCanEverAffectNavigation(false);
	M_GeometryCollection->SetGenerateOverlapEvents(false);
	M_GeometryCollection->SetReceivesDecals(false);
}
