// Copyright (c) Bas Blokzijl. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "WeaponSystems.h"
#include "Engine/DamageEvents.h"
#include "NiagaraSystem.h"
#include "RTSDamageTypes/RTSDamageTypes.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/Weapons/LaserWeapon/LaserWeaponData.h"
#include "RTS_Survival/Weapons/Projectile/ProjectileVfxSettings/ProjectileVfxSettings.h"
#include "Sound/SoundCue.h"
#include "WeaponShellType/WeaponShellType.h"

#include "WeaponData.generated.h"


enum class EProjectileNiagaraSystem : uint8;
class UArmorCalculation;
class ASmallArmsProjectileManager;
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class AArchProjectile;
class AShellCase;
class RTS_SURVIVAL_API AProjectile;
class RTS_SURVIVAL_API ACPPController;
class URTSHidableInstancedStaticMeshComponent;
enum class EWeaponSystemType : uint8;
enum class EWeaponName : uint8;
enum class EWeaponFireMode : uint8;
class RTS_SURVIVAL_API IWeaponOwner;

/**
 * @brief Behaviour-driven attribute adjustments applied to weapon data.
 */
USTRUCT(BlueprintType)
struct FBehaviourWeaponAttributes
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Damage = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Range = 0.0f;

	// Gun diameter in mm.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 WeaponCalibre = 0;

	// Base cooldown time between individual shots, measured in seconds.
	UPROPERTY(BlueprintReadOnly)
	float BaseCooldown = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ReloadSpeed = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Accuracy = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MagSize = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ArmorPenetration = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ArmorPenetrationMaxRange = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float TnTGrams = 0.0f;

	// Range of the Area of Effect (AOE) explosion in centimeters, applicable for AOE-enabled projectiles.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ShrapnelRange = 0.0f;

	// Damage dealt by each projectile in an AOE attack, relevant for AOE-enabled projectiles.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ShrapnelDamage = 0.0f;

	// Number of shrapnel particles generated in an AOE attack, applicable for AOE-enabled projectiles.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ShrapnelParticles = 0;

	// Factor used for armor penetration calculations before damage application in AOE attacks.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ShrapnelPen = 0.0f;

	// The angle cone of the flame weapon in degrees.
	UPROPERTY(BlueprintReadOnly)
	float FlameAngle = 0.f;

	// Used by flame and laser weapons for damage per burst (one full iteration).
	UPROPERTY(BlueprintReadOnly)
	int32 DamageTicks = 0;
};


// Used by the health component to notify when the health bar UI needs to change the shell type icon.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShellTypeChanged, EWeaponShellType);
// To keep track of how many bullets are left in the mag.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMagConsumed, int32);

/** @return read-only map of shell→color, initialized on first use. */
RTS_SURVIVAL_API const TMap<EWeaponShellType, FLinearColor>& GLOBAL_GetShellColorMap();

/** @return color for a shell, or Transparent if not configured. */
RTS_SURVIVAL_API FLinearColor GLOBAL_GetShellColor(EWeaponShellType ShellType);

/** Copy current designer settings into the global cache (e.g., call on game start or after settings tweak). */
RTS_SURVIVAL_API void GLOBAL_RefreshShellColorsFromSettings();


UENUM(BlueprintType, Blueprintable)
enum class EWeaponLaunchSettingsType : uint8
{
	// Use the regular launch vfx as provided without setting parameters.
	None,
	// Color the launch vfx based on the shell type.
	ColorByShellType,
	// Scale the lifetime and size directly based on struct settings.
	DirectLifeTimeSizeScale
};

/**
 * @brief contains the primitve data of a weapon like range, damage and reload speed.
 */
USTRUCT()
struct FWeaponData
{
	GENERATED_BODY()

	UPROPERTY()
	EWeaponName WeaponName = EWeaponName::DEFAULT_WEAPON;

	UPROPERTY()
	ERTSDamageType DamageType = ERTSDamageType::None;

	UPROPERTY()
	EWeaponShellType ShellType = EWeaponShellType::Shell_AP;

	UPROPERTY()
	TArray<EWeaponShellType> ShellTypes = {EWeaponShellType::Shell_AP};

	// Gun diameter in mm.
	UPROPERTY()
	float WeaponCalibre = 0.f;

	// Explosive payload tnt equivalent in grams.
	UPROPERTY()
	float TNTExplosiveGrams = 0.f;

	// Base damage dealt by one bullet.
	UPROPERTY()
	float BaseDamage = 0.0f;

	// Percentage variance used to create fluctuation in damage output.
	UPROPERTY()
	int32 DamageFlux = 0;

	// Effective range of the weapon in centimeters.
	UPROPERTY()
	float Range = 0.0f;

	// Factor used for armor penetration calculations before damage application.
	UPROPERTY()
	float ArmorPen = 0.0f;

	// The armor pen at max range of the projectile.
	UPROPERTY()
	float ArmorPenMaxRange = 0.0f;

	// Maximum number of bullets in the magazine before requiring a reload.
	UPROPERTY()
	int32 MagCapacity = 0;

	// Time taken to reload the entire magazine, measured in seconds.
	UPROPERTY()
	float ReloadSpeed = 0.0f;

	// Base cooldown time between individual shots, measured in seconds.
	UPROPERTY()
	float BaseCooldown = 0.0f;

	// Percentage variance used to create fluctuation in the cooldown and reload time.
	UPROPERTY()
	int32 CooldownFlux = 0;

	// Accuracy rating of the weapon, ranging from 1 to 100 (100 indicating perfect accuracy).
	UPROPERTY()
	int32 Accuracy = 0;

	// Range of the Area of Effect (AOE) explosion in centimeters, applicable for AOE-enabled projectiles.
	UPROPERTY()
	float ShrapnelRange = 0.0f;

	// Damage dealt by each projectile in an AOE attack, relevant for AOE-enabled projectiles.
	UPROPERTY()
	float ShrapnelDamage = 0.0f;

	// Number of shrapnel particles generated in an AOE attack, applicable for AOE-enabled projectiles.
	UPROPERTY()
	uint32 ShrapnelParticles = 0;

	// Factor used for armor penetration calculations before damage application in AOE attacks.
	UPROPERTY()
	float ShrapnelPen = 0.0f;

