#include "VTOLDropshipDeliveryComponent.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/Units/Aircraft/VTOLDropship/VTOLDropshipAircraft.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UVTOLDropshipDeliveryComponent::UVTOLDropshipDeliveryComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVTOLDropshipDeliveryComponent::InitVTOLDropshipDeliveryComponent(
	const float StartHeightOffset,
	UMeshComponent* LandingSocketMesh,
	const FName LandingSocketName,
	const FUnitCost& DeliveryResources,
	const TArray<TSoftClassPtr<AActor>>& ActorsToSpawnWhenLanded,
	const float LandedDuration,
	const float TimeBetweenRuns,
	const TSoftClassPtr<AVTOLDropshipAircraft> AircraftClass,
	const float ActorSpawnOffsetRadius,
	const bool bStartsEnabled)
{
	ClearNextRunTimer();
	M_RuntimeState.bM_IsEnabled = false;
	bM_HasBeenInitialised = false;
	M_DeliveryConfig.M_StartHeightOffset = FMath::Max(0.f, StartHeightOffset);
	M_DeliveryConfig.M_LandingSocketName = LandingSocketName;
	M_DeliveryConfig.M_DeliveryResources = DeliveryResources;
	M_DeliveryConfig.M_ActorsToSpawnWhenLanded = ActorsToSpawnWhenLanded;
	M_DeliveryConfig.M_LandedDuration = FMath::Max(0.f, LandedDuration);
	M_DeliveryConfig.M_TimeBetweenRuns = FMath::Max(0.f, TimeBetweenRuns);
	M_DeliveryConfig.M_AircraftClass = AircraftClass;
	M_DeliveryConfig.M_ActorSpawnOffsetRadius = FMath::Max(0.f, ActorSpawnOffsetRadius);
	M_RuntimeState.M_LandingSocketMesh = LandingSocketMesh;
	M_RuntimeState.bM_IsCleaningUp = false;
	bM_HasBeenInitialised = true;

	if (not CacheAircraftClass())
	{
		bM_HasBeenInitialised = false;
		return;
	}

	if (not GetIsValidConfiguration())
	{
		bM_HasBeenInitialised = false;
		return;
	}

	SetDropshipDeliveryEnabled(bStartsEnabled);
}

void UVTOLDropshipDeliveryComponent::SetDropshipDeliveryEnabled(const bool bNewEnabled)
{
	M_RuntimeState.bM_IsEnabled = bNewEnabled;
	if (not bNewEnabled)
	{
		ClearNextRunTimer();
		return;
	}

	if (M_RuntimeState.bM_IsDeliveryInProgress)
	{
		return;
	}
	StartNextRunTimer();
}

bool UVTOLDropshipDeliveryComponent::GetIsDropshipDeliveryEnabled() const
{
	return M_RuntimeState.bM_IsEnabled;
}

void UVTOLDropshipDeliveryComponent::OnDropshipAircraftRunCompleted(AVTOLDropshipAircraft* CompletedAircraft)
{
	if (not GetIsValidCachedAircraft())
	{
		return;
	}

	if (M_RuntimeState.M_CachedAircraft.Get() != CompletedAircraft)
	{
		RTSFunctionLibrary::ReportError("Unexpected VTOL dropship completed callback on " + GetName());
		return;
	}

	M_RuntimeState.bM_IsDeliveryInProgress = false;
	if (M_RuntimeState.bM_IsEnabled)
	{
		StartNextRunTimer();
	}
}

void UVTOLDropshipDeliveryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HandleOwnerEndPlayCleanup();
	Super::EndPlay(EndPlayReason);
}

void UVTOLDropshipDeliveryComponent::StartNextRunTimer()
{
	ClearNextRunTimer();
	if (not bM_HasBeenInitialised)
	{
		RTSFunctionLibrary::ReportError("VTOL dropship delivery enabled before init on " + GetName());
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	FTimerDelegate NextRunDelegate;
	TWeakObjectPtr<UVTOLDropshipDeliveryComponent> WeakDeliveryComponent(this);
	NextRunDelegate.BindLambda([WeakDeliveryComponent]()
	{
		if (not WeakDeliveryComponent.IsValid())
		{
			return;
		}
		WeakDeliveryComponent->StartDeliveryRun();
	});
	constexpr float MinCancelableTimerDelay = 0.01f;
	const float NextRunDelay = FMath::Max(M_DeliveryConfig.M_TimeBetweenRuns, MinCancelableTimerDelay);
	World->GetTimerManager().SetTimer(
		M_RuntimeState.M_NextRunTimerHandle,
		NextRunDelegate,
		NextRunDelay,
		false);
}

void UVTOLDropshipDeliveryComponent::ClearNextRunTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RuntimeState.M_NextRunTimerHandle);
	}
}

