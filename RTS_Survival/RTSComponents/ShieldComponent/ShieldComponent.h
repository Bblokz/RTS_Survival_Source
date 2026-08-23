// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShieldTypes.h"

#include "ShieldComponent.generated.h"

class UHealthComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class UWidgetComponent;
class UW_ShieldBar;

USTRUCT()
struct FResolvedShieldActivationSettings
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShieldMaterial = nullptr;

	float Radius = 0.0f;
	float TimedLife = 0.0f;
	FVector ShieldOffset = FVector::ZeroVector;
	EShieldBehaviour ShieldBehaviour = EShieldBehaviour::KillOnDestroyed;
};

/**
 * @brief Owns a tank's shield capacity, runtime sphere, recharge lifecycle, and independent shield bar.
 * @note Configure ShieldSettings on the tank Blueprint component defaults.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UShieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShieldComponent();

	/**
	 * @brief Applies one activation's overrides without mutating the component-authored shield defaults.
	 * @param ActivationOverrides Explicit optional values used for this activation and recharge cycle.
	 * @return True only when an inactive shield was successfully activated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shield", meta = (AutoCreateRefTerm = "ActivationOverrides"))
	virtual bool ActivateShield(const FShieldActivationOverrides& ActivationOverrides);

	UFUNCTION(BlueprintPure, Category = "Shield")
	bool CanActivateShield() const;

	/**
	 * @brief Resolves whether this source stops at the shield while keeping pass-through damage unchanged.
	 * @param DamageRequest Source data needed for shield cost and special shell behavior.
	 * @return Whether the caller stops, continues, or found no shield interaction.
	 */
	EShieldDamageResult ApplyShieldDamage(const FShieldDamageRequest& DamageRequest);

	UFUNCTION(BlueprintPure, Category = "Shield")
	float GetCurrentShields() const { return M_CurrentShields; }

	UFUNCTION(BlueprintPure, Category = "Shield")
	float GetMaxShields() const { return M_MaxShields; }

	UFUNCTION(BlueprintPure, Category = "Shield")
	float GetShieldPercentage() const;

	UFUNCTION(BlueprintPure, Category = "Shield")
	EShieldState GetShieldState() const { return M_ShieldState; }

	UFUNCTION(BlueprintPure, Category = "Shield")
	bool GetIsShieldActive() const { return M_ShieldState == EShieldState::Active; }

	UStaticMeshComponent* GetShieldMeshComponent() const { return M_ShieldMeshComponent.Get(); }

	FShieldComponentSettings GetShieldSettings() const { return ShieldSettings; }

protected:
	virtual void BeginPlay() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	/** Allows derived shield systems to skip or replace owner ability registration. */
	virtual void BeginPlay_AddAbility();

	/** Allows derived components to broaden the base tank-only owner restriction. */
	virtual bool GetIsSupportedShieldOwner() const;

	/** Applies the meaning of authored radius to the runtime mesh. */
	virtual void ApplyShieldRadius(UStaticMeshComponent* ShieldMesh, float Radius) const;

	virtual void OnShieldActive();
	virtual void OnShieldDamaged(float AppliedDamage, const FShieldDamageRequest& DamageRequest);
	virtual void OnShieldDestroyed();
	virtual void OnShieldDeactivated(EShieldDeactivationReason Reason);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield")
	FShieldComponentSettings ShieldSettings;

private:
	bool ResolveActivationSettings(const FShieldActivationOverrides& ActivationOverrides);
	bool EnsureShieldMesh();
	void EnsureShieldWidget();
	void ApplyResolvedSettingsToShieldMesh() const;
	void ActivateResolvedShield();
	void DeactivateRuntimeShield();
	void HandleShieldDepleted();
	void HandleTimedLifeExpired();
	void StartTimedLifeIfNeeded();
	void StartRecharge();
	void HandleRechargeComplete();
	void ClearShieldTimers();
	void DestroyRuntimeComponents();
	void AddAbilityToOwner();
	void RemoveAbilityFromOwner() const;
	void ClearOwnerShieldCache() const;
	void BindHealthBarVisibility();
	void UnbindHealthBarVisibility();
	void HandleHealthBarVisibilityChanged(ESlateVisibility HealthBarVisibility);
	void ApplyShieldBarVisibility();
	void UpdateShieldBarValues() const;
	int32 GetOwningPlayer() const;
	float CalculateAdjustedShieldDamage(const FShieldDamageRequest& DamageRequest) const;
	static bool GetIsPassThroughShell(EWeaponShellType ShellType);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ShieldMeshComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UWidgetComponent> M_ShieldWidgetComponent = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UW_ShieldBar> M_ShieldWidget;

	UPROPERTY()
	TWeakObjectPtr<UHealthComponent> M_HealthComponent;

	UPROPERTY()
	float M_CurrentShields = 0.0f;

	UPROPERTY()
	float M_MaxShields = 0.0f;

	UPROPERTY()
	FResolvedShieldActivationSettings M_ResolvedSettings;

	UPROPERTY()
	EShieldState M_ShieldState = EShieldState::Inactive;

	ESlateVisibility M_HealthBarVisibility = ESlateVisibility::Hidden;
	FDelegateHandle M_HealthVisibilityDelegateHandle;
	FTimerHandle M_TimedLifeTimerHandle;
	FTimerHandle M_RechargeTimerHandle;
};
