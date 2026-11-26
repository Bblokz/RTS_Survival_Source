// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldFowManager.h"

#include "Containers/Queue.h"
#include "Components/MeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintRules.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintSettings.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowCloud.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldMapFowComponent.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldUnknownMissionMarkerComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSWorldFogHints, Log, All);

#if !UE_BUILD_SHIPPING
// Counts for the RTS_WORLDFOGHINTS log line, which headless runs read to check the fog hints.
namespace WorldFogHintSummary
{
	struct FCounts
	{
		int32 Missions = 0;
		int32 MissionsAsMarker = 0;
		int32 MissionsAsRealPieceThroughFog = 0;
		int32 EnemyObjects = 0;
		int32 EnemyObjectsUnderFog = 0;
		int32 Divisions = 0;
		// Must stay 0: enemy objects and divisions get no marker and are not touched by the fog hints.
		int32 EnemyPiecesWithMarker = 0;
		// Negative while no mission shows the marker.
		float NearestMarkerDistanceToHQ = -1.f;
	};

	bool GetHasUnknownMarker(const AActor& MapPiece)
	{
		return MapPiece.FindComponentByClass<UWorldUnknownMissionMarkerComponent>() != nullptr;
	}

	void CountMapObject(const AWorldMapObject& MapObject, const EWorldMapFowState AnchorState,
	                    const FVector& PlayerHQLocation, FCounts& Counts)
	{
		if (const AWorldMissionObject* const MissionObject = Cast<AWorldMissionObject>(&MapObject))
		{
			++Counts.Missions;
			if (not MissionObject->GetIsUnidentifiedUnderFog())
			{
				return;
			}

			Counts.MissionsAsMarker += MissionObject->GetIsShownAsUnknownMarker() ? 1 : 0;
			Counts.MissionsAsRealPieceThroughFog += MissionObject->GetIsShownAsUnknownMarker() ? 0 : 1;
			const float DistanceToHQ = FVector::Dist2D(MapObject.GetActorLocation(), PlayerHQLocation);
			const bool bIsNearest = Counts.NearestMarkerDistanceToHQ < 0.f || DistanceToHQ < Counts.NearestMarkerDistanceToHQ;
			Counts.NearestMarkerDistanceToHQ = bIsNearest ? DistanceToHQ : Counts.NearestMarkerDistanceToHQ;
			return;
		}

		if (not MapObject.IsA(AWorldEnemyObject::StaticClass()))
		{
			return;
		}

		++Counts.EnemyObjects;
		Counts.EnemyObjectsUnderFog += AnchorState == EWorldMapFowState::NotVisible ? 1 : 0;
		Counts.EnemyPiecesWithMarker += GetHasUnknownMarker(MapObject) ? 1 : 0;
	}
}
#endif

namespace WorldFowDebugConstants
{
	constexpr float WorldStringDurationSeconds = 10.f;
	constexpr float WorldStringZOffset = 250.f;
	const FColor VisibleColor = FColor::Green;
	const FColor ExplorableColor = FColor::Yellow;
	const FColor POIVisibleColor = FColor::Cyan;
	const FColor HiddenColor = FColor::Red;
}

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

	BindToCampaignProgress();
	CacheCloudActor();
	UpdateFowFromCurrentWorldState();
}

void AWorldFowManager::RefreshWorldFow()
{
	UpdateFowFromCurrentWorldState();
}

void AWorldFowManager::SetRevealSource(const FWorldFowRevealSource& RevealSource)
{
	if (RevealSource.SourceId.IsNone())
	{
		RTSFunctionLibrary::ReportError(TEXT("AWorldFowManager::SetRevealSource called with a reveal source without id."));
		return;
	}

	M_RevealSourcesById.Add(RevealSource.SourceId, RevealSource);
}

