// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ShieldComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/GameUI/ShieldBar/W_ShieldBar.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/ConstructorHelpers.h"

UShieldComponent::UShieldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> EngineSphere(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (EngineSphere.Succeeded())
	{
		ShieldSettings.ShieldSphereMesh = EngineSphere.Object;
	}
}

void UShieldComponent::BeginPlay()
{
	Super::BeginPlay();

	if (not GetIsSupportedShieldOwner())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UShieldComponent requires an ATankMaster owner in the base implementation."));
		DestroyComponent();
		return;
	}

	BindHealthBarVisibility();
	BeginPlay_AddAbility();

	if (ShieldSettings.SpawnMode == EShieldSpawnMode::Immediately)
	{
		ActivateShield(FShieldActivationOverrides());
	}
}

void UShieldComponent::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	ClearShieldTimers();
	UnbindHealthBarVisibility();
	if (not bDestroyingHierarchy)
	{
		RemoveAbilityFromOwner();
	}
	ClearOwnerShieldCache();
	DestroyRuntimeComponents();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UShieldComponent::ActivateShield(const FShieldActivationOverrides& ActivationOverrides)
{
	if (not CanActivateShield())
	{
		return false;
	}

	if (not ResolveActivationSettings(ActivationOverrides) || not EnsureShieldMesh())
	{
		return false;
	}

	EnsureShieldWidget();
	ApplyResolvedSettingsToShieldMesh();
	ActivateResolvedShield();
	return true;
}

bool UShieldComponent::CanActivateShield() const
{
	return M_ShieldState == EShieldState::Inactive;
}

EShieldDamageResult UShieldComponent::ApplyShieldDamage(const FShieldDamageRequest& DamageRequest)
{
	if (M_ShieldState != EShieldState::Active || DamageRequest.DamageSource == EShieldDamageSource::Mine)
	{
		return EShieldDamageResult::NotHandled;
	}

	const float PreviousShields = M_CurrentShields;
	const float AdjustedShieldDamage = CalculateAdjustedShieldDamage(DamageRequest);
	const bool bPassThroughShell = GetIsPassThroughShell(DamageRequest.ShellType);
	const EShieldDamageResult DamageResult = bPassThroughShell || AdjustedShieldDamage > PreviousShields
		                                             ? EShieldDamageResult::PassThrough
		                                             : EShieldDamageResult::Absorbed;

	M_CurrentShields = FMath::Max(0.0f, PreviousShields - AdjustedShieldDamage);
	UpdateShieldBarValues();
	OnShieldDamaged(AdjustedShieldDamage, DamageRequest);

	if (M_CurrentShields <= KINDA_SMALL_NUMBER)
	{
		M_CurrentShields = 0.0f;
		HandleShieldDepleted();
	}

	return DamageResult;
}

float UShieldComponent::GetShieldPercentage() const
{
	if (M_MaxShields <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return M_CurrentShields / M_MaxShields;
}

void UShieldComponent::BeginPlay_AddAbility()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const TWeakObjectPtr<UShieldComponent> WeakThis(this);
	FTimerDelegate AddAbilityDelegate;
	AddAbilityDelegate.BindLambda([WeakThis]()
	{
		UShieldComponent* ShieldComponent = WeakThis.Get();
		if (not IsValid(ShieldComponent))
		{
			return;
		}

		ShieldComponent->AddAbilityToOwner();
	});
	World->GetTimerManager().SetTimerForNextTick(AddAbilityDelegate);
}

bool UShieldComponent::GetIsSupportedShieldOwner() const
{
	return IsValid(Cast<ATankMaster>(GetOwner()));
}

void UShieldComponent::ApplyShieldRadius(UStaticMeshComponent* ShieldMesh, const float Radius) const
{
	if (not IsValid(ShieldMesh) || Radius <= 0.0f)
	{
		return;
	}

	const UStaticMesh* StaticMesh = ShieldMesh->GetStaticMesh();
	if (not IsValid(StaticMesh))
	{
		return;
	}

	const float MeshRadius = StaticMesh->GetBounds().SphereRadius;
	if (MeshRadius <= KINDA_SMALL_NUMBER)
	{
		RTSFunctionLibrary::ReportError(TEXT("Shield sphere mesh has invalid bounds."));
		return;
	}

	ShieldMesh->SetRelativeScale3D(FVector(Radius / MeshRadius));
}

void UShieldComponent::OnShieldActive()
{
}

void UShieldComponent::OnShieldDamaged(const float AppliedDamage, const FShieldDamageRequest& DamageRequest)
{
	(void)AppliedDamage;
	(void)DamageRequest;
}

