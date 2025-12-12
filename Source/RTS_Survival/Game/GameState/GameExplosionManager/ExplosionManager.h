#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExplosionTypeSystems.h/ExplosionTypeSystems.h"
#include "ExplosionManager.generated.h"

class UNiagaraSystem;

/**
 * A simple manager that holds arrays of NiagaraSystem assets for each explosion type.
 * When requested, it picks a random system from that type's list and spawns it at
 * the desired location using NiagaraFunctionLibrary::SpawnSystemAtLocation.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UGameExplosionsManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UGameExplosionsManager();

    /**
     * Called to populate the internal M_ExplosionSystemsMap from user-provided data.
     * For each ERTS_ExplosionType, an array of possible NiagaraSystems is stored.
     */
    UFUNCTION(BlueprintCallable, Category="RTS Explosion Manager")
    void InitExplosionManager(TArray<FExplosionTypeSystems> ExplSystemsData, USoundAttenuation* ExplSoundAtt, USoundConcurrency* ExplSoundConc);

    /**
     * Spawns (activates) an explosion of the given type at the given location.
     * Randomly selects a Niagara asset from M_ExplosionSystemsMap[ExplosionType]
     * and spawns it via NiagaraFunctionLibrary.
     *
     * If Delay is nearly zero, spawn immediately.
     * Otherwise, schedule a timer so the explosion spawns after Delay seconds.
     */
    UFUNCTION(BlueprintCallable, Category="RTS Explosion Manager")
    void SpawnRTSExplosionAtLocation(ERTS_ExplosionType ExplosionType,
    	const FVector& SpawnLocation,
    	const bool bPlaySound = false,
    	float Delay = 0.f);

	
    UFUNCTION(BlueprintCallable, Category="RTS Explosion Manager")
	void SpawnExplosionAtLocationCustomSound(
		ERTS_ExplosionType ExplosionType,
		const FVector& SpawnLocation,
		USoundCue* CustomSound,
		float Delay = 0.f);

    /**
     * Spawns multiple explosions of a given type around a specified center, within a given range,
     * attempting to keep each explosion at least PreferredDistance away from all the previously placed ones.
     */
    UFUNCTION(BlueprintCallable, Category="RTS Explosion Manager")
    void SpawnMultipleExplosionsInArea(
        ERTS_ExplosionType ExplosionType,
        const FVector& CenterLocation,
        int32 ExplosionCount,
        float Range,
        float PreferredDistance
    );

private:
    /**
     * Invoked by timer to spawn an explosion after a delay.
     */
    UFUNCTION()
    void DelayedExplosionSpawn(ERTS_ExplosionType ExplosionType, const FVector& SpawnLocation, const bool bPlaySound);

	UFUNCTION()
	void DelayedExplosionSpawnCustomSound(ERTS_ExplosionType ExplosionType, const FVector& SpawnLocation, USoundCue* CustomSound);

    /**
     * A map from ExplosionType to arrays of NiagaraSystem assets.
     * We pick one at random whenever we spawn an explosion.
     */
    TMap<ERTS_ExplosionType, TArray<UNiagaraSystem*>> M_ExplosionSystemsMap;
    TMap<ERTS_ExplosionType, TArray<USoundCue*>> M_ExplSoundsMap;

	UPROPERTY()
	USoundAttenuation* M_SoundAttenuation = nullptr;
	UPROPERTY()
	USoundConcurrency* M_SoundConcurrency = nullptr;

	USoundCue* PickRandomSoundForType(const ERTS_ExplosionType ExplosionType, const bool bPlaySound = false);
	UNiagaraSystem* PickRandomExplForType(const ERTS_ExplosionType ExplosionType);
};

