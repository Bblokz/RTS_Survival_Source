// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PitDefenseComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/GunPit/GunPitCrewComponent.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Collapse/VerticalCollapse/FRTS_VerticalCollapse.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTS_Statics/SubSystems/ExplosionManagerSubsystem/ExplosionManagerSubsystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"

namespace PitDefense
{
	constexpr int32 PlayerOwnerIndex = 1;
	constexpr float SandbagEffectHeight = 100.f;
	constexpr float BloodEffectHeight = 90.f;
	constexpr float ExplosionPreferredDistance = 160.f;
	constexpr float MinimumExplosionRange = 250.f;
	constexpr float SandbagEffectScale = 1.35f;
	const FName SmokeColorParameterName(TEXT("SmokeColor"));
	const FLinearColor SandbagSmokeColor(0.964706f, 0.376471f, 0.031373f, 1.f);
}

UPitDefenseComponent::UPitDefenseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	M_DestructionSettings.SandbagExplosionSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/DigIn/NS_SandBagExplosion.NS_SandBagExplosion")));
	M_DestructionSettings.SandbagExplosionSound = TSoftObjectPtr<USoundCue>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Sounds/Explosions/SandBagExpl/Cue_Sandwall_Explosion.Cue_Sandwall_Explosion")));
	M_DestructionSettings.SandbagExplosionAttenuation = TSoftObjectPtr<USoundAttenuation>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse.Att_Collapse")));
	M_DestructionSettings.SandbagExplosionConcurrency = TSoftObjectPtr<USoundConcurrency>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse.SC_Collapse")));
	M_DestructionSettings.BloodEffectSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/SpecialEffects/Realistic_Starter_VFX_Pack/Particles/Hit/P_Blood_Converted1.P_Blood_Converted1")));

	const TCHAR* const DeathMontagePaths[] =
	{
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimLibrary/DeathAnimations/Death_Standing_Montage_1.Death_Standing_Montage_1"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimLibrary/DeathAnimations/Death_Standing_Montage_2.Death_Standing_Montage_2"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimLibrary/DeathAnimations/Death_Standing_Montage_3.Death_Standing_Montage_3"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/AnimLibrary/DeathAnimations/Montage_InfDeath_4.Montage_InfDeath_4")
	};
	for (const TCHAR* const DeathMontagePath : DeathMontagePaths)
	{
		M_DestructionSettings.CrewDeathMontages.Add(
			TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(DeathMontagePath)));
	}

	M_DestructionSettings.VerticalCollapseSettings.DeltaZ = 350.f;
	M_DestructionSettings.VerticalCollapseSettings.CollapseTime = 3.f;
	M_DestructionSettings.VerticalCollapseSettings.MinRandomYaw = 4.f;
	M_DestructionSettings.VerticalCollapseSettings.MaxRandomYaw = 12.f;
	M_DestructionSettings.VerticalCollapseSettings.MinPitchRoll = 2.f;
	M_DestructionSettings.VerticalCollapseSettings.MaxPitchRoll = 6.f;
	M_DestructionSettings.VerticalCollapseSettings.bDestroyPostVerticalCollapse = true;
}

void UPitDefenseComponent::BeginPlay()
{
	Super::BeginPlay();

	CreateWeaponHitCollider();
	ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner))
	{
		return;
	}

	PitOwner->OnBxpConstructed.AddUObject(this, &UPitDefenseComponent::RefreshWeaponHitCollider);
	RefreshWeaponHitCollider();
	TWeakObjectPtr<UPitDefenseComponent> WeakThis(this);
	PitOwner->GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->RefreshWeaponHitCollider();
	}));
}

void UPitDefenseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ABuildingExpansion* PitOwner = GetPitOwner())
	{
		PitOwner->OnBxpConstructed.RemoveAll(this);
	}
	M_WeaponHitCollider = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UPitDefenseComponent::HandlePitDeath()
{
	if (bM_DeathStarted)
	{
		return;
	}
	bM_DeathStarted = true;

	DisableWeaponHitCollider();
	PlayCrewDeathEffects();
	SpawnSandbagExplosion();
	SpawnScatteredExplosions();
	SpawnScorchDecals();
	StartVerticalDestruction();
}