void UShieldComponent::OnShieldDestroyed()
{
}

void UShieldComponent::OnShieldDeactivated(const EShieldDeactivationReason Reason)
{
	(void)Reason;
}

bool UShieldComponent::ResolveActivationSettings(const FShieldActivationOverrides& ActivationOverrides)
{
	M_ResolvedSettings.ShieldMaterial = ActivationOverrides.bOverrideShieldMaterial
		                                          ? ActivationOverrides.ShieldMaterial
		                                          : ShieldSettings.ShieldMaterial;
	M_ResolvedSettings.Radius = ActivationOverrides.bOverrideRadius
		                                  ? ActivationOverrides.Radius
		                                  : ShieldSettings.Radius;
	M_ResolvedSettings.TimedLife = ActivationOverrides.bOverrideTimedLife
		                                     ? ActivationOverrides.TimedLife
		                                     : ShieldSettings.TimedLife;
	M_ResolvedSettings.ShieldOffset = ActivationOverrides.bOverrideShieldOffset
		                                        ? ActivationOverrides.ShieldOffset
		                                        : ShieldSettings.ShieldOffset;
	M_ResolvedSettings.ShieldBehaviour = ActivationOverrides.bOverrideShieldBehaviour
		                                           ? ActivationOverrides.ShieldBehaviour
		                                           : ShieldSettings.ShieldBehaviour;

	M_MaxShields = ActivationOverrides.bOverrideMaxShields
		               ? ActivationOverrides.MaxShields
		               : ShieldSettings.MaxShields;
	M_CurrentShields = ActivationOverrides.bOverrideCurrentShields
		                   ? FMath::Clamp(ActivationOverrides.CurrentShields, 0.0f, M_MaxShields)
		                   : M_MaxShields;

	if (M_MaxShields > 0.0f && M_CurrentShields > 0.0f && M_ResolvedSettings.Radius > 0.0f &&
		IsValid(ShieldSettings.ShieldSphereMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		TEXT("Shield activation requires positive current/max shields, positive radius, and a sphere mesh."));
	return false;
}

bool UShieldComponent::EnsureShieldMesh()
{
	UStaticMeshComponent* ExistingShieldMesh = M_ShieldMeshComponent.Get();
	if (IsValid(ExistingShieldMesh))
	{
		return true;
	}

	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor) || not IsValid(OwnerActor->GetRootComponent()))
	{
		RTSFunctionLibrary::ReportError(TEXT("Shield component owner has no valid root component."));
		return false;
	}

	UStaticMeshComponent* NewShieldMesh = NewObject<UStaticMeshComponent>(OwnerActor);
	if (not IsValid(NewShieldMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create runtime shield mesh component."));
		return false;
	}

	OwnerActor->AddInstanceComponent(NewShieldMesh);
	NewShieldMesh->SetupAttachment(OwnerActor->GetRootComponent());
	NewShieldMesh->SetStaticMesh(ShieldSettings.ShieldSphereMesh);
	NewShieldMesh->SetCastShadow(false);
	NewShieldMesh->SetReceivesDecals(false);
	NewShieldMesh->SetCanEverAffectNavigation(false);
	NewShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NewShieldMesh->SetVisibility(false, true);
	NewShieldMesh->SetHiddenInGame(true, true);
	NewShieldMesh->RegisterComponent();
	M_ShieldMeshComponent = NewShieldMesh;
	return true;
}

void UShieldComponent::EnsureShieldWidget()
{
	if (ShieldSettings.ShieldBarWidgetClass == nullptr)
	{
		return;
	}

	UWidgetComponent* ExistingWidgetComponent = M_ShieldWidgetComponent.Get();
	if (IsValid(ExistingWidgetComponent))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor) || not IsValid(OwnerActor->GetRootComponent()))
	{
		return;
	}

	UWidgetComponent* NewWidgetComponent = NewObject<UWidgetComponent>(OwnerActor);
	if (not IsValid(NewWidgetComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create runtime shield widget component."));
		return;
	}

	OwnerActor->AddInstanceComponent(NewWidgetComponent);
	NewWidgetComponent->SetupAttachment(OwnerActor->GetRootComponent());
	NewWidgetComponent->SetRelativeLocation(ShieldSettings.ShieldBarOffset);
	NewWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	NewWidgetComponent->SetDrawAtDesiredSize(true);
	NewWidgetComponent->SetWidgetClass(ShieldSettings.ShieldBarWidgetClass);
	NewWidgetComponent->RegisterComponent();
	NewWidgetComponent->InitWidget();
	M_ShieldWidgetComponent = NewWidgetComponent;
	M_ShieldWidget = Cast<UW_ShieldBar>(NewWidgetComponent->GetWidget());
	UpdateShieldBarValues();
	ApplyShieldBarVisibility();
}

