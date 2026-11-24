// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietCargoTrain.h"

#include "Algo/BinarySearch.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace SovietCargoTrainConstants
{
	// Path points closer than this to the previous one are dropped.
	constexpr float MinPointSpacing = 1.f;
	// Long-axis fix for meshes modelled along Y: yaw -90 turns their +Y end onto +X.
	constexpr float LongAxisYFixYaw = -90.f;
}

// ---------------------------------------------------------------------------------------------------------------------
// FSovietCargoTrainPath

bool FSovietCargoTrainPath::Build(const TArray<FVector>& InPoints)
{
	Points.Reset();
	Distances.Reset();
	for (const FVector& Point : InPoints)
	{
		if (Points.Num() > 0 && FVector::Dist(Points.Last(), Point) < SovietCargoTrainConstants::MinPointSpacing)
		{
			continue;
		}
		Distances.Add(Points.Num() == 0 ? 0.f : Distances.Last() + FVector::Dist(Points.Last(), Point));
		Points.Add(Point);
	}
	return Points.Num() >= 2;
}

float FSovietCargoTrainPath::GetLength() const
{
	return Distances.Num() > 0 ? Distances.Last() : 0.f;
}

FVector FSovietCargoTrainPath::GetLocationAtDistance(const float Distance) const
{
	const int32 NumPoints = Points.Num();
	if (NumPoints < 2)
	{
		return NumPoints == 1 ? Points[0] : FVector::ZeroVector;
	}
	if (Distance <= 0.f)
	{
		// Behind the path start: straight and level along the start direction (off-map run-in).
		const FVector StartDirection = (Points[1] - Points[0]).GetSafeNormal2D();
		return Points[0] + StartDirection * Distance;
	}
	const float Length = GetLength();
	if (Distance >= Length)
	{
		const FVector EndDirection = (Points[NumPoints - 1] - Points[NumPoints - 2]).GetSafeNormal2D();
		return Points[NumPoints - 1] + EndDirection * (Distance - Length);
	}
	// First point whose distance is greater than Distance; the segment starts one before it.
	const int32 UpperIndex = Algo::UpperBound(Distances, Distance);
	const int32 SegmentStart = FMath::Clamp(UpperIndex - 1, 0, NumPoints - 2);
	const float SegmentLength = Distances[SegmentStart + 1] - Distances[SegmentStart];
	const float Alpha = SegmentLength > KINDA_SMALL_NUMBER
		                    ? (Distance - Distances[SegmentStart]) / SegmentLength
		                    : 0.f;
	return FMath::Lerp(Points[SegmentStart], Points[SegmentStart + 1], Alpha);
}

float FSovietCargoTrainPath::GetClosestDistanceToLocation(const FVector& Location) const
{
	float BestDistanceSquared = TNumericLimits<float>::Max();
	float BestPathDistance = 0.f;
	for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
	{
		const FVector ClosestPoint = FMath::ClosestPointOnSegment(Location, Points[Index], Points[Index + 1]);
		const float DistanceSquared = FVector::DistSquared(ClosestPoint, Location);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestPathDistance = Distances[Index] + FVector::Dist(Points[Index], ClosestPoint);
		}
	}
	return BestPathDistance;
}

// ---------------------------------------------------------------------------------------------------------------------
// ASovietCargoTrain

ASovietCargoTrain::ASovietCargoTrain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	M_Root->SetMobility(EComponentMobility::Movable);
	RootComponent = M_Root;
}

void ASovietCargoTrain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	switch (M_State)
	{
	case ESovietCargoTrainState::Arriving:
		Tick_Arriving(DeltaSeconds);
		return;
	case ESovietCargoTrainState::Departing:
		Tick_Departing(DeltaSeconds);
		return;
	default:
		SetActorTickEnabled(false);
		return;
	}
}

