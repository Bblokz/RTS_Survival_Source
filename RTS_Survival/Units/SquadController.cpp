// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadController.h"


#include "BrainComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/CaptureMechanic/CaptureMechanicHelpers.h"
#include "RTS_Survival/CaptureMechanic/CaptureInterface/CaptureInterface.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/PickupItems/Items/WeaponPickUp/WeaponPickup.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceComponent.h"
#include "RTS_Survival/RTSComponents/RepairComponent/RepairComponent.h"
#include "RTS_Survival/RTSComponents/RepairComponent/RepairHelpers/RepairHelpers.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Scavenging/ScavengerComponent/ScavengerComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Squads/Reinforcement/SquadReinforcementComponent.h"
#include "Squads/SquadControllerHpComp/USquadHealthComponent.h"
#include "Squads/SquadControllerHpComp/SquadWeaponIcons/SquadWeaponIcons.h"
#include "Squads/SquadControllerHpComp/SquadWeaponIcons/SquadWeaponIconSettings/SquadWeaponIconSettings.h"
#include "Squads/SquadUnit/SquadUnit.h"
#include "Squads/SquadUnit/AISquadUnit/AISquadUnit.h"
#include "Squads/SquadUnitHpComp/SquadUnitHealthComponent.h"
#include "Weapons/WeaponData/WeaponData.h"

FSquadStartGameAction::FSquadStartGameAction()
{
}

void FSquadStartGameAction::InitStartGameAction(const EAbilityID InAbilityID, AActor* InTargetActor,
                                                const FVector& InTargetLocation, ASquadController* InMySquad)
{
	if (not IsValid(InMySquad))
	{
		return;
	}
	StartGameAction = InAbilityID;
	TargetLocation = InTargetLocation;
	TargetActor = InTargetActor;
	M_MySquad = InMySquad;
	if (UWorld* World = M_MySquad->GetWorld())
	{
		FTimerDelegate TimerDel;
		TimerDel.BindRaw(this, &FSquadStartGameAction::TimerIteration);
		World->GetTimerManager().SetTimer(ActionTimer, TimerDel, 1.33f, true);
	}
}

void FSquadStartGameAction::OnUnitDies(UWorld* World)
{
	if (not World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(ActionTimer);
}

void FSquadStartGameAction::TimerIteration()
{
	if (not GetIsValidSquadController() || not GetIsSquadFullyLoaded())
	{
		return;
	}
	if (not ExecuteStartAbility())
	{
		return;
	}

	if (UWorld* World = M_MySquad->GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActionTimer);
	}
}

bool FSquadStartGameAction::GetIsValidSquadController() const
{
	if (not M_MySquad.IsValid())
	{
		RTSFunctionLibrary::ReportError("SquadController is not valid!"
			"\n At function: FSquadStartGameAction::GetIsValidSquadController");
		return false;
	}
	return true;
}

bool FSquadStartGameAction::GetIsSquadFullyLoaded() const
{
	if (not GetIsValidSquadController())
	{
		return false;
	}
	return M_MySquad->GetIsSquadFullyLoaded();
}

bool FSquadStartGameAction::ExecuteStartAbility() const
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
		return M_MySquad->AttackActor(TargetActor, true) == ECommandQueueError::NoError;
	case EAbilityID::IdAttackGround:
		return M_MySquad->AttackGround(TargetLocation, true) == ECommandQueueError::NoError;
	case EAbilityID::IdMove:
		return M_MySquad->MoveToLocation(TargetLocation, true, FRotator::ZeroRotator, false) ==
			ECommandQueueError::NoError;
	case EAbilityID::IdPatrol:
		return M_MySquad->PatrolToLocation(TargetLocation, true) == ECommandQueueError::NoError;
	case EAbilityID::IdStop:
		break;
	case EAbilityID::IdSwitchWeapon:
		return M_MySquad->SwitchWeapons(true) == ECommandQueueError::NoError;
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
		return M_MySquad->MoveToLocation(TargetLocation, true, FRotator::ZeroRotator, false) ==
			ECommandQueueError::NoError;
	case EAbilityID::IdRotateTowards:
		return M_MySquad->RotateTowards(
			UKismetMathLibrary::FindLookAtRotation(M_MySquad->GetActorLocation(), TargetLocation),
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
		return M_MySquad->ScavengeObject(TargetActor, true) == ECommandQueueError::NoError;
	case EAbilityID::IdDigIn:
		break;
	case EAbilityID::IdBreakCover:
		break;
	case EAbilityID::IdFireRockets:
		break;
	case EAbilityID::IdCancelRocketFire:
		break;
	case EAbilityID::IdRocketsReloading:
		break;
	case EAbilityID::IdRepair:
		return M_MySquad->RepairActor(TargetActor, true) == ECommandQueueError::NoError;
	case EAbilityID::IdReturnToBase:
		break;
	case EAbilityID::IdAircraftOwnerNotExpanded:
		break;
        case EAbilityID::IdEnterCargo:
                return M_MySquad->EnterCargo(TargetActor, true) == ECommandQueueError::NoError;
        case EAbilityID::IdExitCargo:
                break;
        case EAbilityID::IdEnableResourceConversion:
                break;
        case EAbilityID::IdDisableResourceConversion:
                break;
        case EAbilityID::IdCapture:
                return M_MySquad->CaptureActor(TargetActor, true) == ECommandQueueError::NoError;
        case EAbilityID::IdReinforceSquad:
                break;
        }
        return true;
}

FTargetPickupItemState::FTargetPickupItemState()
	: M_TargetPickupItem(nullptr)
	  , bIsBusyPickingUp(false)
{
}

FTargetScavengeState::FTargetScavengeState()
	: M_TargetScavengeObject(nullptr)
	  , bIsBusyScavenging(false)
{
}

FRepairState::FRepairState()
	: M_TargetRepairObject(nullptr)
{
}

FCaptureState::FCaptureState()
	: M_TargetCaptureActor(nullptr)
	  , bIsCaptureInProgress(false)
{
}

ASquadController::ASquadController(): PlayerController(nullptr), RTSComponent(nullptr)
{
        using DeveloperSettings::GamePlay::Navigation::SqPathFinding_OffsetDistance;
        using DeveloperSettings::GamePlay::Navigation::SqPathFinding_FinalDestSpread;
        M_SqPath_Offsets = {
		FVector(SqPathFinding_OffsetDistance, 0, 0), FVector(-SqPathFinding_OffsetDistance, 0, 0),
		FVector(0, SqPathFinding_OffsetDistance, 0), FVector(0, -SqPathFinding_OffsetDistance, 0),
		FVector(SqPathFinding_OffsetDistance, SqPathFinding_OffsetDistance, 0),
		FVector(-SqPathFinding_OffsetDistance, -SqPathFinding_OffsetDistance, 0),
	};
        ExperienceComponent = CreateDefaultSubobject<URTSExperienceComp>(TEXT("ExperienceComponent"));

        // NEW: create the cargo squad component so all cargo logic lives outside the controller.
        CargoSquad = CreateDefaultSubobject<UCargoSquad>(TEXT("CargoSquad"));

        SquadReinforcement = CreateDefaultSubobject<USquadReinforcementComponent>(TEXT("SquadReinforcement"));

        M_SquadWeaponSwitch.Init(this);
}

void ASquadController::RequestSquadMoveForAbility(const FVector& MoveToLocation, const EAbilityID AbilityID)
{
	GeneralMoveToForAbility(MoveToLocation, AbilityID);
}

bool ASquadController::GetIsSquadFullyLoaded() const
{
	return M_SquadLoadingStatus.bM_HasFinishedLoading;
}

void ASquadController::PlaySquadUnitLostVoiceLine()
{
	if (not GetIsValidPlayerController())
	{
		return;
	}
	PlayerController->PlayVoiceLine(this, ERTSVoiceLine::SquadUnitLost, true, false);
}

void ASquadController::SetSquadSelected(const bool bIsSelected)
{
	EnsureSquadUnitsValid();
	for (const auto EachSquadUnit : M_TSquadUnits)
	{
		USelectionComponent* SelectionComponent = EachSquadUnit->GetSelectionComponent();
		if (not IsValid(SelectionComponent))
		{
			const FString UnitName = EachSquadUnit ? EachSquadUnit->GetName() : "nullptr";
			RTSFunctionLibrary::ReportError("SquadUnit : " + UnitName + " does not have a valid SelectionComponent!"
				"\n At function: ASquadController::SetSquadSelected"
				"\n Squad: " + GetName() + "\n"
				" This will not be selected or deselected, skipping.");
			continue;
		}
		if (bIsSelected)
		{
			SelectionComponent->SetUnitSelected();
		}
		else
		{
			SelectionComponent->SetUnitDeselected();
		}
	}
}

void ASquadController::OnSquadUnitInCombat()
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	RTSComponent->SetUnitInCombat(true);
}

int32 ASquadController::GetSquadUnitIndex(const ASquadUnit* SquadUnit)
{
	EnsureSquadUnitsValid();
	return M_TSquadUnits.IndexOfByKey(SquadUnit);
}

bool ASquadController::GetIsValidSquadHealthComponent() const
{
	if (not IsValid(SquadHealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "SquadHealthComponent",
		                                             "ASquadController::GetIsValidSquadHealthComponent");
		return false;
	}
	return true;
}

