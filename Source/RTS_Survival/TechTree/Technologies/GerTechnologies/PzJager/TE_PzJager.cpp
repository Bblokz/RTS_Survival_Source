// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_PzJager.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTE_PzJager::ApplyTechnologyEffect(const UObject* WorldContextObject)
{
	if(DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("apply pz jager effect");
	}
	Super::ApplyTechnologyEffect(WorldContextObject);
}

void UTE_PzJager::OnApplyEffectToActor(AActor* ValidActor)
{
	if(DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Applying pz jager effect to " + ValidActor->GetName());
	}
}