void ASovietCargoTrain::StartRun(const TArray<FVector>& WorldPathPoints, const FVector& StopFrontLocation,
                                 const TArray<UStaticMesh*>& FlatcarCargoMeshes, const int32 NumGoodsWagons)
{
	if (M_State != ESovietCargoTrainState::Idle)
	{
		RTSFunctionLibrary::ReportError(TEXT("ASovietCargoTrain::StartRun called twice on ") + GetName()
			+ TEXT("; spawn a new train for every run."));
		return;
	}
	if (not M_Path.Build(WorldPathPoints))
	{
		RTSFunctionLibrary::ReportError(TEXT("ASovietCargoTrain::StartRun on ") + GetName()
			+ TEXT(" needs at least two distinct path points (got ") + FString::FromInt(WorldPathPoints.Num())
			+ TEXT(")."));
		return;
	}
	if (not GetIsValidConsistMeshes(NumGoodsWagons))
	{
		return;
	}
	SetActorLocation(M_Path.Points[0]);
	StartRun_BuildConsist(FlatcarCargoMeshes, NumGoodsWagons);

	M_StopDistance = FMath::Clamp(M_Path.GetClosestDistanceToLocation(StopFrontLocation), 0.f, M_Path.GetLength());
	M_FrontDistance = 0.f;
	M_State = ESovietCargoTrainState::Arriving;
	ApplyConsistAtFrontDistance();
	StartEngineSound();
	SetActorTickEnabled(true);
}

void ASovietCargoTrain::HideCargo()
{
	bM_CargoHidden = true;
	for (UStaticMeshComponent* Cargo : M_CargoComponents)
	{
		if (IsValid(Cargo))
		{
			Cargo->SetVisibility(false);
		}
	}
}

void ASovietCargoTrain::Depart()
{
	if (M_State == ESovietCargoTrainState::Departing || M_State == ESovietCargoTrainState::Departed)
	{
		return;
	}
	if (M_State != ESovietCargoTrainState::Arrived)
	{
		RTSFunctionLibrary::ReportError(TEXT("ASovietCargoTrain::Depart on ") + GetName()
			+ TEXT(" before it arrived; wait for OnArrived."));
		return;
	}
	M_State = ESovietCargoTrainState::Departing;
	FadeEngineSound(true);
	SetActorTickEnabled(true);
}

bool ASovietCargoTrain::GetHasArrived() const
{
	return M_State == ESovietCargoTrainState::Arrived;
}

TArray<FVector> ASovietCargoTrain::GetCargoWorldLocations() const
{
	TArray<FVector> Locations;
	for (const UStaticMeshComponent* Cargo : M_CargoComponents)
	{
		if (IsValid(Cargo))
		{
			Locations.Add(Cargo->Bounds.Origin);
		}
	}
	return Locations;
}

void ASovietCargoTrain::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(M_EngineAudio))
	{
		M_EngineAudio->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

bool ASovietCargoTrain::GetIsValidConsistMeshes(const int32 NumGoodsWagons) const
{
	bool bIsValid = true;
	if (not IsValid(LocomotiveMesh))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("LocomotiveMesh"),
		                                                      TEXT("ASovietCargoTrain::StartRun"));
		bIsValid = false;
	}
	if (not IsValid(FlatcarMesh))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("FlatcarMesh"),
		                                                      TEXT("ASovietCargoTrain::StartRun"));
		bIsValid = false;
	}
	if (NumGoodsWagons > 0 && not IsValid(GoodsWagonMesh))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("GoodsWagonMesh"),
		                                                      TEXT("ASovietCargoTrain::StartRun"));
		bIsValid = false;
	}
	return bIsValid;
}

void ASovietCargoTrain::StartRun_BuildConsist(const TArray<UStaticMesh*>& FlatcarCargoMeshes,
                                              const int32 NumGoodsWagons)
{
	float FrontOffset = 0.f;
	AddCar(ESovietCargoTrainCarType::Locomotive, nullptr, FrontOffset);
	for (UStaticMesh* CargoMesh : FlatcarCargoMeshes)
	{
		AddCar(ESovietCargoTrainCarType::Flatcar, CargoMesh, FrontOffset);
	}
	for (int32 Index = 0; Index < NumGoodsWagons; ++Index)
	{
		AddCar(ESovietCargoTrainCarType::GoodsWagon, nullptr, FrontOffset);
	}
}

void ASovietCargoTrain::AddCar(const ESovietCargoTrainCarType CarType, UStaticMesh* CargoMesh,
                               float& InOutFrontOffset)
{
	UStaticMesh* const Mesh = GetCarMesh(CarType);
	FSovietCargoTrainCar Car;
	Car.MeshToCarFrame = GetBottomCentreAlignment(Mesh, GetCarYawOffset(CarType), Car.Length);
	Car.FrontOffset = InOutFrontOffset;
	UStaticMeshComponent* const CarComponent = CreateMeshComponent(Mesh, M_Root);
	Car.MeshComponent = CarComponent;
	M_CarComponents.Add(CarComponent);
	if (CarType == ESovietCargoTrainCarType::Flatcar && IsValid(CargoMesh))
	{
		AttachCargoToCar(Car, CargoMesh);
	}
	M_Cars.Add(Car);
	InOutFrontOffset += Car.Length + GapBetweenCars;
}

