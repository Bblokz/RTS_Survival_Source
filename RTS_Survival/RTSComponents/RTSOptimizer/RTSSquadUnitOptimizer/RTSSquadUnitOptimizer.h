// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RTSOptimizer/RTSOptimizer.h"
#include "RTSSquadUnitOptimizer.generated.h"

USTRUCT()
struct FRTSOptimizeCharacterMovement 
{
	GENERATED_BODY()
	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> CharacterMovementComponent = nullptr;

	UPROPERTY()
	float BaseTickInterval = 0.f;
	
	UPROPERTY()
	float CloseOutFovInterval = 0.f;

	UPROPERTY()
	float FarOutFovInterval = 0.f;

	UPROPERTY()
	float BaseMaxSimulationTimeStep = 0.f;

	UPROPERTY()
	int32 BaseMaxSimulationIterations = 0;

	UPROPERTY()
	float OutFOVClose_MaxSimulationTimeStep = 0.f;

	UPROPERTY()
	int32 OutFOVClose_MaxSimulationIterations = 0;

	UPROPERTY()
	float OutFOVFar_MaxSimulationTimeStep = 0.f;

	UPROPERTY()
	int32 OutFOVFar_MaxSimulationIterations = 0;
	
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSSquadUnitOptimizer : public URTSOptimizer
{
	GENERATED_BODY()

public:
	URTSSquadUnitOptimizer();

protected:
	virtual void BeginPlay() override;

	virtual void InFOVUpdateComponents() override;
	virtual void OutFovCloseUpdateComponents() override;
	virtual void OutFovFarUpdateComponents() override;

virtual void DetermineCharMovtOptimization(UCharacterMovementComponent* CharacterMovementComponent) override;
private:
	
	UPROPERTY()
	FRTSOptimizeCharacterMovement M_MvtOptimizationData = {};
	
};
