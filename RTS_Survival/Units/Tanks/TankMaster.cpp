// Copyright Bas Blokzijl - All rights reserved.


#include "TankMaster.h"

#include "AITankMaster.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/Audio/RTSVoiceLineHelpers/RTS_VoiceLineHelpers.h"
#include "RTS_Survival/Audio/SpacialVoiceLinePlayer/SpatialVoiceLinePlayer.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTS_Survival/UnitData/NomadicVehicleData.h"
#include "RTS_Survival/UnitData/VehicleData.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachedWeaponAbilityComponent.h"
#include "TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"
#include "VehicleAI/Components/VehiclePathFollowingComponent.h"
#include "Kismet/KismetMathLibrary.h"

FTankStartGameAction::FTankStartGameAction()
	: StartGameAction(EAbilityID::IdNoAbility)
	  , bM_BeginPlayCalled(false)
{
}

void FTankStartGameAction::InitStartGameAction(const EAbilityID InAbilityID, AActor* InTargetActor,
                                               const FVector& InTargetLocation, ATankMaster* InTankMaster,
                                               const FRotator& InEndRotation)
{
	if (not IsValid(InTankMaster))
	{
		return;
	}

	StartGameAction = InAbilityID;
	TargetActor = InTargetActor;
	TargetLocation = InTargetLocation;
	EndRotation = InEndRotation;
	M_TankMaster = InTankMaster;

	if (bM_BeginPlayCalled)
	{
		StartTimerForNextFrame();
	}
}

void FTankStartGameAction::OnBeginPlay()
{
	bM_BeginPlayCalled = true;
	StartTimerForNextFrame();
}

void FTankStartGameAction::OnTankDestroyed(UWorld* World)
{
	if (not World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(ActionTimer);
}

void FTankStartGameAction::TimerIteration()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	if (StartGameAction == EAbilityID::IdNoAbility)
	{
		ClearActionTimer();
		return;
	}

	(void)ExecuteStartAbility();
	ClearActionTimer();
}

void FTankStartGameAction::StartTimerForNextFrame()
{
	if (StartGameAction == EAbilityID::IdNoAbility)
	{
		return;
	}

	if (not GetIsValidTankMaster())
	{
		return;
	}

	if (UWorld* World = M_TankMaster->GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimer);
		FTimerDelegate TimerDel;
		TimerDel.BindRaw(this, &FTankStartGameAction::TimerIteration);
		World->GetTimerManager().SetTimerForNextTick(TimerDel);
	}
}

void FTankStartGameAction::ClearActionTimer()
{
	if (not M_TankMaster.IsValid())
	{
		return;
	}

	if (UWorld* World = M_TankMaster->GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimer);
	}
}

bool FTankStartGameAction::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("TankMaster is not valid!"
		"\n At function: FTankStartGameAction::GetIsValidTankMaster");
	return false;
}

bool FTankStartGameAction::ExecuteStartAbility() const
{
	switch (StartGameAction)
	{
	case EAbilityID::IdNoAbility:
		break;
	case EAbilityID::IdNoAbility_MoveCloserToTarget:
		break;
	case EAbilityID::IdNoAbility_MoveToEvasionLocation:
		break;
	case EAbilityID::IdIdle:
		break;
	case EAbilityID::IdGeneral_Confirm:
		break;
	case EAbilityID::IdAttack:
		return M_TankMaster->AttackActor(TargetActor, true) == ECommandQueueError::NoError;
	case EAbilityID::IdAttackGround:
		return M_TankMaster->AttackGround(TargetLocation, true) == ECommandQueueError::NoError;
	case EAbilityID::IdMove:
		return M_TankMaster->MoveToLocation(TargetLocation, true, EndRotation, false) ==
			ECommandQueueError::NoError;
	case EAbilityID::IdPatrol:
		return M_TankMaster->PatrolToLocation(TargetLocation, true) == ECommandQueueError::NoError;
	case EAbilityID::IdStop:
		break;
	case EAbilityID::IdSwitchWeapon:
		break;
	case EAbilityID::IdSwitchMelee:
		break;
	case EAbilityID::IdProne:
		break;
	case EAbilityID::IdCrouch:
		break;
	case EAbilityID::IdStand:
		break;
	case EAbilityID::IdSwitchSingleBurst:
		break;
	case EAbilityID::IdReverseMove:
		return M_TankMaster->ReverseUnitToLocation(TargetLocation, true) ==
			ECommandQueueError::NoError;
	case EAbilityID::IdRotateTowards:
		return M_TankMaster->RotateTowards(
			UKismetMathLibrary::FindLookAtRotation(M_TankMaster->GetActorLocation(), TargetLocation),
			true) == ECommandQueueError::NoError;
	case EAbilityID::IdCreateBuilding:
		break;
	case EAbilityID::IdConvertToVehicle:
		break;
	case EAbilityID::IdHarvestResource:
		break;
	case EAbilityID::IdReturnCargo:
		break;
	case EAbilityID::IdPickupItem:
		break;
	case EAbilityID::IdScavenge:
		return M_TankMaster->ScavengeObject(TargetActor, true) == ECommandQueueError::NoError;
	case EAbilityID::IdDigIn:
		return M_TankMaster->DigIn(true) == ECommandQueueError::NoError;
	case EAbilityID::IdBreakCover:
		break;
	case EAbilityID::IdFireRockets:
		break;
	case EAbilityID::IdCancelRocketFire:
		break;
	case EAbilityID::IdRocketsReloading:
		break;
	case EAbilityID::IdRepair:
		return M_TankMaster->RepairActor(TargetActor, true) == ECommandQueueError::NoError;
	case EAbilityID::IdReturnToBase:
		break;
	case EAbilityID::IdAircraftOwnerNotExpanded:
		break;
	case EAbilityID::IdEnterCargo:
		return M_TankMaster->EnterCargo(TargetActor, true) == ECommandQueueError::NoError;
	case EAbilityID::IdExitCargo:
		break;
	case EAbilityID::IdEnableResourceConversion:
		break;
	case EAbilityID::IdDisableResourceConversion:
		break;
	case EAbilityID::IdCapture:
		return M_TankMaster->CaptureActor(TargetActor, true) == ECommandQueueError::NoError;
	case EAbilityID::IdReinforceSquad:
		break;
	}
	return true;
}

