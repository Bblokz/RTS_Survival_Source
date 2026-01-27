// Copyright (C) Bas Blokzijl - All rights reserved.


#include "SquadUnit.h"

#include <RTS_Survival/FOWSystem/FowComponent/FowType.h>

#include "AIController.h"
#include "BrainComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "AISquadUnit/AISquadUnit.h"
#include "AnimSquadUnit/SquadUnitAnimInstance.h"
#include "Components/WidgetComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/ActionUI/ItemActionUI/W_ItemActionUI.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Audio/RTSVoiceLineHelpers/RTS_VoiceLineHelpers.h"
#include "RTS_Survival/Audio/SpacialVoiceLinePlayer/SquadUnitSpacialVoiceLinePlayer/SquadUnitSpatialVoiceLinePlayer.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Game/GameState/UnitDataHelpers/UnitDataHelpers.h"
#include "RTS_Survival/PickupItems/Items/WeaponPickUp/WeaponPickup.h"
#include "Components/ChildActorComponent.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/RTSComponents/RepairComponent/RepairComponent.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSSquadUnitOptimizer/RTSSquadUnitOptimizer.h"
#include "RTS_Survival/Scavenging/ScavengerComponent/ScavengerComponent.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Weapons/InfantryWeapon/SecondaryWeaponComp/SecondaryWeapon.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"


FSquadUnitRagdoll::FSquadUnitRagdoll()
	: bEnableRagdoll(true)
	  , RagdollCollisionProfileName(TEXT("Ragdoll"))
	  , bApplyImpulseOnDeath(true)
	  , ImpulseStrength(30000.0f)
	  , ImpulseDirection(FVector(-1.0f, 0.0f, 0.0f))
	  , ImpulseBoneName(TEXT("pelvis"))
	  , PhysicsTime(1.0f)
	  , TimeTillDeath(3.0f)
	  , LinearDamping(0.5f)
	  , AngularDamping(0.5f)
	  , MassScale(1.0f)
	  , bClampInitialVelocities(true)
	  , MaxInitialSpeed(1200.0f)
	  , DeathMontages()
	  , bIsRagdollActive(false)
{
}

void FSquadUnitRagdoll::StartRagdoll(
	AActor* OwningActor,
	USkeletalMeshComponent* MeshComponent,
	USquadUnitAnimInstance* AnimInstance,
	AInfantryWeaponMaster* InfantryWeapon)
{
	// Always destroy the weapon when ragdoll starts (or when death is handled if ragdoll is disabled).
	DestroyWeapon(InfantryWeapon);

	if (not bEnableRagdoll)
	{
		return;
	}

	if (not IsValid(MeshComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			OwningActor,
			"MeshComponent",
			"FSquadUnitRagdoll::StartRagdoll");
		return;
	}

	StopAnimations(AnimInstance, MeshComponent);
	SetupPhysicsAndCollision(MeshComponent);
	ApplyImpulse(MeshComponent);
	ClampInitialVelocities(MeshComponent);
	ScheduleDisablePhysics(OwningActor, MeshComponent, AnimInstance);

	bIsRagdollActive = true;
}


void FSquadUnitRagdoll::StopAnimations(
	USquadUnitAnimInstance* AnimInstance,
	USkeletalMeshComponent* MeshComponent) const
{
	if (IsValid(AnimInstance))
	{
		AnimInstance->StopAllMontages();
	}

	if (not IsValid(MeshComponent))
	{
		return;
	}

	MeshComponent->bPauseAnims = true;
	MeshComponent->bNoSkeletonUpdate = true;
}

void FSquadUnitRagdoll::SetupPhysicsAndCollision(USkeletalMeshComponent* MeshComponent) const
{
	if (not IsValid(MeshComponent))
	{
		return;
	}

	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (RagdollCollisionProfileName != NAME_None)
	{
		MeshComponent->SetCollisionProfileName(RagdollCollisionProfileName);
	}
	else
	{
		MeshComponent->SetCollisionProfileName(TEXT("Ragdoll"));
	}

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetAllBodiesSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();
	MeshComponent->bBlendPhysics = true;

	const float ClampedLinearDamping = FMath::Max(0.0f, LinearDamping);
	const float ClampedAngularDamping = FMath::Max(0.0f, AngularDamping);
	const float ClampedMassScale = FMath::Max(0.01f, MassScale);

	MeshComponent->SetLinearDamping(ClampedLinearDamping);
	MeshComponent->SetAngularDamping(ClampedAngularDamping);
	MeshComponent->SetAllMassScale(ClampedMassScale);
}

void FSquadUnitRagdoll::ApplyImpulse(USkeletalMeshComponent* MeshComponent) const
{
	if (not bApplyImpulseOnDeath || FMath::IsNearlyZero(ImpulseStrength) || not IsValid(MeshComponent))
	{
		return;
	}

	const FVector NormalizedDirection = ImpulseDirection.GetSafeNormal();
	if (NormalizedDirection.IsNearlyZero())
	{
		return;
	}

	FVector WorldImpulse = MeshComponent->GetComponentTransform().TransformVector(
		NormalizedDirection * ImpulseStrength);

	// Do not allow the ragdoll to be launched upwards.
	if (WorldImpulse.Z > 0.0f)
	{
		WorldImpulse.Z = 0.0f;
	}

	if (ImpulseBoneName != NAME_None)
	{
		MeshComponent->AddImpulseToAllBodiesBelow(WorldImpulse, ImpulseBoneName, true, true);
		return;
	}

	MeshComponent->AddImpulse(WorldImpulse, NAME_None, true);
}

void FSquadUnitRagdoll::ClampInitialVelocities(USkeletalMeshComponent* MeshComponent) const
{
	if (not bClampInitialVelocities)
	{
		return;
	}

	if (not IsValid(MeshComponent))
	{
		return;
	}

	const float MaxInitialSpeedSquared = FMath::Square(MaxInitialSpeed);
	if (MaxInitialSpeedSquared <= 0.0f)
	{
		return;
	}

	const int32 NumBones = MeshComponent->GetNumBones();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FName BoneName = MeshComponent->GetBoneName(BoneIndex);

		FVector BoneVelocity = MeshComponent->GetPhysicsLinearVelocity(BoneName);

		// Never let the ragdoll gain an upward initial velocity.
		if (BoneVelocity.Z > 0.0f)
		{
			BoneVelocity.Z = 0.0f;
		}

		const float SpeedSquared = BoneVelocity.SizeSquared();
		if (SpeedSquared <= MaxInitialSpeedSquared)
		{
			continue;
		}

		const FVector ClampedVelocity = BoneVelocity.GetSafeNormal() * MaxInitialSpeed;
		MeshComponent->SetPhysicsLinearVelocity(ClampedVelocity, false, BoneName);
	}
}

void FSquadUnitRagdoll::ScheduleDisablePhysics(
	AActor* OwningActor,
	USkeletalMeshComponent* MeshComponent,
	USquadUnitAnimInstance* AnimInstance) const
{
	if (PhysicsTime <= 0.0f)
	{
		return;
	}

	if (not IsValid(OwningActor) || not IsValid(MeshComponent))
	{
		return;
	}

	UWorld* World = OwningActor->GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	// Prepare weak references for safety.
	TWeakObjectPtr<USkeletalMeshComponent> MeshComponentWeak = MeshComponent;
	TWeakObjectPtr<USquadUnitAnimInstance> AnimInstanceWeak = AnimInstance;

	// Copy montages into weak pointers so we do not depend on the struct lifetime in the lambda.
	TArray<TWeakObjectPtr<UAnimMontage>> DeathMontagesWeak;
	DeathMontagesWeak.Reserve(DeathMontages.Num());
	for (UAnimMontage* Montage : DeathMontages)
	{
		if (IsValid(Montage))
		{
			DeathMontagesWeak.Add(Montage);
		}
	}

	constexpr float MinRemainingTimeForMontage = 0.2f;
	const bool bCanPlayMontage =
		DeathMontagesWeak.Num() > 0 && (TimeTillDeath - PhysicsTime) >= MinRemainingTimeForMontage;

	FTimerDelegate DisablePhysicsDelegate;
	DisablePhysicsDelegate.BindLambda(
		[MeshComponentWeak, AnimInstanceWeak, DeathMontagesWeak, bCanPlayMontage]()
		{
			if (not MeshComponentWeak.IsValid())
			{
				return;
			}

			USkeletalMeshComponent* MeshComponentLocal = MeshComponentWeak.Get();
			MeshComponentLocal->SetSimulatePhysics(false);
			MeshComponentLocal->SetAllBodiesSimulatePhysics(false);
			MeshComponentLocal->bBlendPhysics = false;

			// Re-enable animation either way so the pose is driven by the AnimBP again.
			MeshComponentLocal->bPauseAnims = false;
			MeshComponentLocal->bNoSkeletonUpdate = false;

			if (not bCanPlayMontage)
			{
				return;
			}

			if (not AnimInstanceWeak.IsValid())
			{
				return;
			}

			USquadUnitAnimInstance* AnimInstanceLocal = AnimInstanceWeak.Get();
			if (not IsValid(AnimInstanceLocal))
			{
				return;
			}

			// Play the first valid full-body death montage.
			for (const TWeakObjectPtr<UAnimMontage>& MontageWeak : DeathMontagesWeak)
			{
				if (not MontageWeak.IsValid())
				{
					continue;
				}

				UAnimMontage* MontageToPlay = MontageWeak.Get();
				if (not IsValid(MontageToPlay))
				{
					continue;
				}

				AnimInstanceLocal->Montage_Play(MontageToPlay);
				break;
			}
		});

	FTimerHandle DisablePhysicsHandle;
	World->GetTimerManager().SetTimer(DisablePhysicsHandle, DisablePhysicsDelegate, PhysicsTime, false);
}

