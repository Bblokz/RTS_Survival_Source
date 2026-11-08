// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldWorkshopComponent.h"

#include "TimerManager.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "FieldWorkshopRearmAura.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/CargoParachuteDescentComponent.h"
#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/AOEBehaviourComponent.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace FieldWorkshopConstants
{
	constexpr float PackUpStepSeconds = 0.03f;
	// The second dust wave rises when the building is this far through its sink.
	constexpr float SecondDustWaveAlpha = 0.5f;
}

UFieldWorkshopComponent::UFieldWorkshopComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UFieldWorkshopComponent::GetWorkingSecondsRemaining() const
{
	switch (M_State)
	{
	case EFieldWorkshopState::WaitingForLanding:
		return M_WorkshopSettings.WorkingSeconds;
	case EFieldWorkshopState::Working:
		{
			const UWorld* World = GetWorld();
			const float Elapsed = IsValid(World) ? World->GetTimeSeconds() - M_WorkStartSeconds : 0.f;
			return FMath::Max(0.f, M_WorkshopSettings.WorkingSeconds - Elapsed);
		}
	default:
		return 0.f;
	}
}

float UFieldWorkshopComponent::GetWorkshopRadius() const
{
	return M_RearmAura.IsValid() ? M_RearmAura->GetRearmRadius() : 0.f;
}

void UFieldWorkshopComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_CacheWorkshopParts();
	BeginPlay_BindWorkshopDeath();
	BeginPlay_StartLandingChecks();
}

void UFieldWorkshopComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimers();
	HideWorkingRadius();
	Super::EndPlay(EndPlayReason);
}

void UFieldWorkshopComponent::BeginPlay_CacheWorkshopParts()
{
	M_Workshop = Cast<ABuildingExpansion>(GetOwner());
	if (not GetIsValidWorkshop())
	{
		return;
	}
	TArray<UAOEBehaviourComponent*> Auras;
	M_Workshop->GetComponents<UAOEBehaviourComponent>(Auras);
	for (UAOEBehaviourComponent* Aura : Auras)
	{
		M_Auras.Add(Aura);
		if (UFieldWorkshopRearmAura* RearmAura = Cast<UFieldWorkshopRearmAura>(Aura))
		{
			M_RearmAura = RearmAura;
		}
	}
	if (UWorld* World = GetWorld())
	{
		M_RadiusPoolSubsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>();
	}
	// A component's BeginPlay runs before the actor's other components may have started their aura timers; the
	// first landing check switches them off again in that case.
	SetAurasEnabled(false);
}

void UFieldWorkshopComponent::BeginPlay_BindWorkshopDeath()
{
	if (not GetIsValidWorkshop())
	{
		return;
	}
	M_Workshop->OnUnitDies.AddWeakLambda(this, [this]()
	{
		OnWorkshopDies();
	});
}

void UFieldWorkshopComponent::BeginPlay_StartLandingChecks()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<UFieldWorkshopComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(M_LandingCheckHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->CheckForLanding();
		}
	}), M_WorkshopSettings.LandingCheckIntervalSeconds, true);
}

void UFieldWorkshopComponent::CheckForLanding()
{
	if (M_State != EFieldWorkshopState::WaitingForLanding)
	{
		return;
	}
	if (not GetHasLanded())
	{
		SetAurasEnabled(false);
		return;
	}
	StartWorking();
}

bool UFieldWorkshopComponent::GetHasLanded() const
{
	if (not GetIsValidWorkshop() || not M_Workshop->IsUnitAlive())
	{
		return false;
	}
	if (M_Workshop->GetStatus() != EBuildingExpansionStatus::BXS_Built)
	{
		return false;
	}
	// UGA_BuildingParachuteDrop switches collision off for the fall and back on when the load touches down.
	if (not M_Workshop->GetActorEnableCollision())
	{
		return false;
	}
	const UCargoParachuteDescentComponent* Descent = M_Workshop->FindComponentByClass<
		UCargoParachuteDescentComponent>();
	return not IsValid(Descent) || not Descent->GetIsDescending();
}

