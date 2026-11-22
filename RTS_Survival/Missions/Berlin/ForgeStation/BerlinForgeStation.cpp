// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BerlinForgeStation.h"

#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Enemy/Strongpoint/EnemyStrongpoint.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ForgeStationConstants
{
	constexpr float WatchTargetsIntervalSeconds = 0.5f;
	constexpr float FindStrongpointIntervalSeconds = 2.f;
	// Pulsing lights never go fully dark.
	constexpr float PulseMinimumFraction = 0.3f;
	constexpr float SedationVentFadeSeconds = 1.5f;
	// After the targets' own BeginPlay has initialised them from their Blueprint.
	constexpr float FinishTargetsDelaySeconds = 0.2f;
}

ABerlinForgeStation::ABerlinForgeStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
	SetRootComponent(ShellMesh);
	ShellMesh->SetMobility(EComponentMobility::Static);
}

ABerlinForgeStation* ABerlinForgeStation::FindForgeStation(const UObject* WorldContextObject)
{
	UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		return nullptr;
	}
	for (TActorIterator<ABerlinForgeStation> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ABerlinForgeStation::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_CacheTargets();
	BeginPlay_BuildRotatingParts();
	BeginPlay_BuildLights();
	BeginPlay_BuildEffects();
	BeginPlay_BuildSounds();
	BeginPlay_StartSedation();
	BeginPlay_StartTimers();
}

void ABerlinForgeStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_WatchTargetsTimerHandle);
		World->GetTimerManager().ClearTimer(M_FindStrongpointTimerHandle);
		World->GetTimerManager().ClearTimer(M_FinishTargetsTimerHandle);
	}
	if (M_Strongpoint.IsValid())
	{
		M_Strongpoint->OnGarrisonEngaged.RemoveDynamic(this, &ABerlinForgeStation::HandleGarrisonEngaged);
	}
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------
// BeginPlay
// ------------------------------------------------------------------

void ABerlinForgeStation::BeginPlay_CacheTargets()
{
	M_TargetTransforms.SetNum(Targets.Num());
	M_IsTargetDestroyed.Init(false, Targets.Num());
	for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
	{
		const AActor* Target = Targets[TargetIndex].Target;
		if (not IsValid(Target))
		{
			RTSFunctionLibrary::ReportError("Forge station " + GetName() + ": target slot " +
				FString::FromInt(TargetIndex) + " has no placed target.");
			M_TargetTransforms[TargetIndex] = GetActorTransform();
			continue;
		}
		M_TargetTransforms[TargetIndex] = Target->GetActorTransform();
		SilenceTargetEffects(*Target);
	}
}

void ABerlinForgeStation::FinishTargetConstruction()
{
	for (const FForgeStationTarget& Target : Targets)
	{
		ABuildingExpansion* Building = Cast<ABuildingExpansion>(Target.Target);
		if (not IsValid(Building) || Building->GetStatus() == EBuildingExpansionStatus::BXS_Built)
		{
			continue;
		}
		UStaticMeshComponent* BuildingMesh = Cast<UStaticMeshComponent>(Building->GetRootComponent());
		UStaticMesh* StationMesh = IsValid(BuildingMesh) ? BuildingMesh->GetStaticMesh() : nullptr;
		Building->StartFieldDefenseConstruction(Building->GetActorLocation(), Building->GetActorRotation());
		Building->FinishFieldDefenseConstruction();
		// The copied Blueprint hands the cooling tower's construction mesh and its materials to the construction; the
		// station part with its own Forge materials (and the sedated crystal) is the one to show.
		if (IsValid(BuildingMesh) && IsValid(StationMesh))
		{
			BuildingMesh->SetStaticMesh(StationMesh);
			BuildingMesh->EmptyOverrideMaterials();
			BuildingMesh->SetVisibility(true);
		}
		Building->SetActorHiddenInGame(false);
		SilenceTargetEffects(*Building);
	}
}