ATankMaster::ATankMaster(const FObjectInitializer& ObjectInitializer)
	: ASelectablePawnMaster(ObjectInitializer), RTSNavCollision(nullptr), bM_NeedToTurnTowardsTarget(false),
	  bM_IsRotationAQueueCommand(false),
	  M_RotateToDirection(FRotator::ZeroRotator), M_TargetActor(nullptr),
	  M_CornerOffset(100)
{
	AITankController = nullptr;
	bIsHidden = false;
	M_SpatialVoiceLinePlayer = CreateDefaultSubobject<USpatialVoiceLinePlayer>(
		TEXT("SpatialVoiceLinePlayer"));
	M_OptimizationComponent = CreateDefaultSubobject<URTSOptimizer>(
		TEXT("RTSOptimizer"));
}


USkeletalMeshComponent* ATankMaster::GetTankMesh() const
{
	return nullptr;
}

void ATankMaster::SetTankStartGameAction(AActor* TargetActor, const FVector TargetLocation,
                                         const FRotator EndRotation, const EAbilityID StartGameAbility)
{
	M_TankStartGameAction.InitStartGameAction(StartGameAbility, TargetActor, TargetLocation, this, EndRotation);
}

void ATankMaster::UpgradeTurnRate(const float NewTurnRate)
{
	if (TurnRate <= 0)
	{
		RTSFunctionLibrary::ReportError("Attempted to update tank turnrate with <= 0 value"
			"\n ATankMaster::UpdateTurnRate"
			"\n for tank: " + GetName());
		return;
	}
	TurnRate = NewTurnRate;
}

void ATankMaster::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
	// Calls bp event.
	Super::OnHealthChanged(PercentageLeft, bIsHealing);
}

void ATankMaster::OnSquadRegistered(ASquadController* SquadController)
{
}

void ATankMaster::OnCargoEmpty()
{
}

void ATankMaster::GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const
{
	OutLocalOffsets.Reset();
	OutLocalOffsets = AimOffsetPoints;
}

void ATankMaster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bM_NeedToTurnTowardsTarget)
	{
		const FRotator CurrentRotation = GetActorRotation();
		FRotator RotationDifference = M_RotateToDirection - CurrentRotation;

		// Normalize to [-180,180] range
		RotationDifference.Normalize();
		float TurnAmount = FMath::Sign(RotationDifference.Yaw) * TurnRate * DeltaSeconds;

		// If we'd overshoot, clamp the turn
		if (FMath::Abs(TurnAmount) > FMath::Abs(RotationDifference.Yaw))
		{
			TurnAmount = RotationDifference.Yaw;
			bM_NeedToTurnTowardsTarget = false;
			if (bM_IsRotationAQueueCommand)
			{
				DoneExecutingCommand(EAbilityID::IdRotateTowards);
			}
			else
			{
				// Finished a rotation command that was not queued.
				OnFinishedStandaloneRotation();
			}
		}

		// Adjust the current rotation
		const FRotator NewRotation = CurrentRotation + FRotator(0, TurnAmount, 0);
		SetActorRotation(NewRotation, ETeleportType::TeleportPhysics);
	}
}

void ATankMaster::BeginPlay()
{
	Super::BeginPlay();
	const float Interval = 2.0f + FMath::RandRange(0.0f, 0.5f);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TimerHandle_CheckIfUpsideDown, this, &ATankMaster::CheckIfUpsideDown,
		                                  Interval, true);
		World->GetTimerManager().SetTimer(TimerHandle_SetupMaxSpeed, this,
		                                  &ATankMaster::SetMaxSpeedOnVehicleMovementComponent,
		                                  1.0f, false);
	}
	// Wait for bp begin play to set the subtype.
	BeginPlay_SetupData();

	BeginPlay_SetupCollisionVsBuildings();

	M_TankStartGameAction.OnBeginPlay();
}

