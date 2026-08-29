// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"
#include "RTS_Survival/Weapons/WeaponAIState/WeaponAIState.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"
#include "RTS_Survival/Weapons/WeaponRangeData/WeaponRangeData.h"
#include "RTS_Survival/Weapons/WeaponTargetingData/WeaponTargetingData.h"

#include "StandaloneTurret.generated.h"

class ASmallArmsProjectileManager;
class UAnimStandaloneTurret;
class USkeletalMeshComponent;
class UWeaponState;
struct FInitWeaponStateArchProjectile;
struct FInitWeaponStateDirectHit;
struct FInitWeaponStateHomingMissile;
struct FInitWeaponStateLaser;
struct FInitWeaponStateMultiHitLaser;
struct FInitWeaponStateMultiTrace;
struct FInitWeaponStateProjectile;
struct FInitWeaponStateRailgun;
struct FInitWeaponStateRailwayCannon;
struct FInitWeaponStateRocketProjectile;
struct FInitWeaponStateSplitterArchProjectile;
struct FInitWeaponStateVerticalRocketProjectile;
struct FInitWeaponStatTrace;

/** Designer-facing steering limits for a standalone turret's animation-driven weapon assembly. */
USTRUCT(BlueprintType)
struct FStandaloneTurretRotationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", UIMin="0.0", Units="deg/s"))
	float YawTurnRateDegreesPerSecond = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", UIMin="0.0", Units="deg/s"))
	float PitchTurnRateDegreesPerSecond = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="-89.0", ClampMax="89.0", Units="deg"))
	float MinimumPitchDegrees = -10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="-89.0", ClampMax="89.0", Units="deg"))
	float MaximumPitchDegrees = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", UIMin="0.0", Units="deg"))
	float AllowedAimErrorDegrees = 4.0f;
};

/** Runtime yaw/pitch solution shared by animation steering and weapon fire direction. */
USTRUCT()
struct FStandaloneTurretAimState
{
	GENERATED_BODY()

	UPROPERTY()
	float CurrentYawDegrees = 0.0f;

	UPROPERTY()
	float CurrentPitchDegrees = 0.0f;

	UPROPERTY()
	float TargetYawDegrees = 0.0f;

	UPROPERTY()
	float TargetPitchDegrees = 0.0f;

	UPROPERTY()
	FVector FireDirection = FVector::ForwardVector;

	UPROPERTY()
	bool bIsAlignedToTarget = false;

	UPROPERTY()
	bool bIsTargetWithinPitchLimits = true;
};

/**
 * @brief Placeable static combat actor that owns targeting, steering, weapon state, health, and selection itself.
 * Its root skeletal mesh stays fixed while UAnimStandaloneTurret receives local yaw and pitch for the weapon bones.
 * @note Derived blueprints must add and configure Health, RTS, and Selection components.
 * @note InitSelectionComponent must be called in the derived blueprint to bind its selection area and decal.
 * @note Set the root mesh Anim Class to a UAnimStandaloneTurret-derived animation blueprint with a physics asset for hits.
 * @note Set M_StandaloneTurretSubtype and implement BP_PostInit to call the desired Setup...Weapon functions.
 */