	UPROPERTY()
	float ProjectileMovementSpeed = 6000;

	UPROPERTY()
	FBehaviourWeaponAttributes BehaviourAttributes;

	void CopyWeaponDataValues(const FWeaponData* WeaponData);
};

USTRUCT(BlueprintType)
struct FLaunchEffectSettings
{
	GENERATED_BODY()

	bool HasLaunchSettings() const;

	// Whether to use custom launch effect settings.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWeaponLaunchSettingsType UseLaunchSettings = EWeaponLaunchSettingsType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UNiagaraSystem* LaunchEffect = nullptr;

	// Extra multiplier used together with the weapon calibre to determine the lifetime of the launch effect.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float LifeTimeMlt = 1.f;

	// Extra multiplier to scale the light intensity of the launch effect.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float LightIntensityMlt = 1.f;

	// Whether to use launch smoke on the muzzle effect
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUseMuzzleLaunchSmoke = true;

	// Whether to use ground smoke on the launch vfx.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUseGroundLaunchSmoke = false;

	// How much the z offset is for  hte ground launchsmoke.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float GroundZOffset = -300;

	// To directly scale the size; used for small arms launch effect settings.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SizeMlt = 1.f;
};

// Provides different vfx for different shells
USTRUCT(BlueprintType)
struct FShellVfxOverwrites
{
	GENERATED_BODY()

	// The Impact data used by HE and HEAT shells for impacts, empty fields will use the
	// AP impacts instead.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> HeAndHeat_ImpactOverwriteData;

	// The impact data used by HE And HEAT shells when bouncing.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FRTSSurfaceImpactData HeAndHeat_BounceOverWriteData;

	void HandleWeaponShellChange(const EWeaponShellType OldType,
	                             const EWeaponShellType NewType,
	                             USoundCue*& OutBounceSound,
	                             UNiagaraSystem*& OutBounceVfx,
	                             TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& OutImpacts);

	bool GetIsUsingHeBounceOverwrite() const { return bIsUsingHeBounceOverwrite; }

private:
	// When an overwrite shell data is used the regular AP data is stored here.
	UPROPERTY()
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> M_CachedAPImpacts;
	// When an overwrite shell data is used the regular AP data is stored here.
	UPROPERTY()
	FRTSSurfaceImpactData M_CachedAPBounce;

	bool bIsApStored = false;
	bool bIsUsingHeBounceOverwrite = false;

	bool UsesAPImpacts(const EWeaponShellType Type) const;
	bool GetShellUsesHeImpacts(const EWeaponShellType ShellType) const;
	void SetOutEffectsToUseHeAndHeat(TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& OutImpacts) const;
	void SetBounceToHeAndHeat(USoundCue*& BounceSound, UNiagaraSystem*& BounceSystem);

	bool HasAnyHeAndHeatOverwrites() const;
	void RestoreImpactsWithStoredAP(
		USoundCue*& OutBounceSound,
		UNiagaraSystem*& OutBounceVfx,
		TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& OutImpacts);
};

/**
 * @brief Looks at the shell type of the weapon data and returns a new weapon data struct with the fields set
 * in line with that weapon shell type. This means that the underlying WeaponData that contains the primitive values for that
 * weapon is not changed. Note that when the shell type is APHE or AP we assume it is a base type and return the struct itself.
 * @return FWeaponData struct with the values for the shell type set.
 */
static FWeaponData GLOBAL_GetWeaponDataForShellType(const FWeaponData& OldWeaponData);