void ATankMaster::BeginDestroy()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_CheckIfUpsideDown);
		World->GetTimerManager().ClearTimer(TimerHandle_SetupMaxSpeed);
		M_TankStartGameAction.OnTankDestroyed(World);
	}
	Super::BeginDestroy();
}

void ATankMaster::UnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		return;
	}
	OnUnitDies_CheckForCargo(DeathType);
	// Before announcer line; announcer is set to not interrupt.
	FRTS_VoiceLineHelpers::PlayUnitDeathVoiceLineOnRadio(this);
	OnUnitDies_AnnouncerDeathVoiceLine();
	SetUnitDying();
	if (IsValid(RTSComponent))
	{
		RTSComponent->DeregisterFromGameState();
	}
	OnUnitDies_DisableWeapons();
	// Before bp event as the bp event may destroy the actor immediately.
	OnUnitDies.Broadcast();
	BP_OnUnitDies();
}

void ATankMaster::CheckIfUpsideDown()
{
	const FRotator CurrentRotation = GetActorRotation();

	// Check if the actor is upside down
	if (FMath::Abs(CurrentRotation.Pitch) >= DeveloperSettings::GamePlay::Navigation::TankPitchRollConstrainAngle
		|| FMath::Abs(CurrentRotation.Roll) >= DeveloperSettings::GamePlay::Navigation::TankPitchRollConstrainAngle)
	{
		// Teleport the actor to the correct orientation
		const FRotator CorrectedRotation = {0, CurrentRotation.Yaw, 0};
		SetActorRotation(CorrectedRotation, ETeleportType::TeleportPhysics);
	}
}


void ATankMaster::PostInitializeComponents()
{
	// Inits hp component and RTS component.
	Super::PostInitializeComponents();
	RTSNavCollision = FindComponentByClass<URTSNavCollision>();
	if (!IsValid(RTSNavCollision))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this,
			"RTSNavCollision",
			"ATankMaster::PostInitializeComponents");
	}
	UActorComponent* PossibleHarvesterComp = GetComponentByClass(UHarvester::StaticClass());
	if (IsValid(PossibleHarvesterComp))
	{
		M_HarvesterComponent = Cast<UHarvester>(PossibleHarvesterComp);
		if (!IsValid(M_HarvesterComponent))
		{
			RTSFunctionLibrary::ReportFailedCastError("PossibleHarvesterComp", "UHarvester",
			                                          "ATankMaster::PostInitializeComponents");
		}
	}
	UBehaviourComp* BehaviourComp = FindComponentByClass<UBehaviourComp>();
	BehaviourComponent = BehaviourComp;
	(void)GetIsValidBehaviourComponent();

	TArray<USceneComponent*> SceneComponents;
	GetRootComponent()->GetChildrenComponents(true, SceneComponents);
	for (const auto EachSceneComp : SceneComponents)
	{
		if (not IsValid(EachSceneComp))
		{
			continue;
		}
		EachSceneComp->SetCanEverAffectNavigation(false);
	}
}

void ATankMaster::SetTurretsToAutoEngage(const bool bUseLastTarget)
{
	for (const auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			EachTurret->SetAutoEngageTargets(bUseLastTarget);
		}
	}
	for (const auto EachHullWeapon : HullWeapons)
	{
		if (GetIsValidHullWeapon(EachHullWeapon))
		{
			EachHullWeapon->SetAutoEngageTargets(bUseLastTarget);
		}
	}
}

void ATankMaster::SetTurretsDisabled()
{
	for (const auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			EachTurret->DisableTurret();
		}
	}
	for (const auto EachHullWeapon : HullWeapons)
	{
		if (GetIsValidHullWeapon(EachHullWeapon))
		{
			EachHullWeapon->DisableHullWeapon();
		}
	}
}

void ATankMaster::ChildBpSetupArmor(TArray<UArmor*> MyArmorSetup)
{
	for (auto EachElm : MyArmorSetup)
	{
		if (IsValid(EachElm))
		{
			M_TankArmor.Add(EachElm);
		}
	}
	for (auto EachValidArmor : M_TankArmor)
	{
	}
}

void ATankMaster::SetupCollisionForMeshAttachedToTracks(UMeshComponent* MeshToSetup, const bool bIsHarvester) const
{
	bool bOwnedByPlayer1 = false;
	if (GetIsValidRTSComponent())
	{
		bOwnedByPlayer1 = RTSComponent->GetOwningPlayer() == 1;
	}
	FRTS_CollisionSetup::SetupCollisionForMeshAttachedToTracks(MeshToSetup, bOwnedByPlayer1);
}


bool ATankMaster::GetIsValidBehaviourComponent() const
{
	if (not IsValid(BehaviourComponent))
	{
		RTSFunctionLibrary::ReportError("Invalid Behaviour Component on unit: " + GetName() +
			"\n ATankMaster::GetIsValidBehaviourComponent");
		return false;
	}
	return true;
}