UStaticMeshComponent* ASovietCargoTrain::CreateMeshComponent(UStaticMesh* Mesh, USceneComponent* AttachParent)
{
	UStaticMeshComponent* const Component = NewObject<UStaticMeshComponent>(this);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	// Pure set dressing: no blocking, no cursor/visibility hits, no overlaps, no navmesh cut-outs, no decals.
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);
	Component->SetReceivesDecals(false);
	Component->SetupAttachment(AttachParent);
	Component->RegisterComponent();
	AddInstanceComponent(Component);
	return Component;
}

void ASovietCargoTrain::AttachCargoToCar(FSovietCargoTrainCar& Car, UStaticMesh* CargoMesh)
{
	UStaticMeshComponent* const CarComponent = Car.MeshComponent.Get();
	if (not IsValid(CarComponent))
	{
		return;
	}
	float CargoLength = 0.f;
	const FTransform CargoAlignment = GetBottomCentreAlignment(CargoMesh, CargoYawOffset, CargoLength);
	// The car frame has the flatcar's bounds bottom at z 0, so its bounds top is twice the half height.
	const float DeckHeight = FlatcarDeckHeight > 0.f
		                         ? FlatcarDeckHeight
		                         : FlatcarMesh->GetBounds().BoxExtent.Z * 2.f;
	const FTransform CargoInCarFrame = CargoAlignment * CargoOffset * FTransform(FVector(0.f, 0.f, DeckHeight));

	UStaticMeshComponent* const CargoComponent = CreateMeshComponent(CargoMesh, CarComponent);
	CargoComponent->SetRelativeTransform(CargoInCarFrame * Car.MeshToCarFrame.Inverse());
	CargoComponent->SetVisibility(not bM_CargoHidden);
	Car.CargoComponent = CargoComponent;
	M_CargoComponents.Add(CargoComponent);
}

UStaticMesh* ASovietCargoTrain::GetCarMesh(const ESovietCargoTrainCarType CarType) const
{
	switch (CarType)
	{
	case ESovietCargoTrainCarType::Locomotive:
		return LocomotiveMesh;
	case ESovietCargoTrainCarType::Flatcar:
		return FlatcarMesh;
	default:
		return GoodsWagonMesh;
	}
}

float ASovietCargoTrain::GetCarYawOffset(const ESovietCargoTrainCarType CarType) const
{
	switch (CarType)
	{
	case ESovietCargoTrainCarType::Locomotive:
		return LocomotiveYawOffset;
	case ESovietCargoTrainCarType::Flatcar:
		return FlatcarYawOffset;
	default:
		return GoodsWagonYawOffset;
	}
}

FTransform ASovietCargoTrain::GetBottomCentreAlignment(const UStaticMesh* Mesh, const float ExtraYaw,
                                                       float& OutLength)
{
	OutLength = 0.f;
	if (not IsValid(Mesh))
	{
		return FTransform::Identity;
	}
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector Centre = Bounds.Origin;
	const FVector Extent = Bounds.BoxExtent;
	const float LongAxisYaw = Extent.X >= Extent.Y ? 0.f : SovietCargoTrainConstants::LongAxisYFixYaw;
	const float Yaw = LongAxisYaw + ExtraYaw;
	const FQuat Rotation = FRotator(0.f, Yaw, 0.f).Quaternion();

	const float YawRadians = FMath::DegreesToRadians(Yaw);
	OutLength = 2.f * (FMath::Abs(FMath::Cos(YawRadians)) * Extent.X + FMath::Abs(FMath::Sin(YawRadians)) * Extent.Y);

	// Turn first, then move the turned bounds centre onto the origin and the bounds bottom onto z 0.
	const FVector RotatedCentre = Rotation.RotateVector(Centre);
	const FVector Translation(-RotatedCentre.X, -RotatedCentre.Y, -(Centre.Z - Extent.Z));
	return FTransform(Rotation, Translation);
}