void UFieldWorkshopComponent::StartWorking()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_LandingCheckHandle);
	M_State = EFieldWorkshopState::Working;
	M_WorkStartSeconds = World->GetTimeSeconds();
	SetAurasEnabled(true);
	ShowWorkingRadius();

	TWeakObjectPtr<UFieldWorkshopComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(M_WorkingTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->StartPackingUp();
		}
	}), M_WorkshopSettings.WorkingSeconds, false);
	UE_LOG(LogTemp, Display, TEXT("RTS_FIELD_WORKSHOP working=%s for=%.0fs radius=%.0f auras=%d"),
	       *GetNameSafe(GetOwner()), M_WorkshopSettings.WorkingSeconds, GetWorkshopRadius(), M_Auras.Num());
}

void UFieldWorkshopComponent::StartPackingUp()
{
	if (M_State != EFieldWorkshopState::Working || not GetIsValidWorkshop())
	{
		return;
	}
	M_State = EFieldWorkshopState::PackingUp;
	HideWorkingRadius();
	TearDownAuras();
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// The crew strikes the depot: it sinks out of sight in a cloud of dust. No collision on the way down, so units
	// standing on or against it are not lifted or blocked by a building that is leaving.
	M_Workshop->SetActorEnableCollision(false);
	M_PackUpStartSeconds = World->GetTimeSeconds();
	M_PackUpStartLocation = M_Workshop->GetActorLocation();
	bM_SpawnedSecondDustWave = false;
	SpawnPackUpSmoke();
	UE_LOG(LogTemp, Display, TEXT("RTS_FIELD_WORKSHOP packing_up=%s"), *M_Workshop->GetName());

	TWeakObjectPtr<UFieldWorkshopComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(M_PackUpTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->TickPackingUp();
		}
	}), FieldWorkshopConstants::PackUpStepSeconds, true);
}

void UFieldWorkshopComponent::TickPackingUp()
{
	const UWorld* World = GetWorld();
	if (M_State != EFieldWorkshopState::PackingUp || not IsValid(World) || not GetIsValidWorkshop())
	{
		return;
	}
	const float Alpha = FMath::Clamp(
		(World->GetTimeSeconds() - M_PackUpStartSeconds) / M_WorkshopSettings.PackUpSeconds, 0.f, 1.f);
	// Ease in: it settles slowly, then drops away.
	const float Depth = M_WorkshopSettings.PackUpSinkDepthCm * Alpha * Alpha;
	(void)M_Workshop->SetActorLocation(M_PackUpStartLocation - FVector(0.f, 0.f, Depth));
	if (not bM_SpawnedSecondDustWave && Alpha >= FieldWorkshopConstants::SecondDustWaveAlpha)
	{
		bM_SpawnedSecondDustWave = true;
		SpawnPackUpSmoke();
	}
	if (Alpha < 1.f)
	{
		return;
	}
	if (UWorld* MutableWorld = GetWorld())
	{
		MutableWorld->GetTimerManager().ClearTimer(M_PackUpTimerHandle);
	}
	FinishPackingUp();
}

void UFieldWorkshopComponent::SpawnPackUpSmoke() const
{
	UNiagaraSystem* Smoke = M_WorkshopSettings.PackUpSmoke.LoadSynchronous();
	if (not IsValid(Smoke) || not GetIsValidWorkshop())
	{
		return;
	}
	const FVector Centre = M_PackUpStartLocation;
	const FVector Scale(M_WorkshopSettings.PackUpSmokeScale);
	// The second wave is turned half a step so the two waves do not stack on the same spots.
	const float WaveYawOffset = bM_SpawnedSecondDustWave ? 180.f / FMath::Max(1, M_WorkshopSettings.PackUpSmokePuffs) : 0.f;
	for (int32 Puff = 0; Puff < M_WorkshopSettings.PackUpSmokePuffs; ++Puff)
	{
		const float Yaw = WaveYawOffset + 360.f * Puff / M_WorkshopSettings.PackUpSmokePuffs;
		const FVector Offset = FRotator(0.f, Yaw, 0.f).Vector() * M_WorkshopSettings.PackUpSmokeRadiusCm;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Smoke, Centre + Offset, FRotator::ZeroRotator, Scale);
	}
}

