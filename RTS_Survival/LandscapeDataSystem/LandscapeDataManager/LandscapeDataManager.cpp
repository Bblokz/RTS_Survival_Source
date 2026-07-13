// Copyright (C) Bas Blokzijl - All rights reserved.

#include "LandscapeDataManager.h"

#include "Data/PCGSpatialData.h"
#include "Engine/Level.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PCGComponent.h"
#include "Sound/SoundAttenuation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TextureResource.h"
#include "TimerManager.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

namespace LandscapeDataManagerConstants
{
	constexpr int32 MinimumTextureResolution = 32;
	constexpr int32 MinimumMaximumTextureResolution = 32;
	constexpr int32 AbsoluteMaximumTextureResolution = 4096;
	constexpr int32 PlaceholderTextureResolution = 1;
	constexpr uint8 MaximumChannelValue = 255;
	constexpr float MinimumNormalizedValue = 0.0f;
	constexpr float MaximumNormalizedValue = 1.0f;
	constexpr double MinimumWorldUnitsPerTexel = 1.0;
	constexpr double HalfTexel = 0.5;
	constexpr uint32 TexelsPerMipFilter = 4;
	constexpr float InvalidSourceCleanupIntervalSeconds = 1.0f;

	int32 SanitizeMaximumResolution(const int32 RequestedResolution)
	{
		const int32 ClampedResolution = FMath::Clamp(
			RequestedResolution,
			MinimumMaximumTextureResolution,
			AbsoluteMaximumTextureResolution);
		return 1 << FMath::FloorLog2(ClampedResolution);
	}

	int32 CalculateResolutionForWorldSize(
		const double WorldSize,
		const double DesiredWorldUnitsPerTexel,
		const int32 MaximumResolution)
	{
		const double RequestedResolution = WorldSize / DesiredWorldUnitsPerTexel;
		if (RequestedResolution >= MaximumResolution)
		{
			return MaximumResolution;
		}

		const int32 RequiredResolution = FMath::Max(
			MinimumTextureResolution,
			FMath::CeilToInt32(RequestedResolution));
		const int32 PowerOfTwoResolution = FMath::RoundUpToPowerOfTwo(RequiredResolution);
		return FMath::Min(PowerOfTwoResolution, MaximumResolution);
	}

	FColor GetAverageSourceColor(
		const TArray<FColor>& SourcePixels,
		const FIntPoint& SourceResolution,
		const FIntPoint& DestinationPixel)
	{
		uint32 RedTotal = 0;
		uint32 GreenTotal = 0;
		uint32 BlueTotal = 0;
		uint32 AlphaTotal = 0;
		for (int32 OffsetY = 0; OffsetY < 2; ++OffsetY)
		{
			for (int32 OffsetX = 0; OffsetX < 2; ++OffsetX)
			{
				const int32 SourceX = FMath::Min(DestinationPixel.X * 2 + OffsetX, SourceResolution.X - 1);
				const int32 SourceY = FMath::Min(DestinationPixel.Y * 2 + OffsetY, SourceResolution.Y - 1);
				const FColor& SourceColor = SourcePixels[SourceY * SourceResolution.X + SourceX];
				RedTotal += SourceColor.R;
				GreenTotal += SourceColor.G;
				BlueTotal += SourceColor.B;
				AlphaTotal += SourceColor.A;
			}
		}

		return FColor(
			static_cast<uint8>(RedTotal / TexelsPerMipFilter),
			static_cast<uint8>(GreenTotal / TexelsPerMipFilter),
			static_cast<uint8>(BlueTotal / TexelsPerMipFilter),
			static_cast<uint8>(AlphaTotal / TexelsPerMipFilter));
	}

	TArray<FColor> BuildNextAverageMip(
		const TArray<FColor>& SourcePixels,
		const FIntPoint& SourceResolution,
		FIntPoint& OutResolution)
	{
		OutResolution.X = FMath::Max(1, SourceResolution.X / 2);
		OutResolution.Y = FMath::Max(1, SourceResolution.Y / 2);

		TArray<FColor> Result;
		Result.SetNumZeroed(OutResolution.X * OutResolution.Y);
		for (int32 PixelY = 0; PixelY < OutResolution.Y; ++PixelY)
		{
			for (int32 PixelX = 0; PixelX < OutResolution.X; ++PixelX)
			{
				const FIntPoint DestinationPixel(PixelX, PixelY);
				Result[PixelY * OutResolution.X + PixelX] = GetAverageSourceColor(
					SourcePixels,
					SourceResolution,
					DestinationPixel);
			}
		}
		return Result;
	}

	void AddTextureMip(
		FTexturePlatformData& PlatformData,
		const TArray<FColor>& Pixels,
		const FIntPoint& Resolution)
	{
		FTexture2DMipMap* TextureMip = new FTexture2DMipMap(Resolution.X, Resolution.Y, 1);
		PlatformData.Mips.Add(TextureMip);
		const int64 ByteCount = static_cast<int64>(Pixels.Num()) * sizeof(FColor);
		TextureMip->BulkData.Lock(LOCK_READ_WRITE);
		void* DestinationData = TextureMip->BulkData.Realloc(ByteCount);
		FMemory::Memcpy(DestinationData, Pixels.GetData(), ByteCount);
		TextureMip->BulkData.Unlock();
	}

	bool IsLandscapeProxyForLandscape(const ALandscapeProxy& Proxy, const ALandscape& Landscape)
	{
		return Proxy.GetLandscapeGuid() == Landscape.GetLandscapeGuid();
	}

	bool GetClippedTextureUVBounds(
		const FVector2D& UnclippedMinimumUV,
		const FVector2D& UnclippedMaximumUV,
		FVector2D& OutMinimumUV,
		FVector2D& OutMaximumUV)
	{
		if (UnclippedMaximumUV.X <= MinimumNormalizedValue
			|| UnclippedMaximumUV.Y <= MinimumNormalizedValue
			|| UnclippedMinimumUV.X >= MaximumNormalizedValue
			|| UnclippedMinimumUV.Y >= MaximumNormalizedValue)
		{
			return false;
		}

		OutMinimumUV = FVector2D(
			FMath::Clamp(UnclippedMinimumUV.X, MinimumNormalizedValue, MaximumNormalizedValue),
			FMath::Clamp(UnclippedMinimumUV.Y, MinimumNormalizedValue, MaximumNormalizedValue));
		OutMaximumUV = FVector2D(
			FMath::Clamp(UnclippedMaximumUV.X, MinimumNormalizedValue, MaximumNormalizedValue),
			FMath::Clamp(UnclippedMaximumUV.Y, MinimumNormalizedValue, MaximumNormalizedValue));
		return true;
	}
}

ALandscapeDataManager::ALandscapeDataManager()
{
	PrimaryActorTick.bCanEverTick = false;
	M_CachedCompleteLandscapeBounds = FBox(EForceInit::ForceInit);

#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif
}

void ALandscapeDataManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	if (GetWorld() != nullptr && not GetWorld()->IsGameWorld())
	{
		Editor_RefreshCachedCompleteLandscapeBounds();
	}
#endif
}

#if WITH_EDITOR
void ALandscapeDataManager::PreSave(FObjectPreSaveContext SaveContext)
{
	if (GetWorld() != nullptr && not GetWorld()->IsGameWorld())
	{
		Editor_RefreshCachedCompleteLandscapeBounds();
	}
	Super::PreSave(SaveContext);
}
#endif

ALandscapeDataManager* ALandscapeDataManager::FindUniqueManager(
	const UObject* WorldContextObject,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (not IsInGameThread())
	{
		OutFailureReason = TEXT("LandscapeDataManager lookup must run on the game thread.");
		return nullptr;
	}

	if (not IsValid(WorldContextObject) || WorldContextObject->GetWorld() == nullptr)
	{
		OutFailureReason = TEXT("Landscape Data node could not resolve a valid world.");
		return nullptr;
	}

	ALandscapeDataManager* FoundManager = nullptr;
	int32 ManagerCount = 0;
	for (TActorIterator<ALandscapeDataManager> ManagerIterator(WorldContextObject->GetWorld()); ManagerIterator; ++ManagerIterator)
	{
		ALandscapeDataManager* CandidateManager = *ManagerIterator;
		if (not IsValid(CandidateManager))
		{
			continue;
		}

		FoundManager = CandidateManager;
		++ManagerCount;
	}

	if (ManagerCount == 1)
	{
		return FoundManager;
	}

	OutFailureReason = ManagerCount == 0
		? TEXT("No LandscapeDataManager is placed in this world.")
		: FString::Printf(TEXT("Expected one LandscapeDataManager, but found %d."), ManagerCount);
	return nullptr;
}

