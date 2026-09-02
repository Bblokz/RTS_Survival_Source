// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "StandaloneTurret.h"

#include "AnimStandaloneTurret.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Collapse/CollapseBySwapParameters.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Collapse/FRTS_Collapse/FRTS_Collapse.h"
#include "RTS_Survival/Collapse/VerticalCollapse/FRTS_VerticalCollapse.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateLaser.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateMultiHitLaser.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace
{
	constexpr float RotationSoundStopThresholdRatio = 0.5f;

	template <typename TWeaponParameters>
	void PrepareStandaloneWeaponParameters(
		TWeaponParameters& WeaponParameters,
		AStandaloneTurret* StandaloneTurret,
		USkeletalMeshComponent* TurretMesh,
		const int32 OwningPlayer)
	{
		WeaponParameters.OwningPlayer = OwningPlayer;
		WeaponParameters.WeaponOwner.SetObject(StandaloneTurret);
		WeaponParameters.WeaponOwner.SetInterface(StandaloneTurret);
		if (not IsValid(WeaponParameters.MeshComponent))
		{
			WeaponParameters.MeshComponent = TurretMesh;
		}
	}

	template <typename TWeaponState>
	TWeaponState* CreateStandaloneWeaponState(AStandaloneTurret* StandaloneTurret, const FString& WeaponType)
	{
		TWeaponState* WeaponState = NewObject<TWeaponState>(StandaloneTurret);
		if (IsValid(WeaponState))
		{
			return WeaponState;
		}

		RTSFunctionLibrary::ReportError(
			"Failed to create " + WeaponType + " weapon state for standalone turret: "
			+ StandaloneTurret->GetName());
		return nullptr;
	}
}

AStandaloneTurret::AStandaloneTurret(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetActorTickInterval(DeveloperSettings::Optimization::TurretTickInterval);

	M_TurretMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("StandaloneTurretMesh"));
	SetRootComponent(M_TurretMesh);
}

void AStandaloneTurret::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	HealthComponent = FindComponentByClass<UHealthComponent>();
	RTSComponent = FindComponentByClass<URTSComponent>();
	SelectionComponent = FindComponentByClass<USelectionComponent>();
	if (not GetIsValidHealthComponent()
		|| not GetIsValidRTSComponent()
		|| not GetIsValidSelectionComponent())
	{
		return;
	}

	RTSComponent->SetUnitType(EAllUnitType::UNType_StandaloneTurret);
	if (M_StandaloneTurretSubtype == EStandaloneTurretSubtype::StandaloneTurret_None)
	{
		RTSFunctionLibrary::ReportError(
			"Standalone turret subtype is not configured on: " + GetName());
		return;
	}

	RTSComponent->SetStandaloneTurretSubtype(M_StandaloneTurretSubtype);
	BP_PostInit();
}

void AStandaloneTurret::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_SetupUnitData();
	BeginPlay_InitAnimationInstance();
	BeginPlay_InitRotationAudioComponent();
	BeginPlay_InitTargetingAndCollision();
	SetupCallbackToProjectileManager();
	BeginPlay_BindMutualDestructionListeners();

	if (M_Weapons.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Standalone turret has no weapons after BP_PostInit: " + GetName());
		return;
	}

	if (bM_AutoEngageOnBeginPlay && GetIsValidAnimInstance())
	{
		SetAutoEngageTargets(false);
	}
}

void AStandaloneTurret::BeginPlay_SetupUnitData()
{
	if (not GetIsValidRTSComponent() || not GetIsValidHealthComponent())
	{
		return;
	}

	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not IsValid(GameState))
	{
		RTSFunctionLibrary::ReportError(
			"Could not load standalone turret data because the game state is invalid for: " + GetName());
		return;
	}

	const FStandaloneTurretData StandaloneTurretData = GameState->GetStandaloneTurretDataOfPlayer(
		RTSComponent->GetOwningPlayer(),
		RTSComponent->GetSubtypeAsStandaloneTurretSubtype());
	if (StandaloneTurretData.MaxHealth <= 0.0f)
	{
		return;
	}

	M_RotationSettings.YawTurnRateDegreesPerSecond =
		StandaloneTurretData.TurretRotationSpeedDegreesPerSecond;
	InitAbilityArray(StandaloneTurretData.Abilities);
	HealthComponent->InitHealthAndResistance(
		StandaloneTurretData.ResistancesAndDamageMlt,
		StandaloneTurretData.MaxHealth);
}

void AStandaloneTurret::BeginPlay_InitAnimationInstance()
{
	if (not GetIsValidTurretMesh())
	{
		SetActorTickEnabled(false);
		return;
	}

	M_AnimInstance = Cast<UAnimStandaloneTurret>(M_TurretMesh->GetAnimInstance());
	if (not GetIsValidAnimInstance())
	{
		SetActorTickEnabled(false);
		return;
	}

	M_AimState.CurrentYawDegrees = FRotator::NormalizeAxis(M_AnimInstance->GetCurrentYawAngle());
	M_AimState.CurrentPitchDegrees = M_AnimInstance->GetCurrentPitchAngle();
}

void AStandaloneTurret::BeginPlay_InitRotationAudioComponent()
{
	if (not IsValid(M_RotationSoundSettings.RotationSound)
		|| not IsValid(M_RotationSoundSettings.SoundAttenuation)
		|| not IsValid(M_RotationSoundSettings.SoundConcurrency)
		|| not GetIsValidTurretMesh())
	{
		return;
	}

	M_RotationAudioComponent = NewObject<UAudioComponent>(
		this,
		UAudioComponent::StaticClass(),
		TEXT("StandaloneTurretRotationAudio"));
	if (not GetIsValidRotationAudioComponent())
	{
		return;
	}

	M_RotationAudioComponent->bAutoActivate = false;
	M_RotationAudioComponent->bAutoDestroy = false;
	M_RotationAudioComponent->bAllowSpatialization = true;
	M_RotationAudioComponent->SetSound(M_RotationSoundSettings.RotationSound);
	M_RotationAudioComponent->AttenuationSettings = M_RotationSoundSettings.SoundAttenuation;
	M_RotationAudioComponent->ConcurrencySet.Add(M_RotationSoundSettings.SoundConcurrency);
	AddInstanceComponent(M_RotationAudioComponent);
	M_RotationAudioComponent->SetupAttachment(M_TurretMesh);
	M_RotationAudioComponent->RegisterComponent();
	M_RotationSoundState = EStandaloneTurretRotationSoundState::Ready;
}

void AStandaloneTurret::BeginPlay_InitTargetingAndCollision()
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}

	RefreshOwningPlayerConfiguration();
}

void AStandaloneTurret::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindMutualDestructionListeners();
	StopRotationSound();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	}

	StopAllWeaponsFire(true);
	Super::EndPlay(EndPlayReason);
}

void AStandaloneTurret::UnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		return;
	}

	DisableTurret();
	SetUnitDying();
	if (GetIsValidRTSComponent())
	{
		RTSComponent->DeregisterFromGameState();
	}
	UnitDies_RemoveFromSelection();
	OnUnitDies.Broadcast();
	BP_OnStandaloneTurretDies(DeathType);
}