void UShieldComponent::ApplyResolvedSettingsToShieldMesh() const
{
	UStaticMeshComponent* ShieldMesh = M_ShieldMeshComponent.Get();
	if (not IsValid(ShieldMesh))
	{
		return;
	}

	ShieldMesh->SetStaticMesh(ShieldSettings.ShieldSphereMesh);
	ShieldMesh->SetRelativeLocation(M_ResolvedSettings.ShieldOffset);
	ApplyShieldRadius(ShieldMesh, M_ResolvedSettings.Radius);
	ShieldMesh->SetMaterial(0, M_ResolvedSettings.ShieldMaterial);
}

void UShieldComponent::ActivateResolvedShield()
{
	UStaticMeshComponent* ShieldMesh = M_ShieldMeshComponent.Get();
	if (not IsValid(ShieldMesh))
	{
		return;
	}

	ClearShieldTimers();
	M_ShieldState = EShieldState::Active;
	FRTS_CollisionSetup::SetupShieldCollision(ShieldMesh, GetOwningPlayer());
	ShieldMesh->SetHiddenInGame(false, true);
	ShieldMesh->SetVisibility(true, true);
	UpdateShieldBarValues();
	ApplyShieldBarVisibility();
	StartTimedLifeIfNeeded();
	OnShieldActive();
}

void UShieldComponent::DeactivateRuntimeShield()
{
	UStaticMeshComponent* ShieldMesh = M_ShieldMeshComponent.Get();
	if (IsValid(ShieldMesh))
	{
		ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ShieldMesh->SetHiddenInGame(true, true);
		ShieldMesh->SetVisibility(false, true);
	}

	ApplyShieldBarVisibility();
}

void UShieldComponent::HandleShieldDepleted()
{
	ClearShieldTimers();
	DeactivateRuntimeShield();
	OnShieldDestroyed();

	switch (M_ResolvedSettings.ShieldBehaviour)
	{
	case EShieldBehaviour::KillOnDestroyed:
		M_ShieldState = EShieldState::Destroying;
		RemoveAbilityFromOwner();
		ClearOwnerShieldCache();
		DestroyRuntimeComponents();
		DestroyComponent();
		break;
	case EShieldBehaviour::Recharge:
		M_ShieldState = EShieldState::Recharging;
		StartRecharge();
		break;
	case EShieldBehaviour::OnlyRechargeOnActivation:
		M_ShieldState = EShieldState::Inactive;
		break;
	default:
		M_ShieldState = EShieldState::Inactive;
		break;
	}

	ApplyShieldBarVisibility();
}

void UShieldComponent::HandleTimedLifeExpired()
{
	if (M_ShieldState != EShieldState::Active)
	{
		return;
	}

	M_CurrentShields = M_MaxShields;
	M_ShieldState = EShieldState::Inactive;
	DeactivateRuntimeShield();
	UpdateShieldBarValues();
	OnShieldDeactivated(EShieldDeactivationReason::TimedOut);
}

void UShieldComponent::StartTimedLifeIfNeeded()
{
	if (M_ResolvedSettings.TimedLife <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_TimedLifeTimerHandle,
		this,
		&UShieldComponent::HandleTimedLifeExpired,
		M_ResolvedSettings.TimedLife,
		false);
}

void UShieldComponent::StartRecharge()
{
	if (ShieldSettings.RechargeDelay <= 0.0f)
	{
		HandleRechargeComplete();
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_RechargeTimerHandle,
		this,
		&UShieldComponent::HandleRechargeComplete,
		ShieldSettings.RechargeDelay,
		false);
}

void UShieldComponent::HandleRechargeComplete()
{
	if (M_ShieldState != EShieldState::Recharging)
	{
		return;
	}

	M_CurrentShields = M_MaxShields;
	ActivateResolvedShield();
}

void UShieldComponent::ClearShieldTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_TimedLifeTimerHandle);
	World->GetTimerManager().ClearTimer(M_RechargeTimerHandle);
}

void UShieldComponent::DestroyRuntimeComponents()
{
	UStaticMeshComponent* ShieldMesh = M_ShieldMeshComponent.Get();
	M_ShieldMeshComponent = nullptr;
	if (IsValid(ShieldMesh))
	{
		ShieldMesh->DestroyComponent();
	}

	UWidgetComponent* WidgetComponent = M_ShieldWidgetComponent.Get();
	M_ShieldWidgetComponent = nullptr;
	M_ShieldWidget.Reset();
	if (IsValid(WidgetComponent))
	{
		WidgetComponent->DestroyComponent();
	}
}

