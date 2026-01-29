// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/PlayerAimAbilitiy/PlayerAimAbilityTypes/PlayerAimAbilityTypes.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"
#include "RTS_Survival/Weapons/WeaponTargetingData/WeaponTargetingData.h"
#include "AttachWeaponAbilityTypes.h"
#include "AttachedWeaponAbilityComponent.generated.h"

class ICommands;
class UWeaponState;
class UStaticMesh;
class UStaticMeshComponent;
class UMeshComponent;
class ASmallArmsProjectileManager;

UENUM()
enum class EAttachedWeaponAbilityMeshSetup : uint8
{
	Uninitialized,
	ExistingMesh,
	SpawnedMesh
};

USTRUCT()
struct FAttachedWeaponAbilityFireState
{
	GENERATED_BODY()

	FVector M_TargetLocation = FVector::ZeroVector;

	FTimerHandle M_FireLoopTimerHandle;
	FTimerHandle M_StopFireTimerHandle;

	bool bM_IsFiring = false;
};

USTRUCT(BlueprintType)
struct FAttachedWeaponAbilitySettings
{
	GENERATED_BODY()

	FAttachedWeaponAbilitySettings();

	// Identifies the attached weapon ability subtype.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttachWeaponAbilitySubType WeaponAbilityType = EAttachWeaponAbilitySubType::Pz38AttachedMortarDefault;

	// Attempts to add the ability to this index of the Unit's Ability Array.
	// Reverts to first empty index if negative or already occupied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;

	// How long the attached weapon(s) fire for.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TotalWeaponFireTime = 2.0f;

	// How long the ability is on cooldown after use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cooldown = 5;

	// Aim assist material type shown on the player aim ability.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPlayerAimAbilityTypes AimAssistType = EPlayerAimAbilityTypes::None;

	// The radius of the ability effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius = 300.0f;
};