FTransform ASovietCargoTrain::GetCarFrameAtDistance(const float CentreDistance, const float CarLength) const
{
	const float BogieOffset = BogieFraction * CarLength;
	const FVector FrontBogie = M_Path.GetLocationAtDistance(CentreDistance + BogieOffset);
	const FVector RearBogie = M_Path.GetLocationAtDistance(CentreDistance - BogieOffset);
	FVector Forward = FrontBogie - RearBogie;
	if (not Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Location = (FrontBogie + RearBogie) * 0.5f + FVector(0.f, 0.f, RailHeightOffset);
	return FTransform(Forward.Rotation(), Location);
}

void ASovietCargoTrain::ApplyConsistAtFrontDistance() const
{
	for (const FSovietCargoTrainCar& Car : M_Cars)
	{
		UStaticMeshComponent* const CarComponent = Car.MeshComponent.Get();
		if (not IsValid(CarComponent))
		{
			continue;
		}
		const float CentreDistance = M_FrontDistance - Car.FrontOffset - Car.Length * 0.5f;
		CarComponent->SetWorldTransform(Car.MeshToCarFrame * GetCarFrameAtDistance(CentreDistance, Car.Length));
		if (not bHideCarsBeyondPathStart)
		{
			continue;
		}
		const bool bCarVisible = CentreDistance >= 0.f;
		CarComponent->SetVisibility(bCarVisible);
		if (UStaticMeshComponent* const CargoComponent = Car.CargoComponent.Get())
		{
			CargoComponent->SetVisibility(bCarVisible && not bM_CargoHidden);
		}
	}
}

float ASovietCargoTrain::GetEasedSpeed() const
{
	// Constant deceleration that brings CruiseSpeed to zero over EaseDistance: v = sqrt(2 a s).
	const float Deceleration = CruiseSpeed * CruiseSpeed / (2.f * FMath::Max(EaseDistance, 1.f));
	// Arriving: distance still to the stop (brake). Departing: distance already backed out from the stop (run-up).
	const float EaseSpan = M_StopDistance - M_FrontDistance;
	const float SpeedFromEase = FMath::Sqrt(2.f * Deceleration * FMath::Max(EaseSpan, 0.f));
	return FMath::Clamp(SpeedFromEase, FMath::Min(MinEaseSpeed, CruiseSpeed), CruiseSpeed);
}

void ASovietCargoTrain::Tick_Arriving(const float DeltaSeconds)
{
	const float Remaining = M_StopDistance - M_FrontDistance;
	const float Step = GetEasedSpeed() * DeltaSeconds;
	if (Step < Remaining)
	{
		M_FrontDistance += Step;
		ApplyConsistAtFrontDistance();
		return;
	}
	M_FrontDistance = M_StopDistance;
	ApplyConsistAtFrontDistance();
	M_State = ESovietCargoTrainState::Arrived;
	SetActorTickEnabled(false);
	FadeEngineSound(false);
	// Last statement: listeners may call Depart or Destroy on this train.
	OnArrived.Broadcast(this);
}

void ASovietCargoTrain::Tick_Departing(const float DeltaSeconds)
{
	const float Step = GetEasedSpeed() * DeltaSeconds;
	if (Step < M_FrontDistance)
	{
		M_FrontDistance -= Step;
		ApplyConsistAtFrontDistance();
		return;
	}
	// The locomotive front is back at the path start, so every car is behind it, off-map.
	M_FrontDistance = 0.f;
	ApplyConsistAtFrontDistance();
	M_State = ESovietCargoTrainState::Departed;
	SetActorTickEnabled(false);
	if (IsValid(M_EngineAudio))
	{
		M_EngineAudio->Stop();
	}
	OnDeparted.Broadcast(this);
	// A listener may already have destroyed the train; the actor object stays valid until garbage collection.
	if (not IsActorBeingDestroyed())
	{
		Destroy();
	}
}

void ASovietCargoTrain::StartEngineSound()
{
	if (not IsValid(EngineLoopSound) || M_Cars.Num() == 0)
	{
		return;
	}
	UStaticMeshComponent* const Locomotive = M_Cars[0].MeshComponent.Get();
	if (not IsValid(Locomotive))
	{
		return;
	}
	M_EngineAudio = UGameplayStatics::SpawnSoundAttached(
		EngineLoopSound, Locomotive, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
		/*bStopWhenAttachedToDestroyed*/ true, 1.f, 1.f, 0.f, EngineSoundAttenuation, nullptr,
		/*bAutoDestroy*/ false);
}

void ASovietCargoTrain::FadeEngineSound(const bool bFadeIn) const
{
	if (not IsValid(M_EngineAudio))
	{
		return;
	}
	if (bFadeIn)
	{
		M_EngineAudio->FadeIn(EngineSoundFadeSeconds, 1.f);
		return;
	}
	M_EngineAudio->FadeOut(EngineSoundFadeSeconds, 0.f);
}
