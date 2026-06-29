// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldFowManager.h"

#include "Components/MeshComponent.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowCloud.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldMapFowComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"

namespace WorldFowMaskConstants
{
	constexpr int32 VisibleChannelIndex = 0;
	constexpr int32 ExplorableChannelIndex = 1;
	constexpr int32 POIChannelIndex = 2;
	constexpr int32 NoMaskChannelIndex = -1;
	constexpr int32 MinimumLineRasterizationSamples = 32;
	constexpr int32 MaximumLineRasterizationSamples = 128;
	constexpr int32 MinimumMaskResolution = 8;
	constexpr float HalfUVOffset = 0.5f;
}

AWorldFowManager::AWorldFowManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWorldFowManager::InitializeWorldFow(
	AWorldPlayerController* WorldPlayerController,
	AGeneratorWorldCampaign* WorldGenerator)
{
	M_WorldPlayerController = WorldPlayerController;
	M_WorldGenerator = WorldGenerator;

	CacheCloudActor();
	UpdateFowFromCurrentWorldState();
}

void AWorldFowManager::UpdateFowFromCurrentWorldState()
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	InitializeMissingCampaignActorStates();
	ApplyRevealRulesFromVisibleAnchors();
	ApplyWorldObjectStates();
	ApplyConnectionStates();
	if (not GetIsValidWorldFowCloud())
	{
		return;
	}

	RebuildMaskTexture();
	PushMaskToCloudMaterial();
}

void AWorldFowManager::Debug_RevealAll()
{
	if constexpr (DeveloperSettings::Debugging::GWorldFow_Compile_DebugSymbols)
	{
		if (not GetIsValidWorldGenerator())
		{
			return;
		}

		const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
		{
			SetAnchorState(AnchorPoint, EWorldMapFowState::Visible);
		}

		ApplyWorldObjectStates();
		ApplyConnectionStates();
		RebuildMaskTexture();
		PushMaskToCloudMaterial();
	}
}

void AWorldFowManager::Debug_HideAll()
{
	if constexpr (DeveloperSettings::Debugging::GWorldFow_Compile_DebugSymbols)
	{
		if (not GetIsValidWorldGenerator())
		{
			return;
		}

		const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
		InitializeMissingCampaignActorStates();
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
		{
			SetAnchorState(AnchorPoint, EWorldMapFowState::NotVisible);
		}
		ApplyWorldObjectStates();
		ApplyConnectionStates();
		RebuildMaskTexture();
		PushMaskToCloudMaterial();
	}
}

bool AWorldFowManager::GetIsValidWorldGenerator() const
{
	if (M_WorldGenerator.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldGenerator"),
		TEXT("GetIsValidWorldGenerator"),
		this
	);
	return false;
}

bool AWorldFowManager::GetIsValidWorldFowCloud() const
{
	if (M_WorldFowCloud.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldFowCloud"),
		TEXT("GetIsValidWorldFowCloud"),
		this
	);
	return false;
}

void AWorldFowManager::CacheCloudActor()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	for (TActorIterator<AWorldFowCloud> CloudIterator(World); CloudIterator; ++CloudIterator)
	{
		M_WorldFowCloud = *CloudIterator;
		return;
	}
}

void AWorldFowManager::InitializeMissingCampaignActorStates()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		if (M_AnchorStates.Contains(AnchorPoint))
		{
			continue;
		}

		SetAnchorState(AnchorPoint, EWorldMapFowState::NotVisible);
	}

	for (const TObjectPtr<AConnection>& Connection : PlacementState.CachedConnections)
	{
		SetConnectionState(Connection, EWorldMapFowState::NotVisible);
	}
}

void AWorldFowManager::ApplyRevealRulesFromVisibleAnchors()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	AAnchorPoint* PlayerHQAnchor = PlacementState.PlayerHQAnchor;
	if (IsValid(PlayerHQAnchor))
	{
		SetAnchorState(PlayerHQAnchor, EWorldMapFowState::Visible);
	}

	TArray<AAnchorPoint*> VisibleAnchors;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (IsValid(AnchorPoint) && GetAnchorState(AnchorPoint) == EWorldMapFowState::Visible)
		{
			VisibleAnchors.Add(AnchorPoint);
		}
	}

	for (AAnchorPoint* VisibleAnchor : VisibleAnchors)
	{
		const TArray<TObjectPtr<AAnchorPoint>>& NeighborAnchors = VisibleAnchor->GetNeighborAnchors();
		for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : NeighborAnchors)
		{
			if (GetAnchorState(NeighborAnchor) == EWorldMapFowState::NotVisible)
			{
				SetAnchorState(NeighborAnchor, EWorldMapFowState::Explorable);
			}
		}
	}
}