void ALandscapeDataManager::BeginPlay()
{
	Super::BeginPlay();
	ValidateLandscapeDataSetup();
}

void ALandscapeDataManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndPlay_ClearPublishedTexture(EndPlayReason);
	StopInvalidSourceCleanupTimer();
	EndPlay_UnbindLevelStreaming();
	UnbindPCGComponents();
	UnbindPCGOwners();
	M_Contributions.Reset();
	M_StagedContributions.Reset();
	M_PendingRemovals.Reset();
	M_CombinedPixels.Reset();
	M_LandscapeDataTexture = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool ALandscapeDataManager::EnsureInitialized()
{
	if (not IsInGameThread())
	{
		RTSFunctionLibrary::ReportError(TEXT("LandscapeDataManager initialization must run on the game thread."));
		return false;
	}

	if (not GetIsUniqueManager(true))
	{
		return false;
	}
	BindLevelStreaming();

	if (bM_IsInitialized
		&& M_TextureMapping.IsValid()
		&& GetIsValidLandscape()
		&& GetIsValidLandscapeDataTexture())
	{
		return true;
	}

	if (not ResolveLandscape(true))
	{
		return false;
	}

#if WITH_EDITOR
	if (not M_CachedCompleteLandscapeBounds.IsValid)
	{
		Editor_RefreshCachedCompleteLandscapeBounds();
	}
#endif

	if (not BuildTextureMapping()
		|| not GetAreLandscapeDynamicMaterialInstancesEnabled(true))
	{
		return false;
	}

	const int32 PixelCount = M_TextureMapping.TextureResolution.X * M_TextureMapping.TextureResolution.Y;
	TArray<FColor> BlankPixels;
	BlankPixels.Init(FColor::Transparent, PixelCount);
	if (not PublishInitialBlankPixels(MoveTemp(BlankPixels)))
	{
		return false;
	}

	bM_IsInitialized = true;
	return true;
}

bool ALandscapeDataManager::GetIsUniqueManager(const bool bReportError) const
{
	FString FailureReason;
	const bool bIsUniqueManager = FindUniqueManager(this, FailureReason) == this;
	if (not bIsUniqueManager && bReportError)
	{
		RTSFunctionLibrary::ReportError(FailureReason);
	}
	return bIsUniqueManager;
}

bool ALandscapeDataManager::ResolveLandscape(const bool bReportError)
{
	if (M_Landscape.Get() != nullptr)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		if (bReportError)
		{
			RTSFunctionLibrary::ReportError(TEXT("LandscapeDataManager could not resolve its world."));
		}
		return false;
	}

	ALandscape* FoundLandscape = nullptr;
	int32 LandscapeCount = 0;
	for (TActorIterator<ALandscape> LandscapeIterator(World); LandscapeIterator; ++LandscapeIterator)
	{
		ALandscape* CandidateLandscape = *LandscapeIterator;
		if (not IsValid(CandidateLandscape))
		{
			continue;
		}
		FoundLandscape = CandidateLandscape;
		++LandscapeCount;
	}

	if (LandscapeCount == 1)
	{
		M_Landscape = FoundLandscape;
		return true;
	}

	if (bReportError)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("LandscapeDataManager expected one main Landscape actor, but found %d. Assign M_Landscape explicitly."),
			LandscapeCount));
	}
	return false;
}

bool ALandscapeDataManager::BuildTextureMapping()
{
	if (not GetIsValidLandscape())
	{
		return false;
	}

	const ALandscape* Landscape = M_Landscape.Get();
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FBox LandscapeBounds = Landscape->GetLoadedBounds();
	if (World->IsPartitionedWorld())
	{
		if (not M_CachedCompleteLandscapeBounds.IsValid)
		{
			RTSFunctionLibrary::ReportError(
				TEXT("LandscapeDataManager in World Partition requires cached complete bounds. "
					"Select the manager and run Refresh Bounds From Landscape before cooking."));
			return false;
		}
		LandscapeBounds = M_CachedCompleteLandscapeBounds;
	}

	const FVector LandscapeSize = LandscapeBounds.GetSize();
	if (not LandscapeBounds.IsValid
		|| not FMath::IsFinite(LandscapeSize.X)
		|| not FMath::IsFinite(LandscapeSize.Y)
		|| LandscapeSize.X <= UE_KINDA_SMALL_NUMBER
		|| LandscapeSize.Y <= UE_KINDA_SMALL_NUMBER
		|| not FMath::IsFinite(M_DesiredWorldUnitsPerTexel))
	{
		RTSFunctionLibrary::ReportError(TEXT("LandscapeDataManager could not derive valid XY Landscape bounds."));
		return false;
	}

	const double WorldUnitsPerTexel = FMath::Max(
		LandscapeDataManagerConstants::MinimumWorldUnitsPerTexel,
		static_cast<double>(M_DesiredWorldUnitsPerTexel));
	const int32 MaximumResolution = LandscapeDataManagerConstants::SanitizeMaximumResolution(
		M_MaximumTextureResolution);
	M_TextureMapping.WorldMinimum = FVector2D(LandscapeBounds.Min.X, LandscapeBounds.Min.Y);
	M_TextureMapping.WorldSize = FVector2D(LandscapeSize.X, LandscapeSize.Y);
	M_TextureMapping.InverseWorldSize = FVector2D(1.0 / LandscapeSize.X, 1.0 / LandscapeSize.Y);
	M_TextureMapping.TextureResolution = FIntPoint(
		LandscapeDataManagerConstants::CalculateResolutionForWorldSize(
			LandscapeSize.X, WorldUnitsPerTexel, MaximumResolution),
		LandscapeDataManagerConstants::CalculateResolutionForWorldSize(
			LandscapeSize.Y, WorldUnitsPerTexel, MaximumResolution));
	M_TextureMapping.WorldUnitsPerTexel = FVector2D(
		LandscapeSize.X / M_TextureMapping.TextureResolution.X,
		LandscapeSize.Y / M_TextureMapping.TextureResolution.Y);
	return true;
}

bool ALandscapeDataManager::ReplacePointContribution(
	FGuid& InOutContributionId,
	const ERTSLandscapeDataChannel Channel,
	const TArray<FRTSLandscapeDataPointStamp>& PointStamps,
	UPCGComponent* SourceComponent)
{
	if (not EnsureInitialized())
	{
		return false;
	}

	if (not InOutContributionId.IsValid())
	{
		InOutContributionId = FGuid::NewGuid();
	}

	FRTSLandscapeDataManagerContribution Contribution;
	Contribution.Channel = Channel;
	Contribution.PointStamps = PointStamps;
	return StoreContribution(InOutContributionId, MoveTemp(Contribution), SourceComponent);
}

bool ALandscapeDataManager::ReplaceVolumeContribution(
	FGuid& InOutContributionId,
	const ERTSLandscapeDataChannel Channel,
	const TArray<const UPCGSpatialData*>& SpatialData,
	UPCGComponent* SourceComponent)
{
	if (not EnsureInitialized())
	{
		return false;
	}

	if (not InOutContributionId.IsValid())
	{
		InOutContributionId = FGuid::NewGuid();
	}

	FRTSLandscapeDataManagerContribution Contribution;
	Contribution.Channel = Channel;
	BuildVolumeRasters(SpatialData, Contribution.Rasters);
	return StoreContribution(InOutContributionId, MoveTemp(Contribution), SourceComponent);
}

bool ALandscapeDataManager::StoreContribution(
	const FGuid& ContributionId,
	FRTSLandscapeDataManagerContribution&& Contribution,
	UPCGComponent* SourceComponent)
{
	if (SourceComponent == nullptr)
	{
		M_Contributions.Add(ContributionId, MoveTemp(Contribution));
		bM_IsDirty = true;
		RebuildLandscapeData();
		return true;
	}

	if (not IsValid(SourceComponent))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("LandscapeDataManager received an invalid non-null source PCG component."));
		return false;
	}

	Contribution.SourceComponentKey = FObjectKey(SourceComponent);
	M_StagedContributions.Add(ContributionId, MoveTemp(Contribution));
	M_PendingRemovals.Remove(ContributionId);
	TrackPCGComponent(SourceComponent);
	return true;
}