void AWorldFowManager::SetRevealSources(const TArray<FWorldFowRevealSource>& RevealSources)
{
	M_RevealSourcesById.Reset();
	for (const FWorldFowRevealSource& RevealSource : RevealSources)
	{
		SetRevealSource(RevealSource);
	}
}

void AWorldFowManager::RemoveRevealSource(const FName SourceId)
{
	M_RevealSourcesById.Remove(SourceId);
}

TArray<FWorldFowRevealSource> AWorldFowManager::GetRevealSources() const
{
	TArray<FWorldFowRevealSource> RevealSources;
	M_RevealSourcesById.GenerateValueArray(RevealSources);
	return RevealSources;
}

void AWorldFowManager::BindToCampaignProgress()
{
	UnbindFromCampaignProgress();
	if (not M_WorldPlayerController.IsValid())
	{
		return;
	}

	M_WorldCampaignProgressManager = M_WorldPlayerController->FindComponentByClass<UWorldCampaignProgressManager>();
	bM_HasCampaignProgress = true;
	if (not GetIsValidWorldCampaignProgressManager())
	{
		bM_HasCampaignProgress = false;
		return;
	}

	M_AnchorClearedHandle = M_WorldCampaignProgressManager->OnAnchorCleared.AddUObject(
		this, &AWorldFowManager::OnAnchorCleared);
}

void AWorldFowManager::UnbindFromCampaignProgress()
{
	if (M_WorldCampaignProgressManager.IsValid() && M_AnchorClearedHandle.IsValid())
	{
		M_WorldCampaignProgressManager->OnAnchorCleared.Remove(M_AnchorClearedHandle);
	}

	M_AnchorClearedHandle.Reset();
	bM_HasCampaignProgress = false;
}

void AWorldFowManager::OnAnchorCleared(const FGuid& /*AnchorKey*/)
{
	// A cleared anchor becomes Visible and floods onward, so the whole reveal pass has to run again.
	RefreshWorldFow();
}

void AWorldFowManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromCampaignProgress();
	M_RevealSourcesById.Empty();
	M_WorldCampaignProgressManager.Reset();
	M_WorldPlayerController.Reset();
	M_WorldGenerator.Reset();
	M_WorldFowCloud.Reset();
	M_MaskTexture = nullptr;
	M_MaskPixels.Empty();
	M_RevealedPixels.Empty();
	M_AnchorStates.Empty();

	Super::EndPlay(EndPlayReason);
}

void AWorldFowManager::UpdateFowFromCurrentWorldState()
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	InitializeMissingCampaignActorStates();
	ApplyPersistentRevealsToAnchors();
	ApplyRevealRulesFromVisibleAnchors();
	ApplyWorldObjectStates();
	ApplyConnectionStates();
	if (not GetIsValidWorldFowCloud())
	{
		// Without a cloud nothing is hidden; the missions are still told, so none keeps an old hint.
		ApplyMissionFogHints();
		NotifyFowRefreshed();
		return;
	}

	RebuildMaskTexture();
	PushMaskToCloudMaterial();
	NotifyFowRefreshed();
}

void AWorldFowManager::NotifyFowRefreshed()
{
	OnWorldFowRefreshed.Broadcast();
	LogFogHintSummary();
}