void FSquadUnitRagdoll::DestroyWeapon(AInfantryWeaponMaster* InfantryWeapon) const
{
	if (not IsValid(InfantryWeapon))
	{
		return;
	}

	InfantryWeapon->DisableAllWeapons();
	InfantryWeapon->Destroy();
}

void FSquadUnitRagdoll::StartRagdoll_CreateEffect(AActor* OwningActor)
{
	if (not IsValid(OwningActor))
	{
		return;
	}
	DeathEffects.CreateFX(OwningActor);
}


ASquadUnit::ASquadUnit(const FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer), AnimBp_SquadUnit(nullptr), M_ActiveCommand(EAbilityID::IdIdle),
	M_SquadController(nullptr),
	M_AISquadUnit(nullptr), M_InfantryWeapon(nullptr), bM_IsSwitchingWeapon(false)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	M_SecondaryWeapon = CreateDefaultSubobject<USecondaryWeapon>("SecondaryWeapon");
	OwningPlayer = 0;
	M_OptimizationComp = CreateDefaultSubobject<URTSSquadUnitOptimizer>("RTSSquadOptimizer");
}

bool ASquadUnit::GetIsUnitIdle() const
{
	if (not GetIsValidRTSComponent() || not GetIsValidSquadController())
	{
		return false;
	}
	const bool bIdleInCommands = M_SquadController->GetIsUnitIdle();
	return bIdleInCommands;
}

bool ASquadUnit::GetIsUnitInCombat() const
{
	if (not GetIsValidRTSComponent())
	{
		return false;
	}
	return RTSComponent->GetIsUnitInCombat();
}

void ASquadUnit::OnSquadInitsData_OverwriteArmorAndResistance(const float MyMaxHealth) const
{
	if (OverwriteUnitArmorAndResistance.ResistancePresetType == EResistancePresetType::None)
	{
		return;
	}
	const float NewMaxHealth = RTSFunctionLibrary::RoundToNearestMultipleOf(
		MyMaxHealth * OverwriteUnitArmorAndResistance.HealthMlt, 10);
	const FResistanceAndDamageReductionData OverwriteData = FUnitResistanceDataHelpers::GetResistancePresetOfType(
		OverwriteUnitArmorAndResistance.ResistancePresetType, NewMaxHealth);
	HealthComponent->InitHealthAndResistance(OverwriteData, NewMaxHealth);
}

void ASquadUnit::OnUnitEnteredLeftCargo(UCargo* CargoComponentEntered, const bool bEnteredCargo) const
{
	if (not IsValid(CargoComponentEntered) || not GetIsValidWeapon())
	{
		return;
	}
	if (not IsValid(CargoComponentEntered->GetOwner()))
	{
		return;
	}
	M_InfantryWeapon->RegisterIgnoreActor(CargoComponentEntered->GetOwner(), bEnteredCargo);
}

void ASquadUnit::PlaySpatialVoiceLine(const ERTSVoiceLine VoiceLineType, const bool bIgnorePlayerCooldown) const
{
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	SpatialVoiceLinePlayer->PlaySpatialVoiceLine(VoiceLineType, GetActorLocation(), bIgnorePlayerCooldown);
}

int32 ASquadUnit::GetOwningPlayer() const
{
	if (!IsValid(RTSComponent))
	{
		return -1;
	}
	return RTSComponent->GetOwningPlayer();
}

bool ASquadUnit::GetIsSquadUnitIdleAndNotEvading() const
{
	if (M_ActiveCommand == EAbilityID::IdNoAbility_MoveToEvasionLocation)
	{
		// sq unit is evading.
		return false;
	}
	if (GetIsValidSquadController())
	{
		// Check for other abilities.
		return M_SquadController->GetIsUnitIdle();
	}
	return true;
}

void ASquadUnit::MoveToEvasionLocation(const FVector& EvasionLocation)
{
	constexpr EAbilityID AbilityNoSquadLogic = EAbilityID::IdNoAbility_MoveToEvasionLocation;
	ExecuteMoveToSelfPathFinding(EvasionLocation, AbilityNoSquadLogic, true);
}

void ASquadUnit::Force_TerminateMovement()
{
	TerminateMovementCommand();
}


TObjectPtr<ASquadController> ASquadUnit::GetSquadControllerChecked() const
{
	if (!IsValid(M_SquadController))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_SquadController",
		                                                      "ASquadUnit::GetSquadControllerChecked");
		return nullptr;
	}
	return M_SquadController;
}

TObjectPtr<AAISquadUnit> ASquadUnit::GetAISquadUnitChecked()
{
	if (!IsValid(M_AISquadUnit))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_AISquadUnit",
		                                                      "ASquadUnit::GetAISquadUnitChecked");
		// Attempt repair.
		if (UWorld* World = GetWorld())
		{
			TObjectPtr<AAISquadUnit> NewAIController = World->SpawnActor<AAISquadUnit>();
			if (IsValid(NewAIController))
			{
				NewAIController->Possess(this);
				M_AISquadUnit = NewAIController;
				return NewAIController;
			}
		}
		return nullptr;
	}
	return M_AISquadUnit;
}

void ASquadUnit::SetSquadController(ASquadController* SquadController)
{
	M_SquadController = SquadController;
	TWeakObjectPtr<ASquadUnit> WeakThis(this);
	auto CallOnDataLoaded = [WeakThis]()-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnDataLoaded_InitExperienceWorth();
	};
	if (M_SquadController)
	{
		M_SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(CallOnDataLoaded, this);
	}
	if (GetIsValidSpatialVoiceLinePlayer())
	{
		SpatialVoiceLinePlayer->OnSquadSet_SetupIdleVoiceLineCheck();
	}
}

UWeaponState* ASquadUnit::GetWeaponState() const
{
	if (!IsValid(M_InfantryWeapon))
	{
		return nullptr;
	}

	TArray<UWeaponState*> Weapons = M_InfantryWeapon->GetWeapons();
	if (Weapons.Num() == 0 || !IsValid(Weapons[0]))
	{
		return nullptr;
	}
	return Weapons[0];
}


bool ASquadUnit::GetHasSecondaryWeapon(bool& OutIsSecondaryWpCompValid)
{
	if (!GetIsValidSecondaryWeapon())
	{
		OutIsSecondaryWpCompValid = false;
		return false;
	}
	// We do have a component that is valid regardless of whether it has a loaded weapon;
	// This lets the squad controller know that this unit can have a secondary weapon (at this component).
	OutIsSecondaryWpCompValid = true;
	return M_SecondaryWeapon->GetHasSecondaryWeapon();
}

void ASquadUnit::OnSpecificTargetInRange()
{
	if (M_ActiveCommand == EAbilityID::IdNoAbility_MoveCloserToTarget)
	{
		// Unit was moving closer now no longer needs to be.
		StopMovementAndClearPath();
		// Continue attacking.
		M_ActiveCommand = EAbilityID::IdAttack;
		Debug_Weapons("Was in range but now close enough: Attack", FColor::Red);
	}
}

void ASquadUnit::OnSpecificTargetOutOfRange(const FVector& TargetLocation)
{
	if (not GetIsValidSquadController())
	{
		return;
	}
	// Is already moving closer?
	if (M_ActiveCommand == EAbilityID::IdNoAbility_MoveCloserToTarget)
	{
		Debug_Weapons("Unit is already moving closer to target, waiting...", FColor::Red);
		return;
	}
	// If one unit is out of range; notify the squad controller to move the full squad closer.
	M_SquadController->OnSquadUnitOutOfRange(TargetLocation);
}

