// Copyright (C) Bas Blokzijl - All rights reserved.


#include "DestructableEnvActor.h"

#include "NiagaraComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "CrushDestructionTypes/ECrushDestructionType.h"
#include "DestructableWireComponent/DestructableWireComp.h"
#include "ImpulseOnCrushed/FImpulseOnCrushed.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Collapse/CollapseBySwapParameters.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Collapse/FRTS_Collapse/FRTS_Collapse.h"
#include "RTS_Survival/Collapse/VerticalCollapse/FRTS_VerticalCollapse.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/RTSDebugBreak/RTSDebugBreak.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "VerticalCollapseOnCrushed/FVerticalCollapseOnCrushed.h"


ADestructableEnvActor::ADestructableEnvActor(const FObjectInitializer& ObjectInitializer)
	: AEnvironmentActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADestructableEnvActor::TriggerDestroyActor(ERTSDeathType DeathType)
{
	OnUnitDies(DeathType);
}

void ADestructableEnvActor::SetupCrushDestructionOverlap(UPrimitiveComponent* OverlapComponent,
                                                         const ECrushDestructionType DestructionType,
                                                         const ECrushDeathType CrushDeathType)
{
	if (not IsValid(OverlapComponent))
	{
		return;
	}
	
	RTS_ENSURE(OverlapComponent && OverlapComponent->GetGenerateOverlapEvents());
	RTS_ENSURE(OverlapComponent && OverlapComponent->GetCollisionResponseToChannel(COLLISION_OBJ_PLAYER) == ECR_Overlap)
	;
	M_CrushDeathType = CrushDeathType;



	// Make sure overlap events will fire.
	if (not OverlapComponent->GetGenerateOverlapEvents())
	{
		OverlapComponent->SetGenerateOverlapEvents(true);
	}

	// To prevent duplicate bindings when users reconfigure in BP,
	// remove any of our potential handlers before adding the chosen one.
	OverlapComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ADestructableEnvActor::HandleBeginOverlap_HeavyTank);
	OverlapComponent->OnComponentBeginOverlap.RemoveDynamic(
		this, &ADestructableEnvActor::HandleBeginOverlap_MediumOrHeavy);
	OverlapComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ADestructableEnvActor::HandleBeginOverlap_AnyTank);

	switch (DestructionType)
	{
	case ECrushDestructionType::HeavyTank:
		OverlapComponent->OnComponentBeginOverlap.
		                  AddDynamic(this, &ADestructableEnvActor::HandleBeginOverlap_HeavyTank);
		break;

	case ECrushDestructionType::MediumOrHeavyTank:
		OverlapComponent->OnComponentBeginOverlap.AddDynamic(
			this, &ADestructableEnvActor::HandleBeginOverlap_MediumOrHeavy);
		break;

	case ECrushDestructionType::AnyTank:
	default:
		OverlapComponent->OnComponentBeginOverlap.AddDynamic(this, &ADestructableEnvActor::HandleBeginOverlap_AnyTank);
		break;
	}
}

void ADestructableEnvActor::GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const
{
	// Move into outlocaloffsets.
	OutLocalOffsets.Reset();
	OutLocalOffsets = AimOffsetPoints;
}


// Called when the game starts or when spawned
void ADestructableEnvActor::BeginPlay()
{
	Super::BeginPlay();
}

void ADestructableEnvActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(M_PostCollapseTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}


void ADestructableEnvActor::ConfigureImpulseOnCrushed(UPrimitiveComponent* const TargetPrimitive,
                                                      const float CrushImpulseScale,
                                                      const float TimeTillImpulse)
{
	// Early returns with minimal nesting.
	if (CrushImpulseScale < 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("ConfigureImpulseOnCrushed: CrushImpulseScale < 0; No impulse."));
	}
	M_ImpulseOnCrushed.Init(TargetPrimitive, CrushImpulseScale, TimeTillImpulse);
}

void ADestructableEnvActor::ConfigureVerticalCollapse(UPrimitiveComponent* const TargetPrimitive,
                                                      const float CollapseSpeedScale,
                                                      USoundBase* const CollapseSound,
                                                      USoundAttenuation* const Attenuation,
                                                      USoundConcurrency* const Concurrency)
{
	if (CollapseSpeedScale <= 0.f)
	{
		RTSFunctionLibrary::ReportError(TEXT("ConfigureVerticalCollapse: CollapseSpeedScale <= 0; clamping to 0.1."));
	}
	M_VerticalCollapseOnCrushed.Init(TargetPrimitive, CollapseSpeedScale, CollapseSound, Attenuation, Concurrency);
}