void AStandaloneTurret::BeginPlay_BindMutualDestructionListeners()
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->IsGameWorld())
	{
		return;
	}

	for (const TWeakObjectPtr<ABuildingExpansion>& BuildingExpansion :
		 M_MutualDestructionBuildingExpansions)
	{
		if (not BuildingExpansion.IsValid())
		{
			continue;
		}

		FOnUnitDies* UnitDiesDelegate = BuildingExpansion->GetOnUnitDiesDelegate();
		if (UnitDiesDelegate != nullptr)
		{
			UnitDiesDelegate->AddUObject(this, &AStandaloneTurret::HandleMutualDestructionActorDied);
		}
	}

	for (const TWeakObjectPtr<ADestructableEnvActor>& DestructableActor : M_MutualDestructionEnvActors)
	{
		if (not DestructableActor.IsValid())
		{
			continue;
		}

		DestructableActor->OnDestructableEnvActorDied.AddUObject(
			this,
			&AStandaloneTurret::HandleMutualDestructionActorDied);
	}
}

void AStandaloneTurret::UnbindMutualDestructionListeners()
{
	for (const TWeakObjectPtr<ABuildingExpansion>& BuildingExpansion :
		 M_MutualDestructionBuildingExpansions)
	{
		if (not BuildingExpansion.IsValid())
		{
			continue;
		}

		FOnUnitDies* UnitDiesDelegate = BuildingExpansion->GetOnUnitDiesDelegate();
		if (UnitDiesDelegate != nullptr)
		{
			UnitDiesDelegate->RemoveAll(this);
		}
	}

	for (const TWeakObjectPtr<ADestructableEnvActor>& DestructableActor : M_MutualDestructionEnvActors)
	{
		if (DestructableActor.IsValid())
		{
			DestructableActor->OnDestructableEnvActorDied.RemoveAll(this);
		}
	}
}

void AStandaloneTurret::HandleMutualDestructionActorDied()
{
	UnitDies(ERTSDeathType::Scavenging);
}

void AStandaloneTurret::UnitDies_RemoveFromSelection()
{
	if (not GetIsValidSelectionComponent() || not SelectionComponent->GetIsSelected())
	{
		return;
	}

	ACPPController* RTSController = Cast<ACPPController>(UGameplayStatics::GetPlayerController(this, 0));
	if (not IsValid(RTSController))
	{
		SetUnitSelected(false);
		return;
	}

	RTSController->RemoveActorFromSelectionAndUpdateUI(this);
}

void AStandaloneTurret::VerticalDestruction(
	const FRTSVerticalCollapseSettings& CollapseSettings,
	const FCollapseFX& CollapseFX)
{
	FRTSVerticalCollapseSettings NonDestroyingCollapseSettings = CollapseSettings;
	NonDestroyingCollapseSettings.bDestroyPostVerticalCollapse = false;
	const TWeakObjectPtr<AStandaloneTurret> WeakThis(this);
	auto OnFinished = [WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->OnVerticalDestructionComplete();
	};
	FRTS_VerticalCollapse::StartVerticalCollapse(
		this,
		NonDestroyingCollapseSettings,
		CollapseFX,
		MoveTemp(OnFinished));
}

void AStandaloneTurret::OnVerticalDestructionComplete()
{
	BP_OnVerticalDestructionComplete();
}

void AStandaloneTurret::CollapseMeshWithSwapping(
	FSwapToDestroyedMesh CollapseParameters,
	const bool bNoLongerBlockWeaponsPostCollapse,
	UNiagaraSystem* AttachSystem,
	USoundCue* AttachSound,
	const FVector AttachOffset)
{
	CollapseParameters.ComponentToSwapOn = PrepareDestroyedMeshSwapComponent(
		CollapseParameters.ComponentToSwapOn);
	if (not IsValid(CollapseParameters.ComponentToSwapOn))
	{
		return;
	}

	if (bNoLongerBlockWeaponsPostCollapse)
	{
		CollapseParameters.ComponentToSwapOn->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Ignore);
		CollapseParameters.ComponentToSwapOn->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Ignore);
	}

	FRTS_Collapse::CollapseSwapMesh(this, CollapseParameters);
	AttemptAttachSpawnSystem(CollapseParameters, AttachSystem);
	if (IsValid(AttachSound))
	{
		constexpr float DestructionSoundVolume = 1.0f;
		constexpr float DestructionSoundPitch = 1.0f;
		constexpr float DestructionSoundStartTime = 0.0f;
		UGameplayStatics::PlaySoundAtLocation(
			this,
			AttachSound,
			AttachOffset,
			FRotator::ZeroRotator,
			DestructionSoundVolume,
			DestructionSoundPitch,
			DestructionSoundStartTime,
			CollapseParameters.Attenuation,
			CollapseParameters.SoundConcurrency);
	}
}

UMeshComponent* AStandaloneTurret::PrepareDestroyedMeshSwapComponent(UMeshComponent* ComponentToSwapOn)
{
	if (not IsValid(ComponentToSwapOn))
	{
		RTSFunctionLibrary::ReportError(
			"Cannot collapse mesh with swapping for standalone turret: ComponentToSwapOn is invalid.");
		return nullptr;
	}
	if (ComponentToSwapOn->IsA<UStaticMeshComponent>())
	{
		return ComponentToSwapOn;
	}
	if (not ComponentToSwapOn->IsA<USkeletalMeshComponent>())
	{
		RTSFunctionLibrary::ReportError(
			"Standalone turret destroyed-mesh swapping requires a static or skeletal mesh component on: "
			+ GetName());
		return nullptr;
	}

	UStaticMeshComponent* DestroyedMeshComponent = NewObject<UStaticMeshComponent>(
		this,
		TEXT("StandaloneTurretDestroyedMesh"));
	if (not IsValid(DestroyedMeshComponent))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to create the destroyed static mesh component for standalone turret: " + GetName());
		return nullptr;
	}

	DestroyedMeshComponent->SetMobility(ComponentToSwapOn->Mobility);
	DestroyedMeshComponent->SetupAttachment(ComponentToSwapOn);
	DestroyedMeshComponent->SetRelativeTransform(FTransform::Identity);
	DestroyedMeshComponent->SetCollisionEnabled(ComponentToSwapOn->GetCollisionEnabled());
	DestroyedMeshComponent->SetCollisionObjectType(ComponentToSwapOn->GetCollisionObjectType());
	DestroyedMeshComponent->SetCollisionResponseToChannels(ComponentToSwapOn->GetCollisionResponseToChannels());
	DestroyedMeshComponent->SetGenerateOverlapEvents(ComponentToSwapOn->GetGenerateOverlapEvents());
	DestroyedMeshComponent->SetCanEverAffectNavigation(ComponentToSwapOn->CanEverAffectNavigation());
	DestroyedMeshComponent->SetReceivesDecals(true);
	AddInstanceComponent(DestroyedMeshComponent);
	DestroyedMeshComponent->RegisterComponent();

	ComponentToSwapOn->SetVisibility(false, false);
	ComponentToSwapOn->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return DestroyedMeshComponent;
}

