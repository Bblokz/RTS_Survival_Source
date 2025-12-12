// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "BuildRadiusComp.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"


UBuildRadiusComp::UBuildRadiusComp(): M_DynamicMaterialInstance(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
}

UMaterialInstanceDynamic* UBuildRadiusComp::GetOrCreateDynamicMaterialInstance()
{
	if (!IsValid(RadiusMeshComponent))
	{
		return nullptr;
	}

	if (!IsValid(M_DynamicMaterialInstance))
	{
		// Create a dynamic material instance
		UMaterialInterface* Currentmat = RadiusMeshComponent->GetMaterial(0);
		if (!IsValid(Currentmat))
		{
			// You should assign a default base material if not set
			RTSFunctionLibrary::ReportError(
				"Current material is invalid in URadiusComp::GetOrCreateDynamicMaterialInstance");
			return nullptr;
		}

		M_DynamicMaterialInstance = UMaterialInstanceDynamic::Create(Currentmat, this);
		RadiusMeshComponent->SetMaterial(0, M_DynamicMaterialInstance);
	}

	return M_DynamicMaterialInstance;
}


void UBuildRadiusComp::BeginPlay()
{
	Super::BeginPlay();
}
