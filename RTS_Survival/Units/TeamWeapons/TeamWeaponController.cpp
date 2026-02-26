// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponController.h"

#include "TeamWeapon.h"
#include "TeamWeaponMover.h"
#include "CrewPositions/CrewPosition.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "Algo/Sort.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"

namespace TeamWeaponControllerCrewPositionStatics
{
	void IssueMoveToLocationForSquadUnit(ASquadUnit* SquadUnit, const FVector& TargetLocation,
		const TSubclassOf<UNavigationQueryFilter> QueryFilter)
	{
		static_cast<void>(QueryFilter);

		if (not IsValid(SquadUnit))
		{
			return;
		}

		SquadUnit->ExecuteMoveToSelfPathFinding(TargetLocation, EAbilityID::IdMove, true);
	}
}

ATeamWeaponController::ATeamWeaponController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATeamWeaponController::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitAnimatedTextWidgetPoolManager();
	SetTeamWeaponState(ETeamWeaponState::Spawning);
}

void ATeamWeaponController::OnAllSquadUnitsLoaded()
{
	Super::OnAllSquadUnitsLoaded();
	SpawnTeamWeapon();
}

void ATeamWeaponController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestorePushedMoveSpeedOverride();

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->AbortPushedFollowPath(TEXT("Team weapon controller ending play"));
		M_TeamWeaponMover->StopLegacyFollowCrew();
	}
	bM_IsShuttingDown = true;

	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_DeployTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void ATeamWeaponController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickRotationRequest(DeltaSeconds);
}

void ATeamWeaponController::ExecuteMoveCommand(const FVector MoveToLocation)
{
	if (bM_IsTeamWeaponAbandoned)
	{
		Super::ExecuteMoveCommand(MoveToLocation);
		return;
	}

	if (not GetIsValidTeamWeapon() || not GetIsValidTeamWeaponMover())
	{
		Super::ExecuteMoveCommand(MoveToLocation);
		return;
	}

	SetPostDeployPackActionForMove(MoveToLocation);

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed || M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		StartPacking();
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing)
	{
		return;
	}

	TryIssuePostDeployPackAction();
}

void ATeamWeaponController::TerminateMoveCommand()
{
	Super::TerminateMoveCommand();
	RestorePushedMoveSpeedOverride();

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->AbortPushedFollowPath(TEXT("Move command terminated"));
		M_TeamWeaponMover->StopLegacyFollowCrew();
	}
}

void ATeamWeaponController::ExecuteAttackCommand(AActor* TargetActor)
{
	if (not IsValid(TargetActor))
	{
		DoneExecutingCommand(EAbilityID::IdAttack);
		return;
	}

	M_SpecificEngageTarget = TargetActor;
	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->SetSpecificEngageTarget(TargetActor);
	}

	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		SquadUnit->TerminateAttackCommand();
	}

	M_UnitsCompletedCommand = 0;
}

void ATeamWeaponController::TerminateAttackCommand()
{
	M_SpecificEngageTarget.Reset();
	bM_HasGuardEngageFlowTargetLocation = false;

	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		if (not GetIsCrewOperator(SquadUnit))
		{
			continue;
		}

		SquadUnit->TerminateAttackCommand();
	}

	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->SetSpecificEngageTarget(nullptr);
	}
}

void ATeamWeaponController::ExecutePatrolCommand(const FVector PatrolToLocation)
{
	// We do not support patrol with team weapons.
	DoneExecutingCommand(EAbilityID::IdPatrol);
}

void ATeamWeaponController::ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand)
{
	if (bM_IsTeamWeaponAbandoned)
	{
		if (IsQueueCommand)
		{
			DoneExecutingCommand(EAbilityID::IdRotateTowards);
		}
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		if (IsQueueCommand)
		{
			DoneExecutingCommand(EAbilityID::IdRotateTowards);
		}
		return;
	}

	SetPostDeployPackActionForRotate(RotateToRotator, EAbilityID::IdRotateTowards, IsQueueCommand);

	if (M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		if (UWorld* World = GetWorld(); IsValid(World))
		{
			World->GetTimerManager().ClearTimer(M_DeployTimer);
		}
		M_PostDeployPackAction.Reset();
		SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
		M_TeamWeapon->PlayPackingMontage(false);
		StartRotationRequest(RotateToRotator, IsQueueCommand, EAbilityID::IdRotateTowards);
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed)
	{
		StartPacking();
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing)
	{
		return;
	}

	StartRotationRequest(RotateToRotator, IsQueueCommand, EAbilityID::IdRotateTowards);
}

void ATeamWeaponController::TerminateRotateTowardsCommand()
{
	M_PostDeployPackAction.Reset();
	FinishRotationRequest();
}


bool ATeamWeaponController::RequestInternalRotateTowards(const FRotator& DesiredRotation)
{
	if (not GetIsValidTeamWeapon())
	{
		return false;
	}

	if (GetHasPendingMovePostDeployPackAction())
	{
		return true;
	}

	SetPostDeployPackActionForRotate(DesiredRotation, EAbilityID::IdRotateTowards, false);

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed)
	{
		StartPacking();
		return true;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing || M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		return true;
	}

	StartRotationRequest(DesiredRotation, false, EAbilityID::IdRotateTowards);
	return true;
}

TArray<UWeaponState*> ATeamWeaponController::GetWeaponsOfSquad()
{
	
	TArray<UWeaponState*> Weapons;
	if(GetIsValidTeamWeapon())
	{
		Weapons = M_TeamWeapon->GetWeapons();
	}
	for (const auto EachSquadUnit : GetSquadUnitsChecked())
	{
		if (auto EachWeaponState = EachSquadUnit->GetWeaponState())
		{
			Weapons.Add(EachWeaponState);
		}
	}
	return Weapons;
}

