// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponController.h"

#include <RTS_Survival/RTSComponents/TowMechanic/VehicleTow/VehicleTow.h>

#include "TeamWeapon.h"
#include "TeamWeaponMover.h"
#include "CrewPositions/CrewPosition.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "Algo/Sort.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "DigIn/TeamWeaponDigInComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/TowMechanic/TowedActor/TowedActor.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"

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

namespace TeamWeaponGuardStatics
{
	// A guard closer than this stands on the gun; one beyond GuardDistance times the factor has lost it.
	constexpr float MinGuardDistanceToWeaponCm = 150.f;
	constexpr float MaxGuardDistanceFactor = 2.5f;
	// A guard this close to a crew position stands in an operator's way.
	constexpr float MinGuardDistanceToCrewPositionCm = 120.f;
	// Walking guards spread over this arc either side of straight behind the gun, a little closer than deployed.
	constexpr float GuardTravelArcHalfDegrees = 55.f;
	constexpr float GuardTravelDistanceFraction = 0.8f;
	constexpr float GuardPlacementCheckIntervalSeconds = 1.5f;
	constexpr float DetachNavProjectionRadiusCm = 500.f;
	// A dug-in guard further than this from its post inside the pit walks back to it.
	constexpr float MaxDugInGuardPostDistanceCm = 80.f;
	// A guard this far from any navmesh stands inside a wall run; it is set on the nearest walkable spot.
	constexpr float StrandedGuardCheckRadiusCm = 40.f;
	constexpr float StrandedGuardSearchRadiusCm = 300.f;
	constexpr float StrandedGuardProjectionHeightCm = 250.f;
	constexpr float GuardEngageFlowProjectionHeightCm = 300.f;
}

namespace TeamWeaponCrewAnimationStatics
{
	// Crew animations follow the first weapon only; team weapons are assumed to carry a single weapon.
	constexpr int32 PrimaryWeaponIndex = 0;
	// Below this planar speed an idle operator counts as settled; braking after a crew move ends well under this.
	// Kept below the anim instance drop speed so arming and dropping can never flap on a braking unit.
	constexpr float SettledSpeedThresholdCmPerSec = 30.0f;
	// Crew moves allow partial paths, so operators may legally stop short of their spot and are snapped when armed.
	// Beyond this distance the unit is considered stuck and is left alone rather than teleported across the map.
	constexpr float MaxSnapToCrewPositionDistanceCm = 600.0f;

	void PrintCrewAnimDebug(const AActor* Operator, const FString& Message)
	{
		if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
		{
			if (not IsValid(Operator))
			{
				return;
			}

			RTSFunctionLibrary::PrintString(
				Operator->GetActorLocation(),
				Operator,
				Message,
				FColor::Cyan);
		}
	}

	USquadUnitAnimInstance* GetOperatorAnimInstanceNoReport(const FTeamWeaponCrewAnimationSlot& Slot)
	{
		const ASquadUnit* Operator = Slot.M_Operator.Get();
		if (not IsValid(Operator))
		{
			return nullptr;
		}
		USquadUnitAnimInstance* AnimInstance = Operator->GetAnimBP_SquadUnit();
		if (not IsValid(AnimInstance))
		{
			return nullptr;
		}
		return AnimInstance;
	}

	bool GetIsSameCrewAnimationAssignment(const FTeamWeaponCrewAnimationSlot& LeftSlot,
	                                      const FTeamWeaponCrewAnimationSlot& RightSlot)
	{
		return LeftSlot.M_Operator == RightSlot.M_Operator &&
			LeftSlot.M_CrewPosition == RightSlot.M_CrewPosition &&
			LeftSlot.M_CrewRole == RightSlot.M_CrewRole;
	}
}

const FString ATeamWeaponController::M_AbandoningAnimatedText = TEXT("Abandoning!");

ATeamWeaponController::ATeamWeaponController()
{
	PrimaryActorTick.bCanEverTick = true;
}


void ATeamWeaponController::BeginPlay()
{
	// Make sure widget pool manager ref is set prior to spawning.
	BeginPlay_InitAnimatedTextWidgetPoolManager();
	Super::BeginPlay();
	SetTeamWeaponState(ETeamWeaponState::Spawning);
}

void ATeamWeaponController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_TowedActorComponent = GetControlledTeamWeaponTowedActorComponentNoReport();
}

void ATeamWeaponController::OnAllSquadUnitsLoaded()
{
	Super::OnAllSquadUnitsLoaded();
	SpawnTeamWeapon();
	SnapOperatorsToCrewPositions();
	StopOperatorWalksAfterSpawnSnap();
}

void ATeamWeaponController::StopOperatorWalksAfterSpawnSnap()
{
	// Deploying already sent the operators walking from where they spawned; the snap put them on their posts, and
	// the old path would walk them off again into the guards.
	for (const TWeakObjectPtr<ASquadUnit>& Operator : M_CrewAssignment.M_Operators)
	{
		ASquadUnit* SquadUnit = Operator.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}
		SquadUnit->TerminateMovementCommand();
	}
	MoveGuardsToGuardPositionsIfMisplaced();
}

bool ATeamWeaponController::TryLoadSquadUnitsFromMapSetup()
{
	return TrySetupSquadUnitsAndTeamWeaponFromMapSetup();
}

void ATeamWeaponController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisarmAllCrewAnimations();
	RestorePushedMoveSpeedOverride();
	RemoveControlledTeamWeaponDestroyedCallback();

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

	ResetTowRelationshipOnTeamWeaponSide(true, true);

	Super::EndPlay(EndPlayReason);
}

void ATeamWeaponController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UCommandData* CommandData = GetIsValidCommandData();
	if (CommandData != nullptr && CommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack &&
		not M_SpecificEngageTarget.IsValid())
	{
		HandleMainAttackTargetBecameInvalid(TEXT("Main target invalid during tick"));
	}

	TickRotationRequest(DeltaSeconds);
	TickPushedCrewFormation(DeltaSeconds);
	TickTowedWeaponAnimation(DeltaSeconds);
	TickCrewAnimationArming();
	TickGuardPlacement(DeltaSeconds);
}

void ATeamWeaponController::UpdateControllerPositionToAverage()
{
	if (bM_IsTeamWeaponAbandoned)
	{
		Super::UpdateControllerPositionToAverage();
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	SetActorLocation(M_TeamWeapon->GetActorLocation());
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

	SetPostPackActionForMove(MoveToLocation);
	M_PostDeployAction.Reset();

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed || M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		StartPacking();
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing)
	{
		return;
	}

	TryIssuePostPackAction();
}

void ATeamWeaponController::TerminateMoveCommand()
{
	Super::TerminateMoveCommand();
	CancelInternalOutOfRangeMove();
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

	if (GetShouldIgnoreAttackExecuteBecauseAttackAlreadyActive(TargetActor))
	{
		return;
	}

	if (bM_IsTeamWeaponAbandoned)
	{
		Super::ExecuteAttackCommand(TargetActor);
		return;
	}

	M_SpecificEngageTarget = TargetActor;
	TerminateAttackGroundForGuards();
	SetPostDeployActionForAttack(TargetActor);
	ApplyCrewRoleWeaponRestrictions();

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
		return;
	}

	TryIssuePostDeployAction();

	M_UnitsCompletedCommand = 0;
}

void ATeamWeaponController::TerminateAttackCommand()
{
	CancelInternalOutOfRangeMove();
	M_PostPackAction.Reset();
	M_SpecificEngageTarget.Reset();
	M_PostDeployAction.Reset();
	bM_HasGuardEngageFlowTargetLocation = false;
	bM_HasCachedOutOfRangeMoveLocation = false;
	TerminateSpecificEngageForGuards();
	ApplyCrewRoleWeaponRestrictions();

	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->SetSpecificEngageTarget(nullptr);
	}
}

void ATeamWeaponController::ExecuteAttackGroundCommand(const FVector GroundLocation)
{
	if (GroundLocation.IsNearlyZero())
	{
		DoneExecutingCommand(EAbilityID::IdAttackGround);
		return;
	}

	if (bM_IsTeamWeaponAbandoned)
	{
		Super::ExecuteAttackGroundCommand(GroundLocation);
		return;
	}

	M_SpecificEngageTarget.Reset();
	TerminateSpecificEngageForGuards();
	M_ActiveAttackGroundLocation = GroundLocation;
	SetPostDeployActionForAttackGround(GroundLocation);
	ApplyCrewRoleWeaponRestrictions();

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
		return;
	}

	TryIssuePostDeployAction();
	M_UnitsCompletedCommand = 0;
}

void ATeamWeaponController::TerminateAttackGroundCommand()
{
	M_PostDeployAction.Reset();
	TerminateAttackGroundForGuards();
	ApplyCrewRoleWeaponRestrictions();

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	M_TeamWeapon->SetAutoEngageTargets(false);
}

void ATeamWeaponController::ExecuteDigIn()
{
	Super::ExecuteDigIn();

	if (not GetIsValidTeamWeaponDigIn())
	{
		DoneExecutingCommand(EAbilityID::IdDigIn);
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed)
	{
		StartTeamWeaponDigIn();
		return;
	}

	bM_ShouldExecuteDigInAfterDeploy = true;
	M_PostPackAction.Reset();
	M_PostDeployAction.Reset();

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
	}
}

void ATeamWeaponController::TerminateDigIn()
{
	Super::TerminateDigIn();
}

void ATeamWeaponController::ExecuteBreakCover()
{
	Super::ExecuteBreakCover();
	bM_ShouldExecuteDigInAfterDeploy = false;
	RemoveTeamWeaponDigIn();
	OnBreakCoverCompleted();
}

void ATeamWeaponController::TerminateBreakCover()
{
	Super::TerminateBreakCover();
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

	SetPostPackActionForRotate(RotateToRotator, EAbilityID::IdRotateTowards, IsQueueCommand, false);

	if (M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		if (UWorld* World = GetWorld(); IsValid(World))
		{
			World->GetTimerManager().ClearTimer(M_DeployTimer);
		}
		M_PostPackAction.Reset();
		M_PostDeployAction.Reset();
		SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
		M_TeamWeapon->PlayPackingMontage(false);
		StartRotationRequest(RotateToRotator, IsQueueCommand, EAbilityID::IdRotateTowards, false);
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

	StartRotationRequest(RotateToRotator, IsQueueCommand, EAbilityID::IdRotateTowards, false);
}

void ATeamWeaponController::TerminateRotateTowardsCommand()
{
	M_PostPackAction.Reset();
	M_PostDeployAction.Reset();
	CancelInternalOutOfRangeMove();
	bM_HasCachedOutOfRangeMoveLocation = false;
	FinishRotationRequest();
}


bool ATeamWeaponController::RequestInternalRotateTowards(const FRotator& DesiredRotation)
{
	if (not GetIsValidTeamWeapon())
	{
		return false;
	}

	if (GetHasPendingMovePostPackAction())
	{
		return true;
	}

	UCommandData* CommandData = GetIsValidCommandData();
	const EAbilityID ActiveCommandType = CommandData != nullptr
		                                     ? CommandData->GetCurrentlyActiveCommandType()
		                                     : EAbilityID::IdNoAbility;
	const bool bHasActiveAttackCommand = ActiveCommandType == EAbilityID::IdAttack;

	if (ActiveCommandType == EAbilityID::IdAttackGround)
	{
		// Attack-ground has no target actor; resume on the ground location once redeployed facing it.
		SetPostPackActionForRotate(DesiredRotation, EAbilityID::IdRotateTowards, false, true);
		SetPostDeployActionForAttackGround(M_ActiveAttackGroundLocation);
		return StartInternalRotateFromCurrentState(DesiredRotation);
	}

	AActor* TargetActorForPostDeploy = nullptr;
	if (bHasActiveAttackCommand)
	{
		TargetActorForPostDeploy = M_SpecificEngageTarget.Get();
		if (not IsValid(TargetActorForPostDeploy))
		{
			HandleMainAttackTargetBecameInvalid(TEXT("Main target invalid while queuing internal rotation"));
			return false;
		}
	}
	else
	{
		TargetActorForPostDeploy = M_TeamWeapon->GetSpecificEngageTarget();
	}

	SetPostPackActionForRotate(DesiredRotation, EAbilityID::IdRotateTowards, false, true);
	SetPostDeployActionForAttack(TargetActorForPostDeploy);
	return StartInternalRotateFromCurrentState(DesiredRotation);
}

bool ATeamWeaponController::StartInternalRotateFromCurrentState(const FRotator& DesiredRotation)
{
	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed)
	{
		StartPacking();
		return true;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing || M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		return true;
	}

	StartRotationRequest(DesiredRotation, false, EAbilityID::IdRotateTowards, true);
	return true;
}