bool ALandscapeDataManager::CommitStagedContributions(const FObjectKey& SourceComponentKey)
{
	TArray<FGuid> ContributionIds;
	for (const TPair<FGuid, FRTSLandscapeDataManagerContribution>& Entry : M_StagedContributions)
	{
		if (Entry.Value.SourceComponentKey == SourceComponentKey)
		{
			ContributionIds.Add(Entry.Key);
		}
	}

	bool bChanged = false;
	for (const FGuid& ContributionId : ContributionIds)
	{
		FRTSLandscapeDataManagerContribution Contribution;
		if (M_StagedContributions.RemoveAndCopyValue(ContributionId, Contribution))
		{
			M_Contributions.Add(ContributionId, MoveTemp(Contribution));
			bChanged = true;
		}
	}
	return bChanged;
}

bool ALandscapeDataManager::ApplyPendingRemovals(const FObjectKey& SourceComponentKey)
{
	TArray<FGuid> ContributionIds;
	for (const TPair<FGuid, FObjectKey>& Entry : M_PendingRemovals)
	{
		if (Entry.Value == SourceComponentKey)
		{
			ContributionIds.Add(Entry.Key);
		}
	}

	bool bChanged = false;
	for (const FGuid& ContributionId : ContributionIds)
	{
		M_PendingRemovals.Remove(ContributionId);
		bChanged = M_Contributions.Remove(ContributionId) > 0 || bChanged;
	}
	return bChanged;
}

void ALandscapeDataManager::DiscardStagedContributions(
	const FObjectKey& SourceComponentKey)
{
	TArray<FGuid> StagedContributionIds;
	for (const TPair<FGuid, FRTSLandscapeDataManagerContribution>& Entry : M_StagedContributions)
	{
		if (Entry.Value.SourceComponentKey == SourceComponentKey)
		{
			StagedContributionIds.Add(Entry.Key);
		}
	}
	for (const FGuid& ContributionId : StagedContributionIds)
	{
		M_StagedContributions.Remove(ContributionId);
	}
}

void ALandscapeDataManager::DiscardPendingRemovals(
	const FObjectKey& SourceComponentKey)
{
	TArray<FGuid> RemovalIds;
	for (const TPair<FGuid, FObjectKey>& Entry : M_PendingRemovals)
	{
		if (Entry.Value == SourceComponentKey)
		{
			RemovalIds.Add(Entry.Key);
		}
	}

	for (const FGuid& ContributionId : RemovalIds)
	{
		M_PendingRemovals.Remove(ContributionId);
	}
}

void ALandscapeDataManager::BuildVolumeRasters(
	const TArray<const UPCGSpatialData*>& SpatialData,
	TArray<FRTSLandscapeDataRasterContribution>& OutRasters) const
{
	OutRasters.Reset();
	OutRasters.Reserve(SpatialData.Num());
	for (const UPCGSpatialData* SpatialInput : SpatialData)
	{
		if (not IsValid(SpatialInput))
		{
			continue;
		}

		FRTSLandscapeDataRasterContribution Raster;
		if (BuildVolumeRaster(*SpatialInput, Raster))
		{
			OutRasters.Add(MoveTemp(Raster));
		}
	}
}

bool ALandscapeDataManager::BuildVolumeRaster(
	const UPCGSpatialData& SpatialData,
	FRTSLandscapeDataRasterContribution& OutRaster) const
{
	if (not GetPixelRectForWorldBounds(SpatialData.GetBounds(), OutRaster.PixelBounds))
	{
		return false;
	}

	const int32 RasterWidth = OutRaster.PixelBounds.Width();
	const int32 RasterHeight = OutRaster.PixelBounds.Height();
	OutRaster.Coverage.SetNumZeroed(RasterWidth * RasterHeight);
	for (int32 PixelY = OutRaster.PixelBounds.Min.Y; PixelY < OutRaster.PixelBounds.Max.Y; ++PixelY)
	{
		RasterizeVolumeRow(SpatialData, PixelY, OutRaster);
	}
	return true;
}

void ALandscapeDataManager::RasterizeVolumeRow(
	const UPCGSpatialData& SpatialData,
	const int32 PixelY,
	FRTSLandscapeDataRasterContribution& InOutRaster) const
{
	const FBox InputBounds = SpatialData.GetBounds();
	const int32 RasterWidth = InOutRaster.PixelBounds.Width();
	for (int32 PixelX = InOutRaster.PixelBounds.Min.X; PixelX < InOutRaster.PixelBounds.Max.X; ++PixelX)
	{
		const FVector SamplePosition = GetWorldPositionForPixel(
			FIntPoint(PixelX, PixelY),
			InputBounds.GetCenter().Z);
		if (not InputBounds.IsInsideXY(SamplePosition))
		{
			continue;
		}

		const float SampledDensity = SpatialData.GetDensityAtPosition(SamplePosition);
		const float Density = FMath::IsFinite(SampledDensity)
			? FMath::Clamp(
				SampledDensity,
				LandscapeDataManagerConstants::MinimumNormalizedValue,
				LandscapeDataManagerConstants::MaximumNormalizedValue)
			: LandscapeDataManagerConstants::MinimumNormalizedValue;
		const int32 RasterIndex = (PixelY - InOutRaster.PixelBounds.Min.Y) * RasterWidth
			+ PixelX - InOutRaster.PixelBounds.Min.X;
		InOutRaster.Coverage[RasterIndex] = static_cast<uint8>(FMath::RoundToInt32(
			Density * LandscapeDataManagerConstants::MaximumChannelValue));
	}
}

void ALandscapeDataManager::RemoveContribution(
	const FGuid& ContributionId,
	const FObjectKey& ExpectedSourceComponentKey)
{
	if (not IsInGameThread())
	{
		RTSFunctionLibrary::ReportError(TEXT("LandscapeDataManager removal must run on the game thread."));
		return;
	}

	if (not ContributionId.IsValid())
	{
		return;
	}

	FObjectKey SourceComponentKey;
	if (const FRTSLandscapeDataManagerContribution* StagedContribution =
		M_StagedContributions.Find(ContributionId))
	{
		SourceComponentKey = StagedContribution->SourceComponentKey;
	}
	else if (const FRTSLandscapeDataManagerContribution* Contribution = M_Contributions.Find(ContributionId))
	{
		SourceComponentKey = Contribution->SourceComponentKey;
	}
	else
	{
		return;
	}
	if (SourceComponentKey != ExpectedSourceComponentKey)
	{
		return;
	}

	M_StagedContributions.Remove(ContributionId);
	if (not M_Contributions.Contains(ContributionId))
	{
		return;
	}

	if (SourceComponentKey.ResolveObjectPtr() != nullptr)
	{
		M_PendingRemovals.Add(ContributionId, SourceComponentKey);
		return;
	}

	if (M_Contributions.Remove(ContributionId) > 0)
	{
		bM_IsDirty = true;
		RebuildLandscapeData();
	}
}

void ALandscapeDataManager::TransferContributionSource(
	const FGuid& ContributionId,
	UPCGComponent* NewSourceComponent)
{
	if (not IsInGameThread() || not IsValid(NewSourceComponent) || not ContributionId.IsValid())
	{
		return;
	}

	const FObjectKey NewSourceComponentKey(NewSourceComponent);
	bool bFoundContribution = false;
	if (FRTSLandscapeDataManagerContribution* Contribution = M_Contributions.Find(ContributionId))
	{
		Contribution->SourceComponentKey = NewSourceComponentKey;
		bFoundContribution = true;
	}
	if (FRTSLandscapeDataManagerContribution* StagedContribution =
		M_StagedContributions.Find(ContributionId))
	{
		StagedContribution->SourceComponentKey = NewSourceComponentKey;
		bFoundContribution = true;
	}
	if (FObjectKey* PendingRemovalSource = M_PendingRemovals.Find(ContributionId))
	{
		*PendingRemovalSource = NewSourceComponentKey;
		bFoundContribution = true;
	}

	if (bFoundContribution)
	{
		TrackPCGComponent(NewSourceComponent);
	}
}

bool ALandscapeDataManager::RemoveContributionsForSource(
	const FObjectKey& SourceComponentKey)
{
	TArray<FGuid> ContributionIds;
	for (const TPair<FGuid, FRTSLandscapeDataManagerContribution>& Entry : M_Contributions)
	{
		if (Entry.Value.SourceComponentKey == SourceComponentKey)
		{
			ContributionIds.Add(Entry.Key);
		}
	}

	DiscardStagedContributions(SourceComponentKey);
	DiscardPendingRemovals(SourceComponentKey);
	bool bRemovedContribution = false;
	for (const FGuid& ContributionId : ContributionIds)
	{
		bRemovedContribution = M_Contributions.Remove(ContributionId) > 0
			|| bRemovedContribution;
	}
	return bRemovedContribution;
}