void ATeamWeaponController::UnitInSquadDied(ASquadUnit* UnitDied, bool bUnitSelected,
                                            const ERTSDeathType DeathType)
{
	if (GetIsGameShuttingDown())
	{
		return;
	}

	Super::UnitInSquadDied(UnitDied, bUnitSelected, DeathType);
	AssignCrewToTeamWeapon();
}

void ATeamWeaponController::OnSquadUnitCommandComplete(EAbilityID CompletedAbilityID)
{
	const UCommandData* CommandData = GetIsValidCommandData();
	if (CommandData && CommandData->GetCurrentActiveCommand() == EAbilityID::IdMove)
	{
		return;
	}
	Super::OnSquadUnitCommandComplete(CompletedAbilityID);
}

void ATeamWeaponController::OnUnitIdleAndNoNewCommands()
{
	Super::OnUnitIdleAndNoNewCommands();

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
	}
}

ESquadPathFindingError ATeamWeaponController::GeneratePaths_Assign(const FVector& MoveToLocation,
                                                                   const FNavPathSharedPtr& SquadPath)
{
	static_cast<void>(MoveToLocation);

	const int32 OffsetCount = M_SqPath_Offsets.Num();

	int32 NonCrewCounter = 0;
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		FNavPathSharedPtr UnitPath = MakeShared<FNavigationPath>(*SquadPath);
		const bool bIsCrewOperator = GetIsCrewOperator(SquadUnit);

		const bool bUseCrewPathOffsets =
			M_TeamWeaponState != ETeamWeaponState::Deploying && M_TeamWeaponState != ETeamWeaponState::Ready_Deployed;

		if (bIsCrewOperator && bUseCrewPathOffsets)
		{
			FVector CrewOffset = FVector::ZeroVector;
			if (TryGetCrewMemberOffset(SquadUnit, CrewOffset))
			{
				ApplyCrewOffsetToPath(UnitPath, CrewOffset);
			}
		}
		else if (NonCrewCounter > 0)
		{
			const FVector UnitOffset = (OffsetCount > 0)
				                           ? M_SqPath_Offsets[NonCrewCounter % OffsetCount]
				                           : FVector::ZeroVector;
			ApplyNonCrewOffsetToPath(UnitPath, UnitOffset);
		}

		if (not bIsCrewOperator)
		{
			NonCrewCounter++;
		}

		M_SquadUnitPaths.Add(SquadUnit, UnitPath);
	}

	return ESquadPathFindingError::NoError;
}

void ATeamWeaponController::SpawnTeamWeapon()
{
	if (M_TeamWeapon)
	{
		return;
	}
	if (not TeamWeaponClass)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("TeamWeaponClass"),
			                                                  TEXT("ATeamWeaponController::SpawnTeamWeapon"), this);
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("World not valid in ATeamWeaponController::SpawnTeamWeapon");
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_TeamWeapon = World->SpawnActor<ATeamWeapon>(TeamWeaponClass, GetActorLocation(), GetActorRotation(),
	                                             SpawnParameters);
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	M_TeamWeapon->SetTeamWeaponController(this);
	M_TeamWeapon->SetTurretOwnerActor(this);
	M_TeamWeaponMover = M_TeamWeapon->GetTeamWeaponMover();

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->InitMover(M_TeamWeapon, this);
		M_TeamWeaponMover->OnPushedMoveArrived.AddUObject(this, &ATeamWeaponController::HandlePushedMoverArrived);
		M_TeamWeaponMover->OnPushedMoveFailed.AddUObject(this, &ATeamWeaponController::HandlePushedMoverFailed);
	}

	AssignCrewToTeamWeapon();
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
}

void ATeamWeaponController::AssignCrewToTeamWeapon()
{
	M_CrewAssignment.Reset();
	if (GetIsGameShuttingDown())
	{
		return;
	}

	if (bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	const int32 RequiredOperators = M_TeamWeapon->GetRequiredOperators();
	const TArray<ASquadUnit*> SquadUnits = GetSquadUnitsChecked();
	M_CrewAssignment.M_RequiredOperators = RequiredOperators;

	int32 OperatorIndex = 0;
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		if (OperatorIndex < RequiredOperators)
		{
			M_CrewAssignment.M_Operators.Add(SquadUnit);
			OperatorIndex++;

			AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
			if (not IsValid(InfantryWeapon))
			{
				continue;
			}

			InfantryWeapon->DisableWeaponSearch(false, true);
			continue;
		}

		M_CrewAssignment.M_Guards.Add(SquadUnit);
	}

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->NotifyCrewReady(M_CrewAssignment.GetHasEnoughOperators());
	}

	TryAbandonTeamWeaponForInsufficientCrew();
	if (bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	TryIssuePostDeployPackAction();
}

void ATeamWeaponController::StartPacking()
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing)
	{
		return;
	}

	SetTeamWeaponState(ETeamWeaponState::Packing);
	M_TeamWeapon->PlayPackingMontage(true);
	ShowPackingAnimatedText();

	const float DeploymentTime = M_TeamWeapon->GetDeploymentTime();
	if (DeploymentTime <= 0.0f)
	{
		HandlePackingTimerFinished();
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_DeployTimer);
	World->GetTimerManager().SetTimer(
		M_DeployTimer,
		this,
		&ATeamWeaponController::HandlePackingTimerFinished,
		DeploymentTime,
		false);
}

