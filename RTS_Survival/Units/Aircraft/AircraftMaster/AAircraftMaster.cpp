#include "AAircraftMaster.h"

#include "RTS_Survival/Units/Aircraft/AircaftHelpers/FRTSAircraftHelpers.h"
#include "RTS_Survival/Units/Aircraft/AircraftMovement/AircraftMovement.h"
#include "Components/DecalComponent.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftWeapon.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/UnitData/AircraftData.h"
#include "RTS_Survival/Units/Aircraft/AircraftAnimInstance/AircraftAnimInstance.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"
#include "Serialization/AsyncPackageLoader.h"

const float VerticalTextAircraftZOffset = 150.f;

AAircraftMaster::AAircraftMaster(const FObjectInitializer& ObjectInitializer)
	: ASelectablePawnMaster(ObjectInitializer)
{
	M_AircraftMovement = CreateDefaultSubobject<UAircraftMovement>(TEXT("AircraftMovement"));
	ExperienceComponent = CreateDefaultSubobject<URTSExperienceComp>(TEXT("ExperienceComponent"));
	M_AircraftMoveData.Path.Owner = this;
	M_AircraftAttackData.Path.Owner = this;
	M_AircraftWeapon = CreateDefaultSubobject<UAircraftWeapon>(TEXT("AircraftWeapon"));
}


void AAircraftMaster::OnRTSUnitSpawned(const bool bSetDisabled, const float TimeNotSelectable, const FVector MoveTo)
{
	if (bSetDisabled)
	{
		OnRTSUnitSpawned_SetDisabled();
	}
	else
	{
		OnRTSUnitSpawned_SetEnabled(TimeNotSelectable);
	}
}

void AAircraftMaster::BeginPlay()
{
	M_AircraftMoveData.Path.Owner = this;
	M_AircraftAttackData.Path.Owner = this;
	Super::BeginPlay();
	// Will set the subtype.
	BP_BeginplayInitAircraft();
	// After the bp has set the type.
	BeginPlay_InitAircraft();
	// Make sure the aircraft weapon knows if we are airborne or landed.
	BeginPlay_PropagateStateToWpAndAnimInst();
}

void AAircraftMaster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReloadManager.OnAircraftDied(this);
	if (M_AircraftOwner.IsValid())
	{
		// idempotent.
		M_AircraftOwner->RemoveAircraftFromBuilding_CloseBayDoors(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AAircraftMaster::BeginDestroy()
{
	Super::BeginDestroy();
	const UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(M_AircraftLandedData.Vto_PrepareHandle);
	}
}

void AAircraftMaster::PostInitializeComponents()
{
	// Will setup selection component and fow component.
	Super::PostInitializeComponents();
	M_AircraftMesh = FindComponentByClass<USkeletalMeshComponent>();

	if (M_AircraftMovementSettings.bM_StartedAsLanded)
	{
		// Do not use the function as we wait for references to be set before propagating the state.
		/** @See BeginPlay_PropagateStateToWpAndAnimInst */
		M_LandedState = EAircraftLandingState::Landed;
	}
	else
	{
		// Do not use the function as we wait for references to be set before propagating the state.
		/** @See BeginPlay_PropagateStateToWpAndAnimInst */
		M_LandedState = EAircraftLandingState::Airborne;
		M_AircraftLandedData.TargetAirborneHeight = GetActorLocation().Z;
	}

	// was: M_MovementState = EAircraftMovementState::Idle;
	SetMovementToIdle();

	PostInit_InitWeaponInitAnim();
	if (IsValid(M_AnimInstAircraft))
	{
		M_AnimInstAircraft->SetMaxMovementSpeed(M_AircraftMovementSettings.MaxMoveSpeed);
	}
}


void AAircraftMaster::InitAircraft(UDecalComponent* SelectionDecal)
{
	M_SelectionDecal = SelectionDecal;
	if (not IsValid(SelectionDecal))
	{
		RTSFunctionLibrary::ReportError("SelectionDecal is not valid on " + GetName() +
			". Ensure to provdie a valid on for AircraftMaster::InitAircraft.");
		return;
	}
}

void AAircraftMaster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (not M_AnimInstAircraft || not M_AircraftMovement)
	{
		return;
	}
	Tick_ProjectDecalToLandscape(DeltaTime);
	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		if (M_AircraftMovementSettings.bDebugState)
		{
			const FString LandedStateStr = FRTSAircraftHelpers::GetAircraftLandingStateString(M_LandedState);
			const FString MovementStateStr = FRTSAircraftHelpers::GetAircraftMovementStateString(M_MovementState);
			const FString DebugStr = FString::Printf(TEXT("Landed: %s | Move: %s"), *LandedStateStr, *MovementStateStr);
			FRTSAircraftHelpers::AircraftDebugAtLocation(
				this,
				DebugStr,
				GetActorLocation() + FVector(0.f, 0.f, 400.f),
				FColor::White,
				DeltaTime);
			if (M_PendingPostLiftOffAction.GetAction() != EPostLiftOffAction::Idle)
			{
				const FString PostLiftOffStr = "PLO action:" +
					FRTSAircraftHelpers::GetAircraftPostLifOffString(M_PendingPostLiftOffAction.GetAction());

				FRTSAircraftHelpers::AircraftDebugAtLocation(
					this,
					PostLiftOffStr,
					GetActorLocation() + FVector(0.f, 0.f, 460.f),
					FColor::White,
					DeltaTime);
			}
			if (GetIsValidAircraftOwner())
			{
				const int32 MyIndex = M_AircraftOwner->GetAssignedSocketIndex(this);
				const FString MyIndexString = MyIndex == INDEX_NONE ? "NONE" : FString::FromInt(MyIndex);
				const FString OwnerStr = "Owner: " + M_AircraftOwner->GetOwner()->GetName() + " (Socket: " + MyIndexString +
					")";
				FRTSAircraftHelpers::AircraftDebugAtLocation(
					this,
					OwnerStr,
					GetActorLocation() + FVector(0.f, 0.f, 520.f),
					FColor::White,
					DeltaTime);
			}
			if (M_AnimInstAircraft)
			{
				const float PropSpeed = M_AnimInstAircraft->GetAnimPropSpeed();
				const FString PropStr = FString::Printf(TEXT("Prop Speed: %.1f"), PropSpeed);
				FRTSAircraftHelpers::AircraftDebugAtLocation(
					this,
					PropStr,
					GetActorLocation() + FVector(0.f, 0.f, 580.f),
					FColor::White,
					DeltaTime);
			}
		}
	}
	M_AnimInstAircraft->UpdateAnim(GetActorRotation(), GetVelocity().Size());

	if (M_LandedState == EAircraftLandingState::VerticalTakeOff)
	{
		Tick_VerticalTakeOff(DeltaTime);
		return;
	}
	if (M_LandedState == EAircraftLandingState::VerticalLanding)
	{
		Tick_VerticalLanding(DeltaTime);
		return;
	}
	if (M_LandedState == EAircraftLandingState::WaitForBayToOpen)
	{
		Tick_WaitingForBayOpening(DeltaTime);
		return;
	}
	if (M_MovementState == EAircraftMovementState::MovingTo)
	{
		Tick_MoveTo(DeltaTime);
		return;
	}
	if (M_MovementState == EAircraftMovementState::AttackMove)
	{
		Tick_AttackMove(DeltaTime);
		return;
	}
	if (M_LandedState == EAircraftLandingState::Airborne)
	{
		if (IsIdleHover())
		{
			// Hovering aircraft does not move, so we do not need to tick the movement component.
			return;
		}
		Tick_IdleCircling(DeltaTime);
	}
}

void AAircraftMaster::ExecuteMoveCommand(const FVector MoveToLocation)
{
	Super::ExecuteMoveCommand(MoveToLocation);

	const FVector FlightTarget(
		MoveToLocation.X,
		MoveToLocation.Y,
		GetMoveToZValue()
	);
	M_AircraftMoveData.SetTargetPoint(FlightTarget);

	if (M_LandedState != EAircraftLandingState::Airborne)
	{
		M_PendingPostLiftOffAction.Set(EPostLiftOffAction::Move);
		if (M_LandedState == EAircraftLandingState::Landed)
		{
			TakeOffFromGroundOrOwner();
		}
		return;
	}
	StartMoveTo();
}


void AAircraftMaster::TerminateMoveCommand()
{
	Super::TerminateMoveCommand();

	// Cancel any queued move
	M_AircraftMoveData.Reset();
	M_PendingPostLiftOffAction.Reset();

	// If we were already en route, abort and go back to idle
	if (M_MovementState == EAircraftMovementState::MovingTo)
	{
		CleanUpMoveAndSwitchToIdle();
	}
}

void AAircraftMaster::ExecuteAttackCommand(AActor* TargetActor)
{
	Super::ExecuteAttackCommand(TargetActor);
	M_AircraftAttackData.TargetActor = TargetActor;

	if (M_LandedState != EAircraftLandingState::Airborne)
	{
		M_PendingPostLiftOffAction.Set(EPostLiftOffAction::Attack);
		if (M_LandedState == EAircraftLandingState::Landed)
		{
			TakeOffFromGroundOrOwner();
		}
		return;
	}

	StartAttackActor();
}


void AAircraftMaster::TerminateAttackCommand()
{
	// Cancel any queued attack 
	M_AircraftAttackData.Reset();
	M_PendingPostLiftOffAction.Reset();

	// If we were already attacking, abort and go back to idle
	if (M_MovementState == EAircraftMovementState::AttackMove)
	{
		CleanUpAttackAndSwitchToIdle();
	}
}