void ADestructableEnvActor::VerticalCollapseInRandomDirection()
{
	OnDestructibleCollapse.Broadcast();
	M_VerticalCollapseOnCrushed.QueueCollapseRandom(GetWorld());
}


void ADestructableEnvActor::BeginDestroy()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(M_PostCollapseTimerHandle);
	}
	Super::BeginDestroy();
}

float ADestructableEnvActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                        AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageReduction > 0.1)
	{
		DamageAmount -= FMath::Max(0, DamageReduction);
	}
	Health -= DamageAmount;
	if constexpr (DeveloperSettings::Debugging::GDamage_System_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			"Damage tanken by " + GetName() + " is " + FString::SanitizeFloat(DamageAmount));
	}
	if (IsUnitAlive() && Health <= 0)
	{
		const ERTSDamageType RtsDamageType = FRTSWeaponHelpers::GetRTSDamageType(DamageEvent);
		const ERTSDeathType DeathType = FRTSWeaponHelpers::TranslateDamageIntoDeathType(RtsDamageType);
		OnUnitDies(DeathType);
		// Do not trigger death message on player units by returning 0.f; (as we do on regular rts units to trigger voicelines
		// and kills)
	}
	return DamageAmount;
}

void ADestructableEnvActor::CollapseMesh(UGeometryCollectionComponent* GeoCollapseComp,
                                         const TSoftObjectPtr<UGeometryCollection> GeoCollection,
                                         UMeshComponent* MeshToCollapse,
                                         const FCollapseDuration CollapseDuration, const FCollapseForce CollapseForce,
                                         const FCollapseFX CollapseFX)
{
	OnDestructibleCollapse.Broadcast();
	FRTS_Collapse::CollapseMesh(this, GeoCollapseComp, GeoCollection, MeshToCollapse, CollapseDuration, CollapseForce,
	                            CollapseFX);
	if (not CollapseDuration.bDestroyOwningActorAfterCollapse)
	{
		TWeakObjectPtr<ADestructableEnvActor> WeakThis(this);
		// Set the timer to wait CollapseDuration seconds then call OnMeshCollapsedNoDestroy.
		M_PostCollapseTimerDelegate.BindLambda([WeakThis, GeoCollapseComp, CollapseDuration]()-> void
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnMeshCollapsedNoDestroy(GeoCollapseComp, CollapseDuration.bKeepGeometryVisibleAfterLifeTime);
			}
		});
		GetWorldTimerManager().SetTimer(M_PostCollapseTimerHandle, M_PostCollapseTimerDelegate,
		                                CollapseDuration.CollapsedGeometryLifeTime, false);
	}
}

void ADestructableEnvActor::CollapseMeshWithSwapping(
	const FSwapToDestroyedMesh CollapseParameters,
	const bool bNoLongerBlockWeaponsPostCollapse,
	UNiagaraSystem* AttachSystem,
	USoundCue* AttachSound,
	const FVector AttachOffset)
{
	
	OnDestructibleCollapse.Broadcast();
	if (not IsValid(CollapseParameters.ComponentToSwapOn))
	{
		RTSFunctionLibrary::ReportError("Cannot collapse mesh with swapping: ComponentToSwapOn is not valid.");
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
		UGameplayStatics::PlaySoundAtLocation(this, AttachSound, AttachOffset, FRotator::ZeroRotator, 1, 1, 0,
		                                      CollapseParameters.Attenuation, CollapseParameters.SoundConcurrency,
		                                      nullptr, nullptr);
	}
}

void ADestructableEnvActor::SetupComponentCollisions(TArray<UMeshComponent*> MeshComponents,
                                                     TArray<UGeometryCollectionComponent*> GeometryComponents,
                                                     bool bOverlapTanks) const
{
	for (auto EachDestructibleMesh : MeshComponents)
	{
		FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision(EachDestructibleMesh, bOverlapTanks);
	}
	for (const auto EachResourceGeometryComponent : GeometryComponents)
	{
		FRTS_CollisionSetup::SetupDestructibleEnvActorGeometryComponentCollision(EachResourceGeometryComponent);
	}
}

