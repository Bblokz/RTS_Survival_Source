// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_DigInProgress.generated.h"

class UProgressBar;

UCLASS()
class RTS_SURVIVAL_API UW_DigInProgress : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 
     * Starts filling the progress bar from StartProgress up to 1.0 over TotalProgressionTime seconds.
     * Every 0.1s, UpdateProgress() will be called to increment the bar smoothly.
     * @param StartProgress     initial percentage [0..1]
     * @param TotalProgressionTime   time (in seconds) over which to go from StartProgress → 1.0
     * @param WorldContext      any valid UObject whose GetWorld() is non‐null (often “this” or “GetOwningPlayer()”)
     * @param InDrawSize
     */
    void FillProgressBar(const float StartProgress, const float TotalProgressionTime, UObject* WorldContext, const FVector2D& InDrawSize);
    
    void StopProgressBar();
    virtual void BeginDestroy() override;

protected:
    /** Bind this in the UMG designer */
    UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
    TObjectPtr<UProgressBar> ProgressBar;

private:
    /** Timer handle for our 0.1s ticking */
    FTimerHandle M_ProgressTimerHandle;

    /** Current normalized progress [0..1] */
    float M_CurrentProgress = 0.0f;
    /** How many seconds remain until we reach 1.0 */
    float M_TimeRemaining = 0.0f;
    /** Amount to add to M_CurrentProgress every 0.1s */
    float M_ProgressIncrement = 0.0f;

    /** We store the world context as a weak pointer to avoid dangling pointers */
    TWeakObjectPtr<UObject> M_WorldContext;

    /** Called by the timer every 0.1s */
    void UpdateProgress();

    /** Checks that M_WorldContext is still valid and its World() is non‐null. */
    bool EnsureWorldContextIsValid() const;

    void StopBar();
};