void ABerlinForgeStation::SilenceTargetEffects(const AActor& Target)
{
	// The target buildings are copies of the Forge cooling towers; their chimney smoke has no place on the station.
	TInlineComponentArray<UNiagaraComponent*> NiagaraComponents(&Target);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		NiagaraComponent->SetAutoActivate(false);
		NiagaraComponent->DeactivateImmediate();
		NiagaraComponent->SetVisibility(false);
	}
}

USceneComponent* ABerlinForgeStation::AttachNewComponent(USceneComponent* Component,
                                                         const FTransform& RelativeTransform)
{
	Component->SetupAttachment(GetRootComponent());
	Component->SetRelativeTransform(RelativeTransform);
	Component->RegisterComponent();
	return Component;
}

void ABerlinForgeStation::AddRuntimeDressing(USceneComponent* Component, const EForgeStationPartRole LinkedRole,
                                             const EForgeStationDressingUse Use)
{
	FForgeStationRuntimeDressing& Dressing = M_RuntimeDressing.AddDefaulted_GetRef();
	Dressing.Component = Component;
	Dressing.LinkedRole = LinkedRole;
	Dressing.Use = Use;
}

void ABerlinForgeStation::BeginPlay_BuildRotatingParts()
{
	for (const FForgeStationRotatingPart& Part : RotatingParts)
	{
		UStaticMesh* Mesh = Part.Mesh.LoadSynchronous();
		if (not IsValid(Mesh))
		{
			RTSFunctionLibrary::ReportError("Forge station " + GetName() + ": a rotating part has no mesh.");
			continue;
		}
		UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(this);
		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetStaticMesh(Mesh);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetCanEverAffectNavigation(false);
		AttachNewComponent(MeshComponent, Part.RelativeTransform);
		AddRuntimeDressing(MeshComponent, Part.LinkedRole, EForgeStationDressingUse::Ambient);
		M_RuntimeDressing.Last().DegreesPerSecond = Part.DegreesPerSecond;
	}
}

void ABerlinForgeStation::BeginPlay_BuildLights()
{
	for (const FForgeStationLight& Light : Lights)
	{
		ULocalLightComponent* LightComponent = nullptr;
		if (Light.bSpotLight)
		{
			USpotLightComponent* Spot = NewObject<USpotLightComponent>(this);
			Spot->SetOuterConeAngle(Light.OuterConeAngle);
			Spot->SetInnerConeAngle(Light.OuterConeAngle * 0.6f);
			LightComponent = Spot;
		}
		else
		{
			LightComponent = NewObject<UPointLightComponent>(this);
		}
		LightComponent->SetMobility(EComponentMobility::Movable);
		LightComponent->SetIntensityUnits(ELightUnits::Candelas);
		LightComponent->SetIntensity(Light.Intensity);
		LightComponent->SetLightColor(Light.Color);
		LightComponent->SetAttenuationRadius(Light.AttenuationRadius);
		LightComponent->SetCastShadows(false);
		AttachNewComponent(LightComponent, FTransform(Light.RelativeRotation, Light.RelativeLocation));
		AddRuntimeDressing(LightComponent, Light.LinkedRole, EForgeStationDressingUse::Ambient);
		if (Light.bPulse)
		{
			M_RuntimeDressing.Last().BaseIntensity = Light.Intensity;
			M_RuntimeDressing.Last().PulseSeconds = FMath::Max(Light.PulseSeconds, 0.1f);
		}
	}
}

void ABerlinForgeStation::BeginPlay_BuildEffects()
{
	for (const FForgeStationEffect& Effect : Effects)
	{
		UNiagaraSystem* System = Effect.System.LoadSynchronous();
		if (not IsValid(System))
		{
			RTSFunctionLibrary::ReportError("Forge station " + GetName() + ": an effect has no Niagara system.");
			continue;
		}
		UNiagaraComponent* NiagaraComponent = NewObject<UNiagaraComponent>(this);
		NiagaraComponent->SetAsset(System);
		NiagaraComponent->SetAutoActivate(Effect.Use == EForgeStationDressingUse::Ambient);
		AttachNewComponent(NiagaraComponent, Effect.RelativeTransform);
		if (Effect.Use == EForgeStationDressingUse::Ambient)
		{
			NiagaraComponent->Activate(true);
		}
		AddRuntimeDressing(NiagaraComponent, Effect.LinkedRole, Effect.Use);
	}
}