TArray<UWeaponState*> ATeamWeaponController::GetWeaponsOfSquad()
{
	TArray<UWeaponState*> Weapons;
	if (GetIsValidTeamWeapon())
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

void ATeamWeaponController::OnControlledTeamWeaponDied()
{
	if (GetIsGameShuttingDown() || bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	AbandonTeamWeapon();
	OnControlledTeamWeaponLost.Broadcast(this);
}

float ATeamWeaponController::GetTeamWeaponArc() const
{
	if(not GetIsValidTeamWeapon())
	{
		return 0.0f;
	}
	return M_TeamWeapon->GetTeamWeaponConfig().M_YawArc.M_MaxYaw;
}

float ATeamWeaponController::GetSquadRange()
{
	return FMath::Max(Super::GetSquadRange(), GetTeamWeaponRange());
}

float ATeamWeaponController::GetTeamWeaponRange() const
{
	if(not GetIsValidTeamWeapon())
	{
		return 0.0f;
	}
	return M_TeamWeapon->GetTurretRange();
}

bool ATeamWeaponController::PrepareToAdoptAbandonedTeamWeapon(ATeamWeapon* AbandonedTeamWeapon)
{
	if (not IsValid(AbandonedTeamWeapon))
	{
		RTSFunctionLibrary::ReportError("AbandonedTeamWeapon not valid in ATeamWeaponController::PrepareToAdoptAbandonedTeamWeapon");
		return false;
	}

	if (not AbandonedTeamWeapon->GetIsAbandoned())
	{
		RTSFunctionLibrary::ReportError("Attempted to adopt a team weapon that is not abandoned."
			"\n At function: ATeamWeaponController::PrepareToAdoptAbandonedTeamWeapon");
		return false;
	}

	M_PendingAdoptedTeamWeapon = AbandonedTeamWeapon;
	return true;
}

bool ATeamWeaponController::GetHasControlledTeamWeapon() const
{
	return M_TeamWeapon != nullptr && not bM_IsTeamWeaponAbandoned;
}

ATeamWeapon* ATeamWeaponController::GetControlledTeamWeaponNoReport() const
{
	if (bM_IsTeamWeaponAbandoned || not GetIsValidTeamWeaponNoReport())
	{
		return nullptr;
	}
	return M_TeamWeapon.Get();
}

bool ATeamWeaponController::GetIsValidTeamWeaponNoReport() const
{
	return IsValid(M_TeamWeapon);
}

FString ATeamWeaponController::GetOrderDebugDescription() const
{
	int32 ValidOperatorCount = 0;
	for (const TWeakObjectPtr<ASquadUnit>& Operator : M_CrewAssignment.M_Operators)
	{
		ValidOperatorCount += Operator.IsValid() ? 1 : 0;
	}
	return FString::Printf(
		TEXT("post_pack=%s post_deploy=%s operators=%d/%d valid=%d squad=%d abandoned=%d rotating=%d reposition=%d cached_reposition=%d dig_in_after_deploy=%d dug_in=%d"),
		*UEnum::GetValueAsString(M_PostPackAction.GetAbilityId()), *UEnum::GetValueAsString(M_PostDeployAction.GetAbilityId()),
		M_CrewAssignment.M_Operators.Num(), M_CrewAssignment.M_RequiredOperators, ValidOperatorCount,
		M_TSquadUnits.Num(), bM_IsTeamWeaponAbandoned ? 1 : 0, M_RotationRequest.bM_IsActive ? 1 : 0,
		bM_HasInternalOutOfRangeReposition ? 1 : 0, bM_HasCachedOutOfRangeMoveLocation ? 1 : 0,
		bM_ShouldExecuteDigInAfterDeploy ? 1 : 0, GetIsTeamWeaponDigInActive() ? 1 : 0);
}

void ATeamWeaponController::GetCrewOperators(TArray<ASquadUnit*>& OutOperators) const
{
	OutOperators.Reset();
	for (const TWeakObjectPtr<ASquadUnit>& Operator : M_CrewAssignment.M_Operators)
	{
		OutOperators.Add(Operator.Get());
	}
}

void ATeamWeaponController::GetCrewGuards(TArray<ASquadUnit*>& OutGuards) const
{
	OutGuards.Reset();
	for (const TWeakObjectPtr<ASquadUnit>& Guard : M_CrewAssignment.M_Guards)
	{
		OutGuards.Add(Guard.Get());
	}
}

bool ATeamWeaponController::TryGetOperatorCrewPositionLocation(const int32 OperatorIndex, FVector& OutLocation) const
{
	UCrewPosition* CrewPosition = nullptr;
	if (not TryGetCrewPositionForOperatorIndex(OperatorIndex, CrewPosition))
	{
		return false;
	}
	OutLocation = CrewPosition->GetComponentLocation();
	return true;
}

void ATeamWeaponController::SetTargetPreference(const ETargetPreference TargetPreference)
{
	// Sets on squad unit's weapons.
	Super::SetTargetPreference(TargetPreference);
	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->SetTargetPreference(TargetPreference);
	}
}

UTowedActorComponent* ATeamWeaponController::GetControlledTeamWeaponTowedActorComponentNoReport()
{
	if (not GetHasControlledTeamWeapon())
	{
		M_TowedActorComponent = nullptr;
		return nullptr;
	}

	ATeamWeapon* ControlledTeamWeapon = M_TeamWeapon.Get();
	if (not IsValid(ControlledTeamWeapon))
	{
		M_TowedActorComponent = nullptr;
		return nullptr;
	}

	M_TowedActorComponent = ControlledTeamWeapon->FindComponentByClass<UTowedActorComponent>();
	return M_TowedActorComponent.Get();
}

const UTowedActorComponent* ATeamWeaponController::GetControlledTeamWeaponTowedActorComponentNoReport() const
{
	if (not GetHasControlledTeamWeapon())
	{
		return nullptr;
	}

	const ATeamWeapon* ControlledTeamWeapon = M_TeamWeapon.Get();
	if (not IsValid(ControlledTeamWeapon))
	{
		return nullptr;
	}

	return ControlledTeamWeapon->FindComponentByClass<UTowedActorComponent>();
}

bool ATeamWeaponController::GetSquadAlreadyHasTeamWeapon() const
{
	return GetHasControlledTeamWeapon();
}

bool ATeamWeaponController::TryGetOperatorDeathMontageSubtype(
	const ASquadUnit* SquadUnit,
	ESquadSubtype& OutTeamWeaponSquadSubtype) const
{
	OutTeamWeaponSquadSubtype = ESquadSubtype::Squad_None;
	if (not IsValid(SquadUnit) || not GetIsCrewOperator(SquadUnit))
	{
		return false;
	}

	OutTeamWeaponSquadSubtype = GetTeamWeaponSquadSubtypeForCrewAnimations();
	return true;
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
	if (CompletedAbilityID == EAbilityID::IdMove)
	{
		// The completing unit is not passed along; the settled check finds the operator that just arrived.
		TryArmCrewAnimationsForSettledOperators();
	}

	if (CompletedAbilityID == EAbilityID::IdAttack)
	{
		UCommandData* CommandData = GetIsValidCommandData();
		if (CommandData != nullptr && CommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack &&
			not M_SpecificEngageTarget.IsValid())
		{
			HandleMainAttackTargetBecameInvalid(TEXT("Guard reported target destroyed/invisible"));
		}

		return;
	}

	UCommandData* CommandData = GetIsValidCommandData();
	if (CommandData == nullptr)
	{
		Super::OnSquadUnitCommandComplete(CompletedAbilityID);
		return;
	}

	const bool bIsMoveCompletion = CompletedAbilityID == EAbilityID::IdMove;
	const bool bIsActiveMoveCommand = CommandData->GetCurrentActiveCommand() == EAbilityID::IdMove;
	if (bIsMoveCompletion && bIsActiveMoveCommand && GetIsPushedWeaponLeadsMovementType())
	{
		return;
	}

	Super::OnSquadUnitCommandComplete(CompletedAbilityID);
}

void ATeamWeaponController::OnUnitIdleAndNoNewCommands()
{
	if (M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}

	if (bM_HasInternalOutOfRangeReposition && M_TeamWeaponState == ETeamWeaponState::Moving)
	{
		if (GetIsValidTeamWeaponMover())
		{
			M_TeamWeaponMover->StopLegacyFollowCrew();
		}

		HandleInternalOutOfRangeMoveCompleted(true);
		return;
	}

	Super::OnUnitIdleAndNoNewCommands();
	ApplyCrewRoleWeaponRestrictions();

	if (GetShouldAutoDeployOnIdle())
	{
		StartDeploying();
	}
}

void ATeamWeaponController::OnSquadUnitOutOfRange(
	const FVector& TargetLocation,
	const EAbilityID CombatAbility)
{
	if (bM_IsTeamWeaponAbandoned)
	{
		Super::OnSquadUnitOutOfRange(TargetLocation, CombatAbility);
	}
	// do not move guards or operators; the team weapon dictates the movement!
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
	if (M_TeamWeapon != nullptr)
	{
		return;
	}

	if (bM_ShouldUseMapSetupControllerData && TrySetupTeamWeaponFromMapSetup())
	{
		AssignCrewToTeamWeapon();
		SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
		StartDeploying();
		return;
	}

	ATeamWeapon* PendingAdoptedTeamWeapon = M_PendingAdoptedTeamWeapon.Get();
	if (PendingAdoptedTeamWeapon != nullptr)
	{
		M_PendingAdoptedTeamWeapon = nullptr;
		if (not AdoptAbandonedTeamWeapon(PendingAdoptedTeamWeapon))
		{
			return;
		}

		AssignCrewToTeamWeapon();
		SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
		StartDeploying();
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
	// The squad spawns its controller at whatever height the spawn point had; the gun belongs on the ground.
	M_TeamWeapon->ConformToGround(0.f, true);
	M_TeamWeaponMover = M_TeamWeapon->GetTeamWeaponMover();

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->InitMover(M_TeamWeapon, this);
		M_TeamWeaponMover->OnPushedMoveArrived.RemoveAll(this);
		M_TeamWeaponMover->OnPushedMoveFailed.RemoveAll(this);
		M_TeamWeaponMover->OnPushedMoveArrived.AddUObject(this, &ATeamWeaponController::HandlePushedMoverArrived);
		M_TeamWeaponMover->OnPushedMoveFailed.AddUObject(this, &ATeamWeaponController::HandlePushedMoverFailed);
	}
	RegisterControlledTeamWeaponDestroyedCallback();

	AssignCrewToTeamWeapon();
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	StartDeploying();
}

bool ATeamWeaponController::TrySetupSquadUnitsAndTeamWeaponFromMapSetup()
{
	bM_ShouldUseMapSetupControllerData = false;

	const bool bHasMapSetupSquadUnits = not M_MapSetupSquadUnits.IsEmpty();
	const bool bHasMapSetupTeamWeapon = M_MapSetupTeamWeapon.IsValid();
	if (not bHasMapSetupSquadUnits && not bHasMapSetupTeamWeapon)
	{
		return false;
	}

	if (bHasMapSetupSquadUnits != bHasMapSetupTeamWeapon)
	{
		RTSFunctionLibrary::ReportError(
			"MapSetupController requires both M_MapSetupSquadUnits and M_MapSetupTeamWeapon to be set."
			"\n At function: ATeamWeaponController::TrySetupSquadUnitsAndTeamWeaponFromMapSetup");
		return false;
	}

	if (not TrySetupSquadUnitsFromMapSetup())
	{
		return false;
	}

	if (not TrySetupTeamWeaponFromMapSetup())
	{
		return false;
	}

	M_SquadLoadingStatus.bM_HasInitializedData = false;
	M_SquadLoadingStatus.bM_HasFinishedLoading = true;
	M_SquadLoadingStatus.M_AmountOfSquadUnitsToLoad = 0;
	bM_ShouldUseMapSetupControllerData = true;
	OnAllSquadUnitsLoaded();
	return true;
}

bool ATeamWeaponController::TrySetupSquadUnitsFromMapSetup()
{
	M_TSquadUnits.Reset();
	for (const TWeakObjectPtr<ASquadUnit>& SquadUnitWeak : M_MapSetupSquadUnits)
	{
		ASquadUnit* SquadUnit = SquadUnitWeak.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		SquadUnit->SetSquadController(this);
		M_TSquadUnits.Add(SquadUnit);
	}

	if (M_TSquadUnits.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"MapSetupController did not contain any valid squad units."
			"\n At function: ATeamWeaponController::TrySetupSquadUnitsFromMapSetup");
		return false;
	}

	return true;
}

