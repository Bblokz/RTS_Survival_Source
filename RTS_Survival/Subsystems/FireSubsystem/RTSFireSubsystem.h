#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTSFireSubsystem.generated.h"

class ARTSFireManager;

/**
 * @brief World subsystem that spawns and exposes the RTS fire manager.
 * Provides a lightweight API so gameplay code can request pooled fire effects.
 */
UCLASS()
class RTS_SURVIVAL_API URTSFireSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * @brief Request a pooled fire effect at a world location.
	 * @param FireType Fire pool to use for the effect.
	 * @param LifeTimeSeconds Seconds to keep active; <= 0 keeps it active until reclaimed.
	 * @param Location World location for the fire effect.
	 * @param Scale World scale for the fire effect.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	int32 SpawnFireAtLocation(ERTSFireType FireType,
	                         float LifeTimeSeconds,
	                         const FVector& Location,
	                         const FVector& Scale);

	/**
	 * @brief Request a pooled fire effect attached to an actor.
	 * @param AttachActor Actor to attach the fire to.
	 * @param FireType Fire pool to use for the effect.
	 * @param LifeTimeSeconds Seconds to keep active; <= 0 keeps it active until reclaimed.
	 * @param AttachOffset Offset from the actor root while attached.
	 * @param Scale World scale for the fire effect (ignores actor scale).
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	int32 SpawnFireAttached(AActor* AttachActor,
	                       ERTSFireType FireType,
	                       float LifeTimeSeconds,
	                       const FVector& AttachOffset,
	                       const FVector& Scale);

	/**
	 * @brief Stop and recycle a previously started pooled fire effect.
	 * @param FireHandle Handle returned by SpawnFireAtLocation/SpawnFireAttached.
	 * @return True when a matching active fire instance was found and stopped.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|Fire")
	bool StopFireByHandle(int32 FireHandle);

private:
	UPROPERTY()
	TObjectPtr<ARTSFireManager> M_FireManager = nullptr;

	bool GetIsValidFireManager() const;
};