void AAircraftMaster::ExecuteReturnToBase()
{
	Super::ExecuteReturnToBase();

	// If we’re in the “prep takeoff” window, cancel it and stay landed.
	if (IsPreparingVTO())
	{
		CancelPrepareVTO_StayLanded();
		DoneExecutingCommand(EAbilityID::IdReturnToBase);
		return;
	}

	// Only check for the landed enum as landing and takeOff are handled by the terminate commands.
	if (ExecuteReturnToBase_IsAlreadyLanded())
	{
		DoneExecutingCommand(EAbilityID::IdReturnToBase);
		return;
	}
	if (ExecuteReturnToBase_IsVto())
	{
		StartVerticalLanding();
		return;
	}


	// Normal RTB flow (navigate to bay air-position, then request landing)
	StartNavigatingToBase();
}


void AAircraftMaster::TakeOffFromGroundOrOwner()
{
	bool bCanLiftOffFromOwner = false;
	// If we are assigned to an airbase owner, request VTO from it; it will open our bay
	// and call StartVTO() when safe to lift off.
	if (GetIsValidAircraftOwner())
	{
		bCanLiftOffFromOwner = M_AircraftOwner->OnAircraftRequestVTO(this);
	}
	if (bCanLiftOffFromOwner)
	{
		// IF we lift off through an owner then we wait for the bays to open and trigger the VTO.
		return;
	}

	// Orphan aircraft: do a direct VTO
	StartVto();
}

UAircraftOwnerComp* AAircraftMaster::GetOwnerAircraft() const
{
	if (M_AircraftOwner.IsValid())
	{
		return M_AircraftOwner.Get();
	}
	return nullptr;
}

TArray<UWeaponState*> AAircraftMaster::GetAllAircraftWeapons() const
{
	TArray<UWeaponState*> MyValidWeapons = {};
	if (EnsureAircraftWeaponIsValid())
	{
		for (auto EachWeapon : M_AircraftWeapon->GetWeapons())
		{
			if (not IsValid(EachWeapon))
			{
				continue;
			}
			MyValidWeapons.Add(EachWeapon);
		}
	}
	return MyValidWeapons;
}

UBombComponent* AAircraftMaster::GetBombComponent() const
{
	return M_AircraftBombComponent;
}

void AAircraftMaster::TerminateReturnToBase()
{
	Super::TerminateReturnToBase();

	TerminateReturnToBase_CheckLandedAndMovementState();

	if (M_LandedState == EAircraftLandingState::VerticalLanding)
	{
		// We are still landing while the return was cancelled; set to lif off immediately.
		CancelReturnToBase_VtoBackToAirborne();
	}

	// If our current Move-To was the RTB navigation, abort it cleanly
	if (M_AircraftReturnData.bNavigatingToSocket && M_MovementState == EAircraftMovementState::MovingTo)
	{
		// Only clear movement state to idle if we did not yet land.
		CleanUpMoveAndSwitchToIdle();
	}
	if (M_LandedState == EAircraftLandingState::WaitForBayToOpen)
	{
		// Ensure that the owner cancels pending landing for us.
		if (M_AircraftOwner.IsValid())
		{
			M_AircraftOwner->CancelPendingLandingFor(this, /*bCloseDoor=*/true);
		}
		// Start idle circling. Make sure the landed state is set back to airborne. (no more waiting for bay doors).
		UpdateLandedState(EAircraftLandingState::Airborne);
		SetMovementToIdle();
	}
	M_AircraftMoveData.Reset();

	// Clear any queued post-VTO action that might have been set for RTB
	M_PendingPostLiftOffAction.Reset();

	// Drop RTB state
	M_AircraftReturnData.Reset();
}


// ---------- Change ownership ----------

void AAircraftMaster::SetOwnerAircraft(UAircraftOwnerComp* NewOwner)
{
	// Early out if unchanged
	if (M_AircraftOwner.Get() == NewOwner)
	{
		return;
	}

	// If we had an old owner, make sure their map is clean (they already do that defensively).
	M_AircraftOwner = NewOwner;
	const EAbilityID PreviousOwnerAbility = HasAbility(EAbilityID::IdReturnToBase)
		                                        ? EAbilityID::IdReturnToBase
		                                        : EAbilityID::IdAircraftOwnerNotExpanded;
	EAbilityID NewOwnerAbility = EAbilityID::IdAircraftOwnerNotExpanded;
	if (M_AircraftOwner.IsValid())
	{
		NewOwnerAbility = EAbilityID::IdReturnToBase;
	}
	SwapAbility(PreviousOwnerAbility, NewOwnerAbility);
}

// ---------- Extracted VTO ----------

void AAircraftMaster::StartVto()
{
	if (not EnsureAircraftMovementIsValid())
	{
		return;
	}
	// Don’t recompute apex mid-VTO / bay-wait
	if (M_LandedState == EAircraftLandingState::VerticalTakeOff ||
		M_LandedState == EAircraftLandingState::WaitForBayToOpen)
	{
		RTSFunctionLibrary::ReportWarning(
			"Attempted to call StartVTO on an aircraft already taking off or waiting for bay to open."
			"\n aircraft name: " + GetName());
		return;
	}

	// Prepare takeoff apex & timing
	M_AircraftLandedData.LandedPosition = GetActorLocation();
	const float Apex = M_AircraftMovementSettings.TakeOffHeight;
	const float GroundZ = M_AircraftLandedData.LandedPosition.Z;

	M_AircraftLandedData.TargetAirborneHeight = GroundZ + Apex;
	M_AircraftLandedData.LandedRotation = GetActorRotation();

	// Kick the usual prepare timer path
	const float PrepTime = M_AircraftMovementSettings.VtoPrepareTime;
	SetVtoTimer(PrepTime);

	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		const FVector TargetLocation = FVector(GetActorLocation().X, GetActorLocation().Y, GroundZ + Apex);
		DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 12, FColor::Cyan, false, 20.f, 0, 1.f);
	}
}

void AAircraftMaster::ChangeOwnerForAircraft(UAircraftOwnerComp* NewOwner)
{
	if (not EnsureValidChangeOwnerRequest(NewOwner))
	{
		return;
	}
	if (M_AircraftOwner.IsValid())
	{
		ChangeOwner_FirstClearOldOwner(NewOwner);
		return;
	}
	// No previous owner data to clear.
	// Reset the aircraft to idle.
	SetUnitToIdle();
	if (M_LandedState == EAircraftLandingState::Landed)
	{
		// Grounded: start VTO and switch owner after lift-off.
		M_PendingPostLiftOffAction.Set(EPostLiftOffAction::ChangeOwner, NewOwner);
		StartVto();
		return;
	}
	// already VTO from previous action or VTO from SetUnitToIdle while performing a landing?
	if (M_LandedState == EAircraftLandingState::VerticalTakeOff ||
		M_LandedState == EAircraftLandingState::WaitForBayToOpen)
	{
		// Already taking off; just set the new owner to switch to after lift-off.
		M_PendingPostLiftOffAction.Set(EPostLiftOffAction::ChangeOwner, NewOwner);
		return;
	}

	// This also sets the pointer to the new owner on this aircraft.
	const bool bChangeOwnerSuccess = NewOwner->OnAircraftChangesToThisOwner(this);
	if (not bChangeOwnerSuccess)
	{
		FRTSAircraftHelpers::AircraftDebug("Failed to change the owner for aircraft: " + GetName() +
			" to new owner: " + NewOwner->GetName() + "\n setting to idle movement.");
		SetMovementToIdle();
		return;
	}
	// Success; update landing Position.
	UpdateLandingPositionForNewOwner(NewOwner);
	// Force move to new owner.
	ReturnToBase(true);
}

void AAircraftMaster::OnOwnerDies(const UAircraftOwnerComp* DeadOrInvalidOwner)
{
	if (!DeadOrInvalidOwner)
	{
		RTSFunctionLibrary::ReportError("OnOwnerDies called with null owner on " + GetName());
		return;
	}
	if (!M_AircraftOwner.IsValid() || M_AircraftOwner.Get() != DeadOrInvalidOwner)
	{
		const FString Curr = M_AircraftOwner.IsValid() ? M_AircraftOwner->GetName() : FString("None");
		RTSFunctionLibrary::ReportError("OnOwnerDies non-matching owner on " + GetName()
			+ " (DeadOwner=" + DeadOrInvalidOwner->GetName() + ", CurrentOwner=" + Curr + ")");
		return;
	}

	// Clear pending VTO timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_AircraftLandedData.Vto_PrepareHandle);
	}

	const EAircraftLandingState State = M_LandedState;
	const bool bAirborneOrWaiting =
		(State == EAircraftLandingState::Airborne || State == EAircraftLandingState::WaitForBayToOpen);

	// Clear the owner pointer right away to avoid any re-entrant duplicate notifications.
	ClearOwnerAircraft();

	if (bAirborneOrWaiting)
	{
		OnOwnerDies_ResetToIdleAirborne();
		return;
	}

	Destroy();
}

