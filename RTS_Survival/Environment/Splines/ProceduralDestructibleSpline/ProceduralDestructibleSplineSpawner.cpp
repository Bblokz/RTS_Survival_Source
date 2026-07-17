// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ProceduralDestructibleSplineSpawner.h"

#include "Components/MeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProceduralDestructibleSplineManager.h"
#include "RTS_Survival/Environment/Splines/DestructibleSpline/DestructibleSplineActor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ProceduralDestructibleSplineSpawnerConstants
{
	constexpr int32 MinimumCurveLengthSamples = 8;
	constexpr int32 MaximumCurveLengthSamples = 64;
	constexpr double MinimumSampleSpacing = 1.0;
	constexpr double BezierHandleLengthDivisor = 3.0;
	constexpr double PieceLengthSampleDivisor = 4.0;
	constexpr double PieceWidthSampleMultiplier = 0.5;

	FVector EvaluateBezier(
		const FVector& Start,
		const FVector& FirstControl,
		const FVector& SecondControl,
		const FVector& End,
		const double Alpha)
	{
		const double InverseAlpha = 1.0 - Alpha;
		return InverseAlpha * InverseAlpha * InverseAlpha * Start
			+ 3.0 * InverseAlpha * InverseAlpha * Alpha * FirstControl
			+ 3.0 * InverseAlpha * Alpha * Alpha * SecondControl
			+ Alpha * Alpha * Alpha * End;
	}

	FVector EvaluateBezierDerivative(
		const FVector& Start,
		const FVector& FirstControl,
		const FVector& SecondControl,
		const FVector& End,
		const double Alpha)
	{
		const double InverseAlpha = 1.0 - Alpha;
		return 3.0 * InverseAlpha * InverseAlpha * (FirstControl - Start)
			+ 6.0 * InverseAlpha * Alpha * (SecondControl - FirstControl)
			+ 3.0 * Alpha * Alpha * (End - SecondControl);
	}

	double EstimateBezierLength(
		const FVector& Start,
		const FVector& FirstControl,
		const FVector& SecondControl,
		const FVector& End,
		const int32 NumSamples)
	{
		double CurveLength = 0.0;
		FVector PreviousPoint = Start;
		for (int32 SampleIndex = 1; SampleIndex <= NumSamples; ++SampleIndex)
		{
			const double Alpha = static_cast<double>(SampleIndex) / NumSamples;
			const FVector SamplePoint = EvaluateBezier(Start, FirstControl, SecondControl, End, Alpha);
			CurveLength += FVector::Distance(PreviousPoint, SamplePoint);
			PreviousPoint = SamplePoint;
		}
		return CurveLength;
	}
}

UProceduralDestructibleSplineSpawner::UProceduralDestructibleSplineSpawner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UProceduralDestructibleSplineSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* OwningActor = GetOwner())
	{
		OwningActor->OnDestroyed.AddUniqueDynamic(
			this, &UProceduralDestructibleSplineSpawner::HandleOwningActorDestroyed);
	}

	if (bM_IsInitialized)
	{
		RegisterWithManager();
	}
}

void UProceduralDestructibleSplineSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bM_IsShuttingDown = true;
	if (AActor* OwningActor = GetOwner())
	{
		OwningActor->OnDestroyed.RemoveDynamic(
			this, &UProceduralDestructibleSplineSpawner::HandleOwningActorDestroyed);
	}

	const bool bShouldCollapseConnections = EndPlayReason == EEndPlayReason::Destroyed;
	UnregisterFromManager(bShouldCollapseConnections);
	M_SocketMesh.Reset();
	M_EligibleSocketNames.Empty();
	M_UsedSocketNames.Empty();
	bM_IsInitialized = false;

	Super::EndPlay(EndPlayReason);
}