bool ATeamWeaponController::TrySetupTeamWeaponFromMapSetup()
{
	if (not GetIsValidMapSetupTeamWeapon())
	{
		return false;
	}

	if (M_TeamWeapon != nullptr)
	{
		return true;
	}

	M_TeamWeapon = M_MapSetupTeamWeapon.Get();
	M_TeamWeapon->SetTeamWeaponController(this);
	M_TeamWeapon->SetTurretOwnerActor(this);
	M_TeamWeaponMover = M_TeamWeapon->GetTeamWeaponMover();

	if (not GetIsValidTeamWeaponMover())
	{
		M_TeamWeapon = nullptr;
		return false;
	}

	M_TeamWeaponMover->InitMover(M_TeamWeapon, this);
	M_TeamWeaponMover->OnPushedMoveArrived.RemoveAll(this);
	M_TeamWeaponMover->OnPushedMoveFailed.RemoveAll(this);
	M_TeamWeaponMover->OnPushedMoveArrived.AddUObject(this, &ATeamWeaponController::HandlePushedMoverArrived);
	M_TeamWeaponMover->OnPushedMoveFailed.AddUObject(this, &ATeamWeaponController::HandlePushedMoverFailed);
	RegisterControlledTeamWeaponDestroyedCallback();
	return true;
}

bool ATeamWeaponController::GetIsValidMapSetupTeamWeapon() const
{
	if (M_MapSetupTeamWeapon.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_MapSetupTeamWeapon"),
		TEXT("ATeamWeaponController::GetIsValidMapSetupTeamWeapon"),
		this);
	return false;
}

bool ATeamWeaponController::AdoptAbandonedTeamWeapon(ATeamWeapon* AbandonedTeamWeapon)
{
	if (not IsValid(AbandonedTeamWeapon))
	{
		RTSFunctionLibrary::ReportError("AbandonedTeamWeapon not valid in ATeamWeaponController::AdoptAbandonedTeamWeapon");
		return false;
	}

	if (not AbandonedTeamWeapon->GetIsAbandoned())
	{
		RTSFunctionLibrary::ReportError("Attempted to adopt a team weapon that is not abandoned."
			"\n At function: ATeamWeaponController::AdoptAbandonedTeamWeapon");
		return false;
	}

	bM_IsTeamWeaponAbandoned = false;

	const int32 OwningPlayer = GetOwningPlayer();
	if (OwningPlayer <= 0)
	{
		RTSFunctionLibrary::ReportError("Invalid owning player in ATeamWeaponController::AdoptAbandonedTeamWeapon");
		return false;
	}

	ATeamWeaponController* PreviousController = AbandonedTeamWeapon->GetTeamWeaponController();
	M_TeamWeapon = AbandonedTeamWeapon;
	M_TeamWeapon->SetTeamWeaponController(this);
	M_TeamWeapon->SetTurretOwnerActor(this);

	if (not M_TeamWeapon->SetOwningPlayerRuntime(static_cast<uint8>(OwningPlayer)))
	{
		RTSFunctionLibrary::ReportError("Failed to set owning player on team weapon while adopting abandoned weapon."
			"\n At function: ATeamWeaponController::AdoptAbandonedTeamWeapon");
		M_TeamWeapon = nullptr;
		return false;
	}

	M_TeamWeaponMover = M_TeamWeapon->GetTeamWeaponMover();
	if (not GetIsValidTeamWeaponMover())
	{
		M_TeamWeapon = nullptr;
		return false;
	}

	if (IsValid(PreviousController))
	{
		M_TeamWeaponMover->OnPushedMoveArrived.RemoveAll(PreviousController);
		M_TeamWeaponMover->OnPushedMoveFailed.RemoveAll(PreviousController);
	}

	M_TeamWeaponMover->OnPushedMoveArrived.RemoveAll(this);
	M_TeamWeaponMover->OnPushedMoveFailed.RemoveAll(this);
	M_TeamWeaponMover->InitMover(M_TeamWeapon, this);
	M_TeamWeaponMover->OnPushedMoveArrived.AddUObject(this, &ATeamWeaponController::HandlePushedMoverArrived);
	M_TeamWeaponMover->OnPushedMoveFailed.AddUObject(this, &ATeamWeaponController::HandlePushedMoverFailed);
	RegisterControlledTeamWeaponDestroyedCallback();

	M_TeamWeapon->OnTeamWeaponRemanned();
	bM_IsTeamWeaponAbandoned = false;
	return true;
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
		DisarmAllCrewAnimations();
		M_CrewAnimationState.Reset();
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
			continue;
		}

		M_CrewAssignment.M_Guards.Add(SquadUnit);
	}

	RebuildCrewAnimationSlots();
	if (M_PushedCrewFormation.GetIsActive())
	{
		// A soldier died or joined mid-push: the posts are handed out again.
		StartPushedCrewFormation();
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

	ApplyCrewRoleWeaponRestrictions();

	TryIssuePostPackAction();
}

void ATeamWeaponController::StartPacking()
{
	if (M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}

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
	if (M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}

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
	M_PostPackAction.Reset();
}

void ATeamWeaponController::StartMoveWithPushedWeapon(const FVector MoveToLocation)
{
	if (not GetIsValidTeamWeapon() || not GetIsValidTeamWeaponMover())
	{
		return;
	}

	M_SquadUnitPaths.Empty();
	FNavPathSharedPtr SquadPath;
	const ESquadPathFindingError PathError = GenerateBaseSquadPath(MoveToLocation, SquadPath);
	if (PathError != ESquadPathFindingError::NoError)
	{
		RTSFunctionLibrary::ReportError(GetSquadPathFindingErrorMsg(PathError));
		// Ending the command calls the idle handler, which only redeploys a gun with no move still pending.
		M_PostPackAction.Reset();
		DoneExecutingCommand(EAbilityID::IdMove);
		return;
	}

	CancelRotationRequestForMove();
	ApplyPushedMoveSpeedOverrideToSquad();
	SetTeamWeaponState(ETeamWeaponState::Moving);
	// Paths of their own let the crew walk off ahead of the gun; the formation keeps them on their posts instead.
	StartPushedCrewFormation();
	M_TeamWeaponMover->StartPushedFollowPath(SquadPath);
	M_PostPackAction.Reset();
}

void ATeamWeaponController::StartPushedCrewFormation()
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	TArray<FTeamWeaponFormationMember> Members;
	BuildPushedCrewFormationMembers(Members);
	for (const FTeamWeaponFormationMember& Member : Members)
	{
		ASquadUnit* SquadUnit = Member.M_Unit.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}
		// Guards may still chase a target or walk to a guard spot; walking with the gun overrides both.
		SquadUnit->TerminateAttackCommand();
		SquadUnit->TerminateMovementCommand();
	}
	M_PushedCrewFormation.Start(M_TeamWeapon, Members, M_TeamWeapon->GetPushedCrewSettings());
}

void ATeamWeaponController::TickPushedCrewFormation(const float DeltaSeconds)
{
	if (not M_PushedCrewFormation.GetIsActive())
	{
		return;
	}

	const float PushedSpeedScale = M_PushedCrewFormation.Tick(DeltaSeconds);
	if (not GetIsValidTeamWeaponMover())
	{
		return;
	}
	M_TeamWeaponMover->SetPushedSpeedScale(PushedSpeedScale);
}

void ATeamWeaponController::BuildPushedCrewFormationMembers(TArray<FTeamWeaponFormationMember>& OutMembers) const
{
	OutMembers.Reset();
	const int32 OperatorCount = M_CrewAssignment.M_Operators.Num();
	for (int32 OperatorIndex = 0; OperatorIndex < OperatorCount; ++OperatorIndex)
	{
		FTeamWeaponFormationMember Member;
		Member.M_Unit = M_CrewAssignment.M_Operators[OperatorIndex];
		Member.bM_IsOperator = true;
		UCrewPosition* CrewPosition = nullptr;
		if (TryGetCrewPositionForOperatorIndex(OperatorIndex, CrewPosition))
		{
			Member.M_CrewPosition = CrewPosition;
		}
		else
		{
			// More operators than crew positions: they walk among the guards behind the gun.
			Member.M_LocalPostOffset = GetGuardTravelPostOffset(OperatorIndex, OperatorCount);
		}
		OutMembers.Add(Member);
	}

	const int32 GuardCount = M_CrewAssignment.M_Guards.Num();
	for (int32 GuardIndex = 0; GuardIndex < GuardCount; ++GuardIndex)
	{
		FTeamWeaponFormationMember Member;
		Member.M_Unit = M_CrewAssignment.M_Guards[GuardIndex];
		Member.bM_IsOperator = false;
		Member.M_LocalPostOffset = GetGuardTravelPostOffset(GuardIndex, GuardCount);
		OutMembers.Add(Member);
	}
}

FVector ATeamWeaponController::GetGuardTravelPostOffset(const int32 GuardIndex, const int32 GuardCount) const
{
	using namespace TeamWeaponGuardStatics;
	// The deployed guard arc reaches beside and in front of the gun; on the move the guards follow behind it.
	const float SpreadAlpha = (static_cast<float>(GuardIndex) + 0.5f) / FMath::Max(GuardCount, 1);
	const float GuardAngle = FMath::Lerp(-GuardTravelArcHalfDegrees, GuardTravelArcHalfDegrees, SpreadAlpha);
	const FVector BehindDirection = -FVector::ForwardVector;
	return FRotator(0.f, GuardAngle, 0.f).RotateVector(BehindDirection) *
		FMath::Max(M_GuardDistance, 0.f) * GuardTravelDistanceFraction;
}

bool ATeamWeaponController::GetIsGuardMisplaced(const ASquadUnit& Guard) const
{
	using namespace TeamWeaponGuardStatics;
	const FVector GuardLocation = Guard.GetActorLocation();
	const int32 GuardIndex = M_CrewAssignment.M_Guards.IndexOfByPredicate(
		[&Guard](const TWeakObjectPtr<ASquadUnit>& CrewGuard) { return CrewGuard.Get() == &Guard; });
	FVector DugInGuardPost;
	if (TryGetDugInGuardPost(GuardIndex, DugInGuardPost))
	{
		return FVector::Dist2D(GuardLocation, DugInGuardPost) > MaxDugInGuardPostDistanceCm;
	}
	const float DistanceToWeaponCm = FVector::Dist2D(GuardLocation, M_TeamWeapon->GetActorLocation());
	const float MaxGuardDistanceCm = FMath::Max(M_GuardDistance, MinGuardDistanceToWeaponCm) * MaxGuardDistanceFactor;
	if (DistanceToWeaponCm < MinGuardDistanceToWeaponCm || DistanceToWeaponCm > MaxGuardDistanceCm)
	{
		return true;
	}
	for (int32 OperatorIndex = 0; OperatorIndex < M_CrewAssignment.M_Operators.Num(); ++OperatorIndex)
	{
		UCrewPosition* CrewPosition = nullptr;
		if (TryGetCrewPositionForOperatorIndex(OperatorIndex, CrewPosition) &&
			FVector::Dist2D(GuardLocation, CrewPosition->GetComponentLocation()) < MinGuardDistanceToCrewPositionCm)
		{
			return true;
		}
	}
	return false;
}

void ATeamWeaponController::MoveGuardsToGuardPositionsIfMisplaced() const
{
	using namespace TeamWeaponGuardStatics;
	if (M_TeamWeaponState == ETeamWeaponState::Towed || not GetIsValidTeamWeapon())
	{
		return;
	}

	for (int32 GuardIndex = 0; GuardIndex < M_CrewAssignment.M_Guards.Num(); ++GuardIndex)
	{
		ASquadUnit* SquadUnit = M_CrewAssignment.M_Guards[GuardIndex].Get();
		if (not GetIsValidSquadUnit(SquadUnit) || SquadUnit->GetIsPathFollowingActive())
		{
			continue;
		}
		if (not GetIsGuardMisplaced(*SquadUnit))
		{
			continue;
		}
		FVector GuardLocation;
		if (TryGetDugInGuardPost(GuardIndex, GuardLocation))
		{
			PlaceGuardOnNavMeshIfStranded(*SquadUnit);
		}
		else if (not TryGetRandomGuardLocation(GuardLocation))
		{
			continue;
		}
		TeamWeaponControllerCrewPositionStatics::IssueMoveToLocationForSquadUnit(SquadUnit, GuardLocation, nullptr);
	}
}

