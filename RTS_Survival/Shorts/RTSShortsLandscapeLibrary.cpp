// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsLandscapeLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsLandscape, Log, All);

AActor* URTSShortsLandscapeLibrary::CreatePaintedLandscape(UObject* WorldContextObject, const FVector Location,
                                                           const FVector Scale, const int32 ComponentsX,
                                                           const int32 ComponentsY, const int32 QuadsPerSection,
                                                           UMaterialInterface* Material, const FString& HeightFile,
                                                           const TArray<ULandscapeLayerInfoObject*>& Layers,
                                                           const TArray<FString>& LayerFiles, const FString& Label)
{
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (not World || ComponentsX <= 0 || ComponentsY <= 0 || QuadsPerSection <= 0 || Layers.Num() != LayerFiles.Num())
	{
		UE_LOG(LogRTSShortsLandscape, Error, TEXT("CreatePaintedLandscape: invalid world or arguments"));
		return nullptr;
	}
	const int32 SizeX = ComponentsX * QuadsPerSection + 1;
	const int32 SizeY = ComponentsY * QuadsPerSection + 1;
	const int32 VertexCount = SizeX * SizeY;

	TArray<uint8> HeightBytes;
	if (not FFileHelper::LoadFileToArray(HeightBytes, *HeightFile) || HeightBytes.Num() != VertexCount * 2)
	{
		UE_LOG(LogRTSShortsLandscape, Error, TEXT("CreatePaintedLandscape: %s must hold %d uint16 heights (has %d bytes)"),
		       *HeightFile, VertexCount, HeightBytes.Num());
		return nullptr;
	}
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(VertexCount);
	FMemory::Memcpy(Heights.GetData(), HeightBytes.GetData(), HeightBytes.Num());

	TArray<FLandscapeImportLayerInfo> ImportLayers;
	for (int32 Index = 0; Index < Layers.Num(); ++Index)
	{
		ULandscapeLayerInfoObject* LayerInfo = Layers[Index];
		if (not IsValid(LayerInfo))
		{
			UE_LOG(LogRTSShortsLandscape, Error, TEXT("CreatePaintedLandscape: layer %d has no layer info"), Index);
			return nullptr;
		}
		FLandscapeImportLayerInfo ImportLayer(LayerInfo->LayerName);
		ImportLayer.LayerInfo = LayerInfo;
		if (not FFileHelper::LoadFileToArray(ImportLayer.LayerData, *LayerFiles[Index]) || ImportLayer.LayerData.Num() != VertexCount)
		{
			UE_LOG(LogRTSShortsLandscape, Error, TEXT("CreatePaintedLandscape: %s must hold %d weights"), *LayerFiles[Index], VertexCount);
			return nullptr;
		}
		ImportLayers.Add(MoveTemp(ImportLayer));
	}

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(Heights));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), ImportLayers);

	ALandscape* Landscape = World->SpawnActor<ALandscape>(Location, FRotator::ZeroRotator);
	if (not IsValid(Landscape))
	{
		UE_LOG(LogRTSShortsLandscape, Error, TEXT("CreatePaintedLandscape: spawning the landscape failed"));
		return nullptr;
	}
	// No edit layers: with them the height and weight textures are only baked on later editor ticks, and a script
	// that saves and quits right after the import leaves a landscape that renders black.
	Landscape->bCanHaveLayersContent = false;
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, 1, QuadsPerSection, HeightDataPerLayers, nullptr,
	                  MaterialLayerDataPerLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	auto LogAllocations = [Landscape](const TCHAR* Step)
	{
		const ULandscapeComponent* Component = Landscape->LandscapeComponents.Num() > 0 ? Landscape->LandscapeComponents[0].Get() : nullptr;
		UE_LOG(LogRTSShortsLandscape, Display, TEXT("CreatePaintedLandscape: after %s component 0 has %d weight layers, %d weightmaps"), Step,
		       Component ? Component->GetWeightmapLayerAllocations().Num() : -1, Component ? Component->GetWeightmapTextures().Num() : -1);
	};
	LogAllocations(TEXT("Import"));
	UE_LOG(LogRTSShortsLandscape, Display, TEXT("CreatePaintedLandscape: can have layers %d, has layers %d"),
	       Landscape->CanHaveLayersContent() ? 1 : 0, Landscape->HasLayersContent() ? 1 : 0);
	if (Landscape->CanHaveLayersContent())
	{
		// Edit layers keep the imported weights in the default layer; bake the final textures before the save.
		Landscape->ForceUpdateLayersContent();
		LogAllocations(TEXT("ForceUpdateLayersContent"));
	}

	if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
	{
		LandscapeInfo->UpdateLayerInfoMap(Landscape);
		LogAllocations(TEXT("UpdateLayerInfoMap"));
		for (const FLandscapeImportLayerInfo& ImportLayer : ImportLayers)
		{
			Landscape->AddTargetLayer(ImportLayer.LayerName, FLandscapeTargetLayerSettings(ImportLayer.LayerInfo), false);
			const int32 LayerIndex = LandscapeInfo->GetLayerInfoIndex(ImportLayer.LayerName);
			if (LayerIndex != INDEX_NONE)
			{
				LandscapeInfo->Layers[LayerIndex].LayerInfoObj = ImportLayer.LayerInfo;
			}
		}
	}
	// The component material instances bind the weightmap channels to the material's layers; the editor's New
	// Landscape flow rebuilds them on later ticks, a script that saves right away has to do it here.
	LogAllocations(TEXT("AddTargetLayer"));
	// Import left the components without weightmaps here; write each layer the way the editor's paint tools do.
	if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
	{
		FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
		for (const FLandscapeImportLayerInfo& ImportLayer : ImportLayers)
		{
			LandscapeEdit.SetAlphaData(ImportLayer.LayerInfo, 0, 0, SizeX - 1, SizeY - 1, ImportLayer.LayerData.GetData(), SizeX,
			                           ELandscapeLayerPaintingRestriction::None, false, false);
		}
		LandscapeEdit.Flush();
	}
	LogAllocations(TEXT("SetAlphaData"));
	if (Landscape->CanHaveLayersContent())
	{
		Landscape->ForceUpdateLayersContent();
	}
	Landscape->UpdateAllComponentMaterialInstances(true);
	LogAllocations(TEXT("UpdateAllComponentMaterialInstances"));
	Landscape->PostEditChange();
	LogAllocations(TEXT("PostEditChange"));
	Landscape->SetActorLabel(Label);
	FString MaterialLayers;
	for (const FName& LayerName : ALandscapeProxy::GetLayersFromMaterial(Material))
	{
		MaterialLayers += LayerName.ToString() + TEXT(" ");
	}
	const ULandscapeComponent* FirstComponent = Landscape->LandscapeComponents.Num() > 0 ? Landscape->LandscapeComponents[0].Get() : nullptr;
	FString Allocations;
	if (FirstComponent != nullptr)
	{
		for (const FWeightmapLayerAllocationInfo& Allocation : FirstComponent->GetWeightmapLayerAllocations())
		{
			Allocations += FString::Printf(TEXT("%s@%d.%d "), Allocation.LayerInfo ? *Allocation.LayerInfo->LayerName.ToString() : TEXT("none"),
			                               Allocation.WeightmapTextureIndex, Allocation.WeightmapTextureChannel);
		}
	}
	UE_LOG(LogRTSShortsLandscape, Display,
	       TEXT("CreatePaintedLandscape: %s %dx%d vertices, %d layers; material layers [%s]; component 0 has %d material instances, allocations [%s]"),
	       *Label, SizeX, SizeY, ImportLayers.Num(), *MaterialLayers,
	       FirstComponent ? FirstComponent->MaterialInstances.Num() : -1, *Allocations);
	return Landscape;
#else
	return nullptr;
#endif
}
