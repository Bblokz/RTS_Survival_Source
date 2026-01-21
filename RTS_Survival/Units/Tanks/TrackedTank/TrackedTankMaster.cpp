// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TrackedTankMaster.h"

#include "TrackedAnimationInstance.h"
#include "Components/AudioComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "PathFollowingComponent/TrackPathFollowingComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Audio/SpacialVoiceLinePlayer/SpatialVoiceLinePlayer.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRockets.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInComponent.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceComponent.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"


FName ATrackedTankMaster::TrackPhysicsMovementName(TEXT("TrackPhysicsCalcMovement"));


ATrackedTankMaster::ATrackedTankMaster(const FObjectInitializer& ObjectInitializer)
	: ATankMaster(ObjectInitializer), TankAnimationBP(nullptr), ChassisMesh(NULL)
{
	TrackPhysicsMovement = CreateDefaultSubobject<UTrackPhysicsMovement>(TrackPhysicsMovementName);
	ExperienceComponent = CreateDefaultSubobject<URTSExperienceComp>(TEXT("ExperienceComponent"));
	RTSOverlapEvasionComponent = CreateDefaultSubobject<
		URTSOverlapEvasionComponent>(TEXT("RTSOverlapEvasionComponent"));
}

void ATrackedTankMaster::UpdateVehicle_Implementation(
	float TargetAngle,
	float DestinationDistance,
	float DesiredSpeed,
	float CalculatedThrottle,
	float CurrentSpeed,
	float DeltaTime,
	float CalculatedBreak,
	float CalculatedSteering)
{
	if (IsValid(TrackPhysicsMovement))
	{
		TrackPhysicsMovement->UpdateTankMovement(
			DeltaTime,
			CurrentSpeed,
			CalculatedThrottle,
			CalculatedSteering);
	}
}

void ATrackedTankMaster::SetRTSOverlapEvasionEnabled(const bool bEnabled)
{
	if(not EnsureRTSOverlapEvasionComponent())
	{
	return;	
	}
	RTSOverlapEvasionComponent->SetOverlapEvasionEnabled(bEnabled);
}

void ATrackedTankMaster::OnFinishedPathFollowing()
{
	if (IsValid(TrackPhysicsMovement))
	{
		TrackPhysicsMovement->OnPathFollowingFinished();

		if (TrackPhysicsMovement->GetLastNoneZeroThrottle() < 0)
		{
			// We arrived in reverse.
			bWasLastMovementReverse = true;

			// If not forced; reverse clears the rotation.
			if (not GetForceFinalRotationRegardlessOfReverse())
			{
				ResetRotateTowardsFinalMovementRotation();
			}
			return;
		}

		bWasLastMovementReverse = false;
	}
}

USkeletalMeshComponent* ATrackedTankMaster::GetTankMesh() const
{
	return ChassisMesh;
}


void ATrackedTankMaster::UpgradeTurnRate(const float NewTurnRate)
{
	if (TurnRate <= 0)
	{
		RTSFunctionLibrary::ReportError("Attempted to update tank turnrate with <= 0 value"
			"\n ATrackedTankMaster::UpdateTurnRate"
			"\n for tank: " + GetName());
		return;
	}
	if (IsValid(TrackPhysicsMovement))
	{
		TrackPhysicsMovement->UpdateTurnRate(NewTurnRate);
		TurnRate = NewTurnRate;
	}
}

void ATrackedTankMaster::OnRTSUnitSpawned(const bool bSetDisabled, const float TimeNotSelectable, const FVector MoveTo)
{
	if (not IsValid(ChassisMesh))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "Tank mesh", "ATrackedTankMaster");
		return;
	}
	if (bSetDisabled)
	{
		OnRTSUnitSpawned_SetDisabled();
	}
	else
	{
		OnRTSUnitSpawned_SetEnabled(TimeNotSelectable, MoveTo);
	}
}

