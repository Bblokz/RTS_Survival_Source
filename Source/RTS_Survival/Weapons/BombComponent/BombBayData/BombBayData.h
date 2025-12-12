#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Weapons/BombComponent/BombActor/BombSettings/BombSettings.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

#include "BombBayData.generated.h"


class ABombActor;

USTRUCT(BlueprintType)
struct FBombBayEntry
{
	GENERATED_BODY()

	void InitBombInstancedIndex(const int32 Index);
	bool EnsureEntryHasSocketSet(const UObject* Owner) const;
	bool EnsureEntryIsValid(const UObject* Owner)const;
	inline bool IsBombArmed() const { return bIsArmed; }
	inline int32 GetBombInstancerIndex() const { return HidableInstancedMeshIndex; }
	void SetBombArmed(const bool bArmed);
	void SetBombActor(ABombActor* Bomb);
	inline ABombActor* GetBombActor() const {return M_Bomb;};
	void SetBombInstancerIndex(const int32 Index);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName SocketName = NAME_None;

private:
	UPROPERTY()
	bool bIsArmed = false;	

	// The index of this bomb entry on the URTSHidableInstancedStaticMeshComponent.
	UPROPERTY()
	int32 HidableInstancedMeshIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<ABombActor> M_Bomb;
};

USTRUCT(BlueprintType)
struct FBombBayData
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FBombBayEntry> TBombEntries;

	int32 GetBombsRemaining()const;
	int32 GetMaxBombs()const;

	// To add offset from their sockets on the bombs.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector LocalOffsetPerBomb = FVector::ZeroVector;
	
	bool GetFirstArmedBomb(FBombBayEntry*& OutBombEntry);
	// Checks if mesh is set as well if all bomb bay entries have their sockets set.
	bool EnsureBombBayIsProperlyInitialized(UObject* Owner);
	void DebugBombBayState()const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (ClampMin = 1), Category="Throw Settings")
	int32 BombsThrownPerInterval_Min = 1;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (ClampMin = 1), Category="Throw Settings")
	int32 BombsThrownPerInterval_Max = 1;

	// How long between bomb throws.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		meta = (ClampMin = 0.1), Category="Throw Settings")
	float BombInterval = 0.5;
	
	// To identify the stats of this bomb weapon.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EWeaponName WeaponName;
	
	// Mesh used on bomb instances.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bomb Settings")
	UStaticMesh* BombMesh = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bomb Settings")
	FBombFXSettings BombFXSettings;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bomb Settings")
	float GravityForceMlt = 1.0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bomb Settings")
	float BombStartingSpeed = 300.f;
	
	UPROPERTY()
	uint8 OwningPlayer = 0;

	UPROPERTY()
	FTimerHandle M_BombingTimer;

	// used for the bomb tracker UI.	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI Settings")
	UMaterialInterface* BombIconMaterial = nullptr;

};
