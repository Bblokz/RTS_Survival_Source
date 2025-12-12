// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RTS_Survival/Navigation/RTSNavAI/IRTSNavAI.h"
#include "AISquadUnit.generated.h"

UCLASS()
class RTS_SURVIVAL_API AAISquadUnit : public AAIController, public IRTSNavAIInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAISquadUnit();


	virtual void SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter) override;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* InPawn) override;

private:
	void OnPosses_SetupNavAgent(APawn* InPawn) const;


};
