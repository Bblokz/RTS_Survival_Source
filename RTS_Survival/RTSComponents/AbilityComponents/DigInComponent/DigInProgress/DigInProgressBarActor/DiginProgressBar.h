// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiginProgressBar.generated.h"

class UWidgetComponent;
class UW_DigInProgress;

/**
 *  An actor that spawns a single world-space UWidgetComponent,
 *  initializes it to a UW_DigInProgress, and in Tick() rotates it
 *  so that it always faces the player camera.
 */
UCLASS()
class RTS_SURVIVAL_API ADiginProgressBar : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ADiginProgressBar();

    void StopProgressBar();

    /** 
     *  Initialize the progress bar with:
     *   - InWidgetClass: the UUserWidget subclass (UW_DigInProgress)
     *   - InTotalTime: total build time (seconds)
     *   - InStartPercent: initial [0..1] fill amount
     *   - InDrawSize: (X,Y) in pixels to size the world-space quad
     */
    void InitializeProgressBar(
        TSubclassOf<UW_DigInProgress> InWidgetClass,
        float InTotalTime,
        float InStartPercent,
        const FVector2D& InDrawSize
    );

protected:
    virtual void BeginPlay() override;

    virtual void BeginDestroy() override;

public:
    virtual void Tick(float DeltaSeconds) override;

private:
    /** Root component to hold the widget */
    UPROPERTY(VisibleDefaultsOnly)
    USceneComponent* SceneRoot;

    /** The world-space widget component that hosts UW_DigInProgress */
    UPROPERTY(VisibleAnywhere)
    UWidgetComponent* ProgressWidgetComponent;

    /** The UUserWidget subclass (should be UW_DigInProgress or derived) */
    TSubclassOf<UW_DigInProgress> WidgetClass;

    /** Total progression time (seconds) */
    float TotalTime = 0.0f;

    /** Initial fill amount [0..1] */
    float StartPercent = 0.0f;

    /** Desired draw size of the widget quad in world-space (pixels) */
    FVector2D DrawSize = FVector2D::ZeroVector;

    /** Has InitializeProgressBar() been called? */
    bool bIsInitialized = false;

    /** Called internally (on next tick) to finish widget setup */
    void FinishInitializeWidget();

};