void AStandaloneTurret::AttemptAttachSpawnSystem(
	const FSwapToDestroyedMesh& CollapseParameters,
	UNiagaraSystem* AttachSystem)
{
	if (not IsValid(AttachSystem))
	{
		return;
	}

	UNiagaraComponent* NiagaraComponent = NewObject<UNiagaraComponent>(this);
	if (not IsValid(NiagaraComponent))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to create attached destruction system for standalone turret: " + GetName());
		return;
	}

	NiagaraComponent->SetAsset(AttachSystem);
	NiagaraComponent->AttachToComponent(
		CollapseParameters.ComponentToSwapOn,
		FAttachmentTransformRules::KeepRelativeTransform);
	NiagaraComponent->Activate();
}

void AStandaloneTurret::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (M_TargetingData.GetMode() == ETargetMode::None)
	{
		PauseRotationSound();
		return;
	}
	if (not GetIsValidTurretMesh() || not GetIsValidAnimInstance())
	{
		PauseRotationSound();
		return;
	}

	UpdateAimSolution();
	UpdateAnimationSteering(DeltaTime);
}

void AStandaloneTurret::SetAutoEngageTargets(const bool bUseLastTarget)
{
	M_WeaponAIState = EWeaponAIState::AutoEngage;
	StopAllWeaponsFire(not bUseLastTarget);
	StartEngagementTimer();
	UpdateAutoEngage();
}

void AStandaloneTurret::SetEngageSpecificTarget(AActor* TargetActor)
{
	if (not RTSFunctionLibrary::RTSIsValid(TargetActor))
	{
		return;
	}
	if (M_WeaponAIState == EWeaponAIState::SpecificEngage
		&& M_TargetingData.GetTargetActor() == TargetActor)
	{
		return;
	}

	StopAllWeaponsFire(true);
	M_WeaponAIState = EWeaponAIState::SpecificEngage;
	SetTargetActor(TargetActor);
	StartEngagementTimer();
	UpdateSpecificEngage();
}

void AStandaloneTurret::SetEngageGroundLocation(const FVector& GroundLocation)
{
	StopAllWeaponsFire(true);
	M_WeaponAIState = EWeaponAIState::TargetGround;
	M_TargetingData.SetTargetGround(GroundLocation);
	M_AimState.bIsAlignedToTarget = false;
	UpdateAimSolution();
	StartEngagementTimer();
	UpdateGroundEngage();
}

void AStandaloneTurret::DisableTurret()
{
	M_WeaponAIState = EWeaponAIState::None;
	bM_IsPendingTargetSearch = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	}

	StopAllWeaponsFire(true);
	DisableAllWeapons();
}

AActor* AStandaloneTurret::GetCurrentTargetActor() const
{
	return M_TargetingData.GetTargetActor();
}

float AStandaloneTurret::GetMaxWeaponRange() const
{
	return M_WeaponRangeData.M_MaxWeaponRange;
}

int32 AStandaloneTurret::GetOwningPlayerForWeaponInit() const
{
	if (not GetIsValidRTSComponent())
	{
		return 0;
	}

	return RTSComponent->GetOwningPlayer();
}

USkeletalMeshComponent* AStandaloneTurret::GetTurretMesh() const
{
	if (not GetIsValidTurretMesh())
	{
		return nullptr;
	}

	return M_TurretMesh;
}

TArray<UWeaponState*> AStandaloneTurret::GetWeapons()
{
	TArray<UWeaponState*> Weapons;
	Weapons.Reserve(M_Weapons.Num());
	for (UWeaponState* Weapon : M_Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapons.Add(Weapon);
		}
	}
	return Weapons;
}

void AStandaloneTurret::RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister)
{
	if (not RTSFunctionLibrary::RTSIsValid(ActorToIgnore))
	{
		return;
	}

	for (UWeaponState* Weapon : M_Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->RegisterActorToIgnore(ActorToIgnore, bRegister);
		}
	}
}

void AStandaloneTurret::ForceSetAllWeaponsFullyReloaded()
{
	for (UWeaponState* Weapon : M_Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->ForceInstantReload();
		}
	}
}

float AStandaloneTurret::GetTurretYawLimit() const
{
	return 0.0f;
}

ETargetPreference AStandaloneTurret::GetTargetPreference() const
{
	return M_TargetPreference;
}

void AStandaloneTurret::SetTargetPreference(const ETargetPreference NewTargetPreference)
{
	if (M_TargetPreference == ETargetPreference::Aircraft)
	{
		return;
	}

	M_TargetPreference = NewTargetPreference;
}

void AStandaloneTurret::GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const
{
	OutLocalOffsets = M_AimOffsetPoints;
}

void AStandaloneTurret::PropagateNewTargetPreference(const ETargetPreference TargetPreference)
{
	SetTargetPreference(TargetPreference);
}

void AStandaloneTurret::ExecuteAttackCommand(AActor* TargetActor)
{
	SetEngageSpecificTarget(TargetActor);
}

void AStandaloneTurret::TerminateAttackCommand()
{
	SetAutoEngageTargets(false);
}

void AStandaloneTurret::ExecuteAttackGroundCommand(const FVector GroundLocation)
{
	if (GroundLocation.IsNearlyZero())
	{
		DoneExecutingCommand(EAbilityID::IdAttackGround);
		return;
	}

	SetEngageGroundLocation(GroundLocation);
}

void AStandaloneTurret::TerminateAttackGroundCommand()
{
	SetAutoEngageTargets(false);
}

void AStandaloneTurret::SetUnitToIdleSpecificLogic()
{
	SetAutoEngageTargets(false);
}

void AStandaloneTurret::OnUnitIdleAndNoNewCommands()
{
	Super::OnUnitIdleAndNoNewCommands();
	SetAutoEngageTargets(false);
}

void AStandaloneTurret::StartEngagementTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || M_Weapons.IsEmpty() || not GetIsValidAnimInstance())
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	World->GetTimerManager().SetTimer(
		M_WeaponSearchHandle,
		this,
		&AStandaloneTurret::UpdateEngagement,
		M_WeaponSearchTimeInterval,
		true);
}

void AStandaloneTurret::UpdateEngagement()
{
	RefreshOwningPlayerConfiguration();

	switch (M_WeaponAIState)
	{
	case EWeaponAIState::AutoEngage:
		UpdateAutoEngage();
		return;
	case EWeaponAIState::SpecificEngage:
		UpdateSpecificEngage();
		return;
	case EWeaponAIState::TargetGround:
		UpdateGroundEngage();
		return;
	case EWeaponAIState::None:
	default:
		return;
	}
}

void AStandaloneTurret::UpdateAutoEngage()
{
	if (not M_TargetingData.GetIsTargetValid())
	{
		StopAllWeaponsFire(true);
		RequestClosestTargets();
		return;
	}

	M_TargetingData.TickAimSelection();
	const FVector TargetLocation = M_TargetingData.GetActiveTargetLocation();
	if (not GetIsTargetWithinWeaponRange(TargetLocation))
	{
		StopAllWeaponsFire(true);
		RequestClosestTargets();
		return;
	}

	EngageCurrentTarget();
}

