// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthLevels/HealthLevels.h"

#include "HealthLevelDecalComponent.generated.h"

class UDecalComponent;
class UMaterialInterface;
class UMeshComponent;

/** @brief One weighted damage decal and the uniform size range used when it is selected. */
USTRUCT(BlueprintType)
struct FHealthLevelDecalEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals", meta = (ClampMin = "0.0"))
	float MinSize = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals", meta = (ClampMin = "0.0"))
	float MaxSize = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals", meta = (ClampMin = "0.0"))
	float Weight = 1.f;
};

/**
 * @brief Adds persistent damage decals to unused mesh sockets as Blueprint health-level events arrive.
 * The configured severity determines the desired total, so skipped health levels need no special handling.
 * @note InitializeHealthLevelDecals: call in Blueprint once to provide the mesh whose sockets receive decals.
 * @note HandleHealthLevelChanged: connect to the owner's health-changed event after initialisation.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UHealthLevelDecalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthLevelDecalComponent();

	/**
	 * @brief Caches the filtered socket set and resets damage visuals left from any earlier mesh binding.
	 * @param InMeshComponent Same-owner mesh whose socket transforms define decal placement.
	 * @return Whether the mesh, filtered sockets, and weighted decal entries are usable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health Level Decals")
	bool InitializeHealthLevelDecals(UMeshComponent* InMeshComponent);

	/**
	 * @brief Provides a direct endpoint for the health component's Blueprint notification pins.
	 * @param HealthLevel Current threshold reported by the health component.
	 * @param bIsHealing Whether the threshold was reached while health was increasing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Health Level Decals")
	void HandleHealthLevelChanged(EHealthLevel HealthLevel, bool bIsHealing);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals|Decals",
		meta = (ClampMin = "0.0", DisplayName = "Overall Size Multiplier"))
	float M_OverallSizeMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals|Decals",
		meta = (DisplayName = "Decals"))
	TArray<FHealthLevelDecalEntry> M_DecalEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals|Progression",
		meta = (ClampMin = "0", DisplayName = "Amount Of Decals At Start Damage Level"))
	int32 M_AmountOfDecalsAtStartDamageLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals|Progression",
		meta = (ValidEnumValues = "Level_75Percent,Level_66Percent,Level_50Percent,Level_33Percent,Level_25Percent",
			DisplayName = "Start Decal Damage Level"))
	EHealthLevel M_StartDecalDamageLevel = EHealthLevel::Level_75Percent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals|Progression",
		meta = (ClampMin = "0", DisplayName = "Added Amount Of Decals Per Sequential Health Level"))
	int32 M_AddedAmountOfDecalsPerSequentialHealthLevel = 1;

	// When non-empty, only socket names containing this text are eligible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals|Sockets",
		meta = (DisplayName = "Socket Name Contains"))
	FString M_SocketNameContains;

	// When non-empty, only socket names beginning with this text are eligible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Level Decals|Sockets",
		meta = (DisplayName = "Socket Name Prefix"))
	FString M_SocketNamePrefix;

private:
	struct FSpawnedHealthLevelDecal
	{
		FName SocketName = NAME_None;
		TWeakObjectPtr<UDecalComponent> DecalComponent;
	};

	UPROPERTY(Transient)
	TWeakObjectPtr<UMeshComponent> M_MeshComponent;
	bool GetIsValidMeshComponent() const;

	UPROPERTY(Transient)
	TArray<FName> M_EligibleSocketNames;

	UPROPERTY(Transient)
	TSet<FName> M_UsedSocketNames;

	TArray<FSpawnedHealthLevelDecal> M_SpawnedDecals;

	void CacheEligibleSocketNames();
	bool DoesSocketPassFilters(const FName SocketName) const;
	bool GetHasUsableDecalEntry() const;
	int32 GetDesiredDecalCount(const EHealthLevel HealthLevel) const;
	static int32 GetDamageLevelIndex(const EHealthLevel HealthLevel);
	void AddDecalsUpToDesiredCount(const int32 DesiredDecalCount);
	TArray<FName> GetAvailableSocketNames() const;
	int32 PickWeightedDecalEntryIndex() const;
	bool SpawnDecalAtSocket(const FName SocketName, const FHealthLevelDecalEntry& DecalEntry);
	void RemoveInvalidSpawnedDecals();
	void ClearSpawnedDecals();
};
