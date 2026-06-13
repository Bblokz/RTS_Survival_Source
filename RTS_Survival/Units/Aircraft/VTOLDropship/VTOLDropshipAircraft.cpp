#include "VTOLDropshipAircraft.h"

#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/Aircraft/VTOLDropship/VTOLDropshipDeliveryComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace VTOLDropshipAircraftConstants
{
	constexpr float ReachedLocationTolerance = 1.f;
	constexpr float MinVerticalMoveSpeed = 1.f;
	// Requested exact projection extent for payload placement near the dropship pad.
	constexpr float PayloadNavigationProjectionExtent = 2.f;
}

AVTOLDropshipAircraft::AVTOLDropshipAircraft(const FObjectInitializer& ObjectInitializer)
	: AAircraftMaster(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AVTOLDropshipAircraft::InitVTOLDropshipAircraft(
	const FTransform& StartTransform,
	const FTransform& LandingTransform,
	const float LandedDuration,
	const FUnitCost& DeliveryResources,
	const TArray<TSoftClassPtr<AActor>>& ActorsToSpawnWhenLanded,
	const float ActorSpawnOffsetRadius,
	UVTOLDropshipDeliveryComponent* OwningDeliveryComponent)
{
	ConfigureDeliveryPayload(
		LandedDuration,
		DeliveryResources,
		ActorsToSpawnWhenLanded,
		ActorSpawnOffsetRadius,
		OwningDeliveryComponent);
	StartDeliveryRun(StartTransform, LandingTransform);
}

void AVTOLDropshipAircraft::StartDeliveryRun(const FTransform& StartTransform, const FTransform& LandingTransform)
{
	M_StartTransform = StartTransform;
	M_LandingTransform = LandingTransform;
	bM_DestroyAfterAscent = false;
	bM_DeliveredPayloadThisRun = false;
	M_DropshipState = EVTOLDropshipAircraftState::Descending;
	SetActorTransform(M_StartTransform);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	ForceNeverSelectable();
}


void AVTOLDropshipAircraft::ConfigureDeliveryPayload(
	const float LandedDuration,
	const FUnitCost& DeliveryResources,
	const TArray<TSoftClassPtr<AActor>>& ActorsToSpawnWhenLanded,
	const float ActorSpawnOffsetRadius,
	UVTOLDropshipDeliveryComponent* OwningDeliveryComponent)
{
	M_LandedDuration = FMath::Max(0.f, LandedDuration);
	M_Payload.M_DeliveryResources = DeliveryResources;
	M_Payload.M_ActorsToSpawnWhenLanded = ActorsToSpawnWhenLanded;
	M_Payload.M_ActorSpawnSpacing = FMath::Max(0.f, ActorSpawnOffsetRadius);
	CachePayloadActorClasses();
	M_OwningDeliveryComponent = OwningDeliveryComponent;
}

void AVTOLDropshipAircraft::AbortDeliveryAndDestroyAfterAscent()
{
	bM_DestroyAfterAscent = true;
	if (M_DropshipState == EVTOLDropshipAircraftState::Cached)
	{
		Destroy();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LandedTimerHandle);
	}
	M_DropshipState = EVTOLDropshipAircraftState::Ascending;
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::DestroyCachedAircraft()
{
	Destroy();
}

void AVTOLDropshipAircraft::BeginPlay()
{
	Super::BeginPlay();
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LandedTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AVTOLDropshipAircraft::Tick(const float DeltaSeconds)
{
	// Intentionally skip AAircraftMaster::Tick so regular Bezier/idle aircraft movement
	// can never move us off the pad.
	ASelectablePawnMaster::Tick(DeltaSeconds);
	ForceNeverSelectable();

	switch (M_DropshipState)
	{
	case EVTOLDropshipAircraftState::Descending:
		TickDescending(DeltaSeconds);
		break;
	case EVTOLDropshipAircraftState::Ascending:
		TickAscending(DeltaSeconds);
		break;
	default:
		break;
	}
}

void AVTOLDropshipAircraft::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::OnRTSUnitSpawned(
	const bool bSetDisabled,
	const float TimeNotSelectable,
	const FVector MoveTo)
{
	SetActorHiddenInGame(bSetDisabled);
	SetActorEnableCollision(not bSetDisabled);
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::ExecuteMoveCommand(const FVector MoveToLocation)
{
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::TerminateMoveCommand()
{
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::ExecuteAttackCommand(AActor* TargetActor)
{
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::TerminateAttackCommand()
{
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::ExecuteReturnToBase()
{
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::TerminateReturnToBase()
{
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::ForceNeverSelectable() const
{
	USelectionComponent* DropshipSelectionComponent = GetSelectionComponent();
	if (not IsValid(DropshipSelectionComponent))
	{
		return;
	}
	DropshipSelectionComponent->SetCanBeSelected(false);
	if (DropshipSelectionComponent->GetIsSelected())
	{
		DropshipSelectionComponent->SetUnitDeselected();
	}
	DropshipSelectionComponent->HideDecals();
}

void AVTOLDropshipAircraft::TickDescending(const float DeltaSeconds)
{
	MoveVerticallyTowardTargetZ(DeltaSeconds, M_LandingTransform.GetLocation().Z);
	const float DistanceToLandingZ = FMath::Abs(GetActorLocation().Z - M_LandingTransform.GetLocation().Z);
	if (DistanceToLandingZ > VTOLDropshipAircraftConstants::ReachedLocationTolerance)
	{
		return;
	}
	OnTouchdown();
}

void AVTOLDropshipAircraft::TickAscending(const float DeltaSeconds)
{
	MoveVerticallyTowardTargetZ(DeltaSeconds, M_StartTransform.GetLocation().Z);
	const float DistanceToStartZ = FMath::Abs(GetActorLocation().Z - M_StartTransform.GetLocation().Z);
	if (DistanceToStartZ > VTOLDropshipAircraftConstants::ReachedLocationTolerance)
	{
		return;
	}
	OnAscentFinished();
}

void AVTOLDropshipAircraft::MoveVerticallyTowardTargetZ(const float DeltaSeconds, const float TargetZ)
{
	const FVector LandingLocation = M_LandingTransform.GetLocation();
	const float NewZ = FMath::FInterpConstantTo(GetActorLocation().Z, TargetZ, DeltaSeconds, GetSafeVerticalMoveSpeed());
	SetActorLocation(FVector(LandingLocation.X, LandingLocation.Y, NewZ));
	SetActorRotation(M_LandingTransform.Rotator());
}

void AVTOLDropshipAircraft::OnTouchdown()
{
	SetActorTransform(M_LandingTransform);
	M_DropshipState = EVTOLDropshipAircraftState::Landed;
	if (not bM_DeliveredPayloadThisRun && not bM_DestroyAfterAscent)
	{
		DeliverResources();
		SpawnPayloadActors();
		bM_DeliveredPayloadThisRun = true;
	}

	if (bM_DestroyAfterAscent || M_LandedDuration <= 0.f)
	{
		OnLandedDurationFinished();
		return;
	}

	FTimerDelegate LandedTimerDelegate;
	TWeakObjectPtr<AVTOLDropshipAircraft> WeakDropshipAircraft(this);
	LandedTimerDelegate.BindLambda([WeakDropshipAircraft]()
	{
		if (not WeakDropshipAircraft.IsValid())
		{
			return;
		}
		WeakDropshipAircraft->OnLandedDurationFinished();
	});
	GetWorldTimerManager().SetTimer(M_LandedTimerHandle, LandedTimerDelegate, M_LandedDuration, false);
}

void AVTOLDropshipAircraft::OnLandedDurationFinished()
{
	M_DropshipState = EVTOLDropshipAircraftState::Ascending;
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::OnAscentFinished()
{
	if (bM_DestroyAfterAscent)
	{
		Destroy();
		return;
	}

	CacheAtStartLocation();
	if (GetIsValidOwningDeliveryComponent())
	{
		M_OwningDeliveryComponent->OnDropshipAircraftRunCompleted(this);
	}
}

void AVTOLDropshipAircraft::CacheAtStartLocation()
{
	M_DropshipState = EVTOLDropshipAircraftState::Cached;
	SetActorTransform(M_StartTransform);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	ForceNeverSelectable();
}

void AVTOLDropshipAircraft::DeliverResources() const
{
	if (M_Payload.M_DeliveryResources.IsEmpty())
	{
		return;
	}

	UPlayerResourceManager* PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(PlayerResourceManager))
	{
		RTSFunctionLibrary::ReportError("PlayerResourceManager not valid for VTOL dropship delivery on " + GetName());
		return;
	}

	const bool bAddedAllResources = PlayerResourceManager->RefundCosts(M_Payload.M_DeliveryResources);
	if (not bAddedAllResources)
	{
		RTSFunctionLibrary::ReportError(
			"VTOL dropship delivery could not fit all delivered resources into storage on " + GetName());
	}
}

void AVTOLDropshipAircraft::CachePayloadActorClasses()
{
	M_Payload.M_LoadedActorsToSpawnWhenLanded.Reset();
	for (const TSoftClassPtr<AActor>& ActorClassSoftPtr : M_Payload.M_ActorsToSpawnWhenLanded)
	{
		TSubclassOf<AActor> LoadedActorClass;
		if (not TryLoadPayloadActorClass(ActorClassSoftPtr, LoadedActorClass))
		{
			continue;
		}
		M_Payload.M_LoadedActorsToSpawnWhenLanded.Add(LoadedActorClass);
	}
}

bool AVTOLDropshipAircraft::TryLoadPayloadActorClass(
	const TSoftClassPtr<AActor>& ActorClassSoftPtr,
	TSubclassOf<AActor>& OutActorClass) const
{
	OutActorClass = nullptr;
	if (ActorClassSoftPtr.IsNull())
	{
		RTSFunctionLibrary::ReportError("Null payload actor class on VTOL dropship: " + GetName());
		return false;
	}

	UClass* LoadedActorClass = ActorClassSoftPtr.LoadSynchronous();
	if (not IsValid(LoadedActorClass))
	{
		RTSFunctionLibrary::ReportError("Invalid payload actor class on VTOL dropship: " + GetName());
		return false;
	}

	OutActorClass = LoadedActorClass;
	return true;
}

void AVTOLDropshipAircraft::SpawnPayloadActors() const
{
	const int32 ActorAmount = M_Payload.M_LoadedActorsToSpawnWhenLanded.Num();
	for (int32 ActorIndex = 0; ActorIndex < ActorAmount; ++ActorIndex)
	{
		FVector SpawnLocation = FVector::ZeroVector;
		if (not TryGetPayloadSpawnLocation(ActorIndex, ActorAmount, SpawnLocation))
		{
			continue;
		}

		FTransform SpawnTransform = M_LandingTransform;
		SpawnTransform.SetLocation(SpawnLocation);
		SpawnPayloadActor(M_Payload.M_LoadedActorsToSpawnWhenLanded[ActorIndex], SpawnTransform);
	}
}

void AVTOLDropshipAircraft::SpawnPayloadActor(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform) const
{
	if (not IsValid(ActorClass.Get()))
	{
		RTSFunctionLibrary::ReportError("Invalid loaded VTOL dropship payload class on " + GetName());
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParameters);
	if (not IsValid(SpawnedActor))
	{
		RTSFunctionLibrary::ReportError("Failed to spawn VTOL dropship payload actor on " + GetName());
	}
}

bool AVTOLDropshipAircraft::TryGetPayloadSpawnLocation(
	const int32 ActorIndex,
	const int32 ActorAmount,
	FVector& OutSpawnLocation) const
{
	FVector DesiredLocation = M_LandingTransform.GetLocation();
	if (ActorAmount > 1 && M_Payload.M_ActorSpawnSpacing > 0.f)
	{
		const float AngleRadians = (TWO_PI / static_cast<float>(ActorAmount)) * static_cast<float>(ActorIndex);
		const float RingRadius = GetPayloadRingRadius(ActorAmount);
		DesiredLocation += FVector(
			FMath::Cos(AngleRadians) * RingRadius,
			FMath::Sin(AngleRadians) * RingRadius,
			0.f);
	}

	if (not TryProjectPayloadLocationToNavigation(DesiredLocation, OutSpawnLocation))
	{
		RTSFunctionLibrary::ReportError("Failed to project VTOL dropship payload spawn to navigation on " + GetName());
		return false;
	}

	return true;
}

float AVTOLDropshipAircraft::GetPayloadRingRadius(const int32 ActorAmount) const
{
	if (ActorAmount <= 2)
	{
		return M_Payload.M_ActorSpawnSpacing / 2.f;
	}

	const float HalfAngleBetweenActors = PI / static_cast<float>(ActorAmount);
	return M_Payload.M_ActorSpawnSpacing / (2.f * FMath::Sin(HalfAngleBetweenActors));
}

float AVTOLDropshipAircraft::GetSafeVerticalMoveSpeed() const
{
	return FMath::Max(M_VerticalMoveSpeed, VTOLDropshipAircraftConstants::MinVerticalMoveSpeed);
}

bool AVTOLDropshipAircraft::TryProjectPayloadLocationToNavigation(
	const FVector& DesiredLocation,
	FVector& OutProjectedLocation) const
{
	bool bWasSuccessful = false;
	const FVector ProjectionExtent(VTOLDropshipAircraftConstants::PayloadNavigationProjectionExtent);
	OutProjectedLocation = URTSBlueprintFunctionLibrary::RTSProjectLocationToNavigation(
		this,
		DesiredLocation,
		ProjectionExtent,
		bWasSuccessful);
	return bWasSuccessful;
}

bool AVTOLDropshipAircraft::GetIsValidOwningDeliveryComponent() const
{
	if (M_OwningDeliveryComponent.IsValid())
	{
		return true;
	}
	return false;
}
