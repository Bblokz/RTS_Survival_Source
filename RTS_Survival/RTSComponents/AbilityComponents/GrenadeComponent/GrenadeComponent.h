// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
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

	// Max distance at which squad units can throw grenades at the target.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float ThrowRange = 400.f;

	// Radius of the grenade explosion damage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float AoeRange = 300.f;

	// Base damage applied at the center of the explosion.
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

	// Damage falloff exponent applied across the AOE radius.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float AoeDamageExponentReduction = 1.f;

	// Cooldown between grenade throws for the squad.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float Cooldown = 10.f;

	// Number of squad units selected to throw grenades.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	int32 SquadUnitsThrowing = 1;

	// Amount of grenades each selected squad unit throws.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	int32 GrenadesPerSquad = 1;

	// 1.67 seconds is the default duration of the throw animation montage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	float GrenadeThrowMontageDuration = 1.67;

	// Mesh override used by the grenade actor when thrown.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	UStaticMesh* GrenadeMesh = nullptr;

	// Sound effect played when the grenade explodes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|SoundHandling")
	USoundBase* ExplosionSound = nullptr;

	// Attenuation settings used for the grenade explosion sound.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|SoundHandling")
	TObjectPtr<USoundAttenuation> ExplosionSoundAttenuation = nullptr;

	// Concurrency settings used for the grenade explosion sound.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|SoundHandling")
	TObjectPtr<USoundConcurrency> ExplosionSoundConcurrency = nullptr;

	// Niagara effect spawned at the explosion location.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	UNiagaraSystem* ExplosionEffect = nullptr;

	// Scale applied to the grenade explosion Niagara effect.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	FVector ExplosionEffectScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	EGrenadeAbilityType GrenadeAbility;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	int32 PreferredIndex = -1;
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

	void SetGrenadeMesh(UStaticMesh* OverrideMesh);
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

	UPROPERTY()
	TObjectPtr<USoundAttenuation> M_ExplosionSoundAttenuation;

	UPROPERTY()
	TObjectPtr<USoundConcurrency> M_ExplosionSoundConcurrency;

	FVector M_ExplosionEffectScale;
	float M_ThrowDuration;

	FVector M_StartLocation;
	FVector M_EndLocation;

	FTimerHandle M_ThrowTimerHandle;
	FTimerHandle M_ExplosionTimerHandle;

	bool GetIsValidStaticMeshComponent() const;
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

	EGrenadeAbilityType GetGrenadeAbilityType() const;

	bool GetIsValidSquadController() const;

	void OnSquadUnitArrivedAtThrowLocation(ASquadUnit* SquadUnit);
	void OnSquadUnitDied(ASquadUnit* DeadUnit);

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

private:
	struct FGrenadeThrowerState
	{
		TWeakObjectPtr<ASquadUnit> M_SquadUnit = nullptr;
		TWeakObjectPtr<AGrenadeActor> M_AttachedGrenade = nullptr;
		FVector M_AttachedGrenadeScale = FVector::OneVector;
		int32 M_GrenadesRemaining = 0;
		FTimerHandle M_ThrowTimerHandle;
	};

	struct FGrenadeThrowSequenceState
	{
		FVector TargetLocation = FVector::ZeroVector;
		FVector ThrowLocation = FVector::ZeroVector;
		bool bM_IsWaitingForArrival = false;
		bool bM_HasStarted = false;
	};

	FUnitAbilityEntry CreateGrenadeAbilityEntry(const EAbilityID AbilityId) const;

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

	// Tracks the active throw sequence and requested throw locations.
	FGrenadeThrowSequenceState M_ThrowSequenceState;

	// Tracks which squad units are currently throwing grenades and how many remain.
	TArray<FGrenadeThrowerState> M_ThrowerStates;

	void Init_SetAbilityWhenSquadDataLoaded();
	void BeginPlay_CacheOwningPlayer();
	void BeginPlay_CreateGrenadePool();
	void ExecuteThrowGrenade_MoveOrThrow(const FVector& TargetLocation);
	FVector CalculateThrowMoveLocation(const FVector& TargetLocation) const;
	void CacheThrowSequenceLocations(const FVector& TargetLocation);
	void StartThrowSequence();
	void BuildThrowerStates();
	void StartThrowForThrower(FGrenadeThrowerState& ThrowerState);
	void DisableThrowerWeapon(const FGrenadeThrowerState& ThrowerState) const;
	void EnableThrowerWeapon(const FGrenadeThrowerState& ThrowerState) const;
	void StartThrowTimer(FGrenadeThrowerState& ThrowerState);
	void OnThrowMontageFinished(TWeakObjectPtr<ASquadUnit> SquadUnit);
	void ClearThrowTimer(FGrenadeThrowerState& ThrowerState);
	void ResetThrowerState(FGrenadeThrowerState& ThrowerState);
	void AttachGrenadeToThrower(FGrenadeThrowerState& ThrowerState, AGrenadeActor* Grenade);
	void DetachGrenadeFromThrower(FGrenadeThrowerState& ThrowerState);
	FVector GetThrowStartLocation(const FGrenadeThrowerState& ThrowerState) const;
	FGrenadeThrowerState* FindThrowerState(const TWeakObjectPtr<ASquadUnit>& SquadUnit);
	bool HasPendingThrows() const;
	void ResetThrowSequenceState();
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