void ATankMaster::ApplyMovementBehaviours()
{
	if (not GetIsValidBehaviourComponent())
	{
		return;
	}

	for (const TSubclassOf<UBehaviour>& MovementBehaviourClass : OnMovementBehaviour)
	{
		if (MovementBehaviourClass)
		{
			BehaviourComponent->AddBehaviour(MovementBehaviourClass);
		}
	}
}

void ATankMaster::RemoveMovementBehaviours()
{
	if (not GetIsValidBehaviourComponent())
	{
		return;
	}

	for (const TSubclassOf<UBehaviour>& MovementBehaviourClass : OnMovementBehaviour)
	{
		if (MovementBehaviourClass)
		{
			BehaviourComponent->RemoveBehaviour(MovementBehaviourClass);
		}
	}
}

void ATankMaster::OnUnitIdleAndNoNewCommands()
{
	// Important; calls the delegate in base ICommands.
	Super::OnUnitIdleAndNoNewCommands();
	const bool bAlwaysFinalRotate = GetForceFinalRotationRegardlessOfReverse();
	if (not bWasLastMovementReverse || bAlwaysFinalRotate)
	{
		// We finished our chain of commands and ended in a non-reverse movement; rotate towards the movement direction.
		RotateTowardsFinalMovementRotation();
		return;
	}
	// In this case we have finished the chain of commands with possibly a movement command at the end, however,
	// this command ended in a reverse movement and thus we do not want the vehicle to rotate; reset.
	ResetRotateTowardsFinalMovementRotation();
}

void ATankMaster::ExecuteStopCommand()
{
	SetTurretsToAutoEngage(false);
}

void ATankMaster::ExecuteAttackCommand(AActor* Target)
{
	M_TargetActor = Target;
	for (const auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			EachTurret->SetEngageSpecificTarget(Target);
		}
	}
	for (const auto EachHullWeapon : HullWeapons)
	{
		if (GetIsValidHullWeapon(EachHullWeapon))
		{
			EachHullWeapon->SetEngageSpecificTarget(Target);
		}
	}
	// Movement implemented in derived classes.
}

void ATankMaster::ExecuteMoveCommand(const FVector MoveToLocation)
{
	ApplyMovementBehaviours();
	SetTurretsToAutoEngage(true);
	// Movement logic specific to child classes.
}

void ATankMaster::ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand)
{
	M_RotateToDirection = RotateToRotator;
	bM_NeedToTurnTowardsTarget = true;
	bM_IsRotationAQueueCommand = IsQueueCommand;
}

void ATankMaster::TerminateRotateTowardsCommand()
{
	Super::TerminateRotateTowardsCommand();
	bM_NeedToTurnTowardsTarget = false;
	bM_IsRotationAQueueCommand = false;
}

UHarvester* ATankMaster::GetIsHarvester()
{
	if (IsValid(M_HarvesterComponent))
	{
		return M_HarvesterComponent;
	}
	return nullptr;
}

void ATankMaster::ExecuteHarvestResourceCommand(ACPPResourceMaster* TargetResource)
{
	if (not IsValid(M_HarvesterComponent) || not GetIsValidAIController())
	{
		RTSFunctionLibrary::ReportError(
			"Harvester component OR AI CONTROLLER is not valid while trying to harvest resource!"
			"\n At function: ATankMaster::ExecuteHarvestResourceCommand"
			"\n For tank: " + GetName());
		DoneExecutingCommand(EAbilityID::IdHarvestResource);
		return;
	}
	UResourceComponent* ResourceComponentOfTarget = nullptr;
	if (IsValid(TargetResource))
	{
		ResourceComponentOfTarget = TargetResource->GetResourceComponent();
	}

	// reset previous targets
	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(false);
	}
	// note that overlap evasion is disabled in the derived tracked tank class.
	AITankController->SetHarvesterMoveBlockDetectionSuppressed(true);
	M_HarvesterComponent->ExecuteHarvestResourceCommand(ResourceComponentOfTarget);
}

void ATankMaster::TerminateHarvestResourceCommand()
{
	if (not IsValid(M_HarvesterComponent) || not GetIsValidAIController())
	{
		RTSFunctionLibrary::ReportError(
			"Harvester component is not valid OR INVALID AICONTROLLER while trying to terminate harvest resource!"
			"\n At function: ATankMaster::TerminateHarvestResourceCommand"
			"\n For tank: " + GetName());

		return;
	}
	M_HarvesterComponent->TerminateHarvestCommand();

	// note that overlap evasion is re-enabled in the derived tracked tank class.
	AITankController->SetHarvesterMoveBlockDetectionSuppressed(false);
	StopBehaviourTree();
	if (IsValid(GetAIController()))
	{
		AITankController->StopMovement();
	}
}

void ATankMaster::ExecuteReturnCargoCommand()
{
	if (IsValid(M_HarvesterComponent))
	{
		M_HarvesterComponent->ExecuteReturnCargoCommand();
	}
	else
	{
		RTSFunctionLibrary::ReportError("Harvester component is not valid while trying to Return Cargo!"
			"\n At function: ATankMaster::ExecuteReturnCargoCommand"
			"\n For tank: " + GetName());
	}
}

