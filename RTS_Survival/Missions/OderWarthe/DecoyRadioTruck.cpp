// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DecoyRadioTruck.h"

#include "EngineUtils.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/Fallback/EnemyFallbackSubsystem.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/DirectionFinding/DirectionFindingManager.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogDecoyRadioTruck, Log, All);

namespace DecoyRadioTruckConstants
{
	const FColor PlacementColor(200, 60, 160);
	constexpr float EditorDashSize = 400.f;
	constexpr float DriveTickSeconds = 0.5f;
	constexpr int32 TruckSpawnId = 1;
	constexpr int32 EscortSpawnId = 2;
	const TCHAR* const TruckClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilRadio/"
		"Rus_ArmoredCar_ZilRadio.Rus_ArmoredCar_ZilRadio_C");
}

ADecoyRadioTruck::ADecoyRadioTruck()
{
	PrimaryActorTick.bCanEverTick = false;
	TruckClass = TSoftClassPtr<AActor>(FSoftObjectPath(DecoyRadioTruckConstants::TruckClassPath));
	TruckOption = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_ZilFuel));
	EscortOption = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_Ba12));
}

FColor ADecoyRadioTruck::GetPlacementColor() const
{
	return DecoyRadioTruckConstants::PlacementColor;
}

FString ADecoyRadioTruck::GetConfigurationProblem() const
{
	if (Route.Num() < 2)
	{
		return TEXT("needs at least two route points (start and parking spot)");
	}
	if (TruckClass.IsNull())
	{
		return TEXT("no truck class");
	}
	if (not IsValid(DirectionFinding))
	{
		return TEXT("no direction-finding manager: the truck would transmit nothing");
	}
	return Super::GetConfigurationProblem();
}

FString ADecoyRadioTruck::GetPlacementKindName() const
{
	return TEXT("DECOY RADIO TRUCK");
}

void ADecoyRadioTruck::BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	for (int32 PointIndex = 1; PointIndex < Route.Num(); ++PointIndex)
	{
		OutDetail.DashedLines.Emplace(GetRoutePointWorld(PointIndex - 1), GetRoutePointWorld(PointIndex),
		                              GetPlacementColor(), DecoyRadioTruckConstants::EditorDashSize);
	}
}

ADecoyRadioTruck* ADecoyRadioTruck::FindDecoyRadioTruck(const UObject* WorldContextObject)
{
	UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ADecoyRadioTruck> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADecoyRadioTruck::BeginPlay()
{
	Super::BeginPlay();
	WaitForEnemyAI(DecoyRadioTruckConstants::DriveTickSeconds);
}

void ADecoyRadioTruck::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_DriveTimerHandle);
		World->GetTimerManager().ClearTimer(M_StartTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ADecoyRadioTruck::OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
	if (Route.Num() < 2)
	{
		return;
	}
	GetWorldTimerManager().SetTimer(M_StartTimerHandle, this, &ADecoyRadioTruck::TryStartAfterGameStart,
	                                DecoyRadioTruckConstants::DriveTickSeconds, true);
}

void ADecoyRadioTruck::TryStartAfterGameStart()
{
	const ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	const UPlayerStartGameControl* const StartGameControl = IsValid(PlayerController)
		                                                        ? PlayerController->FindComponentByClass<
			                                                        UPlayerStartGameControl>()
		                                                        : nullptr;
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(M_StartTimerHandle);
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	EnemyPlacement::TryGetGameDifficulty(this, Difficulty);
	if (Difficulty < MinimumDifficulty)
	{
		UE_LOG(LogDecoyRadioTruck, Log, TEXT("Decoy radio truck %s: difficulty %d is below %d, no truck."), *GetName(),
		       static_cast<int32>(Difficulty), static_cast<int32>(MinimumDifficulty));
		return;
	}
	UE_LOG(LogDecoyRadioTruck, Log, TEXT("Decoy radio truck %s: spawning truck and escort (difficulty %d)."),
	       *GetName(), static_cast<int32>(Difficulty));
	SpawnVehicles();
}

FVector ADecoyRadioTruck::GetRoutePointWorld(const int32 PointIndex) const
{
	return Route.IsValidIndex(PointIndex) ? GetActorTransform().TransformPosition(Route[PointIndex].Location)
		       : GetActorLocation();
}