void AStandaloneTurret::UpdateSpecificEngage()
{
	if (not M_TargetingData.GetIsTargetValid())
	{
		HandleInvalidSpecificTarget();
		return;
	}

	M_TargetingData.TickAimSelection();
	if (not GetIsTargetWithinWeaponRange(M_TargetingData.GetActiveTargetLocation()))
	{
		StopAllWeaponsFire(false);
		return;
	}

	EngageCurrentTarget();
}

void AStandaloneTurret::UpdateGroundEngage()
{
	if (M_TargetingData.GetMode() != ETargetMode::Ground)
	{
		StopAllWeaponsFire(true);
		return;
	}
	if (not GetIsTargetWithinWeaponRange(M_TargetingData.GetActiveTargetLocation()))
	{
		StopAllWeaponsFire(false);
		return;
	}

	EngageCurrentTarget();
}

void AStandaloneTurret::EngageCurrentTarget()
{
	UpdateAimSolution();
	if (not M_AimState.bIsAlignedToTarget)
	{
		StopAllWeaponsFire(false);
		return;
	}

	FireAllWeapons();
}

void AStandaloneTurret::HandleInvalidSpecificTarget()
{
	StopAllWeaponsFire(true);
	CompleteActiveAttackCommand();
}

void AStandaloneTurret::RequestClosestTargets()
{
	if (bM_IsPendingTargetSearch || M_WeaponRangeData.M_WeaponSearchRadius <= 0.0f)
	{
		return;
	}
	if (not GetIsValidGameUnitManger())
	{
		return;
	}

	const TWeakObjectPtr<AStandaloneTurret> WeakThis(this);
	bM_IsPendingTargetSearch = true;
	M_GameUnitManager->RequestClosestTargets(
		GetActorLocation(),
		M_WeaponRangeData.M_WeaponSearchRadius,
		M_TargetSearchResultCount,
		GetOwningPlayerForWeaponInit(),
		M_TargetPreference,
		[WeakThis](const TArray<AActor*>& Targets)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnTargetsFound(Targets);
			}
		},
		FMath::Max(M_MinimumTargetRange, 0.0f));
}

void AStandaloneTurret::OnTargetsFound(const TArray<AActor*>& Targets)
{
	bM_IsPendingTargetSearch = false;
	if (M_WeaponAIState != EWeaponAIState::AutoEngage)
	{
		return;
	}

	for (AActor* Target : Targets)
	{
		if (not RTSFunctionLibrary::RTSIsVisibleTarget(Target, GetOwningPlayerForWeaponInit()))
		{
			continue;
		}
		if (GetIsTargetBelowMinimumRange(Target->GetActorLocation()))
		{
			continue;
		}

		SetTargetActor(Target);
		return;
	}
}

void AStandaloneTurret::SetTargetActor(AActor* TargetActor)
{
	if (not RTSFunctionLibrary::RTSIsValid(TargetActor))
	{
		return;
	}

	M_TargetingData.SetTargetActor(TargetActor);
	M_AimState.bIsAlignedToTarget = false;
	UpdateAimSolution();
}

void AStandaloneTurret::ResetTarget()
{
	M_TargetingData.ResetTarget();
	M_AimState.bIsAlignedToTarget = false;
	M_AimState.bIsTargetWithinPitchLimits = true;
	PauseRotationSound();
}

void AStandaloneTurret::UpdateAimSolution()
{
	if (M_TargetingData.GetMode() == ETargetMode::None || not GetIsValidTurretMesh())
	{
		return;
	}

	const FVector DirectionToTarget = M_TargetingData.GetActiveTargetLocation() - GetAimOriginLocation();
	const FVector LocalDirectionToTarget = M_TurretMesh->GetComponentTransform().InverseTransformVectorNoScale(
		DirectionToTarget);
	const FRotator LocalLookAtRotation = LocalDirectionToTarget.Rotation();
	const float LocalPitch = LocalLookAtRotation.Pitch;

	// The AO's numeric yaw matches component-local yaw: +90 aims toward +Y and -90 aims toward -Y.
	// Its Left/Right animation asset names use the opposite naming convention, but their sample coordinates do not.
	M_AimState.TargetYawDegrees = FRotator::NormalizeAxis(
		FMath::FindDeltaAngleDegrees(
			M_RotationSettings.NeutralPoseForwardYawDegrees,
			LocalLookAtRotation.Yaw));
	M_AimState.TargetPitchDegrees = FMath::Clamp(
		LocalPitch,
		M_RotationSettings.MinimumPitchDegrees,
		M_RotationSettings.MaximumPitchDegrees);
	M_AimState.FireDirection = DirectionToTarget.GetSafeNormal();
	M_AimState.bIsTargetWithinPitchLimits =
		LocalPitch >= M_RotationSettings.MinimumPitchDegrees
		&& LocalPitch <= M_RotationSettings.MaximumPitchDegrees;
	UpdateAimAlignment();
}

void AStandaloneTurret::UpdateAnimationSteering(const float DeltaTime)
{
	const float PreviousYawDegrees = M_AimState.CurrentYawDegrees;
	const float YawErrorBeforeAdjustmentDegrees = FMath::Abs(FMath::FindDeltaAngleDegrees(
		M_AimState.CurrentYawDegrees,
		M_AimState.TargetYawDegrees));

	// FixedTurn returns [0, 360), while Aim Offsets consume this turret's signed [-180, 180] yaw axis.
	M_AimState.CurrentYawDegrees = FRotator::NormalizeAxis(
		FMath::FixedTurn(
			M_AimState.CurrentYawDegrees,
			M_AimState.TargetYawDegrees,
			M_RotationSettings.YawTurnRateDegreesPerSecond * DeltaTime));
	M_AimState.CurrentPitchDegrees = FMath::FInterpConstantTo(
		M_AimState.CurrentPitchDegrees,
		M_AimState.TargetPitchDegrees,
		DeltaTime,
		M_RotationSettings.PitchTurnRateDegreesPerSecond);

	M_AnimInstance->SetYaw(M_AimState.CurrentYawDegrees);
	M_AnimInstance->SetPitch(M_AimState.CurrentPitchDegrees);
	UpdateRotationSound(PreviousYawDegrees, YawErrorBeforeAdjustmentDegrees);
	UpdateAimAlignment();
}