TArray<ASquadUnit*> ASquadController::GetSquadUnitsChecked()
{
	// Create a copy of the current units array
	TArray<ASquadUnit*> CurrentSquadUnits = M_TSquadUnits;

	TArray<ASquadUnit*> ValidSquadUnits;
	for (ASquadUnit* SquadUnit : CurrentSquadUnits)
	{
		if (IsValid(SquadUnit))
		{
			ValidSquadUnits.Add(SquadUnit);
			continue;
		}
		RTSFunctionLibrary::ReportError("Squad unit in array of SquadController is not valid!"
			"\n At function: ASquadController::GetSquadUnitsChecked"
			"\n Squad: " + GetName());
	}
	M_TSquadUnits = ValidSquadUnits;
	return M_TSquadUnits;
}

bool ASquadController::GetIsScavenger()
{
	for (const ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit) && SquadUnit->GetHasValidScavengerComp())
		{
			return true;
		}
	}
	return false;
}

bool ASquadController::GetIsRepairUnit()
{
        for (const ASquadUnit* SquadUnit : M_TSquadUnits)
        {
                if (GetIsValidSquadUnit(SquadUnit) && SquadUnit->GetHasValidRepairComp())
		{
			return true;
		}
	}
	return false;
}

float ASquadController::GetUnitRepairRadius()
{
        RTSFunctionLibrary::ReportError("Calling Get Unit repair radius on a squad controller!");
        return 100.f;
}


void ASquadController::HandleWeaponSwitchOnUnitDeath(ASquadUnit* UnitThatDied)
{
        M_SquadWeaponSwitch.HandleWeaponSwitchOnUnitDeath(UnitThatDied);
}


void ASquadController::UnitInSquadDied(ASquadUnit* UnitDied, const bool bUnitSelected)
{
        M_TSquadUnits.Remove(UnitDied);

	// Adjust the completed command count if necessary.
	if (M_UnitsCompletedCommand > M_TSquadUnits.Num())
	{
		M_UnitsCompletedCommand = M_TSquadUnits.Num();
	}

	const bool bHadCargoSquadInside = IsValid(CargoSquad) && CargoSquad->GetIsInsideCargo();
	if (bHadCargoSquadInside)
	{
		CargoSquad->OnUnitDiedWhileInside(UnitDied);
	}

	// If there are still units left, no squad-wide cleanup is needed.
	if (M_TSquadUnits.Num() > 0)
	{
		return;
	}

	// At this point the squad is empty; if we were inside cargo, free that state.
	if (bHadCargoSquadInside)
	{
		CargoSquad->OnSquadCompletelyDiedInside();
	}
	if (IsValid(PlayerController))
	{
		if (bUnitSelected)
		{
			PlayerController->RemoveSquadFromSelectionAndUpdateUI(this);
		}
		if (IsValid(RTSComponent))
		{
			if (RTSComponent->GetOwningPlayer() == 1)
			{
				PlayerController->PlayAnnouncerVoiceLine(EAnnouncerVoiceLineType::LostSquad, true, false);
			}
		}
	}

	// Re-enable the scavengeable object so other squads can scavenge it.
	// This is not valid if the squad completely scavenged the object; expected, no error report.
	if (IsValid(M_TargetScavengeState.M_TargetScavengeObject))
	{
		M_TargetScavengeState.M_TargetScavengeObject->PauseScavenging(this);
	}

	Destroy();
}

void ASquadController::OnSquadUnitCommandComplete(const EAbilityID CompletedAbilityID)
{
	M_UnitsCompletedCommand++;
	if (CompletedAbilityID == EAbilityID::IdPickupItem)
	{
		// Check if the overlap failed, if so we need to force a pickup.
		CheckCloseEnoughToItemPickupAnyway();
	}

	// Special hand-off: when the EnterCargo move finishes for all units,
	// we don't finish the command yet; let CargoSquad finalize (seat assignment & attach).
	if (CompletedAbilityID == EAbilityID::IdEnterCargo && IsValid(CargoSquad))
	{
		if (M_UnitsCompletedCommand >= M_TSquadUnits.Num())
		{
			CargoSquad->OnEnterCargo_MoveToEntranceCompleted();
			M_UnitsCompletedCommand = 0;
		}
		return;
	}

	// If all units have completed the command.
	if (M_UnitsCompletedCommand >= M_TSquadUnits.Num())
	{
		// All units have completed, call DoneExecutingCommand.
		DoneExecutingCommand(CompletedAbilityID);
		M_UnitsCompletedCommand = 0;
	}
}

bool ASquadController::CanSquadPickupWeapon()
{
	for (const auto EachSquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(EachSquadUnit))
		{
			continue;
		}
		bool bSquadUnitHasValidSecondaryWpComponent = false;
		if (!EachSquadUnit->GetHasSecondaryWeapon(bSquadUnitHasValidSecondaryWpComponent))
		{
			if (bSquadUnitHasValidSecondaryWpComponent)
			{
				return true;
			}
		}
	}
	return false;
}

bool ASquadController::HasSquadSecondaryWeapons()
{
	for (const auto EachSquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(EachSquadUnit))
		{
			bool bValid = false;
			if (EachSquadUnit->GetHasSecondaryWeapon(bValid))
			{
				return true;
			}
		}
	}
	return false;
}

FVector ASquadController::GetSquadLocation()
{
	EnsureSquadUnitsValid();
	const int32 N = M_TSquadUnits.Num();
	if (N <= 0)
	{
		return GetActorLocation();
	}
	FVector Sum = FVector::ZeroVector;
	for (ASquadUnit* U : M_TSquadUnits)
	{
		if (IsValid(U)) { Sum += U->GetActorLocation(); }
	}
	return Sum / static_cast<float>(N);
}

void ASquadController::OnRTSUnitSpawned(const bool bSetDisabled, const float TimeNotSelectable, const FVector MoveTo)
{
	if (bSetDisabled)
	{
		for (auto eachSqUnit : M_TSquadUnits)
		{
			if (GetIsValidSquadUnit(eachSqUnit))
			{
				eachSqUnit->OnSquadSpawned(bSetDisabled, TimeNotSelectable, GetActorLocation());
			}
		}
		return;
	}
	// center grid generation around squad controller location.
	StartGeneratingSpawnLocations(GetActorLocation());
	for (auto eachSqUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(eachSqUnit))
		{
			// Uses the grid points and attempts to project them to obtain valid points.
			FVector SpawnLocation = FindIdealSpawnLocation() + (0, 0, 150);

			eachSqUnit->OnSquadSpawned(bSetDisabled, TimeNotSelectable, SpawnLocation);
		}
	}

	if (UWorld* World = GetWorld())
	{
		FTimerDelegate Del;
		Del.BindWeakLambda(this, [this, MoveTo]()
		{
			if (!IsValid(this)) return;
			SetIsSpawning(true);
			FVector MoveToProjected = ProjectLocationOnNavMesh(MoveTo, 200.0f, true);
			FRotator FinalRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), MoveToProjected);
			MoveToLocation(MoveTo, true, FinalRotation);
		});
		World->GetTimerManager().SetTimer(M_SpawningTimer, Del, 0.5, false);
	}
}

void ASquadController::OnSquadUnitOutOfRange(const FVector& TargetLocation)
{
	// If one unit is out of range, ensure we're allowed to move (exit cargo if needed) and move everyone closer.
	if (IsValid(CargoSquad))
	{
		CargoSquad->CheckCargoState(EAbilityID::IdNoAbility_MoveCloserToTarget);
	}

	FVector ProjectedLocation = ProjectLocationOnNavMesh(TargetLocation, 500, true);
	GeneralMoveToForAbility(ProjectedLocation, EAbilityID::IdNoAbility_MoveCloserToTarget);
	if (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("SQUAD out of range; move closer", FColor::Red);
	}
}


void ASquadController::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupPlayerController();

	// Init cargo subsystem early.
	if (IsValid(CargoSquad))
	{
		CargoSquad->Init(this);
	}
	// Start loading the squad units asynchronously
	LoadSquadUnitsAsync();
	if (GetWorld())
	{
		// Start the timer to update the controller's position every 0.33 seconds
		GetWorld()->GetTimerManager().SetTimer(
			M_UpdatePositionTimerHandle,
			this,
			&ASquadController::UpdateControllerPositionToAverage,
			3.f,
			true
		);
	}
}

void ASquadController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	M_SquadStartGameAction.OnUnitDies(GetWorld());
}

void ASquadController::BeginDestroy()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SpawningTimer);
		World->GetTimerManager().ClearTimer(M_UpdatePositionTimerHandle);
	}
	// Destroy the squad units
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (IsValid(SquadUnit))
		{
			SquadUnit->Destroy();
		}
	}
	Super::BeginDestroy();
}

bool ASquadController::GetCanCaptureUnits()
{
	return true;
}

void ASquadController::SetSquadStartGameAction(AActor* TargetActor, const FVector TargetLocation,
                                               const EAbilityID StartGameAbility)
{
	M_SquadStartGameAction.InitStartGameAction(StartGameAbility, TargetActor, TargetLocation, this);
}