void ALandscapeDataManager::RebuildLandscapeData()
{
	if (bM_IsRebuilding || not EnsureInitialized())
	{
		return;
	}

	TGuardValue<bool> RebuildGuard(bM_IsRebuilding, true);
	TArray<FColor> Pixels;
	RasterizeContributions(Pixels);
	if (PublishPixels(MoveTemp(Pixels)))
	{
		bM_IsDirty = false;
	}
}

void ALandscapeDataManager::RasterizeContributions(TArray<FColor>& OutPixels) const
{
	const int32 PixelCount = M_TextureMapping.TextureResolution.X * M_TextureMapping.TextureResolution.Y;
	OutPixels.Init(FColor::Transparent, PixelCount);
	for (const TPair<FGuid, FRTSLandscapeDataManagerContribution>& ContributionEntry : M_Contributions)
	{
		const FRTSLandscapeDataManagerContribution& Contribution = ContributionEntry.Value;
		for (const FRTSLandscapeDataPointStamp& PointStamp : Contribution.PointStamps)
		{
			RasterizePointStamp(PointStamp, Contribution.Channel, OutPixels);
		}
		for (const FRTSLandscapeDataRasterContribution& Raster : Contribution.Rasters)
		{
			ApplyRasterContribution(Contribution, Raster, OutPixels);
		}
	}
}

void ALandscapeDataManager::RasterizePointStamp(
	const FRTSLandscapeDataPointStamp& PointStamp,
	const ERTSLandscapeDataChannel Channel,
	TArray<FColor>& InOutPixels) const
{
	const FVector PointScale = PointStamp.Transform.GetScale3D();
	if (PointStamp.Transform.ContainsNaN()
		|| PointStamp.BoundsMin.ContainsNaN()
		|| PointStamp.BoundsMax.ContainsNaN()
		|| not FMath::IsFinite(PointStamp.Strength)
		|| PointStamp.BoundsMax.X <= PointStamp.BoundsMin.X
		|| PointStamp.BoundsMax.Y <= PointStamp.BoundsMin.Y
		|| FMath::Abs(PointScale.X) <= UE_KINDA_SMALL_NUMBER
		|| FMath::Abs(PointScale.Y) <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FBox LocalBounds(PointStamp.BoundsMin, PointStamp.BoundsMax);
	FIntRect PixelBounds;
	if (not GetPixelRectForWorldBounds(LocalBounds.TransformBy(PointStamp.Transform), PixelBounds))
	{
		return;
	}

	for (int32 PixelY = PixelBounds.Min.Y; PixelY < PixelBounds.Max.Y; ++PixelY)
	{
		RasterizePointStampRow(PointStamp, Channel, PixelBounds, PixelY, InOutPixels);
	}
}

void ALandscapeDataManager::RasterizePointStampRow(
	const FRTSLandscapeDataPointStamp& PointStamp,
	const ERTSLandscapeDataChannel Channel,
	const FIntRect& PixelBounds,
	const int32 PixelY,
	TArray<FColor>& InOutPixels) const
{
	const uint8 Value = static_cast<uint8>(FMath::RoundToInt32(
		FMath::Clamp(
			PointStamp.Strength,
			LandscapeDataManagerConstants::MinimumNormalizedValue,
			LandscapeDataManagerConstants::MaximumNormalizedValue)
		* LandscapeDataManagerConstants::MaximumChannelValue));
	for (int32 PixelX = PixelBounds.Min.X; PixelX < PixelBounds.Max.X; ++PixelX)
	{
		const FVector WorldPosition = GetWorldPositionForPixel(
			FIntPoint(PixelX, PixelY),
			PointStamp.Transform.GetLocation().Z);
		const FVector LocalPosition = PointStamp.Transform.InverseTransformPosition(WorldPosition);
		if (LocalPosition.X < PointStamp.BoundsMin.X || LocalPosition.X > PointStamp.BoundsMax.X
			|| LocalPosition.Y < PointStamp.BoundsMin.Y || LocalPosition.Y > PointStamp.BoundsMax.Y)
		{
			continue;
		}

		const int32 PixelIndex = PixelY * M_TextureMapping.TextureResolution.X + PixelX;
		ApplyChannelMaximum(InOutPixels[PixelIndex], Channel, Value);
	}
}

void ALandscapeDataManager::ApplyRasterContribution(
	const FRTSLandscapeDataManagerContribution& Contribution,
	const FRTSLandscapeDataRasterContribution& Raster,
	TArray<FColor>& InOutPixels) const
{
	const int32 RasterWidth = Raster.PixelBounds.Width();
	const int32 RasterHeight = Raster.PixelBounds.Height();
	if (RasterWidth <= 0
		|| RasterHeight <= 0
		|| Raster.Coverage.Num() != RasterWidth * RasterHeight)
	{
		return;
	}

	for (int32 LocalY = 0; LocalY < RasterHeight; ++LocalY)
	{
		ApplyRasterContributionRow(Contribution, Raster, LocalY, InOutPixels);
	}
}

void ALandscapeDataManager::ApplyRasterContributionRow(
	const FRTSLandscapeDataManagerContribution& Contribution,
	const FRTSLandscapeDataRasterContribution& Raster,
	const int32 LocalY,
	TArray<FColor>& InOutPixels) const
{
	const int32 RasterWidth = Raster.PixelBounds.Width();
	const int32 PixelY = Raster.PixelBounds.Min.Y + LocalY;
	for (int32 LocalX = 0; LocalX < RasterWidth; ++LocalX)
	{
		const int32 PixelX = Raster.PixelBounds.Min.X + LocalX;
		const int32 SourceIndex = LocalY * RasterWidth + LocalX;
		const int32 DestinationIndex = PixelY * M_TextureMapping.TextureResolution.X + PixelX;
		if (Raster.Coverage.IsValidIndex(SourceIndex) && InOutPixels.IsValidIndex(DestinationIndex))
		{
			ApplyChannelMaximum(
				InOutPixels[DestinationIndex],
				Contribution.Channel,
				Raster.Coverage[SourceIndex]);
		}
	}
}

bool ALandscapeDataManager::GetPixelRectForWorldBounds(
	const FBox& WorldBounds,
	FIntRect& OutPixelBounds) const
{
	if (not WorldBounds.IsValid
		|| not FMath::IsFinite(WorldBounds.Min.X)
		|| not FMath::IsFinite(WorldBounds.Min.Y)
		|| not FMath::IsFinite(WorldBounds.Max.X)
		|| not FMath::IsFinite(WorldBounds.Max.Y)
		|| not M_TextureMapping.IsValid())
	{
		return false;
	}

	const FVector2D UnclippedMinimumUV(
		(WorldBounds.Min.X - M_TextureMapping.WorldMinimum.X) * M_TextureMapping.InverseWorldSize.X,
		(WorldBounds.Min.Y - M_TextureMapping.WorldMinimum.Y) * M_TextureMapping.InverseWorldSize.Y);
	const FVector2D UnclippedMaximumUV(
		(WorldBounds.Max.X - M_TextureMapping.WorldMinimum.X) * M_TextureMapping.InverseWorldSize.X,
		(WorldBounds.Max.Y - M_TextureMapping.WorldMinimum.Y) * M_TextureMapping.InverseWorldSize.Y);
	FVector2D MinimumUV;
	FVector2D MaximumUV;
	if (not LandscapeDataManagerConstants::GetClippedTextureUVBounds(
		UnclippedMinimumUV,
		UnclippedMaximumUV,
		MinimumUV,
		MaximumUV))
	{
		return false;
	}
	OutPixelBounds.Min.X = FMath::Clamp(
		FMath::FloorToInt32(MinimumUV.X * M_TextureMapping.TextureResolution.X),
		0,
		M_TextureMapping.TextureResolution.X);
	OutPixelBounds.Min.Y = FMath::Clamp(
		FMath::FloorToInt32(MinimumUV.Y * M_TextureMapping.TextureResolution.Y),
		0,
		M_TextureMapping.TextureResolution.Y);
	OutPixelBounds.Max.X = FMath::Clamp(
		FMath::CeilToInt32(MaximumUV.X * M_TextureMapping.TextureResolution.X),
		0,
		M_TextureMapping.TextureResolution.X);
	OutPixelBounds.Max.Y = FMath::Clamp(
		FMath::CeilToInt32(MaximumUV.Y * M_TextureMapping.TextureResolution.Y),
		0,
		M_TextureMapping.TextureResolution.Y);
	return OutPixelBounds.Width() > 0 && OutPixelBounds.Height() > 0;
}

FVector ALandscapeDataManager::GetWorldPositionForPixel(
	const FIntPoint& Pixel,
	const double WorldZ) const
{
	const double WorldX = M_TextureMapping.WorldMinimum.X
		+ (Pixel.X + LandscapeDataManagerConstants::HalfTexel)
		* M_TextureMapping.WorldUnitsPerTexel.X;
	const double WorldY = M_TextureMapping.WorldMinimum.Y
		+ (Pixel.Y + LandscapeDataManagerConstants::HalfTexel)
		* M_TextureMapping.WorldUnitsPerTexel.Y;
	return FVector(WorldX, WorldY, WorldZ);
}

UTexture2D* ALandscapeDataManager::CreatePublishedTexture(
	const TArray<FColor>& BasePixels,
	const FIntPoint& Resolution)
{
	if (Resolution.X <= 0
		|| Resolution.Y <= 0
		|| BasePixels.Num() != Resolution.X * Resolution.Y)
	{
		RTSFunctionLibrary::ReportError(TEXT("LandscapeDataManager received an invalid base pixel count."));
		return nullptr;
	}

	UTexture2D* NewTexture = NewObject<UTexture2D>(
		GetTransientPackage(),
		NAME_None,
		RF_Transient);
	if (not IsValid(NewTexture))
	{
		return nullptr;
	}

	FTexturePlatformData* PlatformData = new FTexturePlatformData();
	PlatformData->SizeX = Resolution.X;
	PlatformData->SizeY = Resolution.Y;
	PlatformData->SetNumSlices(1);
	PlatformData->PixelFormat = PF_B8G8R8A8;
	NewTexture->SetPlatformData(PlatformData);

	TArray<FColor> CurrentPixels = BasePixels;
	FIntPoint CurrentResolution = Resolution;
	while (true)
	{
		LandscapeDataManagerConstants::AddTextureMip(*PlatformData, CurrentPixels, CurrentResolution);
		if (CurrentResolution.X == 1 && CurrentResolution.Y == 1)
		{
			break;
		}
		FIntPoint NextResolution;
		CurrentPixels = LandscapeDataManagerConstants::BuildNextAverageMip(
			CurrentPixels, CurrentResolution, NextResolution);
		CurrentResolution = NextResolution;
	}

	NewTexture->SRGB = false;
	NewTexture->NeverStream = true;
	NewTexture->Filter = TF_Trilinear;
	NewTexture->AddressX = TA_Clamp;
	NewTexture->AddressY = TA_Clamp;
	NewTexture->CompressionSettings = TC_Masks;
	NewTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
	NewTexture->UpdateResource();
	return NewTexture;
}

UTexture2D* ALandscapeDataManager::CreateBlankPublishedTexture()
{
	TArray<FColor> BlankPixels;
	BlankPixels.Add(FColor::Transparent);
	const FIntPoint PlaceholderResolution(
		LandscapeDataManagerConstants::PlaceholderTextureResolution,
		LandscapeDataManagerConstants::PlaceholderTextureResolution);
	return CreatePublishedTexture(BlankPixels, PlaceholderResolution);
}

bool ALandscapeDataManager::PublishInitialBlankPixels(TArray<FColor>&& BlankPixels)
{
	UTexture2D* NewTexture = CreateBlankPublishedTexture();
	if (not IsValid(NewTexture))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("LandscapeDataManager failed to publish its initial blank texture."));
		return false;
	}

	M_CombinedPixels = MoveTemp(BlankPixels);
	M_LandscapeDataTexture = NewTexture;
	BindPublishedTextureToLandscape();
	return true;
}

