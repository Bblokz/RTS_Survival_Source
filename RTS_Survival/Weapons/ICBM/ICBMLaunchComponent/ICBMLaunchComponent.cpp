#include "ICBMLaunchComponent.h"

#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/ICBM/ICBMActor/ICBMActor.h"

UICBMLaunchComponent::UICBMLaunchComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UICBMLaunchComponent::BeginPlay()
{
	Super::BeginPlay();
	M_PendingLaunchRequest.M_TargetingData.InitTargetStruct(1);
}

void UICBMLaunchComponent::InitICBMComp(UMeshComponent* MeshComponent)
{
	if (not IsValid(MeshComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"MeshComponent",
			"InitICBMComp",
			this);
		return;
	}
	if (M_LaunchSettings.SocketNames.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("ICBM launch settings contain no socket names in InitICBMComp.");
		return;
	}
	if (not IsValid(M_ICBMActorClass))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_ICBMActorClass",
			"InitICBMComp",
			this);
		return;
	}

	M_OwnerMeshComponent = MeshComponent;
	M_SocketStates.Empty();
	for (const FName& SocketName : M_LaunchSettings.SocketNames)
	{
		if (not MeshComponent->DoesSocketExist(SocketName))
		{
			RTSFunctionLibrary::ReportError(
				"ICBM socket does not exist on mesh: " + SocketName.ToString());
			continue;
		}
		FICBMSocketState NewSocketState;
		NewSocketState.SocketName = SocketName;
		M_SocketStates.Add(NewSocketState);
	}

	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"OwnerActor",
			"InitICBMComp",
			this);
		return;
	}

	M_OwningRTSComponent = OwnerActor->FindComponentByClass<URTSComponent>();
	if (not GetIsValidOwningRTSComponent())
	{
		return;
	}

	M_OwningPlayer = M_OwningRTSComponent->GetOwningPlayer();
	M_PendingLaunchRequest.M_TargetingData.InitTargetStruct(M_OwningPlayer);
	StartReloadingICBMs();
}

void UICBMLaunchComponent::LaunchICBMsAtLocation(const int32 AmountToLaunch, const FVector& TargetLocation)
{
	if (AmountToLaunch <= 0)
	{
		return;
	}

	M_PendingLaunchRequest.M_TargetingData.SetTargetGround(TargetLocation);
	const int32 LaunchedAmount = LaunchReadyRockets(AmountToLaunch);
	RegisterPendingLaunch(AmountToLaunch - LaunchedAmount);
}

void UICBMLaunchComponent::LaunchICBMsAtActor(const int32 AmountToLaunch, AActor* TargetActor)
{
	if (AmountToLaunch <= 0)
	{
		return;
	}
	if (not IsValid(TargetActor))
	{
		RTSFunctionLibrary::ReportError("LaunchICBMsAtActor received invalid target actor.");
		return;
	}

	M_PendingLaunchRequest.M_TargetingData.SetTargetActor(TargetActor);
	const int32 LaunchedAmount = LaunchReadyRockets(AmountToLaunch);
	RegisterPendingLaunch(AmountToLaunch - LaunchedAmount);
}

void UICBMLaunchComponent::OnICBMReachedLaunchReady(AICBMActor* Rocket)
{
	if (not IsValid(Rocket))
	{
		return;
	}
	TryConsumePendingLaunches();
}

void UICBMLaunchComponent::StartReloadingICBMs()
{
	for (int32 SocketIndex = 0; SocketIndex < M_SocketStates.Num(); ++SocketIndex)
	{
		BeginReloadForSocket(SocketIndex);
	}
}

void UICBMLaunchComponent::BeginReloadForSocket(const int32 SocketIndex)
{
	if (not M_SocketStates.IsValidIndex(SocketIndex))
	{
		return;
	}
	if (M_SocketStates[SocketIndex].bM_IsReloading)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	FICBMSocketState& SocketState = M_SocketStates[SocketIndex];
	SocketState.bM_IsReloading = true;

	const float Delay = FMath::RandRange(M_LaunchSettings.MinTimeBeforeReady, M_LaunchSettings.MaxTimeBeforeReady);
	FTimerHandle& SocketTimer = M_SocketReloadTimers.FindOrAdd(SocketIndex);
	TWeakObjectPtr<UICBMLaunchComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(
		SocketTimer,
		FTimerDelegate::CreateLambda([WeakThis, SocketIndex]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnReloadTimerFinished(SocketIndex);
		}),
		Delay,
		false);
}