void AWorldFowManager::LogFogHintSummary() const
{
#if !UE_BUILD_SHIPPING
	if (not M_WorldGenerator.IsValid())
	{
		return;
	}

	WorldFogHintSummary::FCounts Counts;
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	const AAnchorPoint* const PlayerHQAnchor = PlacementState.PlayerHQAnchor;
	const FVector PlayerHQLocation = IsValid(PlayerHQAnchor) ? PlayerHQAnchor->GetActorLocation() : FVector::ZeroVector;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		const AWorldMapObject* const MapObject = IsValid(AnchorPoint) ? AnchorPoint->GetPromotedWorldObject() : nullptr;
		if (IsValid(MapObject))
		{
			WorldFogHintSummary::CountMapObject(*MapObject, GetAnchorState(AnchorPoint), PlayerHQLocation, Counts);
		}
	}

	for (TActorIterator<AWorldDivisionBase> DivisionIterator(GetWorld()); DivisionIterator; ++DivisionIterator)
	{
		++Counts.Divisions;
		Counts.EnemyPiecesWithMarker += WorldFogHintSummary::GetHasUnknownMarker(**DivisionIterator) ? 1 : 0;
	}

	const bool bHasCloud = M_WorldFowCloud.IsValid();
	UE_LOG(LogRTSWorldFogHints, Display,
	       TEXT("RTS_WORLDFOGHINTS missions=%d asMarker=%d asRealPieceThroughFog=%d identified=%d "
		       "nearestUnidentifiedFromHQ=%.0f enemyObjects=%d enemyObjectsUnderFog=%d divisions=%d "
		       "enemyPiecesWithMarker=%d nearestHiddenEnemyToUnidentifiedMission=%.0f openingExposureRadius=%.0f "
		       "hasCloud=%d fogTopZ=%.1f"),
	       Counts.Missions, Counts.MissionsAsMarker, Counts.MissionsAsRealPieceThroughFog,
	       Counts.Missions - Counts.MissionsAsMarker - Counts.MissionsAsRealPieceThroughFog,
	       Counts.NearestMarkerDistanceToHQ, Counts.EnemyObjects, Counts.EnemyObjectsUnderFog, Counts.Divisions,
	       Counts.EnemyPiecesWithMarker, M_NearestHiddenEnemyToUnidentifiedMission,
	       WorldFogHintRules::GetMissionOpeningExposureRadius(
		       UWorldFogHintSettings::Get()->MissionOpeningRadius, UWorldFogHintSettings::Get()->MissionOpeningFalloff,
		       GetMaskPixelWorldSize(), UWorldFogHintSettings::Get()->EnemyPieceHalfWidth),
	       bHasCloud ? 1 : 0, bHasCloud ? M_WorldFowCloud->GetFogTopZ() : 0.f);
#endif
}

void AWorldFowManager::ApplyPersistentRevealsToAnchors()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint) || GetAnchorState(AnchorPoint) == EWorldMapFowState::Visible)
		{
			continue;
		}

		if (GetIsAnchorCleared(AnchorPoint) || GetIsAnchorInsideRevealSource(AnchorPoint))
		{
			SetAnchorState(AnchorPoint, EWorldMapFowState::Visible);
		}
	}
}

bool AWorldFowManager::GetIsAnchorCleared(const AAnchorPoint* AnchorPoint) const
{
	if (not bM_HasCampaignProgress || not IsValid(AnchorPoint) || not GetIsValidWorldCampaignProgressManager())
	{
		return false;
	}

	return M_WorldCampaignProgressManager->GetIsAnchorCleared(AnchorPoint->GetAnchorKey());
}

bool AWorldFowManager::GetIsAnchorInsideRevealSource(const AAnchorPoint* AnchorPoint) const
{
	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	const FVector AnchorLocation = AnchorPoint->GetActorLocation();
	for (const TPair<FName, FWorldFowRevealSource>& RevealSourceById : M_RevealSourcesById)
	{
		const FWorldFowRevealSource& RevealSource = RevealSourceById.Value;
		if (RevealSource.RevealRadius <= 0.f)
		{
			continue;
		}

		if (FVector::DistSquared2D(AnchorLocation, RevealSource.WorldLocation) <= FMath::Square(RevealSource.RevealRadius))
		{
			return true;
		}
	}
	return false;
}

void AWorldFowManager::StampRevealSourcesToMask()
{
	for (const TPair<FName, FWorldFowRevealSource>& RevealSourceById : M_RevealSourcesById)
	{
		const FWorldFowRevealSource& RevealSource = RevealSourceById.Value;
		if (RevealSource.RevealRadius <= 0.f)
		{
			continue;
		}

		StampCircle(
			RevealSource.WorldLocation,
			RevealSource.RevealRadius,
			RevealSource.RevealFalloff,
			WorldFowMaskConstants::VisibleChannelIndex
		);
	}
}

