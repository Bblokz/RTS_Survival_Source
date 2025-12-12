// Copyright (C) Bas Blokzijl - All rights reserved.


#include "DecalManagerSubsystem.h"

#include "RTS_Survival/Game/GameState/GameDecalManager/GameDecalManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UDecalManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDecalManagerSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UDecalManagerSubsystem::SetDecalManager(UGameDecalManager* NewDecalManager)
{
	if(not IsValid(NewDecalManager))
	{
		RTSFunctionLibrary::ReportError("Could not init decal manager in Decalmanager subsystem as it is invalid!");
		return;
	}
	M_DecalManager = NewDecalManager;
}