bool ASquadController::GetIsValidRTSComponent() const
{
	if (IsValid(RTSComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this, "RTSComponent", "ASquadController::GetIsValidRTSComponent");
	return false;
}

bool ASquadController::EnsureValidExperienceComponent()
{
	if (IsValid(ExperienceComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Experience component is not valid"
		"\n At function: ASquadController::EnsureValidExperienceComponent"
		"\n For tank: " + GetName());
	ExperienceComponent = NewObject<URTSExperienceComp>(this, "ExperienceComponent");
	return IsValid(ExperienceComponent);
}

void ASquadController::InitSquadData_SetupSquadHealthAggregation()
{
	if (not IsValid(SquadHealthComponent))
	{
		return;
	}

	SquadHealthComponent->ResetUnitSnapshots();

	float SumMax = 0.f;
	float SumCur = 0.f;

	EnsureSquadUnitsValid();
	for (ASquadUnit* EachSqUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(EachSqUnit))
		{
			continue;
		}
		if (not IsValid(EachSqUnit->HealthComponent))
		{
			continue;
		}

		USquadUnitHealthComponent* UnitHp = Cast<USquadUnitHealthComponent>(EachSqUnit->HealthComponent);
		if (not IsValid(UnitHp))
		{
			continue;
		}

		UnitHp->SetSquadHealthComponent(SquadHealthComponent);

		const float UnitMax = UnitHp->GetMaxHealth();
		const float UnitCur = UnitHp->GetCurrentHealth();

		SquadHealthComponent->SeedUnitSnapshot(UnitHp, UnitMax, UnitCur);

		SumMax += UnitMax;
		SumCur += UnitCur;
	}

	// Full-squad max is fixed from here on (until you explicitly re-init).
	SquadHealthComponent->InitializeFullSquadMax(SumMax);
	SquadHealthComponent->SetCurrentHealth(SumCur);
}


URTSExperienceComp* ASquadController::GetExperienceComponent() const
{
	return ExperienceComponent;
}

void ASquadController::OnUnitLevelUp()
{
	// todo squad level up.
}

void ASquadController::OnSquadSubtypeSet(
)
{
	if (M_SquadLoadingStatus.bM_HasFinishedLoading && not M_SquadLoadingStatus.bM_HasInitializedData)
	{
		InitSquadData();
	}
}

void ASquadController::SetSquadVisionRange(const float NewVision)
{
	if (not IsValid(RTSComponent))
	{
		return;
	}
	M_SquadVisionRange = NewVision;
	EnsureSquadUnitsValid();
	for (auto EachSquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(EachSquadUnit))
		{
			EachSquadUnit->SetOwningPlayerAndStartFow(RTSComponent->GetOwningPlayer(), NewVision);
		}
	}
}

void ASquadController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	SquadHealthComponent = FindComponentByClass<USquadHealthComponent>();
	if (!IsValid(SquadHealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             " squad unit SquadHealthComponent",
		                                             "ASquadController::PostInitializeComponents");
	}

	RTSComponent = FindComponentByClass<URTSComponent>();
	if (!IsValid(RTSComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "RTSComponent",
		                                             "ASquadController::PostInitializeComponents");
	}
	// Pass on a reference to self so the CommandData can access the interface.
	M_UnitCommandData = NewObject<UCommandData>(this);
	M_UnitCommandData->InitCommandData(this);
	UObject* Outer = M_UnitCommandData->GetOuter();
	if (Outer != this)
	{
		RTSFunctionLibrary::ReportError("The outer on this CommandData is not set to this object."
			"\n ASquadController::PostInitializeComponents");
	}
}

void ASquadController::SetUnitToIdleSpecificLogic()
{
}

UCommandData* ASquadController::GetIsValidCommandData()
{
	if (M_UnitCommandData)
	{
		return M_UnitCommandData;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "UnitCommandData",
	                                                      "ASquadController::GetIsValidCommandData", this);
	return nullptr;
}


void ASquadController::OnUnitIdleAndNoNewCommands()
{
	// Important; calls the delegate in base ICommands.
	ICommands::OnUnitIdleAndNoNewCommands();
}

void ASquadController::ExecuteMoveCommand(const FVector MoveToLocation)
{
	if (IsValid(CargoSquad))
	{
		CargoSquad->CheckCargoState(EAbilityID::IdMove);
	}
	M_UnitsCompletedCommand = 0;
	GeneralMoveToForAbility(MoveToLocation, EAbilityID::IdMove);
}

ESquadPathFindingError ASquadController::GeneratePathsForSquadUnits(const FVector& MoveToLocation)
{
	UWorld* World = nullptr;
	UNavigationSystemV1* NavSystem = nullptr;
	AAIController* AIController = nullptr;

	// Step 1: Setup pointers and perform checks
	ESquadPathFindingError Error = GeneratePaths_SetupNav(World, NavSystem, AIController);
	if (Error != ESquadPathFindingError::NoError)
	{
		return Error;
	}

	// Step 2: Create move request and pathfinding query
	FPathFindingQuery PFQuery;
	Error = GeneratePaths_GenerateQuery(MoveToLocation, NavSystem, AIController, PFQuery);
	if (Error != ESquadPathFindingError::NoError)
	{
		return Error;
	}
	FNavPathSharedPtr SquadPath;
	// Step 3: Find path and perform error checking
	Error = GeneratePaths_ExeQuery(NavSystem, PFQuery, SquadPath);
	if (Error != ESquadPathFindingError::NoError)
	{
		return Error;
	}
	if (not SquadPath.IsValid())
	{
		return ESquadPathFindingError::PathResultIsInvalid;
	}

	// Step 4: Assign adjusted paths to each squad unit
	return GeneratePaths_Assign(MoveToLocation, SquadPath);
}

ESquadPathFindingError ASquadController::GeneratePaths_SetupNav(UWorld*& OutWorld,
                                                                UNavigationSystemV1*& OutNavSystem,
                                                                AAIController*& OutAIController)
{
	OutWorld = GetWorld();
	if (!OutWorld)
	{
		return ESquadPathFindingError::InvalidWorld;
	}
	OutNavSystem = UNavigationSystemV1::GetCurrent(OutWorld);
	if (!OutNavSystem)
	{
		return ESquadPathFindingError::InvalidNavSystem;
	}

	if (M_TSquadUnits.Num() == 0 || !IsValid(M_TSquadUnits[0]))
	{
		return ESquadPathFindingError::NoUnitsLeftInSquad;
	}

	OutAIController = M_TSquadUnits[0]->GetAISquadUnitChecked();
	if (!OutAIController)
	{
		return ESquadPathFindingError::InvalidAIController;
	}
	return ESquadPathFindingError::NoError;
}

ESquadPathFindingError ASquadController::GeneratePaths_GenerateQuery(
	const FVector& MoveToLocation,
	UNavigationSystemV1* NavSystem,
	const AAIController* AIController,
	FPathFindingQuery& OutPFQuery)
{
	// Project location to the navmesh.
	FNavLocation NavLocation;
	if (not NavSystem->ProjectPointToNavigation(M_TSquadUnits[0]->GetActorLocation(), NavLocation,
	                                            FVector(100.0f, 100.0f, 500.0f)))
	{
		return ESquadPathFindingError::CannotProjectStartLocation;
	}

	FAIMoveRequest MoveRequest(MoveToLocation);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	// Prevent instant fail if outside nave mesh; use partial path movement.
	MoveRequest.SetRequireNavigableEndLocation(false);
	// Let partial path handle end position.
	MoveRequest.SetProjectGoalLocation(false);
	MoveRequest.SetNavigationFilter(AIController->GetDefaultNavigationFilterClass());
	MoveRequest.SetAcceptanceRadius(DeveloperSettings::GamePlay::Navigation::SquadUnitAcceptanceRadius);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetCanStrafe(false);

	// Build the pathfinding query
	if (not AIController->BuildPathfindingQuery(MoveRequest, OutPFQuery))
	{
		return ESquadPathFindingError::CannotBuildValidPathFindingQuery;
	}
	return ESquadPathFindingError::NoError;
}

ESquadPathFindingError ASquadController::GeneratePaths_ExeQuery(UNavigationSystemV1* NavSystem,
                                                                const FPathFindingQuery& PFQuery,
                                                                FNavPathSharedPtr& OutNavPath) const
{
	const FPathFindingResult PathResult = NavSystem->FindPathSync(PFQuery);

	const ESquadPathFindingError Error = GetPathFindingErrorFromQuery(PathResult);
	if (Error != ESquadPathFindingError::NoError)
	{
		return Error;
	}
	PathResult.Path->EnableRecalculationOnInvalidation(true);
	OutNavPath = PathResult.Path;
	return ESquadPathFindingError::NoError;
}

ESquadPathFindingError ASquadController::GeneratePaths_Assign(const FVector& MoveToLocation,
                                                              const FNavPathSharedPtr& SquadPath)
{
	int32 Counter = 0;
	const int32 N = M_SqPath_Offsets.Num();

	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (!GetIsValidSquadUnit(SquadUnit)) { continue; }

		FNavPathSharedPtr UnitPath = MakeShared<FNavigationPath>(*SquadPath);

		if (Counter > 0)
		{
			const FVector UnitOffset = (N > 0) ? M_SqPath_Offsets[Counter % N] : FVector::ZeroVector;
			for (int32 i = 0; i < UnitPath->GetPathPoints().Num(); ++i)
			{
				const bool bLast = (i == UnitPath->GetPathPoints().Num() - 1);
				const FVector PointOffset = bLast
					                            ? GetFinalPathPointOffset(UnitOffset)
					                            : (UnitOffset * FMath::FRandRange(0.8f, 1.2f));
				FNavPathPoint& P = UnitPath->GetPathPoints()[i];
				P.Location += PointOffset;
				P.Location = ProjectLocationOnNavMesh(P.Location, 200.f, false);
			}
		}

		M_SquadUnitPaths.Add(SquadUnit, UnitPath);
		++Counter;
	}
	return ESquadPathFindingError::NoError;
}