void UShieldComponent::AddAbilityToOwner()
{
	AActor* OwnerActor = GetOwner();
	ICommands* Commands = Cast<ICommands>(OwnerActor);
	if (Commands == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Shield owner does not implement ICommands."));
		return;
	}

	FUnitAbilityEntry ShieldAbilityEntry;
	ShieldAbilityEntry.AbilityId = EAbilityID::IdActivateShield;
	ShieldAbilityEntry.CooldownDuration = ShieldSettings.CooldownDuration;
	Commands->AddAbility(ShieldAbilityEntry, ShieldSettings.PreferredAbilityIndex);
}

void UShieldComponent::RemoveAbilityFromOwner() const
{
	ICommands* Commands = Cast<ICommands>(GetOwner());
	if (Commands == nullptr || not Commands->HasAbility(EAbilityID::IdActivateShield))
	{
		return;
	}

	Commands->RemoveAbility(EAbilityID::IdActivateShield);
}

void UShieldComponent::ClearOwnerShieldCache() const
{
	ATankMaster* TankOwner = Cast<ATankMaster>(GetOwner());
	if (not IsValid(TankOwner))
	{
		return;
	}

	TankOwner->ClearShieldComponentCache(this);
}

void UShieldComponent::BindHealthBarVisibility()
{
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	UHealthComponent* HealthComponent = OwnerActor->FindComponentByClass<UHealthComponent>();
	if (not IsValid(HealthComponent))
	{
		return;
	}

	M_HealthComponent = HealthComponent;
	M_HealthBarVisibility = HealthComponent->GetHealthBarVisibility();
	M_HealthVisibilityDelegateHandle = HealthComponent->GetOnHealthBarVisibilityChanged().AddUObject(
		this,
		&UShieldComponent::HandleHealthBarVisibilityChanged);
}

void UShieldComponent::UnbindHealthBarVisibility()
{
	UHealthComponent* HealthComponent = M_HealthComponent.Get();
	if (IsValid(HealthComponent) && M_HealthVisibilityDelegateHandle.IsValid())
	{
		HealthComponent->GetOnHealthBarVisibilityChanged().Remove(M_HealthVisibilityDelegateHandle);
	}

	M_HealthVisibilityDelegateHandle.Reset();
	M_HealthComponent.Reset();
}

void UShieldComponent::HandleHealthBarVisibilityChanged(const ESlateVisibility HealthBarVisibility)
{
	M_HealthBarVisibility = HealthBarVisibility;
	ApplyShieldBarVisibility();
}

void UShieldComponent::ApplyShieldBarVisibility()
{
	UW_ShieldBar* ShieldWidget = M_ShieldWidget.Get();
	if (not IsValid(ShieldWidget))
	{
		return;
	}

	ShieldWidget->SetVisibility(
		M_ShieldState == EShieldState::Active ? M_HealthBarVisibility : ESlateVisibility::Hidden);
}

void UShieldComponent::UpdateShieldBarValues() const
{
	UW_ShieldBar* ShieldWidget = M_ShieldWidget.Get();
	if (not IsValid(ShieldWidget))
	{
		return;
	}

	ShieldWidget->UpdateShieldValues(M_CurrentShields, M_MaxShields);
}

int32 UShieldComponent::GetOwningPlayer() const
{
	ATankMaster* TankOwner = Cast<ATankMaster>(GetOwner());
	if (not IsValid(TankOwner))
	{
		return INDEX_NONE;
	}

	return TankOwner->GetOwningPlayer();
}

float UShieldComponent::CalculateAdjustedShieldDamage(const FShieldDamageRequest& DamageRequest) const
{
	const float DamageBeforeReduction = GetIsPassThroughShell(DamageRequest.ShellType)
		                                          ? DamageRequest.BaseDamage + DamageRequest.
		                                          RangeAdjustedArmorPenetration
		                                          : DamageRequest.BaseDamage;
	return FMath::Max(
		0.0f,
		DamageBeforeReduction * ShieldSettings.DamageReductionSettings.DamageReductionMlt -
		ShieldSettings.DamageReductionSettings.DamageReduction);
}

bool UShieldComponent::GetIsPassThroughShell(const EWeaponShellType ShellType)
{
	return ShellType == EWeaponShellType::Shell_APCR || ShellType == EWeaponShellType::Shell_Railgun;
}
