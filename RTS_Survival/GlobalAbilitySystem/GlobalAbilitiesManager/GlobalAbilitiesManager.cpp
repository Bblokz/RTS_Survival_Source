// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbilitiesManager.h"


// Sets default values for this component's properties
UGlobalAbilitiesManager::UGlobalAbilitiesManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

}

void UGlobalAbilitiesManager::InitGlobalAbilitiesManager(const int32 OwningPlayer)
{
	M_OwningPlayer = OwningPlayer;
}


void UGlobalAbilitiesManager::BeginPlay()
{
	Super::BeginPlay();

	
}



