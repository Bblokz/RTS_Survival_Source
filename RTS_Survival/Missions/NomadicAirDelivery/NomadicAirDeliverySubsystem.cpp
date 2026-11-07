// Copyright (C) Bas Blokzijl - All rights reserved.

#include "NomadicAirDeliverySubsystem.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AircraftDropTypes.h"
#include "RTS_Survival/Units/Aircraft/GliderDelivery/GliderTankDelivery.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSNomadicAirDelivery, Log, All);

namespace NomadicAirDeliveryPrivate
{
	const FName BeaconColorParameterName(TEXT("MarkerColor"));
	// The truck counts as far enough out of the glider when the touchdown lies at least this far before the spot.
	constexpr float MinGliderSkidLength = 500.f;
}

bool UNomadicAirDeliverySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
}

void UNomadicAirDeliverySubsystem::Deinitialize()
{
	for (FNomadicAirDeliveryRun& Run : M_Runs)
	{
		Run.LoadHandle.Reset();
		if (Run.CargoPlane.IsValid())
		{
			Run.CargoPlane->OnAircraftDropPayloadReleased.RemoveAll(this);
		}
		if (Run.Glider.IsValid())
		{
			Run.Glider->OnTankDelivered.RemoveAll(this);
		}
	}
	M_Runs.Reset();
	Super::Deinitialize();
}

int32 UNomadicAirDeliverySubsystem::StartDelivery(const FNomadicAirDeliveryOrder& Order,
                                                  FOnNomadicAirDeliveryLanded OnLanded)
{
	if (not GetIsValidOrder(Order))
	{
		return INDEX_NONE;
	}

	FNomadicAirDeliveryRun Run;
	Run.DeliveryId = M_NextDeliveryId++;
	Run.Order = Order;
	Run.GroundBuildLocation = GetGroundLocation(Order.BuildLocation);
	Run.OnLanded = MoveTemp(OnLanded);

	TArray<FSoftObjectPath> AssetsToLoad;
	AssetsToLoad.Add(Order.Method == ENomadicAirDeliveryMethod::MammutGlider
		                 ? Order.Settings.HQGliderDeliveryClass.ToSoftObjectPath()
		                 : Order.Settings.CargoPlaneClass.ToSoftObjectPath());
	if (not Order.Settings.BeaconEffect.IsNull())
	{
		AssetsToLoad.Add(Order.Settings.BeaconEffect.ToSoftObjectPath());
	}

	const int32 DeliveryId = Run.DeliveryId;
	M_Runs.Add(MoveTemp(Run));
	UE_LOG(LogRTSNomadicAirDelivery, Display, TEXT("Nomadic air delivery %d started: %s by %s to %s"),
	       DeliveryId, *UEnum::GetValueAsString(Order.NomadicSubtype), *UEnum::GetValueAsString(Order.Method),
	       *Order.BuildLocation.ToCompactString());

	TWeakObjectPtr<UNomadicAirDeliverySubsystem> WeakThis(this);
	const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateLambda([WeakThis, DeliveryId]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAssetsLoaded(DeliveryId);
			}
		}));
	// An already loaded asset completes inside RequestAsyncLoad; the run may already be gone then.
	const int32 RunIndex = FindRunIndex(DeliveryId);
	if (M_Runs.IsValidIndex(RunIndex))
	{
		M_Runs[RunIndex].LoadHandle = Handle;
	}
	return DeliveryId;
}

bool UNomadicAirDeliverySubsystem::GetGliderLandingRun(const FNomadicAirDeliveryOrder& Order,
                                                       const float TankExitDistance,
                                                       FVector& OutTouchdown, FVector& OutStop) const
{
	using namespace NomadicAirDeliveryPrivate;
	const FVector GroundBuildLocation = GetGroundLocation(Order.BuildLocation);
	FVector Direction = FVector::ZeroVector;
	if (not Order.GliderTouchdownLocation.IsNearlyZero())
	{
		Direction = (GroundBuildLocation - Order.GliderTouchdownLocation).GetSafeNormal2D();
	}
	else if (const UGlobalAbilitiesManager* Manager = GetPlayerGlobalAbilitiesManager())
	{
		const FVector SpawnLocation = Manager->GetAircraftBombingSpawnLocation(this, GroundBuildLocation);
		Direction = (GroundBuildLocation - SpawnLocation).GetSafeNormal2D();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}

	// The truck drives TankExitDistance out of the nose on its own, the rest of the way it drives to build.
	const float StopToBuildDistance = TankExitDistance + Order.Settings.GliderStopClearance;
	OutStop = GetGroundLocation(GroundBuildLocation - Direction * StopToBuildDistance);
	OutTouchdown = Order.GliderTouchdownLocation.IsNearlyZero()
		               ? GetGroundLocation(OutStop - Direction * Order.Settings.GliderSkidLength)
		               : GetGroundLocation(Order.GliderTouchdownLocation);
	const float SkidLength = FVector::DotProduct(OutStop - OutTouchdown, Direction);
	return SkidLength >= MinGliderSkidLength;
}

