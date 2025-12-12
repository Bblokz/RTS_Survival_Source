// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionTypeSystems.h/ExplosionTypeSystems.h"
#include "GameDecalManager.generated.h"


struct FDecalTypeSystems;
enum class ERTS_DecalType : uint8;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UGameDecalManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UGameDecalManager();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "RTS Decal Manager")
	void InitDecalManager(TArray<FDecalTypeSystems> DecalSystemsData);

	UFUNCTION(BlueprintCallable, Category = "RTS Decal Manager")
	void SpawnRTSDecalAtLocation(const ERTS_DecalType DecalType,
	                             const FVector& SpawnLocation,
	                             const FVector2D& MinMaxSize,
	                             const FVector2D& MinMaxXYOffset, const float LifeTime);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:

	TMap<ERTS_DecalType,TArray<UMaterialInterface*>> M_DecalMaterialsMap;

	
UMaterialInterface* PickRandomDecalForType(const ERTS_DecalType DecalType)	;

};
