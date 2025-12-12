// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandscapeDeformManager.generated.h"

class UNiagaraComponent;
class ULandscapeDeformComponent;

UCLASS()
class RTS_SURVIVAL_API ALandscapedeformManager : public AActor
{
	GENERATED_BODY()

public:
	ALandscapedeformManager();

	virtual void Tick(float DeltaTime) override;

	void AddLandscapeDeformer(ULandscapeDeformComponent* Deformer);
	void RemoveLandscapeDeformer(ULandscapeDeformComponent* Deformer);
	

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	TArray<TWeakObjectPtr<ULandscapeDeformComponent>> M_Deformers;

	// Set in derived blueprint.
	UPROPERTY(BlueprintReadWrite)
	UNiagaraComponent* M_DeformSystem = nullptr;

	// Name of the 3 vector buffer in the Niagara system to write the deformer location + radii to.
    UPROPERTY(BlueprintReadWrite, Category = "Settings")
	FString NiagaraBufferName;

private:
    UPROPERTY()
    TArray<FVector> M_DrawBuffer;
	

};