/**
 * @brief Ability component used by designers to bolt weapon fire logic onto a unit via mesh sockets.
 *        Treat it as the weapon runtime for an ability and configure it before any Setup calls.
 *        The component expects a mesh source and then builds weapon state objects around it.
 *
 * Designer setup (existing mesh):
 * - Ensure the owning actor already has a mesh component with the required fire sockets.
 * - Call InitWithExistingMesh from Blueprint before any Setup*Weapon calls.
 * - The mesh supplied to Setup*Weapon is ignored and replaced by the existing mesh, so keep
 *   the socket names and fire offsets authored on that mesh.
 *
 * Designer setup (spawned mesh):
 * - Provide a static mesh asset that contains the required fire sockets.
 * - Call InitWithSpawnedMesh from Blueprint before any Setup*Weapon calls and supply a
 *   relative transform that places the mesh correctly on the owner.
 * - The component spawns and registers a UStaticMeshComponent, so collision and materials
 *   should be authored on the mesh asset itself.
 *
 * Runtime flow (init → fire → stop):
 * - InitWithExistingMesh or InitWithSpawnedMesh stores the mesh choice and resolves any
 *   pending Setup*Weapon requests once the mesh is valid.
 * - Setup*Weapon builds a weapon state and caches the owner, sockets, and targeting data.
 * - ExecuteAttachedWeaponAbility updates the target location, reloads weapons if needed,
 *   and starts the internal fire loop.
 * - StopAttachedWeaponAbilityFire clears fire timers and tells all weapon states to stop.
 *
 * @note InitWithExistingMesh: call in blueprint to set the weapon mesh before setup functions.
 * @note InitWithSpawnedMesh: call in blueprint to spawn a mesh before setup functions.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAttachedWeaponAbilityComponent : public UActorComponent, public IWeaponOwner
{
	GENERATED_BODY()

public:
	UAttachedWeaponAbilityComponent();

	void ExecuteAttachedWeaponAbility(const FVector& TargetLocation);
	void StopAttachedWeaponAbilityFire();

	EAttachWeaponAbilitySubType GetAttachedWeaponAbilityType() const;
	EPlayerAimAbilityTypes GetAimAssistType() const;
	float GetAttachedWeaponAbilityRange() const;
	float GetAttachedWeaponAbilityRadius() const;

	/**
	 * @brief Initializes the component to use an existing mesh for weapon setup.
	 * @param ExistingMesh The mesh component used for weapon fire sockets.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Constructor")
	void InitWithExistingMesh(UMeshComponent* ExistingMesh);

	/**
	 * @brief Initializes the component to spawn a mesh for weapon setup.
	 * @param MeshToSpawn The static mesh asset used for the spawned mesh component.
	 * @param RelativeTransform Relative transform applied when attaching to the owner.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Constructor")
	void InitWithSpawnedMesh(UStaticMesh* MeshToSpawn, const FTransform& RelativeTransform);

	virtual TArray<UWeaponState*> GetWeapons() override final;
	virtual void RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister) override;

protected:
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;

	// ---- Begin Override IWeaponOwner ----
	virtual void OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon) override;
	virtual void OnWeaponBehaviourChangesRange(const UWeaponState* WeaponState, const float NewRange) override;
	virtual FVector& GetFireDirection(const int32 WeaponIndex) override final;
	virtual FVector& GetTargetLocation(const int32 WeaponIndex) override;
	virtual bool AllowWeaponToReload(const int32 WeaponIndex) const override;
	virtual void OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor) override;
	virtual void PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode) override;
	virtual void OnReloadStart(const int32 WeaponIndex, const float ReloadTime) override;
	virtual void OnReloadFinished(const int32 WeaponIndex) override;
	virtual void OnProjectileHit(const bool bBounced) override;
	virtual void ForceSetAllWeaponsFullyReloaded() override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) override;

	UFUNCTION(BlueprintCallable)
	virtual void SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters) override;

	// ---- End Override IWeaponOwner ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAttachedWeaponAbilitySettings M_AttachedWeaponAbilitySettings;

private:
	void BeginPlay_AddAbility();
	void AddAbilityToCommands();
	bool GetIsValidOwnerCommandsInterface() const;

	void StartFireLoop();
	void HandleFireLoop();
	void HandleStopFiring();
	void ClearFireTimers();
	void ForceReloadAllWeapons();
	void StopAllWeaponsFire();
	void UpdateAbilityRangeFromWeapons();

	void ProcessPendingWeaponSetups();
	void ProcessPendingDirectHitWeapons();
	void ProcessPendingTraceWeapons();
	void ProcessPendingMultiTraceWeapons();
	void ProcessPendingLaserWeapons();
	void ProcessPendingMultiHitLaserWeapons();
	void ProcessPendingFlameThrowerWeapons();
	void ProcessPendingProjectileWeapons();
	void ProcessPendingRocketWeapons();
	void ProcessPendingMultiProjectileWeapons();
	void ProcessPendingArchProjectileWeapons();
	void ReportMissingInit(const FString& SetupFunctionName) const;

	void BeginPlay_SetupCallbackToProjectileManager();
	void OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager);
	void SetupProjectileManagerIfReady(UWeaponState* Weapon);
	bool GetIsValidProjectileManager() const;

	bool TryPrepareWeaponParameters(FInitWeaponStateDirectHit& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStatTrace& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStateMultiTrace& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStateProjectile& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStateRocketProjectile& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStateMultiProjectile& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStateArchProjectile& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStateLaser& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStateMultiHitLaser& WeaponParameters, const FString& FunctionName);
	bool TryPrepareWeaponParameters(FInitWeaponStateFlameThrower& WeaponParameters, const FString& FunctionName);

	bool PrepareExistingMeshParameters(UMeshComponent*& MeshComponent, const FString& FunctionName) const;
	bool PrepareSpawnedMeshParameters(UMeshComponent*& MeshComponent, const FString& FunctionName) const;

	bool GetIsValidExistingMesh() const;
	bool GetIsValidSpawnedMesh() const;

	void SetupDirectHitWeaponInternal(const FInitWeaponStateDirectHit& DirectHitWeaponParameters);
	void SetupTraceWeaponInternal(const FInitWeaponStatTrace& TraceWeaponParameters);
	void SetupMultiTraceWeaponInternal(const FInitWeaponStateMultiTrace& MultiTraceWeaponParameters);
	void SetupLaserWeaponInternal(const FInitWeaponStateLaser& LaserWeaponParameters);
	void SetupMultiHitLaserWeaponInternal(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters);
	void SetupFlameThrowerWeaponInternal(const FInitWeaponStateFlameThrower& FlameWeaponParameters);
	void SetupProjectileWeaponInternal(const FInitWeaponStateProjectile& ProjectileWeaponParameters);
	void SetupRocketProjectileWeaponInternal(const FInitWeaponStateRocketProjectile& RocketProjectileParameters);
	void SetupMultiProjectileWeaponInternal(const FInitWeaponStateMultiProjectile& MultiProjectileState);
	void SetupArchProjectileWeaponInternal(const FInitWeaponStateArchProjectile& ArchProjParameters);

	void CacheWeaponOwnerInParameters(FInitWeaponStateDirectHit& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStatTrace& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStateMultiTrace& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStateProjectile& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStateRocketProjectile& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStateMultiProjectile& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStateArchProjectile& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStateLaser& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStateMultiHitLaser& WeaponParameters);
	void CacheWeaponOwnerInParameters(FInitWeaponStateFlameThrower& WeaponParameters);

	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	UPROPERTY()
	TArray<TObjectPtr<UWeaponState>> M_AttachedWeapons;

	TArray<FInitWeaponStateDirectHit> M_PendingDirectHitWeapons;
	TArray<FInitWeaponStatTrace> M_PendingTraceWeapons;
	TArray<FInitWeaponStateMultiTrace> M_PendingMultiTraceWeapons;
	TArray<FInitWeaponStateLaser> M_PendingLaserWeapons;
	TArray<FInitWeaponStateMultiHitLaser> M_PendingMultiHitLaserWeapons;
	TArray<FInitWeaponStateFlameThrower> M_PendingFlameThrowerWeapons;
	TArray<FInitWeaponStateProjectile> M_PendingProjectileWeapons;
	TArray<FInitWeaponStateRocketProjectile> M_PendingRocketWeapons;
	TArray<FInitWeaponStateMultiProjectile> M_PendingMultiProjectileWeapons;
	TArray<FInitWeaponStateArchProjectile> M_PendingArchProjectileWeapons;

	EAttachedWeaponAbilityMeshSetup M_WeaponMeshSetupState = EAttachedWeaponAbilityMeshSetup::Uninitialized;

	TWeakObjectPtr<UMeshComponent> M_ExistingWeaponMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_SpawnedWeaponMesh;

	UPROPERTY()
	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;

	bool bM_HasProjectileManager = false;

	FWeaponTargetingData M_TargetingData;
	FAttachedWeaponAbilityFireState M_FireState;

	FVector M_TargetDirectionVectorWorldSpace = FVector::ForwardVector;

	float M_AbilityRange = 0.0f;
	int32 M_OwningPlayer = INDEX_NONE;

	static constexpr float M_FireLoopIntervalSeconds = 0.1f;
};
