// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSMapObjectFilterSubsystem.h"

#include <atomic>

#include "HAL/IConsoleManager.h"
#include "PixelFormat.h"
#include "Math/Float16.h"
#include "RenderingThread.h"
#include "RHIGPUReadback.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RTSMapObjectFilterSettings.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

#if not UE_BUILD_SHIPPING
namespace MapObjectFilterDebug
{
	// Review tooling: shows every registered object as discovered (check resource layouts, UI snapshots).
	int32 GRevealAll = 0;
	FAutoConsoleVariableRef CVarRevealAll(
		TEXT("RTS.MapObjectFilters.RevealAll"),
		GRevealAll,
		TEXT("1: map object filters treat every radixite, metal and scavenge object as discovered."));
}
#endif

namespace MapObjectFilterConstants
{
	// Only checks whether a readback finished or a refresh is due; the readback itself runs per refresh.
	constexpr float RefreshTickerSeconds = 0.1f;
	constexpr float MaxExploredValue = 255.f;
}

/**
 * Async copy of the explored fog render target. The render thread fills ExploredValues and then sets bIsDone;
 * the game thread only reads them after seeing bIsDone.
 */
struct FRTSExploredFogReadback
{
	FRTSExploredFogReadback()
		: Readback(TEXT("MapObjectFilterExploredFog"))
	{
	}

	FRHIGPUTextureReadback Readback;
	FIntPoint Size = FIntPoint::ZeroValue;
	EPixelFormat Format = PF_Unknown;
	TArray<uint8> ExploredValues;
	std::atomic<bool> bIsDone{false};
	std::atomic<bool> bHasFailed{false};
};

namespace MapObjectFilterReadback
{
	enum class EExploredChannelLayout : uint8
	{
		Unsupported,
		FirstByte,
		ThirdByte,
		FirstHalfFloat,
		FirstFloat
	};

	EExploredChannelLayout GetExploredChannelLayout(const EPixelFormat Format)
	{
		switch (Format)
		{
		case PF_G8:
		case PF_R8:
		case PF_A8:
		case PF_L8:
		case PF_R8G8B8A8:
			return EExploredChannelLayout::FirstByte;
		case PF_B8G8R8A8:
			return EExploredChannelLayout::ThirdByte;
		case PF_FloatRGBA:
		case PF_R16F:
		case PF_G16R16F:
			return EExploredChannelLayout::FirstHalfFloat;
		case PF_R32_FLOAT:
		case PF_G32R32F:
		case PF_A32B32G32R32F:
			return EExploredChannelLayout::FirstFloat;
		default:
			return EExploredChannelLayout::Unsupported;
		}
	}

	uint8 ReadExploredValue(const uint8* const Pixel, const EExploredChannelLayout Layout)
	{
		using namespace MapObjectFilterConstants;
		switch (Layout)
		{
		case EExploredChannelLayout::FirstByte:
			return Pixel[0];
		case EExploredChannelLayout::ThirdByte:
			return Pixel[2];
		case EExploredChannelLayout::FirstHalfFloat:
			{
				FFloat16 HalfValue;
				FMemory::Memcpy(&HalfValue, Pixel, sizeof(FFloat16));
				return static_cast<uint8>(FMath::Clamp(HalfValue.GetFloat(), 0.f, 1.f) * MaxExploredValue);
			}
		case EExploredChannelLayout::FirstFloat:
			{
				float FloatValue = 0.f;
				FMemory::Memcpy(&FloatValue, Pixel, sizeof(float));
				return static_cast<uint8>(FMath::Clamp(FloatValue, 0.f, 1.f) * MaxExploredValue);
			}
		default:
			return 0;
		}
	}

	/** Render thread: converts the mapped staging texture to one explored byte per texel. */
	void ConvertReadbackToExploredValues(FRTSExploredFogReadback& State)
	{
		const EExploredChannelLayout Layout = GetExploredChannelLayout(State.Format);
		const int32 BytesPerPixel = GPixelFormats[State.Format].BlockBytes;
		int32 RowPitchInPixels = 0;
		const uint8* const MappedData = static_cast<const uint8*>(State.Readback.Lock(RowPitchInPixels));
		if (Layout == EExploredChannelLayout::Unsupported || MappedData == nullptr || RowPitchInPixels <= 0)
		{
			if (MappedData != nullptr)
			{
				State.Readback.Unlock();
			}
			State.bHasFailed = true;
			return;
		}
		State.ExploredValues.SetNumUninitialized(State.Size.X * State.Size.Y);
		for (int32 Row = 0; Row < State.Size.Y; ++Row)
		{
			const uint8* const RowData = MappedData + static_cast<SIZE_T>(Row) * RowPitchInPixels * BytesPerPixel;
			for (int32 Column = 0; Column < State.Size.X; ++Column)
			{
				State.ExploredValues[Row * State.Size.X + Column] =
					ReadExploredValue(RowData + Column * BytesPerPixel, Layout);
			}
		}
		State.Readback.Unlock();
	}
}