void AAircraftMaster::OnOwnerStartPackingUp(const UAircraftOwnerComp* OwnerPackingUp)
{
	if (!OwnerPackingUp)
	{
		RTSFunctionLibrary::ReportError("OnOwnerStartPackingUp called with null owner on " + GetName());
		return;
	}
	if (!M_AircraftOwner.IsValid() || M_AircraftOwner.Get() != OwnerPackingUp)
	{
		const FString Curr = M_AircraftOwner.IsValid() ? M_AircraftOwner->GetName() : FString("NOTVALID");
		RTSFunctionLibrary::ReportError("OnOwnerStartPackingUp non-matching owner on " + GetName()
			+ " (OwnerPackingUp=" + OwnerPackingUp->GetName() + ", CurrentOwner=" + Curr + ")");
		return;
	}
	ClearOwnerAircraft();
	switch (M_LandedState)
	{
	case EAircraftLandingState::None:
		RTSFunctionLibrary::ReportError("landed state none! for aircraft at OnOwnerStartPackingUp");
		return;
	case EAircraftLandingState::Landed:
		// No post lift off action; the aircraft will entry idle when lift off is complete.
		M_PendingPostLiftOffAction.Reset();
		StartVto();
		break;
	case EAircraftLandingState::VerticalTakeOff:
		// To terminate return to base. Which will VTO back to airborne.
		SetUnitToIdle();
		break;
	case EAircraftLandingState::Airborne:
		break;
	case EAircraftLandingState::WaitForBayToOpen:
		// No post lift off action; the aircraft will entry idle when lift off is complete.
		// Could have been set to changed owner for which we are now performing the landing.
		M_PendingPostLiftOffAction.Reset();
	// To terminate return to base. Which will VTO back to airborne.
		SetUnitToIdle();
		break;
	case EAircraftLandingState::VerticalLanding:
		// No post lift off action; the aircraft will entry idle when lift off is complete.
		// Could have been set to changed owner for which we are now performing the landing.
		M_PendingPostLiftOffAction.Reset();
	// To terminate return to base. Which will VTO back to airborne.
		SetUnitToIdle();
		break;
	}
}


// ---------- Return-to-base flow ----------


void AAircraftMaster::StartNavigatingToBase()
{
	if (not GetIsValidAircraftOwner() || not M_AircraftOwner->IsAircraftAllowedToLandHere(this))
	{
		// No owner -> nothing to return to
		DoneExecutingCommand(EAbilityID::IdReturnToBase);
		return;
	}

	// (Re)read socket index after potential assignment
	const int32 SocketIndex = M_AircraftOwner->GetAssignedSocketIndex(this);
	if (SocketIndex == INDEX_NONE)
	{
		DoneExecutingCommand(EAbilityID::IdReturnToBase);
		return;
	}

	// Cache RTB state (do NOT set bWaitingForDoor yet — we haven’t asked to open)
	M_AircraftReturnData.TargetOwner = M_AircraftOwner;
	M_AircraftReturnData.SocketIndex = SocketIndex;
	M_AircraftReturnData.bNavigatingToSocket = true;

	// Compute the *air* position above our bay (XY from socket, Z = our cruise/airborne height)
	FVector SocketGround;
	if (not M_AircraftOwner->GetSocketWorldLocation(SocketIndex, SocketGround))
	{
		DoneExecutingCommand(EAbilityID::IdReturnToBase);
		return;
	}
	const FVector FlightTarget(SocketGround.X, SocketGround.Y, GetMoveToZValue());

	// Drive the regular Move-To pipeline with this target
	M_AircraftMoveData.SetTargetPoint(FlightTarget);

	StartMoveTo();
}


void AAircraftMaster::StartVerticalLanding()
{
	if (not EnsureAircraftMovementIsValid() || not EnsureAnimInstanceIsValid())
	{
		return;
	}
	M_AircraftMovement->KillMovement();
	const FVector StartLocation = FVector(M_AircraftLandedData.LandedPosition.X, M_AircraftLandedData.LandedPosition.Y,
	                                      GetActorLocation().Z);
	const FRotator StartRotation = M_AircraftLandedData.LandedRotation;
	SetActorTransform(FTransform(StartRotation, StartLocation, GetActorScale3D()));
	// Start landing animation and timed vertical descent.
	const float LandingTime = ComputeTimeToAirborne(M_AircraftLandedData.LandedPosition.Z, GetActorLocation().Z);
	M_AnimInstAircraft->OnStartLanding(LandingTime);
	// Starts the decent in the tick function.
	UpdateLandedState(EAircraftLandingState::VerticalLanding);
}


URTSExperienceComp* AAircraftMaster::GetExperienceComponent() const
{
	return ExperienceComponent;
}

void AAircraftMaster::OnUnitLevelUp()
{
	BP_OnUnitLevelUp();
}

void AAircraftMaster::OnAnyActorKilledByAircraft(AActor* KilledActor)
{
	IExpOnKilledActor(KilledActor);
}

void AAircraftMaster::OnVtoCompleted(const float TargetZ)
{
	if (not EnsureAnimInstanceIsValid())
	{
		return;
	}
	M_AnimInstAircraft->OnVtoComplete();
	UpdateLandedState(EAircraftLandingState::Airborne);
	SetActorLocation({GetActorLocation().X, GetActorLocation().Y, TargetZ});
	M_AircraftMovement->OnVerticalTakeOffCompleted();

	FRTSAircraftHelpers::AircraftDebug(TEXT("Vertical Takeoff Complete"), FColor::Green);
	IssuePostLifOffAction();
	BP_OnVtoCompleted();
}

void AAircraftMaster::PostInit_InitWeaponInitAnim()
{
	if (not IsValid(M_AircraftMesh))
	{
		return;
	}
	const auto Instance = M_AircraftMesh->GetAnimInstance();
	if (not Instance)
	{
		RTSFunctionLibrary::ReportError("Invalid anim instance on aircraft");
		return;
	}
	UAircraftAnimInstance* AircraftAnimInst = Cast<UAircraftAnimInstance>(Instance);
	M_AnimInstAircraft = AircraftAnimInst;
	if (EnsureAnimInstanceIsValid())
	{
		M_AnimInstAircraft->Init(this);
	}
	if (not EnsureAircraftWeaponIsValid())
	{
		return;
	}
	M_AircraftWeapon->InitAircraftWeaponComponent(this, M_AircraftMesh);
	// Find bomb component
	M_AircraftBombComponent = FindComponentByClass<UBombComponent>();
}

void AAircraftMaster::UpdateLandedState(const EAircraftLandingState NewState)
{
	M_LandedState = NewState;
	UpdateAircraftWeaponWithLandedState();
	UpdateAircraftAnimationWithLandedState();
}


void AAircraftMaster::CleanUpMoveAndSwitchToIdle()
{
	SetMovementToIdle();
	FRTSAircraftHelpers::AircraftDebug(TEXT("Move-To Complete"), FColor::Yellow);
}

void AAircraftMaster::OnMoveCompleted()
{
	// If the finished Move-To was our RTB navigation, handle that flow instead of completing a Move command.
	if (M_AircraftReturnData.TargetOwner.IsValid() && M_AircraftReturnData.bNavigatingToSocket)
	{
		OnReturnToBase_MoveCompleted();
		return;
	}

	// Normal Move command completion 
	DoneExecutingCommand(EAbilityID::IdMove);
}


void AAircraftMaster::StartMoveTo()
{
	M_AircraftMoveData.Path.Owner = this;
	M_MovementState = EAircraftMovementState::MovingTo;
	FRotator StartRotation;
	const FVector StartLocation = OnMoveStartGetLocationAndRotation(StartRotation, GetActorLocation(),
	                                                                GetActorRotation());
	M_AircraftMoveData.StartPathFinding(StartLocation, StartRotation,
	                                    M_AircraftMovementSettings.BezierCurveSettings,
	                                    M_AircraftMovementSettings.DeadZoneSettings);

	M_AircraftMovement->MaxSpeed = M_AircraftMovementSettings.MaxMoveSpeed;
	M_AircraftMovement->Acceleration = M_AircraftMovementSettings.Acceleration;
	M_AircraftMovement->Deceleration = M_AircraftMovementSettings.Deceleration;

	FRTSAircraftHelpers::AircraftDebug(TEXT("Beginning Move-To via Spline"), FColor::Cyan);
}

FVector AAircraftMaster::OnMoveStartGetLocationAndRotation(
	FRotator& OutRotation,
	const FVector& CurrentLocation,
	const FRotator& CurrentRotation)
{
	if (FMath::IsNearlyZero(M_AircraftLandedData.TargetAirborneHeight))
	{
		RTSFunctionLibrary::ReportError(
			"Target Airborne Height is zero in AAircraftMaster::OnMoveStartGetLocationAndRotation. "
			"Ensure to set the target airborne height before calling this function.");
		M_AircraftLandedData.TargetAirborneHeight = M_AircraftMovementSettings.TakeOffHeight;
	}
	const float DeltaZ = M_AircraftLandedData.TargetAirborneHeight - CurrentLocation.Z;
	FVector OutLocation;

	if (FMath::Abs(DeltaZ) > 1.f)
	{
		// --- Height recovery path ---

		// Use IdleBankAngle as the recovery pitch magnitude.
		// Clamp to a safe range so tan() is well-behaved.
		const float MinPitchDeg = 1.f; // avoid tan(0)
		const float MaxPitchDeg = 85.f; // avoid near-vertical
		const float RecoveryPitchAbs =
			FMath::Clamp(FMath::Abs(M_AircraftMovementSettings.IdleBankAngle), MinPitchDeg, MaxPitchDeg);

		// tan(theta) = |DeltaZ| / HorizontalDistance  ->  HorizontalDistance = |DeltaZ| / tan(theta)
		const float PitchRad = FMath::DegreesToRadians(RecoveryPitchAbs);
		const float TanPitch = FMath::Max(FMath::Tan(PitchRad), KINDA_SMALL_NUMBER);
		const float HorizontalDistance = FMath::Abs(DeltaZ) / TanPitch;

		// Decide which global axis we travel along (X or Y) based on which one the yaw is most aligned with.
		const float YawRad = FMath::DegreesToRadians(CurrentRotation.Yaw);
		const float CosYaw = FMath::Cos(YawRad); // alignment with +X
		const float SinYaw = FMath::Sin(YawRad); // alignment with +Y

		OutLocation = CurrentLocation;

		if (FMath::Abs(CosYaw) >= FMath::Abs(SinYaw))
		{
			// Move purely along world X. Sign matches facing (forward/back along X).
			const float DirX = FMath::Sign(CosYaw);
			OutLocation.X += DirX * HorizontalDistance;
		}
		else
		{
			// Move purely along world Y. Sign matches facing (forward/back along Y).
			const float DirY = FMath::Sign(SinYaw);
			OutLocation.Y += DirY * HorizontalDistance;
		}

		// Snap Z to target height.
		OutLocation.Z = M_AircraftLandedData.TargetAirborneHeight;

		// Keep yaw, zero roll, apply pitch that points toward the target height.
		// Note: In UE, negative pitch looks "up" and positive looks "down".
		OutRotation = CurrentRotation;
		OutRotation.Roll = 0.f;
		OutRotation.Pitch = (DeltaZ > 0.f) ? -RecoveryPitchAbs : RecoveryPitchAbs;

		return OutLocation;
	}
	OutLocation = CurrentLocation;
	OutLocation.Z = M_AircraftLandedData.TargetAirborneHeight;
	OutRotation = CurrentRotation;
	OutRotation.Pitch = 0.f;
	return OutLocation;
}