FVector ASquadController::GetFinalPathPointOffset(const FVector& UnitOffset) const
{
	using DeveloperSettings::GamePlay::Navigation::SqPathFinding_OffsetDistance;
	using DeveloperSettings::GamePlay::Navigation::SqPathFinding_FinalDestSpread;
	return UnitOffset * SqPathFinding_FinalDestSpread / SqPathFinding_OffsetDistance;
}

void ASquadController::GeneralMoveToForAbility(const FVector& MoveToLocation, const EAbilityID AbilityID)
{
	// Generate paths for each squad unit
	const ESquadPathFindingError Error = GeneratePathsForSquadUnits(MoveToLocation);
	if (Error != ESquadPathFindingError::NoError)
	{
		RTSFunctionLibrary::ReportError(GetSquadPathFindingErrorMsg(Error));
	}

	// Assign paths to squad units
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}
		const FNavPathSharedPtr* UnitPathPtr = M_SquadUnitPaths.Find(SquadUnit);
		if (UnitPathPtr && UnitPathPtr->IsValid())
		{
			SquadUnit->ExecuteMoveAlongPath(*UnitPathPtr, AbilityID);
		}
		else
		{
			// Fall back to moving directly to the location if no path found
			SquadUnit->ExecuteMoveToSelfPathFinding(MoveToLocation, AbilityID);
		}
	}

	// Clear the paths after assignment
	M_SquadUnitPaths.Empty();
}

void ASquadController::TerminateMoveCommand()
{
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			SquadUnit->TerminateMovementCommand();
		}
	}
}

void ASquadController::ExecuteCaptureCommand(AActor* CaptureTarget)
{
	M_CaptureState.Reset();

	ICaptureInterface* CaptureInterface = FCaptureMechanicHelpers::GetValidCaptureInterface(CaptureTarget);
	if (not CaptureInterface || not GetIsValidRTSComponent())
	{
		DoneExecutingCommand(EAbilityID::IdCapture);
		return;
	}

	EnsureSquadUnitsValid();
	const int32 NumSquadUnitsAlive = M_TSquadUnits.Num();
	const int32 UnitsNeededToCapture = CaptureInterface->GetCaptureUnitAmountNeeded();
	if (NumSquadUnitsAlive < UnitsNeededToCapture)
	{
		DoneExecutingCommand(EAbilityID::IdCapture);
		return;
	}

	// Store the target so arrival callbacks can safely use it.
	M_CaptureState.M_TargetCaptureActor = CaptureTarget;

	// Move the squad towards the closest capture location exposed by the actor.
	const FVector MoveToLocation = CaptureInterface->GetCaptureLocationClosestTo(GetActorLocation());
	GeneralMoveToForAbility(MoveToLocation, EAbilityID::IdCapture);
}

void ASquadController::TerminateCaptureCommand()
{
	// If the capture command is cancelled while units are navigating,
	// stop their movement so they do not keep walking to the capture actor.
	TerminateMoveCommand();

	// Reset capture state so late arrivals no longer try to start a capture.
	M_CaptureState.Reset();
}

void ASquadController::RemoveCaptureUnitsFromSquad(const int32 AmountCaptureUnits)
{
	EnsureSquadUnitsValid();
	if (AmountCaptureUnits <= 0 || M_TSquadUnits.Num() == 0 || AmountCaptureUnits > M_TSquadUnits.Num())
	{
		RTSFunctionLibrary::ReportError("Invalid amount of capture units to remove from squad!"
			"\n At function: ASquadController::RemoveCaptureUnitsFromSquad"
			"\n Squad: " + GetName());
		return;
	}

	// Work on a copy so we can safely modify M_TSquadUnits via UnitInSquadDied inside the loop.
	const TArray<ASquadUnit*> CurrentSquadUnits = M_TSquadUnits;

	int32 UnitsToRemove = AmountCaptureUnits;
	for (ASquadUnit* SquadUnit : CurrentSquadUnits)
	{
		if (UnitsToRemove <= 0)
		{
			break;
		}
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		bool bWasSelected = false;
		if (USelectionComponent* SelectionComponent = SquadUnit->GetSelectionComponent())
		{
			bWasSelected = SelectionComponent->GetIsSelected();
		}

		// Use the existing helper so selection, squad array bookkeeping and squad HP aggregation
		// all update exactly the same as on a normal death.
		UnitInSquadDied(SquadUnit, bWasSelected);

		// Prevent this unit from telling the controller again from its own UnitDies / EndPlay path.
		SquadUnit->M_SquadController = nullptr;

		// Make sure the unit does not complete the capture command again when it dies.
		SquadUnit->M_ActiveCommand = EAbilityID::IdIdle;

		// Run the normal death pipeline so the unit cleans up correctly (we use a default death type).
		// todo make sure scavenging death does not trigger voice lines on squad units.
		SquadUnit->UnitDies(ERTSDeathType::Scavenging);

		--UnitsToRemove;
	}
}


void ASquadController::ExecuteAttackCommand(AActor* TargetActor)
{
	// Ensure cargo state is compatible; allow attacking inside,
	// but if later we need to move closer, OnSquadUnitOutOfRange will force an exit before moving.
	if (IsValid(CargoSquad))
	{
		CargoSquad->CheckCargoState(EAbilityID::IdAttack);
	}

	M_UnitsCompletedCommand = 0;

	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			SquadUnit->ExecuteAttackCommand(TargetActor);
		}
	}
}

void ASquadController::TerminateAttackCommand()
{
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			SquadUnit->TerminateAttackCommand();
		}
	}
}

void ASquadController::ExecuteAttackGroundCommand(const FVector GroundLocation)
{
	if (GroundLocation.IsNearlyZero())
	{
		DoneExecutingCommand(EAbilityID::IdAttackGround);
		return;
	}
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			SquadUnit->ExecuteAttackGroundCommand(GroundLocation);
		}
	}
}

void ASquadController::TerminateAttackGroundCommand()
{
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			SquadUnit->TerminateAttackGroundCommand();
		}
	}
}

void ASquadController::ExecutePatrolCommand(const FVector PatrolToLocation)
{
	if (IsValid(CargoSquad))
	{
		CargoSquad->CheckCargoState(EAbilityID::IdPatrol);
	}

	M_UnitsCompletedCommand = 0;

	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			FVector StartLocation = SquadUnit->GetActorLocation();
			SquadUnit->ExecutePatrol(StartLocation, PatrolToLocation);
		}
	}
}

void ASquadController::TerminatePatrolCommand()
{
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			SquadUnit->TerminatePatrol();
		}
	}
}

void ASquadController::ExecutePickupItemCommand(AItemsMaster* TargetItem)
{
	Debug_ItemPickup("EXECUTE Pickup Item");
	if (not IsValid(TargetItem))
	{
		RTSFunctionLibrary::ReportError("Target item not valid for pickup in ASquadController::ExecutePickupItemCommand"
			"aborting command...");
		DoneExecutingCommand(EAbilityID::IdPickupItem);
		return;
	}

	if (!CanSquadPickupWeapon())
	{
		// In this case the squad has already picked up something with an earlier command (shift-queue)
		// and the squad is not able to pick up another weapon.
		DoneExecutingCommand(EAbilityID::IdPickupItem);
		return;
	}
	M_UnitsCompletedCommand = 0;
	M_TargetPickupItemState.M_TargetPickupItem = TargetItem;
	// This will be set to true once the squad is close enough to the item.
	M_TargetPickupItemState.bIsBusyPickingUp = false;
	if (GetDistanceToActor(TargetItem) <= DeveloperSettings::GamePlay::Navigation::SquadUnitWeaponPickupDistance)
	{
		OnItemCloseEnoughInstantPickup();
		return;
	}
	GeneralMoveToForAbility(TargetItem->GetActorLocation(), EAbilityID::IdPickupItem);
}

void ASquadController::OnMoveToScavengeObjectComplete()
{
	if (not IsValid(M_TargetScavengeState.M_TargetScavengeObject))
	{
		return;
	}
	if (!M_TargetScavengeState.bIsBusyScavenging)
	{
		StartScavengeObject(M_TargetScavengeState.M_TargetScavengeObject);
	}
}

void ASquadController::OnScavengingComplete()
{
	EnsureSquadUnitsValid();
	if (not GetIsValidPlayerController() || M_TSquadUnits.Num() <= 0)
	{
		return;
	}
	const bool bIsSelected = M_TSquadUnits[0]->GetIsSelected();
	if (bIsSelected)
	{
		PlayerController->PlayVoiceLine(this, ERTSVoiceLine::CommandCompleted, false, false);
	}
	else
	{
		M_TSquadUnits[0]->PlaySpatialVoiceLine(ERTSVoiceLine::CommandCompleted, false);
	}
	DoneExecutingCommand(EAbilityID::IdScavenge);
}

void ASquadController::OnSquadUnitArrivedAtCaptureActor()
{
	ICaptureInterface* CaptureInterface =
		FCaptureMechanicHelpers::GetValidCaptureInterface(M_CaptureState.M_TargetCaptureActor);
	if (not CaptureInterface || not GetIsValidRTSComponent())
	{
		DoneExecutingCommand(EAbilityID::IdCapture);
		return;
	}

	// Only the first arriving unit should start the capture.
	if (M_CaptureState.bIsCaptureInProgress)
	{
		return;
	}

	EnsureSquadUnitsValid();
	const int32 NumSquadUnitsAlive = M_TSquadUnits.Num();
	const int32 UnitsNeededToCapture = CaptureInterface->GetCaptureUnitAmountNeeded();
	if (NumSquadUnitsAlive < UnitsNeededToCapture)
	{
		DoneExecutingCommand(EAbilityID::IdCapture);
		return;
	}

	M_CaptureState.bIsCaptureInProgress = true;

	// 1. Remove the committed units from the squad (destroy them, update HP, selection, etc.).
	RemoveCaptureUnitsFromSquad(UnitsNeededToCapture);

	// 2. Start capture logic on the actor (which will run its timer and call the BP event on completion).
	const int32 OwningPlayer = RTSComponent->GetOwningPlayer();
	CaptureInterface->OnCaptureByPlayer(OwningPlayer);

	// 3. Capture command is done from the squad's perspective; the actor handles the progress / completion.
	DoneExecutingCommand(EAbilityID::IdCapture);
}