void UPitDefenseComponent::RefreshWeaponHitCollider()
{
	ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner) || not GetIsValidWeaponHitCollider())
	{
		return;
	}

	DisableCrewAndWeaponMeshCollision();
	TArray<UMeshComponent*> CrewAndWeaponMeshes;
	GatherCrewAndWeaponMeshComponents(CrewAndWeaponMeshes);
	FBox WorldBounds(EForceInit::ForceInit);
	for (const UMeshComponent* CrewOrWeaponMesh : CrewAndWeaponMeshes)
	{
		if (not IsValid(CrewOrWeaponMesh) || not CrewOrWeaponMesh->IsRegistered())
		{
			continue;
		}
		WorldBounds += CrewOrWeaponMesh->Bounds.GetBox();
	}

	const FVector UnpaddedExtent = WorldBounds.IsValid ? WorldBounds.GetExtent() : FVector::ZeroVector;
	const FVector PaddedExtent = UnpaddedExtent + M_CollisionSettings.BoundsPadding;
	const FVector2D HorizontalExtent(
		FMath::Clamp(PaddedExtent.X, M_CollisionSettings.MinimumExtent.X,
			M_CollisionSettings.MaximumHorizontalExtent.X),
		FMath::Clamp(PaddedExtent.Y, M_CollisionSettings.MinimumExtent.Y,
			M_CollisionSettings.MaximumHorizontalExtent.Y));
	const UStaticMeshComponent* OuterMesh = Cast<UStaticMeshComponent>(PitOwner->GetRootComponent());
	const float OuterCollisionTop = IsValid(OuterMesh) && OuterMesh->IsRegistered()
		? OuterMesh->Bounds.GetBox().Max.Z + M_CollisionSettings.OuterCollisionClearance
		: PitOwner->GetActorLocation().Z + M_CollisionSettings.OuterCollisionClearance;
	const float CrewAndWeaponTop = WorldBounds.IsValid
		? WorldBounds.Max.Z + M_CollisionSettings.BoundsPadding.Z
		: OuterCollisionTop;
	const float ColliderTop = FMath::Max(
		CrewAndWeaponTop,
		OuterCollisionTop + M_CollisionSettings.MinimumExtent.Z * 2.f);
	const float VerticalExtent = (ColliderTop - OuterCollisionTop) * 0.5f;
	const FVector WorldCenter(
		PitOwner->GetActorLocation().X,
		PitOwner->GetActorLocation().Y,
		OuterCollisionTop + VerticalExtent);

	M_WeaponHitCollider->SetWorldLocation(WorldCenter);
	M_WeaponHitCollider->SetWorldRotation(PitOwner->GetActorRotation());
	M_WeaponHitCollider->SetBoxExtent(FVector(HorizontalExtent, VerticalExtent), false);
	ConfigureWeaponHitCollision(*PitOwner);
}

ABuildingExpansion* UPitDefenseComponent::GetPitOwner() const
{
	ABuildingExpansion* PitOwner = Cast<ABuildingExpansion>(GetOwner());
	if (IsValid(PitOwner))
	{
		return PitOwner;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("PitOwner"),
		TEXT("UPitDefenseComponent::GetPitOwner"),
		GetOwner());
	return nullptr;
}

bool UPitDefenseComponent::GetIsValidWeaponHitCollider() const
{
	if (IsValid(M_WeaponHitCollider))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WeaponHitCollider"),
		TEXT("UPitDefenseComponent::GetIsValidWeaponHitCollider"),
		GetOwner());
	return false;
}

void UPitDefenseComponent::CreateWeaponHitCollider()
{
	ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner) || not IsValid(PitOwner->GetRootComponent()))
	{
		return;
	}

	M_WeaponHitCollider = NewObject<UBoxComponent>(PitOwner, TEXT("PitWeaponHitCollider"));
	if (not GetIsValidWeaponHitCollider())
	{
		return;
	}

	PitOwner->AddInstanceComponent(M_WeaponHitCollider);
	M_WeaponHitCollider->SetupAttachment(PitOwner->GetRootComponent());
	M_WeaponHitCollider->SetHiddenInGame(true);
	M_WeaponHitCollider->SetCanEverAffectNavigation(false);
	M_WeaponHitCollider->RegisterComponent();
}