USTRUCT(BlueprintType)
struct FWeaponVFX
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundCue* LaunchSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundCue* BounceSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FLaunchEffectSettings LaunchEffectSettings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> SurfaceImpactEffects;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FShellVfxOverwrites ShellSpecificVfxOverwrites;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UNiagaraSystem* BounceEffect = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector LaunchScale = FVector(1.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector ImpactScale = FVector(1.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector BounceScale = FVector(1.0f);

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundAttenuation* ImpactAttenuation = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundConcurrency* ImpactConcurrency = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundAttenuation* LaunchAttenuation = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundConcurrency* LaunchConcurrency = nullptr;
};


USTRUCT(BlueprintType, Blueprintable)
struct FWeaponShellCase
{
	GENERATED_BODY()

	// === Public API =============================================
	void InitShellCase(UMeshComponent* NewAttachMeshComponent, UWorld* WorldSpawnedIn);
	void SpawnShellCase() const;
	// Also sets the amount of bullets fired.
	void SpawnShellCaseStartRandomBurst(const int BurstAmount) const;

protected:
	// --- Designer settings ---------------------------------------------------
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ShellCaseSocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UNiagaraSystem* ShellCaseVfx = nullptr;

	// Whether to set the parameters in this struct on the spawned vfx system (set once).
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool AdjustVfxParams = false;

	// Multiplies how long the casing vfx lives.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float LifetimeMlt = 1.f;

	// Multiplies the size of the casing mesh -> 0.5 for ~45 mm.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float MeshSizeMlt = 0.5f;

	// Multiplies the size of the casing smoke -> 0.5 for ~45mm.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SpriteSizeMlt = 0.5f;

	// Multiplies casing speed.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SpeedMlt = 1.f;

	// --- Runtime state -------------------------------------------------------
	UPROPERTY()
	UWorld* World = nullptr;

	UPROPERTY()
	UMeshComponent* AttachMeshComponent = nullptr;

	// Single pooled Niagara component per weapon (single shell-case system).
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<UNiagaraComponent> M_CachedShellCaseNiagara;

	// Have we already applied the static params to the current component?
	UPROPERTY(Transient)
	mutable bool bM_StaticParamsInitialized = false;

private:
	/** Get or create the pooled shell-case Niagara component (one per weapon). */
	UNiagaraComponent* GetOrCreateShellCaseComp() const;

	/** Apply static (one-time) parameters if not done yet for the current component. */
	void InitializeStaticParamsIfNeeded(UNiagaraComponent* NiagaraComp) const;

	/** Clean restart for the current shot/burst. */
	static void RestartNiagara(UNiagaraComponent* NiagaraComp);
};


USTRUCT()
struct FSlot
{
	GENERATED_BODY()

	UPROPERTY()
	UNiagaraComponent* Niagara = nullptr;
	UPROPERTY()
	UNiagaraSystem* LastNiagaraAsset = nullptr;
	UPROPERTY()
	UAudioComponent* Audio = nullptr;
	UPROPERTY()
	USoundBase* LastSoundAsset = nullptr;
};

/**
 * @brief Small, cache-friendly pool that reuses world-space impact emitters per weapon.
 * - Keeps N slots. Each slot owns one UNiagaraComponent and one UAudioComponent.
 * - On each impact we advance an index and reuse that slot:
 *     - If the requested Niagara/Sound asset matches what the slot last used → just restart.
 *     - Else swap the asset(s), relocate, and restart.
 * - Attenuation/Concurrency are fixed per pool instance (per weapon) and applied to audio components.
 */
USTRUCT()
struct RTS_SURVIVAL_API FWeaponImpactPool
{
	GENERATED_BODY()

public:
	/** Initialize the pool. Safe to call multiple times; reinitializes if capacity changes. */
	void Initialize(
		UWorld* InWorld,
		USoundAttenuation* InAttenuation,
		USoundConcurrency* InConcurrency,
		int32 InCapacity, float WeaponCalibre);

	/** Play a penetrating impact (surface-specific effect & sound). */
	void PlayImpact(
		const FVector& Location,
		const FRotator& Rotation,
		UNiagaraSystem* NiagaraSystem,
		const FVector& NiagaraScale,
		USoundBase* Sound);

	/** Play a non-penetration bounce (shared effect & sound). */
	void PlayBounce(
		const FVector& Location,
		const FRotator& Rotation,
		UNiagaraSystem* NiagaraSystem,
		const FVector& NiagaraScale,
		USoundBase* Sound, const bool bIsUsingHEBounceOverwrite);

	void Cleanup();

	/** Heuristic to size pool based on weapon stats. */
	static int32 RecommendCapacity(const FWeaponData& Data,
	                               EWeaponFireMode FireMode, float BurstCooldown);

private:
	UPROPERTY()
	TArray<FSlot> Slots;

	// Keep world pointer weak so we don’t keep a world alive inadvertently.
	UPROPERTY()
	TWeakObjectPtr<UWorld> World;

	UPROPERTY()
	USoundAttenuation* Attenuation = nullptr;

	UPROPERTY()
	USoundConcurrency* Concurrency = nullptr;

	int32 Capacity = 0;
	int32 NextIndex = 0;

private:
	/** Returns the next slot to use (grows array lazily if capacity increased). */
	FSlot& NextSlot();

	/** Ensure the slot owns a Niagara component; create if missing. */
	UNiagaraComponent* GetOrCreateNiagara(FSlot& Slot);

	/** Ensure the slot owns an Audio component; create if missing. */
	UAudioComponent* GetOrCreateAudio(FSlot& Slot);

	/** Core: set assets (if changed), position, and restart both emitters. */
	void Activate(
		FSlot& Slot,
		const FVector& Location,
		const FRotator& Rotation,
		UNiagaraSystem* NiagaraSystem,
		const FVector& NiagaraScale,
		USoundBase* Sound, const bool bIsUsingHeBounceOverwrite);

	// Used to scale bounce vfx of he overwrites.
	float M_WeaponCalibre = 1.f;
};

/**
 * @brief contains logic for firing all different weapon systems in the game. Public interface contains the Fire function.
 * This function calls a function pointer whicsh is set according to the mode of fire that is chosen for this weapon on Init.
 * Each of these different fire modes call a virtual function FireWeaponSystem which is implemented in derived classes
 * as direct hit, trace or projectile.
 * @note --------------------------------------------------------------
 * Uses the weapon owner interface to implement vfx, sounds and hit updates on any owner.
 * @note --------------------------------------------------------------
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponState : public UObject
{
	GENERATED_BODY()

	// To use the pooling impacts.
	friend RTS_SURVIVAL_API AProjectile;

public:
	UWeaponState();
	// Virtual destructor for inheritance
	virtual ~UWeaponState() override;

	/** Fired exactly once per shot for Single mode, and once per completed burst for burst modes.
	 * Also fires with FULL MAG after reload*/
	FOnMagConsumed OnMagConsumed;

	void UpgradeWeaponWithExtraShellType(const EWeaponShellType ExtraShellType);
	void UpgradeWeaponWithRangeMlt(const float RangeMlt);

	// Registers or unregisters an actor to be ignored by this weapon when firing.
	void RegisterActorToIgnore(AActor* RTSValidActor, const bool bRegister);

	FName GetWeaponSocket() const
	{
		return FireSocketName;
	}

	/**
	 * @brief Fire the weapon.
	 * @param AimPointOpt Optional explicit aim point (world-space). Most weapons ignore it and
	 *        use IWeaponOwner::GetFireDirection(). Arching weapons need it.
	 */
	void Fire(const FVector& AimPointOpt);


	/**
	 * @brief Stops the weapon timer.
	 * @param bStopReload Whether to stop the reload timer. If true then the weapon needs to restart reloading
	 * on the next target.
	 * @param bStopCoolDown Will stop the cooldown timer; means the weapon is ready for fire immediatly after.
	 */
	void StopFire(
		const bool bStopReload = false,
		const bool bStopCoolDown = false);

	void DisableWeapon();

	/** @return The range of this weapon in cm. */
	float GetRange() const;

	/** @return The range of this weapon in cm including behaviour adjustments. */
	float GetWeaponRangeBehaviourAdjusted() const;

	void ForceInstantReload();
	bool IsWeaponFullyLoaded() const;

	/** @return The weapon data of this weapon disregarding any changes that are made to fired projectiles by the
	 * shell type, only returns pure primitive weapon data values. */
	const FWeaponData& GetRawWeaponData() const;

	FWeaponVFX* GetWeaponVfx();
	const FWeaponVFX* GetWeaponVfx() const;

	inline int32 GetCurrentMagCapacity() const { return M_CurrentMagCapacity; };

	FWeaponData* GetWeaponDataToUpgrade();

	/**
	 * @brief Apply or remove behaviour-driven weapon attribute modifications.
	 * @param BehaviourWeaponAttributes Incoming attribute deltas provided by a behaviour.
	 * @param bAddUpgrade When true apply the upgrade, otherwise remove it.
	 */
	void Upgrade(const FBehaviourWeaponAttributes& BehaviourWeaponAttributes, const bool bAddUpgrade = true);

	/** @return The weapon data of this weapon, values adjusted for the type of selected shell. */
	FWeaponData GetWeaponDataAdjustedForShellType() const;

	/** @brief Change the weapon's shell type, will only do this if the provided type is part of the shelltypes array.
	 * @param NewShellType The new shell type to set.
	 * @return Whether the shell type was changed.
	 */
	bool ChangeWeaponShellType(const EWeaponShellType NewShellType);

	void ChangeOwningPlayer(const int32 NewOwningPlayer);

	FOnShellTypeChanged OnWeaponShellTypeChanged;

	void SetIsAircraftWeapon(const bool NewbIsAircraftWeapon) { bIsAircraftWeapon = NewbIsAircraftWeapon; };

protected:
	virtual void BeginDestroy() override;

	virtual void OnStopFire();
	virtual void OnDisableWeapon();
	virtual void OnReloadFinished_PostReload();


	// The explicit aim point for this shot.
	FVector ExplicitAimPoint = FVector::ZeroVector;

	// Identifies the weapon on the owner
	int32 WeaponIndex;

	// Whether the weapon is direct, trace or projectile based.
	UPROPERTY()
	EWeaponSystemType WeaponHitType;

	// Contains the basic stats of the weapon.
	UPROPERTY()
	FWeaponData WeaponData;

	void InitWeaponState(
		int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponFireMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false, const bool bIsLaserOrFlame = false);

	UPROPERTY()
	TSubclassOf<UDamageType> DamageTypeClass;
	UPROPERTY()
	FDamageEvent DamageEvent;
	// call this once in InitWeaponState and whenever WeaponData.DamageType changes
	void RefreshDamageTypeClass();

	// Determines the collision channel used for weapon traces.
	ECollisionChannel TraceChannel;

	// The weapon component either static mesh or skeletal mesh.
	UPROPERTY()
	UMeshComponent* MeshComponent = nullptr;

	// The socket the weapon fires from.
	UPROPERTY()
	FName FireSocketName;

	typedef void (UWeaponState::*FireModeFunction)();
	FireModeFunction FireModeFunc;

	void FireSingleShot();
	void FireSingleBurst();
	void FireRandomBurst();

	virtual void FireWeaponSystem();

	// The class that encapsulates this weapon state.
	UPROPERTY()
	TScriptInterface<IWeaponOwner> WeaponOwner;

	// The world this weapon is spawned in.
	UPROPERTY()
	UWorld* World;

	/** @return The launch location and forward vector of the weapon. */
	TPair<FVector, FVector> GetLaunchAndForwardVector() const;

	/**
	 * Damages the hit actor with the calculated damage value also does penetration calculation setups.
	 * @param HitActor
	 * @param BaseDamageToFlux 
	 * @return Whether the actor was killed.
	 */
	bool FluxDamageHitActor_DidActorDie(AActor* HitActor, const float BaseDamageToFlux);

	/**
	 * Notifies the weapon owner that the actor was killed.
	 * @param KilledActor The actor that was killed by the weapon.
	 * @pre Assumes that the weapon owner is valid.
	 */
	void OnActorKilled(AActor* KilledActor) const;

	UPROPERTY()
	int32 OwningPlayer;

	/**
	 * Create VFX at the hit location.
	 * @param HitLocation Where the Weapon hit something.
	 * @param HitSurface
	 * @param ImpactRotation
	 */
	void CreateWeaponImpact(const FVector& HitLocation, const ERTSSurfaceType HitSurface,
	                        const FRotator& ImpactRotation = FRotator::ZeroRotator);

	/**
	 * Creates a bounce vfx of the provided weaponVFX.
	 * @param HitLocation Where the bounce occurred
	 * @param BounceNormal The normal of the surface the weapon bounced off.
	 */
	void CreateWeaponNonPenVfx(const FVector& HitLocation, const FRotator& BounceNormal);

	UPROPERTY()
	FWeaponVFX M_WeaponVfx;

	void ReportErrorForWeapon(const FString& ErrorMessage) const;

	/**
	 * Creates a Launch VFX attached to the weapon.
	 * @param LaunchLocation The location the weapon is launched from.
	 * @param ForwardVector The forward vector of the weapon.
	 * @param bCreateShellCase Whether to create a shell case.
	 */
	virtual void CreateLaunchVfx(
		const FVector& LaunchLocation,
		const FVector& ForwardVector,
		const bool bCreateShellCase = true);

	/**
	 * @brief Spawn/restart pooled muzzle VFX at a socket (also drives ground/muzzle smoke).
	 *        Components are created once per socket and kept; subsequent fires just restart them.
	 * @param SocketName      Socket to attach to (must exist).
	 * @param LaunchRotation  Socket rotation at fire time.
	 * @param LaunchLocation  Socket location at fire time.
	 */
	void CreateLaunchAndSmokeVfx(
		const FName SocketName,
		const FRotator& LaunchRotation,
		const FVector& LaunchLocation);


	// Protected as some derivatives like the multi trace weapon overwrite the way weapon launch VFX works.
	UPROPERTY()
	FWeaponShellCase WeaponShellCase;

	// Checks if the weapon has enough bulelts left for a full single burst again; if not we already reload
	void OnCooldownShutDown();

	// Whether this weapon is mounted on an aircraft; if so it uses a different accuracy calculation.
	UPROPERTY()
	bool bIsAircraftWeapon = false;

	UPROPERTY()
	TArray<AActor*> ActorsToIgnore;

private:
	// Pools world-space impact Niagara & audio per-weapon. NEW
	UPROPERTY()
	FWeaponImpactPool M_ImpactPool;


	// ---------------------------- NIAGARA LAUNCH CACHING ------------------------------------
	/**
     * @brief Get or create (and attach) the pooled launch Niagara component for a socket.
     *        Initializes only static parameters once; does not set shell colors here.
     * @param SocketName     Socket to attach to (must exist).
     * @param LaunchRotation Current socket rotation.
     * @param LaunchLocation Current socket location.
     * @return Valid UNiagaraComponent* on success, nullptr on failure.
     */
	UNiagaraComponent* GetOrCreateLaunchNiagaraForSocket(
		const FName& SocketName,
		const FRotator& LaunchRotation,
		const FVector& LaunchLocation);


	/** Initialize static (one-time) launch VFX parameters on a newly created Niagara component. */
	void InitializeLaunchNiagaraStaticParams(UNiagaraComponent* NiagaraComp) const;

	void SetColorByShellTypeInitParams(UNiagaraComponent* NiagaraComp) const;
	void SetDirectLifeTimeSizeScaleInitParams(UNiagaraComponent* NiagaraComp) const;

	/**
	 * @brief If the current WeaponData.ShellType differs from the last applied, push the new
	 *        shell color(s) to ALL cached launch Niagara components (both muzzle & ground smoke),
	 *        then remember the shell type. No-ops otherwise.
	 */
	void UpdateAllCachedLaunchColorsIfShellChanged();

	// --- cached pooled Niagara per socket (launch/muzzle system) ---
	// Lifetime-owned by the weapon; components are attached to MeshComponent at the socket.
	UPROPERTY()
	TMap<FName, TWeakObjectPtr<UNiagaraComponent>> M_LaunchNiagaraBySocket;

	// --- last shell used to initialize colors on cached launch systems ---
	UPROPERTY()
	EWeaponShellType M_LastShellTypeForLaunchVfx = EWeaponShellType::Shell_None;

	// --------------------------- END Niagara Launch Caching ---------------------------

	// Name of the weapon to access data in the player controller.
	UPROPERTY()
	EWeaponName M_WeaponName;

	// Determines how the weapon handles launch effects and the amount of bullets fired.
	UPROPERTY()
	EWeaponFireMode M_WeaponFireMode;

	// The current magazine capacity of the weapon orignally set to the max capacity in the weapon data.
	UPROPERTY()
	int32 M_CurrentMagCapacity = 0;

	UPROPERTY()
	bool bM_IsOnCooldown = false;

	UPROPERTY()
	bool bM_IsReloading = false;

	// If set to false then onRandomBurst will not create a shell casing effect for each single bullet fired
	// but only at the beginning of the burst.
	UPROPERTY()
	bool bM_CreateShellCaseOnEachRandomBurst = false;


	FTimerHandle M_WeaponTimerHandle;
	// Bound to the cooldown function when Init.
	FTimerDelegate M_CoolDownDel;
	// Bound to the reload function when Init.
	FTimerDelegate M_ReloadDel;
	// Bound to the SingleBurst function if this weapon is on SingleBurst mode.
	FTimerDelegate M_BurstModeDel;

	// Amount of bullets left in current (ongoing) burst.
	int32 M_AmountLeftInBurst = 0;

	int32 M_MaxBurstAmount = 0;
	int32 M_MinBurstAmount = 0;
	float M_BurstCooldown = 0.0f;

	/**
	 * @brief Sets up the penetration data for the weapon in the damage event.
	 * @param ArmorPen The armor penetration value of the weapon.
	 */
	void SetupPenetrationData(
		const float ArmorPen);

	// Sets a timer to reload the weapon.
	void Reload();

	// When the reload timer finishes.
	void OnReloadFinished();

	// Sets up the cooldown timer for the weapon.
	void CoolDown();

	// When the cooldown timer finishes.
	void OnCoolDownFinished();

	/**
	 * @param BaseTime How long the base time is. 
	 * @param FluxPercentage The percentage of fluctuation.
	 * @return A randomized time value within the specified range.
	 */
	float GetTimeWithFlux(float BaseTime, int32 FluxPercentage) const;

	void UpdateWeaponDataForOwningPlayer(const EWeaponName WeaponNameObtainValues, const int32 OwningPlayerOfWeapon);

	// Starts the weapon timer with burst
	void InitSingleBurstMode();

	// Fires the weapon, reduces current burst and mag and checks
	// if this was the last burst in this mode; if so stops burst mode.
	void OnSingleBurst();

	void InitRandomBurst();

	void OnRandomBurst();

	// Will cooldown the weapon if no more bullets are left in the mag.
	void OnStopBurstMode();

	void InitWeaponMode(
		const EWeaponFireMode FireMode,
		const uint32 MaxBurstAmount,
		const uint32 MinBurstAmount,
		const float BurstCooldown);
};