void ASquadUnit::OnSpecificTargetDestroyedOrInvisible()
{
	if (M_ActiveCommand == EAbilityID::IdNoAbility_MoveCloserToTarget)
	{
		StopMovementAndClearPath();
		M_ActiveCommand = EAbilityID::IdAttack;
	}
	OnCommandComplete();
}


ERTSNavAgents ASquadUnit::GetRTSNavAgentType() const
{
	return NavAgentType;
}

void ASquadUnit::GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const
{
	for (const FVector& Offset : AimOffsetPoints)
	{
		OutLocalOffsets.Add(Offset);
	}
}

void ASquadUnit::OnWeaponKilledTarget() const
{
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	if (GetIsSelected())
	{
		// If selected; Queue the voice line to play over radio.
		SpatialVoiceLinePlayer->PlayVoiceLineOverRadio(ERTSVoiceLine::EnemyDestroyed, false, true);
	}
	else
	{
		// If not selected; play the voice line spatial. Stop any other spatial audio.
		SpatialVoiceLinePlayer->PlaySpatialVoiceLine(ERTSVoiceLine::EnemyDestroyed, GetActorLocation(), false);
	}
}

void ASquadUnit::OnWeaponFire()
{
	if (GetIsValidRTSComponent())
	{
		RTSComponent->SetUnitInCombat(true);
	}
	if (M_SquadController)
	{
		M_SquadController->OnSquadUnitInCombat();
	}
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	SpatialVoiceLinePlayer->PlaySpatialVoiceLine(ERTSVoiceLine::Fire, GetActorLocation(), true);
}

void ASquadUnit::OnProjectileHit(const bool bBounced)
{
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	if (bBounced)
	{
		SpatialVoiceLinePlayer->PlaySpatialVoiceLine(ERTSVoiceLine::ShotBounced, GetActorLocation(), false);
	}
	else
	{
		SpatialVoiceLinePlayer->PlaySpatialVoiceLine(ERTSVoiceLine::ShotConnected, GetActorLocation(), false);
	}
}

void ASquadUnit::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetPhysicalMaterials();
	// Find the child weapon actor component.
	BeginPlay_SetupChildActorWeaponComp();
	// Set up a timer with lambda to update the speed of the unit on the anim instance periodically.
	BeginPlay_SetupUpdateAnimSpeed();

	if (IsValid(SelectionComponent) && IsValid(AnimBp_SquadUnit))
	{
		// Bind the selection functions.
		AnimBp_SquadUnit->BindSelectionFunctions(SelectionComponent);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "SelectionComponent or AnimBp_SquadUnit",
		                                             "ASquadUnit::BeginPlay");
	}
	// Disable navigation effects.
	BeginPlay_SetupSelectionHealthCompCollision();
}


void ASquadUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimerHandleUpdateAnim);
		World->GetTimerManager().ClearTimer(M_TimerHandleWeaponSwitch);
		World->GetTimerManager().ClearTimer(M_DeathTimerHandle);
	}
	if (IsValid(M_AISquadUnit))
	{
		M_AISquadUnit->ReceiveMoveCompleted.RemoveDynamic(this, &ASquadUnit::OnMoveCompleted);
	}
	if (IsValid(SelectionComponent) && IsValid(M_SquadController))
	{
		// No error report as is valid.
		const bool bIsSelected = SelectionComponent->GetIsSelected();
		M_SquadController->UnitInSquadDied(this, bIsSelected, ERTSDeathType::Kinetic);
	}
}

void ASquadUnit::BeginDestroy()
{
	Super::BeginDestroy();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimerHandleUpdateAnim);
		World->GetTimerManager().ClearTimer(M_TimerHandleWeaponSwitch);
		World->GetTimerManager().ClearTimer(M_DeathTimerHandle);
		World->GetTimerManager().ClearTimer(M_SpawnSelectionTimerHandle);
	}
}

bool ASquadUnit::GetIsValidSpatialVoiceLinePlayer() const
{
	if (not IsValid(SpatialVoiceLinePlayer))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "SpatialVoiceLinePlayer",
		                                             "ASquadUnit::GetIsValidSpatialVoiceLinePlayer");
		return false;
	}
	return true;
}

void ASquadUnit::OnSquadSpawned(const bool bSetDisabled, const float TimeNotSelectable,
                                const FVector& SquadUnitLocation)
{
	if (bSetDisabled)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		if (IsValid(SelectionComponent))
		{
			SelectionComponent->SetCanBeSelected(false);
		}
		return;
	}

	OnSquadUnitSpawned_SetEnabled(TimeNotSelectable, SquadUnitLocation);
}


void ASquadUnit::StrafeToLocation(const FVector& StrafeLocation)
{
}

void ASquadUnit::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// set up the scavenger component if this unit has one.
	PostInitializeComp_InitScavengerComponent();
	// set up the spatial voice line player
	PostInitializeComp_SetupSpatialVoiceLinePlayer();
	// Set up the repair componet if this unit has one.
	PostInitializeComp_InitRepairComponent();
	// Init owner on secondary weapon.
	PostInitializeComp_InitSecondaryWeapon();

	// Set up capsule collision
	PostInitializeComp_SetupInfantryCapsuleCollision();

	// Set up the reference to the M_AISquadUnit.
	PostInitializeComp_SetupAISquadUnit();
	// set squadUnitAnimInstance
	PostInitializeComp_SetupAnimBP();
}

void ASquadUnit::PostInitializeComp_InitScavengerComponent()
{
	// Setup scavenger component if this unit has one.
	M_ScavengerComponent = FindComponentByClass<UScavengerComponent>();
	if (IsValid(M_ScavengerComponent))
	{
		M_ScavengerComponent->SetSquadOwner(this);
	}
}

void ASquadUnit::PostInitializeComp_SetupSpatialVoiceLinePlayer()
{
	USquadUnitSpatialVoiceLinePlayer* NewSpatialVoiceLinePlayer =
		FindComponentByClass<USquadUnitSpatialVoiceLinePlayer>();
	if (NewSpatialVoiceLinePlayer)
	{
		SpatialVoiceLinePlayer = NewSpatialVoiceLinePlayer;
	}
}

void ASquadUnit::PostInitializeComp_InitRepairComponent()
{
	M_RepairComponent = FindComponentByClass<URepairComponent>();
	if (IsValid(M_RepairComponent))
	{
		M_RepairComponent->SetSquadOwner(this);
	}
}

void ASquadUnit::PostInitializeComp_InitSecondaryWeapon()
{
	if (IsValid(M_SecondaryWeapon))
	{
		M_SecondaryWeapon->InitializeOwner(this);
		return;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this, "M_SecondaryWeapon", "ASquadUnit::PostInitializeComponents");
}

void ASquadUnit::PostInitializeComp_SetupInfantryCapsuleCollision() const
{
	const URTSComponent* MyRTSComponent = FindComponentByClass<URTSComponent>();
	if (IsValid(MyRTSComponent))
	{
		FRTS_CollisionSetup::SetupInfantryCapsuleCollision(GetCapsuleComponent(), MyRTSComponent->GetOwningPlayer());
		return;
	}
	RTSFunctionLibrary::ReportError("cannot access RTS component for squad unit."
		"\n Owner of component: " + GetName() + " does not have a RTS component."
		"function: ASquadUnit::PostInitializeComponents()");
}

void ASquadUnit::PostInitializeComp_SetupAISquadUnit()
{
	AAIController* AIController = Cast<AAIController>(GetController());
	M_AISquadUnit = Cast<AAISquadUnit>(AIController);
	if (!IsValid(M_AISquadUnit))
	{
		// attempt repair by creating a new AI controller
		if (UWorld* World = GetWorld())
		{
			AIController = World->SpawnActor<AAISquadUnit>();
			AIController->Possess(this);
			M_AISquadUnit = Cast<AAISquadUnit>(AIController);
		}
	}
}

void ASquadUnit::PostInitializeComp_SetupAnimBP()
{
	AnimBp_SquadUnit = Cast<USquadUnitAnimInstance>(GetMesh()->GetAnimInstance());
	if (!IsValid(AnimBp_SquadUnit))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "AnimBp_SquadUnit", "ASquadUnit::PostInitializeComponents");
		return;
	}
	AnimBp_SquadUnit->SetSquadUnitMesh(GetMesh());
}

void ASquadUnit::ExecuteMoveToSelfPathFinding(const FVector& MoveToLocation, const EAbilityID AbilityToMoveFor,
                                              const bool bUsePathfinding)
{
	// Used to indicate what to do when move completes.
	M_ActiveCommand = AbilityToMoveFor;
	MoveToAndBindOnCompleted(MoveToLocation, bUsePathfinding, AbilityToMoveFor);
}