bool URTSMapObjectFilterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URTSMapObjectFilterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const URTSMapObjectFilterSettings* const Settings = GetDefault<URTSMapObjectFilterSettings>();
	for (const ERTSMapObjectFilter Filter : TEnumRange<ERTSMapObjectFilter>())
	{
		GetTrack(Filter).M_Mode = Settings->GetLook(Filter).DefaultMode;
	}
	// A filter that defaults to the minimap shows from the start; the fog manager may not be ready yet, the
	// ticker simply retries.
	UpdateRefreshTicker();
}

void URTSMapObjectFilterSubsystem::Deinitialize()
{
	if (M_RefreshTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(M_RefreshTickerHandle);
		M_RefreshTickerHandle.Reset();
	}
	// An in-flight copy keeps its own reference on the render thread and finishes harmlessly.
	M_PendingReadback.Reset();
	Super::Deinitialize();
}

void URTSMapObjectFilterSubsystem::RegisterResource(UResourceComponent& Resource)
{
	switch (Resource.GetResourceType())
	{
	case ERTSResourceType::Resource_Radixite:
		GetTrack(ERTSMapObjectFilter::Radixite).M_UndiscoveredObjects.Add(&Resource);
		break;
	case ERTSResourceType::Resource_Metal:
		GetTrack(ERTSMapObjectFilter::Metal).M_UndiscoveredObjects.Add(&Resource);
		break;
	default:
		break;
	}
}

void URTSMapObjectFilterSubsystem::RegisterScavengeable(AScavengeableObject& Scavengeable)
{
	GetTrack(ERTSMapObjectFilter::Scavengeables).M_UndiscoveredObjects.Add(&Scavengeable);
}

void URTSMapObjectFilterSubsystem::SetFilterMode(const ERTSMapObjectFilter Filter,
                                                 const ERTSMapObjectFilterMode NewMode)
{
	GetTrack(Filter).M_Mode = NewMode;
	UpdateRefreshTicker();
}

ERTSMapObjectFilterMode URTSMapObjectFilterSubsystem::GetFilterMode(const ERTSMapObjectFilter Filter) const
{
	return GetTrack(Filter).M_Mode;
}

void URTSMapObjectFilterSubsystem::SetIsTacticalMapOpen(const bool bIsOpen)
{
	bM_IsTacticalMapOpen = bIsOpen;
	UpdateRefreshTicker();
}

bool URTSMapObjectFilterSubsystem::GetShouldDrawFilter(const ERTSMapObjectFilter Filter,
                                                       const bool bIsTacticalMapView) const
{
	switch (GetTrack(Filter).M_Mode)
	{
	case ERTSMapObjectFilterMode::TacticalMapOnly:
		return bIsTacticalMapView;
	case ERTSMapObjectFilterMode::MiniMapAndTacticalMap:
		return true;
	case ERTSMapObjectFilterMode::Off:
	default:
		return false;
	}
}

bool URTSMapObjectFilterSubsystem::GetShouldDrawAnyFilter(const bool bIsTacticalMapView) const
{
	for (const ERTSMapObjectFilter Filter : TEnumRange<ERTSMapObjectFilter>())
	{
		if (GetShouldDrawFilter(Filter, bIsTacticalMapView))
		{
			return true;
		}
	}
	return false;
}

const TArray<FVector2D>& URTSMapObjectFilterSubsystem::GetDiscoveredObjectUVs(const ERTSMapObjectFilter Filter) const
{
	return GetTrack(Filter).M_DiscoveredObjectUVs;
}

void URTSMapObjectFilterSubsystem::GetObjectCounts(const ERTSMapObjectFilter Filter, int32& OutRegistered,
                                                   int32& OutDiscovered) const
{
	const FRTSMapObjectFilterTrack& Track = GetTrack(Filter);
	OutDiscovered = Track.M_DiscoveredObjects.Num();
	OutRegistered = Track.M_UndiscoveredObjects.Num() + OutDiscovered;
}