bool AWorldFowManager::GetIsValidWorldCampaignProgressManager() const
{
	if (M_WorldCampaignProgressManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldCampaignProgressManager"),
		TEXT("GetIsValidWorldCampaignProgressManager"),
		this
	);
	return false;
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
		NotifyFowRefreshed();
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
		NotifyFowRefreshed();
	}
}

void AWorldFowManager::Debug_AllExplorableConnections()
{
	Debug_DrawRadiusForConnections(EWorldMapFowState::Explorable);
}

void AWorldFowManager::Debug_AllVisibleConnections()
{
	Debug_DrawRadiusForConnections(EWorldMapFowState::Visible);
}

void AWorldFowManager::Debug_AllExplorableAnchors()
{
	Debug_DrawRadiusForAnchors(EWorldMapFowState::Explorable);
}

void AWorldFowManager::Debug_AllExplorableEnemyObjects()
{
	Debug_DrawRadiusForWorldObjects(EWorldMapFowState::Explorable, AWorldEnemyObject::StaticClass());
}

void AWorldFowManager::Debug_AllVisibleEnemyObjects()
{
	Debug_DrawRadiusForWorldObjects(EWorldMapFowState::Visible, AWorldEnemyObject::StaticClass());
}

void AWorldFowManager::Debug_AllExplorableMissionObjects()
{
	Debug_DrawRadiusForWorldObjects(EWorldMapFowState::Explorable, AWorldMissionObject::StaticClass());
}

void AWorldFowManager::Debug_AllVisibleMissionObjects()
{
	Debug_DrawRadiusForWorldObjects(EWorldMapFowState::Visible, AWorldMissionObject::StaticClass());
}

void AWorldFowManager::Debug_AllConnectionsState()
{
	Debug_DrawStateForConnections();
}

void AWorldFowManager::Debug_AllAnchorsState()
{
	Debug_DrawStateForAnchors();
}

void AWorldFowManager::Debug_AllEnemyObjectsState()
{
	Debug_DrawStateForWorldObjects(AWorldEnemyObject::StaticClass());
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

void AWorldFowManager::Debug_DrawRadiusForConnections(const EWorldMapFowState DebugState) const
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AConnection>& Connection : PlacementState.CachedConnections)
	{
		if (not IsValid(Connection) || GetConnectionStateFromConnectedAnchors(Connection) != DebugState)
		{
			continue;
		}

		const UWorldMapFowComponent* FowComponent = Connection->GetFowComponent();
		if (not IsValid(FowComponent))
		{
			continue;
		}

		const FString DebugText = FString::Printf(
			TEXT("%s radius: %.0f"),
			*Debug_GetTextForState(DebugState),
			FowComponent->GetConnectionCorridorWidthForState(DebugState)
		);
		Debug_DrawWorldString(Debug_GetConnectionTextLocation(Connection), DebugText, Debug_GetColorForState(DebugState));
	}
}

void AWorldFowManager::Debug_DrawRadiusForAnchors(const EWorldMapFowState DebugState) const
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint) || GetAnchorState(AnchorPoint) != DebugState)
		{
			continue;
		}

		const UWorldMapFowComponent* FowComponent = AnchorPoint->GetFowComponent();
		if (not IsValid(FowComponent))
		{
			continue;
		}

		const FString DebugText = FString::Printf(
			TEXT("%s radius: %.0f"),
			*Debug_GetTextForState(DebugState),
			FowComponent->GetRevealRadiusForState(DebugState)
		);
		Debug_DrawWorldString(AnchorPoint->GetActorLocation(), DebugText, Debug_GetColorForState(DebugState));
	}
}

