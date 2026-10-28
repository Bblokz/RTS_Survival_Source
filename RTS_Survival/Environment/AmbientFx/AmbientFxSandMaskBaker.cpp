// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only: the console command RTS.AmbientFx.BakeSandMask bakes which parts of the open map are sand into
// <SandMaskFolder>/SandMask_<MapName> (UAmbientFxSandMask), read by ULandscapeAmbientFxSubsystem at runtime.
// The landscape layers carry no physical materials, so a runtime trace cannot tell sand from grass or rock; the
// paint-layer weights can, but they only exist in the editor. Bake again after repainting a map's landscape.

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "CoreMinimal.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Environment/AmbientFx/AmbientFxSandMask.h"
#include "RTS_Survival/Environment/AmbientFx/RTSAmbientFxSettings.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSandMaskBake, Log, All);

namespace AmbientFxSandMaskBaker
{
	// Same cap as the runtime grid, so a huge landscape gets coarser cells instead of a huge mask.
	constexpr int32 MaxCells = 16384;
	constexpr float SandCoverageThreshold = 0.5f;
	constexpr float TraceUp = 50000.f;
	constexpr float TraceDown = 100000.f;
	// Samples per cell: the centre and four points a quarter cell away from it.
	const FVector2D SampleOffsets[] = {
		FVector2D(0.0, 0.0), FVector2D(-0.25, -0.25), FVector2D(0.25, -0.25), FVector2D(-0.25, 0.25), FVector2D(0.25, 0.25)
	};

	// Decoded weightmaps per component and layer: GetLayerWeightAtLocation decodes the whole weightmap otherwise.
	using FWeightCache = TMap<TPair<const ULandscapeComponent*, const ULandscapeLayerInfoObject*>, TArray<uint8>>;