float ASquadController::GetDistanceToActor(const TObjectPtr<AActor> TargetActor)
{
	// find squad unit with closest distance to the item.
	float ClosestDistance = FLT_MAX;
	for (const auto EachSquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(EachSquadUnit))
		{
			continue;
		}
		const float Distance = FVector::Dist(EachSquadUnit->GetActorLocation(), TargetActor->GetActorLocation());
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
		}
	}
	if (DeveloperSettings::Debugging::GScavenging_Compile_DebugSymbols ||
		DeveloperSettings::Debugging::GSquadUnit_SwitchPickUp_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			"The closest distance to the item is: " + FString::SanitizeFloat(ClosestDistance), FColor::Purple);
	}
	return ClosestDistance;
}

void ASquadController::OnItemCloseEnoughInstantPickup()
{
	OnWeaponPickup.Broadcast();
	Debug_ItemPickup("Instant pickup!");
	if (!M_TargetPickupItemState.M_TargetPickupItem)
	{
		RTSFunctionLibrary::ReportError("Pickup item is not valid in ASquadController::OnItemCloseEnoughInstantPickup");
		return;
	}
	// The item is close enough for instant pickup.
	AWeaponPickup* WeaponItem = Cast<AWeaponPickup>(M_TargetPickupItemState.M_TargetPickupItem);
	if (IsValid(WeaponItem))
	{
		StartPickupWeapon(WeaponItem);
		DoneExecutingCommand(EAbilityID::IdPickupItem);
	}
	else
	{
		RTSFunctionLibrary::ReportError("TODO: logic for squad controller to pickup items other than weapons."
			"\n at function ExecutePickupItemCommand");
	}
}

void ASquadController::CheckCloseEnoughToItemPickupAnyway()
{
	if (not IsValid(M_TargetPickupItemState.M_TargetPickupItem) || M_TargetPickupItemState.bIsBusyPickingUp)
	{
		return;
	}
	// Check if close enough even when no overlap happened.
	if (GetDistanceToActor(M_TargetPickupItemState.M_TargetPickupItem) >
		DeveloperSettings::GamePlay::Navigation::SquadUnitWeaponPickupDistance)
	{
		return;
	}
	Debug_ItemPickup("A unit of the squad arrived at the pickup item but no overlap happened,"
		"we force a start by casting the pickup item");
	OnItemCloseEnoughInstantPickup();
}

void ASquadController::TerminatePickupItemCommand()
{
	// Reset pickup target pointer.
	M_TargetPickupItemState.Reset();
	for (const auto EachSquad : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(EachSquad))
		{
			continue;
		}
		// todo different for different items.
		EachSquad->TerminatePickupWeapon();
	}
}

void ASquadController::ExecuteSwitchWeaponsCommand()
{
	M_UnitsCompletedCommand = 0;
	for (const auto EachSquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(EachSquadUnit))
		{
			continue;
		}
		bool bValidComp = false;
		if (EachSquadUnit->GetHasSecondaryWeapon(bValidComp) && !EachSquadUnit->bM_IsSwitchingWeapon)
		{
			EachSquadUnit->ExecuteSwitchWeapon();
			continue;
		}
		// Squad units with no weapon will instantly finish the command.
		M_UnitsCompletedCommand++;
	}
	if (M_UnitsCompletedCommand >= M_TSquadUnits.Num())
	{
		DoneExecutingCommand(EAbilityID::IdSwitchWeapon);
	}
}

void ASquadController::TerminateSwitchWeaponsCommand()
{
	for (const auto EachSquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(EachSquadUnit))
		{
			EachSquadUnit->TerminateSwitchWeapon();
		}
	}
}

void ASquadController::ExecuteReverseCommand(const FVector ReverseToLocation)
{
	if (IsValid(CargoSquad))
	{
		CargoSquad->CheckCargoState(EAbilityID::IdReverseMove);
	}
	// Squad units do not reverse; use regular move function
	ExecuteMoveCommand(ReverseToLocation);
}

void ASquadController::TerminateReverseCommand()
{
	TerminateMoveCommand();
}

void ASquadController::ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand)
{
	M_UnitsCompletedCommand = 0;

	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			SquadUnit->ExecuteRotateTowards(RotateToRotator);
		}
	}
}

void ASquadController::TerminateRotateTowardsCommand()
{
	// No additional termination logic needed for rotation
}

void ASquadController::ExecuteScavengeObject(AActor* TargetObject)
{
	Debug_Scavenging("Execute scavenging");
	AScavengeableObject* ScavengeableObject = Cast<AScavengeableObject>(TargetObject);
	if (not ExeScav_EnsureValidObj(ScavengeableObject))
	{
		DoneExecutingCommand(EAbilityID::IdScavenge);
		return;
	}
	FVector SquadUnitLocation = FVector(0.0f);
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}
		if (UScavengerComponent* ScavComp = SquadUnit->GetScavengerComponent(); IsValid(
			SquadUnit->GetScavengerComponent()))
		{
			ScavComp->SetScavengeState(EScavengeState::MoveToObject);
			ScavComp->SetTargetScavengeObject(ScavengeableObject);
			ScavComp->UpdateOwnerBlockScavengeObjects(false);
			SquadUnitLocation = SquadUnit->GetActorLocation();
		}
	}

	M_UnitsCompletedCommand = 0;
	M_TargetScavengeState.M_TargetScavengeObject = ScavengeableObject;
	M_TargetScavengeState.bIsBusyScavenging = false;
	// If the squad is close enough to the obj start immediately; otherwise move to closest scav point.
	ExeScav_StartOrMoveToScavObj(ScavengeableObject, SquadUnitLocation);
}

void ASquadController::TerminateScavengeObject()
{
	// Loop through all squad units and instruct them to terminate scavenging
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit))
		{
			SquadUnit->TerminateScavenging();
		}
	}

	// Re-enable the scavengeable object so other squads can scavenge it this is not valid
	// if the squad completely scavenged the object; expected, no error report.
	if (IsValid(M_TargetScavengeState.M_TargetScavengeObject))
	{
		// This will also make the scav object valid for new scavenging.
		M_TargetScavengeState.M_TargetScavengeObject->PauseScavenging(this);
	}

	// Reset the scavenge state
	M_TargetScavengeState.Reset();
}

void ASquadController::ExecuteRepairCommand(AActor* TargetActor)
{
	if (not FRTSRepairHelpers::GetIsUnitValidForRepairs(TargetActor))
	{
		FRTSRepairHelpers::Debug_Repair("Unit not valid for repairs, Terminating command...");
		DoneExecutingCommand(EAbilityID::IdRepair);
		return;
	}
	EnsureSquadUnitsValid();
	TArray<URepairComponent*> RepairComponents;
	for (auto EachSquadUnit : M_TSquadUnits)
	{
		URepairComponent* RepairComp = EachSquadUnit->GetRepairComponent();
		if (not IsValid(RepairComp))
		{
			RTSFunctionLibrary::ReportError(
				"Repair component is not valid for squad unit: " + EachSquadUnit->GetName() +
				"\n at function: ASquadController::ExecuteRepairCommand"
			);
			DoneExecutingCommand(EAbilityID::IdRepair);
			return;
		}
		RepairComponents.Add(RepairComp);
	}
	bool bAreOffsetsValid = false;
	TArray<FVector> EachRepairOffset = FRTSRepairHelpers::GetRepairSquadOffsets(
		M_TSquadUnits.Num(), TargetActor, bAreOffsetsValid);
	if (not bAreOffsetsValid)
	{
		RTSFunctionLibrary::ReportError("failed to get repair offsets for squad of size: " +
			FString::FromInt(M_TSquadUnits.Num()) + "\n at function: ASquadController::ExecuteRepairCommand"
			"\n Will terminate repair command.");
		DoneExecutingCommand(EAbilityID::IdRepair);
		return;
	}
	for (int32 i = 0; i < RepairComponents.Num(); i++)
	{
		URepairComponent* RepairComp = RepairComponents[i];
		FVector RepairOffset = EachRepairOffset.IsValidIndex(i) ? EachRepairOffset[i] : FVector::ZeroVector;
		// If the repair component is valid, start the repair command.
		RepairComp->ExecuteRepair(TargetActor, RepairOffset);
	}
	M_RepairState.M_TargetRepairObject = TargetActor;
}

void ASquadController::TerminateRepairCommand()
{
	M_RepairState.Reset();
	EnsureSquadUnitsValid();
	for (const auto EachUnit : M_TSquadUnits)
	{
		URepairComponent* RepairComp = EachUnit->GetRepairComponent();
		if (RepairComp)
		{
			RepairComp->TerminateRepair();
		}
	}
}