FRTSMapObjectFilterTrack& URTSMapObjectFilterSubsystem::GetTrack(const ERTSMapObjectFilter Filter)
{
	const int32 TrackIndex = FMath::Clamp(static_cast<int32>(Filter), 0,
	                                      static_cast<int32>(ERTSMapObjectFilter::Count) - 1);
	return M_Tracks[TrackIndex];
}

const FRTSMapObjectFilterTrack& URTSMapObjectFilterSubsystem::GetTrack(const ERTSMapObjectFilter Filter) const
{
	const int32 TrackIndex = FMath::Clamp(static_cast<int32>(Filter), 0,
	                                      static_cast<int32>(ERTSMapObjectFilter::Count) - 1);
	return M_Tracks[TrackIndex];
}

bool URTSMapObjectFilterSubsystem::GetIsFilterTracked(const ERTSMapObjectFilter Filter) const
{
	// Tracked exactly while some open map draws it.
	return GetShouldDrawFilter(Filter, false) || (bM_IsTacticalMapOpen && GetShouldDrawFilter(Filter, true));
}

bool URTSMapObjectFilterSubsystem::GetIsAnyFilterTracked() const
{
	for (const ERTSMapObjectFilter Filter : TEnumRange<ERTSMapObjectFilter>())
	{
		if (GetIsFilterTracked(Filter))
		{
			return true;
		}
	}
	return false;
}

void URTSMapObjectFilterSubsystem::UpdateRefreshTicker()
{
	if (not GetIsAnyFilterTracked())
	{
		if (M_RefreshTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(M_RefreshTickerHandle);
			M_RefreshTickerHandle.Reset();
		}
		return;
	}
	if (not M_RefreshTickerHandle.IsValid())
	{
		M_RefreshTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &URTSMapObjectFilterSubsystem::OnRefreshTicker),
			MapObjectFilterConstants::RefreshTickerSeconds);
	}
	// Show what the last readback knew straight away, and fetch fresh discovery right after.
	ApplyDiscoveryToTrackedFilters();
	M_LastReadbackRequestSeconds = -DBL_MAX;
}

bool URTSMapObjectFilterSubsystem::OnRefreshTicker(const float DeltaTime)
{
	PollExploredFogReadback();
	const double NowSeconds = FPlatformTime::Seconds();
	const float RefreshSeconds = GetDefault<URTSMapObjectFilterSettings>()->DiscoveryRefreshSeconds;
	if (not M_PendingReadback.IsValid() && NowSeconds - M_LastReadbackRequestSeconds >= RefreshSeconds)
	{
		M_LastReadbackRequestSeconds = NowSeconds;
		RequestExploredFogReadback();
	}
	return true;
}

void URTSMapObjectFilterSubsystem::RequestExploredFogReadback()
{
	const AFowManager* const FowManager = GetIsValidFowManager();
	if (not IsValid(FowManager))
	{
		return;
	}
	// The passive render target holds every area the player has explored; it is created when the fog starts.
	UTextureRenderTarget2D* const ExploredFogTarget = FowManager->GetPassiveRTIfCreated();
	if (not IsValid(ExploredFogTarget))
	{
		return;
	}
	FTextureRenderTargetResource* const TargetResource = ExploredFogTarget->GameThread_GetRenderTargetResource();
	if (TargetResource == nullptr || ExploredFogTarget->SizeX <= 0 || ExploredFogTarget->SizeY <= 0)
	{
		return;
	}
	M_PendingReadback = MakeShared<FRTSExploredFogReadback, ESPMode::ThreadSafe>();
	M_PendingReadback->Size = FIntPoint(ExploredFogTarget->SizeX, ExploredFogTarget->SizeY);
	ENQUEUE_RENDER_COMMAND(MapObjectFilterCopyExploredFog)(
		[State = M_PendingReadback, TargetResource](FRHICommandListImmediate& RHICmdList)
		{
			FRHITexture* const FogTexture = TargetResource->GetRenderTargetTexture();
			if (FogTexture == nullptr)
			{
				State->bHasFailed = true;
				State->bIsDone = true;
				return;
			}
			State->Format = FogTexture->GetFormat();
			State->Readback.EnqueueCopy(RHICmdList, FogTexture);
		});
}

