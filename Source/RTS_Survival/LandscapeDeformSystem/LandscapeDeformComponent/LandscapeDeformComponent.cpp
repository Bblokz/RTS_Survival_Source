// Copyright (C) Bas Blokzijl - All rights reserved.


#include "LandscapeDeformComponent.h"

#include "RTS_Survival/LandscapeDeformSystem/LandscapeDeformManager/LandscapeDeformManager.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values for this component's properties
ULandscapeDeformComponent::ULandscapeDeformComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void ULandscapeDeformComponent::BeginPlay()
{
	if (const auto Manager = FRTS_Statics::GetLandscapeDeformManager(this))
	{
		Manager->AddLandscapeDeformer(this);
	}
	Super::BeginPlay();
}

void ULandscapeDeformComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const auto Manager = FRTS_Statics::GetLandscapeDeformManager(this))
	{
		Manager->RemoveLandscapeDeformer(this);
	}

	Super::EndPlay(EndPlayReason);
}