void ATankMaster::TerminateReturnCargoCommand()
{
	Super::TerminateReturnCargoCommand();
	// nothing to do only called if return cargo failed on the harvester component which only happens if a cargo command was queued
	// then the harvester already dropped resources, cancelled the harvesting command at a point with no resources in cargo and only
	// then attempts to return cargo through this command!
}

void ATankMaster::OnResourceStorageChanged(int32 PercentageResourcesLeft, const ERTSResourceType ResourceType)
{
	if (IsValid(M_HarvesterComponent))
	{
		OnResourceStorageChangedBP(PercentageResourcesLeft, ResourceType);
	}
}

void ATankMaster::OnResourceStorageEmpty()
{
	return;
}

void ATankMaster::SetUnitToIdleSpecificLogic()
{
	for (const auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			// Try to target the last actor if it is still in range.
			EachTurret->SetAutoEngageTargets(true);
		}
	}
}

void ATankMaster::TerminateAttackCommand()
{
	for (const auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			// Try to target the last actor if it is still in range.
			EachTurret->SetAutoEngageTargets(true);
		}
	}
}

void ATankMaster::TerminateMoveCommand()
{
	Super::TerminateMoveCommand();
	RemoveMovementBehaviours();
}

void ATankMaster::ExecuteReverseCommand(const FVector ReverseToLocation)
{
	ApplyMovementBehaviours();
	SetTurretsToAutoEngage(true);
}

void ATankMaster::TerminateReverseCommand()
{
	Super::TerminateReverseCommand();
	RemoveMovementBehaviours();
}

void ATankMaster::ExecuteAttackGroundCommand(const FVector GroundLocation)
{
	if (GroundLocation.IsNearlyZero())
	{
		DoneExecutingCommand(EAbilityID::IdAttackGround);
		return;
	}

	TArray<AActor*> Attached;
	GetAttachedActors(Attached);
	for (AActor* Child : Attached)
	{
		if (ACPPTurretsMaster* Turret = Cast<ACPPTurretsMaster>(Child))
		{
			Turret->SetEngageGroundLocation(GroundLocation);
		}
	}

	if (UHullWeaponComponent* Hull = FindComponentByClass<UHullWeaponComponent>())
	{
		Hull->SetEngageGroundLocation(GroundLocation);
	}

	bM_AttackGroundActive = true;
	M_AttackGroundLocation = GroundLocation;
}


void ATankMaster::TerminateAttackGroundCommand()
{
	bM_AttackGroundActive = false;
	M_AttackGroundLocation = FVector::ZeroVector;
	SetTurretsToAutoEngage(false);
}

void ATankMaster::ExecuteAimAbilityCommand(const FVector TargetLocation, const EAimAbilityType AimAbilityType)
{
	UAimAbilityComponent* AimAbilityComponent = FAbilityHelpers::GetHasAimAbilityComponent(AimAbilityType, this);
	if (not IsValid(AimAbilityComponent))
	{
		DoneExecutingCommand(EAbilityID::IdAimAbility);
		return;
	}

	AimAbilityComponent->ExecuteAimAbility(TargetLocation);
}

void ATankMaster::TerminateAimAbilityCommand(const EAimAbilityType AimAbilityType)
{
	UAimAbilityComponent* AimAbilityComponent = FAbilityHelpers::GetHasAimAbilityComponent(AimAbilityType, this);
	if (not IsValid(AimAbilityComponent))
	{
		return;
	}

	AimAbilityComponent->TerminateAimAbility();
}

void ATankMaster::ExecuteAttachedWeaponAbilityCommand(
	const FVector TargetLocation, const EAttachWeaponAbilitySubType AttachedWeaponAbilityType)
{
	UAttachedWeaponAbilityComponent* AbilityComponent = FAbilityHelpers::GetAttachedWeaponAbilityComponent(
		AttachedWeaponAbilityType, this);
	if (not IsValid(AbilityComponent))
	{
		DoneExecutingCommand(EAbilityID::IdAttachedWeapon);
		return;
	}

	AbilityComponent->ExecuteAttachedWeaponAbility(TargetLocation);
	DoneExecutingCommand(EAbilityID::IdAttachedWeapon);
}

void ATankMaster::TerminateAttachedWeaponAbilityCommand(
	const EAttachWeaponAbilitySubType AttachedWeaponAbilityType)
{
}

void ATankMaster::ExecuteCancelAimAbilityCommand(const EAimAbilityType AimAbilityType)
{
	UAimAbilityComponent* AimAbilityComponent = FAbilityHelpers::GetHasAimAbilityComponent(AimAbilityType, this);
	if (IsValid(AimAbilityComponent))
	{
		AimAbilityComponent->ExecuteCancelAimAbility();
	}

	if (GetIsValidAIController())
	{
		AITankController->StopMovement();
	}
	DoneExecutingCommand(EAbilityID::IdCancelAimAbility);
}

void ATankMaster::TerminateCancelAimAbilityCommand(const EAimAbilityType AimAbilityType)
{
}

void ATankMaster::StopBehaviourTree()
{
	if (GetIsValidAIController())
	{
		AITankController->StopBehaviourTreeAI();
	}
}