void ABerlinForgeStation::BeginPlay_BuildSounds()
{
	for (const FForgeStationSound& Sound : Sounds)
	{
		USoundBase* SoundAsset = Sound.Sound.LoadSynchronous();
		if (not IsValid(SoundAsset))
		{
			RTSFunctionLibrary::ReportError("Forge station " + GetName() + ": a sound slot has no sound.");
			continue;
		}
		UAudioComponent* AudioComponent = NewObject<UAudioComponent>(this);
		AudioComponent->SetSound(SoundAsset);
		AudioComponent->SetVolumeMultiplier(Sound.VolumeMultiplier);
		AudioComponent->bAutoActivate = false;
		AudioComponent->bAllowSpatialization = true;
		if (USoundAttenuation* Attenuation = Sound.Attenuation.LoadSynchronous())
		{
			AudioComponent->AttenuationSettings = Attenuation;
		}
		AttachNewComponent(AudioComponent, FTransform(Sound.RelativeLocation));
		AddRuntimeDressing(AudioComponent, Sound.LinkedRole, Sound.Use);
		FForgeStationRuntimeDressing& Dressing = M_RuntimeDressing.Last();
		Dressing.RepeatSeconds = Sound.RepeatSeconds;
		if (Sound.Use == EForgeStationDressingUse::Ambient)
		{
			// Staggered, so the yard does not start all its machines on the same frame.
			AudioComponent->Play(FMath::FRandRange(0.f, 1.f));
			Dressing.SecondsToReplay = Sound.RepeatSeconds;
		}
	}
}

void ABerlinForgeStation::BeginPlay_StartSedation()
{
	M_SedationCollection = Sedation.ParameterCollection.LoadSynchronous();
	WriteSedationParameters(0.f, 0.f);
	if (not Sedation.bEnabled)
	{
		bM_IsSedationStopped = true;
		return;
	}
	M_SecondsToNextSedation = Sedation.FirstCycleDelay;
}

void ABerlinForgeStation::BeginPlay_StartTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<ABerlinForgeStation> WeakThis(this);
	World->GetTimerManager().SetTimer(M_FinishTargetsTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->FinishTargetConstruction();
		}
	}), ForgeStationConstants::FinishTargetsDelaySeconds, false);
	World->GetTimerManager().SetTimer(M_WatchTargetsTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->WatchTargets();
		}
	}), ForgeStationConstants::WatchTargetsIntervalSeconds, true);
	// The strongpoint may be placed after the station in the level; look for it until found.
	World->GetTimerManager().SetTimer(M_FindStrongpointTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->TryBindStrongpoint();
		}
	}), ForgeStationConstants::FindStrongpointIntervalSeconds, true, 0.f);
}

// ------------------------------------------------------------------
// Targets
// ------------------------------------------------------------------

TArray<AActor*> ABerlinForgeStation::GetTargetActors() const
{
	TArray<AActor*> TargetActors;
	for (const FForgeStationTarget& Target : Targets)
	{
		if (IsValid(Target.Target))
		{
			TargetActors.Add(Target.Target);
		}
	}
	return TargetActors;
}

int32 ABerlinForgeStation::GetAliveTargetCount() const
{
	int32 AliveTargets = 0;
	for (const FForgeStationTarget& Target : Targets)
	{
		if (RTSFunctionLibrary::RTSIsValid(Target.Target.Get()))
		{
			++AliveTargets;
		}
	}
	return AliveTargets;
}

void ABerlinForgeStation::WatchTargets()
{
	for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
	{
		if (M_IsTargetDestroyed[TargetIndex] || RTSFunctionLibrary::RTSIsValid(Targets[TargetIndex].Target.Get()))
		{
			continue;
		}
		HandleTargetDestroyed(TargetIndex);
	}
}

