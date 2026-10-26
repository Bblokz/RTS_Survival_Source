// Copyright (C) Bas Blokzijl - All rights reserved.


#include "BirdFlock.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace BirdFlockConstants
{
	constexpr float GroundTraceUp = 5000.f;
	constexpr float GroundTraceDown = 20000.f;

	// How quickly a bird follows a new ground height (per second); low values glide over dunes.
	constexpr float GroundFollowSpeed = 0.6f;

	// How quickly the bank angle follows the turn rate (per second).
	constexpr float BankFollowSpeed = 2.f;

	// Small step used to sample the tangent of the ellipse.
	constexpr float TangentSampleAngle = 0.001f;
}

ABirdFlock::ABirdFlock()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	BirdInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BirdInstances"));
	RootComponent = BirdInstances;
	BirdInstances->SetMobility(EComponentMobility::Movable);
	BirdInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BirdInstances->SetCanEverAffectNavigation(false);
	BirdInstances->SetCastShadow(true);
	BirdInstances->SetGenerateOverlapEvents(false);
}

void ABirdFlock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildFlock();
}

void ABirdFlock::BeginPlay()
{
	Super::BeginPlay();
	BuildFlock();
}

void ABirdFlock::BuildFlock()
{
	if (not GetIsValidBirdInstances())
	{
		return;
	}
	BirdInstances->ClearInstances();
	BirdInstances->SetStaticMesh(BirdMesh);
	// Must be set before adding instances so each instance gets its custom data slots.
	BirdInstances->SetNumCustomDataFloats(BirdFlockInstanceData::NumFloats);
	M_Birds.Reset();
	M_InstanceTransforms.Reset();
	if (not BirdMesh)
	{
		return;
	}

	FRandomStream Random(FlightSettings.Seed);
	const UWorld* World = GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;
	for (int32 BirdIndex = 0; BirdIndex < FlightSettings.BirdCount; ++BirdIndex)
	{
		FBirdFlockBird Bird = MakeBird(Random, BirdIndex);
		const FTransform BirdTransform = AdvanceBird(Bird, 0.f, WorldTime);
		M_Birds.Add(Bird);
		const int32 InstanceIndex = BirdInstances->AddInstance(BirdTransform, true);
		const float PlayRate = 1.f + Random.FRandRange(-FlightSettings.PlayRateVariation,
		                                               FlightSettings.PlayRateVariation);
		// Large phase range so wing beats are never in sync between birds.
		constexpr float MaxPhaseSeconds = 100.f;
		BirdInstances->SetCustomDataValue(InstanceIndex, BirdFlockInstanceData::PhaseSeconds,
		                                  Random.FRandRange(0.f, MaxPhaseSeconds));
		BirdInstances->SetCustomDataValue(InstanceIndex, BirdFlockInstanceData::PlayRate, PlayRate);
	}
	M_InstanceTransforms.SetNum(M_Birds.Num());
}

FBirdFlockBird ABirdFlock::MakeBird(FRandomStream& Random, const int32 BirdIndex) const
{
	FBirdFlockBird Bird;
	const float HalfLength = FlightSettings.FormationLength * 0.5f;
	Bird.EllipseAngle = Random.FRandRange(-HalfLength, HalfLength);
	Bird.SpeedScale = 1.f + Random.FRandRange(-FlightSettings.SpeedVariation, FlightSettings.SpeedVariation);
	Bird.LateralOffset = Random.FRandRange(-0.5f, 0.5f) * FlightSettings.FormationWidth;
	Bird.HeightOffset = Random.FRandRange(-FlightSettings.HeightSpread, FlightSettings.HeightSpread);
	Bird.DriftPhase = Random.FRandRange(0.f, UE_TWO_PI);
	Bird.Scale = Random.FRandRange(FlightSettings.MinScale, FMath::Max(FlightSettings.MinScale, FlightSettings.MaxScale));
	Bird.GroundHeight = GetActorLocation().Z;
	if (FlightSettings.bFollowTerrain && GetWorld())
	{
		const FVector Start = GetActorTransform().TransformPosition(GetEllipsePoint(Bird, Bird.EllipseAngle));
		Bird.GroundHeight = TraceGroundHeight(Start);
	}
	return Bird;
}

FVector ABirdFlock::GetEllipsePoint(const FBirdFlockBird& Bird, const float Angle) const
{
	const float SemiAxisX = Ellipse.SemiAxisX;
	const float SemiAxisY = Ellipse.SemiAxisY;
	const FVector2D Point(SemiAxisX * FMath::Cos(Angle), SemiAxisY * FMath::Sin(Angle));
	// Outward normal of the ellipse; the lateral offset keeps each bird in its own lane.
	const FVector2D Normal = FVector2D(SemiAxisY * FMath::Cos(Angle), SemiAxisX * FMath::Sin(Angle)).GetSafeNormal();
	const FVector2D Offset = Point + Normal * Bird.LateralOffset;
	return FVector(Offset.X, Offset.Y, 0.f);
}

