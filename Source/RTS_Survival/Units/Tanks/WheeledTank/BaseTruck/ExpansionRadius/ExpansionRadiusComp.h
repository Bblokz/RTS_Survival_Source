// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RadiusComp/RadiusComp.h"
#include "ExpansionRadiusComp.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UExpansionRadiusComp : public URadiusComp
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UExpansionRadiusComp();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


};