bool UNomadicAirDeliverySubsystem::GetIsValidOrder(const FNomadicAirDeliveryOrder& Order) const
{
	if (Order.NomadicSubtype == ENomadicSubtype::Nomadic_None)
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic air delivery ordered without a nomadic subtype."));
		return false;
	}
	const bool bIsGlider = Order.Method == ENomadicAirDeliveryMethod::MammutGlider;
	const bool bHasAircraftClass = bIsGlider
		                               ? not Order.Settings.HQGliderDeliveryClass.IsNull()
		                               : not Order.Settings.CargoPlaneClass.IsNull();
	if (not bHasAircraftClass)
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic air delivery has no aircraft or glider class configured."));
		return false;
	}
	return IsValid(GetWorld());
}

int32 UNomadicAirDeliverySubsystem::FindRunIndex(const int32 DeliveryId) const
{
	return M_Runs.IndexOfByPredicate([DeliveryId](const FNomadicAirDeliveryRun& Run)
	{
		return Run.DeliveryId == DeliveryId;
	});
}

int32 UNomadicAirDeliverySubsystem::FindRunIndexByCargoPlane(const AAircraftMaster* CargoPlane) const
{
	return M_Runs.IndexOfByPredicate([CargoPlane](const FNomadicAirDeliveryRun& Run)
	{
		return Run.CargoPlane.Get() == CargoPlane;
	});
}

int32 UNomadicAirDeliverySubsystem::FindRunIndexByGlider(const AGliderTankDelivery* Glider) const
{
	return M_Runs.IndexOfByPredicate([Glider](const FNomadicAirDeliveryRun& Run)
	{
		return Run.Glider.Get() == Glider;
	});
}

UGlobalAbilitiesManager* UNomadicAirDeliverySubsystem::GetPlayerGlobalAbilitiesManager() const
{
	const ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	return IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
}

FVector UNomadicAirDeliverySubsystem::GetGroundLocation(const FVector& Location) const
{
	FVector GroundLocation;
	GlobalAbilityHelpers::TryFindGroundLocation(this, Location, GroundLocation);
	return GroundLocation;
}

void UNomadicAirDeliverySubsystem::OnAssetsLoaded(const int32 DeliveryId)
{
	const int32 RunIndex = FindRunIndex(DeliveryId);
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	M_Runs[RunIndex].LoadHandle.Reset();
	SpawnBeacon(M_Runs[RunIndex]);
	if (M_Runs[RunIndex].Order.Method == ENomadicAirDeliveryMethod::MammutGlider)
	{
		StartGliderRun(DeliveryId);
		return;
	}
	StartCargoPlaneRun(DeliveryId);
}

void UNomadicAirDeliverySubsystem::SpawnBeacon(FNomadicAirDeliveryRun& Run) const
{
	UNiagaraSystem* BeaconSystem = Run.Order.Settings.BeaconEffect.Get();
	UWorld* World = GetWorld();
	if (not IsValid(BeaconSystem) || not IsValid(World))
	{
		return;
	}
	// Not auto-destroyed: it burns until the truck is down, however long the approach takes.
	UNiagaraComponent* Beacon = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, BeaconSystem, Run.GroundBuildLocation, FRotator::ZeroRotator, FVector::OneVector, false, true);
	if (not IsValid(Beacon))
	{
		return;
	}
	Beacon->SetColorParameter(NomadicAirDeliveryPrivate::BeaconColorParameterName, Run.Order.Settings.BeaconColor);
	Run.Beacon = Beacon;
}