void UVTOLDropshipDeliveryComponent::StartDeliveryRun()
{
	if (not M_RuntimeState.bM_IsEnabled || M_RuntimeState.bM_IsDeliveryInProgress)
	{
		return;
	}
	if (not GetIsValidConfiguration())
	{
		return;
	}

	const FTransform LandingTransform = GetLandingSocketTransform();
	const FTransform StartTransform = GetStartTransformFromLandingTransform(LandingTransform);
	EnsureCachedAircraftAndStartRun(StartTransform, LandingTransform);
}

void UVTOLDropshipDeliveryComponent::EnsureCachedAircraftAndStartRun(
	const FTransform& StartTransform,
	const FTransform& LandingTransform)
{
	AVTOLDropshipAircraft* CachedAircraft = M_RuntimeState.M_CachedAircraft.Get();
	if (not IsValid(CachedAircraft))
	{
		M_RuntimeState.bM_IsDeliveryInProgress = true;
		CachedAircraft = SpawnCachedAircraft(StartTransform, LandingTransform);
		if (not IsValid(CachedAircraft))
		{
			M_RuntimeState.bM_IsDeliveryInProgress = false;
		}
		return;
	}

	if (GetShouldReplaceCachedAircraft(CachedAircraft))
	{
		DestroyCachedAircraftForReplacement(CachedAircraft);
		EnsureCachedAircraftAndStartRun(StartTransform, LandingTransform);
		return;
	}

	M_RuntimeState.bM_IsDeliveryInProgress = true;
	ConfigureCachedAircraftForRun(CachedAircraft);
	CachedAircraft->StartDeliveryRun(StartTransform, LandingTransform);
}

AVTOLDropshipAircraft* UVTOLDropshipDeliveryComponent::SpawnCachedAircraft(
	const FTransform& StartTransform,
	const FTransform& LandingTransform)
{
	if (not GetIsValidLoadedAircraftClass())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVTOLDropshipAircraft* SpawnedAircraft = World->SpawnActor<AVTOLDropshipAircraft>(
		M_DeliveryConfig.M_LoadedAircraftClass,
		StartTransform,
		SpawnParameters);
	if (not IsValid(SpawnedAircraft))
	{
		RTSFunctionLibrary::ReportError("Failed to spawn VTOL dropship aircraft on " + GetName());
		return nullptr;
	}

	M_RuntimeState.M_CachedAircraft = SpawnedAircraft;
	BindCachedAircraftDestroyedCallback(SpawnedAircraft);
	SpawnedAircraft->InitVTOLDropshipAircraft(
		StartTransform,
		LandingTransform,
		M_DeliveryConfig.M_LandedDuration,
		M_DeliveryConfig.M_DeliveryResources,
		M_DeliveryConfig.M_ActorsToSpawnWhenLanded,
		M_DeliveryConfig.M_ActorSpawnOffsetRadius,
		this);
	return SpawnedAircraft;
}

bool UVTOLDropshipDeliveryComponent::GetShouldReplaceCachedAircraft(
	const AVTOLDropshipAircraft* CachedAircraft) const
{
	if (not IsValid(CachedAircraft) || not GetIsValidLoadedAircraftClass())
	{
		return false;
	}
	return not CachedAircraft->IsA(M_DeliveryConfig.M_LoadedAircraftClass);
}

void UVTOLDropshipDeliveryComponent::DestroyCachedAircraftForReplacement(AVTOLDropshipAircraft* CachedAircraft)
{
	if (not IsValid(CachedAircraft))
	{
		return;
	}
	CachedAircraft->OnDestroyed.RemoveDynamic(this, &UVTOLDropshipDeliveryComponent::OnCachedAircraftDestroyed);
	ClearCachedAircraftReference();
	CachedAircraft->DestroyCachedAircraft();
}

void UVTOLDropshipDeliveryComponent::ConfigureCachedAircraftForRun(AVTOLDropshipAircraft* CachedAircraft)
{
	if (not IsValid(CachedAircraft))
	{
		return;
	}

	CachedAircraft->ConfigureDeliveryPayload(
		M_DeliveryConfig.M_LandedDuration,
		M_DeliveryConfig.M_DeliveryResources,
		M_DeliveryConfig.M_ActorsToSpawnWhenLanded,
		M_DeliveryConfig.M_ActorSpawnOffsetRadius,
		this);
}

void UVTOLDropshipDeliveryComponent::BindCachedAircraftDestroyedCallback(AVTOLDropshipAircraft* CachedAircraft)
{
	if (not IsValid(CachedAircraft))
	{
		return;
	}
	CachedAircraft->OnDestroyed.AddUniqueDynamic(this, &UVTOLDropshipDeliveryComponent::OnCachedAircraftDestroyed);
}

void UVTOLDropshipDeliveryComponent::ClearCachedAircraftReference()
{
	M_RuntimeState.M_CachedAircraft.Reset();
}