void ATrackedTankMaster::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (UAudioComponent* Audio = FindComponentByClass<UAudioComponent>())
	{
		M_EngineSoundComponent = Audio;
		if (UWorld* World = GetWorld())
		{
			M_EngineSoundDel.BindUObject(this, &ATrackedTankMaster::UpdateEngineSound);
			World->GetTimerManager().SetTimer(M_EngineSoundHandle, M_EngineSoundDel,
			                                  DeveloperSettings::Optimization::UpdateEngineSoundsInterval, true);
		}
	}
	M_DigInComponent = FindComponentByClass<UDigInComponent>();
	M_AttachedRockets = FindComponentByClass<UAttachedRockets>();
	if (not GetIsValidDigInComponent())
	{
		return;
	}
	M_DigInComponent->SetupOwner(this);
}

void ATrackedTankMaster::BeginPlay()
{
	Super::BeginPlay();
	// Needs the proper Owning player to set which is why we cannot do this in post initialize components!
	BeginPlay_SetupExperienceComponent();
}

void ATrackedTankMaster::BeginDestroy()
{
	if (UWorld* World = GetWorld(); IsValid(World) && M_EngineSoundHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(M_EngineSoundHandle);
	}
	Super::BeginDestroy();
}

void ATrackedTankMaster::UnitDies(const ERTSDeathType DeathType)
{
	if (GetIsValidDigInComponent())
	{
		M_DigInComponent->OnOwningUnitDeath();
	}
	Super::UnitDies(DeathType);
}

void ATrackedTankMaster::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ATrackedTankMaster::OnUnitIdleAndNoNewCommands()
{
	// Important; calls the delegate in base ICommands.
	Super::OnUnitIdleAndNoNewCommands();
	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(true);
	}
}

void ATrackedTankMaster::SetupChassisMeshCollision()
{
	if (not IsValid(ChassisMesh))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "ChassisMesh",
		                                             "ATrackedTankMaster::SetupChassisMeshCollision");
		return;
	}
	if (IsValid(RTSComponent))
	{
		if (RTSComponent->GetOwningPlayer() == 1)
		{
			FRTS_CollisionSetup::SetupPlayerVehicleMovementMeshCollision(ChassisMesh);
		}
		else
		{
			FRTS_CollisionSetup::SetupEnemyVehicleMovementMeshCollision(ChassisMesh);
		}
	}
}

void ATrackedTankMaster::ExecuteHarvestResourceCommand(ACPPResourceMaster* TargetResource)
{
	// For harvesters; make sure we turn off overlap evasion.
	if (EnsureRTSOverlapEvasionComponent())
	{
		RTSOverlapEvasionComponent->SetOverlapEvasionEnabled(false);
	}
	Super::ExecuteHarvestResourceCommand(TargetResource);
}

void ATrackedTankMaster::TerminateHarvestResourceCommand()
{
	if (EnsureRTSOverlapEvasionComponent())
	{
		RTSOverlapEvasionComponent->SetOverlapEvasionEnabled(true);
	}
	Super::TerminateHarvestResourceCommand();
}

bool ATrackedTankMaster::EnsureRTSOverlapEvasionComponent() const
{
	if (not IsValid(RTSOverlapEvasionComponent))
	{
		return false;
	}
	return true;
}

URTSExperienceComp* ATrackedTankMaster::GetExperienceComponent() const
{
	return ExperienceComponent;
}

void ATrackedTankMaster::OnUnitLevelUp()
{
	BP_OnUnitLevelUp();
}

void ATrackedTankMaster::OnTankKilledAnyActor(AActor* KilledActor)
{
	// Call the base function for voice lines.
	Super::OnTankKilledAnyActor(KilledActor);
	// Exp interface function to collect (possible) experience from kill.
	IExpOnKilledActor(KilledActor);
}