void ATeamWeaponController::StartDeploying()
{
	StartDeployingTeamWeapon();
}

void ATeamWeaponController::BeginPlay_InitAnimatedTextWidgetPoolManager()
{
	M_AnimatedTextWidgetPoolManager = FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(this);
}

void ATeamWeaponController::StartMoveWithCrew(const FVector MoveToLocation)
{
	if (not GetIsValidTeamWeapon() || not GetIsValidTeamWeaponMover())
	{
		return;
	}

	if (not M_CrewAssignment.GetHasEnoughOperators())
	{
		return;
	}

	UpdateCrewMoveOffsets();
	if (M_CrewMoveOffsets.Num() == 0)
	{
		M_TeamWeaponMover->AbortPushedFollowPath(TEXT("No crew members for movement"));
		return;
	}

	SetTeamWeaponState(ETeamWeaponState::Moving);
	M_TeamWeaponMover->SetCrewMembersToFollow(M_CrewMoveOffsets);
	M_TeamWeaponMover->MoveWeaponToLocation(MoveToLocation);
	GeneralMoveToForAbility(MoveToLocation, EAbilityID::IdMove);
	M_TeamWeaponMover->StartLegacyFollowCrew();
	M_PostDeployPackAction.Reset();
}

void ATeamWeaponController::StartMoveWithPushedWeapon(const FVector MoveToLocation)
{
	if (not GetIsValidTeamWeapon() || not GetIsValidTeamWeaponMover())
	{
		return;
	}

	M_SquadUnitPaths.Empty();
	UpdateCrewMoveOffsets();
	FNavPathSharedPtr SquadPath;
	const ESquadPathFindingError PathError = GenerateBaseSquadPath(MoveToLocation, SquadPath);
	if (PathError != ESquadPathFindingError::NoError)
	{
		RTSFunctionLibrary::ReportError(GetSquadPathFindingErrorMsg(PathError));
		DoneExecutingCommand(EAbilityID::IdMove);
		return;
	}

	const ESquadPathFindingError AssignError = GeneratePaths_Assign(MoveToLocation, SquadPath);
	if (AssignError != ESquadPathFindingError::NoError)
	{
		RTSFunctionLibrary::ReportError(GetSquadPathFindingErrorMsg(AssignError));
		DoneExecutingCommand(EAbilityID::IdMove);
		return;
	}

	ApplyPushedMoveSpeedOverrideToSquad();
	SetTeamWeaponState(ETeamWeaponState::Moving);
	ExecuteSquadMoveAlongAssignedPaths(EAbilityID::IdMove);
	M_TeamWeaponMover->StartPushedFollowPath(SquadPath);
	M_SquadUnitPaths.Empty();
	M_PostDeployPackAction.Reset();
}

void ATeamWeaponController::ExecuteSquadMoveAlongAssignedPaths(const EAbilityID AbilityId) const
{
	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		const FNavPathSharedPtr* UnitPath = M_SquadUnitPaths.Find(SquadUnit);
		if (UnitPath != nullptr && UnitPath->IsValid())
		{
			SquadUnit->ExecuteMoveAlongPath(*UnitPath, AbilityId);
			continue;
		}

		SquadUnit->ExecuteMoveToSelfPathFinding(GetActorLocation(), AbilityId);
	}
}

void ATeamWeaponController::ApplyPushedMoveSpeedOverrideToSquad()
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	const float PushedMoveSpeed = M_TeamWeapon->GetPushedMoveSpeedCmPerSec();
	M_PushedMoveOriginalUnitSpeeds.Empty();

	for (ASquadUnit* SquadUnit : M_TSquadUnits)
	{
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		UCharacterMovementComponent* CharacterMovement = SquadUnit->GetCharacterMovement();
		if (not IsValid(CharacterMovement))
		{
			continue;
		}

		M_PushedMoveOriginalUnitSpeeds.Add(SquadUnit, CharacterMovement->MaxWalkSpeed);
		CharacterMovement->MaxWalkSpeed = PushedMoveSpeed;
	}
}

void ATeamWeaponController::RestorePushedMoveSpeedOverride()
{
	for (const TPair<TObjectPtr<ASquadUnit>, float>& OriginalSpeedPair : M_PushedMoveOriginalUnitSpeeds)
	{
		ASquadUnit* SquadUnit = OriginalSpeedPair.Key;
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		UCharacterMovementComponent* CharacterMovement = SquadUnit->GetCharacterMovement();
		if (not IsValid(CharacterMovement))
		{
			continue;
		}

		CharacterMovement->MaxWalkSpeed = OriginalSpeedPair.Value;
	}

	M_PushedMoveOriginalUnitSpeeds.Empty();
}

void ATeamWeaponController::StartDeployingTeamWeapon()
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	if (M_TeamWeaponState != ETeamWeaponState::Ready_Packed)
	{
		return;
	}

	SetTeamWeaponState(ETeamWeaponState::Deploying);
	M_TeamWeapon->PlayDeployingMontage(true);
	bM_HasIssuedCrewPositionMovesForCurrentDeploy = false;
	ShowDeployingAnimatedText();
	IssueMoveCrewToPositions();

	const float DeploymentTime = M_TeamWeapon->GetDeploymentTime();
	if (DeploymentTime <= 0.0f)
	{
		HandleDeployingTimerFinished();
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_DeployTimer);
	World->GetTimerManager().SetTimer(
		M_DeployTimer,
		this,
		&ATeamWeaponController::HandleDeployingTimerFinished,
		DeploymentTime,
		false);
}

