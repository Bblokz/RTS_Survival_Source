// AttachedRockets.h
// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "AttachedRocketsData/AttachedRocketsData.h"
#include "Components/ActorComponent.h"
#include "HideableInstancedStaticMeshComponent/RTSHidableInstancedStaticMeshComponent.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/Weapons/Projectile/ProjectileVfxSettings/ProjectileVfxSettings.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/RocketAbilityTypes.h"
#include "AttachedRockets.generated.h"

struct FProjectileVfxSettings;
enum class ERTSSurfaceType : uint8;
class ICommands;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMeshComponent;
struct FStreamableHandle;

// Find the mesh to attach to based on the socket name or use SetupMeshManually.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAttachedRockets : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttachedRockets();
	void ExecuteFireAllRockets();
	// Triggered if unit goes to other command before finishing fire.
	void TerminateFireAllRockets();
	void CancelFireRockets();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Constructor")
	void SetupMeshManually(UStaticMeshComponent* MeshComp);

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup")
	ERocketAbility RocketAbilityType = ERocketAbility::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Frame")
	FName FrameSocketName = "AttachRockets";

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Frame")
	TSoftObjectPtr<UStaticMesh> FrameMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Frame")
	FVector LocalFrameOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Rockets")
	TSoftObjectPtr<UStaticMesh> RocketMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Frame")
	UMaterialInterface* Optional_RocketMaterial = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Rockets")
	FVector LocalRocketOffsetPerSocket = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Rockets")
	int32 MaxRockets = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Launch")
	USoundBase* RocketLaunchSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Launch")
	USoundAttenuation* RocketLaunchSoundAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Launch")
	USoundConcurrency* RocketLaunchSoundConcurrency = nullptr;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Launch")
	UNiagaraSystem* RocketLaunchEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Launch")
	FVector LaunchScale = FVector(1.0f, 1.0f, 1.0f);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Impact")
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> ImpactSurfaceData;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Impact")
	FVector ImpactScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Impact")
	USoundAttenuation* ImpactAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Impact")
	USoundConcurrency* ImpactConcurrency = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Impact")
	UNiagaraSystem* BounceEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Impact")
	USoundCue* BounceSound = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Setup|Impact")
	FVector BounceScale = FVector(1.0f, 1.0f, 1.0f);

private:
	TWeakInterfacePtr<ICommands> M_Owner;
	UPROPERTY()
	UStaticMeshComponent* M_FrameMeshComponent = nullptr;
	bool EnsureFrameMeshCompIsValid()const;
	UPROPERTY()
	URTSHidableInstancedStaticMeshComponent* M_RocketsInstances = nullptr;
	bool EnsureRocketInstancerIsValid() const;

	UPROPERTY()
	int32 M_OwningPlayer = -1;

	
	FProjectileVfxSettings M_ProjectileVfxSettings;
	
	TArray<FName> M_FrameSocketNames;
	int32 M_NumSockets = 0;
	bool bIsLoadingFrame = false;
	bool bIsLoadingRocket = false;
	TSharedPtr<FStreamableHandle> FrameMeshHandle;
	TSharedPtr<FStreamableHandle> RocketMeshHandle;

	bool EnsureOwnerIsValid() const;
	bool EnsureRocketEnumIsValid() const;

	bool SetupOwningPlayer();
	void BeginPlay_LoadDataFromGameState(const int32 PlayerOwningRockets);
	bool SetupOnOwner_InitInterface();
	void BeginPlay_SetupSystemOnOwner();
	void FindMeshComponentWithSocket();
	void OnFoundMeshComponentForFrame(UMeshComponent* MeshComponent);

	void StartAsyncLoadFrameMesh();
	void OnFrameLoadedAsync();
	void OnFrameSetupManually();

	void StartAsyncLoadRocketMesh();
	void OnRocketLoadedAsync();
	void CreateRocketInstances();

	FAttachedRocketsData M_RocketData;

	void ReportError(const FString& Message) const;

	FRotator ApplyAccuracyDeviation(const FRotator& BaseRotation, const int32 Accuracy) const;

	bool ExecuteFire_EnsureFireReady() const;

	FRotator FireRocket_GetLaunchRotation(const FRotator& SocketRotationInWorld)const;
	void OnFireRocketInSalvo();
	void FireRocket() const;
	void CreateLaunchFX(const FVector& LaunchLocation, const FRotator& LaunchRotation, const int32 RocketInstanceIndex) const;
	bool FireRocket_LaunchPoolProjectile(const FVector& LaunchLocation, const FRotator& LaunchRotation) const;
	void OnSalvoFinished();

	
	/** @brief Registers the OnProjectileManagerLoaded callback to the ProjectileManager.
	 * This registers the reference on the weapon state if that weapon state is of Trace type.
	 */
	void BeginPlay_SetupCallbackToProjectileManager();

	TWeakObjectPtr<ASmallArmsProjectileManager> M_ProjectileManager;
	bool EnsureIsValidProjectileManager() const;
	void OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager);
	float GetDamageFromFlux() const;
	float GetArmorPenFromFlux() const;
	float GetCoolDownFromFlux() const;
	float GetReloadFromFlux() const;
	UPROPERTY()
	FTimerHandle M_CooldownTimerHandle;

	int32 M_RocketsRemainingToFire = 0;

	UPROPERTY()
	FTimerHandle M_ReloadTimerHandle;
	void StartReload();
	void OnReloadFinished() const;
	void SetAllRocketsVisible() const;

	UPROPERTY()
	TMap<int32, FName> M_RocketIndexToSocketName;

	void SetFrameMeshCollision() const;
	void SetRocketInstancesCollision() const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ManuallySetupMesh = nullptr;

	bool bM_WasSetupManually = false;
};