void AStandaloneTurret::UpdateRotationSound(
	const float PreviousYawDegrees,
	const float YawErrorBeforeAdjustmentDegrees)
{
	if (M_RotationSoundState == EStandaloneTurretRotationSoundState::Disabled)
	{
		return;
	}

	const float YawMovementDegrees = FMath::Abs(FMath::FindDeltaAngleDegrees(
		PreviousYawDegrees,
		M_AimState.CurrentYawDegrees));
	if (FMath::IsNearlyZero(YawMovementDegrees))
	{
		PauseRotationSound();
		return;
	}

	const float StartThresholdDegrees = FMath::Max(
		M_RotationSoundSettings.MinimumYawAdjustmentDegrees,
		0.0f);
	const float ActiveThresholdDegrees = StartThresholdDegrees * RotationSoundStopThresholdRatio;
	const float RequiredAdjustmentDegrees =
		M_RotationSoundState == EStandaloneTurretRotationSoundState::Playing
			? ActiveThresholdDegrees
			: StartThresholdDegrees;
	if (YawErrorBeforeAdjustmentDegrees < RequiredAdjustmentDegrees)
	{
		PauseRotationSound();
		return;
	}

	PlayRotationSound();
}

void AStandaloneTurret::PlayRotationSound()
{
	if (M_RotationSoundState == EStandaloneTurretRotationSoundState::Disabled
		|| M_RotationSoundState == EStandaloneTurretRotationSoundState::Playing
		|| not GetIsValidRotationAudioComponent())
	{
		return;
	}

	if (M_RotationSoundState == EStandaloneTurretRotationSoundState::Ready)
	{
		M_RotationAudioComponent->Play();
	}
	else
	{
		M_RotationAudioComponent->SetPaused(false);
	}
	M_RotationSoundState = EStandaloneTurretRotationSoundState::Playing;
}

void AStandaloneTurret::PauseRotationSound()
{
	if (M_RotationSoundState != EStandaloneTurretRotationSoundState::Playing
		|| not GetIsValidRotationAudioComponent())
	{
		return;
	}

	M_RotationAudioComponent->SetPaused(true);
	M_RotationSoundState = EStandaloneTurretRotationSoundState::Paused;
}

void AStandaloneTurret::StopRotationSound()
{
	if (M_RotationSoundState == EStandaloneTurretRotationSoundState::Disabled
		|| not GetIsValidRotationAudioComponent())
	{
		return;
	}

	M_RotationAudioComponent->Stop();
	M_RotationSoundState = EStandaloneTurretRotationSoundState::Ready;
}

void AStandaloneTurret::UpdateAimAlignment()
{
	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(
		M_AimState.CurrentYawDegrees,
		M_AimState.TargetYawDegrees));
	const float PitchError = FMath::Abs(M_AimState.CurrentPitchDegrees - M_AimState.TargetPitchDegrees);
	M_AimState.bIsAlignedToTarget = M_AimState.bIsTargetWithinPitchLimits
		&& YawError <= M_RotationSettings.AllowedAimErrorDegrees
		&& PitchError <= M_RotationSettings.AllowedAimErrorDegrees;
}

FVector AStandaloneTurret::GetAimOriginLocation() const
{
	if (not GetIsValidTurretMesh())
	{
		return GetActorLocation();
	}
	if (M_AimOriginSocketName != NAME_None && M_TurretMesh->DoesSocketExist(M_AimOriginSocketName))
	{
		return M_TurretMesh->GetSocketLocation(M_AimOriginSocketName);
	}

	return M_TurretMesh->GetComponentLocation();
}

bool AStandaloneTurret::GetIsTargetWithinWeaponRange(const FVector& TargetLocation) const
{
	if (GetIsTargetBelowMinimumRange(TargetLocation))
	{
		return false;
	}

	return FVector::DistSquared(TargetLocation, GetActorLocation())
		<= M_WeaponRangeData.M_MaxWeaponRangeSquared;
}

bool AStandaloneTurret::GetIsTargetBelowMinimumRange(const FVector& TargetLocation) const
{
	const float MinimumRange = FMath::Max(M_MinimumTargetRange, 0.0f);
	if (MinimumRange <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return FVector::DistSquared(TargetLocation, GetActorLocation()) < FMath::Square(MinimumRange);
}

void AStandaloneTurret::StopAllWeaponsFire(const bool bInvalidateTarget)
{
	M_AimState.bIsAlignedToTarget = false;
	if (bInvalidateTarget)
	{
		ResetTarget();
	}

	for (UWeaponState* Weapon : M_Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->StopFire(false, true);
		}
	}
}

void AStandaloneTurret::DisableAllWeapons()
{
	for (UWeaponState* Weapon : M_Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->DisableWeapon();
		}
	}
}

void AStandaloneTurret::FireAllWeapons()
{
	if (GetIsValidRTSComponent())
	{
		RTSComponent->SetUnitInCombat(true);
	}

	for (UWeaponState* Weapon : M_Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->Fire(M_TargetingData.GetActiveTargetLocation());
		}
	}
}

void AStandaloneTurret::AddInitializedWeapon(UWeaponState* Weapon)
{
	if (not IsValid(Weapon))
	{
		RTSFunctionLibrary::ReportError("Cannot add invalid weapon to standalone turret: " + GetName());
		return;
	}

	M_Weapons.Add(Weapon);
	UpdateWeaponRangeBasedOnWeapons();
	if (ASmallArmsProjectileManager* ProjectileManager = M_ProjectileManager.Get())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(Weapon, ProjectileManager);
	}
}

void AStandaloneTurret::UpdateWeaponRangeBasedOnWeapons()
{
	const TArray<UWeaponState*> Weapons = GetWeapons();
	float FlameThrowerRange = 0.0f;
	if (FRTSWeaponHelpers::GetAdjustedRangeIfFlameThrowerPresent(Weapons, FlameThrowerRange))
	{
		M_WeaponRangeData.ForceSetRange(FlameThrowerRange);
		return;
	}

	M_WeaponRangeData.RecalculateRangeFromWeapons(Weapons);
}

void AStandaloneTurret::SetupCallbackToProjectileManager()
{
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not IsValid(GameState))
	{
		return;
	}

	const TWeakObjectPtr<AStandaloneTurret> WeakThis(this);
	GameState->RegisterCallbackForSmallArmsProjectileMgr(
		[WeakThis](const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnProjectileManagerLoaded(ProjectileManager);
			}
		},
		this);
}

void AStandaloneTurret::OnProjectileManagerLoaded(
	const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)
{
	if (not IsValid(ProjectileManager))
	{
		RTSFunctionLibrary::ReportError(
			"Invalid projectile manager supplied to standalone turret: " + GetName());
		return;
	}

	M_ProjectileManager = ProjectileManager;
	for (UWeaponState* Weapon : M_Weapons)
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(Weapon, ProjectileManager);
	}
}

void AStandaloneTurret::SetupTargetableCollision() const
{
	if (not GetIsValidTurretMesh() || not GetIsValidRTSComponent())
	{
		return;
	}

	M_TurretMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	M_TurretMesh->SetCollisionObjectType(ECC_WorldStatic);
	M_TurretMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_TurretMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	M_TurretMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	M_TurretMesh->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
	M_TurretMesh->SetGenerateOverlapEvents(true);
	M_TurretMesh->SetCanEverAffectNavigation(true);
	M_TurretMesh->SetReceivesDecals(true);

	constexpr int32 PlayerOwnerIndex = 1;
	const ECollisionChannel WeaponTraceChannel = RTSComponent->GetOwningPlayer() == PlayerOwnerIndex
		? COLLISION_TRACE_PLAYER
		: COLLISION_TRACE_ENEMY;
	M_TurretMesh->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Block);
}