void AWorldFowManager::Debug_DrawRadiusForWorldObjects(
	const EWorldMapFowState DebugState,
	UClass* WorldObjectClass) const
{
	if (not GetIsValidWorldGenerator() || not IsValid(WorldObjectClass))
	{
		return;
	}

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AWorldMapObject* WorldMapObject = AnchorPoint->GetPromotedWorldObject();
		if (not IsValid(WorldMapObject) || not WorldMapObject->IsA(WorldObjectClass))
		{
			continue;
		}

		const UWorldMapFowComponent* FowComponent = WorldMapObject->GetFowComponent();
		if (not IsValid(FowComponent) || FowComponent->GetCurrentFowState() != DebugState)
		{
			continue;
		}

		const FString DebugText = FString::Printf(
			TEXT("%s radius: %.0f"),
			*Debug_GetTextForState(DebugState),
			FowComponent->GetRevealRadiusForState(DebugState)
		);
		Debug_DrawWorldString(WorldMapObject->GetActorLocation(), DebugText, Debug_GetColorForState(DebugState));
	}
}

void AWorldFowManager::Debug_DrawStateForConnections() const
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AConnection>& Connection : PlacementState.CachedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		const EWorldMapFowState ConnectionState = GetConnectionStateFromConnectedAnchors(Connection);
		Debug_DrawWorldString(
			Debug_GetConnectionTextLocation(Connection),
			Debug_GetTextForState(ConnectionState),
			Debug_GetColorForState(ConnectionState)
		);
	}
}

void AWorldFowManager::Debug_DrawStateForAnchors() const
{
	if (not GetIsValidWorldGenerator())
	{
		return;
	}

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const EWorldMapFowState AnchorState = GetAnchorState(AnchorPoint);
		Debug_DrawWorldString(
			AnchorPoint->GetActorLocation(),
			Debug_GetTextForState(AnchorState),
			Debug_GetColorForState(AnchorState)
		);
	}
}

void AWorldFowManager::Debug_DrawStateForWorldObjects(UClass* WorldObjectClass) const
{
	if (not GetIsValidWorldGenerator() || not IsValid(WorldObjectClass))
	{
		return;
	}

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AWorldMapObject* WorldMapObject = AnchorPoint->GetPromotedWorldObject();
		if (not IsValid(WorldMapObject) || not WorldMapObject->IsA(WorldObjectClass))
		{
			continue;
		}

		const UWorldMapFowComponent* FowComponent = WorldMapObject->GetFowComponent();
		if (not IsValid(FowComponent))
		{
			continue;
		}

		const EWorldMapFowState ObjectState = FowComponent->GetCurrentFowState();
		Debug_DrawWorldString(
			WorldMapObject->GetActorLocation(),
			Debug_GetTextForState(ObjectState),
			Debug_GetColorForState(ObjectState)
		);
	}
}

void AWorldFowManager::Debug_DrawWorldString(
	const FVector& WorldLocation,
	const FString& DebugText,
	const FColor& Color) const
{
	DrawDebugString(
		GetWorld(),
		WorldLocation + FVector(0.f, 0.f, WorldFowDebugConstants::WorldStringZOffset),
		DebugText,
		nullptr,
		Color,
		WorldFowDebugConstants::WorldStringDurationSeconds,
		true
	);
}

FVector AWorldFowManager::Debug_GetConnectionTextLocation(const AConnection* Connection) const
{
	if (not IsValid(Connection))
	{
		return FVector::ZeroVector;
	}

	if (Connection->GetIsThreeWayConnection())
	{
		return Connection->GetJunctionLocation();
	}

	const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
	if (not ConnectedAnchors.IsValidIndex(0) || not ConnectedAnchors.IsValidIndex(1))
	{
		return Connection->GetActorLocation();
	}

	const AAnchorPoint* StartAnchor = ConnectedAnchors[0];
	const AAnchorPoint* EndAnchor = ConnectedAnchors[1];
	if (not IsValid(StartAnchor) || not IsValid(EndAnchor))
	{
		return Connection->GetActorLocation();
	}

	return (StartAnchor->GetActorLocation() + EndAnchor->GetActorLocation()) * 0.5f;
}