bool ALandscapeDataManager::PublishPixels(TArray<FColor>&& Pixels)
{
	UTexture2D* NewTexture = CreatePublishedTexture(
		Pixels,
		M_TextureMapping.TextureResolution);
	if (not IsValid(NewTexture))
	{
		RTSFunctionLibrary::ReportError(TEXT("LandscapeDataManager failed to publish its runtime texture."));
		return false;
	}

	// The old texture remains immutable while Unreal safely retires its render resource after this swap.
	M_CombinedPixels = MoveTemp(Pixels);
	M_LandscapeDataTexture = NewTexture;
	BindPublishedTextureToLandscape();
	return true;
}

bool ALandscapeDataManager::GetAreLandscapeDynamicMaterialInstancesEnabled(
	const bool bReportError) const
{
	if (not GetIsValidLandscape())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const ALandscape* Landscape = M_Landscape.Get();
	int32 MatchingProxyCount = 0;
	int32 InvalidProxyCount = 0;
	for (TActorIterator<ALandscapeProxy> ProxyIterator(World); ProxyIterator; ++ProxyIterator)
	{
		ALandscapeProxy* LandscapeProxy = *ProxyIterator;
		if (not IsValid(LandscapeProxy)
			|| not LandscapeDataManagerConstants::IsLandscapeProxyForLandscape(*LandscapeProxy, *Landscape))
		{
			continue;
		}
		if (LandscapeProxy->LandscapeComponents.IsEmpty())
		{
			continue;
		}

		++MatchingProxyCount;
		if (not GetDoesLandscapeProxyHaveRequiredMaterialSetup(*LandscapeProxy))
		{
			++InvalidProxyCount;
		}
	}

	const bool bCanWaitForPartitionedProxy = MatchingProxyCount == 0 && World->IsPartitionedWorld();
	const bool bHasValidSetup = bCanWaitForPartitionedProxy
		|| (MatchingProxyCount > 0 && InvalidProxyCount == 0);
	if (not bHasValidSetup && bReportError)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("LandscapeDataManager found %d of %d Landscape proxies without usable dynamic material "
				"instances and required parameters. Enable Use Dynamic Material Instance before Play, and add "
				"%s, %s, and %s to every Landscape material."),
			InvalidProxyCount,
			MatchingProxyCount,
			*M_TextureParameterName.ToString(),
			*M_WorldMappingParameterName.ToString(),
			*M_TextureMetricsParameterName.ToString()));
	}
	return bHasValidSetup;
}

bool ALandscapeDataManager::GetDoesLandscapeProxyHaveRequiredMaterialSetup(
	const ALandscapeProxy& LandscapeProxy) const
{
	if (not LandscapeProxy.bUseDynamicMaterialInstance
		|| LandscapeProxy.LandscapeComponents.IsEmpty())
	{
		return false;
	}

	for (const ULandscapeComponent* LandscapeComponent : LandscapeProxy.LandscapeComponents)
	{
		if (not IsValid(LandscapeComponent)
			|| not GetDoesLandscapeComponentHaveRequiredMaterialSetup(*LandscapeComponent))
		{
			return false;
		}
	}
	return true;
}

bool ALandscapeDataManager::GetDoesLandscapeComponentHaveRequiredMaterialSetup(
	const ULandscapeComponent& LandscapeComponent) const
{
	if (M_TextureParameterName.IsNone()
		|| M_WorldMappingParameterName.IsNone()
		|| LandscapeComponent.MaterialInstancesDynamic.IsEmpty())
	{
		return false;
	}

	for (const UMaterialInstanceDynamic* MaterialInstance : LandscapeComponent.MaterialInstancesDynamic)
	{
		if (not IsValid(MaterialInstance))
		{
			return false;
		}

		UTexture* ExistingTexture = nullptr;
		FLinearColor ExistingVector;
		const bool bHasMetricsParameter = M_TextureMetricsParameterName.IsNone()
			|| MaterialInstance->GetVectorParameterValue(
				FHashedMaterialParameterInfo(M_TextureMetricsParameterName), ExistingVector);
		if (not MaterialInstance->GetTextureParameterValue(
				FHashedMaterialParameterInfo(M_TextureParameterName), ExistingTexture)
			|| not MaterialInstance->GetVectorParameterValue(
				FHashedMaterialParameterInfo(M_WorldMappingParameterName), ExistingVector)
			|| not bHasMetricsParameter)
		{
			return false;
		}
	}
	return true;
}