void AStandaloneTurret::RefreshOwningPlayerConfiguration()
{
	if (not GetIsValidRTSComponent())
	{
		return;
	}

	const int32 OwningPlayer = RTSComponent->GetOwningPlayer();
	if (M_CachedOwningPlayer == OwningPlayer)
	{
		return;
	}

	M_CachedOwningPlayer = OwningPlayer;
	M_TargetingData.InitTargetStruct(OwningPlayer);
	StopAllWeaponsFire(true);
	SetupTargetableCollision();
	for (UWeaponState* Weapon : M_Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->ChangeOwningPlayer(OwningPlayer);
		}
	}
}

void AStandaloneTurret::CompleteActiveAttackCommand()
{
	UCommandData* CommandData = GetIsValidCommandData();
	if (IsValid(CommandData) && CommandData->GetCurrentActiveCommand() == EAbilityID::IdAttack)
	{
		DoneExecutingCommand(EAbilityID::IdAttack);
		return;
	}

	SetAutoEngageTargets(false);
}

UWorld* AStandaloneTurret::GetWeaponSetupWorld(const FString& SetupFunctionName) const
{
	if (not GetIsValidTurretMesh())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		return World;
	}

	RTSFunctionLibrary::ReportError(
		"World is invalid for standalone turret: " + GetName() + " in " + SetupFunctionName);
	return nullptr;
}

bool AStandaloneTurret::GetIsValidTurretMesh() const
{
	if (IsValid(M_TurretMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_TurretMesh",
		"AStandaloneTurret::GetIsValidTurretMesh",
		this);
	return false;
}

bool AStandaloneTurret::GetIsValidAnimInstance() const
{
	if (M_AnimInstance.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_AnimInstance",
		"AStandaloneTurret::GetIsValidAnimInstance",
		this);
	return false;
}

bool AStandaloneTurret::GetIsValidRotationAudioComponent() const
{
	if (IsValid(M_RotationAudioComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_RotationAudioComponent",
		"AStandaloneTurret::GetIsValidRotationAudioComponent",
		this);
	return false;
}

void AStandaloneTurret::OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon)
{
	(void)WeaponIndex;
	(void)Weapon;
}

void AStandaloneTurret::OnWeaponBehaviourChangesRange(
	const UWeaponState* ReportingWeaponState,
	const float NewRange)
{
	(void)ReportingWeaponState;
	(void)NewRange;
	UpdateWeaponRangeBasedOnWeapons();
}

FVector& AStandaloneTurret::GetFireDirection(const int32 WeaponIndex)
{
	(void)WeaponIndex;
	return M_AimState.FireDirection;
}

FVector& AStandaloneTurret::GetTargetLocation(const int32 WeaponIndex)
{
	(void)WeaponIndex;
	return M_TargetingData.GetActiveTargetLocation();
}

AActor* AStandaloneTurret::GetTargetActor(const int32 WeaponIndex) const
{
	(void)WeaponIndex;
	return M_TargetingData.GetTargetActor();
}

bool AStandaloneTurret::AllowWeaponToReload(const int32 WeaponIndex) const
{
	(void)WeaponIndex;
	return true;
}

void AStandaloneTurret::OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor)
{
	(void)WeaponIndex;
	if (not M_TargetingData.WasKilledActorCurrentTarget(KilledActor))
	{
		return;
	}

	CompleteActiveAttackCommand();
}

void AStandaloneTurret::PlayWeaponAnimation(
	const int32 WeaponIndex,
	const EWeaponFireMode FireMode,
	const int32 WeaponCalibre)
{
	(void)WeaponCalibre;
	BP_PlayWeaponAnimation(WeaponIndex, FireMode);
}

void AStandaloneTurret::OnReloadStart(const int32 WeaponIndex, const float ReloadTime)
{
	BP_ReloadWeapon(WeaponIndex, ReloadTime);
}

void AStandaloneTurret::OnProjectileHit(const bool bBounced)
{
	BP_OnProjectileHit(bBounced);
}

void AStandaloneTurret::OnReloadFinished(const int32 WeaponIndex)
{
	BP_OnReloadFinished(WeaponIndex);
}

void AStandaloneTurret::SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupDirectHitWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		DirectHitWeaponParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateDirectHit* DirectHit = CreateStandaloneWeaponState<UWeaponStateDirectHit>(this, TEXT("direct hit"));
	if (not IsValid(DirectHit))
	{
		return;
	}

	DirectHit->InitDirectHitWeapon(
		DirectHitWeaponParameters.OwningPlayer,
		M_Weapons.Num(),
		DirectHitWeaponParameters.WeaponName,
		DirectHitWeaponParameters.WeaponBurstMode,
		DirectHitWeaponParameters.WeaponOwner,
		DirectHitWeaponParameters.MeshComponent,
		DirectHitWeaponParameters.FireSocketName,
		World,
		DirectHitWeaponParameters.WeaponVFX,
		DirectHitWeaponParameters.WeaponShellCase,
		DirectHitWeaponParameters.BurstCooldown,
		DirectHitWeaponParameters.SingleBurstAmountMaxBurstAmount,
		DirectHitWeaponParameters.MinBurstAmount,
		DirectHitWeaponParameters.CreateShellCasingOnEveryRandomBurst);
	AddInitializedWeapon(DirectHit);
}

void AStandaloneTurret::SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupTraceWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		TraceWeaponParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateTrace* TraceWeapon = CreateStandaloneWeaponState<UWeaponStateTrace>(this, TEXT("trace"));
	if (not IsValid(TraceWeapon))
	{
		return;
	}

	TraceWeapon->InitTraceWeapon(
		TraceWeaponParameters.OwningPlayer,
		M_Weapons.Num(),
		TraceWeaponParameters.WeaponName,
		TraceWeaponParameters.WeaponBurstMode,
		TraceWeaponParameters.WeaponOwner,
		TraceWeaponParameters.MeshComponent,
		TraceWeaponParameters.FireSocketName,
		World,
		TraceWeaponParameters.WeaponVFX,
		TraceWeaponParameters.WeaponShellCase,
		TraceWeaponParameters.BurstCooldown,
		TraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		TraceWeaponParameters.MinBurstAmount,
		TraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		TraceWeaponParameters.TraceProjectileType);
	AddInitializedWeapon(TraceWeapon);
}