void ATeamWeaponController::HandlePackingTimerFinished()
{
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	TryIssuePostDeployPackAction();
}

void ATeamWeaponController::HandleDeployingTimerFinished()
{
	SetTeamWeaponState(ETeamWeaponState::Ready_Deployed);
	bM_HasIssuedCrewPositionMovesForCurrentDeploy = false;
	IssueMoveCrewToPositions();
	TryIssuePostDeployPackAction();
}

void ATeamWeaponController::SetPostDeployPackActionForMove(const FVector MoveToLocation)
{
	M_PostDeployPackAction.InitForCommand(
		EAbilityID::IdMove,
		true,
		MoveToLocation,
		false,
		FRotator::ZeroRotator,
		nullptr);
}

bool ATeamWeaponController::GetHasPendingMovePostDeployPackAction() const
{
	return M_PostDeployPackAction.GetAbilityId() == EAbilityID::IdMove;
}

void ATeamWeaponController::SetPostDeployPackActionForRotate(const FRotator& DesiredRotation,
                                                             const EAbilityID AbilityId,
                                                             const bool bShouldTriggerDoneExecuting)
{
	M_PostDeployPackAction.InitForCommand(
		AbilityId,
		false,
		FVector::ZeroVector,
		true,
		DesiredRotation,
		nullptr,
		bShouldTriggerDoneExecuting);
}

void ATeamWeaponController::TryIssuePostDeployPackAction()
{
	if (bM_IsTeamWeaponAbandoned)
	{
		M_PostDeployPackAction.Reset();
		return;
	}

	if (not M_PostDeployPackAction.GetHasAction())
	{
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing || M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed)
	{
		StartPacking();
		return;
	}

	if (not GetIsValidTeamWeapon() || not GetIsValidTeamWeaponMover())
	{
		return;
	}

	IssuePostDeployPackAction();
}

void ATeamWeaponController::IssuePostDeployPackAction()
{
	switch (M_PostDeployPackAction.GetAbilityId())
	{
	case EAbilityID::IdMove:
		IssuePostDeployPackAction_Move();
		break;

	case EAbilityID::IdRotateTowards:
		IssuePostDeployPackAction_Rotate();
		break;

	default:
		M_PostDeployPackAction.Reset();
		break;
	}
}

void ATeamWeaponController::IssuePostDeployPackAction_Move()
{
	if (not M_PostDeployPackAction.GetHasLocation())
	{
		M_PostDeployPackAction.Reset();
		return;
	}

	if (not M_CrewAssignment.GetHasEnoughOperators())
	{
		TryAbandonTeamWeaponForInsufficientCrew();
		M_PostDeployPackAction.Reset();
		return;
	}

	if (M_TeamWeapon->GetMovementType() == ETeamWeaponMovementType::PushedWeaponLeads)
	{
		StartMoveWithPushedWeapon(M_PostDeployPackAction.GetTargetLocation());
		return;
	}

	StartMoveWithCrew(M_PostDeployPackAction.GetTargetLocation());
}

void ATeamWeaponController::IssuePostDeployPackAction_Rotate()
{
	if (not M_PostDeployPackAction.GetHasRotation())
	{
		M_PostDeployPackAction.Reset();
		return;
	}

	StartRotationRequest(
		M_PostDeployPackAction.GetTargetRotation(),
		M_PostDeployPackAction.GetShouldTriggerDoneExecuting(),
		EAbilityID::IdRotateTowards);
	M_PostDeployPackAction.Reset();
}

void ATeamWeaponController::UpdateCrewMoveOffsets()
{
	M_CrewMoveOffsets.Reset();

	int32 ValidCrewCount = 0;
	FVector CrewCenter = FVector::ZeroVector;

	for (const TWeakObjectPtr<ASquadUnit>& CrewMember : M_CrewAssignment.M_Operators)
	{
		ASquadUnit* SquadUnit = CrewMember.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		CrewCenter += SquadUnit->GetActorLocation();
		ValidCrewCount++;
	}

	if (ValidCrewCount == 0)
	{
		return;
	}

	CrewCenter /= static_cast<float>(ValidCrewCount);

	for (const TWeakObjectPtr<ASquadUnit>& CrewMember : M_CrewAssignment.M_Operators)
	{
		ASquadUnit* SquadUnit = CrewMember.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		FTeamWeaponCrewMemberOffset CrewOffset;
		CrewOffset.M_CrewMember = SquadUnit;
		CrewOffset.M_OffsetFromCenter = SquadUnit->GetActorLocation() - CrewCenter;
		M_CrewMoveOffsets.Add(CrewOffset);
	}
}

bool ATeamWeaponController::TryGetCrewMemberOffset(const ASquadUnit* SquadUnit, FVector& OutOffset) const
{
	for (const FTeamWeaponCrewMemberOffset& CrewOffset : M_CrewMoveOffsets)
	{
		if (CrewOffset.M_CrewMember.Get() == SquadUnit)
		{
			OutOffset = CrewOffset.M_OffsetFromCenter;
			return true;
		}
	}

	return false;
}

bool ATeamWeaponController::GetIsCrewOperator(const ASquadUnit* SquadUnit) const
{
	for (const TWeakObjectPtr<ASquadUnit>& CrewMember : M_CrewAssignment.M_Operators)
	{
		if (CrewMember.Get() == SquadUnit)
		{
			return true;
		}
	}

	return false;
}