void ASquadController::ExecuteEnterCargoCommand(AActor* CarrierActor)
{
	// Start the cargo enter flow on the component. This will:
	// - validate the target cargo
	// - path the squad to the closest entrance with Ability = IdEnterCargo
	// - when all units finish that move, call UCargoSquad::OnEnterCargo_MoveToEntranceCompleted(),
	//   which seats/attaches everyone and calls DoneExecutingCommand(IdEnterCargo).
	if (!IsValid(CargoSquad))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "CargoSquad", "ASquadController::ExecuteEnterCargoCommand");
		DoneExecutingCommand(EAbilityID::IdEnterCargo);
		return;
	}

	M_UnitsCompletedCommand = 0; // we count unit move completions for IdEnterCargo
	CargoSquad->ExecuteEnterCargo(CarrierActor);
}

void ASquadController::TerminateEnterCargoCommand()
{
	// If the queue is cleared or the command is interrupted, make sure any in-flight
	// "move to entrance" is stopped. We do NOT force an exit here; if the squad already
	// boarded, the command has finished and Terminate is a no-op (consistent with other abilities).
	StopBehaviourTree();
	StopMovement();

	// Note: Do not call ExitCargoImmediate() here; termination must not undo the result.
	// (If you need a “cancel boarding” behavior later, expose one on UCargoSquad and gate it here.)
}

void ASquadController::ExecuteExitCargoCommand()
{
	// Explicit player command: exit immediately and mark command done from the component.
	if (!IsValid(CargoSquad))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "CargoSquad", "ASquadController::ExecuteExitCargoCommand");
		DoneExecutingCommand(EAbilityID::IdExitCargo);
		return;
	}
	CargoSquad->ExecuteExitCargo_Explicit();
}

void ASquadController::TerminateExitCargoCommand()
{
	// Nothing to clean up; exit is instantaneous and already handled by UCargoSquad.
}


void ASquadController::StopBehaviourTree()
{
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit) || not SquadUnit->GetIsValidAISquadUnit())
		{
			continue;
		}
		UBrainComponent* BrainComp = SquadUnit->M_AISquadUnit->GetBrainComponent();
		if (BrainComp)
		{
			BrainComp->StopLogic("Command Terminated");
		}
	}
}

void ASquadController::StopMovement()
{
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (GetIsValidSquadUnit(SquadUnit) && SquadUnit->GetIsValidAISquadUnit())
		{
			SquadUnit->M_AISquadUnit->StopMovement();
		}
	}
}

void ASquadController::StartPickupWeapon(AWeaponPickup* TargetWeaponItem)
{
	// TargetWeaponItem == pickup item saved at this squad controller has already been checked at the item itself.
        if (not GetIsValidRTSComponent() || not GetIsValidWeaponPickup(TargetWeaponItem))
        {
                return;
        }

        if (not IsPickupWeaponCommandActive(TargetWeaponItem))
        {
                return;
        }

        TObjectPtr<ASquadUnit> SquadUnitWithLowestWeaponValue = FindSquadUnitWithLowestWeaponValueForPickup();
        if (not SquadUnitWithLowestWeaponValue)
        {
                // todo in game visual feedback that no unit can pick up the weapon.
                RTSFunctionLibrary::ReportError("No squad unit found with place for a second weapon on squad " + GetName());
                return;
        }

        // Calls Destroy and SetItemDisabled on the pick up to make sure only this unit can pick it up.
        SquadUnitWithLowestWeaponValue->StartPickupWeapon(TargetWeaponItem);
        OnWeaponPickup.Broadcast();
}

bool ASquadController::GetIsValidWeaponPickup(AWeaponPickup* TargetWeaponItem) const
{
        if (not IsValid(TargetWeaponItem))
        {
                return false;
        }

        return TargetWeaponItem->EnsurePickupInitialized();
}

bool ASquadController::IsPickupWeaponCommandActive(const AWeaponPickup* TargetWeaponItem) const
{
        const EAbilityID CurrentAbility = M_UnitCommandData->GetCurrentlyActiveCommandType();
        if (CurrentAbility == EAbilityID::IdPickupItem)
        {
                return true;
        }

        const FString ItemName = IsValid(TargetWeaponItem) ? TargetWeaponItem->GetName() : "null";
        RTSFunctionLibrary::ReportError(
                "Pickup item of name: " + ItemName + " is not the current command type for squad: " + GetName() +
                "\n But did trigger a StartPickupWeapon!");
        return false;
}

TObjectPtr<ASquadUnit> ASquadController::FindSquadUnitWithLowestWeaponValueForPickup()
{
        TObjectPtr<ASquadUnit> SquadUnitWithLowestWeaponValue = nullptr;
        int32 LowestWeaponValue = TNumericLimits<int32>::Max();
        for (ASquadUnit* SquadUnit : M_TSquadUnits)
        {
                if (not CanSquadUnitPickupWeapon(SquadUnit))
                {
                        continue;
                }

                const int32 WeaponValue = GetWeaponValueForSquadUnit(SquadUnit);
                if (WeaponValue >= LowestWeaponValue)
                {
                        continue;
                }

                LowestWeaponValue = WeaponValue;
                SquadUnitWithLowestWeaponValue = SquadUnit;
        }

        return SquadUnitWithLowestWeaponValue;
}

bool ASquadController::CanSquadUnitPickupWeapon(const ASquadUnit* SquadUnit) const
{
        if (not GetIsValidSquadUnit(SquadUnit))
        {
                return false;
        }

        bool bHasValidSecondaryWeaponComponent = false;
        const bool bHasSecondaryWeapon = SquadUnit->GetHasSecondaryWeapon(bHasValidSecondaryWeaponComponent);
        if (not bHasValidSecondaryWeaponComponent)
        {
                return false;
        }

        if (bHasSecondaryWeapon)
        {
                return false;
        }

        if (SquadUnit->bM_IsSwitchingWeapon)
        {
                RTSFunctionLibrary::PrintString(
                        "Skipped unit for pickup as the unit is busy with switching weapons!!!.", FColor::Red);
                return false;
        }

        return true;
}

int32 ASquadController::GetWeaponValueForSquadUnit(const ASquadUnit* SquadUnit) const
{
        if (not GetIsValidSquadUnit(SquadUnit))
        {
                return TNumericLimits<int32>::Max();
        }

        const UWeaponState* WeaponState = SquadUnit->GetWeaponState();
        if (not IsValid(WeaponState))
        {
                RTSFunctionLibrary::ReportError(
                        "Squad unit weapon state invalid when selecting pickup target for squad: " + GetName());
                return TNumericLimits<int32>::Max();
        }

        return Global_GetWeaponValue(WeaponState->GetRawWeaponData().WeaponName);
}

bool ASquadController::ExeScav_EnsureValidObj(const AScavengeableObject* ScavengeableObject)
{
	if (not IsValid(ScavengeableObject))
	{
		RTSFunctionLibrary::ReportError("Invalid target for scavenging in ASquadController::ExecuteScavengeObject");
		return false;
	}
	if (!ScavengeableObject->GetCanScavenge())
	{
		Debug_Scavenging("Scavengable object is not valid for scavenging: " + ScavengeableObject->GetName());
		return false;
	}
	Debug_Scavenging("Squad set to scavenge object: " + ScavengeableObject->GetName());
	return true;
}


void ASquadController::StartScavengeObject(AScavengeableObject* ScavengeableObject)
{
	if (!IsValid(ScavengeableObject) || !ScavengeableObject->GetCanScavenge())
	{
		DoneExecutingCommand(EAbilityID::IdScavenge);
		return;
	}
	M_TargetScavengeState.bIsBusyScavenging = true;

	ScavengeableObject->SetIsScavengeEnabled(false);
	// Remove any invalid units.
	EnsureSquadUnitsValid();

	TArray<ASquadUnit*> ScavengingUnits = M_TSquadUnits;
	// Get scavenge positions from the scavengeable object
	TArray<FVector> ScavengeLocations = ScavengeableObject->GetScavengePositions(ScavengingUnits.Num());

	// Assign locations to units
	for (int32 i = 0; i < ScavengingUnits.Num(); ++i)
	{
		ASquadUnit* SquadUnit = ScavengingUnits[i];

		if (SquadUnit->GetHasValidScavengerComp())
		{
			FVector ScavengeLocation = ScavengeLocations.IsValidIndex(i)
				                           ? ScavengeLocations[i]
				                           : ScavengeLocations.Last();

			UScavengerComponent* ScavengerComp = SquadUnit->GetScavengerComponent();
			ScavengerComp->SetScavengeState(EScavengeState::MoveToScavengeLocation);
			SquadUnit->ExecuteMoveToSelfPathFinding(ScavengeLocation, EAbilityID::IdScavenge, false);
			// Teleports in two seconds.
			ScavengerComp->TeleportAndRotateAtScavSocket(ScavengeLocation);
		}
	}
}

void ASquadController::ExeScav_StartOrMoveToScavObj(AScavengeableObject* ScavengeableObject,
                                                    const FVector& SquadUnitLocation)
{
	const float DistanceToObject = GetDistanceToActor(ScavengeableObject);
	if (DistanceToObject <= DeveloperSettings::GamePlay::Navigation::SquadUnitScavengeDistance)
	{
		if (DeveloperSettings::Debugging::GScavenging_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Squad is close enough to scavenge object, instant scavenge!",
			                                FColor::Red);
		}
		StartScavengeObject(ScavengeableObject);
	}
	else
	{
		// Move to closest scavenge location socket with respect to one of the units in the squad.
		GeneralMoveToForAbility(ScavengeableObject->GetScavLocationClosestToPosition(SquadUnitLocation),
		                        EAbilityID::IdScavenge);
	}
}