void UNomadicAirDeliverySubsystem::StartCargoPlaneRun(const int32 DeliveryId)
{
	const int32 RunIndex = FindRunIndex(DeliveryId);
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	UClass* CargoPlaneClass = M_Runs[RunIndex].Order.Settings.CargoPlaneClass.Get();
	const UGlobalAbilitiesManager* Manager = GetPlayerGlobalAbilitiesManager();
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(CargoPlaneClass) || not IsValid(Manager) || not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Nomadic cargo plane delivery has no plane class, global ability manager or async spawner."));
		FinishRun(RunIndex, nullptr);
		return;
	}

	const FTrainingOption NomadicOption(EAllUnitType::UNType_Nomadic,
	                                    static_cast<uint8>(M_Runs[RunIndex].Order.NomadicSubtype));
	if (not AsyncSpawner->GetHasClassForTrainingOption(NomadicOption))
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic cargo plane delivery: the player has no truck for ")
			+ NomadicOption.GetTrainingName());
		FinishRun(RunIndex, nullptr);
		return;
	}

	const FVector GroundBuildLocation = M_Runs[RunIndex].GroundBuildLocation;
	FVector SpawnLocation = Manager->GetAircraftBombingSpawnLocation(this, GroundBuildLocation);
	// High enough that the truck on its ropes clears the ground on the way in.
	SpawnLocation.Z = FMath::Max(SpawnLocation.Z, AircraftRopeSling::GetMinCruiseZ(
		                             GroundBuildLocation.Z, AircraftRopeSling::DefaultHangDistance));
	AAircraftMaster* CargoPlane = GlobalAbilityHelpers::SpawnAircraftAtLocation(this, CargoPlaneClass, SpawnLocation);
	if (not IsValid(CargoPlane))
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic cargo plane delivery failed to spawn its plane."));
		FinishRun(RunIndex, nullptr);
		return;
	}
	CargoPlane->SetUnitSelectable(false);
	CargoPlane->OnDestroyed.AddUniqueDynamic(this, &UNomadicAirDeliverySubsystem::OnCargoPlaneDestroyed);
	M_Runs[RunIndex].CargoPlane = CargoPlane;

	const FNomadicAirDeliveryOrder& Order = M_Runs[RunIndex].Order;
	const FVector NomadicSpawnLocation = SpawnLocation - FVector(0.f, 0.f, AircraftRopeSling::DefaultHangDistance);
	TWeakObjectPtr<UNomadicAirDeliverySubsystem> WeakThis(this);
	const auto OnSpawned = [WeakThis, DeliveryId](const FTrainingOption&, AActor* SpawnedActor, const int32)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnCargoNomadicSpawned(DeliveryId, SpawnedActor);
		}
	};
	const bool bRequested = AsyncSpawner->AsyncSpawnOptionAtLocation(
		NomadicOption, NomadicSpawnLocation, this, DeliveryId, OnSpawned, FRotator(0.f, Order.BuildRotation.Yaw, 0.f));
	if (bRequested)
	{
		return;
	}
	// Refused without a callback: the plane must not fly in empty.
	const int32 RefusedRunIndex = FindRunIndex(DeliveryId);
	if (M_Runs.IsValidIndex(RefusedRunIndex))
	{
		AAircraftMaster* EmptyPlane = M_Runs[RefusedRunIndex].CargoPlane.Get();
		FinishRun(RefusedRunIndex, nullptr);
		if (IsValid(EmptyPlane))
		{
			EmptyPlane->Destroy();
		}
	}
}

