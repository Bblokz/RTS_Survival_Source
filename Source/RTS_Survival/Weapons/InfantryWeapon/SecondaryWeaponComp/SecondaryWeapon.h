// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "SecondaryWeapon.generated.h"


enum class EWeaponName : uint8;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USecondaryWeapon : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USecondaryWeapon();

	void InitializeOwner(TObjectPtr<ASquadUnit> NewOwner);

	bool GetHasSecondaryWeapon() const { return M_WeaponClass != nullptr; }

	TSoftClassPtr<AInfantryWeaponMaster> GetWeaponClass() const { return M_WeaponClass; }
	EWeaponName GetWeaponName() const { return M_WeaponName; }

	void SetupSecondaryWeaponMesh(
		UStaticMesh* WeaponMesh,
		const EWeaponName WeaponName,
		const TSoftClassPtr<AInfantryWeaponMaster>& WeaponClass);

	/**
	 * @brief This function allows to define a secondary weapon on a squad unit without any pickups.
	 * This means that this squad unit is not able to pick up any weapons.
	 * @param OwnerSecondaryWeaponMesh The static mesh component that holds the secondary weapon on the back.
	 * @param NewWeaponName The name of the secondary weapon.
	 * @param NewWeaponClass The class of the secondary weapon.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void ManuallySetSecondaryWeapon(
		UStaticMeshComponent* OwnerSecondaryWeaponMesh,
		const EWeaponName NewWeaponName,
		TSoftClassPtr<AInfantryWeaponMaster>
	                                NewWeaponClass);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<ASquadUnit> M_OwningSquadUnit;

	UPROPERTY()
	TSoftClassPtr<AInfantryWeaponMaster> M_WeaponClass;

	UPROPERTY()
	EWeaponName M_WeaponName;

	UPROPERTY()
	UStaticMeshComponent* M_OwnerSecondaryWeaponMesh;

	// Attempts to setup a static mesh component on the skeletal mesh of the ownign squad unit.
	// if successful, the component is attached to the owning squad unit's secondary weapon socket.
	// and stored in M_OwnerSecondaryWeaponMesh.
	void InitialiseSecondaryWeaponMeshComponent();
	void SetCollisionSettings();
};