void UVTOLDropshipDeliveryComponent::OnCachedAircraftDestroyed(AActor* DestroyedActor)
{
	if (M_RuntimeState.bM_IsCleaningUp)
	{
		ClearCachedAircraftReference();
		return;
	}

	if (M_RuntimeState.M_CachedAircraft.Get() != DestroyedActor)
	{
		return;
	}

	ClearCachedAircraftReference();
	M_RuntimeState.bM_IsDeliveryInProgress = false;
	if (M_RuntimeState.bM_IsEnabled)
	{
		StartNextRunTimer();
	}
}

FTransform UVTOLDropshipDeliveryComponent::GetLandingSocketTransform() const
{
	if (not GetIsValidLandingSocketMesh())
	{
		return FTransform::Identity;
	}
	return M_RuntimeState.M_LandingSocketMesh->GetSocketTransform(M_DeliveryConfig.M_LandingSocketName);
}

FTransform UVTOLDropshipDeliveryComponent::GetStartTransformFromLandingTransform(const FTransform& LandingTransform) const
{
	FTransform StartTransform = LandingTransform;
	StartTransform.AddToTranslation(FVector::UpVector * M_DeliveryConfig.M_StartHeightOffset);
	return StartTransform;
}

void UVTOLDropshipDeliveryComponent::HandleOwnerEndPlayCleanup()
{
	ClearNextRunTimer();
	M_RuntimeState.bM_IsEnabled = false;
	M_RuntimeState.bM_IsCleaningUp = true;
	if (not GetIsValidCachedAircraft())
	{
		return;
	}

	AVTOLDropshipAircraft* CachedAircraft = M_RuntimeState.M_CachedAircraft.Get();
	CachedAircraft->OnDestroyed.RemoveDynamic(this, &UVTOLDropshipDeliveryComponent::OnCachedAircraftDestroyed);
	if (M_RuntimeState.bM_IsDeliveryInProgress)
	{
		CachedAircraft->AbortDeliveryAndDestroyAfterAscent();
		return;
	}
	CachedAircraft->DestroyCachedAircraft();
}

bool UVTOLDropshipDeliveryComponent::GetIsValidLandingSocketMesh() const
{
	if (M_RuntimeState.M_LandingSocketMesh.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_LandingSocketMesh",
		"UVTOLDropshipDeliveryComponent::GetIsValidLandingSocketMesh",
		GetOwner());
	return false;
}

bool UVTOLDropshipDeliveryComponent::GetIsValidCachedAircraft() const
{
	if (M_RuntimeState.M_CachedAircraft.IsValid())
	{
		return true;
	}
	return false;
}

bool UVTOLDropshipDeliveryComponent::CacheAircraftClass()
{
	M_DeliveryConfig.M_LoadedAircraftClass = nullptr;
	if (not GetIsValidAircraftClass())
	{
		return false;
	}

	UClass* LoadedAircraftClass = M_DeliveryConfig.M_AircraftClass.LoadSynchronous();
	if (not IsValid(LoadedAircraftClass))
	{
		RTSFunctionLibrary::ReportError("Invalid VTOL dropship aircraft class on " + GetName());
		return false;
	}
	if (not LoadedAircraftClass->IsChildOf(AVTOLDropshipAircraft::StaticClass()))
	{
		RTSFunctionLibrary::ReportError("VTOL dropship aircraft class is not an AVTOLDropshipAircraft on " + GetName());
		return false;
	}

	M_DeliveryConfig.M_LoadedAircraftClass = LoadedAircraftClass;
	return true;
}

bool UVTOLDropshipDeliveryComponent::GetIsValidAircraftClass() const
{
	if (not M_DeliveryConfig.M_AircraftClass.IsNull())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_AircraftClass",
		"UVTOLDropshipDeliveryComponent::GetIsValidAircraftClass",
		GetOwner());
	return false;
}

bool UVTOLDropshipDeliveryComponent::GetIsValidLoadedAircraftClass() const
{
	if (IsValid(M_DeliveryConfig.M_LoadedAircraftClass.Get()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_LoadedAircraftClass",
		"UVTOLDropshipDeliveryComponent::GetIsValidLoadedAircraftClass",
		GetOwner());
	return false;
}

bool UVTOLDropshipDeliveryComponent::GetIsValidConfiguration() const
{
	if (not GetIsValidLandingSocketMesh() || not GetIsValidAircraftClass() || not GetIsValidLoadedAircraftClass())
	{
		return false;
	}
	if (M_DeliveryConfig.M_LandingSocketName.IsNone())
	{
		RTSFunctionLibrary::ReportError("VTOL dropship delivery socket name is none on " + GetName());
		return false;
	}
	// Do not check for socket yet; mesh may not be available!
	if (M_DeliveryConfig.M_StartHeightOffset <= 0.f)
	{
		RTSFunctionLibrary::ReportError("VTOL dropship start height offset must be above zero on " + GetName());
		return false;
	}
	return true;
}
