// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyController.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFieldConstructionComponent/EnemyFieldConstructionComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyNavigationAIComponent/EnemyNavigationAIComponent.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Enemy/EnemyWaves/EnemyWaveController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/EnemyControllerSubsystem/EnemyControllerSubsystem.h"

namespace EnemyControllerFieldConstruction
{
	struct FFieldConstructionSpawnRequest
	{
		FTrainingOption TrainingOption;
		FVector SpawnLocation = FVector::ZeroVector;
	};

	struct FFieldConstructionSpawnContext
	{
		TWeakObjectPtr<AEnemyController> EnemyController = nullptr;
		TArray<ASquadController*> SpawnedSquadControllers;
		TArray<FVector> ConstructionLocations;
		FFindAddtionalLocationsStrategy AdditionalLocationsStrategy;
		EFieldConstructionStrategy Strategy = EFieldConstructionStrategy::None;
		int32 PendingSpawnCount = 0;
		bool bHasCompleted = false;

		void RegisterSpawnedActor(AActor* SpawnedActor)
		{
			if (not IsValid(SpawnedActor))
			{
				RTSFunctionLibrary::ReportError(
					"Field construction wave spawn failed: Spawned actor was invalid.");
				return;
			}

			ASquadController* SquadController = Cast<ASquadController>(SpawnedActor);
			if (not IsValid(SquadController))
			{
				RTSFunctionLibrary::ReportError(
					"Field construction wave spawn failed: Expected SquadController actor.");
				SpawnedActor->Destroy();
				return;
			}

			SpawnedSquadControllers.Add(SquadController);
		}

		void RegisterSpawnFailure()
		{
			RTSFunctionLibrary::ReportError("Field construction wave spawn request failed.");
		}

		void NotifySpawnComplete()
		{
			PendingSpawnCount--;
			if (PendingSpawnCount > 0 || bHasCompleted)
			{
				return;
			}

			bHasCompleted = true;
			if (EnemyController.IsValid())
			{
				EnemyController->CreateFieldConstructionOrder(
					SpawnedSquadControllers,
					ConstructionLocations,
					AdditionalLocationsStrategy,
					Strategy);
			}

			Cleanup();
		}

		void Cleanup()
		{
			SpawnedSquadControllers.Empty();
			ConstructionLocations.Empty();
			PendingSpawnCount = 0;
		}
	};

	bool TryGetRandomTrainingOption(const FAttackWaveElement& WaveElement, FTrainingOption& OutTrainingOption)
	{
		if (WaveElement.UnitOptions.IsEmpty())
		{
			RTSFunctionLibrary::ReportError("Field construction wave element has no unit options.");
			return false;
		}

		const int32 RandomIndex = FMath::RandRange(0, WaveElement.UnitOptions.Num() - 1);
		if (not WaveElement.UnitOptions.IsValidIndex(RandomIndex))
		{
			RTSFunctionLibrary::ReportError(
				"Field construction wave element random index was invalid.");
			return false;
		}

		OutTrainingOption = WaveElement.UnitOptions[RandomIndex];
		return true;
	}
}


AEnemyController::AEnemyController(const FObjectInitializer& ObjectInitializer)
	: AActorObjectsMaster(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	M_FormationController = CreateDefaultSubobject<UEnemyFormationController>(TEXT("FormationController"));
	M_WaveController = CreateDefaultSubobject<UEnemyWaveController>(TEXT("WaveController"));
	M_FieldConstructionComponent = CreateDefaultSubobject<UEnemyFieldConstructionComponent>(TEXT("FieldConstructionComponent"));
	M_EnemyNavigationAIComponent = CreateDefaultSubobject<UEnemyNavigationAIComponent>(TEXT("EnemyNavigationAIComponent"));
	M_EnemyStrategicAIComponent = CreateDefaultSubobject<UEnemyStrategicAIComponent>(TEXT("EnemyStrategicAIComponent"));
	if(M_FormationController)
	{
		M_FormationController->InitFormationController(this);
	}
	if(M_WaveController)
	{
		M_WaveController->InitWaveController(this);

	}
	if (M_FieldConstructionComponent)
	{
		M_FieldConstructionComponent->InitFieldConstructionComponent(this);
	}
	if (M_EnemyNavigationAIComponent)
	{
		M_EnemyNavigationAIComponent->InitNavigationAIComponent(this);
	}
	if (M_EnemyStrategicAIComponent)
	{
		M_EnemyStrategicAIComponent->InitStrategicAIComponent(this);
	}
	
}

