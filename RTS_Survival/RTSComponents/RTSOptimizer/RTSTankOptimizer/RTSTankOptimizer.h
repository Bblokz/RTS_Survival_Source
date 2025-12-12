// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTSTankOptimizer.generated.h"


USTRUCT()
struct FRTSActorTickOptimizationSettings
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> TickingActor = nullptr;

	UPROPERTY()
	float BaseTickInterval = 0.f;

	UPROPERTY()
	float CloseOutFovInterval = 0.f;

	UPROPERTY()
	float FarOutFovInterval = 0.f;
};
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSTankOptimizer : public URTSOptimizer
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URTSTankOptimizer();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void DetermineOwnerOptimization(AActor* ValidOwner) override;

	virtual void DetermineChildActorCompOptimization(UChildActorComponent* ChildActorComponent) override;

	virtual void InFOVUpdateComponents() override;
	virtual void OutFovCloseUpdateComponents() override;
	virtual void OutFovFarUpdateComponents() override;

private:
	TArray<FRTSActorTickOptimizationSettings> M_TickingActors;

};
