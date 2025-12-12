// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "WallActor.generated.h"

class URTSComponent;

UCLASS()
class RTS_SURVIVAL_API AWallActor : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWallActor(const FObjectInitializer& ObjectInitializer);

	
	// Contains owning player.
	UFUNCTION(BlueprintCallable)
	inline URTSComponent* GetRTSComponent() const { return RTSComponent; };
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupWallCollision(UStaticMeshComponent* WallMeshComponent,
		const bool bAffectNavMesh);

	virtual void PostInitializeComponents() override;

	// Contains OwningPlayer and UnitType.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "References")
	URTSComponent* RTSComponent;
private:
	bool GetIsValidRTSComponent()const;

};