void AAircraftMaster::StartAttackActor()
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	if (not M_AircraftAttackData.IsTargetActorVisible(RTSComponent->GetOwningPlayer()))
	{
		DoneExecutingCommand(EAbilityID::IdAttack);
		return;
	}

	FRotator StartRotation;
	const FVector StartLocation = OnMoveStartGetLocationAndRotation(StartRotation, GetActorLocation(),
	                                                                GetActorRotation());
	M_MovementState = EAircraftMovementState::AttackMove;
	M_AircraftMovement->MaxSpeed = M_AircraftMovementSettings.MaxMoveSpeed;
	M_AircraftMovement->Acceleration = M_AircraftMovementSettings.Acceleration;
	M_AircraftMovement->Deceleration = M_AircraftMovementSettings.Deceleration;
	M_AircraftAttackData.StartPathFinding(StartLocation, StartRotation,
	                                      M_AircraftMovementSettings.BezierCurveSettings,
	                                      M_AircraftMovementSettings.AttackMoveSettings,
	                                      M_AircraftMovementSettings.DeadZoneSettings);
	constexpr int32 TargetPointIndex = 1;
	if (M_AircraftAttackData.Path.PathPoints.IsValidIndex(TargetPointIndex))
	{
		AttackMove_IssueActionForNewTargetPoint(M_AircraftAttackData.Path.PathPoints[TargetPointIndex]);
	}
}


void AAircraftMaster::IssuePostLifOffAction()
{
	switch (M_PendingPostLiftOffAction.GetAction())
	{
	case EPostLiftOffAction::Move:
		StartMoveTo();
		break;

	case EPostLiftOffAction::Attack:
		StartAttackActor();
		break;

	case EPostLiftOffAction::ChangeOwner:
		PostLiftOff_ChangeOwner();
		break;

	default:
		SetMovementToIdle();
		break;
	}
	M_PendingPostLiftOffAction.Reset();
}


void AAircraftMaster::Tick_ProjectDecalToLandscape(const float DeltaTime)
{
	if (not EnsureSelectionDecalIsValid() || not GetWorld())
	{
		return;
	}
	const float DeltaZ = M_AircraftMovementSettings.TakeOffHeight * 2;
	const FVector CurrentLocation = GetActorLocation();
	const FVector ProjectionLocation = FVector(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z - DeltaZ);
	// Line trace to the ECC_Landscape to find the ground below the aircraft.
	FCollisionQueryParams TraceParams(FName(TEXT("SelectionDecalTrace")), false, nullptr);
	TraceParams.bReturnPhysicalMaterial = false;

	// Define the delegate using a lambda
	FTraceDelegate TraceDelegate;
	TWeakObjectPtr<AAircraftMaster> WeakThis(this);
	auto OnAsyncTrace = [WeakThis](const FTraceHandle& TraceHandle,
	                               FTraceDatum& TraceDatum)-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		if (TraceDatum.OutHits.IsEmpty())
		{
			FRTSAircraftHelpers::AircraftDebug("Could not hit landscape when tracing for selection decal position!"
				"trace datum (async trace) is empty.");
			return;
		}
		WeakThis->OnLandscapeTraceHit(TraceDatum.OutHits[0]);
	};
	TraceDelegate.BindLambda(OnAsyncTrace);

	// Perform the async trace
	GetWorld()->AsyncLineTraceByChannel(
		EAsyncTraceType::Single,
		CurrentLocation,
		ProjectionLocation,
		COLLISION_TRACE_LANDSCAPE,
		TraceParams,
		FCollisionResponseParams::DefaultResponseParam,
		&TraceDelegate
	);
}

void AAircraftMaster::Tick_VerticalTakeOff(float DeltaTime)
{
	if (not EnsureAircraftMovementIsValid())
	{
		return;
	}

	const float CurrentZ = GetActorLocation().Z;
	const float TargetZ = M_AircraftLandedData.TargetAirborneHeight;
	const float Acceleration = M_AircraftMovementSettings.VerticalAcceleration;
	const float MaxSpeed = M_AircraftMovementSettings.MaxVerticalTakeOffSpeed;

	const bool bReached = M_AircraftMovement->TickVerticalTakeOff(DeltaTime, CurrentZ, TargetZ, Acceleration, MaxSpeed);
	if (bReached)
	{
		OnVtoCompleted(TargetZ);
	}
}

void AAircraftMaster::InitIdleCircleFromCurrentPose()
{
	// Center the circle around current location
	M_AircraftIdleData.CenterPoint = GetActorLocation();

	// Choose CW/CCW to minimize roll change from current roll
	const FRotator Rot = GetActorRotation();
	const float BankTargetLeft = M_AircraftMovementSettings.IdleBankAngle; // + roll
	const float BankTargetRight = -M_AircraftMovementSettings.IdleBankAngle; // - roll
	const float DistLeft = FMath::Abs(BankTargetLeft - Rot.Roll);
	const float DistRight = FMath::Abs(BankTargetRight - Rot.Roll);
	const float TurnSign = (DistLeft <= DistRight) ? 1.f : -1.f;

	// Start angle so the first tangent matches current forward (zero initial bank jerk)
	const float YawRad = FMath::DegreesToRadians(Rot.Yaw);
	M_AircraftIdleData.CurrentAngle = TurnSign * YawRad;
}

void AAircraftMaster::Tick_IdleCircling(float DeltaTime)
{
	if (not EnsureAircraftMovementIsValid())
	{
		return;
	}

	// Circle parameters
	const float Radius = M_AircraftMovementSettings.IdleRadius;
	float& Angle = M_AircraftIdleData.CurrentAngle; // sign encodes CW/CCW
	const FVector Center = M_AircraftIdleData.CenterPoint;
	const float MovementSpeed = M_AircraftMovementSettings.MaxMoveSpeed *
		M_AircraftMovementSettings.IdleMovementSpeedMlt;

	// --- Grace period scale (0..1), ramps from GraceMinSteerScale -> 1 over GraceTotalDuration
	float SteerScale = 1.f;
	if (M_AircraftIdleData.GraceTimeRemaining > 0.f &&
		M_AircraftIdleData.GraceTotalDuration > KINDA_SMALL_NUMBER)
	{
		const float t = 1.f - (M_AircraftIdleData.GraceTimeRemaining / M_AircraftIdleData.GraceTotalDuration);
		SteerScale = FMath::Lerp(M_AircraftIdleData.GraceMinSteerScale, 1.f, t);
		M_AircraftIdleData.GraceTimeRemaining = FMath::Max(0.f, M_AircraftIdleData.GraceTimeRemaining - DeltaTime);
	}

	// Angular speed and signed integration (CW if Angle<0, CCW if Angle>=0)
	const float FullCircleTime = (2.f * PI * Radius) / FMath::Max(MovementSpeed, KINDA_SMALL_NUMBER);
	const float AngularSpeed = (2.f * PI) / FMath::Max(FullCircleTime, KINDA_SMALL_NUMBER);
	const float TurnSign = (Angle >= 0.f) ? 1.f : -1.f;

	// During grace, advance the sweep more slowly
	Angle += TurnSign * AngularSpeed * SteerScale * DeltaTime;

	// Next target point on circle at our altitude
	const FVector CurrentLoc = GetActorLocation();
	const FVector NextPoint(
		Center.X + Radius * FMath::Cos(Angle),
		Center.Y + Radius * FMath::Sin(Angle),
		CurrentLoc.Z
	);
	const FVector Dir = (NextPoint - CurrentLoc).GetSafeNormal();

	// Build desired rotation
	FRotator DesiredRot;
	DesiredRot.Yaw = Dir.Rotation().Yaw;
	DesiredRot.Pitch = 0.f;

	// Gracefully ramp roll toward signed idle bank
	const float TargetRoll = (M_AircraftMovementSettings.IdleBankAngle * TurnSign) * SteerScale;
	const float BaseInterpSpeed = M_AircraftMovementSettings.BankInterpSpeed;
	// Reduce rotation interpolation aggressiveness during grace (but keep a small floor)
	const float InterpSpeed = FMath::Max(0.25f, BaseInterpSpeed * SteerScale);
	const float NewRoll = FMath::FInterpTo(GetActorRotation().Roll, TargetRoll, DeltaTime, InterpSpeed);
	DesiredRot.Roll = NewRoll;

	// Apply movement + rotation smoothing (scale steering force during grace)
	M_AircraftMovement->AddInputVector(Dir * SteerScale);
	const FRotator CurrentRot = GetActorRotation();
	const FRotator NewRotation = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, InterpSpeed);
	SetActorRotation(NewRotation);
}

