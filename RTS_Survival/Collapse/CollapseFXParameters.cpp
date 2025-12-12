#include "CollapseFXParameters.h"

#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"


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
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, CollapseVfx, SpawnLocation, VfxRotation, VfxScale);		
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