void ATeamWeaponController::ApplyCrewOffsetToPath(FNavPathSharedPtr& UnitPath, const FVector& CrewOffset) const
{
	const float CrewPathProjectionHeight = 200.0f;

	for (int32 i = 0; i < UnitPath->GetPathPoints().Num(); ++i)
	{
		FNavPathPoint& PathPoint = UnitPath->GetPathPoints()[i];
		PathPoint.Location += CrewOffset;
		PathPoint.Location = ProjectLocationOnNavMesh(PathPoint.Location, CrewPathProjectionHeight, false);
	}
}

void ATeamWeaponController::ApplyNonCrewOffsetToPath(FNavPathSharedPtr& UnitPath, const FVector& UnitOffset) const
{
	const float NonCrewMinOffsetScale = 0.8f;
	const float NonCrewMaxOffsetScale = 1.2f;
	const float CrewPathProjectionHeight = 200.0f;

	for (int32 i = 0; i < UnitPath->GetPathPoints().Num(); ++i)
	{
		const bool bLast = (i == UnitPath->GetPathPoints().Num() - 1);
		const FVector PointOffset = bLast
			                            ? GetFinalPathPointOffset(UnitOffset)
			                            : (UnitOffset * FMath::FRandRange(NonCrewMinOffsetScale, NonCrewMaxOffsetScale));
		FNavPathPoint& PathPoint = UnitPath->GetPathPoints()[i];
		PathPoint.Location += PointOffset;

		if (bLast && bM_HasGuardEngageFlowTargetLocation && M_SpecificEngageTarget.IsValid() && GetIsValidTeamWeapon())
		{
			const float GuardEngageFlowDistanceCm = M_TeamWeapon->GetGuardEngageFlowDistanceCm();
			const FVector DirectionToEngageTarget = (M_GuardEngageFlowTargetLocation - PathPoint.Location).GetSafeNormal();
			PathPoint.Location += DirectionToEngageTarget * GuardEngageFlowDistanceCm;
		}

		PathPoint.Location = ProjectLocationOnNavMesh(PathPoint.Location, CrewPathProjectionHeight, false);
	}
}

bool ATeamWeaponController::TryGetCrewPositionsSorted(TArray<UCrewPosition*>& OutCrewPositions) const
{
	OutCrewPositions.Reset();

	if (not GetIsValidTeamWeapon())
	{
		return false;
	}

	M_TeamWeapon->GetCrewPositions(OutCrewPositions);
	if (OutCrewPositions.Num() == 0)
	{
		return false;
	}

	Algo::Sort(OutCrewPositions, [](const UCrewPosition* LeftPosition, const UCrewPosition* RightPosition)
	{
		if (not IsValid(LeftPosition) || not IsValid(RightPosition))
		{
			return IsValid(LeftPosition) && not IsValid(RightPosition);
		}

		const ECrewPositionType LeftType = LeftPosition->GetCrewPositionType();
		const ECrewPositionType RightType = RightPosition->GetCrewPositionType();
		const bool bLeftIsNone = LeftType == ECrewPositionType::None;
		const bool bRightIsNone = RightType == ECrewPositionType::None;

		if (bLeftIsNone != bRightIsNone)
		{
			return not bLeftIsNone;
		}

		if (LeftType != RightType)
		{
			return static_cast<uint8>(LeftType) < static_cast<uint8>(RightType);
		}

		return LeftPosition->GetName() < RightPosition->GetName();
	});

	OutCrewPositions.RemoveAll([](const UCrewPosition* CrewPosition)
	{
		return not IsValid(CrewPosition);
	});

	return OutCrewPositions.Num() > 0;
}

void ATeamWeaponController::IssueMoveCrewToPositions()
{
	if (bM_HasIssuedCrewPositionMovesForCurrentDeploy)
	{
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	if (not M_CrewAssignment.GetHasEnoughOperators())
	{
		return;
	}

	TArray<UCrewPosition*> CrewPositions;
	if (not TryGetCrewPositionsSorted(CrewPositions))
	{
		return;
	}

	const int32 CrewMoveOrderCount = FMath::Min(M_CrewAssignment.M_Operators.Num(), CrewPositions.Num());
	if (CrewMoveOrderCount <= 0)
	{
		return;
	}

	const TSubclassOf<UNavigationQueryFilter> TeamWeaponQueryFilter = nullptr;

	for (int32 OperatorIndex = 0; OperatorIndex < CrewMoveOrderCount; ++OperatorIndex)
	{
		ASquadUnit* SquadUnit = M_CrewAssignment.M_Operators[OperatorIndex].Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		UCrewPosition* CrewPosition = CrewPositions[OperatorIndex];
		if (not IsValid(CrewPosition))
		{
			continue;
		}

		const FVector TargetLocation = CrewPosition->GetCrewWorldTransform().GetLocation();
		TeamWeaponControllerCrewPositionStatics::IssueMoveToLocationForSquadUnit(
			SquadUnit,
			TargetLocation,
			TeamWeaponQueryFilter);
	}

	bM_HasIssuedCrewPositionMovesForCurrentDeploy = true;
}

void ATeamWeaponController::ShowDeployingAnimatedText() const
{
	if (not GetIsValidTeamWeapon() || not GetIsValidAnimatedTextWidgetPoolManager())
	{
		return;
	}

	const FTeamWeaponConfig& TeamWeaponConfig = M_TeamWeapon->GetTeamWeaponConfig();
	if (TeamWeaponConfig.M_DeployingAnimatedText.IsEmpty())
	{
		return;
	}

	constexpr bool bAutoWrap = false;
	const float WrapWidth = 300.0f;
	const TEnumAsByte<ETextJustify::Type> Justification = ETextJustify::Center;
	M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
		TeamWeaponConfig.M_DeployingAnimatedText,
		M_TeamWeapon->GetActorLocation(),
		bAutoWrap,
		WrapWidth,
		Justification,
		TeamWeaponConfig.M_AnimatedTextSettings);
}