// To provide parameters to initialise a direct hit weapon.
USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateDirectHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 OwningPlayer = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWeaponName WeaponName = EWeaponName::T26_Mg;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWeaponFireMode WeaponBurstMode = EWeaponFireMode::Single;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TScriptInterface<IWeaponOwner> WeaponOwner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMeshComponent* MeshComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName FireSocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FWeaponVFX WeaponVFX;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FWeaponShellCase WeaponShellCase;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float BurstCooldown = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SingleBurstAmountMaxBurstAmount = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MinBurstAmount = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool CreateShellCasingOnEveryRandomBurst = false;
};

/**
 * @brief Implements FireWeaponSystem with a direct hit system.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateDirectHit : public UWeaponState
{
	GENERATED_BODY()

public:
	void InitDirectHitWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false);

protected:
	// If target actor is RTS valid then fire direct hit.
	virtual void FireWeaponSystem() override final;

private:
	/**
	 * @brief Determines with random if the weapon will hit the target depending on accuracy.
	 * @param HitActor The actor that the weapon will try to hit.
	 * @pre Assumes that HitActor is RTS valid, assumes that WeaponOwner is valid.
	 */
	void FireDirectHit(AActor* HitActor);
};


// To provide parameters to initialise a trace weapon.
USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStatTrace : public FInitWeaponStateDirectHit
{
	GENERATED_BODY()


	// Affects the visual of the projectile fired.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 TraceProjectileType = 1;
};