void ATeamWeaponController::TickGuardPlacement(const float DeltaSeconds)
{
	using namespace TeamWeaponGuardStatics;
	if (M_TeamWeaponState != ETeamWeaponState::Ready_Deployed || bM_IsTeamWeaponAbandoned)
	{
		return;
	}
	M_SecondsUntilGuardPlacementCheck -= DeltaSeconds;
	if (M_SecondsUntilGuardPlacementCheck > 0.f)
	{
		return;
	}
	M_SecondsUntilGuardPlacementCheck = GuardPlacementCheckIntervalSeconds;
	MoveGuardsToGuardPositionsIfMisplaced();
}

void ATeamWeaponController::CancelRotationRequestForMove()
{
	if (not M_RotationRequest.bM_IsActive)
	{
		return;
	}
	// A player rotate order was already ended by the command queue; an internal turn is superseded by the move.
	M_RotationRequest.Reset();
	DetachCrewAfterRotation();
}

void ATeamWeaponController::IssueGuardEngageFlowMoves() const
{
	using namespace TeamWeaponGuardStatics;
	// Guards of a dug-in gun keep their posts inside the pit.
	if (not GetIsValidTeamWeapon() || GetIsTeamWeaponDigInActive())
	{
		return;
	}

	const float GuardEngageFlowDistanceCm = M_TeamWeapon->GetGuardEngageFlowDistanceCm();
	for (const TWeakObjectPtr<ASquadUnit>& GuardUnit : M_CrewAssignment.M_Guards)
	{
		ASquadUnit* SquadUnit = GuardUnit.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}
		const FVector DirectionToEngageTarget = (M_GuardEngageFlowTargetLocation - SquadUnit->GetActorLocation()).
			GetSafeNormal2D();
		const FVector FlowLocation = ProjectLocationOnNavMesh(
			SquadUnit->GetActorLocation() + DirectionToEngageTarget * GuardEngageFlowDistanceCm,
			GuardEngageFlowProjectionHeightCm, false);
		TeamWeaponControllerCrewPositionStatics::IssueMoveToLocationForSquadUnit(SquadUnit, FlowLocation, nullptr);
	}
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

	// The crew walks a little faster than the gun so it can always catch up with its posts.
	const float PushedMoveSpeed = M_TeamWeapon->GetPushedMoveSpeedCmPerSec() *
		M_TeamWeapon->GetPushedCrewSettings().CatchUpSpeedMultiplier;
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
	// The tilt eases while the gun is pushed; deployed it must rest on the ground exactly.
	M_TeamWeapon->ConformToGround(0.f, true);
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
	if (M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	if (bM_ShouldExecuteDigInAfterDeploy)
	{
		StartDeploying();
		return;
	}

	TryIssuePostPackAction();
}

void ATeamWeaponController::HandleDeployingTimerFinished()
{
	// A truck hooked the gun while it was deploying; it stays packed on the truck.
	if (M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}
	SetTeamWeaponState(ETeamWeaponState::Ready_Deployed);
	bM_HasIssuedCrewPositionMovesForCurrentDeploy = false;
	IssueMoveCrewToPositions();
	MoveGuardsToGuardPositionsIfMisplaced();

	if (bM_ShouldExecuteDigInAfterDeploy)
	{
		bM_ShouldExecuteDigInAfterDeploy = false;
		StartTeamWeaponDigIn();
		return;
	}

	TryIssuePostDeployAction();
}

void ATeamWeaponController::SetPostPackActionForMove(const FVector MoveToLocation)
{
	M_PostPackAction.InitForCommand(
		EAbilityID::IdMove,
		true,
		MoveToLocation,
		false,
		FRotator::ZeroRotator,
		nullptr);
}

void ATeamWeaponController::StartInternalOutOfRangeReposition(const FVector MoveToLocation, AActor* TargetActor)
{
	if (not IsValid(TargetActor))
	{
		return;
	}

	bM_HasInternalOutOfRangeReposition = true;
	M_SpecificEngageTarget = TargetActor;
	SetPostPackActionForMove(MoveToLocation);
	SetPostDeployActionForAttack(TargetActor);

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed || M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		StartPacking();
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing)
	{
		return;
	}

	TryIssuePostPackAction();
}

void ATeamWeaponController::HandleInternalOutOfRangeMoveCompleted(const bool bMoveSucceeded)
{
	if (not bM_HasInternalOutOfRangeReposition)
	{
		return;
	}

	bM_HasInternalOutOfRangeReposition = false;

	AActor* TargetActor = M_SpecificEngageTarget.Get();
	if (not bMoveSucceeded || not IsValid(TargetActor))
	{
		HandleMainAttackTargetBecameInvalid(TEXT("Internal out-of-range move could not continue attack"));
		return;
	}

	SetPostDeployActionForAttack(TargetActor);

	if (M_TeamWeaponState == ETeamWeaponState::Moving)
	{
		SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
		return;
	}

	TryIssuePostDeployAction();
}

void ATeamWeaponController::CancelInternalOutOfRangeMove()
{
	bM_HasInternalOutOfRangeReposition = false;
	bM_HasCachedOutOfRangeMoveLocation = false;
	bM_HasGuardEngageFlowTargetLocation = false;
}

bool ATeamWeaponController::GetHasPendingMovePostPackAction() const
{
	return M_PostPackAction.GetAbilityId() == EAbilityID::IdMove;
}

void ATeamWeaponController::SetPostPackActionForRotate(const FRotator& DesiredRotation,
                                                       const EAbilityID AbilityId,
                                                       const bool bShouldTriggerDoneExecuting,
                                                       const bool bIsInternalTurretRotation)
{
	M_PostPackAction.InitForCommand(
		AbilityId,
		false,
		FVector::ZeroVector,
		true,
		DesiredRotation,
		nullptr,
		bShouldTriggerDoneExecuting,
		bIsInternalTurretRotation);
}

bool ATeamWeaponController::GetShouldIgnoreAttackExecuteBecauseAttackAlreadyActive(AActor* NewTargetActor)
{
	if (not IsValid(NewTargetActor))
	{
		return false;
	}

	if (GetCurrentActiveCommand() != EAbilityID::IdAttack)
	{
		return false;
	}

	AActor* CurrentMainTarget = M_SpecificEngageTarget.Get();
	if (not IsValid(CurrentMainTarget))
	{
		return false;
	}

	return NewTargetActor != CurrentMainTarget;
}

void ATeamWeaponController::SetPostDeployActionForAttack(AActor* TargetActor)
{
	if (not IsValid(TargetActor))
	{
		M_PostDeployAction.Reset();
		return;
	}

	M_PostDeployAction.InitForCommand(
		EAbilityID::IdAttack,
		false,
		FVector::ZeroVector,
		false,
		FRotator::ZeroRotator,
		TargetActor,
		false);
}

void ATeamWeaponController::SetPostDeployActionForAttackGround(const FVector& GroundLocation)
{
	if (GroundLocation.IsNearlyZero())
	{
		M_PostDeployAction.Reset();
		return;
	}

	M_PostDeployAction.InitForCommand(
		EAbilityID::IdAttackGround,
		true,
		GroundLocation,
		false,
		FRotator::ZeroRotator,
		nullptr,
		false);
}

void ATeamWeaponController::TryIssuePostPackAction()
{
	if (bM_IsTeamWeaponAbandoned)
	{
		M_PostPackAction.Reset();
		M_PostDeployAction.Reset();
		return;
	}

	if (not M_PostPackAction.GetHasAction())
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

	IssuePostPackAction();
}

void ATeamWeaponController::IssuePostPackAction()
{
	switch (M_PostPackAction.GetAbilityId())
	{
	case EAbilityID::IdMove:
		IssuePostPackAction_Move();
		break;

	case EAbilityID::IdRotateTowards:
		IssuePostPackAction_Rotate();
		break;

	default:
		M_PostPackAction.Reset();
		break;
	}
}

void ATeamWeaponController::IssuePostPackAction_Move()
{
	if (not M_PostPackAction.GetHasLocation())
	{
		M_PostPackAction.Reset();
		return;
	}

	if (not M_CrewAssignment.GetHasEnoughOperators())
	{
		TryAbandonTeamWeaponForInsufficientCrew();
		M_PostPackAction.Reset();
		return;
	}

	if (M_TeamWeapon->GetMovementType() == ETeamWeaponMovementType::PushedWeaponLeads)
	{
		StartMoveWithPushedWeapon(M_PostPackAction.GetTargetLocation());
		M_PostPackAction.Reset();
		return;
	}

	StartMoveWithCrew(M_PostPackAction.GetTargetLocation());
	M_PostPackAction.Reset();
}

void ATeamWeaponController::IssuePostPackAction_Rotate()
{
	if (not M_PostPackAction.GetHasRotation())
	{
		M_PostPackAction.Reset();
		return;
	}

	StartRotationRequest(
		M_PostPackAction.GetTargetRotation(),
		M_PostPackAction.GetShouldTriggerDoneExecuting(),
		EAbilityID::IdRotateTowards,
		M_PostPackAction.GetIsInternalTurretRotation());
	M_PostPackAction.Reset();
}

void ATeamWeaponController::TryIssuePostDeployAction()
{
	if (bM_IsTeamWeaponAbandoned)
	{
		M_PostDeployAction.Reset();
		return;
	}

	if (not M_PostDeployAction.GetHasAction())
	{
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Packing || M_TeamWeaponState == ETeamWeaponState::Deploying)
	{
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
		return;
	}

	if (M_TeamWeaponState != ETeamWeaponState::Ready_Deployed)
	{
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	IssuePostDeployAction();
}

void ATeamWeaponController::IssuePostDeployAction()
{
	switch (M_PostDeployAction.GetAbilityId())
	{
	case EAbilityID::IdAttack:
		IssuePostDeployAction_Attack();
		break;

	case EAbilityID::IdAttackGround:
		IssuePostDeployAction_AttackGround();
		break;

	default:
		M_PostDeployAction.Reset();
		break;
	}
}

void ATeamWeaponController::IssuePostDeployAction_Attack()
{
	if (not M_PostDeployAction.GetHasTargetActor())
	{
		M_PostDeployAction.Reset();
		return;
	}

	AActor* TargetActor = M_PostDeployAction.GetTargetActor().Get();
	if (not IsValid(TargetActor))
	{
		M_PostDeployAction.Reset();
		return;
	}

	M_SpecificEngageTarget = TargetActor;
	M_TeamWeapon->SetSpecificEngageTarget(TargetActor);
	IssueSpecificEngageForGuards(TargetActor);
	ApplyCrewRoleWeaponRestrictions();
	M_PostDeployAction.Reset();
}

void ATeamWeaponController::IssuePostDeployAction_AttackGround()
{
	if (not M_PostDeployAction.GetHasLocation())
	{
		M_PostDeployAction.Reset();
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		M_PostDeployAction.Reset();
		return;
	}

	const FVector AttackGroundLocation = M_PostDeployAction.GetTargetLocation();
	M_TeamWeapon->SetEngageGroundLocation(AttackGroundLocation);
	IssueAttackGroundForGuards(AttackGroundLocation);
	ApplyCrewRoleWeaponRestrictions();
	M_PostDeployAction.Reset();
}

void ATeamWeaponController::ApplyCrewRoleWeaponRestrictions() const
{
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

		AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
		if (not IsValid(InfantryWeapon))
		{
			continue;
		}

		InfantryWeapon->DisableWeaponSearch(false, true);
	}
}

void ATeamWeaponController::IssueSpecificEngageForGuards(AActor* TargetActor) const
{
	if (not IsValid(TargetActor))
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

		// Crews of a team weapon take no automatic cover, which is all the order's provenance decides.
		constexpr bool bIsAutonomousOrder = false;
		SquadUnit->ExecuteAttackCommand(TargetActor, bIsAutonomousOrder);
	}
}

