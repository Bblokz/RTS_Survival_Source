// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "AITrackTank.generated.h"

class RTS_SURVIVAL_API ATrackedTankMaster;

UCLASS()
class RTS_SURVIVAL_API AAITrackTank : public AAITankMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAITrackTank(FObjectInitializer const& ObjectInitializer);

	ATrackedTankMaster* GetTrackedTankMaster() const;


	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* InPawn) override;

	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr) override;
	


};