void UICBMLaunchComponent::OnReloadTimerFinished(const int32 SocketIndex)
{
	if (not M_SocketStates.IsValidIndex(SocketIndex))
	{
		return;
	}

	FICBMSocketState& SocketState = M_SocketStates[SocketIndex];
	SocketState.bM_IsReloading = false;

	if (IsValid(SocketState.M_Rocket))
	{
		return;
	}

	AICBMActor* SpawnedRocket = SpawnRocketAtSocket(SocketIndex);
	if (not IsValid(SpawnedRocket))
	{
		BeginReloadForSocket(SocketIndex);
		return;
	}

	SocketState.M_Rocket = SpawnedRocket;
	SpawnedRocket->InitICBMActor(this, SocketIndex, M_OwningPlayer);

	const FVector LaunchReadyLocation = M_OwnerMeshComponent->GetSocketLocation(SocketState.SocketName);
	SpawnedRocket->StartVerticalMoveToLaunchReady(LaunchReadyLocation, M_LaunchSettings.SpawnVerticalMoveDuration);
}

AICBMActor* UICBMLaunchComponent::SpawnRocketAtSocket(const int32 SocketIndex) const
{
	if (not GetIsValidOwnerMeshComponent())
	{
		return nullptr;
	}
	if (not M_SocketStates.IsValidIndex(SocketIndex))
	{
		return nullptr;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	const FName SocketName = M_SocketStates[SocketIndex].SocketName;
	const FVector SocketLocation = M_OwnerMeshComponent->GetSocketLocation(SocketName);
	const FRotator SocketRotation = M_OwnerMeshComponent->GetSocketRotation(SocketName);
	FVector SpawnLocation = SocketLocation;
	SpawnLocation.Z -= M_LaunchSettings.SpawnNegativeZOffset;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	AICBMActor* SpawnedRocket = World->SpawnActor<AICBMActor>(
		M_ICBMActorClass,
		SpawnLocation,
		SocketRotation,
		SpawnParameters);
	if (not IsValid(SpawnedRocket))
	{
		RTSFunctionLibrary::ReportError("Failed spawning ICBM actor at socket: " + SocketName.ToString());
		return nullptr;
	}
	return SpawnedRocket;
}

bool UICBMLaunchComponent::TryConsumePendingLaunches()
{
	if (M_PendingLaunchRequest.M_RemainingRocketsToLaunch <= 0)
	{
		return false;
	}

	const int32 LaunchedAmount = LaunchReadyRockets(M_PendingLaunchRequest.M_RemainingRocketsToLaunch);
	RegisterPendingLaunch(M_PendingLaunchRequest.M_RemainingRocketsToLaunch - LaunchedAmount);
	return LaunchedAmount > 0;
}

int32 UICBMLaunchComponent::LaunchReadyRockets(const int32 DesiredAmount)
{
	if (DesiredAmount <= 0)
	{
		return 0;
	}

	int32 LaunchedCount = 0;
	for (int32 SocketIndex = 0; SocketIndex < M_SocketStates.Num(); ++SocketIndex)
	{
		if (LaunchedCount >= DesiredAmount)
		{
			break;
		}

		FICBMSocketState& SocketState = M_SocketStates[SocketIndex];
		AICBMActor* Rocket = SocketState.M_Rocket;
		if (not IsValid(Rocket))
		{
			continue;
		}
		if (not Rocket->GetIsLaunchReady())
		{
			continue;
		}

		M_PendingLaunchRequest.M_TargetingData.TickAimSelection();
		const FVector TargetLocation = M_PendingLaunchRequest.M_TargetingData.GetActiveTargetLocation();
		Rocket->FireToLocation(TargetLocation, M_OwningPlayer);
		SocketState.M_Rocket = nullptr;
		++LaunchedCount;
		BeginReloadForSocket(SocketIndex);
	}
	return LaunchedCount;
}

void UICBMLaunchComponent::RegisterPendingLaunch(const int32 RemainingAmountToLaunch)
{
	M_PendingLaunchRequest.M_RemainingRocketsToLaunch = FMath::Max(0, RemainingAmountToLaunch);
}

bool UICBMLaunchComponent::GetIsValidOwnerMeshComponent() const
{
	if (M_OwnerMeshComponent.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwnerMeshComponent",
		"GetIsValidOwnerMeshComponent",
		this);
	return false;
}

bool UICBMLaunchComponent::GetIsValidOwningRTSComponent() const
{
	if (M_OwningRTSComponent.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwningRTSComponent",
		"GetIsValidOwningRTSComponent",
		this);
	return false;
}