/**
 * @brief Implements FireWeaponSystem with a trace system.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateTrace : public UWeaponState
{
	GENERATED_BODY()

public:
	UWeaponStateTrace();

	void InitTraceWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false,
		const int32 TraceProjectileType = 1);

	void SetupProjectileManager(ASmallArmsProjectileManager* ProjectileManager);

protected:
	virtual void FireWeaponSystem() override;

	/** @note protected as also used by multi trace weapon version */
	void OnAsyncTraceComplete(
		FTraceDatum& TraceDatum,
		const float ProjectileLaunchTime,
		const FVector& LaunchLocation,
		const FVector& TraceEnd);

private:
	/**
	 * @brief Spawns a trace from the provided location using the collision channel of this weapon state.
	 * @param PitchAndYaw
	 * @pre WeaponOwner is valid.
	 */
	void FireTrace(const FVector& Direction);
	bool DidTracePen(const FHitResult& TraceHit, AActor*& OutHitActor) const;
	bool DidTracePenArmorCalcComponent(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult) const;

	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;

	// Type of projectile used for the trace; alters visuals.
	int32 M_TraceProjectileType = 1;

	/**
	 *  
	 * @param HitResult 
	 * @param HitActor The actor hit.
	 */
	inline void OnActorPenArmor(const FHitResult& HitResult, AActor* HitActor);

	void OnAsyncTraceHitValidActor(const FHitResult& TraceHit, FVector& OutEndLocation,
	                               const FRotator& ImpactRotation, const ERTSSurfaceType SurfaceTypeHit);
	void OnAsyncTraceNoHit(const FVector& TraceEnd);
};

USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateProjectile : public FInitWeaponStateDirectHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EProjectileNiagaraSystem ProjectileSystem = EProjectileNiagaraSystem::TankShell;
};

USTRUCT(BlueprintType)
struct FArchProjectileSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ApexHeightMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float ApexHeightOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float MinApexOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float CurvatureVerticalVelocityMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	USoundBase* DescentSound = nullptr;
};

USTRUCT(BlueprintType)
struct FSplitterArcWeaponSplitSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="1", UIMin="1"))
	int32 SplitProjectileCount = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float SplitSpreadRadius = 250.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float SplitCalibreMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float SplitDamageMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float SplitArmorPenMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float SplitAoeMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<USoundBase> SplitSound = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<USoundAttenuation> SplitSoundAttenuation = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<USoundConcurrency> SplitSoundConcurrency = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UNiagaraSystem> SplitNiagaraSystem = nullptr;
};

USTRUCT(BlueprintType)
struct FSplitterArcWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FArchProjectileSettings ArchSettings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSplitterArcWeaponSplitSettings SplitSettings;
};


UCLASS()
class UWeaponStateProjectile : public UWeaponState
{
	GENERATED_BODY()

public:
	void InitProjectileWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		EProjectileNiagaraSystem ProjectileNiagaraSystem,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false);
	/**
	 * Notifies weapon owner of the killed actor.
	 * @param KilledActor The actor that was killed by the projectile.
	 */
	void OnProjectileKilledActor(AActor* KilledActor) const;

	void OnProjectileHit(const bool bBounced) const;


	void SetupProjectileManager(ASmallArmsProjectileManager* ProjectileManager);


	void CopyStateFrom(const UWeaponStateProjectile* Other);

protected:
	virtual void FireWeaponSystem() override;

	bool GetIsValidProjectileManager() const;

	ASmallArmsProjectileManager* GetProjectileManager() const;

	EProjectileNiagaraSystem GetProjectileNiagaraSystem() const;

private:
	// Determines the type of projectile niagara system to set.
	UPROPERTY()
	EProjectileNiagaraSystem M_ProjectileNiagaraSystem;

	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;

	void FireProjectile(const FVector& TargetDirection);

	/**
	 * @brief Fires a projectile with the provided stats.
	 * @param ShellAdjustedData The data of the weapon adjusted for the shell type.
	 * @param Projectile The spawned projectile assumed to be valid.
	 * @param LaunchLocation
	 * @param LaunchRotation The Rotation the projectile should take.
	 * @note FORCEINLINE, do not call anywhere else!!
	 */
	FORCEINLINE void FireProjectileWithShellAdjustedStats(const FWeaponData& ShellAdjustedData, AProjectile* Projectile,
	                                                      const FVector& LaunchLocation,
	                                                      const FRotator& LaunchRotation);
};

USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateArchProjectile : public FInitWeaponStateProjectile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FArchProjectileSettings ArchSettings;
};

USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateSplitterArchProjectile : public FInitWeaponStateProjectile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FSplitterArcWeaponSettings SplitterSettings;
};


/**
 * @brief Weapon state that fires arching projectiles.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateArchProjectile : public UWeaponStateProjectile
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes an arch projectile weapon using pooled projectile data and arch settings.
	 * @param NewOwningPlayer Player index owning the weapon.
	 * @param NewWeaponIndex Weapon array index on the owner.
	 * @param NewWeaponName Identifier for this weapon.
	 * @param NewWeaponBurstMode Burst mode configuration.
	 * @param NewWeaponOwner Interface to the weapon owner.
	 * @param NewMeshComponent Mesh used to anchor the fire socket.
	 * @param NewFireSocketName Socket the projectile spawns from.
	 * @param NewWorld World context for spawning.
	 * @param ProjectileNiagaraSystem Niagara system used for the projectile VFX.
	 * @param NewWeaponVFX VFX configuration for the weapon.
	 * @param NewWeaponShellCase Shell casing configuration.
	 * @param NewBurstCooldown Cooldown between bursts.
	 * @param NewSingleBurstAmountMaxBurstAmount Burst count settings.
	 * @param NewMinBurstAmount Minimum burst shots.
	 * @param bNewCreateShellCasingOnEveryRandomBurst Should spawn shell casing each random burst.
	 * @param NewArchSettings Arch tuning settings for the projectile path.
	 */
	void InitArchProjectileWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		const EProjectileNiagaraSystem ProjectileNiagaraSystem,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false,
		const FArchProjectileSettings& NewArchSettings = FArchProjectileSettings());

protected:
	virtual void FireWeaponSystem() override;

private:
	UPROPERTY()
	FArchProjectileSettings M_ArchSettings;

	// Now takes a target *location* instead of a direction.
	void FireProjectile(const FVector& TargetLocation);


	/**
	 * @brief Fires a projectile with the provided stats.
	 * @param ShellAdjustedData The data of the weapon adjusted for the shell type.
	 * @param Projectile The spawned projectile assumed to be valid.
	 * @param LaunchLocation Location the projectile starts from.
	 * @param LaunchRotation The Rotation the projectile should take.
	 * @param TargetLocation The adjusted target location.
	 */
	FORCEINLINE void FireProjectileWithShellAdjustedStats(const FWeaponData& ShellAdjustedData,
	                                                      AProjectile* Projectile,
	                                                      const FVector& LaunchLocation,
	                                                      const FRotator& LaunchRotation,
	                                                      const FVector& TargetLocation);
};