void ATrackedTankMaster::InitTrackedTank(
	USkeletalMeshComponent* Mesh,
	const float NewTrackForceMultiplier,
	const float NewTurnRate,
	UChassisAnimInstance* TrackedAnimBP,
	const float TankCornerOffset,
	const float TankMeshZOffset, const float AngularDamping, const float LinearDamping)
{
	if (IsValid(Mesh))
	{
		ChassisMesh = Mesh;
		SetupChassisMeshCollision();
		Mesh->SetAngularDamping(AngularDamping);
		Mesh->SetLinearDamping(LinearDamping);
		if (EnsureRTSOverlapEvasionComponent())
		{
			RTSOverlapEvasionComponent->TrackOverlapMeshOfOwner(this, ChassisMesh);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "TankMesh",
		                                             "ATrackedTankMaster::InitTrackedTank");
	}
	if (IsValid(TrackedAnimBP))
	{
		TankAnimationBP = TrackedAnimBP;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this, "TrackedAnimBp", "ATrackedTankMaster::INitTrackedTank");
	}

	// The turn rate is also used by TankMaster to rotate towards a specific location.
	TurnRate = NewTurnRate;
	SetTankCornerOffset(TankCornerOffset);
	if (IsValid(TrackPhysicsMovement))
	{
		TrackPhysicsMovement->InitTrackPhysicsMovement(
			Mesh,
			NewTrackForceMultiplier,
			NewTurnRate,
			TrackedAnimBP,
			TankMeshZOffset);
	}
}

void ATrackedTankMaster::ExecuteMoveCommand(const FVector MoveToLocation)
{
	if (IsValid(AITankController))
	{
		AITankController->SetMoveToLocation(MoveToLocation);
		ExecuteCommandMoveBP(false);
		if (GetIsValidRTSNavCollision())
		{
			RTSNavCollision->EnableAffectNavmesh(false);
		}
		// To auto engage turrets.
		Super::ExecuteMoveCommand(MoveToLocation);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "AITankController",
		                                             "ATrackedTankMaster::ExecuteMoveCommand");
	}
}

void ATrackedTankMaster::TerminateMoveCommand()
{
	Super::TerminateMoveCommand();
	StopBehaviourTree();
	if (IsValid(TankAnimationBP))
	{
		// Stop all animations.
		TankAnimationBP->SetChassisAnimToIdle();
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "TankAnimationBP",
		                                                  "ATrackedTankMaster::TerminateMoveCommand()");
	}
	CheckFootPrintForOverlaps();
}

void ATrackedTankMaster::ExecuteReverseCommand(const FVector ReverseToLocation)
{
	if (!IsValid(AITankController))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this, "AITankController", "ATrackedTankMaster::ExecuteReverseCommand");
		return;
	}

	// 1) Provide the target to the AI controller (same as normal move)
	AITankController->SetMoveToLocation(ReverseToLocation);

	// 2) Force the vehicle path follower into reverse mode
	if (UPathFollowingComponent* PFC = AITankController->GetPathFollowingComponent())
	{
		if (UTrackPathFollowingComponent* TrackPFC = Cast<UTrackPathFollowingComponent>(PFC))
		{
			TrackPFC->SetReverse(true);
		}
		else
		{
			RTSFunctionLibrary::ReportFailedCastError(
				"PathFollowingComponent", "UTrackPathFollowingComponent",
				"ATrackedTankMaster::ExecuteReverseCommand");
		}
	}

	ExecuteCommandMoveBP(true);

	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(false);
	}

	Super::ExecuteReverseCommand(ReverseToLocation);
}

void ATrackedTankMaster::TerminateReverseCommand()
{
	// Reset reverse enforcement and stop movement/BT
	if (IsValid(AITankController))
	{
		if (UTrackPathFollowingComponent* TrackPFC =
			Cast<UTrackPathFollowingComponent>(AITankController->GetPathFollowingComponent()))
		{
			TrackPFC->SetReverse(false);
		}
		AITankController->StopMovement();
	}

	StopBehaviourTree();

	// Return chassis to idle, mirroring TerminateMoveCommand
	if (IsValid(TankAnimationBP))
	{
		TankAnimationBP->SetChassisAnimToIdle();
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(
			this, "TankAnimationBP", "ATrackedTankMaster::TerminateReverseCommand()");
	}

	Super::TerminateReverseCommand();
}


void ATrackedTankMaster::StopMovement()
{
	// todo kill forces.
	Super::StopMovement();
}

void ATrackedTankMaster::ExecuteAttackCommand(AActor* Target)
{
	// Set turrets to engage target.
	Super::ExecuteAttackCommand(Target);
}

