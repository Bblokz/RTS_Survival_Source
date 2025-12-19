#include "CollapseFXParameters.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"


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


void FCollapseFX::CreateFX(const AActor* WorldContext) const
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if(not World)
	{
		return;
	}
	if(CollapseVfx)
	{
		const FVector SpawnLocation = WorldContext->GetActorLocation() + FxLocationOffset;
		UNiagaraComponent* Niagara =  UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, CollapseVfx, SpawnLocation, VfxRotation, VfxScale);
		AdjustFX(Niagara);
	}
	if(CollapseSfx)
	{
		const FVector SpawnLocation = WorldContext->GetActorLocation() + FxLocationOffset;
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
}

void FCollapseFX::AdjustFX(UNiagaraComponent* NiagaraComp) const
{
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
