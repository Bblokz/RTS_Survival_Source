// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ReinforcementPoint.h"

#include "Async/Async.h"
#include "CollisionShape.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/Reinforcement/SquadReinforcementComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

float UReinforcementPoint::SearchForSquadsInterval = 2.5f;

UReinforcementPoint::UReinforcementPoint(): M_OwningPlayer(-1), M_ReinforcementActivationRadius(0.0f)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UReinforcementPoint::InitReinforcementPoint(UMeshComponent* InMeshComponent, const FName& InSocketName,
                                                 const int32 OwningPlayer, const float ActivationRadius,
                                                 const bool bStartEnabled)
{
	M_ReinforcementMeshComponent = InMeshComponent;
	M_ReinforcementSocketName = InSocketName;
	M_OwningPlayer = OwningPlayer;
	M_ReinforcementActivationRadius = ActivationRadius;
	SetReinforcementEnabled(bStartEnabled);
}

void UReinforcementPoint::SetReinforcementEnabled(const bool bEnable)
{
	bM_ReinforcementEnabled = bEnable;
	if (bM_ReinforcementEnabled)
	{
		StartSquadSearchTimer();
		return;
	}

	StopSquadSearchTimer();
	ClearTrackedSquadControllers();
}

bool UReinforcementPoint::GetIsReinforcementEnabled() const
{
	return bM_ReinforcementEnabled;
}

FVector UReinforcementPoint::GetReinforcementLocation(bool& bOutHasValidLocation) const
{
	bOutHasValidLocation = false;

	if (not GetIsValidReinforcementMesh())
	{
		return FVector::ZeroVector;
	}

	UMeshComponent* MeshComponent = M_ReinforcementMeshComponent.Get();
	if (not MeshComponent->DoesSocketExist(M_ReinforcementSocketName))
	{
		RTSFunctionLibrary::ReportError("Requested reinforcement socket does not exist on mesh."
			"\n Socket: " + M_ReinforcementSocketName.ToString());
		return FVector::ZeroVector;
	}

	bOutHasValidLocation = true;
	return MeshComponent->GetSocketLocation(M_ReinforcementSocketName);
}

void UReinforcementPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSquadSearchTimer();
	ClearTrackedSquadControllers();
	Super::EndPlay(EndPlayReason);
}

bool UReinforcementPoint::GetIsValidReinforcementMesh(const bool bReportIfMissing) const
{
	if (M_ReinforcementMeshComponent.IsValid())
	{
		return true;
	}
	if (bReportIfMissing && bM_ReinforcementEnabled)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_ReinforcementMeshComponent",
		                                                      "GetIsValidReinforcementMesh", GetOwner());
	}

	return false;
}


bool UReinforcementPoint::GetIsDebugEnabled() const
{
	return DeveloperSettings::Debugging::GReinforcementAbility_Compile_DebugSymbols;
}

void UReinforcementPoint::StartSquadSearchTimer()
{
	if (M_ReinforcementActivationRadius <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	RequestSquadOverlapCheck();
	World->GetTimerManager().SetTimer(M_SearchForSquadsTimerHandle, this,
	                                  &UReinforcementPoint::RequestSquadOverlapCheck,
	                                  SearchForSquadsInterval, true);
}

void UReinforcementPoint::StopSquadSearchTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_SearchForSquadsTimerHandle);
}

void UReinforcementPoint::DrawDebugStatusString(const FString& DebugText, const FVector& DrawLocation) const
{
	if (not GetIsDebugEnabled())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		DrawDebugString(World, DrawLocation, DebugText, nullptr, FColor::Green, 2.5f, true);
	}
}

void UReinforcementPoint::RequestSquadOverlapCheck()
{
	if (not bM_ReinforcementEnabled)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		RTSFunctionLibrary::ReportError("Reinforcement point has no valid owner for overlap query.");
		return;
	}

	if (M_ReinforcementActivationRadius <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	TSet<const AActor*> IgnoredActors;
	const FCollisionQueryParams QueryParams = BuildOverlapQueryParams(OwnerActor, IgnoredActors);

	const FVector OriginLocation = GetSearchOriginLocation();
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(M_ReinforcementActivationRadius);
	const ETriggerOverlapLogic OverlapLogic = (M_OwningPlayer == 1)
		                                          ? ETriggerOverlapLogic::OverlapPlayer
		                                          : ETriggerOverlapLogic::OverlapEnemy;
	const FCollisionObjectQueryParams ObjectQueryParams = BuildObjectQueryParams(OverlapLogic);

	const FTraceDelegate TraceDelegate = BuildOverlapTraceDelegate(MoveTemp(IgnoredActors));

	World->AsyncSweepByObjectType(
		EAsyncTraceType::Multi,
		OriginLocation,
		OriginLocation,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams,
		&TraceDelegate,
		/*UserData*/ 0u
	);
}