void ASquadUnit::ExecuteMoveAlongPath(const FNavPathSharedPtr& Path, const EAbilityID AbilityToMoveFor)
{
	if (!GetIsValidAISquadUnit() || !Path.IsValid())
		return;

	M_ActiveCommand = AbilityToMoveFor;
	M_CurrentPath = Path;

	// Move along the path.
	FAIMoveRequest MoveRequest;
	MoveRequest.SetUsePathfinding(false);
	MoveRequest.SetAcceptanceRadius(DeveloperSettings::GamePlay::Navigation::SquadUnitAcceptanceRadius);
	MoveRequest.SetGoalLocation(Path->GetDestinationLocation());

	M_AISquadUnit->RequestMove(MoveRequest, Path);

	// Bind OnMoveCompleted.
	if (!M_AISquadUnit->ReceiveMoveCompleted.IsAlreadyBound(this, &ASquadUnit::OnMoveCompleted))
	{
		M_AISquadUnit->ReceiveMoveCompleted.AddDynamic(this, &ASquadUnit::OnMoveCompleted);
	}
}

void ASquadUnit::TerminateMovementCommand()
{
	M_ActiveCommand = EAbilityID::IdIdle;

	// Also unbinds the OnMoveCompleted function.
	StopMovementAndClearPath();
}

void ASquadUnit::MoveToAndBindOnCompleted(const FVector& MoveToLocation, const bool bUsePathfinding,
                                          const EAbilityID MoveContext)
{
	if (!GetIsValidAISquadUnit())
	{
		return;
	}

	auto Result = M_AISquadUnit->MoveToLocation(MoveToLocation,
	                                            DeveloperSettings::GamePlay::Navigation::SquadUnitAcceptanceRadius,
	                                            true, bUsePathfinding, false, !bUsePathfinding, nullptr, true);
	if (Result != EPathFollowingRequestResult::RequestSuccessful)
	{
		OnMoveToSelfPathFindingFailed(Result, MoveToLocation, MoveContext);
		return;
	}

	if (!M_AISquadUnit->ReceiveMoveCompleted.IsAlreadyBound(this, &ASquadUnit::OnMoveCompleted))
	{
		M_AISquadUnit->ReceiveMoveCompleted.AddDynamic(this, &ASquadUnit::OnMoveCompleted);
	}
}

void ASquadUnit::MoveToActorAndBindOnCompleted(
	AActor* TargetActor,
	const float AcceptanceRadius,
	const EAbilityID AbilityToMoveFor)
{
	if (!GetIsValidAISquadUnit() || !IsValid(TargetActor))
	{
		OnMoveToActorRequestFailed(TargetActor, AbilityToMoveFor, true,
		                           EPathFollowingRequestResult::Type::Failed);
		return;
	}

	// Clear any existing movement/delegates
	StopMovementAndClearPath();

	// Remember which ability we're performing so OnMoveCompleted can branch on it
	M_ActiveCommand = AbilityToMoveFor;

	// Build the move request toward an actor (which will auto-update if the actor moves)
	constexpr bool bAllowPartialPath = true;
	const FAIMoveRequest MoveRequest = CreateMoveToActorRequest(
		TargetActor, AcceptanceRadius, bAllowPartialPath);

	// Issue the request and grab the generated path (optional)
	FNavPathSharedPtr OutPath;
	const EPathFollowingRequestResult::Type Result =
		M_AISquadUnit->MoveTo(MoveRequest, &OutPath);

	if (Result != EPathFollowingRequestResult::RequestSuccessful)
	{
		OnMoveToActorRequestFailed(
			TargetActor, AbilityToMoveFor, false, Result);
		return;
	}

	// Bind our standard completion handler
	if (!M_AISquadUnit->ReceiveMoveCompleted.IsAlreadyBound(this, &ASquadUnit::OnMoveCompleted))
	{
		M_AISquadUnit->ReceiveMoveCompleted.AddDynamic(this, &ASquadUnit::OnMoveCompleted);
	}
}


void ASquadUnit::ExecuteAttackCommand(AActor* TargetActor)
{
	M_TargetActor = TargetActor;
	M_ActiveCommand = EAbilityID::IdAttack;
	if (GetIsValidWeapon())
	{
		M_InfantryWeapon->SetEngageSpecificTarget(TargetActor);
	}
}

void ASquadUnit::ExecuteAttackGroundCommand(const FVector& TargetLocation)
{
	M_ActiveCommand = EAbilityID::IdAttackGround;
	if (GetIsValidWeapon())
	{
		M_InfantryWeapon->SetEngageGroundLocation(TargetLocation);
	}
}

void ASquadUnit::TerminateAttackGroundCommand()
{
	if (M_ActiveCommand == EAbilityID::IdNoAbility_MoveCloserToTarget)
	{
		// Unit was moving closer to target.
		StopMovementAndClearPath();
		Debug_Weapons("Terminate attack GRND; stop moving closer", FColor::Red);
	}
	M_ActiveCommand = EAbilityID::IdIdle;
	Debug_Weapons("Terminate attack ground; idle", FColor::Red);
	if (GetIsValidWeapon())
	{
		M_InfantryWeapon->SetAutoEngageTargets(false);
	}
}

void ASquadUnit::TerminateAttackCommand()
{
	if (M_ActiveCommand == EAbilityID::IdNoAbility_MoveCloserToTarget)
	{
		// Unit was moving closer to target.
		StopMovementAndClearPath();
		Debug_Weapons("Terminate attack; stop moving closer", FColor::Red);
	}
	M_ActiveCommand = EAbilityID::IdIdle;
	Debug_Weapons("Terminate attack; idle", FColor::Red);
	if (GetIsValidWeapon())
	{
		M_InfantryWeapon->SetAutoEngageTargets(false);
	}
}

void ASquadUnit::ExecutePatrol(const FVector& StartLocation, const FVector& PatrolToLocation)
{
	M_ActiveCommand = EAbilityID::IdPatrol;
	M_LastPatrolMoveTimeOut = 0.f;
	M_PatrolState.PatrolStartLocation = StartLocation;
	M_PatrolState.PatrolEndLocation = PatrolToLocation;
	M_PatrolState.bIsToEndPatrolActive = true;
	MoveToAndBindOnCompleted(PatrolToLocation, true, EAbilityID::IdPatrol);
}

void ASquadUnit::TerminatePatrol()
{
	// Also unbinds the OnMoveCompleted function.
	StopMovementAndClearPath();
	M_ActiveCommand = EAbilityID::IdIdle;
}

void ASquadUnit::ExecuteSwitchWeapon()
{
	bool bValidSecondaryWeapon = false;
	if (!GetHasSecondaryWeapon(bValidSecondaryWeapon) || !bValidSecondaryWeapon)
	{
		const FString SquadName = GetIsValidSquadController() ? M_SquadController->GetName() : "No Squad Controller";
		RTSFunctionLibrary::ReportError("Squad unit ordered to execute switch weapon but has no secondary weapon!"
			"\n Unit: " + GetName() +
			"\n of squad controller: " + SquadName);
		return;
	}
	if (!GetIsWeaponSwitchAllowedToStart())
	{
		return;
	}

	// Start weapon switch sequence.
	bM_IsSwitchingWeapon = true;
	M_ActiveCommand = EAbilityID::IdSwitchWeapon;
	TSoftClassPtr<AInfantryWeaponMaster> SecondaryWeaponClass = M_SecondaryWeapon->GetWeaponClass();
	EWeaponName SecondaryWeaponName = M_SecondaryWeapon->GetWeaponName();
	if (SecondaryWeaponClass.IsValid())
	{
		// Already loaded.
		OnSecondaryWeaponLoaded(SecondaryWeaponName, SecondaryWeaponClass);
	}
	else
	{
		// Asynchronously load the soft class.
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		StreamableManager.RequestAsyncLoad(SecondaryWeaponClass.ToSoftObjectPath(),
		                                   FStreamableDelegate::CreateUObject(
			                                   this, &ASquadUnit::OnSecondaryWeaponLoaded, SecondaryWeaponName,
			                                   SecondaryWeaponClass));
	}
}


void ASquadUnit::TerminateSwitchWeapon()
{
	// Because of this, after switching the unit will no longer call done executing command on the squad controller.
	M_ActiveCommand = EAbilityID::IdIdle;
}

void ASquadUnit::ExecuteRotateTowards(const FRotator& RotateToRotator)
{
	M_ActiveCommand = EAbilityID::IdRotateTowards;
	SetActorRotation(RotateToRotator);
	OnCommandComplete();
}

