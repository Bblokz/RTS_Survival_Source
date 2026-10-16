// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "GrenadeComponent.generated.h"

class ASquadUnit;
class UNiagaraComponent;
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
 * @brief Lingering fire left behind by incendiary grenades such as the molotov.
 * The area deals damage every tick after the impact; a BurnDuration of 0 disables it.
 */
USTRUCT(BlueprintType)
struct FGrenadeBurnAreaSettings
{
	GENERATED_BODY()

	// Seconds the impact area keeps burning after the explosion; 0 means no burn area.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|Burn")
	float BurnDuration = 0.f;

	// Seconds between two burn damage ticks.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|Burn")
	float BurnTickInterval = 1.f;

	// Damage at the centre of the burn area per tick, before falloff and armor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|Burn")
	float BurnDamagePerTick = 0.f;

	// Radius of the burn area; 0 uses the grenade's AoeRange.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|Burn")
	float BurnRadius = 0.f;

	bool GetIsBurnAreaEnabled() const
	{
		return BurnDuration > 0.f && BurnTickInterval > 0.f && BurnDamagePerTick > 0.f;
	}
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

	// Damage type of the explosion and of the burn area (Fire for molotovs).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	ERTSDamageType DamageType = ERTSDamageType::Kinetic;

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
	TObjectPtr<UStaticMesh> GrenadeMesh = nullptr;

	// Sound effect played when the grenade explodes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|SoundHandling")
	TObjectPtr<USoundBase> ExplosionSound = nullptr;

	// Attenuation settings used for the grenade explosion sound.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|SoundHandling")
	TObjectPtr<USoundAttenuation> ExplosionSoundAttenuation = nullptr;

	// Concurrency settings used for the grenade explosion sound.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|SoundHandling")
	TObjectPtr<USoundConcurrency> ExplosionSoundConcurrency = nullptr;

	// Niagara effect spawned at the explosion location.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	TObjectPtr<UNiagaraSystem> ExplosionEffect = nullptr;

	// Scale applied to the grenade explosion Niagara effect.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	FVector ExplosionEffectScale = FVector::OneVector;

	// Optional effect attached to the grenade's "GrenadeEffect" mesh socket while held and in flight (molotov rag flame).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	TObjectPtr<UNiagaraSystem> AttachedGrenadeEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	FVector AttachedGrenadeEffectScale = FVector::OneVector;

	// Lingering fire after the impact; the explosion effect is stopped when the burn ends.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades|Burn")
	FGrenadeBurnAreaSettings BurnArea;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grenades")
	EGrenadeAbilityType GrenadeAbility = static_cast<EGrenadeAbilityType>(0);
	
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

	/**
	 * @brief Starts the optional effect on the grenade mesh socket, e.g. the burning rag of a molotov.
	 * @param AttachedEffect Effect to play; null stops any running attached effect.
	 * @param EffectScale Relative scale of the attached effect.
	 */
	void ActivateAttachedEffect(UNiagaraSystem* AttachedEffect, const FVector& EffectScale);
	void DeactivateAttachedEffect();

	// A grenade is free for a new throw when it is hidden and not still burning from an earlier impact.
	bool GetIsAvailableForThrow() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Lingering burn area of the last impact; runs on its own timer so it survives the grenade being hidden.
	struct FGrenadeBurnState
	{
		FVector BurnLocation = FVector::ZeroVector;
		int32 TicksRemaining = 0;
		float DamagePerTick = 0.f;
		float Radius = 0.f;
		float DamageFalloffExponent = 1.f;
		float FullArmorPen = 0.f;
		float ArmorPenFallOff = 0.f;
		float MaxArmorPen = 0.f;
		ERTSDamageType DamageType = ERTSDamageType::Fire;
		ETriggerOverlapLogic OverlapLogic = ETriggerOverlapLogic::OverlapEnemy;
		FTimerHandle BurnTickTimerHandle;
	};

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_StaticMeshComponent;

	// Plays the optional AttachedGrenadeEffect on the mesh socket; never auto-activates.
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_AttachedEffectComponent;

	// Impact effect of the running burn area, stopped when the burn ends.
	UPROPERTY()
	TWeakObjectPtr<UNiagaraComponent> M_BurnImpactEffectComponent;

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

	FGrenadeBurnState M_BurnState;

	bool GetIsValidStaticMeshComponent() const;
	bool GetIsValidAttachedEffectComponent() const;
	void CacheEffectData(const FGrenadeComponentSettings& DamageParams);
	void StartThrowTimer();
	void OnTickThrow();
	void OnExplode(const FGrenadeComponentSettings DamageParams, const int32 OwningPlayer);
	// Returns the spawned impact effect so a burn area can stop it when the fire ends.
	UNiagaraComponent* PlayExplosionFX(const FVector& ExplosionLocation) const;
	static ETriggerOverlapLogic GetOverlapLogicForOwningPlayer(const int32 OwningPlayer);

	/**
	 * @brief Keeps damaging the impact area for the burn duration (molotov fire); no-op when the burn is disabled.
	 * @param DamageParams Settings of the grenade that exploded.
	 * @param BurnLocation Centre of the burn area.
	 * @param OwningPlayer Player owning the grenade, decides who takes damage.
	 * @param ImpactEffect Impact effect to stop when the burn ends; may be null.
	 */
	void StartBurnArea(const FGrenadeComponentSettings& DamageParams, const FVector& BurnLocation,
	                   const int32 OwningPlayer, UNiagaraComponent* ImpactEffect);
	void OnBurnTick();
	void StopBurnArea();
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

	float GetGrenadeAoeRange() const;

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
	void CancelThrowSequence();
	void CancelThrowerState(FGrenadeThrowerState& ThrowerState);
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