void ATeamWeaponController::ShowPackingAnimatedText() const
{
	if (not GetIsValidTeamWeapon() || not GetIsValidAnimatedTextWidgetPoolManager())
	{
		return;
	}

	const FTeamWeaponConfig& TeamWeaponConfig = M_TeamWeapon->GetTeamWeaponConfig();
	if (TeamWeaponConfig.M_PackingAnimatedText.IsEmpty())
	{
		return;
	}

	constexpr bool bAutoWrap = false;
	const float WrapWidth = 300.0f;
	const TEnumAsByte<ETextJustify::Type> Justification = ETextJustify::Center;
	M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
		TeamWeaponConfig.M_PackingAnimatedText,
		M_TeamWeapon->GetActorLocation(),
		bAutoWrap,
		WrapWidth,
		Justification,
		TeamWeaponConfig.M_AnimatedTextSettings);
}

void ATeamWeaponController::HandlePushedMoverArrived()
{
	RestorePushedMoveSpeedOverride();
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	M_PostDeployPackAction.Reset();

	if (M_SpecificEngageTarget.IsValid())
	{
		StartDeploying();
		return;
	}

	bM_HasGuardEngageFlowTargetLocation = false;
	DoneExecutingCommand(EAbilityID::IdMove);
}

void ATeamWeaponController::HandlePushedMoverFailed(const FString& FailureReason)
{
	RestorePushedMoveSpeedOverride();
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	M_PostDeployPackAction.Reset();
	bM_HasGuardEngageFlowTargetLocation = false;
	if (GetIsGameShuttingDown())
	{
		DoneExecutingCommand(EAbilityID::IdMove);
		return;
	}

	RTSFunctionLibrary::ReportError("Team weapon mover failed: " + FailureReason);
	DoneExecutingCommand(EAbilityID::IdMove);
}

bool ATeamWeaponController::GetIsGameShuttingDown() const
{
	return bM_IsShuttingDown || IsActorBeingDestroyed();
}

void ATeamWeaponController::SetTeamWeaponState(const ETeamWeaponState NewState)
{
	if (M_TeamWeaponState == NewState)
	{
		return;
	}
	M_TeamWeaponState = NewState;

	if (M_TeamWeapon != nullptr)
	{
		M_TeamWeapon->SetWeaponsEnabledForTeamWeaponState(NewState == ETeamWeaponState::Ready_Deployed);
	}

	if (NewState == ETeamWeaponState::Packing || NewState == ETeamWeaponState::Moving ||
		NewState == ETeamWeaponState::Ready_Packed)
	{
		bM_HasIssuedCrewPositionMovesForCurrentDeploy = false;
	}
}

bool ATeamWeaponController::GetIsValidTeamWeapon() const
{
	if (bM_IsTeamWeaponAbandoned || GetIsGameShuttingDown())
	{
		return false;
	}

	if (IsValid(M_TeamWeapon))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeapon",
	                                                      "ATeamWeaponController::GetIsValidTeamWeapon", this);
	return false;
}

bool ATeamWeaponController::GetIsValidTeamWeaponMover() const
{
	if (bM_IsTeamWeaponAbandoned || GetIsGameShuttingDown())
	{
		return false;
	}

	if (M_TeamWeaponMover.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeaponMover",
	                                                      "ATeamWeaponController::GetIsValidTeamWeaponMover", this);
	return false;
}

bool ATeamWeaponController::GetIsValidAnimatedTextWidgetPoolManager() const
{
	if (GetIsGameShuttingDown())
	{
		return false;
	}

	if (M_AnimatedTextWidgetPoolManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_AnimatedTextWidgetPoolManager",
	                                                      "ATeamWeaponController::GetIsValidAnimatedTextWidgetPoolManager",
	                                                      this);
	return false;
}

int ATeamWeaponController::GetOwningPlayer()
{
	if (GetIsValidRTSComponent())
	{
		return RTSComponent->GetOwningPlayer();
	}
	return -1;
}

void ATeamWeaponController::OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret)
{
	if (bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	AActor* SpecificEngageTarget = M_SpecificEngageTarget.Get();
	if (SpecificEngageTarget == nullptr && IsValid(CallingTurret))
	{
		SpecificEngageTarget = CallingTurret->GetCurrentTargetActor();
	}

	if (SpecificEngageTarget == nullptr)
	{
		return;
	}

	M_SpecificEngageTarget = SpecificEngageTarget;
	M_TeamWeapon->SetSpecificEngageTarget(SpecificEngageTarget);

	const FVector MoveLocation = GetMoveLocationWithinTurretRange(TargetLocation, CallingTurret);
	M_GuardEngageFlowTargetLocation = TargetLocation;
	bM_HasGuardEngageFlowTargetLocation = true;
	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed || M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		SetPostDeployPackActionForMove(MoveLocation);
		StartPacking();
		return;
	}

	ExecuteMoveCommand(MoveLocation);
}

void ATeamWeaponController::OnTurretInRange(ACPPTurretsMaster* CallingTurret)
{
	static_cast<void>(CallingTurret);

	if (bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
	}

	if (AActor* SpecificTarget = M_SpecificEngageTarget.Get())
	{
		M_TeamWeapon->SetSpecificEngageTarget(SpecificTarget);
		return;
	}

	M_TeamWeapon->SetWeaponsEnabledForTeamWeaponState(true);
}