FColor AWorldFowManager::Debug_GetColorForState(const EWorldMapFowState State) const
{
	switch (State)
	{
	case EWorldMapFowState::Visible:
		return WorldFowDebugConstants::VisibleColor;
	case EWorldMapFowState::Explorable:
		return WorldFowDebugConstants::ExplorableColor;
	case EWorldMapFowState::POIVisible:
		return WorldFowDebugConstants::POIVisibleColor;
	case EWorldMapFowState::NotVisible:
	default:
		return WorldFowDebugConstants::HiddenColor;
	}
}

FString AWorldFowManager::Debug_GetTextForState(const EWorldMapFowState State) const
{
	switch (State)
	{
	case EWorldMapFowState::Visible:
		return TEXT("Visible");
	case EWorldMapFowState::Explorable:
		return TEXT("Explorable");
	case EWorldMapFowState::POIVisible:
		return TEXT("POI");
	case EWorldMapFowState::NotVisible:
	default:
		return TEXT("Hidden");
	}
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
	RemoveStaleAnchorStates();

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		if (M_AnchorStates.Contains(TObjectKey<const AAnchorPoint>(AnchorPoint)))
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

void AWorldFowManager::RemoveStaleAnchorStates()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	TSet<TObjectKey<const AAnchorPoint>> CurrentAnchorKeys;
	CurrentAnchorKeys.Reserve(PlacementState.CachedAnchors.Num());
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		CurrentAnchorKeys.Add(TObjectKey<const AAnchorPoint>(AnchorPoint.Get()));
	}

	for (auto AnchorStateIterator = M_AnchorStates.CreateIterator(); AnchorStateIterator; ++AnchorStateIterator)
	{
		if (not CurrentAnchorKeys.Contains(AnchorStateIterator.Key()))
		{
			AnchorStateIterator.RemoveCurrent();
		}
	}
}

void AWorldFowManager::ApplyRevealRulesFromVisibleAnchors()
{
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	AAnchorPoint* PlayerHQAnchor = PlacementState.PlayerHQAnchor;
	if (not IsValid(PlayerHQAnchor))
	{
		return;
	}

	SetAnchorState(PlayerHQAnchor, EWorldMapFowState::Visible);

	// Flood visibility outward from every anchor the player can already see, seeding the frontier with
	// the current Visible set (HQ plus any anchors captured in a previous update).
	TQueue<AAnchorPoint*> VisibleFrontier;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (IsValid(AnchorPoint) && GetAnchorState(AnchorPoint) == EWorldMapFowState::Visible)
		{
			VisibleFrontier.Enqueue(AnchorPoint);
		}
	}

	// A neutral anchor is never captured to reveal it, so any neutral anchor the player can reach is
	// promoted straight to Visible instead of Explorable. Feeding that promoted anchor back into the
	// frontier lets visibility cascade through chains of adjacent neutral anchors, while every visible
	// anchor still surfaces its remaining hidden, non-neutral neighbors as Explorable.
	AAnchorPoint* VisibleAnchor = nullptr;
	while (VisibleFrontier.Dequeue(VisibleAnchor))
	{
		for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : VisibleAnchor->GetNeighborAnchors())
		{
			if (not IsValid(NeighborAnchor))
			{
				continue;
			}

			if (GetAnchorHoldsNeutralObject(NeighborAnchor))
			{
				if (GetAnchorState(NeighborAnchor) != EWorldMapFowState::Visible)
				{
					SetAnchorState(NeighborAnchor, EWorldMapFowState::Visible);
					VisibleFrontier.Enqueue(NeighborAnchor);
				}
			}
			else if (GetAnchorState(NeighborAnchor) == EWorldMapFowState::NotVisible)
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
	StampRevealSourcesToMask();
	CacheRevealedPixels();
	ApplyMissionFogHints();
}

