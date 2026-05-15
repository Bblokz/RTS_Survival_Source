// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StealthTechComponent.generated.h"

class UMaterialInterface;
class UMeshComponent;

/**
 * @brief Designer-facing values that define how one stealth activation behaves.
 */
USTRUCT(BlueprintType)
struct FStealthTechComponentSettings
{
	GENERATED_BODY()

	/** Material applied to every non-excluded owner mesh slot while stealth is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth")
	TObjectPtr<UMaterialInterface> M_StealthMaterial = nullptr;

	/** Permanent stealth skips the duration timer and remains active until manually disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth")
	bool bM_IsPermanent = false;

	/** Time in seconds before non-permanent stealth restores materials and starts cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth", meta=(ClampMin="0.0", Units="s"))
	float M_StealthTime = 0.0f;

	/** Time in seconds after timed stealth expiry before stealth can be enabled again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth", meta=(ClampMin="0.0", Units="s"))
	float M_CooldownTime = 0.0f;

	/** Mesh components with any of these tags are ignored so visual helpers can stay visible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth")
	TArray<FName> M_ExcludedMeshTags;
};

/**
 * @brief Runtime cache used to restore one mesh to its exact pre-stealth material state.
 */
USTRUCT()
struct FStealthTechComponentMeshMaterials
{
	GENERATED_BODY()

	/** Weak because the owner controls mesh component lifetime and may destroy them during stealth. */
	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> M_MeshComponent;

	/** Materials captured immediately before stealth so runtime material swaps restore correctly. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> M_OriginalMaterials;

	/** Slot count at capture time, used to report unexpected mesh material layout changes. */
	UPROPERTY()
	int32 M_CachedMaterialSlotCount = 0;
};

/**
 * @brief Internal state that prevents repeated activation while active or cooling down.
 */
UENUM()
enum class EStealthTechComponentState : uint8
{
	/** Activation can be attempted. */
	Ready,
	/** Stealth material is currently applied. */
	Active,
	/** Timed stealth expired and reactivation is temporarily blocked. */
	Cooldown,
};

/**
 * @brief Add to an actor that should swap owner mesh materials while a stealth ability is active.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UStealthTechComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** @brief Sets the component up without ticking because stealth only changes on commands and timers. */
	UStealthTechComponent();

	/**
	 * @brief Attempts to activate stealth and cache the current material state for safe restore.
	 * @return True when stealth was applied; false when blocked by cooldown, invalid settings, or no meshes.
	 */
	UFUNCTION(BlueprintCallable)
	bool EnableStealth();

	/**
	 * @brief Manually cancels active stealth without starting cooldown so player cancellation is not punished.
	 * @return True when active stealth was restored; false when stealth was not active.
	 */
	UFUNCTION(BlueprintCallable)
	bool DisableStealth();

	/** @return True when the component is ready and has enough settings data to attempt activation. */
	UFUNCTION(BlueprintPure)
	bool GetCanEnableStealth() const;

	/** @return True while owner mesh materials are currently replaced with the stealth material. */
	UFUNCTION(BlueprintPure)
	bool GetIsStealthActive() const { return M_StealthState == EStealthTechComponentState::Active; }

	/** @return True while timed stealth has expired and the ability is waiting for cooldown. */
	UFUNCTION(BlueprintPure)
	bool GetIsCooldownActive() const { return M_StealthState == EStealthTechComponentState::Cooldown; }

protected:
	/** @brief Restores materials before destruction so actors do not keep stealth visuals after end play. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Settings grouped so designers can tune stealth without touching runtime cache data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth")
	FStealthTechComponentSettings M_StealthSettings;

private:
	/** @brief Rebuilds the runtime cache from the current owner meshes just before stealth is applied. */
	void CacheOwnerMeshMaterials();

	/**
	 * @brief Captures current materials for one mesh so any runtime material changes can be restored.
	 * @param MeshComponent Mesh owned by the owner actor and not excluded by tags.
	 */
	void CacheMeshMaterials(UMeshComponent* MeshComponent);

	/** @brief Replaces every cached mesh material slot with the configured stealth material. */
	void ApplyStealthMaterialToMeshes();

	/**
	 * @brief Applies the stealth material to one cached mesh after validating the weak pointer.
	 * @param MeshMaterials Cached mesh entry created during this activation.
	 */
	void ApplyStealthMaterialToMesh(const FStealthTechComponentMeshMaterials& MeshMaterials);

	/** @brief Restores cached materials for every valid cached mesh and then clears stale cache data. */
	void RestoreOriginalMaterialsAndClearCache();

	/**
	 * @brief Restores one mesh and reports slot count changes that can prevent a perfect restore.
	 * @param MeshMaterials Cached mesh entry created before stealth was applied.
	 */
	void RestoreOriginalMaterialsForMesh(
		const FStealthTechComponentMeshMaterials& MeshMaterials) const;

	/** @return True when permanent stealth or a valid duration timer allows activation to continue. */
	bool StartStealthDurationTimer();

	/** @brief Starts cooldown after timed expiry so stealth cannot be immediately reactivated. */
	void StartCooldownTimer();

	/** @brief Clears owned timer handles before restore/end play to avoid callbacks after the component ends. */
	void ClearStealthTimers();

	/** @brief Called by the duration timer to restore materials and start cooldown for timed stealth. */
	UFUNCTION()
	void OnStealthDurationElapsed();

	/** @brief Called by the cooldown timer to allow another stealth activation. */
	UFUNCTION()
	void OnCooldownElapsed();

	/** @return True when settings are valid enough to execute and log errors if not. */
	bool GetAreStealthSettingsValid() const;

	/** @return True when the stealth material is set and safe to apply to mesh slots. */
	bool GetIsValidStealthMaterial() const;

	/** @return True when the owner is valid and can be searched for mesh components. */
	bool GetIsOwnerValidForMeshCaching() const;

	/** @return True when the current activation cached at least one valid mesh with material slots. */
	bool GetHasCachedMeshMaterials() const;

	/** @return True when the world exists so timer operations can be safely performed. */
	bool GetIsValidWorldForTimers(const FString& FunctionName) const;

	/** @return True when a mesh tag marks it as excluded from stealth material swaps. */
	bool GetShouldSkipMeshComponent(const UMeshComponent* MeshComponent) const;

	/** Runtime cache from the current activation; cleared after restore to avoid stale material data. */
	UPROPERTY()
	TArray<FStealthTechComponentMeshMaterials> M_MeshMaterials;

	/** Timer handle for non-permanent stealth expiry. */
	FTimerHandle M_StealthDurationTimerHandle;

	/** Timer handle that blocks reactivation after non-permanent stealth naturally expires. */
	FTimerHandle M_CooldownTimerHandle;

	/** Tracks whether calls should activate, restore, or be ignored because cooldown is running. */
	EStealthTechComponentState M_StealthState = EStealthTechComponentState::Ready;
};