void UFieldWorkshopComponent::FinishPackingUp() const
{
	if (M_State != EFieldWorkshopState::PackingUp || not GetIsValidWorkshop() || not M_Workshop->IsUnitAlive())
	{
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("RTS_FIELD_WORKSHOP packed_up=%s"), *M_Workshop->GetName());
	// Sets the packed-up status (a field defense has no owner to tell, so EndPlay reports nothing) and destroys it.
	M_Workshop->FinishPackUpExpansion();
}

void UFieldWorkshopComponent::OnWorkshopDies()
{
	if (M_State == EFieldWorkshopState::Destroyed)
	{
		return;
	}
	M_State = EFieldWorkshopState::Destroyed;
	ClearTimers();
	HideWorkingRadius();
	TearDownAuras();
	UE_LOG(LogTemp, Display, TEXT("RTS_FIELD_WORKSHOP destroyed=%s"), *GetNameSafe(GetOwner()));
}

void UFieldWorkshopComponent::SetAurasEnabled(const bool bEnabled) const
{
	for (const TWeakObjectPtr<UAOEBehaviourComponent>& Aura : M_Auras)
	{
		if (Aura.IsValid())
		{
			Aura->SetAOEEnabled(bEnabled);
		}
	}
}

void UFieldWorkshopComponent::TearDownAuras()
{
	SetAurasEnabled(false);
	for (const TWeakObjectPtr<UAOEBehaviourComponent>& Aura : M_Auras)
	{
		if (Aura.IsValid())
		{
			// Its EndPlay removes the behaviours it applied (the permanent reload buff included) from every unit.
			Aura->DestroyComponent();
		}
	}
	M_Auras.Reset();
	M_RearmAura.Reset();
}

void UFieldWorkshopComponent::ShowWorkingRadius()
{
	const float Radius = GetWorkshopRadius();
	if (M_RadiusId > 0 || Radius <= 0.f || not GetIsValidRadiusPoolSubsystem() || not GetIsValidWorkshop())
	{
		return;
	}
	M_RadiusId = M_RadiusPoolSubsystem->CreateRTSRadius(
		M_Workshop->GetActorLocation(), Radius, M_WorkshopSettings.WorkingRadiusType);
	if (M_RadiusId <= 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("UFieldWorkshopComponent could not create its working radius."));
		return;
	}
	M_RadiusPoolSubsystem->AttachRTSRadiusToActorYawOnly(M_RadiusId, M_Workshop.Get(), FVector::ZeroVector);
}

void UFieldWorkshopComponent::HideWorkingRadius()
{
	if (M_RadiusId <= 0)
	{
		return;
	}
	if (M_RadiusPoolSubsystem.IsValid())
	{
		M_RadiusPoolSubsystem->HideRTSRadiusById(M_RadiusId);
	}
	M_RadiusId = INDEX_NONE;
}

void UFieldWorkshopComponent::ClearTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_LandingCheckHandle);
	World->GetTimerManager().ClearTimer(M_WorkingTimerHandle);
	World->GetTimerManager().ClearTimer(M_PackUpTimerHandle);
}

bool UFieldWorkshopComponent::GetIsValidWorkshop() const
{
	if (M_Workshop.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_Workshop", "GetIsValidWorkshop", GetOwner());
	return false;
}

bool UFieldWorkshopComponent::GetIsValidRadiusPoolSubsystem() const
{
	if (M_RadiusPoolSubsystem.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_RadiusPoolSubsystem", "GetIsValidRadiusPoolSubsystem", GetOwner());
	return false;
}