void UProceduralDestructibleSplineSpawner::InitProceduralDestructibleSplineSpawner(UMeshComponent* SocketMesh)
{
	if (not IsValid(SocketMesh))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("SocketMesh"),
			TEXT("InitProceduralDestructibleSplineSpawner"),
			this);
		return;
	}

	if (bM_IsRegistered)
	{
		UnregisterFromManager(true);
	}

	bM_IsShuttingDown = false;
	M_SocketMesh = SocketMesh;
	M_UsedSocketNames.Empty();
	M_NumOwnConnections = 0;
	bM_IsInitialized = true;
	CacheEligibleSockets();
	if (M_EligibleSocketNames.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ProceduralDestructibleSplineSpawner: no mesh sockets contain filter '")
			+ Settings.SocketFilter + TEXT("'.\n component: ") + GetName());
	}
	if (GetMaximumOwnConnections() > 0 && Settings.DestructibleSplineClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ProceduralDestructibleSplineSpawner: DestructibleSplineClass is required when ")
			TEXT("MaxConnectionsOfOwnSpline is greater than zero.\n component: ") + GetName());
	}
	RegisterWithManager();
}

void UProceduralDestructibleSplineSpawner::HandleOwningActorDestroyed(AActor* /*DestroyedActor*/)
{
	OnOwningActorDies();
}

void UProceduralDestructibleSplineSpawner::OnOwningActorDies()
{
	if (bM_IsShuttingDown)
	{
		return;
	}
	bM_IsShuttingDown = true;
	UnregisterFromManager(true);
}

void UProceduralDestructibleSplineSpawner::CacheEligibleSockets()
{
	M_EligibleSocketNames.Empty();
	if (not GetIsValidSocketMesh())
	{
		return;
	}

	UMeshComponent* SocketMesh = M_SocketMesh.Get();
	for (const FName SocketName : SocketMesh->GetAllSocketNames())
	{
		if (SocketName.ToString().Contains(Settings.SocketFilter))
		{
			M_EligibleSocketNames.AddUnique(SocketName);
		}
	}
	M_EligibleSocketNames.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});
}

void UProceduralDestructibleSplineSpawner::RegisterWithManager()
{
	if (bM_IsRegistered || bM_IsShuttingDown || not bM_IsInitialized)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World) || not World->IsGameWorld())
	{
		return;
	}

	UProceduralDestructibleSplineManager* Manager =
		World->GetSubsystem<UProceduralDestructibleSplineManager>();
	if (not IsValid(Manager))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_Manager"),
			TEXT("RegisterWithManager"),
			this);
		return;
	}

	Manager->RegisterSpawner(this);
}

void UProceduralDestructibleSplineSpawner::UnregisterFromManager(const bool bCollapseConnections)
{
	if (not bM_IsRegistered)
	{
		return;
	}

	bM_IsRegistered = false;
	if (not GetIsValidManager())
	{
		M_Manager.Reset();
		return;
	}
	M_Manager->UnregisterSpawner(this, bCollapseConnections);
	M_Manager.Reset();
}

bool UProceduralDestructibleSplineSpawner::GetIsValidSocketMesh() const
{
	if (M_SocketMesh.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_SocketMesh"),
		TEXT("GetIsValidSocketMesh"),
		this);
	return false;
}

bool UProceduralDestructibleSplineSpawner::GetIsValidManager() const
{
	if (M_Manager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_Manager"),
		TEXT("GetIsValidManager"),
		this);
	return false;
}

bool UProceduralDestructibleSplineSpawner::GetIsInitialized() const
{
	return bM_IsInitialized && not bM_IsShuttingDown && GetIsValidSocketMesh();
}

bool UProceduralDestructibleSplineSpawner::GetIsSocketAvailable(const FName SocketName) const
{
	return M_EligibleSocketNames.Contains(SocketName) && not M_UsedSocketNames.Contains(SocketName);
}

bool UProceduralDestructibleSplineSpawner::ReserveSocket(const FName SocketName)
{
	if (not GetIsSocketAvailable(SocketName))
	{
		return false;
	}
	M_UsedSocketNames.Add(SocketName);
	return true;
}

