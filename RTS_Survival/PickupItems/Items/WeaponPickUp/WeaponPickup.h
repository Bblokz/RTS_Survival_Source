// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/ActionUI/WeaponUI/OnHoverDescription/W_WeaponDescription.h"
#include "RTS_Survival/PickupItems/Items/ItemsMaster.h"
#include "WeaponPickup.generated.h"

enum class EWeaponName : uint32;
class AInfantryWeaponMaster;

UCLASS()
class RTS_SURVIVAL_API AWeaponPickup : public AItemsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWeaponPickup(const FObjectInitializer& ObjectInitializer);
	
	bool EnsurePickupInitialized() const;

	inline TSoftClassPtr<AInfantryWeaponMaster>  GetWeaponClass() const { return WeaponClass; }
	inline EWeaponName GetWeaponName() const { return WeaponName; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<AInfantryWeaponMaster> WeaponClass;	

	// Not Blueprint-visible: EWeaponName is uint32. The details panel still edits it through reflection.
	UPROPERTY(EditDefaultsOnly)
	EWeaponName WeaponName;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnStartOverlap(AActor* OtherActor);
};