void ASquadUnit::SetOwningPlayerAndStartFow(const int32 NewOwningPlayer, const float NewVisionRange)
{
	OwningPlayer = NewOwningPlayer;
	FowComponent = NewObject<UFowComp>(this, TEXT("FowComponent"));
	if (not IsValid(FowComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "FowComponent", "ASquadUnit::SetOwwningPlayerAndStartFow");
	}

	FowComponent->RegisterComponent();
	// If the owning player is 1 then the unit is active in fow otherwise it is passive.
	// the unit either has a vision bubble or is hidden/shown depending on their fow position.
	// Note that squad units of the enemy should always have a vision radius and hence are passiveEnemyVision.
	const EFowBehaviour FowBehaviour = OwningPlayer == 1
		                                   ? EFowBehaviour::Fow_Active
		                                   : EFowBehaviour::Fow_PassiveEnemyVision;
	FowComponent->SetVisionRadius(NewVisionRange);
	// No start immediately as this SquadUnit could have been async loaded and does not participate yet.
	FowComponent->StartFow(FowBehaviour, false);
}

bool ASquadUnit::GetIsValidSquadController() const
{
	if (!IsValid(M_SquadController))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_SquadController",
		                                                      "ASquadUnit::GetIsValidSquadController");
		return false;
	}
	return true;
}

bool ASquadUnit::GetIsValidAISquadUnit()
{
	if (IsValid(M_AISquadUnit))
	{
		return true;
	}
	// Try to repair by calling GetAISquadUnitChecked.
	return (GetAISquadUnitChecked() != nullptr);
}


bool ASquadUnit::GetIsValidSecondaryWeapon()
{
	// check for valid otherwise attempt to find component on squad unit.
	if (IsValid(M_SecondaryWeapon))
	{
		return true;
	}
	M_SecondaryWeapon = FindComponentByClass<USecondaryWeapon>();
	return IsValid(M_SecondaryWeapon);
}

bool ASquadUnit::GetIsValidChildWeaponActor()
{
	if (IsValid(M_ChildWeaponComp))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Child weapon actor is null on squad unit but attempting repair."
		"\n squad unit: " + GetName());
	M_ChildWeaponComp = FindComponentByClass<UChildActorComponent>();
	return IsValid(M_ChildWeaponComp);
}

bool ASquadUnit::GetIsValidWeapon() const
{
	if (IsValid(M_InfantryWeapon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_InfantryWeapon",
	                                                      "ASquadUnit::GetIsValidWeapon");
	return false;
}

void ASquadUnit::SetNoDeathVoiceLineOnDeath()
{
	bM_NoDeathVoiceLineOnDeath = true;
}

void ASquadUnit::SetupWeapon(AInfantryWeaponMaster* NewWeapon)
{
	if (IsValid(NewWeapon))
	{
		M_InfantryWeapon = NewWeapon;
		M_ChildWeaponComp = FindComponentByClass<UChildActorComponent>();
	}
}

void ASquadUnit::TerminatePickupWeapon()
{
	// Stop any movement towards the weapon item.
	// Also unbinds the OnMoveCompleted function.
	StopMovementAndClearPath();

	M_ActiveCommand = EAbilityID::IdIdle;
}

bool ASquadUnit::SwapWeaponsWithUnit(ASquadUnit* OtherUnit)
{
	if (not IsValid(OtherUnit))
	{
		return false;
	}

	if (not GetIsValidChildWeaponActor())
	{
		return false;
	}

	if (not OtherUnit->GetIsValidChildWeaponActor())
	{
		return false;
	}

	UChildActorComponent* OtherChildWeapon = OtherUnit->M_ChildWeaponComp;

	const TSubclassOf<AActor> ThisWeaponClass = M_ChildWeaponComp->GetChildActorClass();
	const TSubclassOf<AActor> OtherWeaponClass = OtherChildWeapon->GetChildActorClass();

	M_ChildWeaponComp->DestroyChildActor();
	OtherChildWeapon->DestroyChildActor();

	M_ChildWeaponComp->SetChildActorClass(OtherWeaponClass);
	M_ChildWeaponComp->CreateChildActor();

	OtherChildWeapon->SetChildActorClass(ThisWeaponClass);
	OtherChildWeapon->CreateChildActor();

	SetupSwappedWeapon(Cast<AInfantryWeaponMaster>(M_ChildWeaponComp->GetChildActor()));
	OtherUnit->SetupSwappedWeapon(Cast<AInfantryWeaponMaster>(OtherChildWeapon->GetChildActor()));

	return GetIsValidWeapon() && OtherUnit->GetIsValidWeapon();
}

void ASquadUnit::SetupSwappedWeapon(AInfantryWeaponMaster* NewWeapon)
{
	SetupWeapon(NewWeapon);

	if (not IsValid(NewWeapon))
	{
		return;
	}

	NewWeapon->SetupOwner(this);

	if (IsValid(RTSComponent))
	{
		NewWeapon->SetOwningPlayer(RTSComponent->GetOwningPlayer());
	}

	NewWeapon->DisableWeaponSearch(true);

	if (IsValid(AnimBp_SquadUnit))
	{
		AnimBp_SquadUnit->SetWeaponAimOffset(NewWeapon->GetAimOffsetType());
	}
}

void ASquadUnit::SetWeaponToAutoEngageTargets(const bool bUseLastTarget)
{
	if (not GetIsValidWeapon())
	{
		return;
	}
	M_InfantryWeapon->SetAutoEngageTargets(bUseLastTarget);
}


void ASquadUnit::StartPickupWeapon(AWeaponPickup* TargetWeaponItem)
{
	if (!IsValid(TargetWeaponItem) || !IsValid(M_ChildWeaponComp))
	{
		RTSFunctionLibrary::ReportError(
			"Either the weapon item or the child weapon component"
			" of the squad unit is null"
			"\n not able to StartPickupWeapon for unit: " + GetName());
		return;
	}
	if (!GetIsWeaponSwitchAllowedToStart())
	{
		if constexpr (DeveloperSettings::Debugging::GSquadUnit_SwitchPickUp_Weapons_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Unit cannot pick up weapon because it is still switching weapons."
			                                "\n squad unit: " + GetName(), FColor::Black);
		}
		return;
	}
	// Start Weapon switch sequence.
	bM_IsSwitchingWeapon = true;
	TSoftClassPtr<AInfantryWeaponMaster> NewWeaponClass = TargetWeaponItem->GetWeaponClass();
	EWeaponName NewWeaponName = TargetWeaponItem->GetWeaponName();
	// Make sure no new units can overlap.
	TargetWeaponItem->SetItemDisabled();
	TargetWeaponItem->Destroy();
	if (NewWeaponClass.IsValid())
	{
		// Already loaded
		OnSecondaryWeaponLoaded(NewWeaponName, NewWeaponClass);
	}
	else
	{
		// Asynchronously load the soft class
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		StreamableManager.RequestAsyncLoad(NewWeaponClass.ToSoftObjectPath(),
		                                   FStreamableDelegate::CreateUObject(
			                                   this, &ASquadUnit::OnSecondaryWeaponLoaded, NewWeaponName,
			                                   NewWeaponClass));
	}
}

bool ASquadUnit::GetHasValidScavengerComp() const
{
	return IsValid(M_ScavengerComponent);
}

bool ASquadUnit::GetHasValidRepairComp() const
{
	return IsValid(M_RepairComponent);
}


void ASquadUnit::OnSecondaryWeaponLoaded(
	const EWeaponName NewWeaponName,
	TSoftClassPtr<AInfantryWeaponMaster> NewWeaponClass)
{
	// Validate preconditions.
	if (!OnSecondaryWeapon_ValidatePreconditions())
	{
		return;
	}

	TSoftClassPtr<AInfantryWeaponMaster> OldPrimaryWeaponClass = nullptr;
	EWeaponName OldPrimaryWeaponName = EWeaponName::DEFAULT_WEAPON;

	// Transfer the primary weapon details to the secondary one.
	OnSecondaryWeapon_TransferPrimaryWeaponDetailsToSecondary(OldPrimaryWeaponClass, OldPrimaryWeaponName);

	// Destroy the old child weapon actor.
	OnSecondaryWeapon_DestroyOldWeaponActor();

	// Spawn and initialize the new weapon.
	OnSecondaryWeapon_SpawnAndInitializeNewWeapon(NewWeaponClass);
}

bool ASquadUnit::OnSecondaryWeapon_ValidatePreconditions()
{
	return GetIsValidChildWeaponActor() && IsValid(AnimBp_SquadUnit) && IsValid(RTSComponent);
}