void URTSMapObjectFilterSubsystem::PollExploredFogReadback()
{
	if (not M_PendingReadback.IsValid())
	{
		return;
	}
	if (not M_PendingReadback->bIsDone)
	{
		ENQUEUE_RENDER_COMMAND(MapObjectFilterResolveExploredFog)(
			[State = M_PendingReadback](FRHICommandListImmediate& RHICmdList)
			{
				if (State->bIsDone || not State->Readback.IsReady())
				{
					return;
				}
				MapObjectFilterReadback::ConvertReadbackToExploredValues(*State);
				State->bIsDone = true;
			});
		return;
	}
	const TSharedPtr<FRTSExploredFogReadback, ESPMode::ThreadSafe> FinishedReadback = MoveTemp(M_PendingReadback);
	if (FinishedReadback->bHasFailed)
	{
		if (not bM_HasReportedUnsupportedFogFormat)
		{
			bM_HasReportedUnsupportedFogFormat = true;
			RTSFunctionLibrary::ReportError(
				"Map object filters could not read the explored fog render target (pixel format "
				+ FString(GetPixelFormatString(FinishedReadback->Format)) + ")."
				"\n See URTSMapObjectFilterSubsystem::PollExploredFogReadback");
		}
		return;
	}
	M_ExploredValues = MoveTemp(FinishedReadback->ExploredValues);
	M_ExploredSize = FinishedReadback->Size;
	ApplyDiscoveryToTrackedFilters();
}

void URTSMapObjectFilterSubsystem::ApplyDiscoveryToTrackedFilters()
{
	if (M_ExploredValues.IsEmpty())
	{
		return;
	}
	const AFowManager* const FowManager = GetIsValidFowManager();
	if (not IsValid(FowManager))
	{
		return;
	}
	const URTSMapObjectFilterSettings* const Settings = GetDefault<URTSMapObjectFilterSettings>();
	for (const ERTSMapObjectFilter Filter : TEnumRange<ERTSMapObjectFilter>())
	{
		if (GetIsFilterTracked(Filter))
		{
			ApplyDiscovery(Filter, *FowManager, *Settings);
		}
	}
}

void URTSMapObjectFilterSubsystem::ApplyDiscovery(const ERTSMapObjectFilter Filter, const AFowManager& FowManager,
                                                  const URTSMapObjectFilterSettings& Settings)
{
	FRTSMapObjectFilterTrack& Track = GetTrack(Filter);
	MoveDiscoveredObjects(Track, Filter, FowManager, Settings);
	RebuildDiscoveredObjectUVs(Track, Filter, FowManager);
}

void URTSMapObjectFilterSubsystem::MoveDiscoveredObjects(FRTSMapObjectFilterTrack& Track,
                                                         const ERTSMapObjectFilter Filter,
                                                         const AFowManager& FowManager,
                                                         const URTSMapObjectFilterSettings& Settings) const
{
	// Backwards with swap-removal: the element swapped in from the end was already visited.
	TArray<TWeakObjectPtr<UObject>>& Undiscovered = Track.M_UndiscoveredObjects;
	for (int32 ObjectIndex = Undiscovered.Num() - 1; ObjectIndex >= 0; --ObjectIndex)
	{
		const UObject* const Object = Undiscovered[ObjectIndex].Get();
		FVector2D MiniMapUV;
		const bool bIsGone = not IsValid(Object) || not GetIsObjectStillOnMap(*Object, Filter)
			|| not GetObjectMiniMapUV(*Object, FowManager, MiniMapUV);
		if (bIsGone)
		{
			Undiscovered.RemoveAtSwap(ObjectIndex, EAllowShrinking::No);
			continue;
		}
		if (GetIsUVDiscovered(MiniMapUV, Settings))
		{
			Track.M_DiscoveredObjects.Add(Undiscovered[ObjectIndex]);
			Undiscovered.RemoveAtSwap(ObjectIndex, EAllowShrinking::No);
		}
	}
}

void URTSMapObjectFilterSubsystem::RebuildDiscoveredObjectUVs(FRTSMapObjectFilterTrack& Track,
                                                              const ERTSMapObjectFilter Filter,
                                                              const AFowManager& FowManager) const
{
	Track.M_DiscoveredObjectUVs.Reset();
	TArray<TWeakObjectPtr<UObject>>& Discovered = Track.M_DiscoveredObjects;
	for (int32 ObjectIndex = Discovered.Num() - 1; ObjectIndex >= 0; --ObjectIndex)
	{
		const UObject* const Object = Discovered[ObjectIndex].Get();
		FVector2D MiniMapUV;
		if (not IsValid(Object) || not GetIsObjectStillOnMap(*Object, Filter)
			|| not GetObjectMiniMapUV(*Object, FowManager, MiniMapUV))
		{
			Discovered.RemoveAtSwap(ObjectIndex, EAllowShrinking::No);
			continue;
		}
		if (GetIsObjectShown(*Object, Filter))
		{
			Track.M_DiscoveredObjectUVs.Add(MiniMapUV);
		}
	}
}