/**
 * @brief Weapon state that launches one arcing projectile and splits it into pooled child projectiles at apex.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateSplitterArchProjectile : public UWeaponStateProjectile
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes a splitter arc weapon so split logic is fully data driven from one setup call.
	 * @param NewOwningPlayer Player index owning the weapon.
	 * @param NewWeaponIndex Weapon array index on the owner.
	 * @param NewWeaponName Identifier for this weapon.
	 * @param NewWeaponBurstMode Burst mode configuration.
	 * @param NewWeaponOwner Interface to the weapon owner.
	 * @param NewMeshComponent Mesh used to anchor the fire socket.
	 * @param NewFireSocketName Socket the projectile spawns from.
	 * @param NewWorld World context for spawning.
	 * @param ProjectileNiagaraSystem Niagara system used for base projectile VFX.
	 * @param NewWeaponVFX VFX configuration for the weapon.
	 * @param NewWeaponShellCase Shell casing configuration.
	 * @param NewBurstCooldown Cooldown between bursts.
	 * @param NewSingleBurstAmountMaxBurstAmount Burst count settings.
	 * @param NewMinBurstAmount Minimum burst shots.
	 * @param bNewCreateShellCasingOnEveryRandomBurst Should spawn shell casing each random burst.
	 * @param NewSplitterSettings Settings for arc and split child projectile behaviour.
	 */
	void InitSplitterArchProjectileWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		const EProjectileNiagaraSystem ProjectileNiagaraSystem,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false,
		const FSplitterArcWeaponSettings& NewSplitterSettings = FSplitterArcWeaponSettings());

protected:
	virtual void FireWeaponSystem() override;

private:
	UPROPERTY()
	FSplitterArcWeaponSettings M_SplitterSettings;


	void FireProjectile(const FVector& TargetLocationRaw);

	void SpawnSplitProjectilesAtApex(const FVector SplitLocation, const FVector OriginalTargetLocation);

	void SpawnSplitEffects(const FVector& SplitLocation) const;

	FWeaponData BuildSplitProjectileData(const FWeaponData& SourceWeaponData) const;

	FVector BuildSplitTargetLocation(const FVector& OriginalTargetLocation) const;

	float CalculateArcTimeToApex(const FVector& LaunchLocation, const FVector& TargetLocation) const;

	/**
	 * @brief Initializes and launches a child projectile using split-adjusted stats.
	 * @param SplitProjectileData Base stat package after split multipliers.
	 * @param SpawnedProjectile Dormant projectile taken from the shared pool.
	 * @param LaunchLocation Split location used as launch origin.
	 * @param TargetLocation Child target location generated by split spread settings.
	 */
	void LaunchSplitProjectile(const FWeaponData& SplitProjectileData,
	                          AProjectile* SpawnedProjectile,
	                          const FVector& LaunchLocation,
	                          const FVector& TargetLocation);
};

USTRUCT(Blueprintable, BlueprintType)
struct FRocketWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UStaticMesh* RocketMesh = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FName> FireSocketNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float RandomSwingYawDeviationMin = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float RandomSwingYawDeviationMax = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.1", UIMin="0.1"))
	float StraightSpeedMlt = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.1", UIMin="0.1"))
	float InCurveSpeedMlt = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="1.0", ClampMax="100.0", UIMin="1.0", UIMax="100.0"))
	float RandomSwingStrength = 50.0f;
};

USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateRocketProjectile : public FInitWeaponStateProjectile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FRocketWeaponSettings RocketSettings;
};

USTRUCT(Blueprintable, BlueprintType)
struct FVerticalRocketWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UStaticMesh* RocketMesh = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FName> FireSocketNames;

	/**
	 * @brief When enabled launch sockets are discovered from RocketsToSpawnBaseMesh sockets that contain SocketNameFilter.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bSetupWithAttachedRocketsMesh = false;

	/** Base mesh used to spawn hideable rocket instances when bSetupWithAttachedRocketsMesh is true. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UStaticMesh* RocketsToSpawnBaseMesh = nullptr;

	/** Any socket containing this text on RocketsToSpawnBaseMesh becomes a launch/instance socket. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName AttachedRocketsSocketNameFilter = NAME_None;

	/**
	 * @brief Minimum vertical offset added to launch location to generate stage-1 apex.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="1.0", UIMin="1.0"))
	float ApexZOffsetMin = 700.0f;

	/**
	 * @brief Maximum vertical offset added to launch location to generate stage-1 apex.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="1.0", UIMin="1.0"))
	float ApexZOffsetMax = 1300.0f;

	/**
	 * @brief Minimum random cone angle used to offset apex horizontally while keeping chosen apex height.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0", ClampMax="89.0", UIMax="89.0"))
	float RandomAngleOffsetMin = 0.0f;

	/**
	 * @brief Maximum random cone angle used to offset apex horizontally while keeping chosen apex height.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0", ClampMax="89.0", UIMax="89.0"))
	float RandomAngleOffsetMax = 22.0f;

	/**
	 * @brief Curvature multiplier for stage-1 bezier. Values > 1 bend more, values < 1 flatten the curve.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.1", UIMin="0.1"))
	float FirstStageBezierCurvature = 1.0f;

	/**
	 * @brief Distance in cm for stage-2 mini-arc before transitioning to final straight intercept.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="50.0", UIMin="50.0"))
	float Stage2ArcDistance = 280.0f;

	/**
	 * @brief Upward offset in cm at the top of stage-2 mini-arc to soften direction changes.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.0", UIMin="0.0"))
	float Stage2ArcHeight = 130.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.1", UIMin="0.1"))
	float Stage1SpeedMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.1", UIMin="0.1"))
	float Stage2ArcSpeedMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ClampMin="0.1", UIMin="0.1"))
	float Stage2StraightSpeedMultiplier = 1.2f;
};

USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateVerticalRocketProjectile : public FInitWeaponStateProjectile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVerticalRocketWeaponSettings VerticalRocketSettings;
};

/**
 * @brief Weapon state that fires a rocket with a curved opening swing before straight flight.
 */