void ASquadUnit::OnSecondaryWeapon_TransferPrimaryWeaponDetailsToSecondary(
	TSoftClassPtr<AInfantryWeaponMaster>& OutOldPrimaryWeaponClass,
	EWeaponName& OutOldPrimaryWeaponName)
{
	if (IsValid(M_InfantryWeapon))
	{
		OutOldPrimaryWeaponClass = M_InfantryWeapon->GetClass();
		if (M_InfantryWeapon->GetWeapons().Num() > 0 && IsValid(M_InfantryWeapon->GetWeapons()[0]))
		{
			OutOldPrimaryWeaponName = M_InfantryWeapon->GetWeapons()[0]->GetRawWeaponData().WeaponName;
		}
		M_InfantryWeapon->DisableWeaponSearch(true);
		if (GetIsValidSecondaryWeapon())
		{
			M_SecondaryWeapon->SetupSecondaryWeaponMesh(
				M_InfantryWeapon->GetWeaponMesh(), OutOldPrimaryWeaponName, OutOldPrimaryWeaponClass);
		}
	}
}

void ASquadUnit::OnSecondaryWeapon_DestroyOldWeaponActor() const
{
	if (IsValid(M_ChildWeaponComp->GetChildActor()))
	{
		M_ChildWeaponComp->GetChildActor()->Destroy();
	}
	M_ChildWeaponComp->SetChildActorClass(nullptr);
}

void ASquadUnit::OnSecondaryWeapon_SpawnAndInitializeNewWeapon(
	TSoftClassPtr<AInfantryWeaponMaster> NewWeaponClass)
{
	M_ChildWeaponComp->SetChildActorClass(NewWeaponClass.Get());
	M_ChildWeaponComp->CreateChildActor();

	if (not IsValid(M_ChildWeaponComp->GetChildActor()))
	{
		return;
	}
	M_InfantryWeapon = Cast<AInfantryWeaponMaster>(M_ChildWeaponComp->GetChildActor());
	if (!IsValid(M_InfantryWeapon))
	{
		RTSFunctionLibrary::ReportError("Spawned new weapon child actor but cannot cast to infantry weapon!"
			"\n squad unit: " + GetName());
		return;
	}
	M_InfantryWeapon->SetupOwner(this);
	M_InfantryWeapon->SetOwningPlayer(RTSComponent->GetOwningPlayer());
	M_InfantryWeapon->DisableWeaponSearch(true);
	AnimBp_SquadUnit->SetWeaponAimOffset(M_InfantryWeapon->GetAimOffsetType());
	AnimBp_SquadUnit->PlaySwitchWeaponMontage(M_InfantryWeapon->GetAimOffsetType());

	// Bind a lambda to call OnSwitchWeaponCompleted after 1 second.
	TWeakObjectPtr<ASquadUnit> SquadUnitWeak = this;
	FTimerDelegate TimerDelSwitchWeapon;
	TimerDelSwitchWeapon.BindLambda([SquadUnitWeak]()
	{
		if (not SquadUnitWeak.IsValid())
		{
			return;
		}
		SquadUnitWeak->OnSwitchWeaponCompleted();
	});
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(M_TimerHandleWeaponSwitch, TimerDelSwitchWeapon, 1.f, false);
	}
}


void ASquadUnit::OnSwitchWeaponCompleted()
{
	bM_IsSwitchingWeapon = false;
	if (not GetIsValidWeapon())
	{
		return;
	}
	if (M_ActiveCommand == EAbilityID::IdAttack && IsValid(M_TargetActor))
	{
		M_InfantryWeapon->SetEngageSpecificTarget(M_TargetActor);
		return;
	}
	M_InfantryWeapon->SetAutoEngageTargets(true);
	if (M_ActiveCommand == EAbilityID::IdSwitchWeapon)
	{
		OnCommandComplete();
	}
}

bool ASquadUnit::GetIsWeaponSwitchAllowedToStart()
{
	if (bM_IsSwitchingWeapon)
	{
		RTSFunctionLibrary::ReportError("Squad unit was ordered to switch weapon but is still active "
			"with another switch command!!");
		return false;
	}
	return true;
}

void ASquadUnit::AttachEffectAtEquipmentMesh(UNiagaraSystem* Effect, const FName EffectSocket)
{
	if (not IsValid(Effect) || not IsValid(M_ScavengeEquipmentMesh))
	{
		return;
	}
	// attach effect on equipment at effect socket.
	UNiagaraComponent* ScavengeEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Effect, M_ScavengeEquipmentMesh, EffectSocket, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset, true);
	if (IsValid(ScavengeEffect))
	{
		ScavengeEffect->SetAutoDestroy(true);
		M_ScavengeEquipmentEffect = ScavengeEffect;
	}
}


void ASquadUnit::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (Result != EPathFollowingResult::Type::Success)
	{
		ReportPathFollowingResultError(Result);
	}
	switch (M_ActiveCommand)
	{
	case EAbilityID::IdPatrol:
		OnMoveCompleted_Patrol();
		break;
	case EAbilityID::IdScavenge:
		OnMoveCompleted_Scavenge();
		break;
	case EAbilityID::IdRepair:
		OnMoveCompleted_Repair();
		break;
	case EAbilityID::IdCapture:
		OnMoveCompleted_Capture();
		break;
	case EAbilityID::IdThrowGrenade:
		if (GetIsValidSquadController())
		{
			M_SquadController->OnSquadUnitArrivedAtThrowGrenadeLocation(this);
		}
		break;
	case EAbilityID::IdAimAbility:
		break;
	case EAbilityID::IdNoAbility_MoveCloserToTarget:
		OnMoveCompleted_MoveCloserToTarget();
		break;
	case EAbilityID::IdFieldConstruction:
		break;
	default:
		// In case of other commands, notify the squad controller.
		OnCommandComplete();
	}
}

void ASquadUnit::OnMoveCompleted_Patrol()
{
	const float TimeNow = FPlatformTime::Seconds();
	if (not FMath::IsNearlyZero(M_LastPatrolMoveTimeOut) && FMath::Abs(M_LastPatrolMoveTimeOut - TimeNow) < 1)
	{
		RTSFunctionLibrary::PrintString(
			"Last patrol move timeout is very close to now, aborting patrol.");
		// Notify the squad controller that the patrol command is completed.
		StopMovementAndClearPath();
		OnCommandComplete();
		return;
	}
	M_LastPatrolMoveTimeOut = TimeNow;
	if (M_PatrolState.bIsToEndPatrolActive)
	{
		MoveToAndBindOnCompleted(M_PatrolState.PatrolStartLocation, true, EAbilityID::IdPatrol);
	}
	else
	{
		MoveToAndBindOnCompleted(M_PatrolState.PatrolEndLocation, true, EAbilityID::IdPatrol);
	}
	M_PatrolState.bIsToEndPatrolActive = !M_PatrolState.bIsToEndPatrolActive;
}

void ASquadUnit::OnMoveCompleted_Scavenge()
{
	if (not GetHasValidScavengerComp())
	{
		// Notify the squad controller that the scavenging command is completed. 
		OnCommandComplete();
		return;
	}
	M_ScavengerComponent->MoveToLocationComplete();
}

void ASquadUnit::OnMoveCompleted_Repair() const
{
	if (not EnsureRepairComponentIsValid())
	{
		return;
	}
	M_RepairComponent->OnFinishedMovementForRepairAbility();
}

void ASquadUnit::OnMoveCompleted_Capture() const
{
	if (not GetIsValidSquadController())
	{
		return;
	}
	M_SquadController->OnSquadUnitArrivedAtCaptureActor();
}

void ASquadUnit::OnMoveCompleted_MoveCloserToTarget()
{
	// Moved closer to target, still attacking.
	M_ActiveCommand = EAbilityID::IdAttack;
	Debug_Weapons("Unit Finished moving closer to target", FColor::Purple);
}

void ASquadUnit::DetermineDeathVoiceLine()
{
	if (not GetIsValidSelectionComponent() || not GetIsValidSpatialVoiceLinePlayer() || not GetIsValidRTSComponent())
	{
		return;
	}
	if (GetIsSelected() && GetIsValidSquadController())
	{
		M_SquadController->PlaySquadUnitLostVoiceLine();
	}
	else
	{
		SpatialVoiceLinePlayer->PlaySpatialVoiceLine(ERTSVoiceLine::UnitDies, GetActorLocation(), true);
	}
}


void ASquadUnit::BeginPlay_SetupSelectionHealthCompCollision() const
{
	if (not IsValid(SelectionComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "SelectionComponent",
		                                             "ASquadUnit::SetupSelectionCompNavigation");
		return;
	}
	if (UBoxComponent* BoxComponent = SelectionComponent->GetSelectionArea())
	{
		BoxComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		BoxComponent->SetCanEverAffectNavigation(false);
	}
	if (UDecalComponent* DecalComponent = SelectionComponent->GetSelectedDecal())
	{
		DecalComponent->SetCanEverAffectNavigation(false);
	}
	if (not IsValid(HealthComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "HealthComponent",
		                                             "ASquadUnit::SetupSelectionCompNavigation");
		return;
	}
	if (UWidgetComponent* HealthBarMesh = HealthComponent->GetHealthBarWidgetComp())
	{
		HealthBarMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		HealthBarMesh->SetCanEverAffectNavigation(false);
	}
}