void ATeamWeaponController::OnMountedWeaponTargetDestroyed(
	ACPPTurretsMaster* CallingTurret,
	UHullWeaponComponent* CallingHullWeapon,
	AActor* DestroyedActor,
	const bool bWasDestroyedByOwnWeapons)
{
}

void ATeamWeaponController::OnFireWeapon(ACPPTurretsMaster* CallingTurret)
{
}

void ATeamWeaponController::OnProjectileHit(const bool bBounced)
{
}

void ATeamWeaponController::StartRotationRequest(const FRotator& DesiredRotation, const bool bShouldTriggerDoneExecuting,
                                                   const EAbilityID CompletionAbilityId)
{
	M_RotationRequest.M_TargetRotation = DesiredRotation;
	M_RotationRequest.bM_IsActive = true;
	M_RotationRequest.bM_ShouldTriggerDoneExecuting = bShouldTriggerDoneExecuting;
	M_RotationRequest.M_CompletionAbilityId = CompletionAbilityId;
	AttachCrewForRotation();
}

void ATeamWeaponController::TickRotationRequest(const float DeltaSeconds)
{
	if (not M_RotationRequest.bM_IsActive)
	{
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		FinishRotationRequest();
		return;
	}

	const float RotationCompletionMarginDegrees = 1.0f;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, M_RotationRequest.M_TargetRotation.Yaw);
	if (FMath::Abs(DeltaYaw) <= RotationCompletionMarginDegrees)
	{
		const float RotationToTargetYaw = DeltaYaw;
		RotateControllerAndTeamWeapon(RotationToTargetYaw);
		FinishRotationRequest();
		return;
	}

	const float TurnSpeedYaw = M_TeamWeapon->GetTurnSpeedYaw();
	float StepYaw = FMath::Sign(DeltaYaw) * TurnSpeedYaw * DeltaSeconds;
	if (FMath::Abs(StepYaw) > FMath::Abs(DeltaYaw))
	{
		StepYaw = DeltaYaw;
	}

	RotateControllerAndTeamWeapon(StepYaw);
	SnapOperatorsToCrewPositionsDuringRotation();
}

void ATeamWeaponController::RotateControllerAndTeamWeapon(const float StepYaw)
{
	const FRotator RotationStep(0.0f, StepYaw, 0.0f);
	const bool bSweep = false;
	AddActorWorldRotation(RotationStep, bSweep, nullptr, ETeleportType::TeleportPhysics);
	M_TeamWeapon->AddActorWorldRotation(RotationStep, bSweep, nullptr, ETeleportType::TeleportPhysics);
}

void ATeamWeaponController::FinishRotationRequest()
{
	const bool bShouldCallDoneExecuting = M_RotationRequest.bM_ShouldTriggerDoneExecuting;
	const EAbilityID CompletionAbilityId = M_RotationRequest.M_CompletionAbilityId;
	SnapOperatorsToCrewPositionsDuringRotation();
	M_RotationRequest.Reset();
	DetachCrewAfterRotation();
	MoveGuardsToRandomGuardPositions();

	if (bShouldCallDoneExecuting && CompletionAbilityId != EAbilityID::IdNoAbility)
	{
		DoneExecutingCommand(CompletionAbilityId);
	}
}

void ATeamWeaponController::AttachCrewForRotation()
{
	if (bM_AreCrewAttachedForRotation)
	{
		return;
	}

	bM_AreCrewAttachedForRotation = true;
}

void ATeamWeaponController::DetachCrewAfterRotation()
{
	if (not bM_AreCrewAttachedForRotation)
	{
		return;
	}

	bM_AreCrewAttachedForRotation = false;
}

void ATeamWeaponController::SnapOperatorsToCrewPositionsDuringRotation()
{
	if (not M_RotationRequest.bM_IsActive)
	{
		return;
	}

	if (not M_CrewAssignment.GetHasEnoughOperators())
	{
		return;
	}

	TArray<UCrewPosition*> CrewPositions;
	if (not TryGetCrewPositionsSorted(CrewPositions))
	{
		return;
	}

	const int32 CrewSnapCount = FMath::Min(M_CrewAssignment.M_Operators.Num(), CrewPositions.Num());
	for (int32 OperatorIndex = 0; OperatorIndex < CrewSnapCount; ++OperatorIndex)
	{
		ASquadUnit* SquadUnit = M_CrewAssignment.M_Operators[OperatorIndex].Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		UCrewPosition* CrewPosition = CrewPositions[OperatorIndex];
		if (not IsValid(CrewPosition))
		{
			continue;
		}

		FVector OperatorTeleportLocation = CrewPosition->GetComponentLocation();
		if (not TryGetLandscapeTeleportLocationForCrewPosition(SquadUnit, OperatorTeleportLocation, OperatorTeleportLocation))
		{
			continue;
		}

		SquadUnit->SetActorLocationAndRotation(
			OperatorTeleportLocation,
			CrewPosition->GetComponentRotation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
}

bool ATeamWeaponController::TryGetLandscapeTeleportLocationForCrewPosition(const ASquadUnit* SquadUnit,
	const FVector& CrewPositionLocation,
	FVector& OutTeleportLocation) const
{
	OutTeleportLocation = CrewPositionLocation;

	if (not IsValid(SquadUnit))
	{
		return false;
	}

	if (GetIsGameShuttingDown())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("World not valid in TryGetLandscapeTeleportLocationForCrewPosition");
		return false;
	}

	constexpr float TraceStartHeightCm = 500.0f;
	constexpr float TraceLengthCm = 2000.0f;
	constexpr float LandscapeClearanceCm = 2.0f;
	const FVector TraceStart = CrewPositionLocation + FVector::UpVector * TraceStartHeightCm;
	const FVector TraceEnd = TraceStart - FVector::UpVector * TraceLengthCm;

	FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(TeamWeaponCrewLandscapeSnap), false, this);
	FHitResult LandscapeHitResult;
	const bool bDidHitLandscape = World->LineTraceSingleByChannel(
		LandscapeHitResult,
		TraceStart,
		TraceEnd,
		COLLISION_TRACE_LANDSCAPE,
		CollisionQueryParams);
	if (not bDidHitLandscape)
	{
		return false;
	}

	const UCapsuleComponent* SquadCapsuleComponent = SquadUnit->GetCapsuleComponent();
	if (not IsValid(SquadCapsuleComponent))
	{
		return false;
	}

	const float ScaledCapsuleHalfHeight = SquadCapsuleComponent->GetScaledCapsuleHalfHeight();
	OutTeleportLocation.Z = LandscapeHitResult.ImpactPoint.Z + ScaledCapsuleHalfHeight + LandscapeClearanceCm;
	return true;
}