void ADecoyRadioTruck::SpawnVehicles()
{
	ARTSAsyncSpawner* const Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner))
	{
		RTSFunctionLibrary::ReportError("Decoy radio truck " + GetName() + ": no async spawner, nothing spawns.");
		return;
	}
	const FVector Start = GetRoutePointWorld(0);
	const FVector Next = GetRoutePointWorld(1);
	const FRotator Facing = (Next - Start).GetSafeNormal2D().Rotation();
	const FVector EscortStart = Start - (Next - Start).GetSafeNormal2D() * EscortTrailCm;
	TWeakObjectPtr<ADecoyRadioTruck> WeakThis(this);
	// The Blueprints keep their own (Soviet) owner.
	Spawner->AsyncSpawnClassAtLocation(TruckClass, TruckOption, Start, this, DecoyRadioTruckConstants::TruckSpawnId,
	                                   [WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
	                                   {
		                                   if (WeakThis.IsValid())
		                                   {
			                                   WeakThis->OnTruckSpawned(SpawnedActor);
		                                   }
	                                   }, Facing);
	Spawner->AsyncSpawnOptionAtLocation(EscortOption, EscortStart, this, DecoyRadioTruckConstants::EscortSpawnId,
	                                    [WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
	                                    {
		                                    if (WeakThis.IsValid())
		                                    {
			                                    WeakThis->OnEscortSpawned(SpawnedActor);
		                                    }
	                                    }, Facing);
}

void ADecoyRadioTruck::OnTruckSpawned(AActor* SpawnedActor)
{
	if (not IsValid(SpawnedActor))
	{
		RTSFunctionLibrary::ReportError("Decoy radio truck " + GetName() + ": the truck did not spawn.");
		return;
	}
	M_Vehicles.Truck = SpawnedActor;
	UE_LOG(LogDecoyRadioTruck, Log, TEXT("Decoy radio truck %s: truck %s spawned."), *GetName(), *SpawnedActor->GetName());
	// The first point is the start: its wait runs before the drive begins.
	M_RoutePointIndex = 0;
	HandleArrival();
	GetWorldTimerManager().SetTimer(M_DriveTimerHandle, this, &ADecoyRadioTruck::TickDrive,
	                                DecoyRadioTruckConstants::DriveTickSeconds, true);
}

void ADecoyRadioTruck::OnEscortSpawned(AActor* SpawnedActor)
{
	M_Vehicles.Escort = SpawnedActor;
	if (M_RoutePointIndex > 0)
	{
		OrderToRoutePoint(M_RoutePointIndex);
	}
}

void ADecoyRadioTruck::OrderMove(AActor* Vehicle, const FVector& Location, const FRotator& Facing)
{
	ICommands* const Commands = Cast<ICommands>(Vehicle);
	if (Commands == nullptr)
	{
		return;
	}
	Commands->MoveToLocation(Location, true, Facing, false);
}

void ADecoyRadioTruck::OrderToRoutePoint(const int32 PointIndex) const
{
	const FVector Target = GetRoutePointWorld(PointIndex);
	const FVector From = GetRoutePointWorld(FMath::Max(0, PointIndex - 1));
	const FVector Direction = (Target - From).GetSafeNormal2D();
	const FRotator Facing = Direction.Rotation();
	OrderMove(M_Vehicles.Truck.Get(), Target, Facing);
	OrderMove(M_Vehicles.Escort.Get(), Target - Direction * EscortTrailCm, Facing);
}

void ADecoyRadioTruck::TickDrive()
{
	AActor* const Truck = M_Vehicles.Truck.Get();
	if (not UEnemyFallbackSubsystem::GetIsUnitActorAlive(Truck))
	{
		HandleTruckDestroyed();
		return;
	}
	if (bM_IsTransmitting)
	{
		return;
	}
	const double NowSeconds = GetWorld()->GetTimeSeconds();
	if (M_WaitUntilSeconds >= 0.0)
	{
		if (NowSeconds < M_WaitUntilSeconds)
		{
			return;
		}
		M_WaitUntilSeconds = -1.0;
		++M_RoutePointIndex;
		OrderToRoutePoint(M_RoutePointIndex);
		return;
	}
	if (FVector::Dist2D(Truck->GetActorLocation(), GetRoutePointWorld(M_RoutePointIndex)) <= ArriveRadiusCm)
	{
		HandleArrival();
	}
}

void ADecoyRadioTruck::HandleArrival()
{
	if (M_RoutePointIndex >= Route.Num() - 1)
	{
		StartTransmitting();
		return;
	}
	// Waiting 0 s still goes through one drive tick, which keeps every point on the same path.
	M_WaitUntilSeconds = GetWorld()->GetTimeSeconds() + Route[M_RoutePointIndex].WaitSeconds;
}

void ADecoyRadioTruck::StartTransmitting()
{
	bM_IsTransmitting = true;
	UE_LOG(LogDecoyRadioTruck, Log, TEXT("Decoy radio truck %s: parked, transmitting."), *GetName());
	if (GetIsValidDirectionFinding())
	{
		DirectionFinding->ActivateDecoy(M_Vehicles.Truck.IsValid() ? M_Vehicles.Truck->GetActorLocation()
			                                : GetRoutePointWorld(Route.Num() - 1));
	}
}

void ADecoyRadioTruck::HandleTruckDestroyed()
{
	if (bM_IsTruckDestroyed)
	{
		return;
	}
	bM_IsTruckDestroyed = true;
	bM_IsTransmitting = false;
	GetWorldTimerManager().ClearTimer(M_DriveTimerHandle);
	if (GetIsValidDirectionFinding())
	{
		DirectionFinding->DeactivateDecoy();
	}
	OnRadioTruckDestroyed.Broadcast();
}

bool ADecoyRadioTruck::GetIsValidDirectionFinding() const
{
	if (IsValid(DirectionFinding))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "DirectionFinding",
	                                                       "ADecoyRadioTruck::GetIsValidDirectionFinding", this);
	return false;
}