void ATrackedTankMaster::ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand)
{
	// Rotation logic in TankMaster.
	Super::ExecuteRotateTowardsCommand(RotateToRotator, IsQueueCommand);
	if (IsValid(TankAnimationBP))
	{
		const bool bIsRotateToRight = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, RotateToRotator.Yaw) > 0;
		TankAnimationBP->SetChassisAnimToStationaryRotation(bIsRotateToRight);
	}
}

void ATrackedTankMaster::TerminateRotateTowardsCommand()
{
	if (IsValid(TankAnimationBP))
	{
		TankAnimationBP->SetChassisAnimToIdle();
	}
	Super::TerminateRotateTowardsCommand();
}

void ATrackedTankMaster::ExecuteDigIn()
{
	Super::ExecuteDigIn();
	if (not GetIsValidDigInComponent())
	{
		return;
	}
	M_DigInComponent->ExecuteDigInCommand();
}

void ATrackedTankMaster::TerminateDigIn()
{
	Super::TerminateDigIn();
	// Nothing to do here, the dig in command is completed so the tank is dug in now and the command queue
	// is free for attack commands.
}

void ATrackedTankMaster::ExecuteBreakCover()
{
	Super::ExecuteBreakCover();
	if (not GetIsValidDigInComponent())
	{
		return;
	}
	// Will call OnBreakCoverCompleted when the unit is movable again.
	M_DigInComponent->TerminateDigInCommand();
}

void ATrackedTankMaster::TerminateBreakCover()
{
	// cover is broken and unit is movable; no state to clean up.
}

void ATrackedTankMaster::ExecuteFireRockets()
{
	if (IsValid(M_AttachedRockets))
	{
		M_AttachedRockets->ExecuteFireAllRockets();
	}
}

void ATrackedTankMaster::TerminateFireRockets()
{
	if (IsValid(M_AttachedRockets))
	{
		M_AttachedRockets->TerminateFireAllRockets();
	}
}

void ATrackedTankMaster::ExecuteCancelFireRockets()
{
	if (IsValid(M_AttachedRockets))
	{
		M_AttachedRockets->CancelFireRockets();
	}
}

void ATrackedTankMaster::TerminateCancelFireRockets()
{
	Super::TerminateCancelFireRockets();
}

void ATrackedTankMaster::OnStartDigIn()
{
	if (not SwapAbility(EAbilityID::IdDigIn, EAbilityID::IdBreakCover))
	{
		RTSFunctionLibrary::ReportError("Failed to swap dig in ability"
			"\n At function: ATrackedTankMaster::OnStartDigIn"
			"\n For tank: " + GetName());
		return;
	}
	const bool bMoveAbilityRemoved = RemoveAbility(EAbilityID::IdMove);
	const bool bReverseMoveAbilityRemoved = RemoveAbility(EAbilityID::IdReverseMove);
	const bool bRotateTowardsAbilityRemoved = RemoveAbility(EAbilityID::IdRotateTowards);
	if (not(bMoveAbilityRemoved && bReverseMoveAbilityRemoved && bRotateTowardsAbilityRemoved))
	{
		RTSFunctionLibrary::ReportError("Failed to remove move, reverse move or rotate towards ability"
			"\n At function: ATrackedTankMaster::OnStartDigIn"
			"\n For tank: " + GetName());
		return;
	}
	SetTurretsDisabled();
}


void ATrackedTankMaster::OnDigInCompleted()
{
	// Re-enable the mounted weapons.
	SetTurretsToAutoEngage(true);

	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	if (GetIsSelected())
	{
		// If selected; Queue the voice line to play over radio.
		GetSpatialVoiceLinePlayer()->PlayVoiceLineOverRadio(ERTSVoiceLine::CommandCompleted, false, true);
	}
	else
	{
		// If not selected; play the voice line spatial. Stop any other spatial audio.
		GetSpatialVoiceLinePlayer()->ForcePlaySpatialVoiceLine(ERTSVoiceLine::CommandCompleted, GetActorLocation());
	}
}