FVector ASquadController::GetOwnerLocation() const
{
	return GetActorLocation();
}

FString ASquadController::GetOwnerName() const
{
	return GetName();
}

AActor* ASquadController::GetOwnerActor()
{
	return this;
}

void ASquadController::OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing)
{
}

void ASquadController::InitSquadData_InitExperienceComponent()
{
	if (not EnsureValidExperienceComponent() || not GetIsValidRTSComponent())
	{
		return;
	}
	FTrainingOption UnitTypeAndSubType;
	UnitTypeAndSubType.UnitType = RTSComponent->GetUnitType();
	UnitTypeAndSubType.SubtypeValue = RTSComponent->GetUnitSubType();

	ExperienceComponent->InitExperienceComponent(
		this,
		UnitTypeAndSubType,
		RTSComponent->GetOwningPlayer());
}


FVector ASquadController::FindNavigablePointNear(const FVector& Location, const float Radius) const
{
	if (!GetWorld())
	{
		return Location;
	}

	const UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem)
	{
		return Location;
	}

	FNavLocation NavLocation;
	if (NavSystem->GetRandomPointInNavigableRadius(Location, Radius, NavLocation))
	{
		return NavLocation.Location;
	}
	return Location;
}

void ASquadController::EnsureSquadUnitsValid()
{
	M_TSquadUnits.RemoveAllSwap([](ASquadUnit* U) { return !IsValid(U); });
}

FVector ASquadController::FindIdealSpawnLocation()
{
	// Use the next grid point as the initial location
	const FVector InitialLocation = FindIdealSpawnLocation_GetNextGridPoint();
	FVector IdealSpawnLocation = InitialLocation;

	// Attempt to project the initial location onto the navmesh within a 200x200x200 radius
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys)
	{
		FNavLocation ProjectedLocation;
		const FVector ProjectionExtent(20.f, 20.f, 200.f);

		if (NavSys->ProjectPointToNavigation(InitialLocation, ProjectedLocation, ProjectionExtent))
		{
			// Successfully projected onto navmesh
			return ProjectedLocation.Location;
		}
	}

	// If NavMesh projection failed, perform a visibility trace from 250 units above the initial location down to the initial position
	const FVector TraceStart = InitialLocation + FVector(0.f, 0.f, 250.f);
	const FVector TraceEnd = InitialLocation;

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		CollisionParams
	);

	if (bHit)
	{
		// Successfully hit something; use the hit location plus 50 units in Z
		const FVector SpawnLocation = HitResult.Location + FVector(0.f, 0.f, 50.f);
		return SpawnLocation;
	}
	RTSFunctionLibrary::PrintString(TEXT("Visibility trace failed."), FColor::Red);

	// If both methods fail, fall back to initial location plus 100 units in Z
	IdealSpawnLocation += FVector(0.f, 0.f, 100.f);
	RTSFunctionLibrary::PrintString(TEXT("Using fallback spawn location."), FColor::Yellow);
	return IdealSpawnLocation;
}


void ASquadController::BeginReinforcementSpawnGrid(const FVector& OriginLocation)
{
        StartGeneratingSpawnLocations(OriginLocation);
}

FVector ASquadController::GetNextReinforcementSpawnLocation()
{
        return FindIdealSpawnLocation();
}

void ASquadController::RegisterReinforcedUnit(ASquadUnit* ReinforcedUnit)
{
        if (not IsValid(ReinforcedUnit))
        {
	        RTSFunctionLibrary::ReportError(TEXT("Attempted to register invalid reinforced unit." 
                        "\nASquadController::RegisterReinforcedUnit"));
                return;
        }
        if (M_TSquadUnits.Contains(ReinforcedUnit))
        {
                return;
        }
        M_TSquadUnits.Add(ReinforcedUnit);
}


void ASquadController::StartGeneratingSpawnLocations(const FVector& GridOriginLocation)
{
        M_SpawnIndex = 0;
        M_GridOriginLocation = GridOriginLocation;
        M_GridSize = 250.0f;

	const float HalfGridSize = M_GridSize / 2.0f;
	const float MaxRandomOffset = M_GridSize * 0.33f;

	// Base offsets for the grid points
	const TArray<FVector> BaseOffsets = {
		FVector(-HalfGridSize, -HalfGridSize, 0.0f), // Bottom Left
		FVector(-HalfGridSize, HalfGridSize, 0.0f), // Top Left
		FVector(HalfGridSize, -HalfGridSize, 0.0f), // Bottom Right
		FVector(HalfGridSize, HalfGridSize, 0.0f), // Top Right
		FVector(0.0f, -HalfGridSize, 0.0f), // Middle Bottom
		FVector(-HalfGridSize, 0.0f, 0.0f), // Middle Left
		FVector(0.0f, HalfGridSize, 0.0f), // Middle Top
		FVector(HalfGridSize, 0.0f, 0.0f) // Middle Right
	};

	// Initialize M_Offsets with randomness
	M_Offsets.Empty();
	M_Offsets.Reserve(BaseOffsets.Num());

	for (const FVector& Offset : BaseOffsets)
	{
		FVector RandomizedOffset = Offset;

		// Add random offset to X and Y components
		const float RandomX = FMath::FRandRange(-MaxRandomOffset, MaxRandomOffset);
		const float RandomY = FMath::FRandRange(-MaxRandomOffset, MaxRandomOffset);

		RandomizedOffset.X += RandomX;
		RandomizedOffset.Y += RandomY;

		M_Offsets.Add(RandomizedOffset);
	}
}


FVector ASquadController::FindIdealSpawnLocation_GetNextGridPoint()
{
	constexpr int32 MaxUnits = 8;
	if (M_SpawnIndex >= MaxUnits)
	{
		RTSFunctionLibrary::ReportError("SpawnIndex exceeds the number of grid points.");
		return M_GridOriginLocation;
	}

	// Get the next offset from the precomputed offsets
	const FVector NextOffset = M_Offsets[M_SpawnIndex];
	M_SpawnIndex++;

	// Calculate the next grid point by adding the offset to the grid origin
	const FVector NextPoint = M_GridOriginLocation + NextOffset;
	return NextPoint;
}


FVector ASquadController::ProjectLocationOnNavMesh(const FVector& Location, const float Radius,
                                                   const bool bProjectInZDirection) const
{
	const float ProjectionRadius = Radius > 0 ? Radius : 200.f;
	const float ZDirectionRadius = bProjectInZDirection ? ProjectionRadius : 5.f;
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (not NavSys)
	{
		return Location;
	}
	FNavLocation ProjectedLocation;
	const FVector ProjectionExtent(ProjectionRadius, ProjectionRadius, ZDirectionRadius);

	if (NavSys->ProjectPointToNavigation(Location, ProjectedLocation, ProjectionExtent))
	{
		// Successfully projected onto navmesh
		return ProjectedLocation.Location;
	}
	return Location;
}

void ASquadController::UpdateControllerPositionToAverage()
{
	// Calculate the average position of all valid squad units
	FVector AveragePosition = FVector::ZeroVector;
	int32 ValidUnitCount = 0;

	for (const ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (IsValid(SquadUnit))
		{
			AveragePosition += SquadUnit->GetActorLocation();
			ValidUnitCount++;
		}
	}

	if (ValidUnitCount > 0)
	{
		AveragePosition /= ValidUnitCount;
		// Move the controller to the average position of the squad units
		SetActorLocation(AveragePosition);
	}
}

void ASquadController::Debug_Scavenging(const FString& DebugMessage) const
{
	if (DeveloperSettings::Debugging::GScavenging_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(DebugMessage);
	}
}


void ASquadController::Debug_ItemPickup(const FString& DebugMessage) const
{
	if (DeveloperSettings::Debugging::GSquadUnit_SwitchPickUp_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(DebugMessage);
	}
}

void ASquadController::InitSquadData()
{
	if (not M_SquadLoadingStatus.bM_HasFinishedLoading || M_SquadLoadingStatus.bM_HasInitializedData || not
		GetIsValidRTSComponent())
	{
		const FString RTSValid = GetIsValidRTSComponent() ? "true" : "false";
		RTSFunctionLibrary::ReportError("Attempting to initialize squad data before squad units have finished loading!"
			"Or data was already set or no valid rts comp!"
			"\n RTS Component valid: " + RTSValid +
			"\n SquadController: " + GetName());
		return;
	}
	EnsureSquadUnitsValid();
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not GameState)
	{
		return;
	}
	FSquadData SquadData = GameState->GetSquadDataOfPlayer(
		RTSComponent->GetOwningPlayer(),
		RTSComponent->GetSubtypeAsSquadSubtype());
	InitSquadData_SetValues(SquadData.MaxWalkSpeedCms,
	                        SquadData.MaxAcceleration, SquadData.MaxHealth, SquadData.VisionRadius,
	                        RTSComponent->GetSubtypeAsSquadSubtype(), SquadData.ResistancesAndDamageMlt);
	InitSquadData_SetAbilities(SquadData.Abilities);
	InitSquadData_InitExperienceComponent();
	// After units are fully valid and any BP HP tweaks have run
	InitSquadData_SetupSquadHealthAggregation();
	// Notify callbacks set and future ones that the blueprint data is loaded on the squad controller.
	SquadDataCallbacks.SetDataLoaded(true);
	SquadDataCallbacks.OnSquadDataReady.Broadcast();
	M_SquadLoadingStatus.bM_HasInitializedData = true;
}