void ATeamWeaponController::IssueAttackGroundForGuards(const FVector& GroundLocation) const
{
	for (const TWeakObjectPtr<ASquadUnit>& GuardUnit : M_CrewAssignment.M_Guards)
	{
		ASquadUnit* SquadUnit = GuardUnit.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		SquadUnit->ExecuteAttackGroundCommand(GroundLocation);
	}
}

void ATeamWeaponController::TerminateSpecificEngageForGuards() const
{
	for (const TWeakObjectPtr<ASquadUnit>& GuardUnit : M_CrewAssignment.M_Guards)
	{
		ASquadUnit* SquadUnit = GuardUnit.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		SquadUnit->TerminateAttackCommand();
	}
}

void ATeamWeaponController::TerminateAttackGroundForGuards() const
{
	for (const TWeakObjectPtr<ASquadUnit>& GuardUnit : M_CrewAssignment.M_Guards)
	{
		ASquadUnit* SquadUnit = GuardUnit.Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		SquadUnit->TerminateAttackGroundCommand();
	}
}

void ATeamWeaponController::HandleMainAttackTargetBecameInvalid(const FString& Reason)
{
	static_cast<void>(Reason);

	if (GetIsGameShuttingDown() || bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	M_SpecificEngageTarget.Reset();
	M_PostDeployAction.Reset();
	TerminateSpecificEngageForGuards();
	ApplyCrewRoleWeaponRestrictions();

	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->SetSpecificEngageTarget(nullptr);
	}

	bM_HasGuardEngageFlowTargetLocation = false;
	bM_HasCachedOutOfRangeMoveLocation = false;
	bM_HasInternalOutOfRangeReposition = false;

	if (M_TeamWeaponState == ETeamWeaponState::Moving)
	{
		if (GetIsValidTeamWeaponMover())
		{
			M_TeamWeaponMover->AbortPushedFollowPath(TEXT("Main target invalid during internal reposition"));
			M_TeamWeaponMover->StopLegacyFollowCrew();
		}

		SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	}

	if (M_RotationRequest.bM_IsActive)
	{
		FinishRotationRequest();
	}

	UCommandData* CommandData = GetIsValidCommandData();
	if (CommandData != nullptr && CommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack)
	{
		DoneExecutingCommand(EAbilityID::IdAttack);
		M_UnitsCompletedCommand = 0;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed)
	{
		M_TeamWeapon->SetWeaponsEnabledForTeamWeaponState(true);
	}
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
			                            : (UnitOffset * FMath::FRandRange(
				                            NonCrewMinOffsetScale, NonCrewMaxOffsetScale));
		FNavPathPoint& PathPoint = UnitPath->GetPathPoints()[i];
		PathPoint.Location += PointOffset;

		if (bLast && bM_HasGuardEngageFlowTargetLocation && M_SpecificEngageTarget.IsValid() && GetIsValidTeamWeapon())
		{
			const float GuardEngageFlowDistanceCm = M_TeamWeapon->GetGuardEngageFlowDistanceCm();
			const FVector DirectionToEngageTarget = (M_GuardEngageFlowTargetLocation - PathPoint.Location).
				GetSafeNormal();
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

bool ATeamWeaponController::TryGetCrewPositionForOperatorIndex(const int32 OperatorIndex,
                                                               UCrewPosition*& OutCrewPosition) const
{
	OutCrewPosition = nullptr;
	if (OperatorIndex < 0)
	{
		return false;
	}

	TArray<UCrewPosition*> CrewPositions;
	if (not TryGetCrewPositionsSorted(CrewPositions))
	{
		return false;
	}

	if (OperatorIndex >= CrewPositions.Num())
	{
		return false;
	}

	OutCrewPosition = CrewPositions[OperatorIndex];
	return IsValid(OutCrewPosition);
}

void ATeamWeaponController::IssueMoveCrewToPositions()
{
	if (M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}

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

	const TSubclassOf<UNavigationQueryFilter> TeamWeaponQueryFilter = nullptr;

	for (int32 OperatorIndex = 0; OperatorIndex < M_CrewAssignment.M_Operators.Num(); ++OperatorIndex)
	{
		ASquadUnit* SquadUnit = M_CrewAssignment.M_Operators[OperatorIndex].Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		UCrewPosition* CrewPosition = nullptr;
		if (not TryGetCrewPositionForOperatorIndex(OperatorIndex, CrewPosition))
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

void ATeamWeaponController::ShowAbandoningAnimatedText() const
{
	if (not GetIsValidTeamWeapon() || not GetIsValidAnimatedTextWidgetPoolManager())
	{
		return;
	}

	const FTeamWeaponConfig& TeamWeaponConfig = M_TeamWeapon->GetTeamWeaponConfig();
	FRTSVerticalAnimTextSettings AbandoningAnimatedTextSettings = TeamWeaponConfig.M_AnimatedTextSettings;
	constexpr float AbandoningVisibleDuration = 1.0f;
	constexpr float AbandoningHideDuration = 0.5f;
	AbandoningAnimatedTextSettings.VisibleDuration = AbandoningVisibleDuration;
	AbandoningAnimatedTextSettings.FadeOutDuration = AbandoningHideDuration;

	constexpr bool bAutoWrap = false;
	const float WrapWidth = 300.0f;
	const TEnumAsByte<ETextJustify::Type> Justification = ETextJustify::Center;
	M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
		M_AbandoningAnimatedText,
		M_TeamWeapon->GetActorLocation(),
		bAutoWrap,
		WrapWidth,
		Justification,
		AbandoningAnimatedTextSettings);
}

void ATeamWeaponController::HandlePushedMoverArrived()
{
	RestorePushedMoveSpeedOverride();
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	if (bM_HasGuardEngageFlowTargetLocation && M_SpecificEngageTarget.IsValid())
	{
		// The guards walked behind the gun; they step out towards the target it repositioned for.
		IssueGuardEngageFlowMoves();
	}
	M_PostPackAction.Reset();
	bM_HasCachedOutOfRangeMoveLocation = false;
	bM_HasGuardEngageFlowTargetLocation = false;

	if (bM_HasInternalOutOfRangeReposition)
	{
		HandleInternalOutOfRangeMoveCompleted(true);
		return;
	}

	TryIssuePostDeployAction();

	DoneExecutingCommand(EAbilityID::IdMove);
}

void ATeamWeaponController::HandlePushedMoverFailed(const FString& FailureReason)
{
	RestorePushedMoveSpeedOverride();
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);

	if (bM_HasInternalOutOfRangeReposition)
	{
		HandleInternalOutOfRangeMoveCompleted(false);
		return;
	}

	if (bM_HasCachedOutOfRangeMoveLocation && M_SpecificEngageTarget.IsValid())
	{
		SetPostPackActionForMove(M_CachedOutOfRangeMoveLocation);
		SetPostDeployActionForAttack(M_SpecificEngageTarget.Get());
		TryIssuePostPackAction();
		return;
	}

	M_PostPackAction.Reset();
	M_PostDeployAction.Reset();
	bM_HasCachedOutOfRangeMoveLocation = false;
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

	if (NewState != ETeamWeaponState::Moving)
	{
		M_PushedCrewFormation.Stop();
	}

	// Crew montages only exist while deployed. Arming on entering Ready_Deployed is deliberately not done here:
	// the deploy flow re-issues crew moves right after, so arming waits for the settled checks.
	if (NewState != ETeamWeaponState::Ready_Deployed)
	{
		DisarmAllCrewAnimations();
	}

	ApplyCrewRoleWeaponRestrictions();

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

bool ATeamWeaponController::GetIsPushedWeaponLeadsMovementType() const
{
	if (not GetIsValidTeamWeapon())
	{
		return false;
	}

	return M_TeamWeapon->GetMovementType() == ETeamWeaponMovementType::PushedWeaponLeads;
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

bool ATeamWeaponController::GetIsValidTeamWeaponDigIn() const
{
	if (GetIsValidTeamWeaponNoReport() && IsValid(M_TeamWeapon->GetDigInComponent()))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TeamWeapon->GetDigInComponent()",
	                                                      "GetIsValidTeamWeaponDigIn", this);
	return false;
}

bool ATeamWeaponController::GetIsTeamWeaponDigInActive() const
{
	if (not GetIsValidTeamWeaponNoReport())
	{
		return false;
	}
	const UTeamWeaponDigInComponent* DigInComponent = M_TeamWeapon->GetDigInComponent();
	return IsValid(DigInComponent) && DigInComponent->GetIsDigInActive();
}

void ATeamWeaponController::StartTeamWeaponDigIn()
{
	bM_ShouldExecuteDigInAfterDeploy = false;
	if (not GetIsValidTeamWeaponDigIn())
	{
		DoneExecutingCommand(EAbilityID::IdDigIn);
		return;
	}
	UTeamWeaponDigInComponent* DigInComponent = M_TeamWeapon->GetDigInComponent();
	if (DigInComponent->GetIsDigInActive())
	{
		DoneExecutingCommand(EAbilityID::IdDigIn);
		return;
	}
	// Card and hull first: a pit with no build time stands at once and completes the command.
	OnStartDigIn();
	if (not DigInComponent->StartDigIn())
	{
		RemoveTeamWeaponDigIn();
		DoneExecutingCommand(EAbilityID::IdDigIn);
	}
}

void ATeamWeaponController::RemoveTeamWeaponDigIn()
{
	if (GetIsValidTeamWeaponNoReport())
	{
		if (UTeamWeaponDigInComponent* DigInComponent = M_TeamWeapon->GetDigInComponent())
		{
			DigInComponent->RemoveDigIn();
		}
		M_TeamWeapon->SetDigInHullRotationLocked(false);
	}
	RestoreCommandCardAfterDigIn();
}

void ATeamWeaponController::ApplyDigInCommandCard()
{
	if (M_DigInCardState.bIsApplied)
	{
		return;
	}
	M_DigInCardState.bIsApplied = true;
	M_DigInCardState.RemovedAbilities.Reset();
	if (HasAbility(EAbilityID::IdDigIn))
	{
		SwapAbility(EAbilityID::IdDigIn, EAbilityID::IdBreakCover);
	}
	const TArray<FUnitAbilityEntry> CardEntries = GetUnitAbilityEntries();
	for (const EAbilityID AbilityToRemove : {EAbilityID::IdMove, EAbilityID::IdRotateTowards, EAbilityID::IdTowActor})
	{
		const int32 SlotIndex = CardEntries.IndexOfByPredicate([AbilityToRemove](const FUnitAbilityEntry& Entry)
		{
			return Entry.AbilityId == AbilityToRemove;
		});
		if (SlotIndex == INDEX_NONE || not RemoveAbility(AbilityToRemove))
		{
			continue;
		}
		FTeamWeaponDigInRemovedAbility RemovedAbility;
		RemovedAbility.Entry = CardEntries[SlotIndex];
		RemovedAbility.SlotIndex = SlotIndex;
		M_DigInCardState.RemovedAbilities.Add(RemovedAbility);
	}
}

void ATeamWeaponController::RestoreCommandCardAfterDigIn()
{
	if (not M_DigInCardState.bIsApplied)
	{
		return;
	}
	M_DigInCardState.bIsApplied = false;
	if (HasAbility(EAbilityID::IdBreakCover))
	{
		SwapAbility(EAbilityID::IdBreakCover, EAbilityID::IdDigIn);
	}
	for (const FTeamWeaponDigInRemovedAbility& RemovedAbility : M_DigInCardState.RemovedAbilities)
	{
		if (not HasAbility(RemovedAbility.Entry.AbilityId))
		{
			AddAbility(RemovedAbility.Entry, RemovedAbility.SlotIndex);
		}
	}
	M_DigInCardState.RemovedAbilities.Reset();
}

void ATeamWeaponController::HandleTurretOutOfRangeWhileDugIn()
{
	const EAbilityID ActiveCommand = GetCurrentActiveCommand();
	if (ActiveCommand == EAbilityID::IdAttack || ActiveCommand == EAbilityID::IdAttackGround)
	{
		DoneExecutingCommand(ActiveCommand);
	}
}

bool ATeamWeaponController::TryGetDugInGuardPost(const int32 GuardIndex, FVector& OutLocation) const
{
	if (not GetIsValidTeamWeaponNoReport())
	{
		return false;
	}
	const UTeamWeaponDigInComponent* DigInComponent = M_TeamWeapon->GetDigInComponent();
	return IsValid(DigInComponent) && DigInComponent->TryGetGuardPostLocation(GuardIndex, OutLocation);
}

void ATeamWeaponController::PlaceGuardOnNavMeshIfStranded(ASquadUnit& Guard) const
{
	using namespace TeamWeaponGuardStatics;
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (not IsValid(NavigationSystem))
	{
		return;
	}
	const FVector GuardLocation = Guard.GetActorLocation();
	FNavLocation NavLocation;
	const FVector OnNavMeshExtent(StrandedGuardCheckRadiusCm, StrandedGuardCheckRadiusCm, StrandedGuardProjectionHeightCm);
	if (NavigationSystem->ProjectPointToNavigation(GuardLocation, NavLocation, OnNavMeshExtent))
	{
		return;
	}
	const FVector SearchExtent(StrandedGuardSearchRadiusCm, StrandedGuardSearchRadiusCm, StrandedGuardProjectionHeightCm);
	if (not NavigationSystem->ProjectPointToNavigation(GuardLocation, NavLocation, SearchExtent))
	{
		return;
	}
	const UCapsuleComponent* Capsule = Guard.GetCapsuleComponent();
	const float HalfHeightCm = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
	Guard.SetActorLocation(NavLocation.Location + FVector(0.f, 0.f, HalfHeightCm), false, nullptr,
	                       ETeleportType::TeleportPhysics);
}

void ATeamWeaponController::OnStartDigIn()
{
	bM_ShouldExecuteDigInAfterDeploy = false;
	FinishRotationRequest();
	ApplyDigInCommandCard();

	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->SetDigInHullRotationLocked(true);
	}
}

void ATeamWeaponController::OnDigInCompleted()
{
	// The guards walk into the pit; their posts exist once it stands.
	MoveGuardsToGuardPositionsIfMisplaced();
	// A stop or another order may have ended the dig-in command while the sandbags went up; the pit stays.
	if (GetCurrentActiveCommand() == EAbilityID::IdDigIn)
	{
		DoneExecutingCommand(EAbilityID::IdDigIn);
	}
}

void ATeamWeaponController::OnBreakCoverCompleted()
{
	bM_ShouldExecuteDigInAfterDeploy = false;

	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->SetDigInHullRotationLocked(false);
	}

	DoneExecutingCommand(EAbilityID::IdBreakCover);
}

void ATeamWeaponController::WallGotDestroyedForceBreakCover()
{
	const ECommandQueueError BreakCoverError = BreakCover(true);
	if (BreakCoverError != ECommandQueueError::NoError)
	{
		RTSFunctionLibrary::ReportError("Failed to break cover after team weapon dig-in wall got destroyed"
			"\n At function: ATeamWeaponController::WallGotDestroyedForceBreakCover"
			"\n For squad: " + GetName());
	}
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
	if (bM_IsTeamWeaponAbandoned || M_TeamWeaponState == ETeamWeaponState::Towed || not IsValid(CallingTurret))
	{
		return;
	}

	bM_HasCachedOutOfRangeMoveLocation = false;

	if (not GetIsValidTeamWeapon())
	{
		return;
	}
	if (GetIsTeamWeaponDigInActive())
	{
		HandleTurretOutOfRangeWhileDugIn();
		return;
	}

	UCommandData* CommandData = GetIsValidCommandData();
	const bool bHasActiveAttackCommand = CommandData != nullptr &&
		CommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack;

	AActor* SpecificEngageTarget = M_SpecificEngageTarget.Get();
	if (bHasActiveAttackCommand)
	{
		if (not IsValid(SpecificEngageTarget))
		{
			HandleMainAttackTargetBecameInvalid(TEXT("Main target invalid while handling out of range"));
			return;
		}
	}
	else if (SpecificEngageTarget == nullptr && IsValid(CallingTurret))
	{
		SpecificEngageTarget = CallingTurret->GetCurrentTargetActor();
	}

	if (SpecificEngageTarget == nullptr)
	{
		return;
	}

	M_SpecificEngageTarget = SpecificEngageTarget;
	SetPostDeployActionForAttack(SpecificEngageTarget);

	const FVector MoveLocation = GetMoveLocationWithinTurretRange(TargetLocation, CallingTurret);
	M_CachedOutOfRangeMoveLocation = MoveLocation;
	bM_HasCachedOutOfRangeMoveLocation = true;
	M_GuardEngageFlowTargetLocation = TargetLocation;
	bM_HasGuardEngageFlowTargetLocation = true;
	StartInternalOutOfRangeReposition(MoveLocation, SpecificEngageTarget);
}

void ATeamWeaponController::OnTurretInRange(ACPPTurretsMaster* CallingTurret)
{
	static_cast<void>(CallingTurret);

	if (bM_IsTeamWeaponAbandoned || M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}

	bM_HasCachedOutOfRangeMoveLocation = false;

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
		SetPostDeployActionForAttack(SpecificTarget);
		TryIssuePostDeployAction();
		return;
	}

	M_TeamWeapon->SetWeaponsEnabledForTeamWeaponState(true);
}