void UPitDefenseComponent::ConfigureWeaponHitCollision(const ABuildingExpansion& PitOwner) const
{
	if (not GetIsValidWeaponHitCollider())
	{
		return;
	}

	const URTSComponent* RTSComponent = PitOwner.FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	M_WeaponHitCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	M_WeaponHitCollider->SetCollisionObjectType(ECC_WorldDynamic);
	M_WeaponHitCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_WeaponHitCollider->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
	M_WeaponHitCollider->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);
	const ECollisionChannel WeaponTraceChannel = RTSComponent->GetOwningPlayer() == PitDefense::PlayerOwnerIndex
		? COLLISION_TRACE_PLAYER
		: COLLISION_TRACE_ENEMY;
	M_WeaponHitCollider->SetCollisionResponseToChannel(WeaponTraceChannel, ECR_Block);
	M_WeaponHitCollider->SetGenerateOverlapEvents(true);
}

void UPitDefenseComponent::DisableCrewAndWeaponMeshCollision() const
{
	TArray<UMeshComponent*> CrewAndWeaponMeshes;
	GatherCrewAndWeaponMeshComponents(CrewAndWeaponMeshes);
	for (UMeshComponent* CrewOrWeaponMesh : CrewAndWeaponMeshes)
	{
		if (not IsValid(CrewOrWeaponMesh))
		{
			continue;
		}
		CrewOrWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CrewOrWeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		CrewOrWeaponMesh->SetGenerateOverlapEvents(false);
		CrewOrWeaponMesh->SetCanEverAffectNavigation(false);
	}
}

void UPitDefenseComponent::DisableWeaponHitCollider() const
{
	if (not GetIsValidWeaponHitCollider())
	{
		return;
	}
	M_WeaponHitCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_WeaponHitCollider->SetGenerateOverlapEvents(false);
}

void UPitDefenseComponent::GatherCrewAndWeaponMeshComponents(TArray<UMeshComponent*>& OutMeshComponents) const
{
	OutMeshComponents.Reset();
	const ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner))
	{
		return;
	}

	TArray<UChildActorComponent*> ChildActorComponents;
	PitOwner->GetComponents<UChildActorComponent>(ChildActorComponents);
	for (const UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		AActor* ChildActor = IsValid(ChildActorComponent) ? ChildActorComponent->GetChildActor() : nullptr;
		if (not IsValid(ChildActor))
		{
			continue;
		}

		TArray<UMeshComponent*> ActorMeshComponents;
		ChildActor->GetComponents<UMeshComponent>(ActorMeshComponents);
		OutMeshComponents.Append(ActorMeshComponents);
	}
}

void UPitDefenseComponent::GatherPitCrewComponents(TArray<UGunPitCrewComponent*>& OutCrewComponents) const
{
	OutCrewComponents.Reset();
	TArray<AActor*> PitActors;
	GatherPitActors(PitActors);
	for (AActor* PitActor : PitActors)
	{
		if (not IsValid(PitActor))
		{
			continue;
		}
		if (UGunPitCrewComponent* CrewComponent = PitActor->FindComponentByClass<UGunPitCrewComponent>())
		{
			OutCrewComponents.Add(CrewComponent);
		}
	}
}

void UPitDefenseComponent::GatherPitActors(TArray<AActor*>& OutPitActors) const
{
	OutPitActors.Reset();
	ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner))
	{
		return;
	}

	OutPitActors.Add(PitOwner);
	TArray<UChildActorComponent*> ChildActorComponents;
	PitOwner->GetComponents<UChildActorComponent>(ChildActorComponents);
	for (const UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		AActor* ChildActor = IsValid(ChildActorComponent) ? ChildActorComponent->GetChildActor() : nullptr;
		if (not IsValid(ChildActor))
		{
			continue;
		}
		OutPitActors.Add(ChildActor);
	}
}

void UPitDefenseComponent::PlayCrewDeathEffects() const
{
	TArray<UAnimMontage*> DeathMontages;
	for (const TSoftObjectPtr<UAnimMontage>& SoftDeathMontage : M_DestructionSettings.CrewDeathMontages)
	{
		if (UAnimMontage* DeathMontage = SoftDeathMontage.LoadSynchronous())
		{
			DeathMontages.Add(DeathMontage);
		}
	}

	UNiagaraSystem* BloodEffect = M_DestructionSettings.BloodEffectSystem.LoadSynchronous();
	TArray<UGunPitCrewComponent*> CrewComponents;
	GatherPitCrewComponents(CrewComponents);
	for (UGunPitCrewComponent* CrewComponent : CrewComponents)
	{
		if (not IsValid(CrewComponent))
		{
			continue;
		}
		CrewComponent->PlayCrewDeathMontages(DeathMontages);
		if (not IsValid(BloodEffect))
		{
			continue;
		}
		for (const USkeletalMeshComponent* CrewMesh : CrewComponent->GetCrewMeshes())
		{
			if (not IsValid(CrewMesh))
			{
				continue;
			}
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, BloodEffect, CrewMesh->GetComponentLocation() + FVector::UpVector * PitDefense::BloodEffectHeight);
		}
	}
}