	/**
	 * @brief Traces onto the landscape and adds up the weights of the sand layers there.
	 * @param World The editor world.
	 * @param Point World XY to sample.
	 * @param TopZ Top of the landscape bounds; the trace starts above it.
	 * @param SandLayers Layer names that count as sand.
	 * @param Cache Decoded weightmaps, reused between samples.
	 * @param OutLayersSeen Receives every layer name found, for the log.
	 * @param bOutHitLandscape Whether there is landscape under the point.
	 * @return Sand coverage at the point, 0 to 1.
	 */
	float GetSandWeightAt(UWorld& World, const FVector2D& Point, const float TopZ, const TArray<FName>& SandLayers,
	                      FWeightCache& Cache, TSet<FName>& OutLayersSeen, bool& bOutHitLandscape)
	{
		FHitResult Hit;
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AmbientFxSandMaskBake), false);
		const FVector Start(Point, TopZ + TraceUp);
		const FVector End(Point, TopZ - TraceDown);
		if (not World.LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE, QueryParams))
		{
			return 0.f;
		}
		const ULandscapeHeightfieldCollisionComponent* Collision =
			Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent());
		ULandscapeComponent* Landscape = Collision != nullptr ? Collision->GetRenderComponent() : nullptr;
		if (Landscape == nullptr)
		{
			return 0.f;
		}
		bOutHitLandscape = true;
		float SandWeight = 0.f;
		for (const FWeightmapLayerAllocationInfo& Allocation : Landscape->GetWeightmapLayerAllocations())
		{
			ULandscapeLayerInfoObject* LayerInfo = Allocation.LayerInfo;
			if (LayerInfo == nullptr)
			{
				continue;
			}
			OutLayersSeen.Add(LayerInfo->LayerName);
			if (not SandLayers.Contains(LayerInfo->LayerName))
			{
				continue;
			}
			TArray<uint8>& LayerCache = Cache.FindOrAdd(TPair<const ULandscapeComponent*, const ULandscapeLayerInfoObject*>(Landscape, LayerInfo));
			SandWeight += Landscape->GetLayerWeightAtLocation(Hit.ImpactPoint, LayerInfo, &LayerCache);
		}
		return SandWeight;
	}

	UAmbientFxSandMask* FindOrCreateMaskAsset(const FSoftObjectPath& MaskPath)
	{
		if (UAmbientFxSandMask* Existing = Cast<UAmbientFxSandMask>(MaskPath.TryLoad()))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*MaskPath.GetLongPackageName());
		UAmbientFxSandMask* Mask = NewObject<UAmbientFxSandMask>(
			Package, FName(*MaskPath.GetAssetName()), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Mask);
		return Mask;
	}

	// A small picture of the mask in Saved/AmbientFx (white = sand, grey = landscape, black = no landscape).
	void WritePreview(const UAmbientFxSandMask& Mask, const TArray<uint8>& bHasLandscape, const FString& MapName)
	{
		TArray<FColor> Pixels;
		Pixels.SetNum(Mask.SandCells.Num());
		for (int32 CellY = 0; CellY < Mask.CellCount.Y; ++CellY)
		{
			for (int32 CellX = 0; CellX < Mask.CellCount.X; ++CellX)
			{
				const int32 CellIndex = CellY * Mask.CellCount.X + CellX;
				const uint8 Value = Mask.SandCells[CellIndex] != 0 ? 255 : (bHasLandscape[CellIndex] != 0 ? 90 : 0);
				// Image rows run top to bottom: flip so +Y is up, as in the top-down editor view.
				Pixels[(Mask.CellCount.Y - 1 - CellY) * Mask.CellCount.X + CellX] = FColor(Value, Value, Value);
			}
		}
		const FString PreviewPath = FPaths::ProjectSavedDir() / TEXT("AmbientFx") / (TEXT("SandMask_") + MapName + TEXT(".bmp"));
		FFileHelper::CreateBitmap(*PreviewPath, Mask.CellCount.X, Mask.CellCount.Y, Pixels.GetData());
	}

	void BakeSandMask()
	{
		UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
		const URTSAmbientFxSettings* Settings = URTSAmbientFxSettings::Get();
		if (World == nullptr || Settings == nullptr)
		{
			UE_LOG(LogRTSSandMaskBake, Error, TEXT("RTS_SAND_MASK RESULT FAIL no editor world"));
			return;
		}
		FBox LandscapeBounds(ForceInit);
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			LandscapeBounds += It->GetComponentsBoundingBox(true);
		}
		const FString MapName = FPackageName::GetShortName(World->GetOutermost());
		if (not LandscapeBounds.IsValid)
		{
			UE_LOG(LogRTSSandMaskBake, Error, TEXT("RTS_SAND_MASK RESULT FAIL map %s has no landscape"), *MapName);
			return;
		}
		const FVector2D BoundsSize(LandscapeBounds.GetSize());
		const float CellSize = FMath::Max(Settings->SandCellSize, FMath::Sqrt(BoundsSize.X * BoundsSize.Y / MaxCells));
		const FIntPoint CellCount(
			FMath::Max(1, FMath::FloorToInt(BoundsSize.X / CellSize)),
			FMath::Max(1, FMath::FloorToInt(BoundsSize.Y / CellSize)));
		const FVector2D Origin(LandscapeBounds.Min);

		TArray<uint8> SandCells;
		TArray<uint8> bHasLandscape;
		SandCells.SetNumZeroed(CellCount.X * CellCount.Y);
		bHasLandscape.SetNumZeroed(CellCount.X * CellCount.Y);
		FWeightCache Cache;
		TSet<FName> LayersSeen;
		int32 SandCount = 0;
		for (int32 CellY = 0; CellY < CellCount.Y; ++CellY)
		{
			for (int32 CellX = 0; CellX < CellCount.X; ++CellX)
			{
				const FVector2D CellCentre = Origin + (FVector2D(CellX, CellY) + 0.5) * CellSize;
				float WeightSum = 0.f;
				bool bHitLandscape = false;
				for (const FVector2D& Offset : SampleOffsets)
				{
					WeightSum += GetSandWeightAt(*World, CellCentre + Offset * CellSize, LandscapeBounds.Max.Z,
					                             Settings->SandLayerNames, Cache, LayersSeen, bHitLandscape);
				}
				const int32 CellIndex = CellY * CellCount.X + CellX;
				bHasLandscape[CellIndex] = bHitLandscape ? 1 : 0;
				const bool bIsSand = WeightSum / UE_ARRAY_COUNT(SampleOffsets) >= SandCoverageThreshold;
				SandCells[CellIndex] = bIsSand ? 1 : 0;
				SandCount += bIsSand ? 1 : 0;
			}
		}

		const FSoftObjectPath MaskPath = Settings->GetSandMaskPath(MapName);
		UAmbientFxSandMask* Mask = FindOrCreateMaskAsset(MaskPath);
		Mask->Modify();
		Mask->Origin = Origin;
		Mask->CellCount = CellCount;
		Mask->CellSize = CellSize;
		Mask->SandCells = MoveTemp(SandCells);
		Mask->MarkPackageDirty();
		WritePreview(*Mask, bHasLandscape, MapName);
		const bool bIsSaved = UEditorLoadingAndSavingUtils::SavePackages({Mask->GetOutermost()}, false);

		FString LayerNames;
		for (const FName& LayerName : LayersSeen)
		{
			LayerNames += LayerName.ToString() + TEXT(" ");
		}
		UE_LOG(LogRTSSandMaskBake, Display,
		       TEXT("RTS_SAND_MASK RESULT DONE map=%s grid=%dx%d cell=%.0f sand=%d of %d layers=[%s] saved=%d asset=%s"),
		       *MapName, CellCount.X, CellCount.Y, CellSize, SandCount, CellCount.X * CellCount.Y,
		       *LayerNames.TrimEnd(), bIsSaved ? 1 : 0, *MaskPath.ToString());
	}

	FAutoConsoleCommand BakeSandMaskCommand(
		TEXT("RTS.AmbientFx.BakeSandMask"),
		TEXT("Bakes which parts of the open map are sand (landscape paint layers in the RTS Ambient Effects settings) into SandMask_<MapName>."),
		FConsoleCommandDelegate::CreateStatic(&BakeSandMask));
}

#endif // WITH_EDITOR