void ADestructableEnvActor::DestroyAndSpawnActors(const FDestroySpawnActorsParameters& SpawnParams,
                                                  FCollapseFX CollapseFX)
{
	FRTS_Collapse::OnDestroySpawnActors(this,
	                                    SpawnParams,
	                                    CollapseFX);
}

void ADestructableEnvActor::VerticalDestruction(const FRTSVerticalCollapseSettings& CollapseSettings,
                                                const FCollapseFX& CollapseFX)
{
	
	OnDestructibleCollapse.Broadcast();
	TWeakObjectPtr<ADestructableEnvActor> WeakThis(this);
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
		CollapseSettings,
		CollapseFX,
		OnFinished);
}


void ADestructableEnvActor::OnUnitDies(const ERTSDeathType DeathType)
{
	if (IsUnitAlive())
	{
		SetUnitDying();
		OnUnitDies_CheckForWireComponent();
		BP_OnUnitDies(DeathType);
	}
}

bool ADestructableEnvActor::GetIsOverLapDestroyByHeavyTank(AActor* OtherActor)
{
	IRTSNavAgentInterface* NavAgent = GetNavAgentInterfaceFromOverlapActor(OtherActor);
	if (not NavAgent)
	{
		return false;
	}
	if (NavAgent->GetRTSNavAgentType() == ERTSNavAgents::HeavyTank)
	{
		TriggerDestroyActor(ERTSDeathType::Scavenging);
		return true;
	}
	return false;
}

bool ADestructableEnvActor::GetIsOverLapDestroyByMediumOrHeavyTank(AActor* OtherActor)
{
	IRTSNavAgentInterface* NavAgent = GetNavAgentInterfaceFromOverlapActor(OtherActor);
	if (not NavAgent)
	{
		return false;
	}
	const bool bIsHeavyTank = NavAgent->GetRTSNavAgentType() == ERTSNavAgents::HeavyTank;
	const bool bIsMediumTank = NavAgent->GetRTSNavAgentType() == ERTSNavAgents::MediumTank;
	if (bIsMediumTank || bIsHeavyTank)
	{
		TriggerDestroyActor(ERTSDeathType::Scavenging);
		return true;
	}
	return false;
}

bool ADestructableEnvActor::GetIsOverlapByAnyTank(AActor* OtherActor)
{
	IRTSNavAgentInterface* NavAgent = GetNavAgentInterfaceFromOverlapActor(OtherActor);
	if (not NavAgent)
	{
		return false;
	}
	if (NavAgent->GetRTSNavAgentType() != ERTSNavAgents::Character)
	{
		TriggerDestroyActor(ERTSDeathType::Scavenging);
		return true;
	}
	return false;
}

void ADestructableEnvActor::OnUnitDies_CheckForWireComponent() const
{
	UDestructibleWire* WireComponent = FindComponentByClass<UDestructibleWire>();
	if (IsValid(WireComponent))
	{
		WireComponent->OnOwningActorDies();
	}
}

void ADestructableEnvActor::AttemptAttachSpawnSystem(const FSwapToDestroyedMesh& CollapseParameters,
                                                     UNiagaraSystem* AttachSystem)
{
	if (not IsValid(AttachSystem))
	{
		return;
	}
	// Create component to attach the system to
	const auto NiagaraComponent = NewObject<UNiagaraComponent>(this);
	if (not IsValid(NiagaraComponent))
	{
		RTSFunctionLibrary::ReportError("Failed to create niagara attach system for destructable actor: " + GetName());
		return;
	}
	// attach wiht offset here.
	NiagaraComponent->SetAsset(AttachSystem);
	NiagaraComponent->AttachToComponent(CollapseParameters.ComponentToSwapOn,
	                                    FAttachmentTransformRules::KeepRelativeTransform);
	// Start system
	NiagaraComponent->Activate();
}