UCLASS()
class RTS_SURVIVAL_API UWeaponStateRocketProjectile : public UWeaponStateProjectile
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes a rocket projectile weapon with swing/straight movement settings.
	 * @param NewOwningPlayer Player index owning the weapon.
	 * @param NewWeaponIndex Weapon array index on the owner.
	 * @param NewWeaponName Identifier for this weapon.
	 * @param NewWeaponBurstMode Burst mode configuration.
	 * @param NewWeaponOwner Interface to the weapon owner.
	 * @param NewMeshComponent Mesh used to anchor the fire socket.
	 * @param NewFireSocketName Socket the projectile spawns from.
	 * @param NewWorld World context for spawning.
	 * @param ProjectileNiagaraSystem Niagara system used for the projectile VFX.
	 * @param NewWeaponVFX VFX configuration for the weapon.
	 * @param NewWeaponShellCase Shell casing configuration.
	 * @param NewRocketSettings Swing/straight tuning for the rocket launch.
	 * @param NewBurstCooldown Cooldown between bursts.
	 * @param NewSingleBurstAmountMaxBurstAmount Burst count settings.
	 * @param NewMinBurstAmount Minimum burst shots.
	 * @param bNewCreateShellCasingOnEveryRandomBurst Should spawn shell casing each random burst.
	 */
	void InitRocketProjectileWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		const EProjectileNiagaraSystem ProjectileNiagaraSystem,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const FRocketWeaponSettings& NewRocketSettings,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false);

protected:
	virtual void FireWeaponSystem() override;
	virtual void CreateLaunchVfx(
		const FVector& LaunchLocation,
		const FVector& ForwardVector,
		const bool bCreateShellCase = true) override;

private:
	struct FRocketLaunchSocketData
	{
		FName SocketName = NAME_None;
		FVector LaunchLocation = FVector::ZeroVector;
		FVector ForwardVector = FVector::ZeroVector;
	};

	UPROPERTY()
	FRocketWeaponSettings M_RocketSettings;

	int32 M_NextRocketSocketIndex = 0;

	void FireProjectile(const FVector& TargetLocation);

	FRocketLaunchSocketData GetRocketLaunchSocketData(const bool bAdvanceSocketIndex);

	/**
	 * @brief Fires a rocket with the provided stats and swing configuration.
	 * @param ShellAdjustedData The data of the weapon adjusted for the shell type.
	 * @param Projectile The spawned projectile assumed to be valid.
	 * @param LaunchLocation Location the projectile starts from.
	 * @param LaunchRotation The Rotation the projectile should take.
	 * @param TargetLocation The adjusted target location.
	 */
	FORCEINLINE void FireProjectileWithShellAdjustedStats(const FWeaponData& ShellAdjustedData,
	                                                      AProjectile* Projectile,
	                                                      const FVector& LaunchLocation,
	                                                      const FRotator& LaunchRotation,
	                                                      const FVector& TargetLocation);
};

/**
 * @brief Weapon state that fires rockets in a staged vertical profile with optional attached-rocket instance setup.
 */
UCLASS()
class RTS_SURVIVAL_API UVerticalRocketWeaponState : public UWeaponStateProjectile
{
	GENERATED_BODY()

public:
	void InitVerticalRocketWeapon(
		const int32 NewOwningPlayer,
		const int32 NewWeaponIndex,
		const EWeaponName NewWeaponName,
		const EWeaponFireMode NewWeaponBurstMode,
		TScriptInterface<IWeaponOwner> NewWeaponOwner,
		UMeshComponent* NewMeshComponent,
		const FName NewFireSocketName,
		UWorld* NewWorld,
		const EProjectileNiagaraSystem ProjectileNiagaraSystem,
		FWeaponVFX NewWeaponVFX,
		FWeaponShellCase NewWeaponShellCase,
		const FVerticalRocketWeaponSettings& NewVerticalRocketSettings,
		const float NewBurstCooldown = 0.0f,
		const int32 NewSingleBurstAmountMaxBurstAmount = 0,
		const int32 NewMinBurstAmount = 0,
		const bool bNewCreateShellCasingOnEveryRandomBurst = false);

protected:
	virtual void FireWeaponSystem() override;
	virtual void CreateLaunchVfx(
		const FVector& LaunchLocation,
		const FVector& ForwardVector,
		const bool bCreateShellCase = true) override;
	virtual void OnReloadFinished_PostReload() override;

private:
	struct FVerticalRocketLaunchSocketData
	{
		FName SocketName = NAME_None;
		FVector LaunchLocation = FVector::ZeroVector;
		FVector ForwardVector = FVector::ZeroVector;
		int32 HiddenInstanceIndex = INDEX_NONE;
	};

	UPROPERTY()
	FVerticalRocketWeaponSettings M_VerticalRocketSettings;

	UPROPERTY()
	TArray<FName> M_LaunchSocketNames;

	UPROPERTY()
	TObjectPtr<URTSHidableInstancedStaticMeshComponent> M_AttachedRocketInstances;

	UPROPERTY()
	TMap<FName, int32> M_SocketToInstanceIndex;

	int32 M_NextRocketSocketIndex = 0;

	bool SetupAttachedRocketInstances();
	void CollectLaunchSocketsFromAttachedRocketMesh();
	FVerticalRocketLaunchSocketData GetVerticalRocketLaunchSocketData(const bool bAdvanceSocketIndex);
	void FireProjectile(const FVector& TargetLocation);
	void UnhideAllAttachedRocketInstances() const;
	bool GetIsValidAttachedRocketInstances() const;

	FORCEINLINE void FireProjectileWithShellAdjustedStats(const FWeaponData& ShellAdjustedData,
	                                                      AProjectile* Projectile,
	                                                      const FVerticalRocketLaunchSocketData& LaunchData,
	                                                      const FVector& TargetLocation);
};


USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateLaser
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 OwningPlayer = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWeaponName WeaponName = EWeaponName::T26_Mg;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TScriptInterface<IWeaponOwner> WeaponOwner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMeshComponent* MeshComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName FireSocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FLaserWeaponSettings LaserWeaponSettings;
};

USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateMultiHitLaser
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 OwningPlayer = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWeaponName WeaponName = EWeaponName::T26_Mg;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TScriptInterface<IWeaponOwner> WeaponOwner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMeshComponent* MeshComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName FireSocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FMultiHitLaserWeaponSettings LaserWeaponSettings;
};
