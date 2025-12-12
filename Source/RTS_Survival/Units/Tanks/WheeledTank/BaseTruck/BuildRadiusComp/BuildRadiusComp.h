// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/RadiusComp/RadiusComp.h"
#include "BuildRadiusComp.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UBuildRadiusComp : public URadiusComp
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UBuildRadiusComp();


	/** Gets or creates the dynamic material instance
     * This function is used by  the PlayerBuildRadiusManager to set up the material parameters of all other radii nearby
     * the dynamic material allows for blending multiple radii together.
     */
    UMaterialInstanceDynamic* GetOrCreateDynamicMaterialInstance();
	

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
    /** The dynamic material instance for the radius mesh */
    UPROPERTY()
    UMaterialInstanceDynamic* M_DynamicMaterialInstance;
	

};
