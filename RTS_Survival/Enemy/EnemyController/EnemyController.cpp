// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyController.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFieldConstructionComponent/EnemyFieldConstructionComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyNavigationAIComponent/EnemyNavigationAIComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyRetreatController/EnemyRetreatController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyRetreatController/EnemyRetreatData.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Enemy/EnemyWaves/EnemyWaveController.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
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
	M_EnemyRetreatController = CreateDefaultSubobject<UEnemyRetreatController>(TEXT("EnemyRetreatController"));
	CacheGenerationSeedFromGameInstance();
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
	if (M_EnemyRetreatController)
	{
		M_EnemyRetreatController->InitRetreatController(this);
	}
	
}

void AEnemyController::MoveFormationToLocation(const TArray<ASquadController*>& SquadControllers,
                                               const TArray<ATankMaster*>& TankMasters,
                                               const TArray<FVector>& Waypoints,
                                               const FRotator& FinalWaypointDirection,
                                               const int32 MaxFormationWidth,
                                               const float FormationOffsetMlt,
                                               const FVector& AverageSpawnLocation)
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
		FormationOffsetMlt,
		AverageSpawnLocation);
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

void AEnemyController::CreateSingleRandomPatrolWithAttackMoveWave(
	const TArray<FVector>& SpawnLocations,
	const TArray<FTrainingOption>& TrainingOptions,
	const TArray<FVector>& PatrolPoints,
	const int32 OverrideFirstPatrolPointIndex,
	const int32 AmountIterationsAtPatrolPoint,
	const float GuardTimePerPatrolPointIteration,
	const float GuardSphereRadius,
	const int32 MaxFormationWidth,
	const float TimeTillPatrol,
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
	if (SpawnLocations.IsEmpty() || TrainingOptions.IsEmpty() || PatrolPoints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"CreateSingleRandomPatrolWithAttackMoveWave requires non-empty spawn locations, training options and patrol points.");
		return;
	}

	TArray<FAttackWaveElement> WaveElements;
	WaveElements.Reserve(SpawnLocations.Num());
	for (const FVector& SpawnLocation : SpawnLocations)
	{
		FAttackWaveElement WaveElement;
		WaveElement.SpawnLocation = SpawnLocation;
		WaveElement.UnitOptions = TrainingOptions;
		WaveElements.Add(WaveElement);
	}

	M_WaveController->StartSingleRandomPatrolWithAttackMoveWave(
		EEnemyWaveType::Wave_NoOwningBuilding,
		WaveElements,
		PatrolPoints,
		OverrideFirstPatrolPointIndex,
		AmountIterationsAtPatrolPoint,
		GuardTimePerPatrolPointIteration,
		GuardSphereRadius,
		MaxFormationWidth,
		TimeTillPatrol,
		nullptr,
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
		if (not TryGetSeededTrainingOption(WaveElement, PickedOption))
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

TArray<AActor*> AEnemyController::GetAllTankActorsInFormations() const
{
	return GetAllFormationActorsByUnitType(false);
}

TArray<AActor*> AEnemyController::GetAllSquadControllerActorsInFormations() const
{
	return GetAllFormationActorsByUnitType(true);
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
	const FAttackMoveWaveSettings& AttackMoveSettings,
	const FVector& AverageSpawnLocation)
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
		AttackMoveSettings,
		AverageSpawnLocation);
}

void AEnemyController::MoveRandomPatrolWithAttackMoveFormation(
	const TArray<ASquadController*>& SquadControllers,
	const TArray<ATankMaster*>& TankMasters,
	const TArray<FVector>& PatrolPoints,
	const int32 OverrideFirstPatrolPointIndex,
	const int32 AmountIterationsAtPatrolPoint,
	const float GuardTimePerPatrolPointIteration,
	const float GuardSphereRadius,
	const int32 MaxFormationWidth,
	const float FormationOffsetMlt,
	const FAttackMoveWaveSettings& AttackMoveSettings,
	const FVector& AverageSpawnLocation)
{
	if (not GetIsValidFormationController())
	{
		return;
	}

	M_FormationController->MoveRandomPatrolWithAttackMoveFormation(
		SquadControllers,
		TankMasters,
		PatrolPoints,
		OverrideFirstPatrolPointIndex,
		AmountIterationsAtPatrolPoint,
		GuardTimePerPatrolPointIteration,
		GuardSphereRadius,
		MaxFormationWidth,
		FormationOffsetMlt,
		AttackMoveSettings,
		AverageSpawnLocation);
}