void AAircraftMaster::SetVtoTimer(const float TimeTillStartVto)
{
	if (not GetWorld())
	{
		return;
	}

	// Guard: if VTO is already in progress, do NOT re-arm another prepare/VTO.
	if (M_LandedState == EAircraftLandingState::VerticalTakeOff)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(M_AircraftLandedData.Vto_PrepareHandle);
	M_AircraftLandedData.Vto_PrepareHandle.Invalidate();


	if (TimeTillStartVto <= 0.f)
	{
		OnVtoStart();
		return;
	}
	TWeakObjectPtr<AAircraftMaster> WeakThis(this);
	auto OnVtoPrepComplete = [WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnVtoStart();
	};
	FTimerDelegate VtoPrepDelegate;
	VtoPrepDelegate.BindLambda(OnVtoPrepComplete);
	GetWorld()->GetTimerManager().SetTimer(
		M_AircraftLandedData.Vto_PrepareHandle,
		VtoPrepDelegate,
		TimeTillStartVto, false);
	OnPrepareVto(TimeTillStartVto);
}

void AAircraftMaster::OnPrepareVto(const float PrepTime)
{
	if (not EnsureAnimInstanceIsValid())
	{
		return;
	}

	M_AnimInstAircraft->OnPrepareVto(PrepTime);
	VerticalText_PrepTakeOff();
	BP_OnPrepareVto();
}

void AAircraftMaster::OnVtoStart()
{
	if (not EnsureAnimInstanceIsValid())
	{
		return;
	}
	// start VTO -> EAircraftLandingState::VerticalTakeOff tells tick to start.

	ReloadManager.OnAircraftTakeOff(this);
	UpdateLandedState(EAircraftLandingState::VerticalTakeOff);
	M_AnimInstAircraft->OnVtoStart(
		ComputeTimeToAirborne(GetActorLocation().Z, M_AircraftLandedData.TargetAirborneHeight));
	BP_OnVtoStart();
}


void AAircraftMaster::Tick_MoveTo(const float DeltaTime)
{
	auto& Path = M_AircraftMoveData.Path;
	auto& Points = Path.PathPoints;
	int32& CurrentPathIndex = Path.CurrentPathIndex;
	const int32 MaxIndex = Points.Num() - 1;

	// if we have no more segments left, bail
	if (CurrentPathIndex >= MaxIndex)
	{
		RTSFunctionLibrary::ReportError("index out of bounds in AAircraftMaster::Tick_MoveTo. ");
		return;
	}

	const FVector PrevLoc = Points[CurrentPathIndex].Location;
	const FVector NextLoc = Points[CurrentPathIndex + 1].Location;
	float DistanceToNext;
	const float Ratio = GetRatioSegmentCompleted(PrevLoc, NextLoc, DistanceToNext);

	// if we’re within acceptance radius or fully past it, advance to the next point
	if (DistanceToNext <= M_AircraftMovementSettings.PathPointAcceptanceRadius || Ratio >= 1.f)
	{
		const bool bIsNotFinalSegment = (CurrentPathIndex + 1 < MaxIndex);
		if (Ratio >= 1 && bIsNotFinalSegment)
		{
			OnPointOvershot_AdjustFutureRotation(Path.PathPoints[CurrentPathIndex + 1],
			                                     Path.PathPoints[CurrentPathIndex + 2]);
		}
		// reset segment start rotation so next segment blends smoothly
		CurrentPathIndex++;
		// if that was the last index, we’re done
		if (CurrentPathIndex >= MaxIndex)
		{
			OnMoveCompleted();
			return;
		}
		return;
	}

	const FAircraftPathPoint NextPoint = Points[CurrentPathIndex + 1];
	M_AircraftMovement->TickMovement_Homing(NextPoint, DeltaTime);
}

void AAircraftMaster::Tick_AttackMove(const float DeltaTime)
{
	auto& Path = M_AircraftAttackData.Path;
	auto& Points = Path.PathPoints;
	int32& CurrentPathIndex = Path.CurrentPathIndex;
	const int32 MaxIndex = Points.Num() - 1;

	// if we have no more segments left, bail
	if (CurrentPathIndex >= MaxIndex)
	{
		RTSFunctionLibrary::ReportError("index out of bounds in AAircraftMaster::Tick_AttackMove. ");
		return;
	}

	const FVector PrevLoc = Points[CurrentPathIndex].Location;
	const FVector NextLoc = Points[CurrentPathIndex + 1].Location;
	float DistanceToNext;
	const float Ratio = GetRatioSegmentCompleted(PrevLoc, NextLoc, DistanceToNext);


	// if we’re within acceptance radius or fully past it, advance to the next point
	if (DistanceToNext <= M_AircraftMovementSettings.PathPointAcceptanceRadius || Ratio >= 1.f)
	{
		const bool bIsNotFinalSegment = (CurrentPathIndex + 1 < MaxIndex);
		if (Ratio >= 1 && bIsNotFinalSegment)
		{
			OnPointOvershot_AdjustFutureRotation(Path.PathPoints[CurrentPathIndex + 1],
			                                     Path.PathPoints[CurrentPathIndex + 2]);
		}
		CurrentPathIndex++;
		// if that was the last index, we’re done
		if (CurrentPathIndex >= MaxIndex)
		{
			OnAttackMoveCompleted();
			return;
		}
		// We finished going from prev to next, in the next tick the next point will be the new prev and index + 1
		// will be the new next point.
		const int32 NewTargetPointIndex = CurrentPathIndex + 1;
		if (NewTargetPointIndex <= MaxIndex)
		{
			AttackMove_IssueActionForNewTargetPoint(Points[NewTargetPointIndex]);
		}
		return;
	}
	const FAircraftPathPoint NextPoint = Points[CurrentPathIndex + 1];
	M_AircraftMovement->TickMovement_Homing(NextPoint, DeltaTime);
}

void AAircraftMaster::AttackMove_IssueActionForNewTargetPoint(const FAircraftPathPoint& NextPoint)
{
	EAirPathPointType NewPointType = NextPoint.PointType;
	switch (NewPointType)
	{
	case EAirPathPointType::Regular:
		break;
	case EAirPathPointType::Bezier:
		break;
	case EAirPathPointType::AttackDive:
		StartWeaponFire();
		break;
	case EAirPathPointType::GetOutOfDive:
		StartBombThrowing(M_AircraftAttackData.TargetActor);
		break;
	}
	StopIssuedActionForNewType(NewPointType);
}

void AAircraftMaster::StopIssuedActionForNewType(const EAirPathPointType NewType)
{
	const bool bIsInDive = NewType == EAirPathPointType::AttackDive;
	const bool bIsGettingOutOfDive = NewType == EAirPathPointType::GetOutOfDive;
	if (IssuedActionsState.bM_AreBombsActive && not bIsGettingOutOfDive)
	{
		StopBombThrowing();
	}
	if (IssuedActionsState.bM_AreWeaponsActive && not bIsInDive)
	{
		StopWeaponFire();
	}
}

void AAircraftMaster::StopBombThrowing()
{
	if (not IsValidBombComponent())
	{
		return;
	}
	M_AircraftBombComponent->StopThrowingBombs();
	IssuedActionsState.bM_AreBombsActive = false;
}

void AAircraftMaster::StartWeaponFire()
{
	FRTSAircraftHelpers::AircraftDebug("fire weapons", FColor::Purple);
	FireAttachedWeapons();
	IssuedActionsState.bM_AreWeaponsActive = true;
}

void AAircraftMaster::FireAttachedWeapons() const
{
	if (not EnsureAircraftWeaponIsValid())
	{
		return;
	}
	M_AircraftWeapon->AllWeaponsFire(M_AircraftAttackData.TargetActor);
}

void AAircraftMaster::StartBombThrowing(AActor* TargetActor)
{
	if (not IsValidBombComponent())
	{
		return;
	}
	M_AircraftBombComponent->StartThrowingBombs(TargetActor);
	IssuedActionsState.bM_AreBombsActive = true;
}

void AAircraftMaster::StopWeaponFire()
{
	FRTSAircraftHelpers::AircraftDebug("Stop fire", FColor::Purple);
	if (not EnsureAircraftWeaponIsValid())
	{
		return;
	}
	M_AircraftWeapon->AllWeaponsStopFire(true, true);
	IssuedActionsState.bM_AreWeaponsActive = false;
}


bool AAircraftMaster::GetIsHomingTargetLocation(const FAircraftPathPoint& NextPoint,
                                                const bool bIsMovingToFinalPoint)
{
	if (bIsMovingToFinalPoint)
	{
		return true;
	}
	// if( NextPoint.IsHomingPoint())
	// {
	// 	return true;
	// }
	const bool bIsAttackDive = NextPoint.PointType == EAirPathPointType::AttackDive;
	const bool bIsOutOfDive = NextPoint.PointType == EAirPathPointType::GetOutOfDive;
	return bIsAttackDive || bIsOutOfDive;
}