bool ALandscapeDataManager::GetIsPublishedTextureBoundToLandscape() const
{
	if (not GetIsValidLandscape() || not GetIsValidLandscapeDataTexture())
	{
		return false;
	}

	const ALandscape* Landscape = M_Landscape.Get();
	int32 MatchingProxyCount = 0;
	for (TActorIterator<ALandscapeProxy> ProxyIterator(GetWorld()); ProxyIterator; ++ProxyIterator)
	{
		const ALandscapeProxy* LandscapeProxy = *ProxyIterator;
		if (not IsValid(LandscapeProxy)
			|| not LandscapeDataManagerConstants::IsLandscapeProxyForLandscape(
				*LandscapeProxy,
				*Landscape)
			|| LandscapeProxy->LandscapeComponents.IsEmpty())
		{
			continue;
		}

		++MatchingProxyCount;
		if (not GetDoesLandscapeProxyUsePublishedMaterialValues(*LandscapeProxy))
		{
			return false;
		}
	}
	return MatchingProxyCount > 0 || GetWorld()->IsPartitionedWorld();
}

bool ALandscapeDataManager::GetDoesLandscapeProxyUsePublishedMaterialValues(
	const ALandscapeProxy& LandscapeProxy) const
{
	for (const ULandscapeComponent* LandscapeComponent : LandscapeProxy.LandscapeComponents)
	{
		if (not IsValid(LandscapeComponent)
			|| not GetDoesLandscapeComponentUsePublishedMaterialValues(*LandscapeComponent))
		{
			return false;
		}
	}
	return not LandscapeProxy.LandscapeComponents.IsEmpty();
}

bool ALandscapeDataManager::GetDoesLandscapeComponentUsePublishedMaterialValues(
	const ULandscapeComponent& LandscapeComponent) const
{
	const FLinearColor ExpectedWorldMapping(
		M_TextureMapping.WorldMinimum.X,
		M_TextureMapping.WorldMinimum.Y,
		M_TextureMapping.InverseWorldSize.X,
		M_TextureMapping.InverseWorldSize.Y);
	const FLinearColor ExpectedTextureMetrics(
		M_TextureMapping.TextureResolution.X,
		M_TextureMapping.TextureResolution.Y,
		M_TextureMapping.WorldUnitsPerTexel.X,
		M_TextureMapping.WorldUnitsPerTexel.Y);
	constexpr float ParameterComparisonTolerance = UE_KINDA_SMALL_NUMBER;
	for (const UMaterialInstanceDynamic* MaterialInstance : LandscapeComponent.MaterialInstancesDynamic)
	{
		if (not IsValid(MaterialInstance))
		{
			return false;
		}

		UTexture* BoundTexture = nullptr;
		FLinearColor BoundWorldMapping;
		FLinearColor BoundTextureMetrics;
		const bool bHasMetricsValue = M_TextureMetricsParameterName.IsNone()
			|| MaterialInstance->GetVectorParameterValue(
				FHashedMaterialParameterInfo(M_TextureMetricsParameterName), BoundTextureMetrics);
		const bool bMetricsMatch = M_TextureMetricsParameterName.IsNone()
			|| BoundTextureMetrics.Equals(ExpectedTextureMetrics, ParameterComparisonTolerance);
		if (not MaterialInstance->GetTextureParameterValue(
				FHashedMaterialParameterInfo(M_TextureParameterName), BoundTexture)
			|| BoundTexture != M_LandscapeDataTexture
			|| not MaterialInstance->GetVectorParameterValue(
				FHashedMaterialParameterInfo(M_WorldMappingParameterName), BoundWorldMapping)
			|| not BoundWorldMapping.Equals(ExpectedWorldMapping, ParameterComparisonTolerance)
			|| not bHasMetricsValue
			|| not bMetricsMatch)
		{
			return false;
		}
	}
	return not LandscapeComponent.MaterialInstancesDynamic.IsEmpty();
}

void ALandscapeDataManager::BindPublishedTextureToLandscape()
{
	if (not GetIsValidLandscape() || not GetIsValidLandscapeDataTexture())
	{
		return;
	}

	const ALandscape* Landscape = M_Landscape.Get();
	for (TActorIterator<ALandscapeProxy> ProxyIterator(GetWorld()); ProxyIterator; ++ProxyIterator)
	{
		ALandscapeProxy* LandscapeProxy = *ProxyIterator;
		if (not IsValid(LandscapeProxy)
			|| not LandscapeDataManagerConstants::IsLandscapeProxyForLandscape(*LandscapeProxy, *Landscape))
		{
			continue;
		}
		if (LandscapeProxy->LandscapeComponents.IsEmpty())
		{
			continue;
		}
		BindPublishedTextureToLandscapeProxy(*LandscapeProxy);
	}
}

void ALandscapeDataManager::BindPublishedTextureToLandscapeProxy(
	ALandscapeProxy& LandscapeProxy) const
{
	if (not GetDoesLandscapeProxyHaveRequiredMaterialSetup(LandscapeProxy))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("LandscapeDataManager cannot bind proxy %s because its dynamic material setup is invalid."),
			*LandscapeProxy.GetName()));
		return;
	}

	const FLinearColor WorldMapping(
		M_TextureMapping.WorldMinimum.X,
		M_TextureMapping.WorldMinimum.Y,
		M_TextureMapping.InverseWorldSize.X,
		M_TextureMapping.InverseWorldSize.Y);
	const FLinearColor TextureMetrics(
		M_TextureMapping.TextureResolution.X,
		M_TextureMapping.TextureResolution.Y,
		M_TextureMapping.WorldUnitsPerTexel.X,
		M_TextureMapping.WorldUnitsPerTexel.Y);
	LandscapeProxy.SetLandscapeMaterialTextureParameterValue(
		M_TextureParameterName,
		M_LandscapeDataTexture);
	LandscapeProxy.SetLandscapeMaterialVectorParameterValue(
		M_WorldMappingParameterName,
		WorldMapping);
	if (not M_TextureMetricsParameterName.IsNone())
	{
		LandscapeProxy.SetLandscapeMaterialVectorParameterValue(
			M_TextureMetricsParameterName,
			TextureMetrics);
	}
}

void ALandscapeDataManager::BindLevelStreaming()
{
	if (M_LevelAddedToWorldHandle.IsValid())
	{
		return;
	}

	M_LevelAddedToWorldHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(
		this,
		&ALandscapeDataManager::HandleLevelAddedToWorld);
}

void ALandscapeDataManager::EndPlay_UnbindLevelStreaming()
{
	if (not M_LevelAddedToWorldHandle.IsValid())
	{
		return;
	}

	FWorldDelegates::LevelAddedToWorld.Remove(M_LevelAddedToWorldHandle);
	M_LevelAddedToWorldHandle.Reset();
}

void ALandscapeDataManager::EndPlay_ClearPublishedTexture(
	const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason != EEndPlayReason::Destroyed
		&& EndPlayReason != EEndPlayReason::RemovedFromWorld)
	{
		return;
	}

	FString FailureReason;
	if (FindUniqueManager(this, FailureReason) != this
		|| not M_TextureMapping.IsValid()
		|| not GetIsValidLandscapeDataTexture())
	{
		return;
	}

	UTexture2D* BlankTexture = CreateBlankPublishedTexture();
	if (not IsValid(BlankTexture))
	{
		return;
	}
	M_LandscapeDataTexture = BlankTexture;
	BindPublishedTextureToLandscape();
}

void ALandscapeDataManager::HandleLevelAddedToWorld(ULevel* Level, UWorld* World)
{
	if (World != GetWorld()
		|| not IsValid(Level)
		|| not bM_IsInitialized
		|| not GetIsValidLandscape()
		|| not GetIsValidLandscapeDataTexture())
	{
		return;
	}

	const ALandscape* Landscape = M_Landscape.Get();
	for (AActor* LevelActor : Level->Actors)
	{
		ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(LevelActor);
		if (not IsValid(LandscapeProxy)
			|| not LandscapeDataManagerConstants::IsLandscapeProxyForLandscape(
				*LandscapeProxy,
				*Landscape)
			|| LandscapeProxy->LandscapeComponents.IsEmpty())
		{
			continue;
		}

		BindPublishedTextureToLandscapeProxy(*LandscapeProxy);
	}
}

