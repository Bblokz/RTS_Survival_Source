// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyController.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFieldConstructionComponent/EnemyFieldConstructionComponent.h"
#include "RTS_Survival/Enemy/EnemyWaves/EnemyWaveController.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/EnemyControllerSubsystem/EnemyControllerSubsystem.h"


AEnemyController::AEnemyController(const FObjectInitializer& ObjectInitializer)
	: AActorObjectsMaster(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	M_FormationController = CreateDefaultSubobject<UEnemyFormationController>(TEXT("FormationController"));
	M_WaveController = CreateDefaultSubobject<UEnemyWaveController>(TEXT("WaveController"));
	M_FieldConstructionComponent = CreateDefaultSubobject<UEnemyFieldConstructionComponent>(TEXT("FieldConstructionComponent"));
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
	M_FormationController->MoveFormationToLocation(SquadControllers, TankMasters, Waypoints, FinalWaypointDirection, MaxFormationWidth);
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
		PerAffectingBuildingTimerFraction);
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

void AEnemyController::CreateFieldConstructionOrder(
	const TArray<ASquadController*>& SquadControllers,
	const TArray<FVector>& ConstructionLocations,
	const EFieldConstructionStrategy Strategy)
{
	if (not GetIsValidFieldConstructionComponent())
	{
		return;
	}

	M_FieldConstructionComponent->CreateFieldConstructionOrder(
		SquadControllers,
		ConstructionLocations,
		Strategy);
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

void AEnemyController::AddToWaveSupply(const int32 AddSupply)
{
	M_Resources.WaveSupply += AddSupply;
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