void AWorldFowManager::CacheRevealedPixels()
{
	M_RevealedPixels.SetNumUninitialized(M_MaskPixels.Num());
	for (int32 PixelIndex = 0; PixelIndex < M_MaskPixels.Num(); ++PixelIndex)
	{
		M_RevealedPixels[PixelIndex] = M_MaskPixels[PixelIndex].R;
	}
}

void AWorldFowManager::ApplyMissionFogHints()
{
	const UWorldFogHintSettings* const Settings = UWorldFogHintSettings::Get();
	TArray<FVector> HiddenEnemyPieceLocations;
	GatherHiddenEnemyPieceLocations(HiddenEnemyPieceLocations);
	const float ExposureRadius = WorldFogHintRules::GetMissionOpeningExposureRadius(
		Settings->MissionOpeningRadius, Settings->MissionOpeningFalloff, GetMaskPixelWorldSize(),
		Settings->EnemyPieceHalfWidth);
	// A mask of the right size only exists with a cloud; without it nothing is cut.
	const bool bHasMask = M_WorldFowCloud.IsValid() && M_MaskPixels.Num() == M_MaskResolution * M_MaskResolution;
	M_NearestHiddenEnemyToUnidentifiedMission = -1.f;

	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		AWorldMissionObject* const MissionObject =
			IsValid(AnchorPoint) ? Cast<AWorldMissionObject>(AnchorPoint->GetPromotedWorldObject()) : nullptr;
		const UWorldMapFowComponent* const FowComponent = IsValid(MissionObject) ? MissionObject->GetFowComponent() : nullptr;
		if (not IsValid(FowComponent))
		{
			continue;
		}

		const FVector MissionLocation = MissionObject->GetActorLocation();
		const EWorldMissionFogPresentation Presentation = WorldFogHintRules::GetMissionFogPresentation(
			EMapItemType::Mission, FowComponent->GetCurrentFowState(), GetIsWorldLocationRevealed(MissionLocation),
			MissionObject->GetIsClearedByPlayer(), Settings->bShowUnknownMissionMarkers, Settings->UnknownMissionLook,
			WorldFogHintRules::GetIsAnyPieceInsideRadius(MissionLocation, HiddenEnemyPieceLocations, ExposureRadius));
		if (Presentation == EWorldMissionFogPresentation::RealPieceThroughFog && bHasMask)
		{
			StampCircle(MissionLocation, Settings->MissionOpeningRadius, Settings->MissionOpeningFalloff,
			            WorldFowMaskConstants::VisibleChannelIndex);
		}
		MissionObject->ApplyFogPresentation(Presentation);
		if (Presentation != EWorldMissionFogPresentation::AsFogState)
		{
			NoteHiddenEnemyDistance(MissionLocation, HiddenEnemyPieceLocations);
		}
	}
}

void AWorldFowManager::NoteHiddenEnemyDistance(const FVector& MissionLocation,
                                               const TArray<FVector>& HiddenEnemyPieceLocations)
{
	for (const FVector& EnemyPieceLocation : HiddenEnemyPieceLocations)
	{
		const float Distance = FVector::Dist2D(MissionLocation, EnemyPieceLocation);
		const bool bIsNearest = M_NearestHiddenEnemyToUnidentifiedMission < 0.f
			|| Distance < M_NearestHiddenEnemyToUnidentifiedMission;
		M_NearestHiddenEnemyToUnidentifiedMission = bIsNearest ? Distance : M_NearestHiddenEnemyToUnidentifiedMission;
	}
}

