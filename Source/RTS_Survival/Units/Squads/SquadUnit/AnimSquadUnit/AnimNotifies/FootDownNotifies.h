#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "FootDownNotifies.generated.h"

// Forward declaration
class UNiagaraSystem;

/**
 * ULeftFootDownNotify
 * Notifies when the left foot touches the ground.
 */
UCLASS()
class RTS_SURVIVAL_API ULeftFootDownNotify : public UAnimNotify
{
    GENERATED_BODY()

public:
    // Override the Notify function
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};

/**
 * URightFootDownNotify
 * Notifies when the right foot touches the ground.
 */
UCLASS()
class RTS_SURVIVAL_API URightFootDownNotify : public UAnimNotify
{
    GENERATED_BODY()

public:
    // Override the Notify function
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
