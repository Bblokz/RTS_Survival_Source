// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSTargetAcquisition.h"
#include "TankTargetAcquisition.generated.h"


class ATrackedTankMaster;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTankTargetAcquisition : public URTSTargetAcquisition
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTankTargetAcquisition();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void IssueAttackClosestVisibleTargetInAggroRange(AActor* TargetActor) override;
	virtual ETargetPreference GetOwnerTargetPreference() const override;
	virtual float GetOwnerRange() const override;
	virtual bool CanAggroEnemies() const override;
	
	
	private:
	UPROPERTY()
	TWeakObjectPtr<ATrackedTankMaster> M_OwningTank;
	[[nodiscard]]bool EnsureIsValidOwningTank()const;
	void OnDigIn_Debugging() const;

};