void ASquadUnit::BeginPlay_SetPhysicalMaterials() const
{
	if (not IsValid(GetMesh()) || not IsValid(PhysicalMaterialOverride))
	{
		return;
	}
	USkeletalMeshComponent* MeshComp = GetMesh();
	MeshComp->SetPhysMaterialOverride(PhysicalMaterialOverride);
}

void ASquadUnit::TerminateScavenging()
{
	if (IsValid(M_ScavengeEquipmentEffect))
	{
		M_ScavengeEquipmentEffect->DestroyComponent();
	}
	if (IsValid(M_ScavengeEquipmentMesh))
	{
		M_ScavengeEquipmentMesh->DestroyComponent();
	}
	M_ScavengeEquipmentMesh = nullptr;
	M_ScavengeEquipmentEffect = nullptr;

	// Re-enable weapon systems, after it was hidden by the scavenger equipment.
	if (IsValid(M_InfantryWeapon))
	{
		M_InfantryWeapon->SetAutoEngageTargets(true);
	}

	// Reset animations
	if (IsValid(AnimBp_SquadUnit))
	{
		if (GetHasValidScavengerComp())
		{
			AnimBp_SquadUnit->StopAllMontages();
		}
	}

	// Important to do this after checking whether the squad unit was patrolling.
	if (GetHasValidScavengerComp())
	{
		M_ScavengerComponent->TerminateScavenging();
	}

	// Important to do this after the scavenging component has reset to stop the patrol flag.
	// Also unbinds the OnMoveCompleted function.
	StopMovementAndClearPath();

	M_ActiveCommand = EAbilityID::IdIdle;
}


void ASquadUnit::UnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		return;
	}

	SetUnitDying();
	// Check if the no vl on death flag is set, if not determine and play death voice line.
	if (DeathType != ERTSDeathType::Scavenging && not bM_NoDeathVoiceLineOnDeath)
	{
		DetermineDeathVoiceLine();
	}

	bool bIsSelected = false;

	// Update selection state and complete any active command.
	UnitDies_HandleSelectionAndCommand(bIsSelected);

	// Inform the squad controller that this unit has died.
	UnitDies_RemoveFromSquadController(bIsSelected, DeathType);

	// Notify the animation instance (unbind selection delegates etc.).
	UnitDies_NotifyAnimInstance();

	if (DeathType != ERTSDeathType::Scavenging)
	{
		// Start ragdoll simulation and ensure the weapon is destroyed at this moment.
		M_RagdollSettings.StartRagdoll(this, GetMesh(), AnimBp_SquadUnit, M_InfantryWeapon);
	}
	else
	{
		// Scavenging death; no voice line and no physics ragdoll.
		SetActorHiddenInGame(true);
	}
	M_InfantryWeapon = nullptr;


	// Remove the unit from its AI controller.
	UnitDies_RemoveFromAIController();

	// Clean up timers, health bar, collision and deregister from the game state.
	UnitDies_CleanupComponents();

	OnUnitDies.Broadcast();

	// Schedule the actual destruction of this unit after a short delay.
	UnitDies_ScheduleDestruction();
}


void ASquadUnit::UnitDies_HandleSelectionAndCommand(bool& OutIsSelected)
{
	OutIsSelected = false;
	if (IsValid(SelectionComponent))
	{
		SelectionComponent->SetCanBeSelected(false);
		SelectionComponent->HideDecals();
		OutIsSelected = SelectionComponent->IsSelected();
	}
	if (M_ActiveCommand != EAbilityID::IdIdle)
	{
		OnCommandComplete();
	}
}

void ASquadUnit::UnitDies_RemoveFromSquadController(bool bIsSelected, const ERTSDeathType DeathType)
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	M_SquadController->HandleWeaponSwitchOnUnitDeath(this);
	M_SquadController->UnitInSquadDied(this, bIsSelected, DeathType);
}

void ASquadUnit::UnitDies_NotifyAnimInstance()
{
	if (IsValid(AnimBp_SquadUnit))
	{
		AnimBp_SquadUnit->UnitDies();
	}
}

void ASquadUnit::UnitDies_DestroyInfantryWeapon()
{
	if (IsValid(M_InfantryWeapon))
	{
		M_InfantryWeapon->DisableAllWeapons();
		M_InfantryWeapon->Destroy();
	}
}

void ASquadUnit::UnitDies_RemoveFromAIController()
{
	if (IsValid(M_AISquadUnit))
	{
		if (IsValid(M_AISquadUnit->GetBrainComponent()))
		{
			M_AISquadUnit->GetBrainComponent()->StopLogic("Unit Died");
		}
		M_AISquadUnit->UnPossess();
		M_AISquadUnit->Destroy();
	}
}

void ASquadUnit::UnitDies_CleanupComponents()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TimerHandleUpdateAnim);
		World->GetTimerManager().ClearTimer(M_TimerHandleWeaponSwitch);
		World->GetTimerManager().ClearTimer(M_DeathTimerHandle);
		World->GetTimerManager().ClearTimer(M_SpawnSelectionTimerHandle);
	}

	if (IsValid(HealthComponent))
	{
		HealthComponent->MakeHealthBarInvisible();
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (not M_RagdollSettings.bIsRagdollActive)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (IsValid(RTSComponent))
	{
		RTSComponent->DeregisterFromGameState();
	}
}


void ASquadUnit::UnitDies_ScheduleDestruction()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float ClampedTimeTillDeath = FMath::Max(M_RagdollSettings.TimeTillDeath, 0.0f);

	FTimerDelegate DeathDelegate;
	TWeakObjectPtr<ASquadUnit> SquadUnitWeak = this;

	DeathDelegate.BindLambda([SquadUnitWeak]()
	{
		if (not SquadUnitWeak.IsValid())
		{
			return;
		}

		ASquadUnit* SquadUnit = SquadUnitWeak.Get();
		SquadUnit->Destroy();
	});

	World->GetTimerManager().SetTimer(M_DeathTimerHandle, DeathDelegate, ClampedTimeTillDeath, false);
}

void ASquadUnit::StopMovementAndClearPath()
{
	if (GetIsValidAISquadUnit())
	{
		M_AISquadUnit->ReceiveMoveCompleted.RemoveDynamic(this, &ASquadUnit::OnMoveCompleted);
		M_CurrentPath.Reset();
		M_AISquadUnit->StopMovement();
	}
}

void ASquadUnit::Debug_Weapons(const FString& DebugMessage, const FColor Color)
{
	if constexpr (DeveloperSettings::Debugging::GSquadUnit_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Sq Unit: " + DebugMessage, Color);
	}
}

void ASquadUnit::OnDataLoaded_InitExperienceWorth()
{
	if (not GetIsValidSquadController())
	{
		return;
	}
	M_ExperienceWorth = M_SquadController->GetExperienceWorth();
	if (M_ExperienceWorth <= 0)
	{
		RTSFunctionLibrary::ReportError("When attempting to derive the experience worth from the squad controller"
			"an invalid value was provided by it. "
			"\n For Squad unit: " + GetName() +
			"\n Experience value: " + FString::FromInt(M_ExperienceWorth));
	}
}

void ASquadUnit::OnMoveToSelfPathFindingFailed(const EPathFollowingRequestResult::Type RequestResult,
                                               const FVector& MoveToLocation, const EAbilityID MoveContext)
{
	const FString AIControllerName = IsValid(M_AISquadUnit) ? M_AISquadUnit->GetName() : "Invalid AI Controller";
	FString BaseMessage = "Move to self path finding failed for unit: " + GetName() +
		"\n AI Controller: " + AIControllerName +
		"\n Move Location: " + MoveToLocation.ToString() +
		"\n Ability: " + UEnum::GetValueAsString(MoveContext) +
		"\n Request Result: " + UEnum::GetValueAsString(RequestResult);

	RTSFunctionLibrary::ReportError(BaseMessage);
}