void UNomadicAirDeliverySubsystem::OnCargoNomadicSpawned(const int32 DeliveryId, AActor* SpawnedActor)
{
	const int32 RunIndex = FindRunIndex(DeliveryId);
	ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(SpawnedActor);
	if (not M_Runs.IsValidIndex(RunIndex) || not M_Runs[RunIndex].CargoPlane.IsValid() || not IsValid(Nomadic))
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic cargo plane delivery lost its plane or truck before take-off."));
		if (IsValid(SpawnedActor))
		{
			SpawnedActor->Destroy();
		}
		if (M_Runs.IsValidIndex(RunIndex))
		{
			FinishRun(RunIndex, nullptr);
		}
		return;
	}

	AAircraftMaster* CargoPlane = M_Runs[RunIndex].CargoPlane.Get();
	const UGlobalAbilitiesManager* Manager = GetPlayerGlobalAbilitiesManager();
	FAircraftDropRequest DropRequest;
	DropRequest.State = EAircraftDropRequestState::MovingToDropLocation;
	DropRequest.PayloadType = EAircraftDropPayloadType::Tank;
	DropRequest.OwningPlayer = M_Runs[RunIndex].Order.OwningPlayer;
	DropRequest.ExecuteLocation = M_Runs[RunIndex].GroundBuildLocation;
	DropRequest.RetreatLocation = IsValid(Manager)
		                              ? Manager->GetAircraftBombingRetreatLocation(this, DropRequest.ExecuteLocation)
		                              : CargoPlane->GetActorLocation();
	DropRequest.TankAttachZOffset = -AircraftRopeSling::DefaultHangDistance;
	DropRequest.bRopeSlungPayload = true;
	DropRequest.HowLongStayLanded = M_Runs[RunIndex].Order.Settings.CargoPlaneHoverSeconds;
	DropRequest.AttachedTank = Nomadic;
	M_Runs[RunIndex].SlungNomadic = Nomadic;
	Nomadic->OnDestroyed.AddUniqueDynamic(this, &UNomadicAirDeliverySubsystem::OnSlungNomadicDestroyed);
	CargoPlane->OnAircraftDropPayloadReleased.AddUObject(this, &UNomadicAirDeliverySubsystem::OnCargoPayloadReleased);
	CargoPlane->OrderAircraftDrop(DropRequest);
}

void UNomadicAirDeliverySubsystem::OnCargoPayloadReleased(AAircraftMaster* CargoPlane, ATankMaster* ReleasedTank)
{
	const int32 RunIndex = FindRunIndexByCargoPlane(CargoPlane);
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	if (IsValid(CargoPlane))
	{
		CargoPlane->OnAircraftDropPayloadReleased.RemoveAll(this);
	}
	FinishRun(RunIndex, Cast<ANomadicVehicle>(ReleasedTank));
}

void UNomadicAirDeliverySubsystem::OnCargoPlaneDestroyed(AActor* DestroyedActor)
{
	// The plane takes off and retreats after the release; only a plane lost before it counts as a failure.
	const int32 RunIndex = FindRunIndexByCargoPlane(Cast<AAircraftMaster>(DestroyedActor));
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	RTSFunctionLibrary::ReportError(TEXT("Nomadic cargo plane was destroyed before it set its truck down."));
	FinishRun(RunIndex, nullptr);
}

void UNomadicAirDeliverySubsystem::OnSlungNomadicDestroyed(AActor* DestroyedActor)
{
	const int32 RunIndex = M_Runs.IndexOfByPredicate([DestroyedActor](const FNomadicAirDeliveryRun& Run)
	{
		return Run.SlungNomadic.Get() == DestroyedActor;
	});
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	RTSFunctionLibrary::ReportError(TEXT("Nomadic truck was destroyed while it hung under the cargo plane."));
	FinishRun(RunIndex, nullptr);
}

void UNomadicAirDeliverySubsystem::StartGliderRun(const int32 DeliveryId)
{
	const int32 RunIndex = FindRunIndex(DeliveryId);
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	UClass* GliderClass = M_Runs[RunIndex].Order.Settings.HQGliderDeliveryClass.Get();
	UWorld* World = GetWorld();
	const AGliderTankDelivery* GliderDefaults = IsValid(GliderClass)
		                                            ? GliderClass->GetDefaultObject<AGliderTankDelivery>()
		                                            : nullptr;
	if (not IsValid(GliderDefaults) || not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic glider delivery failed to load its glider class."));
		FinishRun(RunIndex, nullptr);
		return;
	}

	const FNomadicAirDeliveryOrder Order = M_Runs[RunIndex].Order;
	FVector Touchdown;
	FVector Stop;
	if (not GetGliderLandingRun(Order, GliderDefaults->GetTankExitDistance(), Touchdown, Stop))
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic glider delivery: the touchdown point lies past the build spot."));
		FinishRun(RunIndex, nullptr);
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGliderTankDelivery* Glider = World->SpawnActor<AGliderTankDelivery>(
		GliderClass, Touchdown, FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(Glider))
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic glider delivery failed to spawn its glider."));
		FinishRun(RunIndex, nullptr);
		return;
	}
	M_Runs[RunIndex].Glider = Glider;

	const FVector Direction = (Stop - Touchdown).GetSafeNormal2D();
	FGliderDeliveryOrder GliderOrder;
	GliderOrder.TouchdownLocation = Touchdown;
	GliderOrder.StopLocation = Stop;
	GliderOrder.OwningPlayer = Order.OwningPlayer;
	GliderOrder.TankOption = FTrainingOption(EAllUnitType::UNType_Nomadic, static_cast<uint8>(Order.NomadicSubtype));
	GliderOrder.ApproachDistance = Order.Settings.GliderMaxApproachDistance;
	if (const UGlobalAbilitiesManager* Manager = GetPlayerGlobalAbilitiesManager())
	{
		// The manager clamps the approach to the map edge; only its horizontal distance is used.
		const FVector ApproachStart = Manager->GetStraightFlightSpawnLocation(
			Touchdown, Direction, Order.Settings.GliderMaxApproachDistance);
		GliderOrder.ApproachDistance = FVector::Dist2D(ApproachStart, Touchdown);
	}
	Glider->OnTankDelivered.AddUObject(this, &UNomadicAirDeliverySubsystem::OnGliderTankDelivered);
	Glider->StartDelivery(GliderOrder);
}

