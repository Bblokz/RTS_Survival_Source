// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "FieldConstruction.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Navigation/RTSNavAgents/ERTSNavAgents.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"

#include "FieldMine.generated.h"

class URTSComponent;
class USphereComponent;
class UAnimatedTextWorldSubsystem;
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class IRTSNavAgentInterface;

/**
 * @brief Field construction variant representing a mine that arms and detonates on enemy overlap.
 */
USTRUCT(BlueprintType)
struct FMineSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float AverageDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float AOEDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float AOERange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float AOEFallOffScaler = 0.f;

	// Rear armor threshold that still receives full AOE damage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float FullArmorPen = 0.f;

	// Higher values reduce damage more quickly as rear armor approaches MaxArmorPen.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float ArmorPenFallOff = 0.f;

	// Rear armor level that fully negates AOE damage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float MaxArmorPen = 0.f;

	// Subtraction from the mine's detonation timer when triggered.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float SubtractedTime = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	TArray<ERTSNavAgents> TriggeringNavAgents;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	TObjectPtr<USoundBase> TriggerSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	TObjectPtr<USoundAttenuation> SoundAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	TObjectPtr<USoundConcurrency> SoundConcurrency = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	FCollapseFX CollapseFX;
};

/**
 * @brief Mine actor derived from field construction; scans for enemy nav agents and detonates with AOE damage.
 */
UCLASS()
class RTS_SURVIVAL_API AFieldMine : public AFieldConstruction
{
	GENERATED_BODY()

public:
	AFieldMine(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	// Blueprint configuration for the mine behaviour.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	FMineSettings MineSettings;

private:
	UPROPERTY()
	TObjectPtr<URTSComponent> M_RTSComponent = nullptr;

	UPROPERTY()
	TObjectPtr<USphereComponent> M_TriggerSphere = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UAnimatedTextWorldSubsystem> M_AnimatedTextSubsystem;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_TargetActor;

	FTimerHandle M_DetonationTimerHandle;
	float M_CachedDamage = 0.f;
	bool bM_HasTriggered = false;

	void BeginPlay_DisableMineMeshCollision();
	void BeginPlay_CacheAnimatedTextSubsystem();
	void BeginPlay_SetupTriggerSphere();

	/**
	 * @brief Checks overlap candidates and arms the mine for valid nav agent targets.
	 * @param OverlappedComponent Mine trigger component.
	 * @param OtherActor Actor entering the trigger.
	 * @param OtherComp Component on the actor that overlapped.
	 * @param OtherBodyIndex Unused body index from the overlap event.
	 * @param bFromSweep Whether the overlap was from a sweep.
	 * @param SweepResult Sweep hit result (unused).
	 */
	UFUNCTION()
	void OnMineTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	                          const FHitResult& SweepResult);

	void TriggerMineForActor(AActor& TriggeringActor);
	void HandleMineDetonation();

	bool DoesNavAgentTriggerMine(const AActor& OtherActor) const;
	bool ShouldTriggerForNavAgent(const IRTSNavAgentInterface& NavAgentInterface) const;
	ETriggerOverlapLogic GetOverlapLogicForMineOwner() const;

	float PlayTriggerSound() const;
	float CalculateMineDamage() const;

	void ApplyDirectDamage(AActor& TargetActor);
	void ApplyAoeDamage(const FVector& Epicenter);
	void ShowMineDamageText(const FVector& TextLocation) const;
	void SpawnCollapseFx() const;

	TArray<TWeakObjectPtr<AActor>> BuildActorsToIgnore() const;

	bool GetIsValidRTSComponent() const;
	bool GetIsValidTriggerSphere() const;
	bool GetIsValidAnimatedTextSubsystem() const;


	UPROPERTY()
	TWeakObjectPtr<AActor> M_DamageCauserController;
};
