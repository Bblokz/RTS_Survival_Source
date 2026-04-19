// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "ExperienceLevelBehaviour.generated.h"

class UHealthComponent;
class UFowComp;

USTRUCT()
struct FExperienceLevelBehaviourOwnerStats
{
	GENERATED_BODY()

	UPROPERTY()
	float M_BaseMaxHealth = 0.f;

	UPROPERTY()
	float M_BaseVisionRadius = 0.f;

	UPROPERTY()
	float M_AppliedHealthMultiplier = 1.f;

	UPROPERTY()
	float M_AppliedVisionRadiusMultiplier = 1.f;

	bool bM_HasCachedBaseMaxHealth = false;
	bool bM_HasCachedBaseVisionRadius = false;
};

/**
 * @brief Behaviour base for experience-level buffs that can adjust weapon, health, and vision multipliers.
 * Caches owner component references so derived classes can safely add type-specific multiplier logic.
 */
UCLASS()
class RTS_SURVIVAL_API UExperienceLevelBehaviour : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UExperienceLevelBehaviour();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;
	virtual void OnStack(UBehaviour* StackedBehaviour) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|Experience Multipliers")
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|Experience Multipliers")
	float VisionRadiusMultiplier = 1.0f;

	bool GetIsValidHealthComponent() const;
	bool GetIsValidFowComponent() const;

private:
	void CacheOwnerComponents();
	void ApplyOwnerComponentMultipliers();
	void ApplyHealthMultiplier();
	void ApplyVisionRadiusMultiplier();
	void RestoreOwnerComponentMultipliers();
	void RestoreHealthMultiplier();
	void RestoreVisionRadiusMultiplier();
	void ClearCachedOwnerComponents();

	UPROPERTY()
	TWeakObjectPtr<UHealthComponent> M_HealthComponent;

	UPROPERTY()
	TWeakObjectPtr<UFowComp> M_FowComponent;

	UPROPERTY()
	FExperienceLevelBehaviourOwnerStats M_OwnerStats;
};