void ATankMaster::OnFinishedStandaloneRotation()
{
	return;
}

void ATankMaster::StopRotating()
{
	bM_NeedToTurnTowardsTarget = false;
	bM_IsRotationAQueueCommand = false;
}

int ATankMaster::GetOwningPlayer()
{
	if (IsValid(RTSComponent))
	{
		return RTSComponent->GetOwningPlayer();
	}
	RTSFunctionLibrary::ReportError("RTSComponent is not valid"
		"\n At function: ATankMaster::GetOwningPlayer"
		"\n For tank: " + GetName());
	return 1;
}

void ATankMaster::OnTurretOutOfRange(
	const FVector TargetLocation,
	ACPPTurretsMaster* CallingTurret)
{
	UWorld* World = GetWorld();
	const bool bHasValidAIController = GetIsValidAIController();
	const bool bCanTurretTakeControl = GetCanTurretTakeControl();
	const bool bHasValidWorld = World != nullptr;
	if (not bHasValidAIController || not bCanTurretTakeControl || not bHasValidWorld)
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	const bool bIsAlreadyMoving = AITankController->GetMoveStatus() == EPathFollowingStatus::Moving;
	const float TimeSinceLastRequestSeconds = bM_HasTurretOutOfRangeMoveRequest
		? CurrentTimeSeconds - M_LastTurretOutOfRangeMoveRequestTimeSeconds
		: DeveloperSettings::GamePlay::Turret::MoveToTargetReissueElapsedSeconds;
	const bool bElapsedEnoughTime = TimeSinceLastRequestSeconds >=
		DeveloperSettings::GamePlay::Turret::MoveToTargetReissueElapsedSeconds;

	if (bIsAlreadyMoving && not bElapsedEnoughTime)
	{
		return;
	}

	AITankController->MoveToLocationWithGoalAcceptance(TargetLocation);
	M_LastTurretOutOfRangeMoveRequestTimeSeconds = CurrentTimeSeconds;
	bM_HasTurretOutOfRangeMoveRequest = true;

	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(false);
	}
}

void ATankMaster::OnTurretInRange(ACPPTurretsMaster* CallingTurret)
{
	bM_HasTurretOutOfRangeMoveRequest = false;

	if (not GetIsValidAIController())
	{
		return;
	}

	const bool bIsIdle = UnitCommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdIdle;
	const bool bCanStopForTurret = UnitCommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack || bIsIdle;
	if (not bCanStopForTurret)
	{
		return;
	}

	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(true);
	}
	AITankController->StopMovement();
}

void ATankMaster::OnMountedWeaponTargetDestroyed(ACPPTurretsMaster* CallingTurret,
                                                 UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor,
                                                 const bool bWasDestroyedByOwnWeapons)
{
	if (CallingTurret)
	{
		OnTurretKilledActor(CallingTurret, DestroyedActor);
	}
	else if (CallingHullWeapon)
	{
		OnHullWeaponKilledActor(CallingHullWeapon, DestroyedActor);
	}
	if (bWasDestroyedByOwnWeapons)
	{
		OnTankKilledAnyActor(DestroyedActor);
	}
}

void ATankMaster::OnFireWeapon(ACPPTurretsMaster* CallingTurret)
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	RTSComponent->SetUnitInCombat(true);
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	M_SpatialVoiceLinePlayer->PlaySpatialVoiceLine(ERTSVoiceLine::Fire, GetActorLocation(), false);
}

void ATankMaster::OnProjectileHit(const bool bBounced)
{
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	if (bBounced)
	{
		M_SpatialVoiceLinePlayer->PlaySpatialVoiceLine(ERTSVoiceLine::ShotBounced, GetActorLocation(), false);
	}
	else
	{
		M_SpatialVoiceLinePlayer->PlaySpatialVoiceLine(ERTSVoiceLine::ShotConnected, GetActorLocation(), false);
	}
}


void ATankMaster::OnTankKilledAnyActor(AActor* KilledActor)
{
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	if (GetIsSelected())
	{
		// If selected; Queue the voice line to play over radio.
		M_SpatialVoiceLinePlayer->PlayVoiceLineOverRadio(ERTSVoiceLine::EnemyDestroyed, false, true);
	}
	else
	{
		// If not selected; play the voice line spatial. Stop any other spatial audio.
		M_SpatialVoiceLinePlayer->ForcePlaySpatialVoiceLine(ERTSVoiceLine::EnemyDestroyed, GetActorLocation());
	}
}

FRotator ATankMaster::GetOwnerRotation() const
{
	if (IsValid(GetTankMesh()))
	{
		return GetTankMesh()->GetComponentRotation();
	}
	return FRotator::ZeroRotator;
}

bool ATankMaster::CheckTurretIsValid(ACPPTurretsMaster* TurretToCheck) const
{
	if (IsValid(TurretToCheck))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Turret is null!"
		"\n In function: ATankMaster::CheckTurretIsValid");
	return false;
}

bool ATankMaster::GetIsValidAIController()
{
	if (IsValid(AITankController))
	{
		return true;
	}
	return RepairAIControllerReference();
}


