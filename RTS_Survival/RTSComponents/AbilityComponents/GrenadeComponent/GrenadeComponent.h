// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Player/Abilities.h"
#include "GrenadeComponent.generated.h"

class ASquadUnit;
class UNiagaraSystem;
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class UStaticMesh;
class UStaticMeshComponent;
class ASquadController;
class URTSComponent;

UENUM()
enum class EGrenadeAbilityState : uint8
{
	NotActive,
	MovingToThrowPosition,
	Throwing,
	ResupplyingGrenades
};

/**
 * @brief Settings for grenade throwing behaviour configured per squad.
 */
USTRUCT(BlueprintType)
struct FGrenadeComponentSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float ThrowRange = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float AoeRange = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float Damage = 40.f;

	// Rear armor threshold that still receives full AOE damage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float FullArmorPen = 0.f;

	// Higher values reduce damage more quickly as rear armor approaches MaxArmorPen.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float ArmorPenFallOff = 0.f;

	// Rear armor level that fully negates AOE damage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Mine")
	float MaxArmorPen = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float AoeDamageExponentReduction = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float Cooldown = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	int32 SquadUnitsThrowing = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	int32 GrenadesPerSquad = 1;

	// 1.67 seconds is the default duration of the throw animation montage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float GrenadeThrowMontageDuration = 1.67;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	UStaticMesh* GrenadeMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	USoundBase* ExplosionSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	UNiagaraSystem* ExplosionEffect = nullptr;
};

/**
 * @brief Simple grenade actor thrown by squads and reused through pooling.
 */
UCLASS()
class RTS_SURVIVAL_API AGrenadeActor : public AActor
{
	GENERATED_BODY()

public:
	AGrenadeActor();

	/**
	 * @brief Launches the grenade and triggers the explosion logic after travel.
	 * @param DamageParams Settings for damage falloff and visuals.
	 * @param StartLocation Location where the throw starts.
	 * @param EndLocation Target location to land at.
	 * @param OwningPlayer Player owning the grenade for team awareness.
	 */
	void ThrowAndExplode(const FGrenadeComponentSettings& DamageParams, const FVector& StartLocation,
	                     const FVector& EndLocation, const int32 OwningPlayer, UStaticMesh* OverrideMesh);

	void ResetGrenade();

	


protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_StaticMeshComponent;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_ExplosionEffect;

	UPROPERTY()
	TObjectPtr<USoundBase> M_ExplosionSound;

	float M_ThrowDuration;

	FVector M_StartLocation;
	FVector M_EndLocation;

	FTimerHandle M_ThrowTimerHandle;
	FTimerHandle M_ExplosionTimerHandle;

	void CacheEffectData(const FGrenadeComponentSettings& DamageParams);
	void StartThrowTimer();
	void OnTickThrow();
	void OnExplode(const FGrenadeComponentSettings DamageParams, const int32 OwningPlayer);
	void PlayExplosionFX(const FVector& ExplosionLocation) const;
	FVector CalculateArcLocation(const float Alpha) const;
};

/**
 * @brief Component handling grenade throwing ability for eligible squads.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UGrenadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrenadeComponent();

	/**
	 * @brief Initializes the component with its owning squad controller.
	 * @param SquadController Controller owning this component.
	 */
	void Init(ASquadController* SquadController);

	/**
	 * @brief Executes the throw grenade ability at the target location.
	 * @param TargetLocation World position for the grenade throw.
	 */
	void ExecuteThrowGrenade(const FVector& TargetLocation);

	void TerminateThrowGrenade();
	void CancelThrowGrenade();

	bool GetIsValidSquadController() const;

void OnSquadUnitDied(ASquadUnit* DeadUnit);

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup", meta=(AllowPrivateAccess="true"))
	FGrenadeComponentSettings M_Settings;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_SquadController;

	UPROPERTY()
	TWeakObjectPtr<URTSComponent> M_RTSComponent;

	UPROPERTY()
	TArray<TObjectPtr<AGrenadeActor>> M_GrenadePool;

	EGrenadeAbilityState M_AbilityState;
	int32 M_GrenadesRemaining;
	FTimerHandle M_CooldownTimerHandle;

	void Init_SetAbilityWhenSquadDataLoaded();
	void BeginPlay_CacheOwningPlayer();
	void BeginPlay_CreateGrenadePool();
	void ExecuteThrowGrenade_MoveOrThrow(const FVector& TargetLocation);
	void StartThrowSequence(const FVector& TargetLocation);
	void OnThrowFinished();
	void StartCooldown();
	void ResetCooldown();
	bool CanUseGrenades() const;
	AGrenadeActor* AcquireGrenade();
	AGrenadeActor* SpawnFallbackGrenade();
	void DestroyGrenadePool();
	void SetAbilityToResupplying();
	void SetAbilityToThrowGrenade();
	void SetAbilityToCancel();
	void ReportIllegalStateTransition(const FString& FromState, const FString& ToState) const;
	bool GetIsValidRTSComponent() const;

	int32 M_OwningPlayer;
};