void AEnemyController::RetreatAllFormations(
	const FVector& CounterattackLocation,
	const EPostRetreatCounterStrategy PostRetreatCounterStrategy,
	const float TileTillCounterAttackAfterLastRetreatingUnitReached,
	const float MaxTimeWaitTillCounterAttack)
{
	if (not GetIsValidFormationController() || not GetIsValidEnemyRetreatController())
	{
		return;
	}

	TArray<FFormationData> ActiveFormations;
	M_FormationController->GetActiveFormationData(ActiveFormations);
	if (ActiveFormations.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"No active formations found to retreat. \nAt AEnemyController::RetreatAllFormations()");
		return;
	}

	TArray<FRetreatElement> RetreatingSquadControllers;
	TArray<FRetreatElement> ReverseRetreatUnits;
	TArray<int32> FormationIDsToRemoveAfterRetreat;

	for (const FFormationData& Formation : ActiveFormations)
	{
		bool bHasIssuedRetreatOrderForFormation = false;
		const FVector RetreatLocation = Formation.AverageSpawnLocation;
		for (const FFormationUnitData& FormationUnit : Formation.FormationUnits)
		{
			if (not FormationUnit.Unit.IsValid())
			{
				continue;
			}

			ICommands* Commands = FormationUnit.Unit.Get();
			if (not Commands)
			{
				continue;
			}

			if (Commands->GetIsSquadUnit())
			{
				const ECommandQueueError CommandError = Commands->RetreatToLocation(RetreatLocation, true);
				if (CommandError == ECommandQueueError::NoError)
				{
					FRetreatElement RetreatElement;
					RetreatElement.Unit = FormationUnit.Unit;
					RetreatElement.RetreatLocation = RetreatLocation;
					RetreatingSquadControllers.Add(RetreatElement);
					bHasIssuedRetreatOrderForFormation = true;
				}
				continue;
			}

			const ECommandQueueError CommandError = Commands->ReverseUnitToLocation(RetreatLocation, true);
			if (CommandError == ECommandQueueError::NoError)
			{
				FRetreatElement RetreatElement;
				RetreatElement.Unit = FormationUnit.Unit;
				RetreatElement.RetreatLocation = RetreatLocation;
				ReverseRetreatUnits.Add(RetreatElement);
				bHasIssuedRetreatOrderForFormation = true;
			}
		}

		if (bHasIssuedRetreatOrderForFormation)
		{
			FormationIDsToRemoveAfterRetreat.Add(Formation.FormationID);
		}
	}

	if (RetreatingSquadControllers.IsEmpty() && ReverseRetreatUnits.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"No valid retreat units found to track. \nAt AEnemyController::RetreatAllFormations()");
		return;
	}

	M_FormationController->RemoveActiveFormationsByID(FormationIDsToRemoveAfterRetreat);

	M_EnemyRetreatController->StartRetreat(
		RetreatingSquadControllers,
		ReverseRetreatUnits,
		CounterattackLocation,
		PostRetreatCounterStrategy,
		TileTillCounterAttackAfterLastRetreatingUnitReached,
		MaxTimeWaitTillCounterAttack);
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
	CacheGenerationSeedFromGameInstance();
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

bool AEnemyController::GetIsValidEnemyRetreatController() const
{
	if (not IsValid(M_EnemyRetreatController))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"M_EnemyRetreatController",
			"AEnemyController::GetIsValidEnemyRetreatController",
			this);
		return false;
	}
	return true;
}

void AEnemyController::CacheGenerationSeedFromGameInstance()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	UGameInstance* const GameInstance = World->GetGameInstance();
	URTSGameInstance* const RTSGameInstance = Cast<URTSGameInstance>(GameInstance);
	if (not IsValid(RTSGameInstance))
	{
		RTSFunctionLibrary::ReportError("Enemy controller could not cache generation seed from game instance.");
		return;
	}

	const FCampaignGenerationSettings CampaignGenerationSettings = RTSGameInstance->GetCampaignGenerationSettings();
	M_CachedGenerationSeed = CampaignGenerationSettings.GenerationSeed;
}

int32 AEnemyController::GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt) const
{
	if (OptionCount <= 0)
	{
		return INDEX_NONE;
	}

	const uint32 CombinedSeed = static_cast<uint32>(M_CachedGenerationSeed)
		+ static_cast<uint32>(DecisionSalt)
		+ static_cast<uint32>(M_SeedDecisionCounter);
	FRandomStream SeededRandomStream(static_cast<int32>(CombinedSeed));
	++M_SeedDecisionCounter;
	return SeededRandomStream.RandRange(0, OptionCount - 1);
}

float AEnemyController::GetSeededFloatInRange(const float MinValue, const float MaxValue, const int32 DecisionSalt) const
{
	const uint32 CombinedSeed = static_cast<uint32>(M_CachedGenerationSeed)
		+ static_cast<uint32>(DecisionSalt)
		+ static_cast<uint32>(M_SeedDecisionCounter);
	FRandomStream SeededRandomStream(static_cast<int32>(CombinedSeed));
	++M_SeedDecisionCounter;
	return SeededRandomStream.FRandRange(MinValue, MaxValue);
}

bool AEnemyController::TryGetSeededTrainingOption(
	const FAttackWaveElement& WaveElement,
	FTrainingOption& OutTrainingOption) const
{
	if (WaveElement.UnitOptions.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Field construction wave element has no unit options.");
		return false;
	}

	const int32 SeededIndex = GetSeededIndex(WaveElement.UnitOptions.Num(), 173);
	if (not WaveElement.UnitOptions.IsValidIndex(SeededIndex))
	{
		RTSFunctionLibrary::ReportError(
			"Field construction wave element seeded index was invalid.");
		return false;
	}

	OutTrainingOption = WaveElement.UnitOptions[SeededIndex];
	return true;
}

TArray<AActor*> AEnemyController::GetAllFormationActorsByUnitType(const bool bGetSquadActors) const
{
	TArray<AActor*> ValidFormationActors;
	if (not GetIsValidFormationController())
	{
		return ValidFormationActors;
	}

	TArray<FFormationData> ActiveFormations;
	M_FormationController->GetActiveFormationData(ActiveFormations);

	for (const FFormationData& FormationData : ActiveFormations)
	{
		for (const FFormationUnitData& FormationUnitData : FormationData.FormationUnits)
		{
			if (not FormationUnitData.Unit.IsValid())
			{
				continue;
			}

			ICommands* Commands = FormationUnitData.Unit.Get();
			if (not Commands)
			{
				continue;
			}

			const bool bIsSquadUnit = Commands->GetIsSquadUnit();
			if (bIsSquadUnit != bGetSquadActors)
			{
				continue;
			}

			AActor* OwnerActor = Commands->GetOwnerActor();
			if (not RTSFunctionLibrary::RTSIsValid(OwnerActor))
			{
				continue;
			}

			ValidFormationActors.Add(OwnerActor);
		}
	}

	return ValidFormationActors;
}