void ABerlinForgeStation::HandleTargetDestroyed(const int32 TargetIndex)
{
	M_IsTargetDestroyed[TargetIndex] = true;
	const EForgeStationPartRole PartRole = Targets[TargetIndex].Role;
	SpawnWreck(TargetIndex);
	StopDressingLinkedTo(PartRole);
	if (PartRole == Sedation.LinkedRole)
	{
		StopSedation();
	}
	OnTargetDestroyed.Broadcast(this, PartRole);
}

void ABerlinForgeStation::SpawnWreck(const int32 TargetIndex)
{
	const FForgeStationTarget& Target = Targets[TargetIndex];
	const FTransform& WreckTransform = M_TargetTransforms[TargetIndex];
	if (UNiagaraSystem* DeathEffect = Target.DeathEffect.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DeathEffect, WreckTransform.GetLocation(),
		                                               WreckTransform.Rotator(),
		                                               FVector(Target.DeathEffectScale));
	}
	UStaticMesh* DestroyedMesh = Target.DestroyedMesh.LoadSynchronous();
	if (not IsValid(DestroyedMesh))
	{
		return;
	}
	UStaticMeshComponent* Wreck = NewObject<UStaticMeshComponent>(this);
	Wreck->SetMobility(EComponentMobility::Movable);
	Wreck->SetStaticMesh(DestroyedMesh);
	Wreck->SetCanEverAffectNavigation(false);
	Wreck->SetupAttachment(GetRootComponent());
	Wreck->SetWorldTransform(WreckTransform);
	Wreck->RegisterComponent();
	M_Wrecks.Add(Wreck);
}

void ABerlinForgeStation::StopDressingLinkedTo(const EForgeStationPartRole PartRole)
{
	if (PartRole == EForgeStationPartRole::None)
	{
		return;
	}
	for (FForgeStationRuntimeDressing& Dressing : M_RuntimeDressing)
	{
		if (Dressing.LinkedRole == PartRole)
		{
			DeactivateDressing(Dressing);
		}
	}
}

bool ABerlinForgeStation::GetIsTargetRoleDestroyed(const EForgeStationPartRole PartRole) const
{
	for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
	{
		if (Targets[TargetIndex].Role == PartRole && M_IsTargetDestroyed.IsValidIndex(TargetIndex))
		{
			return M_IsTargetDestroyed[TargetIndex];
		}
	}
	return false;
}

void ABerlinForgeStation::DeactivateDressing(FForgeStationRuntimeDressing& Dressing)
{
	USceneComponent* Component = Dressing.Component;
	Dressing.DegreesPerSecond = 0.f;
	Dressing.BaseIntensity = 0.f;
	Dressing.RepeatSeconds = 0.f;
	if (not IsValid(Component))
	{
		return;
	}
	if (UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(Component))
	{
		NiagaraComponent->Deactivate();
		return;
	}
	if (UAudioComponent* AudioComponent = Cast<UAudioComponent>(Component))
	{
		AudioComponent->FadeOut(ForgeStationConstants::SedationVentFadeSeconds, 0.f);
		return;
	}
	if (ULocalLightComponent* LightComponent = Cast<ULocalLightComponent>(Component))
	{
		LightComponent->SetVisibility(false);
		return;
	}
	// Rotating parts belong to their target; they vanish with it (the wreck has its own pieces).
	Component->SetVisibility(false);
}

void ABerlinForgeStation::StopStation()
{
	if (bM_IsStopped)
	{
		return;
	}
	bM_IsStopped = true;
	StopSedation();
	for (FForgeStationRuntimeDressing& Dressing : M_RuntimeDressing)
	{
		// The meshes of living targets stay visible; only machines and lights go quiet.
		if (Cast<UStaticMeshComponent>(Dressing.Component) != nullptr)
		{
			Dressing.DegreesPerSecond = 0.f;
			continue;
		}
		DeactivateDressing(Dressing);
	}
}

// ------------------------------------------------------------------
// Strongpoint
// ------------------------------------------------------------------

