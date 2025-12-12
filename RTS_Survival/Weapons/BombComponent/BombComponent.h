// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BombBayData/BombBayData.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/Healthbar/AmmoHealthBar/AmmoHpBarTrackerState/FAmmoHpBarTrackerState.h"
#include "RTS_Survival/GameUI/Healthbar/BombHpBarTrackerState/BombHpBarTrackerState.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/HideableInstancedStaticMeshComponent/RTSHidableInstancedStaticMeshComponent.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "BombComponent.generated.h"

/*
 *((SocketName="B_7"),(SocketName="B_0"),(SocketName="B_6"),(SocketName="B_1"),(SocketName="B_5"),(SocketName="B_2"),(SocketName="B_4"),(SocketName="B_3"),(SocketName="B_00"))
 **/

// To keep track of how many bullets are left in the mag.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMagConsumed, int32);

struct FBombFXSettings;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UBombComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UBombComponent();


	/** Fired exactly once per bomb thrown.
	 * Also fires when reloaded.*/
	FOnMagConsumed OnMagConsumed;

	FWeaponData GetBombBayWeaponData() const { return M_WeaponData; }
	FBombBayData GetBombBayData() const { return BombBayData; }
	void ForceReloadAllBombsInstantly();

	void InitBombComponent(UMeshComponent* MeshComponent, const int8 OwningPlayer);

	/** @return true when all bombs are reloaded, false when post this function there is still a bomb to reload */
	bool ReloadSingleBomb();

	/** 
     * Calculates per-bomb reload time and returns how many ReloadSingleBomb() calls 
     * are required to fully reload all bombs.
     * OutReloadTimePerBomb = WeaponData.ReloadSpeed / MaxBombs.
     */
	int32 GetAmountBombsToReload(float& OutReloadTimePerBomb);

	void OnBombKilledActor(AActor* ActorKilled);

	void StartThrowingBombs(AActor* TargetActor);
	void StopThrowingBombs();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bomb Bay Data")
	FBombBayData BombBayData;

private:
	/**
     * @brief Enable and configure UI tracking for bombs.
     * @param Settings Icon material + UV ratio.
     * @param WidgetOwner Actor that owns the UW_BombHealthBar WidgetComponent(s).
     */
	void ConfigureBombAmmoTracking(const FBombTrackerInitSettings& Settings, AActor* WidgetOwner);

	void InitBombTracker();

	
	void StartBombs_InitTimer();
	void KillBombTimer();
	void BombInterval();
	void ThrowBombFromEntry(FBombBayEntry* Entry);
	// The Mesh component of the owner on which we attach our bombs.
	TWeakObjectPtr<UMeshComponent> M_OwnerMeshComponent = nullptr;
	bool EnsureOwnerMeshComponentIsValid() const;

	TWeakObjectPtr<URTSHidableInstancedStaticMeshComponent> M_BombInstances = nullptr;
	/** @pre The owner mesh component and bomb mesh on the BombBayData are both set*/
	bool CreateBombInstancerOnOwnerMesh();
	bool EnsureBombInstancerMeshIsValid() const;

	FWeaponData M_WeaponData;
	void OnInit_InitWeaponData();
	/**
	 * @pre Valid BombEntry data; all defaults should have been set! 
	 */
	void OnInit_SetupAllBombEntries();

	void CreateLaunchSound() const;

	TWeakObjectPtr<AActor> M_TargetActor = nullptr;

	int32 M_ActiveEntries = -1;

	ABombActor* CreateNewBomb(const FVector& Location, const FRotator& Rotation) const;

	ABombActor* EnsureBombActorForEntry(FBombBayEntry& Entry);


	FBombHpBarTrackerState M_BombTrackerState;
};