void ATankMaster::BeginPlay_SetupData()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	URTSComponent* RTSComp = GetRTSComponent();
	if (RTSComp->GetUnitType() == EAllUnitType::UNType_Nomadic)
	{
		const ENomadicSubtype NomadicSubtype = RTSComp->GetSubtypeAsNomadicSubtype();
		const FNomadicData MyNomadicData = URTSBlueprintFunctionLibrary::BP_GetNomadicDataOfPlayer(
			RTSComp->GetOwningPlayer(), NomadicSubtype, this);
		InitAbilityArray(MyNomadicData.Abilities);
		BeginPlay_SetupData_Resistances(MyNomadicData.Vehicle_ResistancesAndDamageMlt, MyNomadicData.MaxHealthVehicle);
	}
	else
	{
		const ETankSubtype TankSubtype = RTSComp->GetSubtypeAsTankSubtype();
		const FTankData MyTankData = URTSBlueprintFunctionLibrary::BP_GetTankDataOfPlayer(
			RTSComp->GetOwningPlayer(), TankSubtype, this);
		InitAbilityArray(MyTankData.Abilities);
		BeginPlay_SetupData_Resistances(MyTankData.ResistancesAndDamageMlt, MyTankData.MaxHealth);
	}
}

void ATankMaster::BeginPlay_SetupData_Resistances(const FResistanceAndDamageReductionData& ResistanceData,
                                                  const float MaxHealth) const
{
	if (not GetIsValidHealthComponent())
	{
		return;
	}
	HealthComponent->InitHealthAndResistance(ResistanceData, MaxHealth);
}


bool ATankMaster::GetIsValidSpatialVoiceLinePlayer() const
{
	if (IsValid(M_SpatialVoiceLinePlayer))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Invalid spatial voice line player on unit: " + GetName() +
		"\n ATankMaster::GetIsValidSpatialVoiceLinePlayer");
	return false;
}

EAnnouncerVoiceLineType ATankMaster::OverrideAnnouncerDeathVoiceLine(const EAnnouncerVoiceLineType OriginalLine) const
{
	// Simply use the original selected by the helper.
	return OriginalLine;
}

void ATankMaster::OnUnitDies_CheckForCargo(const ERTSDeathType DeathType) const
{
	UCargo* CargoComp = FindComponentByClass<UCargo>();
	if (not IsValid(CargoComp))
	{
		return;
	}
	CargoComp->OnOwnerDies(DeathType);
}

void ATankMaster::OnUnitDies_AnnouncerDeathVoiceLine() const
{
	if (not IsValid(PlayerController) || not IsValid(RTSComponent))
	{
		return;
	}
	if (RTSComponent->GetOwningPlayer() != 1)
	{
		return;
	}
	const ETankSubtype TankSubtype = RTSComponent->GetSubtypeAsTankSubtype();
	EAnnouncerVoiceLineType DeathVoiceLine = FRTS_VoiceLineHelpers::GetDeathVoiceLineForTank(TankSubtype);
	DeathVoiceLine = OverrideAnnouncerDeathVoiceLine(DeathVoiceLine);
	PlayerController->PlayAnnouncerVoiceLine(DeathVoiceLine, true, false);
}

bool ATankMaster::RepairAIControllerReference()
{
	AController* MyController = GetController();
	if (IsValid(MyController))
	{
		if (AAITankMaster* AIController = Cast<AAITankMaster>(MyController))
		{
			SetAIController(AIController);
		}
		else
		{
			RTSFunctionLibrary::ReportFailedCastError("Controller", "AAITankMaster",
			                                          "ATankMaster::RepairAIControllerReference");
			return false;
		}
	}
	else
	{
		SpawnDefaultController();
		AController* SpawnedController = GetController();
		if (AAITankMaster* AIController = Cast<AAITankMaster>(SpawnedController))
		{
			SetAIController(AIController);
		}
		else
		{
			RTSFunctionLibrary::ReportFailedCastError("SpawnedController", "AITankMaster",
			                                          "ATankMaster::RepairAIControllerReference");
			return false;
		}
	}
	return true;
}

bool ATankMaster::GetCanTurretTakeControl() const
{
	const bool bIsIdle = UnitCommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdIdle;
	return UnitCommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack || bIsIdle;
}