void ABerlinForgeStation::TryBindStrongpoint()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	for (TActorIterator<AEnemyStrongpoint> It(World); It; ++It)
	{
		if (It->GetStrongpointId() != StrongpointId)
		{
			continue;
		}
		M_Strongpoint = *It;
		M_Strongpoint->OnGarrisonEngaged.AddUniqueDynamic(this, &ABerlinForgeStation::HandleGarrisonEngaged);
		World->GetTimerManager().ClearTimer(M_FindStrongpointTimerHandle);
		if (M_Strongpoint->GetIsEngaged())
		{
			HandleGarrisonEngaged(M_Strongpoint.Get());
		}
		return;
	}
}

void ABerlinForgeStation::HandleGarrisonEngaged(AEnemyStrongpoint* Strongpoint)
{
	// The running cycle finishes; no new one starts while the station is under attack.
	bM_IsAlerted = true;
}

// ------------------------------------------------------------------
// Tick
// ------------------------------------------------------------------

void ABerlinForgeStation::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	M_RunningSeconds += DeltaSeconds;
	TickRotatingParts(DeltaSeconds);
	TickPulsingLights();
	TickRepeatingSounds(DeltaSeconds);
	TickSedation(DeltaSeconds);
}

void ABerlinForgeStation::TickRotatingParts(const float DeltaSeconds)
{
	for (const FForgeStationRuntimeDressing& Dressing : M_RuntimeDressing)
	{
		if (Dressing.DegreesPerSecond == 0.f || not IsValid(Dressing.Component))
		{
			continue;
		}
		Dressing.Component->AddLocalRotation(FRotator(0.f, Dressing.DegreesPerSecond * DeltaSeconds, 0.f));
	}
}

void ABerlinForgeStation::TickPulsingLights() const
{
	for (const FForgeStationRuntimeDressing& Dressing : M_RuntimeDressing)
	{
		ULocalLightComponent* LightComponent = Cast<ULocalLightComponent>(Dressing.Component);
		if (Dressing.BaseIntensity <= 0.f || not IsValid(LightComponent))
		{
			continue;
		}
		const float Wave = 0.5f + 0.5f * FMath::Sin(2.f * PI * M_RunningSeconds / Dressing.PulseSeconds);
		const float Fraction = FMath::Lerp(ForgeStationConstants::PulseMinimumFraction, 1.f, Wave);
		LightComponent->SetIntensity(Dressing.BaseIntensity * Fraction);
	}
}

void ABerlinForgeStation::TickRepeatingSounds(const float DeltaSeconds)
{
	for (FForgeStationRuntimeDressing& Dressing : M_RuntimeDressing)
	{
		UAudioComponent* AudioComponent = Cast<UAudioComponent>(Dressing.Component);
		if (Dressing.RepeatSeconds <= 0.f || Dressing.Use != EForgeStationDressingUse::Ambient
			|| not IsValid(AudioComponent))
		{
			continue;
		}
		Dressing.SecondsToReplay -= DeltaSeconds;
		if (Dressing.SecondsToReplay > 0.f)
		{
			continue;
		}
		Dressing.SecondsToReplay = Dressing.RepeatSeconds * FMath::FRandRange(0.8f, 1.2f);
		AudioComponent->Play();
	}
}

void ABerlinForgeStation::TickSedation(const float DeltaSeconds)
{
	if (bM_IsSedationStopped)
	{
		return;
	}
	if (M_SedationPhase == EForgeSedationPhase::Idle)
	{
		M_SecondsToNextSedation -= DeltaSeconds;
		if (M_SecondsToNextSedation <= 0.f && not bM_IsAlerted)
		{
			EnterSedationPhase(EForgeSedationPhase::Pulse);
		}
		return;
	}
	M_SedationPhaseSeconds += DeltaSeconds;
	WriteSedationParameters(GetSedationPulseValue(), GetSedationFrostValue());
	switch (M_SedationPhase)
	{
	case EForgeSedationPhase::Pulse:
		if (M_SedationPhaseSeconds >= Sedation.PulseSeconds)
		{
			EnterSedationPhase(EForgeSedationPhase::Vent);
		}
		break;
	case EForgeSedationPhase::Vent:
		if (M_SedationPhaseSeconds >= Sedation.VentSeconds)
		{
			EnterSedationPhase(EForgeSedationPhase::Thaw);
		}
		break;
	case EForgeSedationPhase::Thaw:
		if (M_SedationPhaseSeconds >= Sedation.ThawSeconds)
		{
			EnterSedationPhase(EForgeSedationPhase::Idle);
		}
		break;
	default:
		break;
	}
}