void AAircraftMaster::OnAttackMoveCompleted()
{
	FRTSAircraftHelpers::AircraftDebug("Attack Move path completed", FColor::Green);
	if (not GetIsValidRTSComponent())
	{
		return;
	}
	if (not M_AircraftAttackData.IsTargetActorVisible(RTSComponent->GetOwningPlayer()))
	{
		FRTSAircraftHelpers::AircraftDebug("Target no longer visible or valid; done with attack command.");
		DoneExecutingCommand(EAbilityID::IdAttack);
		return;
	}
	StartAttackActor();
}

void AAircraftMaster::CleanUpAttackAndSwitchToIdle()
{
	SetMovementToIdle();
	FRTSAircraftHelpers::AircraftDebug(TEXT("Attack command Complete"), FColor::Yellow);
}

float AAircraftMaster::GetMoveToZValue() const
{
	switch (M_LandedState)
	{
	case EAircraftLandingState::None:
		return GetActorLocation().Z + M_AircraftMovementSettings.TakeOffHeight;
	case EAircraftLandingState::Landed:
		// Calculate airborne height.
		return GetActorLocation().Z + M_AircraftMovementSettings.TakeOffHeight;
	// Falls through
	case EAircraftLandingState::Airborne:
	// Falls through
	case EAircraftLandingState::WaitForBayToOpen:
	case EAircraftLandingState::VerticalTakeOff:
		// Use the target airborne height as that is the height from which we will start flying.
		return M_AircraftLandedData.TargetAirborneHeight;
	case EAircraftLandingState::VerticalLanding:
		//todo when implementating landing logic; is this the best way of doing this?
		return M_AircraftLandedData.LandedPosition.Z + M_AircraftMovementSettings.TakeOffHeight;
	}
	RTSFunctionLibrary::ReportError("GetMoveToZValue called with invalid M_LandedState on " + GetName() +
		". Returning default value.");
	return M_AircraftLandedData.LandedPosition.Z + M_AircraftMovementSettings.TakeOffHeight;
}

float AAircraftMaster::GetRatioSegmentCompleted(const FVector& StartSegment, const FVector& EndSegment,
                                                float& OutDistanceToNext) const
{
	// how far along that segment are we?
	const float SegmentLen = FVector::Dist(StartSegment, EndSegment);
	const float Travelled = FVector::Dist(GetActorLocation(), StartSegment);
	OutDistanceToNext = FVector::Dist(GetActorLocation(), EndSegment);
	const float Ratio = (SegmentLen > KINDA_SMALL_NUMBER)
		                    ? FMath::Clamp(Travelled / SegmentLen, 0.f, 1.f)
		                    : 1.f;

	// if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	// {
	// 	const FString DebugString = "Ratio = " + FString::SanitizeFloat(Ratio) + "\n segmentlength: " +
	// 		FString::SanitizeFloat(SegmentLen) +
	// 		"\n travelled: " + FString::SanitizeFloat(Travelled) +
	// 		"\n Vel : " + FString::SanitizeFloat(GetVelocity().Length());
	// 	FRTSAircraftHelpers::AircraftDebug(DebugString);
	// }
	return Ratio;
}

void AAircraftMaster::OnPointOvershot_AdjustFutureRotation(
	FAircraftPathPoint& InOvershotPoint,
	FAircraftPathPoint& InFutureTargetPoint) const
{
	const FVector AircraftLocation = GetActorLocation();
	// Set to current to calculate future distances correctly from the point moved to the overshot location.
	InOvershotPoint.Location = AircraftLocation;

	// Save the roll as it may be designed in for example a bezier point.
	const float Roll = InFutureTargetPoint.Roll;
	// Help the aircraft get back on track by adjusting the rotation.
	FRotator Rot = UKismetMathLibrary::FindLookAtRotation(
		AircraftLocation,
		InFutureTargetPoint.Location);
	// Set the designed roll back to the future rotation.
	InFutureTargetPoint.Roll = Roll + Rot.Roll;
}

void AAircraftMaster::BeginPlay_InitAircraft()
{
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not GameState || not GetIsValidRTSComponent())
	{
		return;
	}

	EAircraftSubtype AircraftSubtype = RTSComponent->GetSubtypeAsAircraftSubtype();
	const uint8 OwningPlayer = RTSComponent->GetOwningPlayer();
	const FAircraftData AircraftData = GameState->GetAircraftDataOfPlayer(AircraftSubtype, OwningPlayer);
	UnitCommandData->SetAbilities(AircraftData.Abilities);

	if (FowComponent)
	{
		FowComponent->SetVisionRadius(AircraftData.VisionRadius);
	}
	if (HealthComponent)
	{
		HealthComponent->InitHealthAndResistance(AircraftData.ResistancesAndDamageMlt, AircraftData.MaxHealth);
	}
	if (EnsureExperienceComponentIsValid())
	{
		const FTrainingOption UnitTypeAndSubtype = FTrainingOption(
			RTSComponent->GetUnitType(),
			static_cast<uint8>(AircraftSubtype)
		);
		ExperienceComponent->InitExperienceComponent(this, UnitTypeAndSubtype, OwningPlayer);
	}
	if (IsValidBombComponent())
	{
		M_AircraftBombComponent->InitBombComponent(M_AircraftMesh, RTSComponent->GetOwningPlayer());
	}
}

void AAircraftMaster::BeginPlay_PropagateStateToWpAndAnimInst() const
{
	UpdateAircraftWeaponWithLandedState();
	UpdateAircraftAnimationWithLandedState();
}

void AAircraftMaster::SwapAbilityToReturnToBase()
{
	if (not SwapAbility(EAbilityID::IdAircraftOwnerNotExpanded, EAbilityID::IdReturnToBase))
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to swap ability to return to base, but aircraft does not have the No Aircraft Owner Ability!"
			"\n Aircraft: " + GetName());
		return;
	}
}

void AAircraftMaster::SwapAbilityToNoOwner()
{
	if (not SwapAbility(EAbilityID::IdReturnToBase, EAbilityID::IdAircraftOwnerNotExpanded))
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to swap ability to no aircraft owner, but aircraft does not have the Return To Base Ability!"
			"\n Aircraft: " + GetName());
		return;
	}
}


bool AAircraftMaster::EnsureAircraftMovementIsValid() const
{
	if (not IsValid(M_AircraftMovement))
	{
		RTSFunctionLibrary::ReportError("AircraftMovement is not valid on " + GetName() +
			". Ensure that the AircraftMovement component is added and initialised correctly.");
		return false;
	}
	return true;
}

bool AAircraftMaster::EnsureSelectionDecalIsValid() const
{
	if (not IsValid(M_SelectionDecal))
	{
		return false;
	}
	return true;
}

bool AAircraftMaster::EnsureExperienceComponentIsValid() const
{
	if (not IsValid(ExperienceComponent))
	{
		RTSFunctionLibrary::ReportError("ExperienceComponent is not valid on " + GetName() +
			". Ensure that the ExperienceComponent is added and initialised correctly.");
		return false;
	}
	return true;
}

bool AAircraftMaster::IsIdleHover() const
{
	return M_AircraftMovementSettings.AirborneBehavior == EAircraftAirborneBehavior::Hover;
}

void AAircraftMaster::OnLandscapeTraceHit(const FHitResult& TraceHit) const
{
	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
	DrawDebugSphere(GetWorld(), TraceHit.Location, 100.f, 12, FColor::Green, false, 5.f);
		
	}
	if (not M_SelectionDecal)
	{
		return;
	}
	M_SelectionDecal->SetWorldLocation(TraceHit.Location);
}

void AAircraftMaster::SetMovementToIdle()
{
	M_MovementState = EAircraftMovementState::Idle;
	InitIdleCircleFromCurrentPose();

	// Configure soft-entry into idle circling
	M_AircraftIdleData.GraceTotalDuration = 0.8f; // seconds
	M_AircraftIdleData.GraceTimeRemaining = 0.8f; // start full
	M_AircraftIdleData.GraceMinSteerScale = 0.35f; // softer steering at start
}


void AAircraftMaster::ClearOwnerAircraft()
{
	M_AircraftOwner.Reset();
	// Did we have a valid owner to land on before?
	if (HasAbility(EAbilityID::IdReturnToBase))
	{
		SwapAbilityToNoOwner();
	}
}

bool AAircraftMaster::EnsureAircraftMeshIsValid() const
{
	if (not IsValid(M_AircraftMesh))
	{
		RTSFunctionLibrary::ReportError("AircraftMesh is not valid on " + GetName() +
			". Ensure that the AircraftMesh component is added and initialised correctly.");
		return false;
	}
	return true;
}

bool AAircraftMaster::EnsureAnimInstanceIsValid() const
{
	if (not IsValid(M_AnimInstAircraft))
	{
		RTSFunctionLibrary::ReportError("Anim instance is not set on aircraft!"
			"Ensure that the anim instance on the sk mesh derives"
			"from the correct base class!");
		return false;
	}
	return true;
}

bool AAircraftMaster::IsValidBombComponent() const
{
	if (not IsValid(M_AircraftBombComponent))
	{
		return false;
	}
	return true;
}

bool AAircraftMaster::EnsureAircraftWeaponIsValid() const
{
	if (not IsValid(M_AircraftWeapon))
	{
		RTSFunctionLibrary::ReportError("AircraftWeapon is not valid on " + GetName() +
			". Ensure that the AircraftWeapon component is added and initialised correctly.");
		return false;
	}
	return true;
}