void ATeamWeaponController::MoveGuardsToRandomGuardPositions() const
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	for (const TWeakObjectPtr<ASquadUnit>& GuardUnit : M_CrewAssignment.M_Guards)
	{
		ASquadUnit* SquadUnit = GuardUnit.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		FVector GuardLocation;
		if (not TryGetRandomGuardLocation(GuardLocation))
		{
			continue;
		}

		TeamWeaponControllerCrewPositionStatics::IssueMoveToLocationForSquadUnit(
			SquadUnit,
			GuardLocation,
			nullptr);
	}
}

bool ATeamWeaponController::TryGetRandomGuardLocation(FVector& OutGuardLocation) const
{
	OutGuardLocation = FVector::ZeroVector;

	if (not GetIsValidTeamWeapon())
	{
		return false;
	}

	const float ClampedGuardDistance = FMath::Max(M_GuardDistance, 0.0f);
	const float MinGuardAngle = FMath::Min(M_GuardMinMaxAngle.X, M_GuardMinMaxAngle.Y);
	const float MaxGuardAngle = FMath::Max(M_GuardMinMaxAngle.X, M_GuardMinMaxAngle.Y);
	const float RandomGuardAngle = FMath::FRandRange(MinGuardAngle, MaxGuardAngle);

	const FVector BehindDirection = (-M_TeamWeapon->GetActorForwardVector()).GetSafeNormal();
	const FVector GuardDirection = FRotator(0.0f, RandomGuardAngle, 0.0f).RotateVector(BehindDirection).GetSafeNormal();
	const FVector UnprojectedGuardLocation = M_TeamWeapon->GetActorLocation() + GuardDirection * ClampedGuardDistance;
	const float GuardProjectionHeight = 300.0f;
	OutGuardLocation = ProjectLocationOnNavMesh(UnprojectedGuardLocation, GuardProjectionHeight, false);

	return true;
}

FVector ATeamWeaponController::GetMoveLocationWithinTurretRange(const FVector& TargetLocation,
                                                                const ACPPTurretsMaster* CallingTurret) const
{
	if (bM_IsTeamWeaponAbandoned)
	{
		return TargetLocation;
	}

	if (not IsValid(CallingTurret))
	{
		return TargetLocation;
	}

	const float RangeSafetyMargin = 0.9f;
	const float MaxRange = CallingTurret->GetMaxWeaponRange();
	if (MaxRange <= 0.0f)
	{
		return TargetLocation;
	}

	const FVector TeamWeaponLocation = GetActorLocation();
	FVector DirectionToTarget = TargetLocation - TeamWeaponLocation;
	if (DirectionToTarget.IsNearlyZero())
	{
		return TeamWeaponLocation;
	}

	DirectionToTarget.Normalize();
	return TargetLocation - DirectionToTarget * (MaxRange * RangeSafetyMargin);
}

FRotator ATeamWeaponController::GetOwnerRotation() const
{
	return GetActorRotation();
}

void ATeamWeaponController::TryAbandonTeamWeaponForInsufficientCrew()
{
	if (GetIsGameShuttingDown())
	{
		return;
	}

	if (bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	if (M_CrewAssignment.GetHasEnoughOperators())
	{
		return;
	}

	AbandonTeamWeapon();
}

void ATeamWeaponController::AbandonTeamWeapon()
{
	if (GetIsGameShuttingDown())
	{
		return;
	}

	SetTeamWeaponState(ETeamWeaponState::Abandoned);
	M_PostDeployPackAction.Reset();
	FinishRotationRequest();

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->AbortPushedFollowPath(TEXT("Team weapon abandoned due to insufficient operators"));
		M_TeamWeaponMover->StopLegacyFollowCrew();
	}

	RestorePushedMoveSpeedOverride();

	for (const TWeakObjectPtr<ASquadUnit>& CrewOperator : M_CrewAssignment.M_Operators)
	{
		ASquadUnit* SquadUnit = CrewOperator.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
		if (not IsValid(InfantryWeapon))
		{
			continue;
		}

		InfantryWeapon->DisableWeaponSearch(true, false);
	}

	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->Destroy();
	}

	M_TeamWeapon = nullptr;
	M_TeamWeaponMover = nullptr;
	bM_IsTeamWeaponAbandoned = true;
}
