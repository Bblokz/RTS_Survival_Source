#include "CollapseFXParameters.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"


namespace COllapseVFXNiagaraParams
{
	const FName ScaleParamName = FName("SizeMlt");
	const FName LifeTimeName = "LifetimeMlt";
	const FName UseMuzzleSmokeName = "UseMuzzleSmoke";
	const FName UseGroundSmokeName = "UseGroundSmoke";
	const FName GroundZOffsetName = "GroundZOffset";
	const FName ParticlesMltName = "NormalizedWeaponCalibre";
	const FName SandBagSizeMltName = "SandbagSizeMlt";
	const FName SandBagSpawnCountName = "Spawn Count";
	
	FName ColorName = "MuzzleColor";
}

namespace CollapseVfxLifetime
{
	// Once VfxLifetime ends the system stops spawning; particles still alive get this long to fade before removal.
	constexpr float FadeOutSeconds = 2.f;

	void StopSpawning(const TWeakObjectPtr<UNiagaraComponent> WeakNiagara)
	{
		UNiagaraComponent* Niagara = WeakNiagara.Get();
		if (not IsValid(Niagara))
		{
			return;
		}
		Niagara->Deactivate();
	}

	void Remove(const TWeakObjectPtr<UNiagaraComponent> WeakNiagara)
	{
		UNiagaraComponent* Niagara = WeakNiagara.Get();
		if (not IsValid(Niagara))
		{
			return;
		}
		Niagara->DestroyComponent();
	}
}


void FCollapseFX::CreateFX(const AActor* WorldContext) const
{
	if(not WorldContext)
	{
		return;
	}
	const FVector SpawnLocation = WorldContext->GetActorLocation() + FxLocationOffset;
	SpawnVfxAtLocation(WorldContext, SpawnLocation);
	PlaySfxAtLocation(WorldContext, SpawnLocation);
}

void FCollapseFX::SpawnVfxAtLocation(const UObject* WorldContext, const FVector& SpawnLocation) const
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if(not World || not CollapseVfx)
	{
		return;
	}
	UNiagaraComponent* Niagara =  UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, CollapseVfx, SpawnLocation, VfxRotation, VfxScale);
	AdjustFX(Niagara);
}

void FCollapseFX::PlaySfxAtLocation(const UObject* WorldContext, const FVector& SpawnLocation) const
{
	if(not WorldContext || not WorldContext->GetWorld() || not CollapseSfx)
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(
		WorldContext,
		CollapseSfx,
		SpawnLocation,
		FRotator::ZeroRotator,
		1.f,
		1.f,
		0.f,
		SfxAttenuation,
		SfxConcurrency
	);
}

void FCollapseFX::AdjustFX(UNiagaraComponent* NiagaraComp) const
{
	LimitVfxLifetime(NiagaraComp);
	if(not NiagaraComp ||CollapseNiagaraParameters.VFXAdjustParams == ECollapseVFXAdjustNiagaraParams::Not)
	{
		return;
	}
	switch (CollapseNiagaraParameters.VFXAdjustParams) {
	case ECollapseVFXAdjustNiagaraParams::Not:
		break;
	case ECollapseVFXAdjustNiagaraParams::SetScaleColorLifetime:
		AdjustFxForSetScaleColorLifetime(NiagaraComp);
		break;
	case ECollapseVFXAdjustNiagaraParams::SandbagSizeAndCount:
		AdjustFxForSandbagSizeAndCount(NiagaraComp);
		break;
	}
	
}

void FCollapseFX::LimitVfxLifetime(UNiagaraComponent* NiagaraComp) const
{
	if (VfxLifetime <= 0.f || not IsValid(NiagaraComp))
	{
		return;
	}
	UWorld* World = NiagaraComp->GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// Timers live on the world: the actor that spawned the system is usually destroyed long before they fire.
	const TWeakObjectPtr<UNiagaraComponent> WeakNiagara(NiagaraComp);
	FTimerHandle StopSpawningHandle;
	World->GetTimerManager().SetTimer(
		StopSpawningHandle,
		FTimerDelegate::CreateStatic(&CollapseVfxLifetime::StopSpawning, WeakNiagara),
		VfxLifetime,
		false);
	FTimerHandle RemoveHandle;
	World->GetTimerManager().SetTimer(
		RemoveHandle,
		FTimerDelegate::CreateStatic(&CollapseVfxLifetime::Remove, WeakNiagara),
		VfxLifetime + CollapseVfxLifetime::FadeOutSeconds,
		false);
}

void FCollapseFX::AdjustFxForSetScaleColorLifetime(UNiagaraComponent* NiagaraComp) const
{
	using  namespace COllapseVFXNiagaraParams;
	NiagaraComp->SetBoolParameter(UseGroundSmokeName, true);
	NiagaraComp->SetBoolParameter(UseMuzzleSmokeName, false);
	NiagaraComp->SetFloatParameter(ScaleParamName, CollapseNiagaraParameters.SizeMlt);
	NiagaraComp->SetFloatParameter(LifeTimeName, CollapseNiagaraParameters.LifeTimeMlt);
	NiagaraComp->SetFloatParameter(ParticlesMltName, CollapseNiagaraParameters.ParticlesMlt);
	NiagaraComp->SetVectorParameter(GroundZOffsetName, CollapseNiagaraParameters.GroundSmokeOffset);
	NiagaraComp->SetColorParameter(ColorName, CollapseNiagaraParameters.SmokeColor);
}

void FCollapseFX::AdjustFxForSandbagSizeAndCount(UNiagaraComponent* NiagaraComp) const
{
	using namespace COllapseVFXNiagaraParams;
	NiagaraComp->SetFloatParameter(SandBagSizeMltName, CollapseNiagaraParameters.SizeMlt);
	// Used as counter here!
	NiagaraComp->SetFloatParameter(SandBagSpawnCountName, CollapseNiagaraParameters.ParticlesMlt);
	
}