void AStandaloneTurret::SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupMultiTraceWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		MultiTraceWeaponParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateMultiTrace* MultiTrace =
		CreateStandaloneWeaponState<UWeaponStateMultiTrace>(this, TEXT("multi trace"));
	if (not IsValid(MultiTrace))
	{
		return;
	}

	MultiTrace->InitMultiTraceWeapon(
		MultiTraceWeaponParameters.OwningPlayer,
		M_Weapons.Num(),
		MultiTraceWeaponParameters.WeaponName,
		MultiTraceWeaponParameters.WeaponBurstMode,
		MultiTraceWeaponParameters.WeaponOwner,
		MultiTraceWeaponParameters.MeshComponent,
		World,
		MultiTraceWeaponParameters.FireSocketNames,
		MultiTraceWeaponParameters.bUseOwnerFireDirection,
		MultiTraceWeaponParameters.WeaponVFX,
		MultiTraceWeaponParameters.WeaponShellCase,
		MultiTraceWeaponParameters.BurstCooldown,
		MultiTraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		MultiTraceWeaponParameters.MinBurstAmount,
		MultiTraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		MultiTraceWeaponParameters.TraceProjectileType);
	AddInitializedWeapon(MultiTrace);
}

void AStandaloneTurret::SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupLaserWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	FInitWeaponStateLaser Parameters = LaserWeaponParameters;
	PrepareStandaloneWeaponParameters(Parameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateLaser* LaserWeapon = CreateStandaloneWeaponState<UWeaponStateLaser>(this, TEXT("laser"));
	if (not IsValid(LaserWeapon))
	{
		return;
	}

	LaserWeapon->InitLaserWeapon(
		Parameters.OwningPlayer,
		M_Weapons.Num(),
		Parameters.WeaponName,
		Parameters.WeaponOwner,
		Parameters.MeshComponent,
		Parameters.FireSocketName,
		World,
		Parameters.LaserWeaponSettings);
	AddInitializedWeapon(LaserWeapon);
}

void AStandaloneTurret::SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupMultiHitLaserWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	FInitWeaponStateMultiHitLaser Parameters = LaserWeaponParameters;
	PrepareStandaloneWeaponParameters(Parameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateMultiHitLaser* LaserWeapon =
		CreateStandaloneWeaponState<UWeaponStateMultiHitLaser>(this, TEXT("multi-hit laser"));
	if (not IsValid(LaserWeapon))
	{
		return;
	}

	LaserWeapon->InitMultiHitLaserWeapon(
		Parameters.OwningPlayer,
		M_Weapons.Num(),
		Parameters.WeaponName,
		Parameters.WeaponOwner,
		Parameters.MeshComponent,
		Parameters.FireSocketName,
		World,
		Parameters.LaserWeaponSettings);
	AddInitializedWeapon(LaserWeapon);
}

void AStandaloneTurret::SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupFlameThrowerWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	FInitWeaponStateFlameThrower Parameters = FlameWeaponParameters;
	PrepareStandaloneWeaponParameters(Parameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateFlameThrower* FlameWeapon =
		CreateStandaloneWeaponState<UWeaponStateFlameThrower>(this, TEXT("flame thrower"));
	if (not IsValid(FlameWeapon))
	{
		return;
	}

	FlameWeapon->InitFlameThrowerWeapon(
		Parameters.OwningPlayer,
		M_Weapons.Num(),
		Parameters.WeaponName,
		Parameters.WeaponOwner,
		Parameters.MeshComponent,
		Parameters.FireSocketName,
		World,
		Parameters.FlameSettings);
	AddInitializedWeapon(FlameWeapon);
}

void AStandaloneTurret::SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupProjectileWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		ProjectileWeaponParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateProjectile* Projectile =
		CreateStandaloneWeaponState<UWeaponStateProjectile>(this, TEXT("projectile"));
	if (not IsValid(Projectile))
	{
		return;
	}

	Projectile->InitProjectileWeapon(
		ProjectileWeaponParameters.OwningPlayer,
		M_Weapons.Num(),
		ProjectileWeaponParameters.WeaponName,
		ProjectileWeaponParameters.WeaponBurstMode,
		ProjectileWeaponParameters.WeaponOwner,
		ProjectileWeaponParameters.MeshComponent,
		ProjectileWeaponParameters.FireSocketName,
		World,
		ProjectileWeaponParameters.ProjectileSystem,
		ProjectileWeaponParameters.WeaponVFX,
		ProjectileWeaponParameters.WeaponShellCase,
		ProjectileWeaponParameters.BurstCooldown,
		ProjectileWeaponParameters.SingleBurstAmountMaxBurstAmount,
		ProjectileWeaponParameters.MinBurstAmount,
		ProjectileWeaponParameters.CreateShellCasingOnEveryRandomBurst);
	AddInitializedWeapon(Projectile);
}

void AStandaloneTurret::SetupRailwayCannonWeapon(FInitWeaponStateRailwayCannon RailwayCannonParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupRailwayCannonWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		RailwayCannonParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateRailwayCannon* RailwayCannon =
		CreateStandaloneWeaponState<UWeaponStateRailwayCannon>(this, TEXT("railway cannon"));
	if (not IsValid(RailwayCannon))
	{
		return;
	}

	RailwayCannon->InitRailwayCannonWeapon(M_Weapons.Num(), World, RailwayCannonParameters);
	AddInitializedWeapon(RailwayCannon);
}

void AStandaloneTurret::SetupRailgunWeapon(FInitWeaponStateRailgun RailgunWeaponParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupRailgunWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		RailgunWeaponParameters.ProjectileWeaponParameters,
		this,
		M_TurretMesh,
		GetOwningPlayerForWeaponInit());
	URailgunWeaponState* RailgunWeapon =
		CreateStandaloneWeaponState<URailgunWeaponState>(this, TEXT("railgun"));
	if (not IsValid(RailgunWeapon))
	{
		return;
	}

	RailgunWeapon->InitRailgunWeapon(M_Weapons.Num(), World, RailgunWeaponParameters);
	AddInitializedWeapon(RailgunWeapon);
}

void AStandaloneTurret::SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupRocketProjectileWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		RocketProjectileParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateRocketProjectile* RocketProjectile =
		CreateStandaloneWeaponState<UWeaponStateRocketProjectile>(this, TEXT("rocket projectile"));
	if (not IsValid(RocketProjectile))
	{
		return;
	}

	RocketProjectile->InitRocketProjectileWeapon(
		RocketProjectileParameters.OwningPlayer,
		M_Weapons.Num(),
		RocketProjectileParameters.WeaponName,
		RocketProjectileParameters.WeaponBurstMode,
		RocketProjectileParameters.WeaponOwner,
		RocketProjectileParameters.MeshComponent,
		RocketProjectileParameters.FireSocketName,
		World,
		RocketProjectileParameters.ProjectileSystem,
		RocketProjectileParameters.WeaponVFX,
		RocketProjectileParameters.WeaponShellCase,
		RocketProjectileParameters.RocketSettings,
		RocketProjectileParameters.BurstCooldown,
		RocketProjectileParameters.SingleBurstAmountMaxBurstAmount,
		RocketProjectileParameters.MinBurstAmount,
		RocketProjectileParameters.CreateShellCasingOnEveryRandomBurst);
	AddInitializedWeapon(RocketProjectile);
}

