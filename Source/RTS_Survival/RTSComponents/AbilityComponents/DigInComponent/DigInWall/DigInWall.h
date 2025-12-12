// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInType/DigInType.h"
#include "DigInWall.generated.h"

class ADiginProgressBar;
class UW_DigInProgress;
class UWidgetComponent;
class UDigInComponent;
// Loads one of its wall meshes.
// During construction the building material is set.
// Assumes the static mesh has only one material.
UCLASS()
class RTS_SURVIVAL_API ADigInWall : public AHPActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADigInWall(const FObjectInitializer& ObjectInitializer);

	void StartBuildingWall(const float NewMaxHealth,
	                       const float BuildTime, const uint8 OwningPlayer, UDigInComponent* OwningDigInComponent, const FVector& ScalingFactor);

	void DestroyWall();

protected:
	virtual void PostInitializeComponents() override;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	virtual void UnitDies(const ERTSDeathType DeathType) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EDigInType DigInWallType = EDigInType::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<TSoftObjectPtr<UStaticMesh>> WallMeshOptions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UMaterialInstance* ConstructionMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DigInWall|ProgressBar")
	TSubclassOf<UW_DigInProgress> ProgressBarWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DigInWall|ProgressBar")
	FVector2D ProgressBarSize = FVector2D(100.f, 20.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DigInWall|ProgressBar")
	FVector RelativeWidgetOffset = FVector(0.0f, 0.0f, 450.0f);
	
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnUnitDies();

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	UStaticMeshComponent* WallMeshComponent;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	UStaticMesh* GetChosenMesh() { return M_ChosenMesh; }

	// Only called if the mesh collapse does not result in destorying the actor.
	UFUNCTION(BlueprintImplementableEvent, Category="Health")
	void BP_OnMeshCollapsed();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	float GetWallMultiplier() const { return M_WallMultiplier; }

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	int32 InterpolateDependingOnHealth(const int32 From, const int32 To) const;

private:
	// How often during construction the wall hp is adjusted.
	FTimerHandle M_WallBuildTimerHandle;
	float M_TimeRemaining = 0.0f;

	float MaxHealth = 0.0f;

	void SetupRandomWallMesh();
	bool GetIsValidWallMeshComponent() const;

	void SetupDigInComp(UDigInComponent* OwningDigInComponent);
	TWeakObjectPtr<UDigInComponent> M_OwningDigInComponent;
	bool EnsureIsValidDigInOwner() const;
	void OnLoadedWallMesh(UStaticMesh* LoadedMesh);

	bool EnsureConstructionMaterialIsValid() const;
	UPROPERTY()
	UMaterialInterface* M_WallMaterial;


	bool EnsureWallMaterialIsValid() const;

	void StartBuildTimer(float BuildTime, float Interval, const float StartHpMlt);
	void OnBuildInterval();
	void OnBuildingComplete();

	void CreateProgressBar(const float TotalTime, const float StartPercentage);
	void DestroyProgressBar() const;

	bool EnsureProgressBarClassIsValid()const;

	UPROPERTY()
	TObjectPtr<UStaticMesh> M_ChosenMesh = nullptr;

	// Spawned at our location + Offset, has the world space progressbar rotating to the camera.
	TWeakObjectPtr<ADiginProgressBar> M_SpawnedProgressBarActor;

	
	void SetupWallMlt(const FVector& ScalingFactor);

	// Set to the largest scaler, x, y or z. Used to calculate health and sandbags sizes.
	float M_WallMultiplier =1.f;

	
};