void ATrackedTankMaster::OnBreakCoverCompleted()
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	// We want to put back the move ability at the index it used to be.
	const int32 MoveIndex = FRTS_Statics::GetIndexOfAbilityForBaseTank(RTSComponent->GetOwningPlayer(),
	                                                                   RTSComponent->GetSubtypeAsTankSubtype(),
	                                                                   EAbilityID::IdMove, this);
	AddAbility(EAbilityID::IdMove, MoveIndex);
	const int32 ReverseMoveIndex = FRTS_Statics::GetIndexOfAbilityForBaseTank(
		RTSComponent->GetOwningPlayer(), RTSComponent->GetSubtypeAsTankSubtype(), EAbilityID::IdReverseMove, this);
	AddAbility(EAbilityID::IdReverseMove, ReverseMoveIndex);
	const int32 RotateTowardsIndex = FRTS_Statics::GetIndexOfAbilityForBaseTank(
		RTSComponent->GetOwningPlayer(), RTSComponent->GetSubtypeAsTankSubtype(), EAbilityID::IdRotateTowards, this);
	AddAbility(EAbilityID::IdRotateTowards, RotateTowardsIndex);
	SwapAbility(EAbilityID::IdBreakCover, EAbilityID::IdDigIn);
}


void ATrackedTankMaster::WallGotDestroyedForceBreakCover()
{
	const ECommandQueueError Error = BreakCover(true);
	if (Error != ECommandQueueError::NoError)
	{
		const FString ErrorString = UEnum::GetValueAsString(Error);
		RTSFunctionLibrary::ReportError("Failed to break cover when wall got destroyed"
			"\n At function: ATrackedTankMaster::WallGotDestroyedForceBreakCover"
			"\n For tank: " + GetName());
	}
}


void ATrackedTankMaster::UpdateEngineSound() const
{
	if (IsValid(M_EngineSoundComponent) && IsValid(ChassisMesh))
	{
		const float Speed = ChassisMesh->GetBoneLinearVelocity(NAME_None).Length();
		M_EngineSoundComponent->SetFloatParameter(AudioSpeedParam, Speed);
	}
}

bool ATrackedTankMaster::EnsureValidExperienceComponent()
{
	if (IsValid(ExperienceComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Experience component is not valid"
		"\n At function: ATrackedTankMaster::EnsureValidExperienceComponent"
		"\n For tank: " + GetName());
	ExperienceComponent = NewObject<URTSExperienceComp>(this, "ExperienceComponent");
	return IsValid(ExperienceComponent);
}

bool ATrackedTankMaster::GetIsValidDigInComponent() const
{
	if (IsValid(M_DigInComponent))
	{
		return true;
	}

	// IMPORTANT: no error report here as some tanks do not use this component!!
	return false;
}

void ATrackedTankMaster::BeginPlay_SetupExperienceComponent()
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

void ATrackedTankMaster::OnRTSUnitSpawned_SetEnabled(const float TimeNotSelectable, const FVector MoveTo)
{
	this->SetActorHiddenInGame(false);
	this->SetActorEnableCollision(true);
	ChassisMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChassisMesh->SetSimulatePhysics(true);
	ChassisMesh->SetVisibility(true, false);
	// Calculate world rotation to the MoveTo location.
	FRotator FinalRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), MoveTo);
	MoveToLocation(MoveTo, true, FinalRotation);
	// Set after the move to command; this notifies that the unit has not received any commands from the player yet.
	SetIsSpawning(true);

	for (auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			EachTurret->SetAutoEngageTargets(false);
		}
	}
	BP_OnRTSUnitSpawned(false);
	if (TimeNotSelectable <= 0.f)
	{
		if (IsValid(SelectionComponent))
		{
			SelectionComponent->SetCanBeSelected(true);
		}
		return;
	}

	auto SetSelectableAfterDelay = [this]()
	{
		if (IsValid(SelectionComponent))
		{
			SelectionComponent->SetCanBeSelected(true);
		}
	};
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(M_OnSpawnedSelectionTimerHandle, SetSelectableAfterDelay,
		                                  TimeNotSelectable, false);
	}
}