void ASquadController::InitSquadData_SetValues(const float MaxWalkSpeed, const float MaxAcceleration,
                                               const float MaxHealth, const float VisionRadius,
                                               const ESquadSubtype SquadSubtype,
                                               const FResistanceAndDamageReductionData& ResistanceData)
{
	EnsureSquadUnitsValid();
	for (const auto EachSqUnit : M_TSquadUnits)
	{
		if (IsValid(EachSqUnit->GetCharacterMovement()))
		{
			EachSqUnit->GetCharacterMovement()->MaxWalkSpeed = MaxWalkSpeed;
			EachSqUnit->GetCharacterMovement()->MaxAcceleration = MaxAcceleration;
		}
		if (IsValid(EachSqUnit->HealthComponent))
		{
			EachSqUnit->HealthComponent->InitHealthAndResistance(ResistanceData, MaxHealth);
			EachSqUnit->OnSquadInitsData_OverwriteArmorAndResistance(MaxHealth);
		}
		if (IsValid(EachSqUnit->GetRTSComponent()))
		{
			EachSqUnit->GetRTSComponent()->SetUnitSubtype(ENomadicSubtype::Nomadic_None, ETankSubtype::Tank_None,
			                                              SquadSubtype);
			EachSqUnit->GetRTSComponent()->SetUnitType(EAllUnitType::UNType_Squad);
		}
	}

	if (IsValid(SquadHealthComponent))
	{
		// Sets TargetTypeIcon (and stores resist data) so the squad bar matches its members.
		SquadHealthComponent->InitHealthAndResistance(ResistanceData, MaxHealth);
	}

	SetSquadVisionRange(VisionRadius);
}

void ASquadController::InitSquadData_SetAbilities(const TArray<EAbilityID>& Abilities)
{
	InitAbilityArray(Abilities);
}

void ASquadController::OnRepairUnitNotValidForRepairs()
{
	if (GetActiveCommandID() != EAbilityID::IdRepair)
	{
		const FString NameOfActiveCommand = Global_GetAbilityIDAsString(GetActiveCommandID());
		RTSFunctionLibrary::ReportError("The squad controller is told by repair component of one of the units"
			"that the target to repair is not valid for repairs,"
			"however there is no repair ability active!"
			"\n Active command: " + NameOfActiveCommand);
		return;
	}
	DoneExecutingCommand(EAbilityID::IdRepair);
}

void ASquadController::OnRepairTargetFullyRepaired(AActor* RepairTarget)
{
	if (not RepairTarget || RepairTarget != M_RepairState.M_TargetRepairObject)
	{
		const FString TargetName = IsValid(RepairTarget) ? RepairTarget->GetName() : "null";
		const FString ControllerTargetName = IsValid(M_RepairState.M_TargetRepairObject)
			                                     ? M_RepairState.M_TargetRepairObject->GetName()
			                                     : "null";
		RTSFunctionLibrary::ReportError("A repair component told squad controller that the target: " + TargetName +
			" is fully repaired but it does not match the target at the controller"
			"\n Controller Repair Target: " + ControllerTargetName);
	}
	DoneExecutingCommand(EAbilityID::IdRepair);
}

void ASquadController::UpdateSquadWeaponIcon()
{
	EnsureSquadUnitsValid();
	EWeaponName WeaponWithHighestValue = EWeaponName::DEFAULT_WEAPON;
	int32 HighestWeaponValue = -1;
	for (auto EachSqUnit : M_TSquadUnits)
	{
		if (not EachSqUnit->GetWeaponState())
		{
			continue;
		}
		const EWeaponName WeaponNameOfUnit = EachSqUnit->GetWeaponState()->GetRawWeaponData().WeaponName;
		if (Global_GetWeaponValue(WeaponNameOfUnit) > HighestWeaponValue)
		{
			HighestWeaponValue = Global_GetWeaponValue(WeaponNameOfUnit);
			WeaponWithHighestValue = WeaponNameOfUnit;
		}
	}
	SetWeaponIcon(WeaponWithHighestValue);
}

void ASquadController::SetWeaponIcon(const EWeaponName HighestValuedWeapon)
{
	ESquadWeaponIcon SquadWeaponIcon = Global_GetWeaponIconForWeapon(HighestValuedWeapon);
	// Default = no icon; nullptr hides the icon.
	USlateBrushAsset* Asset = nullptr;
	if (not GetIsValidSquadHealthComponent())
	{
		return;
	}
	const bool bNotNone = SquadWeaponIcon != ESquadWeaponIcon::None;
	if (const USquadWeaponIconSettings* Settings = USquadWeaponIconSettings::Get(); Settings && bNotNone)
	{
		const USlateBrushAsset* SlateBrushAsset = Settings->GetBrushForWeaponIconType(SquadWeaponIcon);
		if (not SlateBrushAsset)
		{
			const FString SquadWeaponIconStringFromUEnum = UEnum::GetValueAsString(SquadWeaponIcon);
			RTSFunctionLibrary::ReportError(
				"Squad controller wanted to set weapon icon with non null squad weapon icon of type:"
				+ SquadWeaponIconStringFromUEnum
				+ "\n but no asset was found in the USquadWeaponIconSettings! Does the mapping entry exist?");
			return;
		}
	}
	SquadHealthComponent->UpdateSquadWeaponIcon(Asset);
	
}

bool ASquadController::GetIsValidSquadUnit(const ASquadUnit* Unit) const
{
	if (!IsValid(Unit))
	{
		RTSFunctionLibrary::ReportError("Squad Unit is null when trying to accesss from array in SquadController!");
		return false;
	}
	return true;
}

bool ASquadController::GetIsValidPlayerController()
{
	if (IsValid(PlayerController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this, "PlayerController",
	                                             "ASquadController::GetIsValidPlayerController"
	                                             "\n attempting repair...");
	if (const UWorld* World = GetWorld())
	{
		APlayerController* Controller = World->GetFirstPlayerController();
		if (IsValid(Controller))
		{
			this->PlayerController = Cast<ACPPController>(Controller);
			return true;
		}
	}
	return false;
}

void ASquadController::BeginPlay_SetupPlayerController()
{
	PlayerController = FRTS_Statics::GetRTSController(this);
	(void)GetIsValidPlayerController();
}

void ASquadController::LoadSquadUnitsAsync()
{
	// Create a StreamableManager to manage async loading
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	// Starts the grid generation at the actor location of the squad, later used at OnSquadUnitClassLoaded for the grid location.
	StartGeneratingSpawnLocations(GetActorLocation());
	M_SquadLoadingStatus.bM_HasFinishedLoading = false;
	M_SquadLoadingStatus.M_AmountOfSquadUnitsToLoad = SquadUnitClasses.Num();

	// Loop through each soft class reference and start async loading
	for (TSoftClassPtr<ASquadUnit> SoftClass : SquadUnitClasses)
	{
		if (SoftClass.IsValid())
		{
			// Already loaded
			OnSquadUnitClassLoaded(SoftClass);
			continue;
		}
		// Asynchronously load the class
		StreamableManager.RequestAsyncLoad(SoftClass.ToSoftObjectPath(),
		                                   FStreamableDelegate::CreateUObject(
			                                   this, &ASquadController::OnSquadUnitClassLoaded, SoftClass));
	}
}

void ASquadController::OnSquadUnitClassLoaded(TSoftClassPtr<ASquadUnit> LoadedClass)
{
	FString ErrorMessage;

	if (!LoadedClass.IsValid())
	{
		ErrorMessage = "Invalid Loaded Class.";
	}
	else
	{
		// Ensure the class is loaded
		UClass* UnitClass = LoadedClass.Get();

		if (UnitClass)
		{
			// Uses the grid initialized around the squad controller earlier;
			FVector Spawnlocation = FindIdealSpawnLocation();
			// Find a random point in the navigable radius from the controller's location
			const UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
			// Use the hit location for spawning
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			Spawnlocation.Z += 50;

			// Spawn the unit
			ASquadUnit* SpawnedUnit = GetWorld()->SpawnActor<ASquadUnit>(
				UnitClass, Spawnlocation, FRotator::ZeroRotator, SpawnParameters);
			if (IsValid(SpawnedUnit))
			{
				// Set the squad controller reference on the unit
				SpawnedUnit->SetSquadController(this);
				M_TSquadUnits.Add(SpawnedUnit);
			}
			else
			{
				ErrorMessage = "Unable to spawn valid squad unit.";
			}
		}
		else
		{
			ErrorMessage = "Unable to find valid navigation system.";
		}
	}

	// Report the collected error message if any
	if (!ErrorMessage.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(ErrorMessage + "\n for squad:" + GetName());
	}
	M_SquadLoadingStatus.M_AmountOfSquadUnitsToLoad--;
	if (M_SquadLoadingStatus.M_AmountOfSquadUnitsToLoad <= 0)
	{
		// All squad units have been loaded
		OnAllSquadUnitsLoaded();
	}
}

void ASquadController::OnAllSquadUnitsLoaded()
{
	M_SquadLoadingStatus.bM_HasFinishedLoading = true;
	UpdateSquadWeaponIcon();
	// Did the squad type not get set yet? Then wait till Bp calls on when it is and the data will be initialized at
	// OnSquadSubtypeSet
	if (not GetIsValidRTSComponent() || RTSComponent->GetSubtypeAsSquadSubtype() == ESquadSubtype::Squad_None)
	{
		return;
	}
	if (not M_SquadLoadingStatus.bM_HasInitializedData)
	{
		InitSquadData();
	}
}