void UProceduralDestructibleSplineSpawner::ReleaseSocket(const FName SocketName)
{
	M_UsedSocketNames.Remove(SocketName);
}

bool UProceduralDestructibleSplineSpawner::TryGetSocketTransform(
	const FName SocketName,
	FTransform& OutSocketTransform) const
{
	if (not M_EligibleSocketNames.Contains(SocketName) || not GetIsValidSocketMesh())
	{
		return false;
	}

	OutSocketTransform = M_SocketMesh->GetSocketTransform(SocketName, ERelativeTransformSpace::RTS_World);
	return true;
}

int32 UProceduralDestructibleSplineSpawner::GetMaximumOwnConnections() const
{
	return FMath::Max(0, Settings.MaxConnectionsOfOwnSpline);
}

float UProceduralDestructibleSplineSpawner::GetConnectionRange() const
{
	return FMath::Max(0.f, Settings.Range);
}

float UProceduralDestructibleSplineSpawner::GetMaximumDepartureYaw() const
{
	return FMath::Clamp(Settings.MaxDegreesRefuseConnection, 0.f, 360.f);
}

ADestructibleSplineActor* UProceduralDestructibleSplineSpawner::SpawnConnectionSpline(
	const FName OwnSocket,
	const UProceduralDestructibleSplineSpawner& OtherSpawner,
	const FName OtherSocket) const
{
	FTransform StartTransform;
	FTransform EndTransform;
	if (not TryGetSocketTransform(OwnSocket, StartTransform)
		|| not OtherSpawner.TryGetSocketTransform(OtherSocket, EndTransform))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World) || Settings.DestructibleSplineClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ProceduralDestructibleSplineSpawner: cannot spawn a connection because its world or ")
			TEXT("DestructibleSplineClass is invalid.\n component: ") + GetName());
		return nullptr;
	}

	ADestructibleSplineActor* SplineActor = World->SpawnActorDeferred<ADestructibleSplineActor>(
		Settings.DestructibleSplineClass,
		FTransform::Identity,
		GetOwner(),
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(SplineActor))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ProceduralDestructibleSplineSpawner: failed to spawn the configured destructible spline.\n ")
			TEXT("component: ") + GetName());
		return nullptr;
	}

	if (not ConfigureConnectionSpline(*SplineActor, StartTransform, EndTransform))
	{
		SplineActor->Destroy();
		return nullptr;
	}

	if (not FinishConnectionSplineSpawn(SplineActor, StartTransform, EndTransform))
	{
		if (IsValid(SplineActor))
		{
			SplineActor->Destroy();
		}
		return nullptr;
	}
	return SplineActor;
}

bool UProceduralDestructibleSplineSpawner::FinishConnectionSplineSpawn(
	ADestructibleSplineActor* SplineActor,
	const FTransform& StartTransform,
	const FTransform& EndTransform) const
{
	if (not IsValid(SplineActor))
	{
		return false;
	}
	SplineActor->FinishSpawning(FTransform::Identity);
	if (not IsValid(SplineActor))
	{
		return false;
	}
	if (not ConfigureConnectionSpline(*SplineActor, StartTransform, EndTransform))
	{
		return false;
	}
	SplineActor->RebuildSpline();
	if (SplineActor->GetNumRemainingPieces() <= 0)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ProceduralDestructibleSplineSpawner: the configured spline class built no pieces. ")
			TEXT("Ensure its PieceMesh is assigned.\n actor: ") + SplineActor->GetName());
		return false;
	}
	return true;
}