void ATrackedTankMaster::OnRTSUnitSpawned_SetDisabled()
{
	this->SetActorHiddenInGame(true);
	this->SetActorEnableCollision(false);
	ChassisMesh->SetSimulatePhysics(false);
	if (IsValid(SelectionComponent))
	{
		SelectionComponent->SetCanBeSelected(false);
	}
	if (IsValid(RTSNavCollision))
	{
		RTSNavCollision->EnableAffectNavmesh(false);
	}
	for (auto EachTurret : Turrets)
	{
		if (CheckTurretIsValid(EachTurret))
		{
			EachTurret->DisableTurret();
		}
	}
	BP_OnRTSUnitSpawned(true);
}

void ATrackedTankMaster::CheckFootPrintForOverlaps() const
{
	if (EnsureRTSOverlapEvasionComponent())
	{
		RTSOverlapEvasionComponent->CheckFootprintForOverlaps();
	}
}

/*
* STEER WITH ANGULAR VELOCITY
* 	float OutLeftTrack = 0.f;
	float OutRightTrack = 0.f;

	UVehicleAIFunctionLibrary::ConvertToTankSteering(m_Steering, m_Throttle, m_TurnInPlaceThreshold, m_NormalTurnMultiplier, OutLeftTrack, OutRightTrack);

	float DesiredYawRate = (OutRightTrack - OutLeftTrack) * m_ThrottleMultiplier;

	// Clamp the DesiredYawRate within a certain range
	float MaxYawRate = 10.f; // Adjust this value to control the maximum angular momentum
	DesiredYawRate = FMath::Clamp(DesiredYawRate, -MaxYawRate, MaxYawRate);

	FVector ForwardVector = m_TankMesh->GetForwardVector();
	FVector TankVelocity = (OutLeftTrack + OutRightTrack) * 0.5f * m_ThrottleMultiplier * ForwardVector;

	if (m_Break > 0)
	{
		TankVelocity *= (1 - m_Break);
	}

	m_TankMesh->SetPhysicsLinearVelocity(TankVelocity, true);

	// Get the current angular velocity
	FVector CurrentAngularVelocity = m_TankMesh->GetPhysicsAngularVelocityInDegrees();
	FVector TargetAngularVelocity = FVector(0, 0, DesiredYawRate);

	// Smoothly dampen the angular momentum as the m_Steering value approaches zero
	float DampingFactor = 5.f; // Adjust this value to control the rate at which the angular momentum dampens
	FVector NewAngularVelocity = FMath::Lerp(CurrentAngularVelocity, TargetAngularVelocity, DeltaSeconds * DampingFactor);

	m_TankMesh->SetPhysicsAngularVelocityInDegrees(NewAngularVelocity, false);
 */

// struct item
// {
// 	const int index;
// 	const float weight;
// };
// static bool CheckConstrains(TPair<float, TArray<item>> CurrentItemSet, const float MaxWeights, const item NewItem)
// {
// 	for(auto elm : CurrentItemSet.Value)
// 	{
// 		if(FMath::Abs(elm.index - NewItem.index) <= 1)
// 		{
// 			return false;
// 		}
// 	}
// 	if(CurrentItemSet.Key + NewItem.weight > MaxWeights)
// 	{
// 		return false;
// 	}
// 	return true;
// }
//
// static TPair<float, TArray<item>> Exhaustive(TArray<item> SuperSetItems, const float MaxWeights)
// {
// 	// valid sets of pairs.
// 	TArray<TPair<float, TArray<item>>> validSets;
// 	// subsets possible of superset.
// 	TArray<TArray<item>> subsets;
// 	
// 	// Generate all possible subsets we want to exhaust.
// 	int numElements = SuperSetItems.Num();
// 	for (int i = 0; i < (1 << numElements); i++)
// 	{
// 		TArray<item> subset;
// 		for (int j = 0; j < numElements; j++)
// 		{
// 			if ((i & (1 << j)) != 0)
// 			{
// 				subset.Add(SuperSetItems[j]);
// 			}
// 		}
// 		subsets.Add(subset);
// 		
// 	}
// }