void UNomadicAirDeliverySubsystem::OnGliderTankDelivered(AGliderTankDelivery* Glider, ATankMaster* Tank)
{
	const int32 RunIndex = FindRunIndexByGlider(Glider);
	if (IsValid(Glider))
	{
		Glider->OnTankDelivered.RemoveAll(this);
	}
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(Tank);
	if (IsValid(Tank) && not IsValid(Nomadic))
	{
		RTSFunctionLibrary::ReportError(TEXT("Nomadic glider delivery: the glider delivered a unit that is no nomadic."));
	}
	FinishRun(RunIndex, Nomadic);
}

void UNomadicAirDeliverySubsystem::FinishRun(const int32 RunIndex, ANomadicVehicle* Nomadic)
{
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	// Taken out first: the requester's callback may start new deliveries.
	FNomadicAirDeliveryRun Run = MoveTemp(M_Runs[RunIndex]);
	M_Runs.RemoveAt(RunIndex);
	if (Run.Beacon.IsValid())
	{
		Run.Beacon->DestroyComponent();
	}
	if (Run.CargoPlane.IsValid())
	{
		Run.CargoPlane->OnAircraftDropPayloadReleased.RemoveAll(this);
		Run.CargoPlane->OnDestroyed.RemoveDynamic(this, &UNomadicAirDeliverySubsystem::OnCargoPlaneDestroyed);
	}
	if (Run.SlungNomadic.IsValid())
	{
		Run.SlungNomadic->OnDestroyed.RemoveDynamic(this, &UNomadicAirDeliverySubsystem::OnSlungNomadicDestroyed);
	}
	UE_LOG(LogRTSNomadicAirDelivery, Display, TEXT("Nomadic air delivery %d %s: %s"), Run.DeliveryId,
	       IsValid(Nomadic) ? TEXT("landed") : TEXT("failed"), *UEnum::GetValueAsString(Run.Order.NomadicSubtype));

	// The requester hears of the truck before it starts to unfold, so a callback on the conversion is not missed.
	TWeakObjectPtr<ANomadicVehicle> WeakNomadic(Nomadic);
	Run.OnLanded.ExecuteIfBound(Nomadic);
	if (WeakNomadic.IsValid())
	{
		OrderBuildOnSpot(*WeakNomadic.Get(), Run.GroundBuildLocation, Run.Order.BuildRotation);
	}
	OnAnyDeliveryFinished.Broadcast(WeakNomadic.Get(), Run.Order);
}

void UNomadicAirDeliverySubsystem::OrderBuildOnSpot(ANomadicVehicle& Nomadic, const FVector& GroundBuildLocation,
                                                    const FRotator& BuildRotation) const
{
	// The build radius of the HQ is only checked where the player places a building (ACPPController); the command
	// itself builds anywhere, which is what an air-delivered base needs.
	const FRotator YawOnly(0.f, BuildRotation.Yaw, 0.f);
	const ECommandQueueError Error = Nomadic.CreateBuildingAtLocation(GroundBuildLocation, YawOnly, true);
	if (Error == ECommandQueueError::NoError)
	{
		return;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("Air-delivered nomadic %s could not be ordered to build, error %d."), *Nomadic.GetName(),
		static_cast<int32>(Error)));
}
