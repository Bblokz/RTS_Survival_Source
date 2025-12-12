// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CaptureMechanic/CaptureInterface/CaptureInterface.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "CaptureActor.generated.h"

class UMeshComponent;

USTRUCT(Blueprintable)
struct FCaptureSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 AmountCaptureUnitsNeeded = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CaptureDuration = 5.f;
};


UCLASS()
class RTS_SURVIVAL_API ACaptureActor : public ADestructableEnvActor, public ICaptureInterface
{
	GENERATED_BODY()

public:
	ACaptureActor(const FObjectInitializer& ObjectInitializer);

	// ICaptureInterface
	virtual int32 GetCaptureUnitAmountNeeded() const override;
	virtual void OnCaptureByPlayer(const int32 Player) override;
	virtual FVector GetCaptureLocationClosestTo(const FVector& FromLocation) override;
	// ~ICaptureInterface

	/** Text that can be shown while this actor is being captured. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capture")
	FText CaptureProgressText;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/**
	 * @brief Caches all socket locations on MeshComp whose name contains SocketNameContaining.
	 * The locations are stored relative to the actor location so they can be reconstructed later.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="BeginPlay")
	void SetupCaptureLocations(UMeshComponent* MeshComp, const FName SocketNameContaining);

	/** Called internally when capture progress has completed for the current capturing player. */
	UFUNCTION()
	void OnCaptureProgressComplete();
	void StartCaptureProgressBarForCaptureActor(ACaptureActor& CaptureActor) const;

	/** Settings controlling how many units are needed and how long the capture takes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capture")
	FCaptureSettings CaptureSettings;

	/** Blueprint hook fired when a capture has completely finished. */
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnCapturedByPlayer(const int32 Player);

private:
	/** Cached socket locations relative to the actor location. */
	UPROPERTY()
	TArray<FVector> M_CaptureLocationsRelativeToWorldLocation;

	/** Player index currently capturing this actor, or INDEX_NONE when idle. */
	int32 M_CurrentCapturingPlayer;

	/** Timer handle used for capture progress. */
	FTimerHandle M_CaptureTimerHandle;
};