void ALandscapeDataManager::TrackPCGComponent(UPCGComponent* SourceComponent)
{
	if (not IsValid(SourceComponent))
	{
		return;
	}

	for (int32 ComponentIndex = M_BoundPCGComponents.Num() - 1; ComponentIndex >= 0; --ComponentIndex)
	{
		UPCGComponent* BoundComponent = M_BoundPCGComponents[ComponentIndex].Get();
		if (BoundComponent == nullptr)
		{
			M_BoundPCGComponents.RemoveAtSwap(ComponentIndex);
			continue;
		}
		if (BoundComponent == SourceComponent)
		{
			return;
		}
	}

	M_BoundPCGComponents.Add(SourceComponent);
	StartInvalidSourceCleanupTimer();
	SourceComponent->OnPCGGraphGeneratedExternal.AddUniqueDynamic(
		this, &ALandscapeDataManager::HandlePCGGraphGenerated);
	SourceComponent->OnPCGGraphCleanedExternal.AddUniqueDynamic(
		this, &ALandscapeDataManager::HandlePCGGraphCleaned);
	SourceComponent->OnPCGGraphCancelledExternal.AddUniqueDynamic(
		this, &ALandscapeDataManager::HandlePCGGraphCancelled);

	AActor* SourceOwner = SourceComponent->GetOwner();
	if (not IsValid(SourceOwner))
	{
		return;
	}
	TrackPCGOwner(*SourceOwner);
}

void ALandscapeDataManager::StartInvalidSourceCleanupTimer()
{
	UWorld* World = GetWorld();
	if (World == nullptr
		|| World->GetTimerManager().IsTimerActive(M_InvalidSourceCleanupTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_InvalidSourceCleanupTimerHandle,
		this,
		&ALandscapeDataManager::HandleInvalidSourceCleanupTimer,
		LandscapeDataManagerConstants::InvalidSourceCleanupIntervalSeconds,
		true);
}

void ALandscapeDataManager::StopInvalidSourceCleanupTimer()
{
	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		World->GetTimerManager().ClearTimer(M_InvalidSourceCleanupTimerHandle);
	}
	M_InvalidSourceCleanupTimerHandle.Invalidate();
}

void ALandscapeDataManager::HandleInvalidSourceCleanupTimer()
{
	TSet<FObjectKey> InvalidSourceComponentKeys;
	GatherInvalidSourceComponentKeys(InvalidSourceComponentKeys);
	bool bRemovedContribution = false;
	for (const FObjectKey& SourceComponentKey : InvalidSourceComponentKeys)
	{
		bRemovedContribution = RemoveContributionsForSource(SourceComponentKey)
			|| bRemovedContribution;
	}

	PruneInvalidBoundPCGComponents();
	if (bRemovedContribution)
	{
		bM_IsDirty = true;
		RebuildLandscapeData();
	}
	if (M_BoundPCGComponents.IsEmpty())
	{
		StopInvalidSourceCleanupTimer();
	}
}

void ALandscapeDataManager::GatherInvalidSourceComponentKeys(
	TSet<FObjectKey>& OutSourceComponentKeys) const
{
	const FObjectKey NativeContributionKey;
	for (const TPair<FGuid, FRTSLandscapeDataManagerContribution>& Entry : M_Contributions)
	{
		const FObjectKey& SourceComponentKey = Entry.Value.SourceComponentKey;
		if (SourceComponentKey != NativeContributionKey && SourceComponentKey.ResolveObjectPtr() == nullptr)
		{
			OutSourceComponentKeys.Add(SourceComponentKey);
		}
	}
	for (const TPair<FGuid, FRTSLandscapeDataManagerContribution>& Entry : M_StagedContributions)
	{
		const FObjectKey& SourceComponentKey = Entry.Value.SourceComponentKey;
		if (SourceComponentKey != NativeContributionKey && SourceComponentKey.ResolveObjectPtr() == nullptr)
		{
			OutSourceComponentKeys.Add(SourceComponentKey);
		}
	}
	for (const TPair<FGuid, FObjectKey>& Entry : M_PendingRemovals)
	{
		if (Entry.Value != NativeContributionKey && Entry.Value.ResolveObjectPtr() == nullptr)
		{
			OutSourceComponentKeys.Add(Entry.Value);
		}
	}
}

void ALandscapeDataManager::PruneInvalidBoundPCGComponents()
{
	for (int32 ComponentIndex = M_BoundPCGComponents.Num() - 1; ComponentIndex >= 0; --ComponentIndex)
	{
		if (M_BoundPCGComponents[ComponentIndex].Get() == nullptr)
		{
			M_BoundPCGComponents.RemoveAtSwap(ComponentIndex);
		}
	}
}

void ALandscapeDataManager::TrackPCGOwner(AActor& SourceOwner)
{
	for (int32 OwnerIndex = M_BoundPCGOwners.Num() - 1; OwnerIndex >= 0; --OwnerIndex)
	{
		AActor* BoundOwner = M_BoundPCGOwners[OwnerIndex].Get();
		if (BoundOwner == nullptr)
		{
			M_BoundPCGOwners.RemoveAtSwap(OwnerIndex);
			continue;
		}
		if (BoundOwner == &SourceOwner)
		{
			return;
		}
	}

	M_BoundPCGOwners.Add(&SourceOwner);
	SourceOwner.OnEndPlay.AddUniqueDynamic(
		this,
		&ALandscapeDataManager::HandleTrackedSourceActorEndPlay);
}

void ALandscapeDataManager::UnbindPCGComponents()
{
	for (const TWeakObjectPtr<UPCGComponent>& WeakComponent : M_BoundPCGComponents)
	{
		UPCGComponent* Component = WeakComponent.Get();
		if (Component == nullptr)
		{
			continue;
		}
		Component->OnPCGGraphGeneratedExternal.RemoveDynamic(
			this, &ALandscapeDataManager::HandlePCGGraphGenerated);
		Component->OnPCGGraphCleanedExternal.RemoveDynamic(
			this, &ALandscapeDataManager::HandlePCGGraphCleaned);
		Component->OnPCGGraphCancelledExternal.RemoveDynamic(
			this, &ALandscapeDataManager::HandlePCGGraphCancelled);
	}
	M_BoundPCGComponents.Reset();
}

void ALandscapeDataManager::UnbindPCGOwners()
{
	for (const TWeakObjectPtr<AActor>& WeakOwner : M_BoundPCGOwners)
	{
		AActor* SourceOwner = WeakOwner.Get();
		if (SourceOwner == nullptr)
		{
			continue;
		}
		SourceOwner->OnEndPlay.RemoveDynamic(
			this,
			&ALandscapeDataManager::HandleTrackedSourceActorEndPlay);
	}
	M_BoundPCGOwners.Reset();
}

void ALandscapeDataManager::HandlePCGGraphGenerated(UPCGComponent* PCGComponent)
{
	if (not IsValid(PCGComponent))
	{
		return;
	}

	const FObjectKey SourceComponentKey(PCGComponent);
	const bool bCommittedContributions = CommitStagedContributions(SourceComponentKey);
	const bool bAppliedRemovals = ApplyPendingRemovals(SourceComponentKey);
	if (bCommittedContributions || bAppliedRemovals)
	{
		bM_IsDirty = true;
	}
	if (bM_IsDirty)
	{
		RebuildLandscapeData();
	}
}

void ALandscapeDataManager::HandlePCGGraphCleaned(UPCGComponent* PCGComponent)
{
	if (not IsValid(PCGComponent))
	{
		return;
	}

	const FObjectKey SourceComponentKey(PCGComponent);
	const bool bAppliedRemovals = ApplyPendingRemovals(SourceComponentKey);
	DiscardStagedContributions(SourceComponentKey);
	if (bAppliedRemovals)
	{
		bM_IsDirty = true;
	}
	if (bM_IsDirty)
	{
		RebuildLandscapeData();
	}
}

void ALandscapeDataManager::HandlePCGGraphCancelled(UPCGComponent* PCGComponent)
{
	if (not IsValid(PCGComponent))
	{
		return;
	}

	const FObjectKey SourceComponentKey(PCGComponent);
	DiscardStagedContributions(SourceComponentKey);
	if (ApplyPendingRemovals(SourceComponentKey))
	{
		bM_IsDirty = true;
		RebuildLandscapeData();
	}
}