void ABerlinForgeStation::EnterSedationPhase(const EForgeSedationPhase NewPhase)
{
	const EForgeSedationPhase OldPhase = M_SedationPhase;
	M_SedationPhase = NewPhase;
	M_SedationPhaseSeconds = 0.f;
	if (NewPhase == EForgeSedationPhase::Vent)
	{
		SetSedationVentActive(true);
	}
	if (OldPhase == EForgeSedationPhase::Vent)
	{
		SetSedationVentActive(false);
	}
	if (NewPhase == EForgeSedationPhase::Pulse)
	{
		// The next cycle counts from this start.
		M_SecondsToNextSedation = Sedation.IntervalSeconds;
	}
	if (NewPhase == EForgeSedationPhase::Idle)
	{
		const float CycleSeconds = Sedation.PulseSeconds + Sedation.VentSeconds + Sedation.ThawSeconds;
		M_SecondsToNextSedation = FMath::Max(Sedation.IntervalSeconds - CycleSeconds, 0.f);
		WriteSedationParameters(0.f, 0.f);
	}
}

void ABerlinForgeStation::SetSedationVentActive(const bool bActive)
{
	for (FForgeStationRuntimeDressing& Dressing : M_RuntimeDressing)
	{
		if (Dressing.Use != EForgeStationDressingUse::SedationVent || not IsValid(Dressing.Component))
		{
			continue;
		}
		if (UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(Dressing.Component))
		{
			bActive ? NiagaraComponent->Activate(true) : NiagaraComponent->Deactivate();
			continue;
		}
		if (UAudioComponent* AudioComponent = Cast<UAudioComponent>(Dressing.Component))
		{
			bActive
				? AudioComponent->Play()
				: AudioComponent->FadeOut(ForgeStationConstants::SedationVentFadeSeconds, 0.f);
		}
	}
}

void ABerlinForgeStation::StopSedation()
{
	if (M_SedationPhase == EForgeSedationPhase::Vent)
	{
		SetSedationVentActive(false);
	}
	bM_IsSedationStopped = true;
	M_SedationPhase = EForgeSedationPhase::Idle;
	WriteSedationParameters(0.f, 0.f);
}

float ABerlinForgeStation::GetSedationPulseValue() const
{
	if (M_SedationPhase != EForgeSedationPhase::Pulse)
	{
		return 0.f;
	}
	// Up and back down over the pulse step.
	const float Alpha = FMath::Clamp(M_SedationPhaseSeconds / Sedation.PulseSeconds, 0.f, 1.f);
	return FMath::Sin(PI * Alpha);
}

float ABerlinForgeStation::GetSedationFrostValue() const
{
	if (M_SedationPhase == EForgeSedationPhase::Vent)
	{
		return FMath::Clamp(M_SedationPhaseSeconds / Sedation.VentSeconds, 0.f, 1.f);
	}
	if (M_SedationPhase == EForgeSedationPhase::Thaw)
	{
		return 1.f - FMath::Clamp(M_SedationPhaseSeconds / Sedation.ThawSeconds, 0.f, 1.f);
	}
	return 0.f;
}

void ABerlinForgeStation::WriteSedationParameters(const float Pulse, const float Frost)
{
	if (not IsValid(M_SedationCollection))
	{
		return;
	}
	UKismetMaterialLibrary::SetScalarParameterValue(this, M_SedationCollection, Sedation.PulseParameter, Pulse);
	UKismetMaterialLibrary::SetScalarParameterValue(this, M_SedationCollection, Sedation.FrostParameter, Frost);
}