void AAircraftMaster::UpdateAircraftWeaponWithLandedState() const
{
	if (not EnsureAircraftWeaponIsValid())
	{
		return;
	}
	M_AircraftWeapon->UpdateLandedState(M_LandedState);
}

void AAircraftMaster::UpdateAircraftAnimationWithLandedState() const
{
	if (not EnsureAnimInstanceIsValid())
	{
		return;
	}
	M_AnimInstAircraft->UpdateLandedState(M_LandedState);
}

bool AAircraftMaster::EnsureValidChangeOwnerRequest(TWeakObjectPtr<UAircraftOwnerComp> NewOwner) const
{
	if (not NewOwner.IsValid())
	{
		RTSFunctionLibrary::ReportError("Attempted to change owner for aircraft with null owner!"
			"\n Aircraft: " + GetName() + "\n NewOwner is null.");
		return false;
	}
	UAircraftOwnerComp* NewOwnerPtr = NewOwner.Get();
	const bool bFull = NewOwner->IsAirBaseFull();
	const bool bIsPackedUp = NewOwner->IsAirbasePackedUp();
	if (bFull || bIsPackedUp)
	{
		RTSFunctionLibrary::DisplayNotification(FText::FromString(
			"Aircraft cannot change owner to the selected airbase as it is either full or packed up."
			"\n Aircraft: " + GetName() + "\n NewOwner: " + NewOwnerPtr->GetName() +
			"\n Full: " + (bFull ? "true" : "false") +
			"\n PackedUp: " + (bIsPackedUp ? "true" : "false")));
		return false;
	}
	// Already owned by this owner?
	if (M_AircraftOwner.IsValid() && M_AircraftOwner.Get() == NewOwnerPtr)
	{
		RTSFunctionLibrary::ReportError("Attempted to change owner for aircraft to the same owner!"
			"\n Aircraft: " + GetName() + "\n Owner: " + NewOwnerPtr->GetName());
		return false;
	}

	return true;
}

void AAircraftMaster::ChangeOwner_FirstClearOldOwner(UAircraftOwnerComp* NewOwner)
{
	// Terminate any previous command. If we were landing, termination triggers VTO instead.
	SetUnitToIdle();
	if (M_LandedState == EAircraftLandingState::Landed)
	{
		// We’re still on the ground: open doors then VTO.
		M_PendingPostLiftOffAction.Set(EPostLiftOffAction::ChangeOwner, NewOwner);

		float OpenAnimTime = 0.f;
		bool bWasDoorAlreadyOpen = false;
		if (M_AircraftOwner.IsValid())
		{
			M_AircraftOwner->RequestOpenBayDoorsNoPendingAction(this, bWasDoorAlreadyOpen, OpenAnimTime);
		}
		SetVtoTimer(OpenAnimTime);
		return;
	}
	if (M_LandedState == EAircraftLandingState::VerticalTakeOff)
	{
		// VTO already in progress (e.g., we cancelled a landing): just queue the action.
		// DO NOT call SetVtoTimer() again.
		M_PendingPostLiftOffAction.Set(EPostLiftOffAction::ChangeOwner, NewOwner);
		return;
	}

	// Airborne: perform the actual owner switch now.
	if (!RemoveDataOnOldOwner_SetNew(NewOwner))
	{
		FRTSAircraftHelpers::AircraftDebug(TEXT("Failed to change owner; set to idle"));
		SetMovementToIdle();
		return;
	}

	// Force RTB to the new owner’s bay.
	ReturnToBase(true);
}


void AAircraftMaster::PostLiftOff_ChangeOwner()
{
	if (not EnsureValidChangeOwnerRequest(M_PendingPostLiftOffAction.GetNewOwner()))
	{
		SetMovementToIdle();
		return;
	}
	UAircraftOwnerComp* NewOwner = M_PendingPostLiftOffAction.GetNewOwner().Get();
	const bool bSuccess = RemoveDataOnOldOwner_SetNew(NewOwner);
	if (not bSuccess)
	{
		FRTSAircraftHelpers::AircraftDebug("Failed to change owner; set to idle");
		SetMovementToIdle();
		return;
	}
	// force return to base on new owner.
	ReturnToBase(true);
}

bool AAircraftMaster::RemoveDataOnOldOwner_SetNew(UAircraftOwnerComp* NewOwner)
{
	// Clear our aircraft on our previous owner.
	if (M_AircraftOwner.IsValid())
	{
		bool bSuccessfull = M_AircraftOwner->RemoveAircraftFromBuilding_CloseBayDoors(this);
		if (not bSuccessfull)
		{
			RTSFunctionLibrary::ReportError("The aircraft was set to change owners and attempted to clear"
				"the data on the previous owner, but this failed!"
				"\n Aircraft name: " + GetName());
		}
	}
	// This also sets the pointer to the new owner on this aircraft.
	const bool bChangeOwnerSuccess = NewOwner->OnAircraftChangesToThisOwner(this);
	if (not bChangeOwnerSuccess)
	{
		FRTSAircraftHelpers::AircraftDebug("Failed to change the owner for aircraft: " + GetName() +
			" to new owner: " + NewOwner->GetName());
		return false;
	}
	// Success; update landing position.
	UpdateLandingPositionForNewOwner(NewOwner);
	return true;
}

void AAircraftMaster::UpdateLandingPositionForNewOwner(UAircraftOwnerComp* NewOwner)
{
	if (not IsValid(NewOwner))
	{
		return;
	}
	bool bValidSSocketIndex = false;
	const int32 SocketIndex = NewOwner->GetAssignedSocketIndex(this);
	bValidSSocketIndex = NewOwner->GetSocketWorldLocation(SocketIndex, M_AircraftLandedData.LandedPosition);
	bValidSSocketIndex = NewOwner->GetSocketWorldRotation(SocketIndex, M_AircraftLandedData.LandedRotation);
	if (not bValidSSocketIndex)
	{
		RTSFunctionLibrary::ReportError(
			"Invalid socket index for trying to update the landed position and rotation for the aircraft"
			"after switching owners."
			"\n aircraft name: " + GetName() + "\n new owner name: " + NewOwner->GetName() + "\n socket index: " +
			FString::FromInt(SocketIndex) + "\n socket index valid: " + (bValidSSocketIndex ? "true" : "false"));
	}
}

void AAircraftMaster::OnOwnerDies_ResetToIdleAirborne()
{
	if (EnsureAircraftMovementIsValid())
	{
		M_AircraftMovement->KillMovement();
	}

	M_AircraftReturnData.Reset();
	M_PendingPostLiftOffAction.Reset();

	// Clear landing cache
	M_AircraftLandedData.LandedPosition = FVector::ZeroVector;
	M_AircraftLandedData.LandedRotation = FRotator::ZeroRotator;
	M_AircraftLandedData.TargetAirborneHeight = GetActorLocation().Z;

	M_AircraftMoveData.Reset();
	M_AircraftAttackData.Reset();
	UpdateLandedState(EAircraftLandingState::Airborne);
	SetMovementToIdle();
}


bool AAircraftMaster::GetIsValidAircraftOwner() const
{
	if (M_AircraftOwner.IsValid())
	{
		return true;
	}
	// Not an error: aircraft can be orphan (no owner).
	return false;
}

void AAircraftMaster::OnLandingFinish()
{
	UpdateLandedState(EAircraftLandingState::Landed);
	ReloadManager.OnAircraftLanded(this);
	if (M_AircraftOwner.IsValid())
	{
		M_AircraftOwner->OnAircraftLanded(this);
	}
	BP_OnLandingCompleted();
	if (EnsureAnimInstanceIsValid())
	{
		M_AnimInstAircraft->OnLandingComplete();
	}

	// Done with the Return-To-Base command
	DoneExecutingCommand(EAbilityID::IdReturnToBase);
}

void AAircraftMaster::Tick_VerticalLanding(float DeltaTime)
{
	if (not EnsureAircraftMovementIsValid())
	{
		return;
	}

	const float CurrentZ = GetActorLocation().Z;
	const float TargetZ = M_AircraftLandedData.LandedPosition.Z;
	const float Accel = M_AircraftMovementSettings.VerticalAcceleration;
	const float MaxSpeed = M_AircraftMovementSettings.MaxVerticalTakeOffSpeed;
	// reuse; add a Landing var later if desired

	const bool bReached = M_AircraftMovement->TickVerticalLanding(
		DeltaTime, CurrentZ, TargetZ, Accel, MaxSpeed);

	if (bReached)
	{
		// Snap exactly to target Z (symmetry with VTO completion)
		SetActorLocation({GetActorLocation().X, GetActorLocation().Y, TargetZ});
		OnLandingFinish();
	}
}


void AAircraftMaster::OnRTSUnitSpawned_SetEnabled(const float TimeNotSelectable)
{
	this->SetActorHiddenInGame(false);
	this->SetActorEnableCollision(true);
	BP_OnRTSUnitSpawned(false);
	if (GetIsValidSelectionComponent())
	{
		SelectionComponent->SetCanBeSelected(true);
	}
}

void AAircraftMaster::OnRTSUnitSpawned_SetDisabled()
{
	this->SetActorHiddenInGame(true);
	this->SetActorEnableCollision(false);
	if (GetIsValidSelectionComponent())
	{
		SelectionComponent->SetCanBeSelected(false);
	}
	BP_OnRTSUnitSpawned(true);
}

