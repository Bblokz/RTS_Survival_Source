// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "AircraftCannonPodsComponent.generated.h"

class AAircraftMaster;
class UAircraftWeapon;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UWeaponState;

/** One under-wing gun pod: the visible pod mesh and the aircraft mesh socket its cannon fires from. */
USTRUCT(BlueprintType)
struct FAircraftCannonPod
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMesh> PodMesh = nullptr;

	// Socket on the aircraft's skeletal mesh at the pod's pylon top; the pod keeps world scale 1 whatever the bone scale.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AttachSocketName = NAME_None;

	// Socket on the aircraft's skeletal mesh at the pod's muzzle. The aircraft weapon aims from the socket it fires
	// from, so it must live on the aircraft mesh (a socket on the pod would aim from the aircraft centre).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName MuzzleSocketName = NAME_None;
};

/**
 * @brief Turns an aircraft Blueprint into a gun-pod variant (Ju 87 G): adds the pods under the wings, one projectile
 * cannon per pod firing the configured shell from the pod's muzzle socket, removes the bomb bay and sets the target
 * preference. Add it to a child Blueprint of the base aircraft and fill Pods and CannonWeapon; nothing else is needed.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAircraftCannonPodsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAircraftCannonPodsComponent();

	const TArray<FAircraftCannonPod>& GetPods() const { return Pods; }
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetPodComponents() const { return M_PodComponents; }
	/** @return The cannons this component added to the aircraft weapon, one per pod (empty before they are set up). */
	TArray<UWeaponState*> GetCannons() const;
	EWeaponShellType GetFiredShellType() const { return FiredShellType; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cannon Pods")
	TArray<FAircraftCannonPod> Pods;

	// Weapon, burst, VFX and shell case of every pod's cannon. Owner, weapon owner, mesh and socket are filled per pod.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cannon Pods")
	FInitWeaponStateProjectile CannonWeapon;

	// Added to the cannon's shell types and selected, so the reused weapon data fires this shell.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cannon Pods")
	EWeaponShellType FiredShellType = EWeaponShellType::Shell_APCR;

	// The cannon's range is scaled to this so its shells reach the ground from the dive; 0 keeps the weapon data range.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cannon Pods")
	float CannonRange;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cannon Pods")
	bool bRemoveBombBay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cannon Pods")
	ETargetPreference TargetPreference = ETargetPreference::Tank;

private:
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_PodComponents;

	UPROPERTY()
	TArray<TObjectPtr<UWeaponState>> M_Cannons;

	TWeakObjectPtr<AAircraftMaster> M_Aircraft;

	bool GetIsValidAircraft() const;

	void BeginPlay_RemoveBombBay() const;
	void BeginPlay_CreatePods(USkeletalMeshComponent& AircraftMesh);
	/** The Blueprint sets up its own guns after the components' BeginPlay, with its own owner value: wait a frame. */
	void BeginPlay_QueueCannonSetup();

	void SetupCannons();
	void SetupCannon(UAircraftWeapon& AircraftWeapon, USkeletalMeshComponent& AircraftMesh,
	                 const FAircraftCannonPod& Pod, int32 OwningPlayer);
	/** @return False when the aircraft weapon's Blueprint-callable projectile setup could not be called. */
	bool CallSetupProjectileWeapon(UAircraftWeapon& AircraftWeapon, const FInitWeaponStateProjectile& Parameters) const;
	void SelectFiredShell(UWeaponState& Cannon) const;
	void ApplyCannonRange(UWeaponState& Cannon) const;
};
