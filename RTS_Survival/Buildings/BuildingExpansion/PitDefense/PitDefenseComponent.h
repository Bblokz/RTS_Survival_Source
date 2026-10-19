// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Collapse/VerticalCollapse/RTSVerticalCollapseSettings.h"

#include "PitDefenseComponent.generated.h"

class ABuildingExpansion;
class UAnimMontage;
class UBoxComponent;
class UGunPitCrewComponent;
class UMeshComponent;
class UNiagaraSystem;
class USoundAttenuation;
class USoundConcurrency;
class USoundCue;

/** @brief Bounds rules for the single invisible collider that receives weapon hits for an entire pit. */
USTRUCT(BlueprintType)
struct FPitDefenseCollisionSettings
{
	GENERATED_BODY()

	// Extra room makes low walls, seated crew, and elevated weapon parts easy to hit without complex collision.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Collision")
	FVector BoundsPadding = FVector(100.f, 100.f, 125.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Collision")
	FVector MinimumExtent = FVector(250.f, 250.f, 175.f);

	// Caps the hit target inside the pit while its vertical clearance keeps it separate from the outer wall collision.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Collision")
	FVector2D MaximumHorizontalExtent = FVector2D(250.f, 250.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Collision", meta = (ClampMin = "0"))
	float OuterCollisionClearance = 5.f;
};

/** @brief Shared assets and timing used to turn every pit death into the same readable battlefield event. */
USTRUCT(BlueprintType)
struct FPitDefenseDestructionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction")
	TSoftObjectPtr<UNiagaraSystem> SandbagExplosionSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction")
	TSoftObjectPtr<USoundCue> SandbagExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction")
	TSoftObjectPtr<USoundAttenuation> SandbagExplosionAttenuation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction")
	TSoftObjectPtr<USoundConcurrency> SandbagExplosionConcurrency;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction")
	TSoftObjectPtr<UNiagaraSystem> BloodEffectSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction")
	TArray<TSoftObjectPtr<UAnimMontage>> CrewDeathMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction", meta = (ClampMin = "0"))
	int32 SmallExplosionCount = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction", meta = (ClampMin = "0"))
	int32 ScorchDecalCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction")
	FVector2D ScorchDecalSize = FVector2D(225.f, 400.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction", meta = (ClampMin = "0"))
	float ScorchDecalLifeTime = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pit Defense|Destruction")
	FRTSVerticalCollapseSettings VerticalCollapseSettings;
};

/**
 * @brief Added to pit building-expansion Blueprints to provide forgiving weapon collision and a complete death flow.
 * It derives its bounds and crew from the pit's child turret, so every pit variant uses the same safe behavior.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPitDefenseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPitDefenseComponent();

	/** Starts crew deaths, effects, decals, and a destroy-on-finish vertical collapse exactly once. */
	UFUNCTION(BlueprintCallable, Category = "Pit Defense")
	void HandlePitDeath();

	/** Rebuilds the collider after construction or owner setup changed the pit's visible bounds or allegiance. */
	void RefreshWeaponHitCollider();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Pit Defense", meta = (AllowPrivateAccess = "true"))
	FPitDefenseCollisionSettings M_CollisionSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Pit Defense", meta = (AllowPrivateAccess = "true"))
	FPitDefenseDestructionSettings M_DestructionSettings;

	UPROPERTY()
	TObjectPtr<UBoxComponent> M_WeaponHitCollider;

	bool bM_DeathStarted = false;

	ABuildingExpansion* GetPitOwner() const;
	bool GetIsValidWeaponHitCollider() const;
	void CreateWeaponHitCollider();
	void ConfigureWeaponHitCollision(const ABuildingExpansion& PitOwner) const;
	void DisableCrewAndWeaponMeshCollision() const;
	void DisableWeaponHitCollider() const;

	void GatherCrewAndWeaponMeshComponents(TArray<UMeshComponent*>& OutMeshComponents) const;
	void GatherPitCrewComponents(TArray<UGunPitCrewComponent*>& OutCrewComponents) const;
	void GatherPitActors(TArray<AActor*>& OutPitActors) const;

	void PlayCrewDeathEffects() const;
	void SpawnSandbagExplosion() const;
	void SpawnScatteredExplosions() const;
	void SpawnScorchDecals() const;
	void StartVerticalDestruction() const;
};