void AAircraftMaster::VerticalText_WaitForLanding() const
{
	const FVector Location = GetActorLocation() + FVector(0.f, 0.f, VerticalTextAircraftZOffset);
	const FString Text = FRTSRichTextConverter::MakeRTSRich("Waiting for bay to open",
	                                                                  ERTSRichText::Text_Armor);
	FRTSVerticalAnimTextSettings Settings;
	Settings.DeltaZ = 75.f;
	Settings.VisibleDuration = 3.f;
	Settings.FadeOutDuration = 2.f;
	VerticalAnimatedText(Location, Text, Settings);
}

void AAircraftMaster::VerticalText_PrepTakeOff() const
{
	if (M_AircraftMovementSettings.VtoPrepareTime <= 0.f)
	{
		// No prep time; no text.
		return;
	}

	const FVector Location = GetActorLocation() + FVector(0.f, 0.f, VerticalTextAircraftZOffset);
	constexpr float FadeDuration = 1.f;
	const float VisibleDuration = FMath::Max(M_AircraftMovementSettings.VtoPrepareTime - FadeDuration, 1.f);
	const FString Text = FRTSRichTextConverter::MakeRTSRich("Preparing for takeoff",

	                                                                  ERTSRichText::Text_Armor);
	FRTSVerticalAnimTextSettings Settings;
	Settings.DeltaZ = 75.f;
	Settings.VisibleDuration = VisibleDuration;
	Settings.FadeOutDuration = FadeDuration;
	VerticalAnimatedText(Location, Text, Settings);
}

void AAircraftMaster::VerticalAnimatedText(const FVector& Location, const FString& InText,
                                           const FRTSVerticalAnimTextSettings Settings) const
{
	const TEnumAsByte<ETextJustify::Type> Justify = ETextJustify::Center;
	URTSBlueprintFunctionLibrary::RTSSpawnVerticalAnimatedTextAtLocation(this,
	                                                                     InText, Location, false, 0.f, Justify,
	                                                                     Settings);
}

float AAircraftMaster::ComputeTimeToAirborne(const float InFromZ, const float InToZ) const
{
	// Distance to climb (cm)
	const float Distance = InToZ - InFromZ;
	if (Distance <= 0.f)
	{
		return 0.f;
	}

	// Read movement safely without mutating anything
	const UAircraftMovement* const Movement = M_AircraftMovement;

	// Initial upward speed (cm/s). Clamp to >= 0 because we only care about upward progress.
	const float V0 = (IsValid(Movement) ? FMath::Max(0.f, Movement->Velocity.Z) : 0.f);

	// Effective acceleration (cm/s^2):
	//   your explicit VTO accel + UFloatingPawnMovement accel contributed by AddInputVector(Up)
	float EffectiveAccel = FMath::Max(KINDA_SMALL_NUMBER, M_AircraftMovementSettings.VerticalAcceleration);
	if (IsValid(Movement))
	{
		EffectiveAccel += FMath::Max(0.f, Movement->Acceleration);
	}

	// Effective top speed (cm/s): respect both your VTO cap and the movement component's cap
	float Cap = FMath::Max(0.f, M_AircraftMovementSettings.MaxVerticalTakeOffSpeed);
	if (IsValid(Movement))
	{
		Cap = FMath::Min(Cap, Movement->MaxSpeed);
	}

	// If there's effectively no cap, solve v0*t + 0.5*a*t^2 = Distance
	if (Cap <= KINDA_SMALL_NUMBER)
	{
		const float Disc = V0 * V0 + 2.f * EffectiveAccel * Distance;
		return (FMath::Sqrt(Disc) - V0) / EffectiveAccel;
	}

	// Clamp initial speed to the cap (defensive; sim already clamps)
	const float V0Clamped = FMath::Min(V0, Cap);

	// Distance needed to accelerate from V0 to Cap under constant accel
	const float DistanceToCap = FMath::Max(0.f, (Cap * Cap - V0Clamped * V0Clamped) / (2.f * EffectiveAccel));

	// Case 1: never reaches the cap → solve quadratic
	if (Distance <= DistanceToCap)
	{
		const float Disc = V0Clamped * V0Clamped + 2.f * EffectiveAccel * Distance;
		return (FMath::Sqrt(Disc) - V0Clamped) / EffectiveAccel;
	}

	// Case 2: accelerate to cap, then cruise
	const float TAccel = (Cap - V0Clamped) / EffectiveAccel;
	const float SCruise = Distance - DistanceToCap;
	const float TCruise = SCruise / Cap;

	return TAccel + TCruise;
}

bool AAircraftMaster::IsPreparingVTO() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(M_AircraftLandedData.Vto_PrepareHandle);
}

void AAircraftMaster::CancelPrepareVTO_StayLanded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_AircraftLandedData.Vto_PrepareHandle);
		M_AircraftLandedData.Vto_PrepareHandle.Invalidate();
	}
	OnLandingFinish();
}


void AAircraftMaster::OnReturnToBase_MoveCompleted()
{
	if (not EnsureAircraftMovementIsValid())
	{
		return;
	}
	// Set to hover idle unit we can start the landing.
	M_MovementState = EAircraftMovementState::Idle;
	// Stop any move pathing while we go into landing
	M_AircraftMoveData.Reset();

	// If the owner vanished, abort RTB.
	// NOTE: ISAircraftAllowedToLandHere will asign the aircraft to a new socket if this aircraft does not have one.
	// it will also then set  the owner on this aircraft if it was not assigned before.
	if ((not GetIsValidAircraftOwner()) || (not M_AircraftOwner->IsAircraftAllowedToLandHere(this)))
	{
		DoneExecutingCommand(EAbilityID::IdReturnToBase);
		return;
	}

	bool bDoorsAlreadyOpen = false;
	// Now that we’re in position, ask to open the doors. Will call start StartVerticalLanding if the doors
	// are already open.
	if (!M_AircraftOwner->RequestOpenBayDoorsForLanding(this, bDoorsAlreadyOpen))
	{
		DoneExecutingCommand(EAbilityID::IdReturnToBase);
		return;
	}

	if (not bDoorsAlreadyOpen)
	{
		// We’re now waiting for the bay to open; owner will call Notify_LandingBayOpen().
		SetAircraftToHoverWaiting();
		if (EnsureAnimInstanceIsValid())
		{
			M_AnimInstAircraft->OnWaitForLanding();
		}
	}
}

bool AAircraftMaster::ExecuteReturnToBase_IsAlreadyLanded() const
{
	return M_LandedState == EAircraftLandingState::Landed;
}

bool AAircraftMaster::ExecuteReturnToBase_IsVto() const
{
	// This can be true if we had previously a movement command from the landed state which triggered vto.
	// The cancel of the movement command sets the aircraft to idle movement but will still lif off.
	return M_LandedState == EAircraftLandingState::VerticalTakeOff;
}

void AAircraftMaster::SetAircraftToHoverWaiting()
{
	if (EnsureAircraftMovementIsValid())
	{
		M_AircraftMovement->KillMovement();
	}

	// Set landing-state so Tick() routes here
	UpdateLandedState(EAircraftLandingState::WaitForBayToOpen);
	VerticalText_WaitForLanding();

	// Update the aircraft rotation to be as how it should be when landing.
	SetActorRotation(M_AircraftLandedData.LandedRotation);

	// Cache the Z to oscillate around and reset the phase
	M_HoverWaitBaseZ = GetActorLocation().Z;
	M_HoverWaitPhase = 0.f;
	M_AircraftReturnData.bWaitingForDoor = true;
}


void AAircraftMaster::Tick_WaitingForBayOpening(const float DeltaTime)
{
	// Simple vertical bob around the position we reached above the bay.
	const float Amp = FMath::Max(0.f, M_AircraftMovementSettings.IdleHoverDeltaZ);
	const float Speed = FMath::Max(0.01f, M_AircraftMovementSettings.IdleHoverSpeedMlt);

	// A comfortable default angular speed (radians/sec), scaled by user setting.
	constexpr float BaseAngularSpeed = 1.25f;
	M_HoverWaitPhase += DeltaTime * BaseAngularSpeed * Speed;

	const float OffsetZ = Amp * FMath::Sin(M_HoverWaitPhase);
	const FVector Cur = GetActorLocation();
	SetActorLocation({Cur.X, Cur.Y, M_HoverWaitBaseZ + OffsetZ});
}


void AAircraftMaster::TerminateReturnToBase_CheckLandedAndMovementState() const
{
	const FString Name = GetName();
	if (M_LandedState == EAircraftLandingState::VerticalTakeOff || M_LandedState == EAircraftLandingState::None)
	{
		const FString StateName = M_LandedState == EAircraftLandingState::VerticalTakeOff ? "VerticalTakeOff" : "None";
		RTSFunctionLibrary::ReportError("Aicraft is set to " + StateName + " while cancelling RTB!"
			"\n this should NOT be possible."
			"Aircraft name: " + Name);
	}
	// Note that Idle as movement state could be possible as we can terminate landing when we started idle->return to base
	// but our socket or owner is not valid.
	if (M_MovementState == EAircraftMovementState::None)
	{
		const FString StateName = "None";
		RTSFunctionLibrary::ReportError("Aircraft is set to " + StateName + " while cancelling RTB!"
			"\n this should NOT be possible."
			"Aircraft name: " + Name);
	}
}

void AAircraftMaster::CancelReturnToBase_VtoBackToAirborne()
{
	// Instantly recover airborne height.
	const float PrepTime = 0.f;
	SetVtoTimer(PrepTime);

	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		const FVector TargetLocation = FVector(GetActorLocation().X, GetActorLocation().Y,
		                                       M_AircraftLandedData.TargetAirborneHeight);
		DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 12, FColor::Cyan, false, 20.f, 0, 1.f);
	}
}
