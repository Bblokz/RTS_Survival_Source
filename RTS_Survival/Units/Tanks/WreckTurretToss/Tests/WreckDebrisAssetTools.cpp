// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WreckDebrisAssetTools.h"

#if WITH_EDITOR

#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace WreckDebrisAssetToolsPrivate
{
	const TCHAR* const UserNamespacePrefix = TEXT("User.");

	void SetupMeshRenderer(UNiagaraMeshRendererProperties& MeshRenderer, const TArray<UStaticMesh*>& Meshes,
	                       const TArray<float>& MeshScales, const FNiagaraVariable& MaterialParameter,
	                       UMaterialInterface* DefaultMaterial)
	{
		MeshRenderer.Modify();
		MeshRenderer.Meshes.Reset();
		for (int32 MeshIndex = 0; MeshIndex < Meshes.Num(); ++MeshIndex)
		{
			FNiagaraMeshRendererMeshProperties& MeshProperties = MeshRenderer.Meshes.AddDefaulted_GetRef();
			MeshProperties.Mesh = Meshes[MeshIndex];
			MeshProperties.Scale = FVector(MeshScales.IsValidIndex(MeshIndex) ? MeshScales[MeshIndex] : 1.f);
		}
		// Every debris mesh has one material slot; one override covers them all.
		MeshRenderer.bOverrideMaterials = true;
		MeshRenderer.OverrideMaterials.Reset();
		FNiagaraMeshMaterialOverride& MaterialOverride = MeshRenderer.OverrideMaterials.AddDefaulted_GetRef();
		MaterialOverride.ExplicitMat = DefaultMaterial;
		MaterialOverride.UserParamBinding.Parameter = MaterialParameter;
		MeshRenderer.PostEditChange();
	}
}

bool UWreckDebrisAssetTools::SetupDebrisMeshRenderer(UNiagaraSystem* System, const TArray<UStaticMesh*>& Meshes,
                                                     const TArray<float>& MeshScales,
                                                     const FName UserMaterialParameter,
                                                     UMaterialInterface* DefaultMaterial)
{
	using namespace WreckDebrisAssetToolsPrivate;
	if (not IsValid(System) || Meshes.IsEmpty() || not IsValid(DefaultMaterial) || UserMaterialParameter.IsNone())
	{
		RTSFunctionLibrary::ReportError(TEXT("UWreckDebrisAssetTools::SetupDebrisMeshRenderer - invalid input."));
		return false;
	}
	System->Modify();
	const FNiagaraVariable MaterialParameter(FNiagaraTypeDefinition::GetUMaterialDef(),
	                                         FName(UserNamespacePrefix + UserMaterialParameter.ToString()));
	FNiagaraUserRedirectionParameterStore& ExposedParameters = System->GetExposedParameters();
	ExposedParameters.AddParameter(MaterialParameter, true);
	ExposedParameters.SetUObject(DefaultMaterial, MaterialParameter);

	int32 MeshRendererCount = 0;
	for (const FNiagaraEmitterHandle& EmitterHandle : System->GetEmitterHandles())
	{
		const FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (EmitterData == nullptr)
		{
			continue;
		}
		for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
		{
			UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer);
			if (not IsValid(MeshRenderer))
			{
				continue;
			}
			SetupMeshRenderer(*MeshRenderer, Meshes, MeshScales, MaterialParameter, DefaultMaterial);
			++MeshRendererCount;
		}
	}
	System->RequestCompile(false);
	System->WaitForCompilationComplete();
	System->MarkPackageDirty();
	return MeshRendererCount > 0;
}

#endif