void AWorldFowManager::ApplyWorldObjectStates()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AWorldMapObject* PromotedWorldObject = AnchorPoint->GetPromotedWorldObject();
		SetObjectState(PromotedWorldObject, GetAnchorState(AnchorPoint));
	}
}

void AWorldFowManager::ApplyConnectionStates()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AConnection>& Connection : PlacementState.CachedConnections)
	{
		SetConnectionState(Connection, GetConnectionStateFromConnectedAnchors(Connection));
	}
}

void AWorldFowManager::RebuildMaskTexture()
{
	M_MaskResolution = FMath::Max(M_MaskResolution, WorldFowMaskConstants::MinimumMaskResolution);
	const int32 PixelCount = M_MaskResolution * M_MaskResolution;
	M_MaskPixels.Init(FColor::Black, PixelCount);

	StampAnchorsToMask();
	StampConnectionsToMask();
	StampWorldObjectsToMask();
}

void AWorldFowManager::StampAnchorsToMask()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		UWorldMapFowComponent* FowComponent = AnchorPoint->GetFowComponent();
		const int32 ChannelIndex = GetMaskChannelForComponent(FowComponent);
		if (ChannelIndex == WorldFowMaskConstants::NoMaskChannelIndex)
		{
			continue;
		}

		StampCircle(
			AnchorPoint->GetActorLocation(),
			FowComponent->GetRevealRadiusForCurrentState(),
			FowComponent->GetRevealFalloffForCurrentState(),
			ChannelIndex
		);
	}
}

void AWorldFowManager::StampConnectionsToMask()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AConnection>& Connection : PlacementState.CachedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
		StampConnectionSegment(Connection, ConnectedAnchors.IsValidIndex(0) ? ConnectedAnchors[0].Get() : nullptr,
			ConnectedAnchors.IsValidIndex(1) ? ConnectedAnchors[1].Get() : nullptr);

		if (ConnectedAnchors.IsValidIndex(2))
		{
			StampConnectionSegment(Connection, ConnectedAnchors[2].Get(), nullptr);
		}
	}
}

void AWorldFowManager::StampWorldObjectsToMask()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AWorldMapObject* PromotedWorldObject = AnchorPoint->GetPromotedWorldObject();
		if (not IsValid(PromotedWorldObject))
		{
			continue;
		}

		UWorldMapFowComponent* FowComponent = PromotedWorldObject->GetFowComponent();
		if (not IsValid(FowComponent) || not FowComponent->GetWritesPOIMaskForCurrentState())
		{
			continue;
		}

		StampCircle(
			FowComponent->GetPOIRevealOrigin(),
			FowComponent->GetPOIRevealRadius(),
			FowComponent->GetPOIRevealFalloff(),
			WorldFowMaskConstants::POIChannelIndex
		);
	}
}

void AWorldFowManager::PushMaskToCloudMaterial()
{
	if (not GetIsValidWorldFowCloud())
	{
		return;
	}

	if (M_MaskPixels.Num() != M_MaskResolution * M_MaskResolution)
	{
		return;
	}

	if (not IsValid(M_MaskTexture))
	{
		M_MaskTexture = UTexture2D::CreateTransient(M_MaskResolution, M_MaskResolution, PF_B8G8R8A8);
	}

	if (not IsValid(M_MaskTexture))
	{
		return;
	}

	void* TextureData = M_MaskTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, M_MaskPixels.GetData(), M_MaskPixels.Num() * sizeof(FColor));
	M_MaskTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	M_MaskTexture->UpdateResource();

	UMeshComponent* CloudMeshComponent = M_WorldFowCloud->GetCloudMeshComponent();
	if (not IsValid(CloudMeshComponent))
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = CloudMeshComponent->CreateAndSetMaterialInstanceDynamic(
		M_WorldFowCloudMaterialSlot
	);
	if (not IsValid(DynamicMaterial))
	{
		return;
	}

	DynamicMaterial->SetTextureParameterValue(M_WorldFowMaskParameterName, M_MaskTexture);
}

void AWorldFowManager::SetAnchorState(AAnchorPoint* AnchorPoint, const EWorldMapFowState NewState)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	M_AnchorStates.FindOrAdd(AnchorPoint) = NewState;
	UWorldMapFowComponent* FowComponent = AnchorPoint->GetFowComponent();
	if (not IsValid(FowComponent))
	{
		return;
	}

	FowComponent->SetCurrentFowState(NewState);
}

void AWorldFowManager::SetObjectState(AWorldMapObject* WorldMapObject, const EWorldMapFowState NewState)
{
	if (not IsValid(WorldMapObject))
	{
		return;
	}

	UWorldMapFowComponent* FowComponent = WorldMapObject->GetFowComponent();
	if (not IsValid(FowComponent))
	{
		return;
	}

	if (NewState == EWorldMapFowState::NotVisible && FowComponent->GetCanBecomePOIVisible())
	{
		FowComponent->SetCurrentFowState(EWorldMapFowState::POIVisible);
		return;
	}

	FowComponent->SetCurrentFowState(NewState);
}