bool ATeamWeaponController::GetIsReadyDeployed() const
{
	return M_TeamWeaponState == ETeamWeaponState::Ready_Deployed;
}

void ATeamWeaponController::OnMountedWeaponTargetDestroyed(
	ACPPTurretsMaster* CallingTurret,
	UHullWeaponComponent* CallingHullWeapon,
	AActor* DestroyedActor,
	const bool bWasDestroyedByOwnWeapons)
{
	static_cast<void>(CallingTurret);
	static_cast<void>(CallingHullWeapon);
	static_cast<void>(bWasDestroyedByOwnWeapons);

	if (GetIsGameShuttingDown() || bM_IsTeamWeaponAbandoned || not IsValid(DestroyedActor))
	{
		return;
	}

	UCommandData* CommandData = GetIsValidCommandData();
	const bool bAttackActive = CommandData != nullptr &&
		CommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack;
	AActor* MainTarget = M_SpecificEngageTarget.Get();

	if (bAttackActive)
	{
		if (DestroyedActor == MainTarget)
		{
			HandleMainAttackTargetBecameInvalid(TEXT("Main target destroyed (team weapon turret)"));
			return;
		}

		if (IsValid(MainTarget))
		{
			SetPostDeployActionForAttack(MainTarget);
			TryIssuePostDeployAction();
			return;
		}

		HandleMainAttackTargetBecameInvalid(TEXT("Main target invalid after non-main kill"));
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
	{
		StartDeploying();
		return;
	}

	if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed && GetIsValidTeamWeapon())
	{
		M_TeamWeapon->SetWeaponsEnabledForTeamWeaponState(true);
	}
}

void ATeamWeaponController::OnFireWeapon(ACPPTurretsMaster* CallingTurret)
{
	if(not IsValid(M_TeamWeapon))
	{
		return;
	}
	M_TeamWeapon->PlayFireAnim();
		
}

