// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WpoDistanceManager.h"

#include "RTS_Survival/Environment/DestructableEnvActor/WpoTree/WpoTree.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


UWpoDistanceManager::UWpoDistanceManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UWpoDistanceManager::SetupWpoDistance(UStaticMeshComponent* MeshComponent) const
{
	if (not IsValid(MeshComponent))
	{
		RTSFunctionLibrary::ReportError("invalid mesh for wpo distance manager");
		return;
	}
	// todo this function is not implemented by epic....
	MeshComponent->SetWorldPositionOffsetDisableDistance(WpoSettings.WpoDistanceDisable);
}


void UWpoDistanceManager::BeginPlay()
{
	Super::BeginPlay();
	if constexpr (DeveloperSettings::Debugging::GWpoTreeAndFoliage_Compile_DebugSymbols)
	{
		AWpoTree* TreeOwner = Cast<AWpoTree>(GetOwner());
		if (not TreeOwner)
		{
			return;
		}
		WpoTreeDebugger.StartDebugging(TreeOwner, WpoSettings.WpoDistanceDisable);
	}
}

void UWpoDistanceManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if constexpr (DeveloperSettings::Debugging::GWpoTreeAndFoliage_Compile_DebugSymbols)
	{
		WpoTreeDebugger.StopDebugging();
	}
}
