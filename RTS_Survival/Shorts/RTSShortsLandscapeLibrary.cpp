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

bool URTSShortsLandscapeLibrary::ImportStageHeights(AActor* LandscapeActor, const FString& HeightFile)
{
#if WITH_EDITOR
	ALandscape* Landscape = Cast<ALandscape>(LandscapeActor);
	if (not IsValid(Landscape) || Landscape->CanHaveLayersContent())
	{
		UE_LOG(LogRTSShortsLandscape, Error, TEXT("ImportStageHeights: requires a landscape without edit layers."));
		return false;
	}
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (not IsValid(LandscapeInfo))
	{
		return false;
	}
	int32 MinimumX = 0;
	int32 MinimumY = 0;
	int32 MaximumX = 0;
	int32 MaximumY = 0;
	if (not LandscapeInfo->GetLandscapeExtent(MinimumX, MinimumY, MaximumX, MaximumY))
	{
		return false;
	}
	const int64 VertexCount = (static_cast<int64>(MaximumX) - MinimumX + 1) * (static_cast<int64>(MaximumY) - MinimumY + 1);
	TArray<uint8> HeightBytes;
	if (VertexCount <= 0 || VertexCount > MAX_int32 / sizeof(uint16)
		|| not FFileHelper::LoadFileToArray(HeightBytes, *HeightFile) || HeightBytes.Num() != VertexCount * sizeof(uint16))
	{
		UE_LOG(LogRTSShortsLandscape, Error, TEXT("ImportStageHeights: height file does not match the landscape grid."));
		return false;
	}
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(static_cast<int32>(VertexCount));
	FMemory::Memcpy(Heights.GetData(), HeightBytes.GetData(), HeightBytes.Num());
	Landscape->Modify();
	{
		FHeightmapAccessor<false> HeightAccessor(LandscapeInfo);
		HeightAccessor.SetData(MinimumX, MinimumY, MaximumX, MaximumY, Heights.GetData());
		HeightAccessor.Flush();
	}
	Landscape->PostEditChange();
	Landscape->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

#if WITH_EDITOR
namespace RTSLandscapeRegion
{
	ULandscapeInfo* GetInfo(AActor* LandscapeActor, const TCHAR* Caller)
	{
		ALandscape* Landscape = Cast<ALandscape>(LandscapeActor);
		ULandscapeInfo* Info = IsValid(Landscape) ? Landscape->GetLandscapeInfo() : nullptr;
		if (not IsValid(Info))
		{
			UE_LOG(LogRTSShortsLandscape, Error, TEXT("%s: needs a landscape actor with landscape info."), Caller);
		}
		return Info;
	}

	bool GetRegionSize(const int32 MinX, const int32 MinY, const int32 MaxX, const int32 MaxY, int32& OutSizeX,
	                   int32& OutSizeY, const TCHAR* Caller)
	{
		OutSizeX = MaxX - MinX + 1;
		OutSizeY = MaxY - MinY + 1;
		if (OutSizeX <= 0 || OutSizeY <= 0 || static_cast<int64>(OutSizeX) * OutSizeY > MAX_int32 / 4)
		{
			UE_LOG(LogRTSShortsLandscape, Error, TEXT("%s: invalid rectangle %d %d %d %d."), Caller, MinX, MinY, MaxX, MaxY);
			return false;
		}
		return true;
	}

	bool LoadExact(TArray<uint8>& OutBytes, const FString& File, const int64 ExpectedBytes, const TCHAR* Caller)
	{
		if (not FFileHelper::LoadFileToArray(OutBytes, *File) || OutBytes.Num() != ExpectedBytes)
		{
			UE_LOG(LogRTSShortsLandscape, Error, TEXT("%s: %s must hold %lld bytes."), Caller, *File, ExpectedBytes);
			return false;
		}
		return true;
	}
}
#endif

bool URTSShortsLandscapeLibrary::ExportLandscapeRegion(AActor* LandscapeActor, const int32 MinX, const int32 MinY,
                                                       const int32 MaxX, const int32 MaxY, const FString& HeightFile,
                                                       const TArray<ULandscapeLayerInfoObject*>& Layers,
                                                       const TArray<FString>& LayerFiles)
{
#if WITH_EDITOR
	ULandscapeInfo* Info = RTSLandscapeRegion::GetInfo(LandscapeActor, TEXT("ExportLandscapeRegion"));
	int32 SizeX = 0;
	int32 SizeY = 0;
	if (not Info || Layers.Num() != LayerFiles.Num()
		|| not RTSLandscapeRegion::GetRegionSize(MinX, MinY, MaxX, MaxY, SizeX, SizeY, TEXT("ExportLandscapeRegion")))
	{
		return false;
	}
	FLandscapeEditDataInterface Edit(Info);
	TArray<uint16> Heights;
	Heights.SetNumZeroed(SizeX * SizeY);
	Edit.GetHeightDataFast(MinX, MinY, MaxX, MaxY, Heights.GetData(), SizeX);
	if (not FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Heights.GetData()),
	                                                             Heights.Num() * sizeof(uint16)), *HeightFile))
	{
		return false;
	}
	for (int32 Index = 0; Index < Layers.Num(); ++Index)
	{
		if (not IsValid(Layers[Index]))
		{
			UE_LOG(LogRTSShortsLandscape, Error, TEXT("ExportLandscapeRegion: layer %d has no layer info."), Index);
			return false;
		}
		TArray<uint8> Weights;
		Weights.SetNumZeroed(SizeX * SizeY);
		Edit.GetWeightDataFast(Layers[Index], MinX, MinY, MaxX, MaxY, Weights.GetData(), SizeX);
		if (not FFileHelper::SaveArrayToFile(Weights, *LayerFiles[Index]))
		{
			return false;
		}
	}
	return true;