void UPitDefenseComponent::SpawnSandbagExplosion() const
{
	const ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner))
	{
		return;
	}

	const FVector EffectLocation = PitOwner->GetActorLocation() + FVector::UpVector * PitDefense::SandbagEffectHeight;
	if (UNiagaraSystem* SandbagEffect = M_DestructionSettings.SandbagExplosionSystem.LoadSynchronous())
	{
		UNiagaraComponent* SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			SandbagEffect,
			EffectLocation,
			PitOwner->GetActorRotation(),
			FVector(PitDefense::SandbagEffectScale));
		if (IsValid(SpawnedEffect))
		{
			SpawnedEffect->SetColorParameter(PitDefense::SmokeColorParameterName, PitDefense::SandbagSmokeColor);
		}
	}

	UGameplayStatics::PlaySoundAtLocation(
		this,
		M_DestructionSettings.SandbagExplosionSound.LoadSynchronous(),
		EffectLocation,
		PitOwner->GetActorRotation(),
		1.f,
		1.f,
		0.f,
		M_DestructionSettings.SandbagExplosionAttenuation.LoadSynchronous(),
		M_DestructionSettings.SandbagExplosionConcurrency.LoadSynchronous());
}

void UPitDefenseComponent::SpawnScatteredExplosions() const
{
	const ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner) || M_DestructionSettings.SmallExplosionCount <= 0)
	{
		return;
	}
	const UWorld* World = GetWorld();
	UExplosionManagerSubsystem* ExplosionSubsystem = IsValid(World)
		? World->GetSubsystem<UExplosionManagerSubsystem>()
		: nullptr;
	UGameExplosionsManager* ExplosionManager = IsValid(ExplosionSubsystem)
		? ExplosionSubsystem->GetExplosionManager()
		: nullptr;
	if (not IsValid(ExplosionManager))
	{
		return;
	}

	const float ExplosionRange = GetIsValidWeaponHitCollider()
		? FMath::Max(M_WeaponHitCollider->GetScaledBoxExtent().GetMax() * 0.8f, PitDefense::MinimumExplosionRange)
		: PitDefense::MinimumExplosionRange;
	ExplosionManager->SpawnMultipleExplosionsInArea(
		ERTS_ExplosionType::Small,
		PitOwner->GetActorLocation() + FVector::UpVector * PitDefense::SandbagEffectHeight,
		M_DestructionSettings.SmallExplosionCount,
		ExplosionRange,
		PitDefense::ExplosionPreferredDistance);
}

void UPitDefenseComponent::SpawnScorchDecals() const
{
	const ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner) || M_DestructionSettings.ScorchDecalCount <= 0)
	{
		return;
	}

	const float DecalOffset = GetIsValidWeaponHitCollider()
		? FMath::Max(M_WeaponHitCollider->GetScaledBoxExtent().GetMax() * 0.6f, M_DestructionSettings.ScorchDecalSize.X)
		: M_DestructionSettings.ScorchDecalSize.X;
	for (int32 DecalIndex = 0; DecalIndex < M_DestructionSettings.ScorchDecalCount; ++DecalIndex)
	{
		URTSBlueprintFunctionLibrary::RTSSpawnDecal(
			this,
			ERTS_DecalType::Scorch,
			PitOwner->GetActorLocation(),
			M_DestructionSettings.ScorchDecalSize,
			M_DestructionSettings.ScorchDecalLifeTime,
			FVector2D(-DecalOffset, DecalOffset));
	}
}

void UPitDefenseComponent::StartVerticalDestruction() const
{
	ABuildingExpansion* PitOwner = GetPitOwner();
	if (not IsValid(PitOwner))
	{
		return;
	}
	FRTS_VerticalCollapse::StartVerticalCollapse(
		PitOwner,
		M_DestructionSettings.VerticalCollapseSettings,
		FCollapseFX());
}