void AWorldFowManager::GatherHiddenEnemyPieceLocations(TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();
	const FWorldCampaignPlacementState& PlacementState = M_WorldGenerator->GetPlacementState();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		const AWorldEnemyObject* const EnemyObject =
			IsValid(AnchorPoint) ? Cast<AWorldEnemyObject>(AnchorPoint->GetPromotedWorldObject()) : nullptr;
		// A destroyed enemy object is gone from the map; one the player already sees has nothing left to give away.
		const bool bIsHidden = IsValid(EnemyObject) && not EnemyObject->GetIsClearedByPlayer()
			&& GetAnchorState(AnchorPoint) != EWorldMapFowState::Visible;
		if (bIsHidden)
		{
			OutLocations.Add(EnemyObject->GetActorLocation());
		}
	}

	// Divisions are enemy pieces only and take no part in the fog states: the mask says whether one is seen.
	for (TActorIterator<AWorldDivisionBase> DivisionIterator(GetWorld()); DivisionIterator; ++DivisionIterator)
	{
		const FVector DivisionLocation = DivisionIterator->GetActorLocation();
		if (not GetIsWorldLocationRevealed(DivisionLocation))
		{
			OutLocations.Add(DivisionLocation);
		}
	}
}

float AWorldFowManager::GetMaskPixelWorldSize() const
{
	if (not M_WorldFowCloud.IsValid())
	{
		return 0.f;
	}

	const FVector2D MapSize = M_WorldFowCloud->GetMapSizeXY();
	return FMath::Max(MapSize.X, MapSize.Y) / FMath::Max(1, M_MaskResolution);
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

	M_AnchorStates.FindOrAdd(TObjectKey<const AAnchorPoint>(AnchorPoint)) = NewState;
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

bool AWorldFowManager::GetAnchorHoldsNeutralObject(const AAnchorPoint* AnchorPoint) const
{
	return IsValid(AnchorPoint) && Cast<AWorldNeutralObject>(AnchorPoint->GetPromotedWorldObject()) != nullptr;
}

EWorldMapFowState AWorldFowManager::GetAnchorState(const AAnchorPoint* AnchorPoint) const
{
	const EWorldMapFowState* FoundState = M_AnchorStates.Find(TObjectKey<const AAnchorPoint>(AnchorPoint));
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

bool AWorldFowManager::GetIsWorldLocationRevealed(const FVector& WorldLocation) const
{
	// Half of full visibility (see GetIsMaskPixelSet): inside the soft edge of a reveal circle counts as seen.
	// The cached copy, not the mask: the mask also carries the drawn-only openings around unidentified missions.
	return GetIsMaskPixelSet(M_RevealedPixels, WorldLocation);
}

bool AWorldFowManager::GetIsFogDrawnOpenAt(const FVector& WorldLocation) const
{
	// Quiet checks on purpose: a map without a cloud hides nothing.
	if (not M_WorldFowCloud.IsValid() || M_MaskPixels.Num() != M_MaskResolution * M_MaskResolution)
	{
		return true;
	}

	const FIntPoint Pixel = WorldToPixel(WorldLocation);
	const int32 PixelIndex = Pixel.Y * M_MaskResolution + Pixel.X;
	// Same threshold as the reveal answer: inside the soft edge counts as open.
	constexpr uint8 DrawnOpenThreshold = 128;
	return not M_MaskPixels.IsValidIndex(PixelIndex) || M_MaskPixels[PixelIndex].R >= DrawnOpenThreshold;
}

bool AWorldFowManager::GetIsMaskPixelSet(const TArray<uint8>& VisiblePixels, const FVector& WorldLocation) const
{
	constexpr uint8 RevealedThreshold = 128;
	// Quiet checks on purpose: a map without a cloud hides nothing.
	if (not M_WorldFowCloud.IsValid() || VisiblePixels.Num() != M_MaskResolution * M_MaskResolution)
	{
		return true;
	}

	const FIntPoint Pixel = WorldToPixel(WorldLocation);
	const int32 PixelIndex = Pixel.Y * M_MaskResolution + Pixel.X;
	return not VisiblePixels.IsValidIndex(PixelIndex) || VisiblePixels[PixelIndex] >= RevealedThreshold;
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
