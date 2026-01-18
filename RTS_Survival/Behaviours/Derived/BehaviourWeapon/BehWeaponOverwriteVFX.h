// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"

#include "BehWeaponOverwriteVFX.generated.h"

class UWeaponState;

USTRUCT(BlueprintType)
struct FBehWeaponVfxOverrideSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideLaunchSound = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideLaunchSound"))
	USoundCue* LaunchSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideBounceSound = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideBounceSound"))
	USoundCue* BounceSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideLaunchEffectSettings = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideLaunchEffectSettings"))
	FLaunchEffectSettings LaunchEffectSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideSurfaceImpactEffects = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideSurfaceImpactEffects"))
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> SurfaceImpactEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideShellSpecificVfxOverwrites = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideShellSpecificVfxOverwrites"))
	FShellVfxOverwrites ShellSpecificVfxOverwrites;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideBounceEffect = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideBounceEffect"))
	UNiagaraSystem* BounceEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideLaunchScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideLaunchScale"))
	FVector LaunchScale = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideImpactScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideImpactScale"))
	FVector ImpactScale = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideBounceScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideBounceScale"))
	FVector BounceScale = FVector(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideImpactAttenuation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideImpactAttenuation"))
	USoundAttenuation* ImpactAttenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideImpactConcurrency = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideImpactConcurrency"))
	USoundConcurrency* ImpactConcurrency = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideLaunchAttenuation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideLaunchAttenuation"))
	USoundAttenuation* LaunchAttenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (InlineEditConditionToggle))
	bool bOverrideLaunchConcurrency = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour|VFX", meta = (EditCondition = "bOverrideLaunchConcurrency"))
	USoundConcurrency* LaunchConcurrency = nullptr;
};

USTRUCT()
struct FBehWeaponVfxOverrideFlags
{
	GENERATED_BODY()

	UPROPERTY()
	bool bOverrideLaunchSound = false;

	UPROPERTY()
	bool bOverrideBounceSound = false;

	UPROPERTY()
	bool bOverrideLaunchEffectSettings = false;

	UPROPERTY()
	bool bOverrideSurfaceImpactEffects = false;

	UPROPERTY()
	bool bOverrideShellSpecificVfxOverwrites = false;

	UPROPERTY()
	bool bOverrideBounceEffect = false;

	UPROPERTY()
	bool bOverrideLaunchScale = false;

	UPROPERTY()
	bool bOverrideImpactScale = false;

	UPROPERTY()
	bool bOverrideBounceScale = false;

	UPROPERTY()
	bool bOverrideImpactAttenuation = false;

	UPROPERTY()
	bool bOverrideImpactConcurrency = false;

	UPROPERTY()
	bool bOverrideLaunchAttenuation = false;

	UPROPERTY()
	bool bOverrideLaunchConcurrency = false;
};

USTRUCT()
struct FBehWeaponVfxOverrideRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FWeaponVFX PreviousWeaponVfx;

	UPROPERTY()
	FBehWeaponVfxOverrideFlags OverrideFlags;
};

/**
 * @brief Weapon behaviour that temporarily overwrites selected VFX settings on weapon states.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehWeaponOverwriteVFX : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UBehWeaponOverwriteVFX();

protected:
	virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState) override;
	virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState) override;

private:
	bool GetHasAnyVfxOverride() const;
	FBehWeaponVfxOverrideFlags BuildOverrideFlags() const;
	void ApplyVfxOverrides(UWeaponState* WeaponState);
	void RestoreVfxOverrides(UWeaponState* WeaponState);
	void ApplyOverridesToVfx(FWeaponVFX& WeaponVfx, const FBehWeaponVfxOverrideSettings& Overrides) const;
	void RestoreOverridesFromRecord(FWeaponVFX& WeaponVfx, const FBehWeaponVfxOverrideRecord& Record) const;
	const FBehWeaponVfxOverrideRecord* TryGetCachedVfxRecord(const TWeakObjectPtr<UWeaponState>& WeaponPtr) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour|VFX", meta = (AllowPrivateAccess = "true"))
	FBehWeaponVfxOverrideSettings M_VfxOverrides;

	UPROPERTY()
	TMap<TWeakObjectPtr<UWeaponState>, FBehWeaponVfxOverrideRecord> M_PreviousVfxPerWeapon;
};
