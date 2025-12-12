#include "FootDownNotifies.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"

void ULeftFootDownNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp) return;

    // Cast to USquadUnitAnimInstance to access custom properties
    if (USquadUnitAnimInstance* AnimInstance = Cast<USquadUnitAnimInstance>(MeshComp->GetAnimInstance()))
    {
        if (AnimInstance->LeftFootSocketName.IsNone()) return;

        // Get the location of the left foot socket
        FVector SocketLocation = MeshComp->GetSocketLocation(AnimInstance->LeftFootSocketName);

        // Spawn the Niagara effect at the location
        if (AnimInstance->FootstepEffect)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                MeshComp->GetWorld(), 
                AnimInstance->FootstepEffect, 
                SocketLocation
            );
        }
    }
}

void URightFootDownNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp) return;

    // Cast to USquadUnitAnimInstance to access custom properties
    if (USquadUnitAnimInstance* AnimInstance = Cast<USquadUnitAnimInstance>(MeshComp->GetAnimInstance()))
    {
        if (AnimInstance->RightFootSocketName.IsNone()) return;

        // Get the location of the right foot socket
        FVector SocketLocation = MeshComp->GetSocketLocation(AnimInstance->RightFootSocketName);

        // Spawn the Niagara effect at the location
        if (AnimInstance->FootstepEffect)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                MeshComp->GetWorld(),
                AnimInstance->FootstepEffect,
                SocketLocation
            );
        }
    }
}