FTransform ABirdFlock::AdvanceBird(FBirdFlockBird& Bird, const float DeltaSeconds, const float WorldTime) const
{
	// UE is X forward / Y right, so an increasing ellipse angle runs clockwise seen from above.
	const float Direction = Ellipse.bClockwise ? 1.f : -1.f;
	const FVector Tangent = GetEllipsePoint(Bird, Bird.EllipseAngle + BirdFlockConstants::TangentSampleAngle)
		- GetEllipsePoint(Bird, Bird.EllipseAngle);
	const float ArcPerAngle = FMath::Max(Tangent.Size() / BirdFlockConstants::TangentSampleAngle, KINDA_SMALL_NUMBER);
	// Constant arc speed: the angle advances slower on the long, flat sides of the ellipse.
	Bird.EllipseAngle = FMath::Fmod(
		Bird.EllipseAngle + Direction * FlightSettings.Speed * Bird.SpeedScale * DeltaSeconds / ArcPerAngle, UE_TWO_PI);

	const FTransform ActorTransform = GetActorTransform();
	const FVector LocalPoint = GetEllipsePoint(Bird, Bird.EllipseAngle);
	const FVector WorldTangent = ActorTransform.TransformVectorNoScale(Tangent * Direction);
	const float Yaw = WorldTangent.Rotation().Yaw;

	if (DeltaSeconds > 0.f && Bird.bHasPreviousYaw)
	{
		const float TurnRate = FMath::FindDeltaAngleDegrees(Bird.PreviousYaw, Yaw) / DeltaSeconds;
		// Banking into the turn: positive yaw rate turns right and positive roll drops the right wing.
		const float TargetRoll = FMath::Clamp(TurnRate * FlightSettings.BankPerTurnRate,
		                                      -FlightSettings.MaxBankDegrees, FlightSettings.MaxBankDegrees);
		Bird.Roll = FMath::FInterpTo(Bird.Roll, TargetRoll, DeltaSeconds, BirdFlockConstants::BankFollowSpeed);
	}
	Bird.PreviousYaw = Yaw;
	Bird.bHasPreviousYaw = true;

	const float Drift = FlightSettings.HeightDriftAmplitude
		* FMath::Sin(UE_TWO_PI * WorldTime / FlightSettings.HeightDriftPeriod + Bird.DriftPhase);
	FVector WorldLocation = ActorTransform.TransformPosition(LocalPoint);
	WorldLocation.Z = Bird.GroundHeight + FlightSettings.FlightHeight + Bird.HeightOffset + Drift;
	return FTransform(FRotator(0.f, Yaw, Bird.Roll), WorldLocation, FVector(Bird.Scale));
}

void ABirdFlock::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (M_Birds.IsEmpty() || not GetIsValidBirdInstances())
	{
		return;
	}
	if (FlightSettings.bFollowTerrain)
	{
		UpdateGroundHeightRoundRobin(DeltaSeconds);
	}
	const UWorld* World = GetWorld();
	const float WorldTime = World ? World->GetTimeSeconds() : 0.f;
	M_InstanceTransforms.SetNum(M_Birds.Num());
	for (int32 BirdIndex = 0; BirdIndex < M_Birds.Num(); ++BirdIndex)
	{
		M_InstanceTransforms[BirdIndex] = AdvanceBird(M_Birds[BirdIndex], DeltaSeconds, WorldTime);
	}
	BirdInstances->BatchUpdateInstancesTransforms(0, M_InstanceTransforms, true, true);
}

void ABirdFlock::UpdateGroundHeightRoundRobin(const float DeltaSeconds)
{
	if (not M_Birds.IsValidIndex(M_NextGroundTraceIndex))
	{
		M_NextGroundTraceIndex = 0;
	}
	FBirdFlockBird& Bird = M_Birds[M_NextGroundTraceIndex];
	const FVector WorldPoint = GetActorTransform().TransformPosition(GetEllipsePoint(Bird, Bird.EllipseAngle));
	const float TargetHeight = TraceGroundHeight(WorldPoint);
	// The bird is only re-traced every BirdCount frames, so scale the follow step accordingly.
	const float EffectiveDelta = DeltaSeconds * M_Birds.Num();
	Bird.GroundHeight = FMath::FInterpTo(Bird.GroundHeight, TargetHeight, EffectiveDelta,
	                                     BirdFlockConstants::GroundFollowSpeed);
	M_NextGroundTraceIndex = (M_NextGroundTraceIndex + 1) % M_Birds.Num();
}

float ABirdFlock::TraceGroundHeight(const FVector& WorldLocation) const
{
	const UWorld* World = GetWorld();
	const float FallbackHeight = GetActorLocation().Z;
	if (not World)
	{
		return FallbackHeight;
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BirdFlockGround), false, this);
	FHitResult Hit;
	const FVector Start(WorldLocation.X, WorldLocation.Y, FallbackHeight + BirdFlockConstants::GroundTraceUp);
	const FVector End(WorldLocation.X, WorldLocation.Y, FallbackHeight - BirdFlockConstants::GroundTraceDown);
	if (World->LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return Hit.ImpactPoint.Z;
	}
	return FallbackHeight;
}

bool ABirdFlock::GetIsValidBirdInstances() const
{
	if (IsValid(BirdInstances))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "BirdInstances", "GetIsValidBirdInstances", this);
	return false;
}