void ALandscapeDataManager::HandleTrackedSourceActorEndPlay(
	AActor* Actor,
	const EEndPlayReason::Type EndPlayReason)
{
	if (not IsValid(Actor))
	{
		return;
	}

	bool bRemovedContribution = false;
	for (int32 ComponentIndex = M_BoundPCGComponents.Num() - 1; ComponentIndex >= 0; --ComponentIndex)
	{
		UPCGComponent* SourceComponent = M_BoundPCGComponents[ComponentIndex].Get();
		if (SourceComponent == nullptr || SourceComponent->GetOwner() != Actor)
		{
			continue;
		}

		SourceComponent->OnPCGGraphGeneratedExternal.RemoveDynamic(
			this, &ALandscapeDataManager::HandlePCGGraphGenerated);
		SourceComponent->OnPCGGraphCleanedExternal.RemoveDynamic(
			this, &ALandscapeDataManager::HandlePCGGraphCleaned);
		SourceComponent->OnPCGGraphCancelledExternal.RemoveDynamic(
			this, &ALandscapeDataManager::HandlePCGGraphCancelled);
		bRemovedContribution = RemoveContributionsForSource(FObjectKey(SourceComponent))
			|| bRemovedContribution;
		M_BoundPCGComponents.RemoveAtSwap(ComponentIndex);
	}

	Actor->OnEndPlay.RemoveDynamic(
		this,
		&ALandscapeDataManager::HandleTrackedSourceActorEndPlay);
	M_BoundPCGOwners.Remove(Actor);
	if (bRemovedContribution)
	{
		bM_IsDirty = true;
	}
	if (bM_IsDirty
		&& EndPlayReason != EEndPlayReason::LevelTransition
		&& EndPlayReason != EEndPlayReason::Quit)
	{
		RebuildLandscapeData();
	}
}

void ALandscapeDataManager::RefreshBoundsFromLandscape()
{
	if (not IsInGameThread())
	{
		return;
	}

#if WITH_EDITOR
	if (GetWorld() == nullptr || GetWorld()->IsGameWorld())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Refresh Bounds From Landscape is an editor setup action and cannot run during play."));
		return;
	}
	if (not M_Contributions.IsEmpty()
		|| not M_StagedContributions.IsEmpty()
		|| not M_PendingRemovals.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Clean or clear PCG Landscape data before refreshing its texture bounds."));
		return;
	}
	Modify();
	if (not ResolveLandscape(true))
	{
		return;
	}

	if (not Editor_RefreshCachedCompleteLandscapeBounds())
	{
		return;
	}

	const FRTSLandscapeDataTextureMapping PreviousMapping = M_TextureMapping;
	bM_IsInitialized = false;
	if (not EnsureInitialized())
	{
		M_TextureMapping = PreviousMapping;
	}
	else
	{
		bM_IsDirty = false;
	}
	MarkPackageDirty();
#else
	RTSFunctionLibrary::ReportError(
		TEXT("Refresh Bounds From Landscape is unavailable outside editor builds."));
#endif
}

void ALandscapeDataManager::ClearLandscapeData()
{
	if (not IsInGameThread())
	{
		return;
	}

	M_Contributions.Reset();
	M_StagedContributions.Reset();
	M_PendingRemovals.Reset();
	bM_IsDirty = true;
	RebuildLandscapeData();
}

bool ALandscapeDataManager::GetTexturePixelFromWorldPosition(
	const FVector& WorldPosition,
	FIntPoint& OutPixel) const
{
	if (not M_TextureMapping.IsValid()
		|| not FMath::IsFinite(WorldPosition.X)
		|| not FMath::IsFinite(WorldPosition.Y)
		|| not FMath::IsFinite(WorldPosition.Z))
	{
		return false;
	}

	const FVector2D UV(
		(WorldPosition.X - M_TextureMapping.WorldMinimum.X) * M_TextureMapping.InverseWorldSize.X,
		(WorldPosition.Y - M_TextureMapping.WorldMinimum.Y) * M_TextureMapping.InverseWorldSize.Y);
	if (UV.X < LandscapeDataManagerConstants::MinimumNormalizedValue
		|| UV.X > LandscapeDataManagerConstants::MaximumNormalizedValue
		|| UV.Y < LandscapeDataManagerConstants::MinimumNormalizedValue
		|| UV.Y > LandscapeDataManagerConstants::MaximumNormalizedValue)
	{
		return false;
	}

	OutPixel.X = FMath::Min(
		FMath::FloorToInt32(UV.X * M_TextureMapping.TextureResolution.X),
		M_TextureMapping.TextureResolution.X - 1);
	OutPixel.Y = FMath::Min(
		FMath::FloorToInt32(UV.Y * M_TextureMapping.TextureResolution.Y),
		M_TextureMapping.TextureResolution.Y - 1);
	return OutPixel.X >= 0 && OutPixel.X < M_TextureMapping.TextureResolution.X
		&& OutPixel.Y >= 0 && OutPixel.Y < M_TextureMapping.TextureResolution.Y;
}

float ALandscapeDataManager::GetLandscapeDataAtWorldPosition(
	const FVector& WorldPosition,
	const ERTSLandscapeDataChannel Channel) const
{
	if (not IsInGameThread())
	{
		return 0.0f;
	}

	FIntPoint Pixel;
	if (not GetTexturePixelFromWorldPosition(WorldPosition, Pixel))
	{
		return 0.0f;
	}

	const int32 PixelIndex = Pixel.Y * M_TextureMapping.TextureResolution.X + Pixel.X;
	if (not M_CombinedPixels.IsValidIndex(PixelIndex))
	{
		return 0.0f;
	}
	return static_cast<float>(GetChannelValue(M_CombinedPixels[PixelIndex], Channel))
		/ LandscapeDataManagerConstants::MaximumChannelValue;
}

void ALandscapeDataManager::ApplyChannelMaximum(
	FColor& Pixel,
	const ERTSLandscapeDataChannel Channel,
	const uint8 Value)
{
	switch (Channel)
	{
	case ERTSLandscapeDataChannel::Scorched:
		Pixel.R = FMath::Max(Pixel.R, Value);
		break;
	case ERTSLandscapeDataChannel::Gravel:
		Pixel.G = FMath::Max(Pixel.G, Value);
		break;
	case ERTSLandscapeDataChannel::Concrete:
		Pixel.B = FMath::Max(Pixel.B, Value);
		break;
	case ERTSLandscapeDataChannel::ExtraSand:
		Pixel.A = FMath::Max(Pixel.A, Value);
		break;
	}
}

uint8 ALandscapeDataManager::GetChannelValue(
	const FColor& Pixel,
	const ERTSLandscapeDataChannel Channel)
{
	switch (Channel)
	{
	case ERTSLandscapeDataChannel::Scorched:
		return Pixel.R;
	case ERTSLandscapeDataChannel::Gravel:
		return Pixel.G;
	case ERTSLandscapeDataChannel::Concrete:
		return Pixel.B;
	case ERTSLandscapeDataChannel::ExtraSand:
		return Pixel.A;
	}
	return 0;
}

bool ALandscapeDataManager::ValidateLandscapeDataSetup()
{
	const bool bHasUniqueManager = GetIsUniqueManager(true);
	const bool bIsInitialized = bHasUniqueManager && EnsureInitialized();
	if (bIsInitialized && bM_IsDirty)
	{
		RebuildLandscapeData();
	}
	const bool bHasValidPixelCount = M_TextureMapping.IsValid()
		&& M_CombinedPixels.Num()
		== M_TextureMapping.TextureResolution.X * M_TextureMapping.TextureResolution.Y;
	if (not bHasValidPixelCount)
	{
		RTSFunctionLibrary::ReportError(TEXT("LandscapeDataManager CPU texture data does not match its mapping."));
	}

	const bool bIsTextureBound = bIsInitialized && GetIsPublishedTextureBoundToLandscape();
	if (bIsInitialized && not bIsTextureBound)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("LandscapeDataManager parameters did not reach every loaded Landscape material instance."));
	}
	return bHasUniqueManager
		&& bIsInitialized
		&& bHasValidPixelCount
		&& not bM_IsDirty
		&& bIsTextureBound
		&& GetIsValidLandscape()
		&& GetIsValidLandscapeDataTexture();
}

bool ALandscapeDataManager::GetIsValidLandscape() const
{
	if (M_Landscape.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_Landscape"),
		TEXT("GetIsValidLandscape"),
		this);
	return false;
}

bool ALandscapeDataManager::GetIsValidLandscapeDataTexture() const
{
	if (IsValid(M_LandscapeDataTexture))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_LandscapeDataTexture"),
		TEXT("GetIsValidLandscapeDataTexture"),
		this);
	return false;
}

#if WITH_EDITOR
bool ALandscapeDataManager::Editor_RefreshCachedCompleteLandscapeBounds()
{
	if (not ResolveLandscape(false))
	{
		return false;
	}

	const ALandscape* Landscape = M_Landscape.Get();
	const FBox CompleteBounds = Landscape->GetCompleteBounds();
	if (CompleteBounds.IsValid)
	{
		M_CachedCompleteLandscapeBounds = CompleteBounds;
		return true;
	}
	return false;
}
#endif