bool UProceduralDestructibleSplineSpawner::ConfigureConnectionSpline(
	ADestructibleSplineActor& SplineActor,
	const FTransform& StartTransform,
	const FTransform& EndTransform) const
{
	if (not IsValid(SplineActor.Spline))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ProceduralDestructibleSplineSpawner: spawned spline actor has no valid spline component.\n ")
			TEXT("actor: ") + SplineActor.GetName());
		return false;
	}

	TArray<FVector> WorldPoints;
	TArray<FVector> WorldTangents;
	BuildConnectionCurve(StartTransform, EndTransform, WorldPoints, WorldTangents);
	if (WorldPoints.Num() < 2 || WorldPoints.Num() != WorldTangents.Num())
	{
		return false;
	}

	SplineActor.Spline->ClearSplinePoints(false);
	for (int32 PointIndex = 0; PointIndex < WorldPoints.Num(); ++PointIndex)
	{
		SplineActor.Spline->AddSplinePoint(
			WorldPoints[PointIndex], ESplineCoordinateSpace::World, false);
		SplineActor.Spline->SetSplinePointType(PointIndex, ESplinePointType::CurveCustomTangent, false);
		SplineActor.Spline->SetTangentsAtSplinePoint(
			PointIndex,
			WorldTangents[PointIndex],
			WorldTangents[PointIndex],
			ESplineCoordinateSpace::World,
			false);
	}
	SplineActor.Spline->UpdateSpline();
	return true;
}

void UProceduralDestructibleSplineSpawner::BuildConnectionCurve(
	const FTransform& StartTransform,
	const FTransform& EndTransform,
	TArray<FVector>& OutWorldPoints,
	TArray<FVector>& OutWorldTangents) const
{
	using namespace ProceduralDestructibleSplineSpawnerConstants;
	OutWorldPoints.Empty();
	OutWorldTangents.Empty();

	const FVector Start = StartTransform.GetLocation();
	const FVector End = EndTransform.GetLocation();
	const FVector ConnectionDelta = End - Start;
	const double ConnectionDistance = ConnectionDelta.Size();
	if (ConnectionDistance <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector DirectPath = ConnectionDelta / ConnectionDistance;
	const double Curviness = FMath::Clamp(static_cast<double>(Settings.Curviness), 0.0, 1.0);
	FVector StartDirection = FMath::Lerp(
		DirectPath, StartTransform.GetUnitAxis(EAxis::X), Curviness).GetSafeNormal();
	FVector EndDirection = FMath::Lerp(
		DirectPath, -EndTransform.GetUnitAxis(EAxis::X), Curviness).GetSafeNormal();
	StartDirection = StartDirection.IsNearlyZero() ? DirectPath : StartDirection;
	EndDirection = EndDirection.IsNearlyZero() ? DirectPath : EndDirection;

	const double HandleLength = ConnectionDistance / BezierHandleLengthDivisor;
	const FVector FirstControl = Start + StartDirection * HandleLength;
	const FVector SecondControl = End - EndDirection * HandleLength;
	const double PieceSizeX = FMath::Max(1.0, static_cast<double>(Settings.SplinePieceSize.X));
	const double PieceSizeY = FMath::Max(1.0, static_cast<double>(Settings.SplinePieceSize.Y));
	const double SampleSpacing = FMath::Max(
		MinimumSampleSpacing,
		FMath::Min(PieceSizeX / PieceLengthSampleDivisor, PieceSizeY * PieceWidthSampleMultiplier));
	const int32 NumLengthSamples = FMath::Clamp(
		FMath::CeilToInt32(ConnectionDistance / SampleSpacing),
		MinimumCurveLengthSamples,
		MaximumCurveLengthSamples);

	const double CurveLength = EstimateBezierLength(
		Start, FirstControl, SecondControl, End, NumLengthSamples);
	const int32 NumPieces = FMath::Max(1, FMath::CeilToInt32(CurveLength / PieceSizeX));
	OutWorldPoints.Reserve(NumPieces + 1);
	OutWorldTangents.Reserve(NumPieces + 1);
	for (int32 PointIndex = 0; PointIndex <= NumPieces; ++PointIndex)
	{
		const double Alpha = static_cast<double>(PointIndex) / NumPieces;
		OutWorldPoints.Add(EvaluateBezier(Start, FirstControl, SecondControl, End, Alpha));
		OutWorldTangents.Add(
			EvaluateBezierDerivative(Start, FirstControl, SecondControl, End, Alpha) / NumPieces);
	}
}