#else
	return false;
#endif
}

bool URTSShortsLandscapeLibrary::ImportLandscapeRegion(AActor* LandscapeActor, const int32 MinX, const int32 MinY,
                                                       const int32 MaxX, const int32 MaxY, const FString& HeightFile,
                                                       const TArray<ULandscapeLayerInfoObject*>& Layers,
                                                       const TArray<FString>& LayerFiles)
{
#if WITH_EDITOR
	ALandscape* Landscape = Cast<ALandscape>(LandscapeActor);
	ULandscapeInfo* Info = RTSLandscapeRegion::GetInfo(LandscapeActor, TEXT("ImportLandscapeRegion"));
	int32 SizeX = 0;
	int32 SizeY = 0;
	if (not Info || Layers.Num() != LayerFiles.Num()
		|| not RTSLandscapeRegion::GetRegionSize(MinX, MinY, MaxX, MaxY, SizeX, SizeY, TEXT("ImportLandscapeRegion")))
	{
		return false;
	}
	if (Landscape->CanHaveLayersContent())
	{
		UE_LOG(LogRTSShortsLandscape, Error, TEXT("ImportLandscapeRegion: requires a landscape without edit layers."));
		return false;
	}
	const int64 VertexCount = static_cast<int64>(SizeX) * SizeY;
	TArray<uint8> HeightBytes;
	if (not HeightFile.IsEmpty()
		&& not RTSLandscapeRegion::LoadExact(HeightBytes, HeightFile, VertexCount * 2, TEXT("ImportLandscapeRegion")))
	{
		return false;
	}
	TArray<TArray<uint8>> LayerWeights;
	for (int32 Index = 0; Index < Layers.Num(); ++Index)
	{
		if (not IsValid(Layers[Index]))
		{
			UE_LOG(LogRTSShortsLandscape, Error, TEXT("ImportLandscapeRegion: layer %d has no layer info."), Index);
			return false;
		}
		TArray<uint8>& Weights = LayerWeights.AddDefaulted_GetRef();
		if (not RTSLandscapeRegion::LoadExact(Weights, LayerFiles[Index], VertexCount, TEXT("ImportLandscapeRegion")))
		{
			return false;
		}
	}
	Landscape->Modify();
	{
		FLandscapeEditDataInterface Edit(Info);
		if (HeightBytes.Num() > 0)
		{
			Edit.SetHeightData(MinX, MinY, MaxX, MaxY, reinterpret_cast<const uint16*>(HeightBytes.GetData()), SizeX, true);
		}
		for (int32 Index = 0; Index < Layers.Num(); ++Index)
		{
			Edit.SetAlphaData(Layers[Index], MinX, MinY, MaxX, MaxY, LayerWeights[Index].GetData(), SizeX,
			                  ELandscapeLayerPaintingRestriction::None, false, false);
		}
		Edit.Flush();
	}
	Landscape->PostEditChange();
	Landscape->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

int32 URTSShortsLandscapeLibrary::AddLandscapeComponents(AActor* LandscapeActor, const int32 MinComponentX,
                                                         const int32 MinComponentY, const int32 MaxComponentX,
                                                         const int32 MaxComponentY)
{
#if WITH_EDITOR
	ALandscape* Landscape = Cast<ALandscape>(LandscapeActor);
	ULandscapeInfo* Info = RTSLandscapeRegion::GetInfo(LandscapeActor, TEXT("AddLandscapeComponents"));
	if (not Info || Landscape->CanHaveLayersContent() || MaxComponentX < MinComponentX || MaxComponentY < MinComponentY)
	{
		UE_LOG(LogRTSShortsLandscape, Error, TEXT("AddLandscapeComponents: needs a landscape without edit layers and a valid rectangle."));
		return -1;
	}
	Info->Modify();
	Landscape->Modify();
	TArray<ULandscapeComponent*> NewComponents;
	for (int32 ComponentY = MinComponentY; ComponentY <= MaxComponentY; ++ComponentY)
	{
		for (int32 ComponentX = MinComponentX; ComponentX <= MaxComponentX; ++ComponentX)
		{
			const FIntPoint Key(ComponentX, ComponentY);
			if (Info->XYtoComponentMap.Contains(Key))
			{
				continue;
			}
			const FIntPoint ComponentBase = Key * Info->ComponentSizeQuads;
			ULandscapeComponent* Component = NewObject<ULandscapeComponent>(Landscape, NAME_None, RF_Transactional);
			Component->Init(ComponentBase.X, ComponentBase.Y, Landscape->ComponentSizeQuads, Landscape->NumSubsections,
			                Landscape->SubsectionSizeQuads);
			const int32 ComponentVerts = (Component->SubsectionSizeQuads + 1) * Component->NumSubsections;
			TArray<FColor> HeightData;
			HeightData.AddZeroed(FMath::Square(ComponentVerts));
			Component->InitHeightmapData(HeightData, true);
			Component->UpdateMaterialInstances();
			Info->XYtoComponentMap.Add(Key, Component);
			Info->XYtoAddCollisionMap.Remove(Key);
			NewComponents.Add(Component);
		}
	}
	for (ULandscapeComponent* Component : NewComponents)
	{
		Component->RegisterComponent();
		Component->UpdateCachedBounds();
		Component->UpdateBounds();
		Component->MarkRenderStateDirty();
	}
	Landscape->PostEditChange();
	Landscape->MarkPackageDirty();
	UE_LOG(LogRTSShortsLandscape, Display, TEXT("AddLandscapeComponents: added %d components."), NewComponents.Num());
	return NewComponents.Num();
#else
	return -1;
#endif
}

FString URTSShortsLandscapeLibrary::GetLandscapeGridInfo(AActor* LandscapeActor)
{
#if WITH_EDITOR
	ULandscapeInfo* Info = RTSLandscapeRegion::GetInfo(LandscapeActor, TEXT("GetLandscapeGridInfo"));
	int32 MinX = 0;
	int32 MinY = 0;
	int32 MaxX = 0;
	int32 MaxY = 0;
	if (not Info || not Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		return FString();
	}
	return FString::Printf(TEXT("%d %d %d %d %d"), MinX, MinY, MaxX, MaxY, Info->ComponentSizeQuads);
#else
	return FString();
#endif
}

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