void AWorldFowManager::SetConnectionState(AConnection* Connection, const EWorldMapFowState NewState)
{
	if (not IsValid(Connection))
	{
		return;
	}

	UWorldMapFowComponent* FowComponent = Connection->GetFowComponent();
	if (not IsValid(FowComponent))
	{
		return;
	}

	FowComponent->SetCurrentFowState(NewState);
}

void AWorldFowManager::StampConnectionSegment(
	const AConnection* Connection,
	const AAnchorPoint* StartAnchor,
	const AAnchorPoint* EndAnchor)
{
	if (not IsValid(Connection) || not IsValid(StartAnchor))
	{
		return;
	}

	const UWorldMapFowComponent* FowComponent = Connection->GetFowComponent();
	const int32 ChannelIndex = GetMaskChannelForComponent(FowComponent);
	if (ChannelIndex == WorldFowMaskConstants::NoMaskChannelIndex)
	{
		return;
	}

	const FVector SegmentEnd = IsValid(EndAnchor) ? EndAnchor->GetActorLocation() : Connection->GetJunctionLocation();
	StampLine(
		StartAnchor->GetActorLocation(),
		SegmentEnd,
		FowComponent->GetConnectionCorridorWidthForCurrentState(),
		FowComponent->GetRevealFalloffForCurrentState(),
		ChannelIndex
	);
}

void AWorldFowManager::StampCircle(
	const FVector& WorldLocation,
	const float Radius,
	const float Falloff,
	const int32 ChannelIndex)
{
	const FIntPoint CenterPixel = WorldToPixel(WorldLocation);
	const int32 PixelRadius = GetPixelRadius(Radius + Falloff);
	const int32 HardPixelRadius = GetPixelRadius(Radius);
	const int32 MinimumPixelX = FMath::Max(0, CenterPixel.X - PixelRadius);
	const int32 MaximumPixelX = FMath::Min(M_MaskResolution - 1, CenterPixel.X + PixelRadius);
	const int32 MinimumPixelY = FMath::Max(0, CenterPixel.Y - PixelRadius);
	const int32 MaximumPixelY = FMath::Min(M_MaskResolution - 1, CenterPixel.Y + PixelRadius);

	for (int32 PixelY = MinimumPixelY; PixelY <= MaximumPixelY; ++PixelY)
	{
		for (int32 PixelX = MinimumPixelX; PixelX <= MaximumPixelX; ++PixelX)
		{
			const float Distance = FVector2D::Distance(FVector2D(PixelX, PixelY), FVector2D(CenterPixel));
			if (Distance > PixelRadius)
			{
				continue;
			}

			float FalloffAlpha = 1.f;
			if (PixelRadius > HardPixelRadius)
			{
				FalloffAlpha = FMath::GetMappedRangeValueClamped(
					FVector2D(HardPixelRadius, PixelRadius),
					FVector2D(1.f, 0.f),
					Distance
				);
			}

			const uint8 PixelValue = static_cast<uint8>(FMath::RoundToInt(FalloffAlpha * 255.f));
			FColor& Pixel = M_MaskPixels[PixelY * M_MaskResolution + PixelX];
			WriteMaskPixelChannel(Pixel, ChannelIndex, PixelValue);
		}
	}
}

void AWorldFowManager::StampLine(
	const FVector& Start,
	const FVector& End,
	const float Width,
	const float Falloff,
	const int32 ChannelIndex)
{
	const int32 LineRasterizationSampleCount = GetLineRasterizationSampleCount(Start, End);
	for (int32 StepIndex = 0; StepIndex <= LineRasterizationSampleCount; ++StepIndex)
	{
		const float Alpha = static_cast<float>(StepIndex) / LineRasterizationSampleCount;
		StampCircle(FMath::Lerp(Start, End, Alpha), Width, Falloff, ChannelIndex);
	}
}

int32 AWorldFowManager::GetLineRasterizationSampleCount(const FVector& Start, const FVector& End) const
{
	const float CorridorLength = FVector::Dist2D(Start, End);
	const FVector2D MapSize = GetIsValidWorldFowCloud() ? M_WorldFowCloud->GetMapSizeXY() : FVector2D::ZeroVector;
	const float MapDiagonal = FMath::Max(1.0f, MapSize.Size());
	const float LengthAlpha = FMath::Clamp(CorridorLength / MapDiagonal, 0.0f, 1.0f);
	const float BaseSampleCount = FMath::Lerp(
		static_cast<float>(WorldFowMaskConstants::MinimumLineRasterizationSamples),
		static_cast<float>(WorldFowMaskConstants::MaximumLineRasterizationSamples),
		LengthAlpha
	);

	return FMath::Max(1, FMath::RoundToInt(BaseSampleCount * FMath::Max(0.0f, M_LineRasterizationSamplingMlt)));
}

