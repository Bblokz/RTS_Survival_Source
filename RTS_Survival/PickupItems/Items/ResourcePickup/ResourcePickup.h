// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/PickupItems/Items/ItemsMaster.h"
#include "RTS_Survival/UnitData/UnitCost.h"
#include "ResourcePickup.generated.h"

class UAnimatedTextWidgetPoolManager;
class UPlayerResourceManager;
class UPrimitiveComponent;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * @brief Blueprint-configurable payload and feedback used when a resource pickup is collected.
 */
USTRUCT(BlueprintType)
struct FResourcePickupSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup")
	FUnitCost ResourceGain;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup|FX")
	TObjectPtr<UNiagaraSystem> PickupNiagaraSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup|Audio")
	TObjectPtr<USoundBase> PickupSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup|Audio")
	TObjectPtr<USoundAttenuation> PickupSoundAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup|Audio")
	TObjectPtr<USoundConcurrency> PickupSoundConcurrency = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup|Vertical Text")
	FRTSVerticalAnimTextSettings VerticalTextSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup|Vertical Text")
	FVector VerticalTextOffset = FVector(0.f, 0.f, 140.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup|Vertical Text")
	float VerticalTextLineSpacing = 26.f;
};

/**
 * @brief Place this pickup in a level and assign pickup settings in BP to grant resources to the player on overlap.
 */
UCLASS()
class RTS_SURVIVAL_API AResourcePickup : public AItemsMaster
{
	GENERATED_BODY()

public:
	AResourcePickup(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Resource Pickup")
	FResourcePickupSettings PickupSettings;

private:
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> M_TriggerCircle = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;

	UPROPERTY()
	TWeakObjectPtr<UAnimatedTextWidgetPoolManager> M_AnimatedTextManager;

	UPROPERTY()
	bool bM_HasBeenCollected = false;

	void PostInitialize_SetupTriggerCircle();
	void BeginPlay_CachePlayerResourceManager();
	void BeginPlay_CacheAnimatedTextManager();

	void HandlePickupByPlayerUnit(AActor& OverlappingActor);
	bool GrantResourcesToPlayer() const;
	void PlayPickupSoundAtActorLocation() const;
	void SpawnPickupNiagaraAtActorLocation() const;
	void ShowVerticalResourceGainText() const;
	int32 GetConfiguredResourceAmount(const ERTSResourceType ResourceType) const;

	bool GetIsValidTriggerCircle() const;
	bool GetIsValidPlayerResourceManager() const;
	bool GetIsValidAnimatedTextManager() const;
};