void AStandaloneTurret::SetupVerticalRocketProjectileWeapon(
	FInitWeaponStateVerticalRocketProjectile VerticalRocketProjectileParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupVerticalRocketProjectileWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		VerticalRocketProjectileParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UVerticalRocketWeaponState* VerticalRocket =
		CreateStandaloneWeaponState<UVerticalRocketWeaponState>(this, TEXT("vertical rocket"));
	if (not IsValid(VerticalRocket))
	{
		return;
	}

	VerticalRocket->InitVerticalRocketWeapon(
		VerticalRocketProjectileParameters.OwningPlayer,
		M_Weapons.Num(),
		VerticalRocketProjectileParameters.WeaponName,
		VerticalRocketProjectileParameters.WeaponBurstMode,
		VerticalRocketProjectileParameters.WeaponOwner,
		VerticalRocketProjectileParameters.MeshComponent,
		VerticalRocketProjectileParameters.FireSocketName,
		World,
		VerticalRocketProjectileParameters.ProjectileSystem,
		VerticalRocketProjectileParameters.WeaponVFX,
		VerticalRocketProjectileParameters.WeaponShellCase,
		VerticalRocketProjectileParameters.VerticalRocketSettings,
		VerticalRocketProjectileParameters.BurstCooldown,
		VerticalRocketProjectileParameters.SingleBurstAmountMaxBurstAmount,
		VerticalRocketProjectileParameters.MinBurstAmount,
		VerticalRocketProjectileParameters.CreateShellCasingOnEveryRandomBurst);
	AddInitializedWeapon(VerticalRocket);
}

void AStandaloneTurret::SetupHomingMissileWeapon(FInitWeaponStateHomingMissile HomingMissileParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupHomingMissileWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		HomingMissileParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateHomingMissile* HomingMissile =
		CreateStandaloneWeaponState<UWeaponStateHomingMissile>(this, TEXT("homing missile"));
	if (not IsValid(HomingMissile))
	{
		return;
	}

	HomingMissile->InitHomingMissileWeapon(
		HomingMissileParameters.OwningPlayer,
		M_Weapons.Num(),
		HomingMissileParameters.WeaponName,
		HomingMissileParameters.WeaponBurstMode,
		HomingMissileParameters.WeaponOwner,
		HomingMissileParameters.MeshComponent,
		HomingMissileParameters.FireSocketName,
		World,
		HomingMissileParameters.ProjectileSystem,
		HomingMissileParameters.WeaponVFX,
		HomingMissileParameters.WeaponShellCase,
		HomingMissileParameters.HomingMissileSettings,
		HomingMissileParameters.BurstCooldown,
		HomingMissileParameters.SingleBurstAmountMaxBurstAmount,
		HomingMissileParameters.MinBurstAmount,
		HomingMissileParameters.CreateShellCasingOnEveryRandomBurst);
	AddInitializedWeapon(HomingMissile);
}

void AStandaloneTurret::SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupMultiProjectileWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		MultiProjectileParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateMultiProjectile* MultiProjectile =
		CreateStandaloneWeaponState<UWeaponStateMultiProjectile>(this, TEXT("multi projectile"));
	if (not IsValid(MultiProjectile))
	{
		return;
	}

	MultiProjectile->InitMultiProjectileWeapon(
		MultiProjectileParameters.OwningPlayer,
		M_Weapons.Num(),
		MultiProjectileParameters.WeaponName,
		MultiProjectileParameters.WeaponBurstMode,
		MultiProjectileParameters.WeaponOwner,
		MultiProjectileParameters.MeshComponent,
		World,
		MultiProjectileParameters.FireSocketNames,
		MultiProjectileParameters.ProjectileSystem,
		MultiProjectileParameters.WeaponVFX,
		MultiProjectileParameters.WeaponShellCase,
		MultiProjectileParameters.BurstCooldown,
		MultiProjectileParameters.SingleBurstAmountMaxBurstAmount,
		MultiProjectileParameters.MinBurstAmount,
		MultiProjectileParameters.CreateShellCasingOnEveryRandomBurst);
	AddInitializedWeapon(MultiProjectile);
}

void AStandaloneTurret::SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjectileParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupArchProjectileWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		ArchProjectileParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateArchProjectile* ArchProjectile =
		CreateStandaloneWeaponState<UWeaponStateArchProjectile>(this, TEXT("arch projectile"));
	if (not IsValid(ArchProjectile))
	{
		return;
	}

	ArchProjectile->InitArchProjectileWeapon(
		ArchProjectileParameters.OwningPlayer,
		M_Weapons.Num(),
		ArchProjectileParameters.WeaponName,
		ArchProjectileParameters.WeaponBurstMode,
		ArchProjectileParameters.WeaponOwner,
		ArchProjectileParameters.MeshComponent,
		ArchProjectileParameters.FireSocketName,
		World,
		ArchProjectileParameters.ProjectileSystem,
		ArchProjectileParameters.WeaponVFX,
		ArchProjectileParameters.WeaponShellCase,
		ArchProjectileParameters.BurstCooldown,
		ArchProjectileParameters.SingleBurstAmountMaxBurstAmount,
		ArchProjectileParameters.MinBurstAmount,
		ArchProjectileParameters.CreateShellCasingOnEveryRandomBurst,
		ArchProjectileParameters.ArchSettings);
	AddInitializedWeapon(ArchProjectile);
}

void AStandaloneTurret::SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjectileParameters)
{
	SetupArchProjectileWeapon(ArchProjectileParameters);
}

void AStandaloneTurret::SetupSplitterArchProjectileWeapon(
	FInitWeaponStateSplitterArchProjectile SplitterArchProjectileParameters)
{
	UWorld* World = GetWeaponSetupWorld(TEXT("SetupSplitterArchProjectileWeapon"));
	if (not IsValid(World))
	{
		return;
	}

	PrepareStandaloneWeaponParameters(
		SplitterArchProjectileParameters, this, M_TurretMesh, GetOwningPlayerForWeaponInit());
	UWeaponStateSplitterArchProjectile* SplitterProjectile =
		CreateStandaloneWeaponState<UWeaponStateSplitterArchProjectile>(this, TEXT("splitter arch projectile"));
	if (not IsValid(SplitterProjectile))
	{
		return;
	}

	SplitterProjectile->InitSplitterArchProjectileWeapon(
		SplitterArchProjectileParameters.OwningPlayer,
		M_Weapons.Num(),
		SplitterArchProjectileParameters.WeaponName,
		SplitterArchProjectileParameters.WeaponBurstMode,
		SplitterArchProjectileParameters.WeaponOwner,
		SplitterArchProjectileParameters.MeshComponent,
		SplitterArchProjectileParameters.FireSocketName,
		World,
		SplitterArchProjectileParameters.ProjectileSystem,
		SplitterArchProjectileParameters.WeaponVFX,
		SplitterArchProjectileParameters.WeaponShellCase,
		SplitterArchProjectileParameters.BurstCooldown,
		SplitterArchProjectileParameters.SingleBurstAmountMaxBurstAmount,
		SplitterArchProjectileParameters.MinBurstAmount,
		SplitterArchProjectileParameters.CreateShellCasingOnEveryRandomBurst,
		SplitterArchProjectileParameters.SplitterSettings);
	AddInitializedWeapon(SplitterProjectile);
}