UCLASS()
class RTS_SURVIVAL_API AStandaloneTurret : public ASelectableActorObjectsMaster,
	public IWeaponOwner,
	public IAimOffsetProvider
{
	GENERATED_BODY()

public:
	AStandaloneTurret(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Targeting")
	void SetAutoEngageTargets(bool bUseLastTarget);

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Targeting")
	void SetEngageSpecificTarget(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Targeting")
	void SetEngageGroundLocation(const FVector& GroundLocation);

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Targeting")
	void DisableTurret();

	UFUNCTION(BlueprintPure, Category="Standalone Turret|Targeting")
	AActor* GetCurrentTargetActor() const;

	UFUNCTION(BlueprintPure, Category="Standalone Turret|Weapon")
	float GetMaxWeaponRange() const;

	UFUNCTION(BlueprintPure, Category="Standalone Turret|Weapon")
	int32 GetOwningPlayerForWeaponInit() const;

	UFUNCTION(BlueprintPure, Category="Standalone Turret|Components")
	USkeletalMeshComponent* GetTurretMesh() const;

	virtual TArray<UWeaponState*> GetWeapons() override;
	virtual void RegisterIgnoreActor(AActor* ActorToIgnore, bool bRegister) override;
	virtual void ForceSetAllWeaponsFullyReloaded() override;
	virtual float GetTurretYawLimit() const override;
	virtual ETargetPreference GetTargetPreference() const override;
	virtual void SetTargetPreference(ETargetPreference NewTargetPreference) override;
	virtual void GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const override;
	virtual void PropagateNewTargetPreference(ETargetPreference TargetPreference) override;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void UnitDies(ERTSDeathType DeathType) override;

	virtual void ExecuteAttackCommand(AActor* TargetActor) override;
	virtual void TerminateAttackCommand() override;
	virtual void ExecuteAttackGroundCommand(FVector GroundLocation) override;
	virtual void TerminateAttackGroundCommand() override;
	virtual void SetUnitToIdleSpecificLogic() override;
	virtual void OnUnitIdleAndNoNewCommands() override;

	UFUNCTION(BlueprintImplementableEvent, Category="Standalone Turret|Initialization")
	void BP_PostInit();

	UFUNCTION(BlueprintImplementableEvent, Category="Standalone Turret|Weapon")
	void BP_PlayWeaponAnimation(int32 WeaponIndex, EWeaponFireMode FireMode);

	UFUNCTION(BlueprintImplementableEvent, Category="Standalone Turret|Weapon")
	void BP_ReloadWeapon(int32 WeaponIndex, float ReloadTime);

	UFUNCTION(BlueprintImplementableEvent, Category="Standalone Turret|Weapon")
	void BP_OnReloadFinished(int32 WeaponIndex);

	UFUNCTION(BlueprintImplementableEvent, Category="Standalone Turret|Weapon")
	void BP_OnProjectileHit(bool bBounced);

	UFUNCTION(BlueprintImplementableEvent, Category="Standalone Turret|Health")
	void BP_OnStandaloneTurretDies(ERTSDeathType DeathType);

	virtual void OnWeaponAdded(int32 WeaponIndex, UWeaponState* Weapon) override;
	virtual void OnWeaponBehaviourChangesRange(const UWeaponState* ReportingWeaponState, float NewRange) override;
	virtual FVector& GetFireDirection(int32 WeaponIndex) override;
	virtual FVector& GetTargetLocation(int32 WeaponIndex) override;
	virtual AActor* GetTargetActor(int32 WeaponIndex) const override;
	virtual bool AllowWeaponToReload(int32 WeaponIndex) const override;
	virtual void OnWeaponKilledActor(int32 WeaponIndex, AActor* KilledActor) override;
	virtual void PlayWeaponAnimation(int32 WeaponIndex, EWeaponFireMode FireMode, int32 WeaponCalibre) override;
	virtual void OnReloadStart(int32 WeaponIndex, float ReloadTime) override;
	virtual void OnProjectileHit(bool bBounced) override;
	virtual void OnReloadFinished(int32 WeaponIndex) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	void SetupRailwayCannonWeapon(FInitWeaponStateRailwayCannon RailwayCannonParameters);

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupRailgunWeapon(FInitWeaponStateRailgun RailgunWeaponParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupVerticalRocketProjectileWeapon(
		FInitWeaponStateVerticalRocketProjectile VerticalRocketProjectileParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupHomingMissileWeapon(FInitWeaponStateHomingMissile HomingMissileParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjectileParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjectileParameters) override;

	UFUNCTION(BlueprintCallable, Category="Standalone Turret|Weapon")
	virtual void SetupSplitterArchProjectileWeapon(
		FInitWeaponStateSplitterArchProjectile SplitterArchProjectileParameters) override;

private:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Standalone Turret|Components",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> M_TurretMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Standalone Turret|Rotation",
		meta=(AllowPrivateAccess="true"))
	FStandaloneTurretRotationSettings M_RotationSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Standalone Turret|Unit Data",
		meta=(AllowPrivateAccess="true"))
	EStandaloneTurretSubtype M_StandaloneTurretSubtype = EStandaloneTurretSubtype::StandaloneTurret_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Standalone Turret|Targeting",
		meta=(AllowPrivateAccess="true"))
	ETargetPreference M_TargetPreference = ETargetPreference::Tank;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Standalone Turret|Targeting",
		meta=(AllowPrivateAccess="true", ClampMin="0.0", UIMin="0.0", Units="cm"))
	float M_MinimumTargetRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Standalone Turret|Targeting",
		meta=(AllowPrivateAccess="true"))
	FName M_AimOriginSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Standalone Turret|Targeting",
		meta=(AllowPrivateAccess="true"))
	bool bM_AutoEngageOnBeginPlay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Standalone Turret|Aim Offsets",
		meta=(AllowPrivateAccess="true"))
	TArray<FVector> M_AimOffsetPoints = {
		FVector(0.0f, 0.0f, 100.0f),
		FVector(-50.0f, 0.0f, 100.0f),
		FVector(0.0f, 25.0f, 100.0f),
		FVector(0.0f, -25.0f, 100.0f)
	};

	UPROPERTY()
	TArray<TObjectPtr<UWeaponState>> M_Weapons;

	UPROPERTY()
	FWeaponRangeData M_WeaponRangeData;

	UPROPERTY()
	FWeaponTargetingData M_TargetingData;

	UPROPERTY()
	FStandaloneTurretAimState M_AimState;

	UPROPERTY()
	EWeaponAIState M_WeaponAIState = EWeaponAIState::None;

	UPROPERTY()
	TWeakObjectPtr<UAnimStandaloneTurret> M_AnimInstance;

	UPROPERTY()
	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;

	FTimerHandle M_WeaponSearchHandle;
	bool bM_IsPendingTargetSearch = false;
	int32 M_CachedOwningPlayer = INDEX_NONE;

	static constexpr double M_WeaponSearchTimeInterval = 0.3;
	static constexpr int32 M_TargetSearchResultCount = 3;

	void BeginPlay_InitAnimationInstance();
	void BeginPlay_SetupUnitData();
	void BeginPlay_InitTargetingAndCollision();
	void StartEngagementTimer();
	void UpdateEngagement();
	void UpdateAutoEngage();
	void UpdateSpecificEngage();
	void UpdateGroundEngage();
	void EngageCurrentTarget();
	void HandleInvalidSpecificTarget();
	void RequestClosestTargets();
	void OnTargetsFound(const TArray<AActor*>& Targets);
	void SetTargetActor(AActor* TargetActor);
	void ResetTarget();
	void UpdateAimSolution();
	void UpdateAnimationSteering(float DeltaTime);
	void UpdateAimAlignment();
	FVector GetAimOriginLocation() const;
	bool GetIsTargetWithinWeaponRange(const FVector& TargetLocation) const;
	bool GetIsTargetBelowMinimumRange(const FVector& TargetLocation) const;
	void StopAllWeaponsFire(bool bInvalidateTarget);
	void DisableAllWeapons();
	void FireAllWeapons();
	void AddInitializedWeapon(UWeaponState* Weapon);
	void UpdateWeaponRangeBasedOnWeapons();
	void SetupCallbackToProjectileManager();
	void OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager);
	void SetupTargetableCollision() const;
	void RefreshOwningPlayerConfiguration();
	void CompleteActiveAttackCommand();
	UWorld* GetWeaponSetupWorld(const FString& SetupFunctionName) const;
	bool GetIsValidTurretMesh() const;
	bool GetIsValidAnimInstance() const;
};
