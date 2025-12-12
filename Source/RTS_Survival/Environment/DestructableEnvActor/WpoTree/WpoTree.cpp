// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WpoTree.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "WpoSettings/WpoDistanceManager.h"


AWpoTree::AWpoTree(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AWpoTree::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWpoTree::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	WpoDistanceManager = FindComponentByClass<UWpoDistanceManager>();
	(void)EnsureIsValidWpoDistanceManager();
}

bool AWpoTree::EnsureIsValidWpoDistanceManager() const
{
	if (not IsValid(WpoDistanceManager))
	{
		RTSFunctionLibrary::ReportError("invalid wpo distance manager"
			"\n for tree: " + GetName() +
			"\n Add it and call SetupWpoDistance on a static mesh component with it.");
		return false;
	}
	return true;
}
