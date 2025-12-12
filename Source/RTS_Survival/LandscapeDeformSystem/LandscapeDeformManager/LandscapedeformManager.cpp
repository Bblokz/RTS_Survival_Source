// Copyright (C) Bas Blokzijl - All rights reserved.


#include "LandscapeDeformManager.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "RTS_Survival/LandscapeDeformSystem/LandscapeDeformComponent/LandscapeDeformComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


ALandscapedeformManager::ALandscapedeformManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALandscapedeformManager::AddLandscapeDeformer(ULandscapeDeformComponent* Deformer)
{
	if (not IsValid(Deformer))
	{
		RTSFunctionLibrary::ReportError("Attemtped to register invalid Landscape deformer with manager!"
			"\n at function: ALandscapedeformManager::AddLandscapeDeformer");
		return;
	}
	if (M_Deformers.Contains(Deformer))
	{
		const FString OwnerName = IsValid(Deformer->GetOwner()) ? Deformer->GetOwner()->GetName() : "INVALID OWNER";
		RTSFunctionLibrary::ReportError("Attempted to add duplicate Landscape deformer to manager!"
			"\n at function: ALandscapedeformManager::AddLandscapeDeformer"
			"\n Deformer: " + Deformer->GetName()
			+ "\n Owner: " + OwnerName);
	}
	M_Deformers.Add(Deformer);
	const int32 NewLenght = M_Deformers.Num();
	M_DrawBuffer.SetNum(NewLenght);
}

void ALandscapedeformManager::RemoveLandscapeDeformer(ULandscapeDeformComponent* Deformer)
{
	if (not M_Deformers.Contains(Deformer))
	{
		FString DeformerName = IsValid(Deformer) ? Deformer->GetName() : "INVALID DEFORMER";
		FString DeformerOwnerName = IsValid(Deformer->GetOwner()) ? Deformer->GetOwner()->GetName() : "INVALID OWNER";
		RTSFunctionLibrary::ReportError("Attempted to remove non existing Landscape deformer from manager!"
			"\n at function: ALandscapedeformManager::RemoveLandscapeDeformer"
			"\n Deformer: " + DeformerName
			+ "\n Owner: " + DeformerOwnerName);
		return;
	}
	M_Deformers.Remove(Deformer);
	const int32 NewLenght = M_Deformers.Num();
	M_DrawBuffer.SetNum(NewLenght);
}

void ALandscapedeformManager::BeginPlay()
{
	Super::BeginPlay();
}

void ALandscapedeformManager::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ALandscapedeformManager::Tick);
	Super::Tick(DeltaTime);

	if (not IsValid(M_DeformSystem))
	{
		return;
	}
	for (int i=0; i<M_Deformers.Num(); ++i)
	{
		auto Deformer = M_Deformers[i];
		if (not Deformer.IsValid() || not IsValid(Deformer->GetOwner()))
		{
			RTSFunctionLibrary::ReportError("Invalid Landscape deformer in manager or owner of deformer not valid!"
				"\n at function: ALandscapedeformManager::Tick");
			continue;
		}
		FVector DeformerLocation = Deformer->GetOwner()->GetActorLocation();
		DeformerLocation.Z = Deformer->GetDeformRadius();
		M_DrawBuffer[i] = DeformerLocation;
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		M_DeformSystem, FName(*NiagaraBufferName), M_DrawBuffer);
}