void AWorldFowManager::WriteMaskPixelChannel(FColor& Pixel, const int32 ChannelIndex, const uint8 Value) const
{
	if (ChannelIndex == WorldFowMaskConstants::VisibleChannelIndex)
	{
		Pixel.R = FMath::Max(Pixel.R, Value);
		return;
	}

	if (ChannelIndex == WorldFowMaskConstants::ExplorableChannelIndex)
	{
		Pixel.G = FMath::Max(Pixel.G, Value);
		return;
	}

	if (ChannelIndex == WorldFowMaskConstants::POIChannelIndex)
	{
		Pixel.B = FMath::Max(Pixel.B, Value);
	}
}

int32 AWorldFowManager::GetMaskChannelForComponent(const UWorldMapFowComponent* FowComponent) const
{
	if (not IsValid(FowComponent))
	{
		return WorldFowMaskConstants::NoMaskChannelIndex;
	}

	if (FowComponent->GetWritesVisibleMaskForCurrentState())
	{
		return WorldFowMaskConstants::VisibleChannelIndex;
	}

	if (FowComponent->GetWritesExplorableMaskForCurrentState())
	{
		return WorldFowMaskConstants::ExplorableChannelIndex;
	}

	if (FowComponent->GetWritesPOIMaskForCurrentState())
	{
		return WorldFowMaskConstants::POIChannelIndex;
	}

	return WorldFowMaskConstants::NoMaskChannelIndex;
}

int32 AWorldFowManager::GetPixelRadius(const float WorldRadius) const
{
	if (not GetIsValidWorldFowCloud())
	{
		return FMath::Max(1, FMath::CeilToInt(WorldRadius));
	}

	const FVector2D MapSize = M_WorldFowCloud->GetMapSizeXY();
	const float SmallestMapAxis = FMath::Max(1.f, FMath::Min(MapSize.X, MapSize.Y));
	return FMath::Max(1, FMath::CeilToInt(WorldRadius / SmallestMapAxis * M_MaskResolution));
}

EWorldMapFowState AWorldFowManager::GetAnchorState(const AAnchorPoint* AnchorPoint) const
{
	const EWorldMapFowState* FoundState = M_AnchorStates.Find(AnchorPoint);
	if (FoundState == nullptr)
	{
		return EWorldMapFowState::NotVisible;
	}

	return *FoundState;
}

EWorldMapFowState AWorldFowManager::GetConnectionStateFromConnectedAnchors(const AConnection* Connection) const
{
	if (not IsValid(Connection))
	{
		return EWorldMapFowState::NotVisible;
	}

	int32 VisibleAnchorCount = 0;
	int32 ExplorableAnchorCount = 0;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : Connection->GetConnectedAnchors())
	{
		const EWorldMapFowState AnchorState = GetAnchorState(AnchorPoint);
		VisibleAnchorCount += AnchorState == EWorldMapFowState::Visible ? 1 : 0;
		ExplorableAnchorCount += AnchorState == EWorldMapFowState::Explorable ? 1 : 0;
	}

	if (VisibleAnchorCount >= 2)
	{
		return EWorldMapFowState::Visible;
	}

	if (VisibleAnchorCount >= 1 && ExplorableAnchorCount >= 1)
	{
		return EWorldMapFowState::Explorable;
	}

	return EWorldMapFowState::NotVisible;
}

FIntPoint AWorldFowManager::WorldToPixel(const FVector& WorldLocation) const
{
	if (not GetIsValidWorldFowCloud())
	{
		const int32 CenterPixel = M_MaskResolution / 2;
		return FIntPoint(CenterPixel, CenterPixel);
	}

	const FVector MapCenter = M_WorldFowCloud->GetActorLocation();
	const FVector2D MapSize = M_WorldFowCloud->GetMapSizeXY();
	const float MaskU = (WorldLocation.X - MapCenter.X) / FMath::Max(1.f, MapSize.X)
		+ WorldFowMaskConstants::HalfUVOffset;
	const float MaskV = (WorldLocation.Y - MapCenter.Y) / FMath::Max(1.f, MapSize.Y)
		+ WorldFowMaskConstants::HalfUVOffset;

	return FIntPoint(
		FMath::Clamp(FMath::RoundToInt(MaskU * (M_MaskResolution - 1)), 0, M_MaskResolution - 1),
		FMath::Clamp(FMath::RoundToInt(MaskV * (M_MaskResolution - 1)), 0, M_MaskResolution - 1)
	);
}