bool URTSMapObjectFilterSubsystem::GetIsUVDiscovered(const FVector2D& MiniMapUV,
                                                     const URTSMapObjectFilterSettings& Settings) const
{
#if not UE_BUILD_SHIPPING
	if (MapObjectFilterDebug::GRevealAll != 0)
	{
		return true;
	}
#endif
	if (M_ExploredSize.X <= 0 || M_ExploredSize.Y <= 0)
	{
		return false;
	}
	// Minimap UVs sit half a texel before the fog texel centres (see AFowManager::GetMiniMapUVFromWorldLocation).
	constexpr float HalfTexel = 0.5f;
	const int32 CenterColumn = FMath::FloorToInt32(MiniMapUV.X * M_ExploredSize.X + HalfTexel);
	const int32 CenterRow = FMath::FloorToInt32(MiniMapUV.Y * M_ExploredSize.Y + HalfTexel);
	const uint8 Threshold = static_cast<uint8>(
		FMath::Clamp(Settings.DiscoveredThreshold, 0.f, 1.f) * MapObjectFilterConstants::MaxExploredValue);
	const int32 Radius = Settings.DiscoverySampleRadiusTexels;
	for (int32 Row = FMath::Max(0, CenterRow - Radius); Row <= FMath::Min(M_ExploredSize.Y - 1, CenterRow + Radius);
	     ++Row)
	{
		for (int32 Column = FMath::Max(0, CenterColumn - Radius);
		     Column <= FMath::Min(M_ExploredSize.X - 1, CenterColumn + Radius); ++Column)
		{
			if (M_ExploredValues[Row * M_ExploredSize.X + Column] >= Threshold)
			{
				return true;
			}
		}
	}
	return false;
}

bool URTSMapObjectFilterSubsystem::GetIsObjectStillOnMap(const UObject& Object, const ERTSMapObjectFilter Filter)
{
	if (Filter == ERTSMapObjectFilter::Scavengeables)
	{
		const AScavengeableObject* const Scavengeable = Cast<AScavengeableObject>(&Object);
		return IsValid(Scavengeable) && not Scavengeable->GetIsScavengedOrDestroyed();
	}
	const UResourceComponent* const Resource = Cast<UResourceComponent>(&Object);
	return IsValid(Resource) && Resource->StillContainsResources();
}

bool URTSMapObjectFilterSubsystem::GetIsObjectShown(const UObject& Object, const ERTSMapObjectFilter Filter)
{
	if (Filter != ERTSMapObjectFilter::Scavengeables)
	{
		return true;
	}
	const AScavengeableObject* const Scavengeable = Cast<AScavengeableObject>(&Object);
	return IsValid(Scavengeable) && Scavengeable->GetCanScavenge();
}

bool URTSMapObjectFilterSubsystem::GetObjectMiniMapUV(const UObject& Object, const AFowManager& FowManager,
                                                      FVector2D& OutUV)
{
	const AActor* ObjectActor = Cast<AActor>(&Object);
	if (const UActorComponent* const ObjectComponent = Cast<UActorComponent>(&Object))
	{
		ObjectActor = ObjectComponent->GetOwner();
	}
	if (not IsValid(ObjectActor))
	{
		return false;
	}
	return FowManager.GetMiniMapUVFromWorldLocation(ObjectActor->GetActorLocation(), OutUV);
}

const AFowManager* URTSMapObjectFilterSubsystem::GetIsValidFowManager()
{
	if (M_FowManager.IsValid())
	{
		return M_FowManager.Get();
	}
	M_FowManager = FRTS_Statics::GetFowManager(this);
	if (M_FowManager.IsValid())
	{
		return M_FowManager.Get();
	}
	// The ticker retries every refresh; report a missing manager once.
	if (not bM_HasReportedMissingFowManager)
	{
		bM_HasReportedMissingFowManager = true;
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_FowManager", "GetIsValidFowManager",
		                                                            this);
	}
	return nullptr;
}