void AEnemyController::MoveFormationToLocation(const TArray<ASquadController*>& SquadControllers,
                                               const TArray<ATankMaster*>& TankMasters,
                                               const TArray<FVector>& Waypoints,
                                               const FRotator& FinalWaypointDirection,
                                               const int32 MaxFormationWidth,
                                               const float FormationOffsetMlt)
{
	if(not GetIsValidFormationController())
	{
		return;
	}
	M_FormationController->MoveFormationToLocation(
		SquadControllers,
		TankMasters,
		Waypoints,
		FinalWaypointDirection,
		MaxFormationWidth,
		FormationOffsetMlt);
}

void AEnemyController::CreateAttackWave(const EEnemyWaveType WaveType, const TArray<FAttackWaveElement>& WaveElements,
                                        const float WaveInterval, const float IntervalVarianceFraction, const TArray<FVector>& Waypoints,
                                        const FRotator& FinalWaypointDirection, const int32 MaxFormationWidth, const bool bInstantStart,
                                        AActor* WaveCreator, TArray<AActor*> WaveTimerAffectingBuildings, const float PerAffectingBuildingTimerFraction, const
                                        float FormationOffsetMultiplier)
{
	TArray<TWeakObjectPtr<AActor>> WeakBuildings;
	for(AActor* Building : WaveTimerAffectingBuildings)
	{
		WeakBuildings.Add(Building);
	}
	if(not GetIsValidWaveController())
	{
		return;
	}
	M_WaveController->StartNewAttackWave(
		WaveType, WaveElements, WaveInterval, IntervalVarianceFraction, Waypoints,
		FinalWaypointDirection, MaxFormationWidth, bInstantStart, WaveCreator, WeakBuildings,
		PerAffectingBuildingTimerFraction, FormationOffsetMultiplier);
}

void AEnemyController::CreateSingleAttackWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	const int32 MaxFormationWidth,
	const float TimeTillWave,
	AActor* WaveCreator, const float FormationOffsetMultiplier)
{
	if (not GetIsValidWaveController())
	{
		return;
	}
	const float ClampedTimeTillWave = FMath::Max(TimeTillWave, 0.f);
	M_WaveController->StartSingleAttackWave(
		WaveType,
		WaveElements,
		Waypoints,
		FinalWaypointDirection,
		MaxFormationWidth,
		ClampedTimeTillWave,
		WaveCreator,
		FormationOffsetMultiplier);
}

void AEnemyController::CreateAttackMoveWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const float WaveInterval,
	const float IntervalVarianceFraction,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	const int32 MaxFormationWidth,
	const bool bInstantStart,
	AActor* WaveCreator,
	TArray<AActor*> WaveTimerAffectingBuildings,
	const float PerAffectingBuildingTimerFraction,
	const float FormationOffsetMultiplier,
	const float HelpOffsetRadiusMltMax,
	const float HelpOffsetRadiusMltMin,
	const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
	const int32 MaxTriesFindNavPointForHelpOffset,
	const float ProjectionScale)
{
	TArray<TWeakObjectPtr<AActor>> WeakBuildings;
	for (AActor* Building : WaveTimerAffectingBuildings)
	{
		WeakBuildings.Add(Building);
	}
	if (not GetIsValidWaveController())
	{
		return;
	}

	M_WaveController->StartNewAttackMoveWave(
		WaveType,
		WaveElements,
		WaveInterval,
		IntervalVarianceFraction,
		Waypoints,
		FinalWaypointDirection,
		MaxFormationWidth,
		bInstantStart,
		WaveCreator,
		WeakBuildings,
		PerAffectingBuildingTimerFraction,
		FormationOffsetMultiplier,
		HelpOffsetRadiusMltMax,
		HelpOffsetRadiusMltMin,
		MaxAttackTimeBeforeAdvancingToNextWayPoint,
		MaxTriesFindNavPointForHelpOffset,
		ProjectionScale);
}

void AEnemyController::CreateSingleAttackMoveWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	const int32 MaxFormationWidth,
	const float TimeTillWave,
	AActor* WaveCreator,
	const float FormationOffsetMultiplier,
	const float HelpOffsetRadiusMltMax,
	const float HelpOffsetRadiusMltMin,
	const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
	const int32 MaxTriesFindNavPointForHelpOffset,
	const float ProjectionScale)
{
	if (not GetIsValidWaveController())
	{
		return;
	}

	const float ClampedTimeTillWave = FMath::Max(TimeTillWave, 0.f);
	M_WaveController->StartSingleAttackMoveWave(
		WaveType,
		WaveElements,
		Waypoints,
		FinalWaypointDirection,
		MaxFormationWidth,
		ClampedTimeTillWave,
		WaveCreator,
		FormationOffsetMultiplier,
		HelpOffsetRadiusMltMax,
		HelpOffsetRadiusMltMin,
		MaxAttackTimeBeforeAdvancingToNextWayPoint,
		MaxTriesFindNavPointForHelpOffset,
		ProjectionScale);
}

void AEnemyController::CreateFieldConstructionOrder(
	const TArray<ASquadController*>& SquadControllers,
	const TArray<FVector>& ConstructionLocations,
	const FFindAddtionalLocationsStrategy& AdditionalLocationsStrategy,
	const EFieldConstructionStrategy Strategy)
{
	if (not GetIsValidFieldConstructionComponent())
	{
		return;
	}

	M_FieldConstructionComponent->CreateFieldConstructionOrder(
		SquadControllers,
		ConstructionLocations,
		AdditionalLocationsStrategy,
		Strategy);
}

void AEnemyController::CreateFieldConstructionOrderFromWaveElements(
	const TArray<FAttackWaveElement>& WaveElements,
	const TArray<FVector>& ConstructionLocations,
	const FFindAddtionalLocationsStrategy& AdditionalLocationsStrategy,
	const EFieldConstructionStrategy Strategy)
{
	using namespace EnemyControllerFieldConstruction;

	if (not GetIsValidFieldConstructionComponent())
	{
		return;
	}

	if (WaveElements.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Field construction wave elements list is empty.");
		return;
	}

	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Field construction requires a valid async spawner.");
		return;
	}

	TArray<FFieldConstructionSpawnRequest> SpawnRequests;
	SpawnRequests.Reserve(WaveElements.Num());
	for (const FAttackWaveElement& WaveElement : WaveElements)
	{
		FTrainingOption PickedOption;
		if (not TryGetRandomTrainingOption(WaveElement, PickedOption))
		{
			continue;
		}

		SpawnRequests.Add({PickedOption, WaveElement.SpawnLocation});
	}

	if (SpawnRequests.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Field construction wave spawn requests are empty.");
		return;
	}

	TSharedPtr<FFieldConstructionSpawnContext> SpawnContext = MakeShared<FFieldConstructionSpawnContext>();
	SpawnContext->EnemyController = this;
	SpawnContext->ConstructionLocations = ConstructionLocations;
	SpawnContext->AdditionalLocationsStrategy = AdditionalLocationsStrategy;
	SpawnContext->Strategy = Strategy;
	SpawnContext->PendingSpawnCount = SpawnRequests.Num();

	TWeakPtr<FFieldConstructionSpawnContext> WeakSpawnContext = SpawnContext;
	TWeakObjectPtr<AEnemyController> WeakEnemyController(this);
	int32 SpawnRequestId = 0;
	for (const FFieldConstructionSpawnRequest& SpawnRequest : SpawnRequests)
	{
		const int32 CurrentSpawnRequestId = SpawnRequestId++;
		auto OnSpawned = [WeakSpawnContext](const FTrainingOption& TrainingOption, AActor* SpawnedActor,
		                                    const int32 SpawnRequestId)
		{
			static_cast<void>(TrainingOption);
			static_cast<void>(SpawnRequestId);

			const TSharedPtr<FFieldConstructionSpawnContext> PinnedSpawnContext = WeakSpawnContext.Pin();
			if (not PinnedSpawnContext.IsValid())
			{
				return;
			}

			PinnedSpawnContext->RegisterSpawnedActor(SpawnedActor);
			PinnedSpawnContext->NotifySpawnComplete();
		};

		if (not AsyncSpawner->AsyncSpawnOptionAtLocation(
			SpawnRequest.TrainingOption,
			SpawnRequest.SpawnLocation,
			WeakEnemyController,
			CurrentSpawnRequestId,
			OnSpawned))
		{
			SpawnContext->RegisterSpawnFailure();
			SpawnContext->NotifySpawnComplete();
		}
	}
}