void ASquadUnit::OnMoveToActorRequestFailed(const AActor* TargetActor, const EAbilityID AbilityToMoveFor,
                                            const bool bFailedBeforeMakingRequest,
                                            const EPathFollowingRequestResult::Type
                                            RequestResult)
{
	const FString ActorName = IsValid(TargetActor) ? TargetActor->GetName() : "Invalid Actor";
	const FString AIControllerName = IsValid(M_AISquadUnit) ? M_AISquadUnit->GetName() : "Invalid AI Controller";
	FString BaseMessage = "Move to actor request failed for unit: " + GetName() +
		"\n AI Controller: " + AIControllerName +
		"\n Target Actor: " + ActorName +
		"\n Ability: " + UEnum::GetValueAsString(AbilityToMoveFor) +
		"\n Request Result: " + UEnum::GetValueAsString(RequestResult);
	// todo actions to take on failure.
	if (bFailedBeforeMakingRequest)
	{
		BaseMessage += "\n Failed before making request. likely no valid actor";
	}
	else
	{
		switch (RequestResult)
		{
		case EPathFollowingRequestResult::Failed:
			break;
		case EPathFollowingRequestResult::AlreadyAtGoal:
			if (AbilityToMoveFor == EAbilityID::IdRepair && IsValid(M_RepairComponent))
			{
				BaseMessage += "\n Signaling to repair component to finished movement to repair object";
				RTSFunctionLibrary::ReportError(BaseMessage);
				M_RepairComponent->OnFinishedMovementForRepairAbility();
			}
			break;
		case EPathFollowingRequestResult::RequestSuccessful:
			break;
		}
	}
	RTSFunctionLibrary::ReportError(BaseMessage);
}

FAIMoveRequest ASquadUnit::CreateMoveToActorRequest(AActor* GoalActor, const float AcceptanceRadius,
                                                    const bool bAllowPartialPathFinding)
{
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(GoalActor);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(bAllowPartialPathFinding);
	MoveRequest.SetCanStrafe(false);
	// UE auto fails movement if not on NavMesh, we want to prevent this when we do partial path finding.
	MoveRequest.SetRequireNavigableEndLocation(not bAllowPartialPathFinding);
	// Let the partial path handle the end location.
	MoveRequest.SetProjectGoalLocation(true);
	return MoveRequest;
}


void ASquadUnit::OnScavengeStart(UStaticMesh* ScavengeEquipment, const FName ScavengeSocketName,
                                 float TotalScavengeTime,
                                 const TObjectPtr<AScavengeableObject>
                                 & ScaveObj, const TObjectPtr<UNiagaraSystem>& Effect, const FName EffectSocket)
{
	if (!IsValid(ScaveObj))
	{
		RTSFunctionLibrary::ReportError("Scav object is not valid in OnScavengeStart."
			"\n squad unit: " + GetName());
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GScavenging_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Start scavenging on unit: " + GetName(), FColor::Blue);
	}
	// Attach equipment mesh to primary mesh and attach effect to this equipment.
	OnScavengeStart_SetupScavengeEquipmentMesh(ScavengeEquipment, ScavengeSocketName, Effect, EffectSocket);
	if (GetIsValidWeapon())
	{
		M_InfantryWeapon->DisableWeaponSearch(true, true);
	}
	const FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), ScaveObj->GetActorLocation());
	SetActorRotation(LookAt);
	// create the equipment mesh if valid and attach at socket name to the mesh of the unit.
	// Start timer on scav comp, if already done this will do nothing (done by other squad unit)
	ScaveObj->StartScavengeTimer(GetSquadControllerChecked());
	if (IsValid(AnimBp_SquadUnit))
	{
		AnimBp_SquadUnit->PlayWeldingMontage();
	}
}

void ASquadUnit::OnScavengeStart_SetupScavengeEquipmentMesh(UStaticMesh* ScavengeEquipment,
                                                            const FName& ScavengeSocketName,
                                                            const TObjectPtr<UNiagaraSystem>& Effect,
                                                            const FName& EffectSocketName)
{
	if (not IsValid(ScavengeEquipment))
	{
		RTSFunctionLibrary::ReportError("No valid equipment mesh provided for the scavenging process."
			"\n see squad unit: " + GetName() +
			"\n At Function: ASquadUnit::OnScavengeStart_SetupScavengeEquipementMesh");
		return;
	}
	UStaticMeshComponent* ScavengeMesh = NewObject<UStaticMeshComponent>(this);
	ScavengeMesh->SetStaticMesh(ScavengeEquipment);
	ScavengeMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale,
	                                ScavengeSocketName);
	ScavengeMesh->RegisterComponent();
	ScavengeMesh->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	ScavengeMesh->SetCanEverAffectNavigation(false);
	M_ScavengeEquipmentMesh = ScavengeMesh;
	if (M_ScavengeEquipmentMesh)
	{
		AttachEffectAtEquipmentMesh(Effect, EffectSocketName);
	}
}


void ASquadUnit::OnCommandComplete()
{
	if (GetIsValidSquadController())
	{
		// Notify the squad controller that this unit has completed its command.
		M_SquadController->OnSquadUnitCommandComplete(M_ActiveCommand);
		M_ActiveCommand = EAbilityID::IdIdle;
	}
}

void ASquadUnit::ReportPathFollowingResultError(const EPathFollowingResult::Type Result) const
{
	switch (Result)
	{
	case EPathFollowingResult::Success:
		return;
	case EPathFollowingResult::Blocked:
		RTSFunctionLibrary::PrintString("Path following result is blocked."
			"\n squad unit: " + GetName());
		return;
	case EPathFollowingResult::OffPath:
		RTSFunctionLibrary::ReportError("path following result is Off path."
			"\n squad unit: " + GetName());
		return;
	case EPathFollowingResult::Aborted:
		RTSFunctionLibrary::PrintString("path following result is Aborted."
			"\n squad unit: " + GetName());
		return;
	case EPathFollowingResult::Invalid:
		RTSFunctionLibrary::ReportError("path following result is Invalid."
			"\n squad unit: " + GetName());
	}
}

void ASquadUnit::BeginPlay_SetupChildActorWeaponComp()
{
	// Find child actor component that contains the infantry weapon.
	TArray<UChildActorComponent*> ChildActorComponents;
	GetComponents<UChildActorComponent>(ChildActorComponents);
	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (not IsValid(ChildActorComponent))
		{
			continue;
		}
		UClass* ChildActorClass = ChildActorComponent->GetChildActorClass();
		if (not IsValid(ChildActorClass))
		{
			continue;
		}
		if (ChildActorClass->IsChildOf(AInfantryWeaponMaster::StaticClass()))
		{
			SetupWeapon(Cast<AInfantryWeaponMaster>(ChildActorComponent->GetChildActor()));
			M_ChildWeaponComp = ChildActorComponent;
			break;
		}
	}
	if (!IsValid(M_ChildWeaponComp))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "M_ChildWeaponComp", "ASquadUnit::BeginPlay");
	}
}

void ASquadUnit::BeginPlay_SetupUpdateAnimSpeed()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	TWeakObjectPtr<ASquadUnit> SquadUnitWeak(this);

	FTimerDelegate TimerDelUpdateAnim;
	TimerDelUpdateAnim.BindLambda([SquadUnitWeak]()
	{
		if (not SquadUnitWeak.IsValid())
		{
			return;
		}

		ASquadUnit* SquadUnit = SquadUnitWeak.Get();

		if (not IsValid(SquadUnit->AnimBp_SquadUnit))
		{
			return;
		}

		SquadUnit->AnimBp_SquadUnit->UpdateAnimState(SquadUnit->GetVelocity());
	});

	World->GetTimerManager().SetTimer(
		M_TimerHandleUpdateAnim,
		TimerDelUpdateAnim,
		DeveloperSettings::Optimization::UpdateSquadAnimInterval,
		true);
}


bool ASquadUnit::EnsureRepairComponentIsValid() const
{
	if (not IsValid(M_RepairComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "M_RepairComponent",
		                                             "ASquadUnit::EnsureRepairComponentIsValid");
		return false;
	}
	return true;
}

void ASquadUnit::OnSquadUnitSpawned_SetEnabled(const float TimeNotSelectable, const FVector& SquadUnitLocation)
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	if (FMath::IsNearlyZero(TimeNotSelectable) || TimeNotSelectable <= 0.f)
	{
		OnSquadUnitSpawned_TeleportAndSetSelectable(SquadUnitLocation);
		return;
	}
	TWeakObjectPtr<ASquadUnit> WeakThis(this);
	auto TeleportAfterDelay = [WeakThis, SquadUnitLocation]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnSquadUnitSpawned_TeleportAndSetSelectable(SquadUnitLocation);
	};
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(M_SpawnSelectionTimerHandle, TeleportAfterDelay, TimeNotSelectable, false);
	}
}

void ASquadUnit::OnSquadUnitSpawned_TeleportAndSetSelectable(const FVector& SquadUnitLocation)
{
	SetActorLocation(SquadUnitLocation);
	if (IsValid(SelectionComponent))
	{
		SelectionComponent->SetCanBeSelected(true);
	}
}