void ATankMaster::SetMaxSpeedOnVehicleMovementComponent()
{
	if (URTSComponent* RTSComp = GetRTSComponent(); IsValid(RTSComp) && GetIsValidAIController())
	{
		if (RTSComp->GetUnitType() == EAllUnitType::UNType_Tank)
		{
			UPathFollowingComponent* PathFollowingComponent = AITankController->GetPathFollowingComponent();
			if (IsValid(PathFollowingComponent) && PathFollowingComponent->IsA(
				UTrackPathFollowingComponent::StaticClass()))
			{
				UTrackPathFollowingComponent* VehiclePathFollowingComponent = Cast<UTrackPathFollowingComponent>(
					PathFollowingComponent);
				if (IsValid(VehiclePathFollowingComponent))
				{
					ETankSubtype TankSubtype = RTSComp->GetSubtypeAsTankSubtype();
					const FTankData MyTankData = URTSBlueprintFunctionLibrary::BP_GetTankDataOfPlayer(
						RTSComp->GetOwningPlayer(), RTSComp->GetSubtypeAsTankSubtype(), this);
					VehiclePathFollowingComponent->SetDesiredSpeed(MyTankData.VehicleMaxSpeedKmh,
					                                               ESpeedUnits::KilometersPerHour);
					VehiclePathFollowingComponent->SetDesiredReverseSpeed(MyTankData.VehicleReverseSpeedKmh,
					                                                      ESpeedUnits::KilometersPerHour);
					return;
				}
				RTSFunctionLibrary::ReportFailedCastError("PathFollowingComponent",
				                                          "UVehiclePathFollowingComponent",
				                                          "ATankMaster::SetMaxSpeedOnVehicleMovementComponent");
				return;
			}
			RTSFunctionLibrary::ReportError("pathfollowing component is not of the right class!"
				"\n At function: ATankMaster::SetMaxSpeedOnVehicleMovementComponent"
				"\n For tank: " + GetName());
			return;
		}
		FString UnitTypeString = Global_GetUnitTypeString(RTSComp->GetUnitType());
		RTSFunctionLibrary::PrintString(
			"No adjustment to max vehicle speed as of non tank nor  unit type: " +
			UnitTypeString, FColor::Red);
		return;
	}
	RTSFunctionLibrary::ReportError("RTSComponent is not valid"
		"\n At function: ATankMaster::SetMaxSpeedOnVehicleMovementComponent"
		"\n For tank: " + GetName());
}

void ATankMaster::OnTurretKilledActor(ACPPTurretsMaster* CallingTurret, AActor* DestroyedActor)
{
	if (not IsValid(CallingTurret))
	{
		RTSFunctionLibrary::ReportError("Calling turret is not valid"
			"\n At function: ATankMaster::OnTurretTargetDestroyed"
			"\n For tank: " + GetName());
		return;
	}

	if (UnitCommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack)
	{
		if (M_TargetActor == DestroyedActor)
		{
			// The target was destroyed, so we can stop the attack command.
			DoneExecutingCommand(EAbilityID::IdAttack);
			return;
		}
		if (RTSFunctionLibrary::RTSIsValid(M_TargetActor))
		{
			// The turret killed an actor that was not the main target.
			CallingTurret->SetEngageSpecificTarget(M_TargetActor);
		}
		else
		{
			// The target was destroyed, so we can stop the attack command; will auto engage all turrets.
			DoneExecutingCommand(EAbilityID::IdAttack);
		}
	}
	else
	{
		// No attack command active, so turret can auto engage.
		CallingTurret->SetAutoEngageTargets(true);
	}
}

void ATankMaster::OnHullWeaponKilledActor(UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor)
{
	if (not GetIsValidHullWeapon(CallingHullWeapon))
	{
		return;
	}
	if (UnitCommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack)
	{
		if (M_TargetActor == DestroyedActor)
		{
			// The target was destroyed, so we can stop the attack command.
			DoneExecutingCommand(EAbilityID::IdAttack);
			return;
		}
		if (RTSFunctionLibrary::RTSIsValid(M_TargetActor))
		{
			// The turret killed an actor that was not the main target.
			CallingHullWeapon->SetEngageSpecificTarget(M_TargetActor);
		}
		else
		{
			// The target was destroyed, so we can stop the attack command; will auto engage all turrets.
			DoneExecutingCommand(EAbilityID::IdAttack);
		}
	}
	else
	{
		// No attack command active, so turret can auto engage.
		CallingHullWeapon->SetAutoEngageTargets(true);
	}
}

void ATankMaster::OnUnitDies_DisableWeapons()
{
	for (auto EachTurret : Turrets)
	{
		if (IsValid(EachTurret))
		{
			EachTurret->DisableTurret();
		}
	}
	for (auto EachHullWeapon : HullWeapons)
	{
		if (IsValid(EachHullWeapon))
		{
			EachHullWeapon->DisableHullWeapon();
		}
	}
}


bool ATankMaster::GetIsValidRTSNavCollision() const
{
	if (IsValid(RTSNavCollision))
	{
		return true;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(
		this,
		"RTSNavCollision",
		"ATankMaster::GetIsValidRTSNavCollision");
	return false;
}

void ATankMaster::BeginPlay_SetupCollisionVsBuildings()
{
	if (GetIsValidRTSComponent())
	{
		const ECollisionResponse Response = RTSComponent->GetOwningPlayer() == 1 ? ECR_Ignore : ECR_Overlap;
		FRTS_CollisionSetup::ForceBuildingPlacementResponseOnActor(this, Response, /*bRecurseAttachedActors=*/true);
	}
}

bool ATankMaster::GetIsValidHullWeapon(const UHullWeaponComponent* HullWeapon) const
{
	if (IsValid(HullWeapon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Hull weapon is not valid"
		"\n At function: ATankMaster::GetIsValidHullWeapon"
		"\n For tank: " + GetName());
	return false;
}