void ATeamWeaponController::OnTurretWeaponReloadStart(ACPPTurretsMaster* CallingTurret, const int32 WeaponIndex,
                                                       const float ReloadTime)
{
	if (CallingTurret == nullptr || CallingTurret != M_TeamWeapon.Get())
	{
		return;
	}
	if (WeaponIndex != TeamWeaponCrewAnimationStatics::PrimaryWeaponIndex)
	{
		return;
	}
	if (M_TeamWeaponState != ETeamWeaponState::Ready_Deployed || bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	for (const FTeamWeaponCrewAnimationSlot& Slot : M_CrewAnimationState.M_Slots)
	{
		if (not Slot.bM_IsArmed)
		{
			continue;
		}
		USquadUnitAnimInstance* AnimInstance = TeamWeaponCrewAnimationStatics::GetOperatorAnimInstanceNoReport(Slot);
		if (AnimInstance == nullptr)
		{
			continue;
		}
		AnimInstance->OnTeamWeaponReloadStarted(ReloadTime);
	}
}

void ATeamWeaponController::OnProjectileHit(const bool bBounced)
{
}

void ATeamWeaponController::StartRotationRequest(const FRotator& DesiredRotation,
                                                 const bool bShouldTriggerDoneExecuting,
                                                 const EAbilityID CompletionAbilityId,
                                                 const bool bIsInternalTurretRotation)
{
	M_RotationRequest.M_TargetRotation = DesiredRotation;
	M_RotationRequest.bM_IsActive = true;
	M_RotationRequest.bM_ShouldTriggerDoneExecuting = bShouldTriggerDoneExecuting;
	M_RotationRequest.bM_IsInternalTurretRotation = bIsInternalTurretRotation;
	M_RotationRequest.M_CompletionAbilityId = CompletionAbilityId;
	AttachCrewForRotation();
}

void ATeamWeaponController::TickRotationRequest(const float DeltaSeconds)
{
	if (M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}

	if (not M_RotationRequest.bM_IsActive)
	{
		return;
	}

	UCommandData* CommandData = GetIsValidCommandData();
	const bool bHasActiveAttackCommand = CommandData != nullptr &&
		CommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack;
	if (M_RotationRequest.bM_IsInternalTurretRotation && bHasActiveAttackCommand && not M_SpecificEngageTarget.
		IsValid())
	{
		HandleMainAttackTargetBecameInvalid(TEXT("Main target invalid during rotation"));
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
	// Turning on a slope changes the ground under the footprint.
	M_TeamWeapon->ConformToGround(DeltaSeconds, false);
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
	const bool bWasInternalTurretRotation = M_RotationRequest.bM_IsInternalTurretRotation;
	const bool bHasPendingOutOfRangeReposition = bM_HasInternalOutOfRangeReposition ||
		bM_HasCachedOutOfRangeMoveLocation;
	UCommandData* CommandData = GetIsValidCommandData();
	const bool bHasActiveAttackCommand = CommandData != nullptr &&
		CommandData->GetCurrentlyActiveCommandType() == EAbilityID::IdAttack;
	const bool bHasPendingPostDeployAttack = M_PostDeployAction.GetAbilityId() == EAbilityID::IdAttack;
	AActor* MainSpecificTarget = M_SpecificEngageTarget.Get();
	const bool bHasSpecificEngageTarget = IsValid(MainSpecificTarget);
	const bool bHasAttackIntent = bHasActiveAttackCommand || bHasPendingPostDeployAttack || bHasSpecificEngageTarget;
	if (GetIsValidTeamWeaponNoReport())
	{
		M_TeamWeapon->ConformToGround(0.f, true);
	}
	SnapOperatorsToCrewPositionsDuringRotation();
	M_RotationRequest.Reset();
	DetachCrewAfterRotation();
	MoveGuardsToRandomGuardPositions();

	if (bShouldCallDoneExecuting && CompletionAbilityId != EAbilityID::IdNoAbility)
	{
		DoneExecutingCommand(CompletionAbilityId);
	}

	if ((bWasInternalTurretRotation || bHasAttackIntent) && not bHasPendingOutOfRangeReposition &&
		not bM_IsTeamWeaponAbandoned && GetIsValidTeamWeapon() &&
		not GetHasPendingMovePostPackAction())
	{
		if (M_TeamWeaponState == ETeamWeaponState::Ready_Packed)
		{
			StartDeploying();
		}
		else if (M_TeamWeaponState == ETeamWeaponState::Ready_Deployed)
		{
			M_TeamWeapon->SetWeaponsEnabledForTeamWeaponState(true);
		}
	}

	TryIssuePostDeployAction();
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

	SnapOperatorsToCrewPositions();
}

void ATeamWeaponController::SnapOperatorsToCrewPositions()
{
	if (not M_CrewAssignment.GetHasEnoughOperators())
	{
		return;
	}

	for (int32 OperatorIndex = 0; OperatorIndex < M_CrewAssignment.M_Operators.Num(); ++OperatorIndex)
	{
		ASquadUnit* SquadUnit = M_CrewAssignment.M_Operators[OperatorIndex].Get();
		if (not GetIsValidSquadUnit(SquadUnit))
		{
			continue;
		}

		UCrewPosition* CrewPosition = nullptr;
		if (not TryGetCrewPositionForOperatorIndex(OperatorIndex, CrewPosition))
		{
			continue;
		}

		SnapOperatorToCrewPosition(SquadUnit, CrewPosition);
	}

	// Teleported operators are settled by definition; arm right away instead of waiting for the tick fallback.
	TryArmCrewAnimationsForSettledOperators();
}

bool ATeamWeaponController::SnapOperatorToCrewPosition(ASquadUnit* SquadUnit, const UCrewPosition* CrewPosition) const
{
	if (not IsValid(SquadUnit) || not IsValid(CrewPosition))
	{
		return false;
	}

	FVector OperatorTeleportLocation = CrewPosition->GetComponentLocation();
	if (not TryGetLandscapeTeleportLocationForCrewPosition(SquadUnit, OperatorTeleportLocation,
	                                                       OperatorTeleportLocation))
	{
		return false;
	}

	// Upright: the crew position tilts with the gun on a slope, a soldier must not.
	SquadUnit->SetActorLocationAndRotation(
		OperatorTeleportLocation,
		FRotator(0.f, CrewPosition->GetComponentRotation().Yaw, 0.f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	return true;
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
	float GroundZ = 0.f;
	if (GetIsValidTeamWeaponNoReport())
	{
		// The gun mesh blocks the landscape channel; its own trace steps past the gun and other units.
		if (not M_TeamWeapon->TryGetGroundHeightAt(CrewPositionLocation, GroundZ))
		{
			return false;
		}
	}
	else
	{
		const FVector TraceStart = CrewPositionLocation + FVector::UpVector * TraceStartHeightCm;
		const FVector TraceEnd = TraceStart - FVector::UpVector * TraceLengthCm;
		FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(TeamWeaponCrewLandscapeSnap), false, this);
		FHitResult LandscapeHitResult;
		if (not World->LineTraceSingleByChannel(LandscapeHitResult, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE,
		                                        CollisionQueryParams))
		{
			return false;
		}
		GroundZ = LandscapeHitResult.ImpactPoint.Z;
	}

	const UCapsuleComponent* SquadCapsuleComponent = SquadUnit->GetCapsuleComponent();
	if (not IsValid(SquadCapsuleComponent))
	{
		return false;
	}

	const float ScaledCapsuleHalfHeight = SquadCapsuleComponent->GetScaledCapsuleHalfHeight();
	OutTeleportLocation.Z = GroundZ + ScaledCapsuleHalfHeight + LandscapeClearanceCm;
	return true;
}

void ATeamWeaponController::MoveGuardsToRandomGuardPositions() const
{
	if (M_TeamWeaponState == ETeamWeaponState::Towed)
	{
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}
	if (GetIsTeamWeaponDigInActive())
	{
		MoveGuardsToGuardPositionsIfMisplaced();
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


bool ATeamWeaponController::GetShouldAutoDeployOnIdle()
{
	if (M_TeamWeaponState != ETeamWeaponState::Ready_Packed)
	{
		return false;
	}

	if (M_PostPackAction.GetHasAction())
	{
		return false;
	}

	if (bM_HasCachedOutOfRangeMoveLocation)
	{
		return false;
	}

	if (bM_HasInternalOutOfRangeReposition)
	{
		return false;
	}

	const bool bHasActiveAttackCommand = GetCurrentActiveCommand() == EAbilityID::IdAttack;
	if (bHasActiveAttackCommand && M_SpecificEngageTarget.IsValid())
	{
		return false;
	}

	return true;
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
	OnTeamWeaponAbandonedForInsufficientCrew.Broadcast(this);
}

void ATeamWeaponController::AbandonTeamWeapon()
{
	if (GetIsGameShuttingDown())
	{
		return;
	}

	RemoveControlledTeamWeaponDestroyedCallback();
	ResetTowRelationshipOnTeamWeaponSide(true, true);

	SetTeamWeaponState(ETeamWeaponState::Abandoned);
	M_PostPackAction.Reset();
	M_PostDeployAction.Reset();
	bM_HasCachedOutOfRangeMoveLocation = false;
	FinishRotationRequest();

	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->AbortPushedFollowPath(TEXT("Team weapon abandoned due to insufficient operators"));
		M_TeamWeaponMover->StopLegacyFollowCrew();
	}

	RestorePushedMoveSpeedOverride();
	ShowAbandoningAnimatedText();

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

		InfantryWeapon->SetAutoEngageTargets(false);
	}

	if (GetIsValidTeamWeapon())
	{
		// Nobody is left to hold the pit; a new crew digs in again.
		RemoveTeamWeaponDigIn();
		M_TeamWeapon->OnTeamWeaponAbandoned();
	}

	M_TeamWeapon = nullptr;
	M_TeamWeaponMover = nullptr;
	bM_IsTeamWeaponAbandoned = true;
	// The Abandoned state transition already disarmed every operator; only the bookkeeping is left.
	M_CrewAnimationState.Reset();
}

void ATeamWeaponController::RegisterControlledTeamWeaponDestroyedCallback()
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	M_TeamWeapon->OnDestroyed.AddUniqueDynamic(
		this,
		&ATeamWeaponController::HandleControlledTeamWeaponDestroyed);
}

void ATeamWeaponController::RemoveControlledTeamWeaponDestroyedCallback()
{
	ATeamWeapon* TeamWeapon = M_TeamWeapon.Get();
	if (TeamWeapon == nullptr)
	{
		return;
	}

	TeamWeapon->OnDestroyed.RemoveDynamic(
		this,
		&ATeamWeaponController::HandleControlledTeamWeaponDestroyed);
}

void ATeamWeaponController::HandleControlledTeamWeaponDestroyed(AActor* DestroyedActor)
{
	if (GetIsGameShuttingDown() || bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	if (M_TeamWeapon.Get() != DestroyedActor)
	{
		return;
	}

	OnControlledTeamWeaponLost.Broadcast(this);
}


bool ATeamWeaponController::GetIsValidTowedActorComponent() const
{
	if (M_TowedActorComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_TowedActorComponent",
	                                                             "ATeamWeaponController::GetIsValidTowedActorComponent", this);
	return false;
}

void ATeamWeaponController::ExecuteDetachTowCommand()
{
	UTowedActorComponent* TowedActorComponent = GetControlledTeamWeaponTowedActorComponentNoReport();
	if (not IsValid(TowedActorComponent) || TowedActorComponent->IsTowFree())
	{
		DoneExecutingCommand(EAbilityID::IdDetachTow);
		return;
	}

	if (not IsValid(Cast<ATankMaster>(TowedActorComponent->GetTowingActor())))
	{
		ResetTowRelationshipOnTeamWeaponSide(false, true);
		SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
		SetTeamWeaponAutoEngageEnabled(true);
		DoneExecutingCommand(EAbilityID::IdDetachTow);
		return;
	}

	ATeamWeapon* TeamWeaponActor = M_TeamWeapon;
	if (not IsValid(TeamWeaponActor))
	{
		ResetTowRelationshipOnTeamWeaponSide(false, true);
		SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
		DoneExecutingCommand(EAbilityID::IdDetachTow);
		return;
	}

	ExecuteDetachTowCommand_ResetTowedVisualState(TeamWeaponActor);

	TeamWeaponActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// The gun stays where it swung to behind the truck, but upright on its heading and on the ground.
	TeamWeaponActor->EndTowSway();
	PlaceDetachedWeaponOnNavigableGround(*TeamWeaponActor);
	TeamWeaponActor->ConformToGround(0.f, true);

	ReleaseCargoSquadUnitsFromTow();

	AssignCrewToTeamWeapon();
	SnapOperatorsToCrewPositions();
	MoveGuardsToRandomGuardPositions();

	ResetTowRelationshipOnTeamWeaponSide(false, false);
	SetTeamWeaponState(ETeamWeaponState::Ready_Packed);
	SetTeamWeaponAutoEngageEnabled(true);
	DoneExecutingCommand(EAbilityID::IdDetachTow);
}

void ATeamWeaponController::PlaceDetachedWeaponOnNavigableGround(ATeamWeapon& TeamWeaponActor) const
{
	using namespace TeamWeaponGuardStatics;
	// A gun swung out behind the truck can hang over ground the crew cannot walk (a steep bank); every later
	// push starts its path from the gun and would fail there for good.
	const FVector WeaponLocation = TeamWeaponActor.GetActorLocation();
	const FVector NavigableLocation = ProjectLocationOnNavMesh(WeaponLocation, DetachNavProjectionRadiusCm, true);
	TeamWeaponActor.SetActorLocation(FVector(NavigableLocation.X, NavigableLocation.Y, WeaponLocation.Z), false, nullptr,
	                                 ETeleportType::TeleportPhysics);
}

void ATeamWeaponController::SetTeamWeaponAutoEngageEnabled(const bool bEnableAutoEngage) const
{
	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	M_TeamWeapon->SetAutoEngageTargets(bEnableAutoEngage);
}

void ATeamWeaponController::ExecuteDetachTowCommand_ResetTowedVisualState(ATeamWeapon* TeamWeaponActor) const
{
	if (not IsValid(TeamWeaponActor))
	{
		return;
	}

	TeamWeaponActor->NotifyMoverMovementState(false, FVector::ZeroVector);

}

void ATeamWeaponController::TerminateDetachTowCommand()
{
}

void ATeamWeaponController::OnActorBeingTowed(AActor* TowingVehicle, UVehicleTowComponent* TowComp)
{
	CancelPendingDeployForTow();
	if (GetIsValidTeamWeapon())
	{
		M_TeamWeapon->PlayPackingMontage(false);
		M_TeamWeapon->StartTowSway();
		SetTeamWeaponAutoEngageEnabled(false);
	}

	SetTeamWeaponState(ETeamWeaponState::Towed);
	if (UTowedActorComponent* TowedActorComponent = GetControlledTeamWeaponTowedActorComponentNoReport())
	{
		TowedActorComponent->SetTowRelationship(TowingVehicle, TowComp);
		TowedActorComponent->RemoveAbilitiesWhileTowed();
	}
	if (UCommandData* CommandData = GetIsValidCommandData())
	{
		CommandData->SwapAbility(EAbilityID::IdTowActor, EAbilityID::IdDetachTow);
	}

	TickTowedWeaponAnimation(0.f);
}

void ATeamWeaponController::CancelPendingDeployForTow()
{
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_DeployTimer);
	}
	M_PostPackAction.Reset();
	M_PostDeployAction.Reset();
	bM_ShouldExecuteDigInAfterDeploy = false;
	// A towed gun leaves its pit behind: walls, protection and the dug-in command card go.
	RemoveTeamWeaponDigIn();
	CancelInternalOutOfRangeMove();
	if (M_RotationRequest.bM_IsActive)
	{
		M_RotationRequest.Reset();
		DetachCrewAfterRotation();
	}
	if (GetIsValidTeamWeaponMover())
	{
		M_TeamWeaponMover->AbortPushedFollowPath(TEXT("Team weapon hooked to a truck"));
		M_TeamWeaponMover->StopLegacyFollowCrew();
	}
	RestorePushedMoveSpeedOverride();
}

void ATeamWeaponController::ResetTowRelationshipOnTeamWeaponSide(const bool bDetachTeamWeaponActor,
                                                                 const bool bReleaseCargoSquadUnits)
{
	UTowedActorComponent* TowedActorComponent = GetControlledTeamWeaponTowedActorComponentNoReport();
	if (not IsValid(TowedActorComponent) || TowedActorComponent->IsTowFree())
	{
		return;
	}

	ATeamWeapon* TeamWeaponActor = M_TeamWeapon.Get();
	if (IsValid(TeamWeaponActor))
	{
		ExecuteDetachTowCommand_ResetTowedVisualState(TeamWeaponActor);
	}

	if (bDetachTeamWeaponActor && IsValid(TeamWeaponActor))
	{
		TeamWeaponActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		TeamWeaponActor->EndTowSway();
	}

	if (bReleaseCargoSquadUnits)
	{
		ReleaseCargoSquadUnitsFromTow();
	}

	TowedActorComponent->RestoreAbilitiesAfterTow();
	if (UVehicleTowComponent* TowingTowComp = TowedActorComponent->GetTowingVehicleTowComp())
	{
		TowingTowComp->ClearTowRelationship();
		TowingTowComp->SwapAbilityToTow();
	}
	TowedActorComponent->ClearTowRelationship();

	if (UCommandData* CommandData = GetIsValidCommandData())
	{
		CommandData->SwapAbility(EAbilityID::IdDetachTow, EAbilityID::IdTowActor);
	}
}

void ATeamWeaponController::TickTowedWeaponAnimation(const float DeltaSeconds)
{
	if (M_TeamWeaponState != ETeamWeaponState::Towed)
	{
		return;
	}

	if (not GetIsValidTeamWeapon())
	{
		return;
	}

	// Visual only: the gun trails, bounces and leans about the tow hook; the attachment itself stays rigid.
	M_TeamWeapon->TickTowSway(DeltaSeconds);

	UTowedActorComponent* TowedActorComponent = GetControlledTeamWeaponTowedActorComponentNoReport();
	if (not IsValid(TowedActorComponent))
	{
		M_TeamWeapon->NotifyMoverMovementState(false, FVector::ZeroVector);
		return;
	}

	AActor* TowingActor = TowedActorComponent->GetTowingActor();
	if (not IsValid(TowingActor))
	{
		M_TeamWeapon->NotifyMoverMovementState(false, FVector::ZeroVector);
		return;
	}

	const FVector TowingVelocity = TowingActor->GetVelocity();
	M_TeamWeapon->NotifyMoverMovementState(not TowingVelocity.IsNearlyZero(), TowingVelocity);
}

void ATeamWeaponController::ReleaseCargoSquadUnitsFromTow()
{
	UCargo* TankCargo = nullptr;
	UTowedActorComponent* TowedActorComponent = GetControlledTeamWeaponTowedActorComponentNoReport();
	if (IsValid(TowedActorComponent))
	{
		if (ATankMaster* TowingTank = Cast<ATankMaster>(TowedActorComponent->GetTowingActor()))
		{
			TankCargo = TowingTank->FindComponentByClass<UCargo>();
		}
	}

	UCargoSquad* TeamWeaponCargoSquad = FindComponentByClass<UCargoSquad>();
	if (not IsValid(TankCargo) || not IsValid(TeamWeaponCargoSquad))
	{
		RTSFunctionLibrary::ReportError("Missing cargo component during team weapon detach in ATeamWeaponController::ReleaseCargoSquadUnitsFromTow");
		return;
	}

	TeamWeaponCargoSquad->ExitCargoImmediate(false);
}

// ---- Crew animations ----

void ATeamWeaponController::RebuildCrewAnimationSlots()
{
	const ESquadSubtype NewTeamWeaponSquadSubtype = GetTeamWeaponSquadSubtypeForCrewAnimations();
	const bool bSubtypeUnchanged = NewTeamWeaponSquadSubtype == M_CrewAnimationState.M_TeamWeaponSquadSubtype;

	TArray<FTeamWeaponCrewAnimationSlot> NewSlots;
	NewSlots.Reserve(M_CrewAssignment.M_Operators.Num());
	for (int32 OperatorIndex = 0; OperatorIndex < M_CrewAssignment.M_Operators.Num(); ++OperatorIndex)
	{
		FTeamWeaponCrewAnimationSlot NewSlot;
		NewSlot.M_Operator = M_CrewAssignment.M_Operators[OperatorIndex];

		UCrewPosition* CrewPosition = nullptr;
		if (TryGetCrewPositionForOperatorIndex(OperatorIndex, CrewPosition))
		{
			NewSlot.M_CrewPosition = CrewPosition;
			NewSlot.M_CrewRole = CrewPosition->GetCrewPositionType();
		}
		NewSlots.Add(NewSlot);
	}

	// Keep operators armed only when their whole assignment survived; everything else is disarmed.
	for (FTeamWeaponCrewAnimationSlot& OldSlot : M_CrewAnimationState.M_Slots)
	{
		if (not OldSlot.bM_IsArmed)
		{
			continue;
		}

		FTeamWeaponCrewAnimationSlot* MatchingNewSlot = NewSlots.FindByPredicate(
			[&OldSlot](const FTeamWeaponCrewAnimationSlot& NewSlot)
			{
				return TeamWeaponCrewAnimationStatics::GetIsSameCrewAnimationAssignment(NewSlot, OldSlot);
			});
		if (MatchingNewSlot != nullptr && bSubtypeUnchanged)
		{
			MatchingNewSlot->bM_IsArmed = true;
			continue;
		}

		DisarmCrewAnimationSlot(OldSlot);
	}

	M_CrewAnimationState.M_Slots = MoveTemp(NewSlots);
	M_CrewAnimationState.M_TeamWeaponSquadSubtype = NewTeamWeaponSquadSubtype;
}

ESquadSubtype ATeamWeaponController::GetTeamWeaponSquadSubtypeForCrewAnimations() const
{
	ESquadSubtype TeamWeaponSquadSubtype = ESquadSubtype::Squad_None;
	if (GetIsValidRTSComponent())
	{
		TeamWeaponSquadSubtype = GetRTSComponent()->GetSubtypeAsSquadSubtype();
	}
	if (TeamWeaponSquadSubtype != ESquadSubtype::Squad_None)
	{
		return TeamWeaponSquadSubtype;
	}

	// Adopted or map placed weapons carry the authoritative subtype on the weapon actor itself.
	if (GetIsValidTeamWeapon())
	{
		return M_TeamWeapon->GetSquadSubtypeFromRTSComponent();
	}
	return ESquadSubtype::Squad_None;
}

bool ATeamWeaponController::GetIsOperatorSettledAtCrewPosition(const FTeamWeaponCrewAnimationSlot& Slot) const
{
	if (not GetIsValidCrewAnimationSlotOperator(Slot))
	{
		return false;
	}

	const UCrewPosition* CrewPosition = Slot.M_CrewPosition.Get();
	if (not IsValid(CrewPosition))
	{
		return false;
	}

	const ASquadUnit* Operator = Slot.M_Operator.Get();
	// Still walking its crew move (or any other move): not there yet.
	if (Operator->GetIsPathFollowingActive())
	{
		return false;
	}

	// Let braking finish so the montage never starts on a sliding unit.
	if (Operator->GetVelocity().Size2D() > TeamWeaponCrewAnimationStatics::SettledSpeedThresholdCmPerSec)
	{
		return false;
	}

	// Crew moves allow partial paths and do not project the goal, so a unit that finished its move can legally
	// stand short of the spot; ArmCrewAnimationSlot snaps it onto the position. Only a stuck unit is skipped.
	const float DistanceToCrewPositionCm = FVector::Dist2D(
		Operator->GetActorLocation(),
		CrewPosition->GetComponentLocation());
	return DistanceToCrewPositionCm <= TeamWeaponCrewAnimationStatics::MaxSnapToCrewPositionDistanceCm;
}

void ATeamWeaponController::TryArmCrewAnimationsForSettledOperators()
{
	if (M_TeamWeaponState != ETeamWeaponState::Ready_Deployed || bM_IsTeamWeaponAbandoned)
	{
		return;
	}

	for (FTeamWeaponCrewAnimationSlot& Slot : M_CrewAnimationState.M_Slots)
	{
		if (Slot.bM_IsArmed || not Slot.GetHasRole())
		{
			continue;
		}
		if (not GetIsOperatorSettledAtCrewPosition(Slot))
		{
			continue;
		}
		ArmCrewAnimationSlot(Slot);
	}
}

void ATeamWeaponController::ArmCrewAnimationSlot(FTeamWeaponCrewAnimationSlot& Slot)
{
	USquadUnitAnimInstance* AnimInstance = TeamWeaponCrewAnimationStatics::GetOperatorAnimInstanceNoReport(Slot);
	const UCrewPosition* CrewPosition = Slot.M_CrewPosition.Get();
	if (AnimInstance == nullptr || not IsValid(CrewPosition))
	{
		return;
	}

	// The full body pose is authored relative to the weapon, so put the operator exactly on its crew position
	// (crew moves may legally end short of it). Without a landscape hit keep the reached spot but face the weapon.
	ASquadUnit* Operator = Slot.M_Operator.Get();
	if (not SnapOperatorToCrewPosition(Operator, CrewPosition))
	{
		FRotator OperatorRotation = Operator->GetActorRotation();
		OperatorRotation.Yaw = CrewPosition->GetComponentRotation().Yaw;
		Operator->SetActorRotation(OperatorRotation, ETeleportType::TeleportPhysics);
	}

	AnimInstance->StartTeamWeaponCrewAnimation(Slot.M_CrewRole, M_CrewAnimationState.M_TeamWeaponSquadSubtype);
	Slot.bM_IsArmed = true;

	if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
	{
		TeamWeaponCrewAnimationStatics::PrintCrewAnimDebug(
			Operator,
			"Armed crew animation: " + Operator->GetName() + " role: " + UEnum::GetValueAsString(Slot.M_CrewRole));
	}
}

void ATeamWeaponController::DisarmCrewAnimationSlot(FTeamWeaponCrewAnimationSlot& Slot)
{
	if (not Slot.bM_IsArmed)
	{
		return;
	}
	Slot.bM_IsArmed = false;

	USquadUnitAnimInstance* AnimInstance = TeamWeaponCrewAnimationStatics::GetOperatorAnimInstanceNoReport(Slot);
	if (AnimInstance == nullptr)
	{
		return;
	}
	AnimInstance->StopTeamWeaponCrewAnimation();
}

void ATeamWeaponController::DisarmAllCrewAnimations()
{
	for (FTeamWeaponCrewAnimationSlot& Slot : M_CrewAnimationState.M_Slots)
	{
		DisarmCrewAnimationSlot(Slot);
	}
}

void ATeamWeaponController::TickCrewAnimationArming()
{
	if (M_TeamWeaponState != ETeamWeaponState::Ready_Deployed || bM_IsTeamWeaponAbandoned)
	{
		return;
	}
	if (not M_CrewAnimationState.GetHasSlotsToArm() && not M_CrewAnimationState.GetHasArmedSlots())
	{
		return;
	}

	for (FTeamWeaponCrewAnimationSlot& Slot : M_CrewAnimationState.M_Slots)
	{
		if (not Slot.bM_IsArmed)
		{
			continue;
		}
		TickArmedCrewAnimationSlot(Slot);
	}

	TryArmCrewAnimationsForSettledOperators();
}

void ATeamWeaponController::TickArmedCrewAnimationSlot(FTeamWeaponCrewAnimationSlot& Slot)
{
	// The anim instance drops the crew animation itself on StopAllMontages or when the unit starts moving;
	// mirror that here so the slot can be re-armed once the operator is settled again.
	const USquadUnitAnimInstance* AnimInstance = TeamWeaponCrewAnimationStatics::GetOperatorAnimInstanceNoReport(Slot);
	if (AnimInstance == nullptr || not AnimInstance->GetIsTeamWeaponCrewAnimationActive())
	{
		Slot.bM_IsArmed = false;
		return;
	}

	// Movement that bypasses the team weapon state machine (retreat, cargo, evasion) leaves Ready_Deployed intact.
	// Only an actual move order ends the crewing; standing slightly off the spot never does (it was snapped on arm).
	ASquadUnit* Operator = Slot.M_Operator.Get();
	if (not Operator->GetIsPathFollowingActive())
	{
		ResnapArmedOperatorPushedOffPost(Slot);
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols)
	{
		TeamWeaponCrewAnimationStatics::PrintCrewAnimDebug(
			Operator,
			"Operator was ordered to move; disarming crew animation.");
	}
	DisarmCrewAnimationSlot(Slot);
}

void ATeamWeaponController::ResnapArmedOperatorPushedOffPost(const FTeamWeaponCrewAnimationSlot& Slot) const
{
	ASquadUnit* Operator = Slot.M_Operator.Get();
	const UCrewPosition* CrewPosition = Slot.M_CrewPosition.Get();
	if (not IsValid(Operator) || not IsValid(CrewPosition))
	{
		return;
	}
	// Other soldiers bump a crewing operator off his spot; his montage is authored for the spot, so put him back.
	const float DistanceToPostCm = FVector::Dist2D(Operator->GetActorLocation(), CrewPosition->GetComponentLocation());
	const bool bIsBraking = Operator->GetVelocity().Size2D() > TeamWeaponCrewAnimationStatics::SettledSpeedThresholdCmPerSec;
	if (DistanceToPostCm <= CrewPosition->GetAcceptanceRadius() || bIsBraking)
	{
		return;
	}
	SnapOperatorToCrewPosition(Operator, CrewPosition);
}

bool ATeamWeaponController::GetIsValidCrewAnimationSlotOperator(const FTeamWeaponCrewAnimationSlot& Slot) const
{
	// Silent on purpose: an operator that just died leaves a dangling weak pointer until the crew is rebuilt.
	return TeamWeaponCrewAnimationStatics::GetOperatorAnimInstanceNoReport(Slot) != nullptr;
}

// ---- End crew animations ----