void ADestructableEnvActor::OnMeshCollapsedNoDestroy(UGeometryCollectionComponent* GeoComponent,
                                                     const bool bKeepGeometryVisible)
{
	BP_OnMeshCollapsed();
	if (not IsValid(GeoComponent))
	{
		RTSFunctionLibrary::ReportError("Invalid geo component in OnMeshCollapsedNoDestroy."
			"\n actor: " + GetName());
		return;
	}
	GeoComponent->SetVisibility(bKeepGeometryVisible);
	GeoComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// ============================================================================
// Overlap handlers now forward to the struct to queue the impulse.
// ============================================================================

void ADestructableEnvActor::HandleBeginOverlap_HeavyTank(UPrimitiveComponent* OverlappedComponent,
                                                         AActor* OtherActor,
                                                         UPrimitiveComponent* OtherComp,
                                                         const int32 /*OtherBodyIndex*/,
                                                         const bool bFromSweep,
                                                         const FHitResult& SweepResult)
{
	if (not IsValid(OtherActor))
	{
		return;
	}
	if (not GetIsOverLapDestroyByHeavyTank(OtherActor))
	{
		return;
	}
	switch (M_CrushDeathType)
	{
	case ECrushDeathType::Impulse:
	OnDestructibleCollapse.Broadcast();
		M_ImpulseOnCrushed.QueueImpulseFromOverlap(OverlappedComponent, OtherComp, bFromSweep, SweepResult, GetWorld());
		break;
	case ECrushDeathType::VerticalCollapse:
	OnDestructibleCollapse.Broadcast();
		M_VerticalCollapseOnCrushed.QueueCollapseFromOverlap(OverlappedComponent, OtherComp, bFromSweep, SweepResult,
		                                                     GetWorld());
		break;
	case ECrushDeathType::RegularDeath:
	OnDestructibleCollapse.Broadcast();
		OnUnitDies(ERTSDeathType::Scavenging);
		break;
	default:
		break;
	}
}

void ADestructableEnvActor::HandleBeginOverlap_MediumOrHeavy(UPrimitiveComponent* OverlappedComponent,
                                                             AActor* OtherActor,
                                                             UPrimitiveComponent* OtherComp,
                                                             const int32 /*OtherBodyIndex*/,
                                                             const bool bFromSweep,
                                                             const FHitResult& SweepResult)
{
	if (not IsValid(OtherActor))
	{
		return;
	}
	if (not GetIsOverLapDestroyByMediumOrHeavyTank(OtherActor))
	{
		return;
	}

	switch (M_CrushDeathType)
	{
	case ECrushDeathType::Impulse:
	OnDestructibleCollapse.Broadcast();
		M_ImpulseOnCrushed.QueueImpulseFromOverlap(OverlappedComponent, OtherComp, bFromSweep, SweepResult, GetWorld());
		break;
	case ECrushDeathType::VerticalCollapse:
	OnDestructibleCollapse.Broadcast();
		M_VerticalCollapseOnCrushed.QueueCollapseFromOverlap(OverlappedComponent, OtherComp, bFromSweep, SweepResult,
		                                                     GetWorld());
		break;
	case ECrushDeathType::RegularDeath:
	OnDestructibleCollapse.Broadcast();
		OnUnitDies(ERTSDeathType::Scavenging);
		break;
	default:
		break;
	}
}

void ADestructableEnvActor::HandleBeginOverlap_AnyTank(UPrimitiveComponent* OverlappedComponent,
                                                       AActor* OtherActor,
                                                       UPrimitiveComponent* OtherComp,
                                                       const int32 /*OtherBodyIndex*/,
                                                       const bool bFromSweep,
                                                       const FHitResult& SweepResult)
{
	if (not IsValid(OtherActor))
	{
		return;
	}
	if (not GetIsOverlapByAnyTank(OtherActor))
	{
		return;
	}

	switch (M_CrushDeathType)
	{
	case ECrushDeathType::Impulse:
		
	OnDestructibleCollapse.Broadcast();
		M_ImpulseOnCrushed.QueueImpulseFromOverlap(OverlappedComponent, OtherComp, bFromSweep, SweepResult, GetWorld());
		break;
	case ECrushDeathType::VerticalCollapse:
	OnDestructibleCollapse.Broadcast();
		M_VerticalCollapseOnCrushed.QueueCollapseFromOverlap(OverlappedComponent, OtherComp, bFromSweep, SweepResult,
		                                                     GetWorld());
		break;
	case ECrushDeathType::RegularDeath:
	OnDestructibleCollapse.Broadcast();
		OnUnitDies(ERTSDeathType::Scavenging);
		break;
	default:
		break;
	}
}

IRTSNavAgentInterface* ADestructableEnvActor::GetNavAgentInterfaceFromOverlapActor(AActor* OverlappingActor) const
{
	if (not IsValid(OverlappingActor))
	{
		return nullptr;
	}
	return Cast<IRTSNavAgentInterface>(OverlappingActor);
}

void ADestructableEnvActor::OnVerticalDestructionComplete()
{
	BP_OnVerticalDestructionComplete();
}