void AEnemyController::SetFieldConstructionOrderInterval(const float NewIntervalSeconds)
{
	if (not GetIsValidFieldConstructionComponent())
	{
		return;
	}

	M_FieldConstructionComponent->SetFieldConstructionOrderInterval(NewIntervalSeconds);
}

void AEnemyController::DebugAllActiveFormations() const
{
	if(not GetIsValidFormationController())
	{
	return;	
	}
	M_FormationController->DebugAllActiveFormations();
}

void AEnemyController::MoveAttackMoveFormationToLocation(
	const TArray<ASquadController*>& SquadControllers,
	const TArray<ATankMaster*>& TankMasters,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	const int32 MaxFormationWidth,
	const float FormationOffsetMlt,
	const FAttackMoveWaveSettings& AttackMoveSettings)
{
	if (not GetIsValidFormationController())
	{
		return;
	}

	M_FormationController->MoveAttackMoveFormationToLocation(
		SquadControllers,
		TankMasters,
		Waypoints,
		FinalWaypointDirection,
		MaxFormationWidth,
		FormationOffsetMlt,
		AttackMoveSettings);
}

void AEnemyController::AddToWaveSupply(const int32 AddSupply)
{
	M_Resources.WaveSupply += AddSupply;
}

UEnemyNavigationAIComponent* AEnemyController::GetEnemyNavigationAIComponent() const
{
	if (not GetIsValidEnemyNavigationAIComponent())
	{
		return nullptr;
	}

	return M_EnemyNavigationAIComponent;
}

UEnemyStrategicAIComponent* AEnemyController::GetEnemyStrategicAIComponent() const
{
	if (not GetIsValidEnemyStrategicAIComponent())
	{
		return nullptr;
	}

	return M_EnemyStrategicAIComponent;
}

void AEnemyController::BeginPlay()
{
	Super::BeginPlay();
}


void AEnemyController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Register this controller with the world subsystem
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	UEnemyControllerSubsystem* Subsystem = World->GetSubsystem<UEnemyControllerSubsystem>();
	if (not IsValid(Subsystem))
	{
		RTSFunctionLibrary::ReportError(TEXT("Could not find EnemyControllerSubsystem to register controller."));
		return;
	}

	Subsystem->SetEnemyController(this);
}

bool AEnemyController::GetIsValidFormationController() const
{
	if(not IsValid(M_FormationController))
	{
		RTSFunctionLibrary::ReportError("Invalid formation controller on enemy controller!");
		return false;
	}
	return true;
}

bool AEnemyController::GetIsValidWaveController() const
{
	if(not IsValid(M_WaveController))
	{
		RTSFunctionLibrary::ReportError("Invalid wave controller for enemy controller!");
		return false;
	}
	return true;
}

bool AEnemyController::GetIsValidFieldConstructionComponent() const
{
	if (not IsValid(M_FieldConstructionComponent))
	{
		RTSFunctionLibrary::ReportError("Invalid field construction component for enemy controller!");
		return false;
	}
	return true;
}

bool AEnemyController::GetIsValidEnemyNavigationAIComponent() const
{
	if (not IsValid(M_EnemyNavigationAIComponent))
	{
		RTSFunctionLibrary::ReportError("Invalid enemy navigation AI component for enemy controller!");
		return false;
	}
	return true;
}

bool AEnemyController::GetIsValidEnemyStrategicAIComponent() const
{
	if (not IsValid(M_EnemyStrategicAIComponent))
	{
		RTSFunctionLibrary::ReportError("Invalid enemy strategic AI component for enemy controller!");
		return false;
	}
	return true;
}