void UReinforcementPoint::HandleAsyncSquadOverlapResults(TArray<FHitResult> HitResults)
{
	if (not bM_ReinforcementEnabled)
	{
		return;
	}

	TSet<TWeakObjectPtr<ASquadController>> OverlappingControllers;
	for (const FHitResult& HitResult : HitResults)
	{
		ASquadUnit* OverlappingUnit = Cast<ASquadUnit>(HitResult.GetActor());
		if (not IsValid(OverlappingUnit))
		{
			continue;
		}
		if (OverlappingUnit->GetOwningPlayer() != M_OwningPlayer)
		{
			continue;
		}

		ASquadController* SquadController = OverlappingUnit->GetSquadControllerChecked();
		if (not IsValid(SquadController))
		{
			continue;
		}

		OverlappingControllers.Add(SquadController);
		HandleSquadControllerOverlap(SquadController);
	}

	TArray<TWeakObjectPtr<ASquadController>> ControllersToRemove;
	for (const TWeakObjectPtr<ASquadController>& TrackedController : M_TrackedSquadControllers)
	{
		if (not TrackedController.IsValid())
		{
			ControllersToRemove.Add(TrackedController);
			continue;
		}

		if (OverlappingControllers.Contains(TrackedController))
		{
			continue;
		}

		HandleSquadControllerExit(TrackedController.Get());
		ControllersToRemove.Add(TrackedController);
	}

	for (const TWeakObjectPtr<ASquadController>& ControllerToRemove : ControllersToRemove)
	{
		M_TrackedSquadControllers.Remove(ControllerToRemove);
	}
}

void UReinforcementPoint::HandleSquadControllerOverlap(ASquadController* SquadController)
{
	if (not IsValid(SquadController))
	{
		return;
	}

	if (M_TrackedSquadControllers.Contains(SquadController))
	{
		return;
	}

	TObjectPtr<USquadReinforcementComponent> ReinforcementComponent = SquadController->GetSquadReinforcementComponent();
	if (not IsValid(ReinforcementComponent))
	{
		return;
	}

	if constexpr (DeveloperSettings::Debugging::GReinforcementAbility_Compile_DebugSymbols)
	{
		const FVector DrawLocation = SquadController->GetActorLocation() + FVector::UpVector * 150.0f;
		const FString DebugString = FString("Reinforcement overlap begin - Squad: ") + SquadController->GetName();
		DrawDebugStatusString(DebugString, DrawLocation);
	}

	ReinforcementComponent->ActivateReinforcements(true, this);
	M_TrackedSquadControllers.Add(SquadController);
}

void UReinforcementPoint::HandleSquadControllerExit(ASquadController* SquadController)
{
	if (not IsValid(SquadController))
	{
		return;
	}

	TObjectPtr<USquadReinforcementComponent> ReinforcementComponent = SquadController->GetSquadReinforcementComponent();
	if (not IsValid(ReinforcementComponent))
	{
		return;
	}

	ReinforcementComponent->ActivateReinforcements(false, this);
}

void UReinforcementPoint::ClearTrackedSquadControllers()
{
	for (const TWeakObjectPtr<ASquadController>& TrackedController : M_TrackedSquadControllers)
	{
		if (not TrackedController.IsValid())
		{
			continue;
		}

		HandleSquadControllerExit(TrackedController.Get());
	}
	M_TrackedSquadControllers.Reset();
}

FVector UReinforcementPoint::GetSearchOriginLocation() const
{
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return FVector::ZeroVector;
	}

	if (GetIsValidReinforcementMesh(false))
	{
		return M_ReinforcementMeshComponent->GetComponentLocation();
	}

	return OwnerActor->GetActorLocation();
}

FCollisionQueryParams UReinforcementPoint::BuildOverlapQueryParams(AActor* OwnerActor,
                                                                    TSet<const AActor*>& OutIgnoredActors) const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTS_ReinforcementPointSweep), false, OwnerActor);
	QueryParams.AddIgnoredActor(OwnerActor);
	OutIgnoredActors.Add(OwnerActor);
	return QueryParams;
}

FTraceDelegate UReinforcementPoint::BuildOverlapTraceDelegate(TSet<const AActor*> IgnoredActors) const
{
	FTraceDelegate TraceDelegate;
	const TWeakObjectPtr<UReinforcementPoint> WeakReinforcementPoint = this;
	TraceDelegate.BindLambda(
		[WeakReinforcementPoint, IgnoredActors = MoveTemp(IgnoredActors)](
		const FTraceHandle& /*TraceHandle*/,
		FTraceDatum& TraceDatum) mutable
		{
			TArray<FHitResult> HitResults = TraceDatum.OutHits;
			HitResults.RemoveAll([&IgnoredActors](const FHitResult& Hit)
			{
				AActor* HitActor = Hit.GetActor();
				if (not IsValid(HitActor))
				{
					return true;
				}

				return IgnoredActors.Contains(HitActor);
			});
			AsyncTask(
				ENamedThreads::GameThread,
				[WeakReinforcementPoint, HitResults = MoveTemp(HitResults)]() mutable
				{
					if (not WeakReinforcementPoint.IsValid())
					{
						return;
					}

					WeakReinforcementPoint->HandleAsyncSquadOverlapResults(MoveTemp(HitResults));
				});
		});

	return TraceDelegate;
}

FCollisionObjectQueryParams UReinforcementPoint::BuildObjectQueryParams(
	const ETriggerOverlapLogic OverlapLogic) const
{
	FCollisionObjectQueryParams ObjectQueryParams;
	if (OverlapLogic == ETriggerOverlapLogic::OverlapEnemy || OverlapLogic == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	}

	if (OverlapLogic == ETriggerOverlapLogic::OverlapPlayer || OverlapLogic == ETriggerOverlapLogic::OverlapBoth)
	{
		ObjectQueryParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	}

	return ObjectQueryParams;
}
