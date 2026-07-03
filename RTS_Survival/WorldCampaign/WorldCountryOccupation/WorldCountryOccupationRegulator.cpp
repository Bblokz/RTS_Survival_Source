// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldPlayerObject/WorldPlayerObject.h"

namespace
{
	constexpr int32 CountryOccupationMaterialIndex = 0;
	constexpr float DefaultCountryBoundsPaddingXY = 0.f;

	EMapItemType NormalizeMapItemType(const EMapItemType MapItemType)
	{
		return MapItemType == EMapItemType::None
			       ? EMapItemType::Empty
			       : MapItemType;
	}

	bool GetIsSovietOccupationItemType(const EMapItemType MapItemType)
	{
		return MapItemType == EMapItemType::EnemyItem || MapItemType == EMapItemType::Mission;
	}

	struct FCountryOccupationEvaluation
	{
		bool bAllowsSovietOccupation = true;
		bool bAllowsResistanceOccupation = true;
		bool bAllowsNeutralOccupation = true;
		bool bHasSovietOccupationItem = false;
		bool bHasResistanceOccupationItem = false;
	};

	void UpdateCountryOccupationEvaluation(
		const EMapItemType MapItemType,
		FCountryOccupationEvaluation& InOutEvaluation)
	{
		/*
		 * Empty anchors do not claim a side, so they leave every family possible.
		 * Enemy/mission, player, and neutral anchors each remove the families they cannot coexist with.
		 */
		if (GetIsSovietOccupationItemType(MapItemType))
		{
			InOutEvaluation.bHasSovietOccupationItem = true;
			InOutEvaluation.bAllowsResistanceOccupation = false;
			InOutEvaluation.bAllowsNeutralOccupation = false;
			return;
		}

		if (MapItemType == EMapItemType::PlayerItem)
		{
			InOutEvaluation.bHasResistanceOccupationItem = true;
			InOutEvaluation.bAllowsSovietOccupation = false;
			InOutEvaluation.bAllowsNeutralOccupation = false;
			return;
		}

		if (MapItemType == EMapItemType::NeutralItem)
		{
			InOutEvaluation.bAllowsSovietOccupation = false;
			InOutEvaluation.bAllowsResistanceOccupation = false;
		}
	}
}

UWorldCountryOccupationRegulator::UWorldCountryOccupationRegulator()
{
	PrimaryComponentTick.bCanEverTick = false;
	M_CountryBoundsPaddingXY = DefaultCountryBoundsPaddingXY;
}

void UWorldCountryOccupationRegulator::InitializeCountryOccupation(AGeneratorWorldCampaign* WorldGenerator)
{
	if (not IsValid(WorldGenerator))
	{
		RTSFunctionLibrary::ReportError(TEXT("Country occupation initialization failed: world generator is invalid."));
		return;
	}

	/*
	 * Initialization is intentionally ordered: first clear old generated state, then validate designer data,
	 * then build all lookups before applying any material. Later update calls depend on all three caches.
	 */
	ResetRuntimeMappings();
	if (not GetIsValidSovietOccupiedMaterial() || not GetIsValidResistanceMaterial() || not GetIsValidNeutralMaterial())
	{
		return;
	}

	const FWorldCampaignPlacementState& PlacementState = WorldGenerator->GetPlacementState();
	InitializeRuntimeCountryEntries();
	InitializeAnchorMappings(PlacementState);
	InitializeItemTypeCache(PlacementState);
	ApplyInitialCountryMaterials();
}

void UWorldCountryOccupationRegulator::UpdateCountryOccupationForAnchor(AAnchorPoint* AnchorPoint,
                                                                        const EMapItemType MapItemType)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
	if (not AnchorKey.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("Country occupation update failed: anchor key is invalid."));
		return;
	}

	M_CurrentItemTypeByAnchorKey.Add(AnchorKey, NormalizeMapItemType(MapItemType));

	/*
	 * Most anchors are mapped during initialization. The fallback keeps the public function useful for anchors
	 * that become relevant later without forcing a full country remap.
	 */
	int32 CountryIndex = INDEX_NONE;
	const int32* CachedCountryIndex = M_CountryIndexByAnchorKey.Find(AnchorKey);
	if (CachedCountryIndex != nullptr)
	{
		CountryIndex = *CachedCountryIndex;
	}
	else if (TryFindCountryIndexForAnchor(AnchorPoint, CountryIndex))
	{
		CacheAnchorCountryMapping(AnchorPoint, CountryIndex);
	}

	if (not GetIsCountryIndexValid(CountryIndex))
	{
		return;
	}

	ReevaluateCountryOccupation(CountryIndex);
}

void UWorldCountryOccupationRegulator::ResetRuntimeMappings()
{
	M_CountryIndexByAnchorKey.Reset();
	M_AnchorsByCountryMeshActor.Reset();
	M_CurrentItemTypeByAnchorKey.Reset();
	M_TraceableCountryIndices.Reset();
}

void UWorldCountryOccupationRegulator::InitializeRuntimeCountryEntries()
{
	if (M_CountryMeshActorPairs.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("WorldCountryOccupationRegulator has no country mesh actor pairs configured."));
		return;
	}

	M_TraceableCountryIndices.Reserve(M_CountryMeshActorPairs.Num());
	for (int32 CountryIndex = 0; CountryIndex < M_CountryMeshActorPairs.Num(); CountryIndex++)
	{
		/*
		 * These actors are placed in the level and referenced from the generator component.
		 * Invalid pairs are skipped once here so anchor mapping can stay focused on valid countries.
		 */
		const FWorldCountryOccupationActorPair& ActorPair = M_CountryMeshActorPairs[CountryIndex];
		if (not GetIsValidCountryMeshActor(ActorPair, CountryIndex,
		                                   TEXT("UWorldCountryOccupationRegulator::InitializeRuntimeCountryEntries")))
		{
			continue;
		}

		AStaticMeshActor* CountryMeshActor = ActorPair.M_CountryMeshActor.Get();
		UStaticMeshComponent* CountryMesh = GetCountryMeshForIndex(CountryIndex);
		if (not GetIsValidCountryMeshComponent(
			CountryMesh,
			CountryIndex,
			TEXT("UWorldCountryOccupationRegulator::InitializeRuntimeCountryEntries")))
		{
			continue;
		}

		M_TraceableCountryIndices.Add(CountryIndex);
		M_AnchorsByCountryMeshActor.FindOrAdd(TObjectKey<AStaticMeshActor>(CountryMeshActor));
	}
}

void UWorldCountryOccupationRegulator::InitializeAnchorMappings(const FWorldCampaignPlacementState& PlacementState)
{
	int32 MappedAnchorCount = 0;
	int32 UnmappedAnchorCount = 0;

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		/*
		 * Ownership is cached both directions because later systems usually start from an anchor,
		 * while material refresh needs all anchors belonging to one country.
		 */
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		int32 CountryIndex = INDEX_NONE;
		if (not TryFindCountryIndexForAnchor(AnchorPoint, CountryIndex))
		{
			UnmappedAnchorCount++;
			continue;
		}

		CacheAnchorCountryMapping(AnchorPoint, CountryIndex);
		MappedAnchorCount++;
	}

	if (UnmappedAnchorCount > 0)
	{
		UE_LOG(LogRTS, Warning,
		       TEXT(
			       "WorldCountryOccupationRegulator mapped %d anchors and skipped %d anchors outside configured countries."
		       ),
		       MappedAnchorCount,
		       UnmappedAnchorCount);
	}
}

void UWorldCountryOccupationRegulator::InitializeItemTypeCache(const FWorldCampaignPlacementState& PlacementState)
{
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
		if (not AnchorKey.IsValid())
		{
			continue;
		}

		M_CurrentItemTypeByAnchorKey.Add(AnchorKey, GetInitialMapItemTypeForAnchor(AnchorPoint, PlacementState));
	}
}

void UWorldCountryOccupationRegulator::ApplyInitialCountryMaterials()
{
	for (const int32 CountryIndex : M_TraceableCountryIndices)
	{
		const TArray<TWeakObjectPtr<AAnchorPoint>>* CountryAnchors = FindCountryAnchorsByIndex(CountryIndex);
		if (CountryAnchors == nullptr)
		{
			continue;
		}

		const EWorldCountryOccupationState OccupationState =
			DetermineInitialCountryOccupationState(*CountryAnchors);
		ApplyCountryOccupationState(CountryIndex, OccupationState);
	}
}

void UWorldCountryOccupationRegulator::ReevaluateCountryOccupation(const int32 CountryIndex)
{
	const TArray<TWeakObjectPtr<AAnchorPoint>>* CountryAnchors = FindCountryAnchorsByIndex(CountryIndex);
	if (CountryAnchors == nullptr)
	{
		return;
	}

	const EWorldCountryOccupationState OccupationState = DetermineCountryOccupationState(*CountryAnchors);
	ApplyCountryOccupationState(CountryIndex, OccupationState);
}

void UWorldCountryOccupationRegulator::CacheAnchorCountryMapping(AAnchorPoint* AnchorPoint, const int32 CountryIndex)
{
	if (not IsValid(AnchorPoint) || not GetIsCountryIndexValid(CountryIndex))
	{
		return;
	}

	const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
	if (not AnchorKey.IsValid())
	{
		return;
	}

	AStaticMeshActor* CountryMeshActor = GetCountryMeshActorForIndex(CountryIndex);
	if (not IsValid(CountryMeshActor))
	{
		return;
	}

	M_CountryIndexByAnchorKey.Add(AnchorKey, CountryIndex);
	TArray<TWeakObjectPtr<AAnchorPoint>>& CountryAnchors =
		M_AnchorsByCountryMeshActor.FindOrAdd(TObjectKey<AStaticMeshActor>(CountryMeshActor));
	/*
	 * Update calls can discover an anchor after initialization, so this stays idempotent and avoids duplicated
	 * weak references before material evaluation counts the country's anchors.
	 */
	if (CountryAnchors.ContainsByPredicate([AnchorPoint](const TWeakObjectPtr<AAnchorPoint>& ExistingAnchor)
	{
		return ExistingAnchor.Get() == AnchorPoint;
	}))
	{
		return;
	}

	CountryAnchors.Add(AnchorPoint);
}

void UWorldCountryOccupationRegulator::ApplyCountryOccupationState(
	const int32 CountryIndex,
	const EWorldCountryOccupationState OccupationState)
{
	UMaterialInterface* OccupationMaterial = GetMaterialForOccupationState(OccupationState);
	if (not IsValid(OccupationMaterial))
	{
		return;
	}

	UStaticMeshComponent* CountryMesh = GetCountryMeshForIndex(CountryIndex);
	if (not IsValid(CountryMesh))
	{
		return;
	}

	CountryMesh->SetMaterial(CountryOccupationMaterialIndex, OccupationMaterial);
}

UWorldCountryOccupationRegulator::EWorldCountryOccupationState
UWorldCountryOccupationRegulator::DetermineInitialCountryOccupationState(
	const TArray<TWeakObjectPtr<AAnchorPoint>>& CountryAnchors) const
{
	for (const TWeakObjectPtr<AAnchorPoint>& AnchorPoint : CountryAnchors)
	{
		if (not AnchorPoint.IsValid())
		{
			continue;
		}

		const EMapItemType MapItemType = GetCachedMapItemTypeForAnchor(AnchorPoint.Get());
		if (GetIsSovietOccupationItemType(MapItemType))
		{
			return EWorldCountryOccupationState::SovietOccupied;
		}
	}

	return EWorldCountryOccupationState::Neutral;
}

UWorldCountryOccupationRegulator::EWorldCountryOccupationState
UWorldCountryOccupationRegulator::DetermineCountryOccupationState(
	const TArray<TWeakObjectPtr<AAnchorPoint>>& CountryAnchors) const
{
	/*
	 * The later update rules are stricter than initial generation. A country changes material only when all
	 * valid anchors fit exactly one requested family: Soviet-side, Resistance-side, or Neutral/empty.
	 */
	FCountryOccupationEvaluation Evaluation;

	for (const TWeakObjectPtr<AAnchorPoint>& AnchorPoint : CountryAnchors)
	{
		if (not AnchorPoint.IsValid())
		{
			continue;
		}

		UpdateCountryOccupationEvaluation(
			GetCachedMapItemTypeForAnchor(AnchorPoint.Get()),
			Evaluation);
	}

	if (Evaluation.bAllowsSovietOccupation && Evaluation.bHasSovietOccupationItem)
	{
		return EWorldCountryOccupationState::SovietOccupied;
	}

	if (Evaluation.bAllowsResistanceOccupation && Evaluation.bHasResistanceOccupationItem)
	{
		return EWorldCountryOccupationState::ResistanceOccupied;
	}

	return Evaluation.bAllowsNeutralOccupation
		       ? EWorldCountryOccupationState::Neutral
		       : EWorldCountryOccupationState::Mixed;
}

EMapItemType UWorldCountryOccupationRegulator::GetInitialMapItemTypeForAnchor(
	const AAnchorPoint* AnchorPoint,
	const FWorldCampaignPlacementState& PlacementState) const
{
	if (not IsValid(AnchorPoint))
	{
		return EMapItemType::Empty;
	}

	/*
	 * Prefer the promoted object when it exists so new campaigns and restored saves classify anchors the same way.
	 * The placement maps remain as fallback for any setup phase where promoted actors are not available yet.
	 */
	const AWorldMapObject* PromotedWorldObject = AnchorPoint->GetPromotedWorldObject();
	if (IsValid(PromotedWorldObject))
	{
		if (Cast<AWorldEnemyObject>(PromotedWorldObject) != nullptr)
		{
			return EMapItemType::EnemyItem;
		}

		if (Cast<AWorldMissionObject>(PromotedWorldObject) != nullptr)
		{
			return EMapItemType::Mission;
		}

		if (Cast<AWorldNeutralObject>(PromotedWorldObject) != nullptr)
		{
			return EMapItemType::NeutralItem;
		}

		if (Cast<AWorldPlayerObject>(PromotedWorldObject) != nullptr)
		{
			return EMapItemType::PlayerItem;
		}
	}

	const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
	if (PlacementState.PlayerHQAnchorKey == AnchorKey)
	{
		return EMapItemType::PlayerItem;
	}

	if (PlacementState.EnemyHQAnchorKey == AnchorKey || PlacementState.EnemyItemsByAnchorKey.Contains(AnchorKey))
	{
		return EMapItemType::EnemyItem;
	}

	if (PlacementState.MissionsByAnchorKey.Contains(AnchorKey))
	{
		return EMapItemType::Mission;
	}

	if (PlacementState.NeutralItemsByAnchorKey.Contains(AnchorKey))
	{
		return EMapItemType::NeutralItem;
	}

	return EMapItemType::Empty;
}

EMapItemType UWorldCountryOccupationRegulator::GetCachedMapItemTypeForAnchor(const AAnchorPoint* AnchorPoint) const
{
	if (not IsValid(AnchorPoint))
	{
		return EMapItemType::Empty;
	}

	const EMapItemType* CachedMapItemType = M_CurrentItemTypeByAnchorKey.Find(AnchorPoint->GetAnchorKey());
	return CachedMapItemType != nullptr
		       ? *CachedMapItemType
		       : EMapItemType::Empty;
}

UMaterialInterface* UWorldCountryOccupationRegulator::GetMaterialForOccupationState(
	const EWorldCountryOccupationState OccupationState) const
{
	switch (OccupationState)
	{
	case EWorldCountryOccupationState::SovietOccupied:
		return GetIsValidSovietOccupiedMaterial() ? M_SovietOccupiedMaterial.Get() : nullptr;
	case EWorldCountryOccupationState::ResistanceOccupied:
		return GetIsValidResistanceMaterial() ? M_ResistanceMaterial.Get() : nullptr;
	case EWorldCountryOccupationState::Neutral:
		return GetIsValidNeutralMaterial() ? M_NeutralMaterial.Get() : nullptr;
	case EWorldCountryOccupationState::Mixed:
	default:
		return nullptr;
	}
}

AStaticMeshActor* UWorldCountryOccupationRegulator::GetCountryMeshActorForIndex(const int32 CountryIndex) const
{
	if (not M_CountryMeshActorPairs.IsValidIndex(CountryIndex))
	{
		return nullptr;
	}

	return M_CountryMeshActorPairs[CountryIndex].M_CountryMeshActor.Get();
}

UStaticMeshComponent* UWorldCountryOccupationRegulator::GetCountryMeshForIndex(const int32 CountryIndex) const
{
	AStaticMeshActor* CountryMeshActor = GetCountryMeshActorForIndex(CountryIndex);
	if (not IsValid(CountryMeshActor))
	{
		return nullptr;
	}

	return CountryMeshActor->GetStaticMeshComponent();
}

const TArray<TWeakObjectPtr<AAnchorPoint>>* UWorldCountryOccupationRegulator::FindCountryAnchorsByIndex(
	const int32 CountryIndex) const
{
	AStaticMeshActor* CountryMeshActor = GetCountryMeshActorForIndex(CountryIndex);
	if (not IsValid(CountryMeshActor))
	{
		return nullptr;
	}

	return M_AnchorsByCountryMeshActor.Find(TObjectKey<AStaticMeshActor>(CountryMeshActor));
}

bool UWorldCountryOccupationRegulator::TryFindCountryIndexForAnchor(const AAnchorPoint* AnchorPoint,
                                                                    int32& OutCountryIndex) const
{
	OutCountryIndex = INDEX_NONE;
	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	/*
	 * Bounds are a fast non-collision broad shape. When several country bounds overlap, choose the smallest
	 * XY bounds area so a tighter local country beats a large neighboring rectangle.
	 */
	for (const int32 CountryIndex : M_TraceableCountryIndices)
	{
		UStaticMeshComponent* CountryMesh = GetCountryMeshForIndex(CountryIndex);
		if (not IsValid(CountryMesh))
		{
			continue;
		}

		if (not GetDoesCountryMeshBoundsContainAnchor(CountryMesh, AnchorPoint))
		{
			continue;
		}

		if (not GetIsCountryIndexValid(OutCountryIndex))
		{
			OutCountryIndex = CountryIndex;
			continue;
		}

		const float CandidateBoundsArea = GetCountryMeshBoundsArea2D(CountryMesh);
		const UStaticMeshComponent* CurrentCountryMesh = GetCountryMeshForIndex(OutCountryIndex);
		if (CandidateBoundsArea < GetCountryMeshBoundsArea2D(CurrentCountryMesh))
		{
			OutCountryIndex = CountryIndex;
		}
	}

	return GetIsCountryIndexValid(OutCountryIndex);
}

bool UWorldCountryOccupationRegulator::GetDoesCountryMeshBoundsContainAnchor(
	const UStaticMeshComponent* CountryMesh,
	const AAnchorPoint* AnchorPoint) const
{
	if (not IsValid(CountryMesh) || not IsValid(AnchorPoint))
	{
		return false;
	}

	const FVector AnchorLocation = AnchorPoint->GetActorLocation();
	/*
	 * Countries are flat map regions for this feature. Z is ignored so country mesh thickness or world-map
	 * elevation never prevents an anchor from being assigned to its visible country.
	 */
	const FBox CountryBounds = CountryMesh->Bounds.GetBox().ExpandBy(
		FVector(M_CountryBoundsPaddingXY, M_CountryBoundsPaddingXY, DefaultCountryBoundsPaddingXY));
	return AnchorLocation.X >= CountryBounds.Min.X
		&& AnchorLocation.X <= CountryBounds.Max.X
		&& AnchorLocation.Y >= CountryBounds.Min.Y
		&& AnchorLocation.Y <= CountryBounds.Max.Y;
}

float UWorldCountryOccupationRegulator::GetCountryMeshBoundsArea2D(const UStaticMeshComponent* CountryMesh) const
{
	if (not IsValid(CountryMesh))
	{
		return TNumericLimits<float>::Max();
	}

	const FVector BoundsSize = CountryMesh->Bounds.GetBox().GetSize();
	return FMath::Max(BoundsSize.X, DefaultCountryBoundsPaddingXY)
		* FMath::Max(BoundsSize.Y, DefaultCountryBoundsPaddingXY);
}

bool UWorldCountryOccupationRegulator::GetIsCountryIndexValid(const int32 CountryIndex) const
{
	return M_CountryMeshActorPairs.IsValidIndex(CountryIndex);
}

bool UWorldCountryOccupationRegulator::GetIsValidCountryMeshActor(const FWorldCountryOccupationActorPair& ActorPair,
                                                                  const int32 CountryIndex,
                                                                  const FString& FunctionName) const
{
	if (IsValid(ActorPair.M_CountryMeshActor.Get()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		FString::Printf(TEXT("M_CountryMeshActorPairs[%d].M_CountryMeshActor"), CountryIndex),
		FunctionName,
		this);
	return false;
}

bool UWorldCountryOccupationRegulator::GetIsValidCountryMeshComponent(const UStaticMeshComponent* CountryMesh,
                                                                      const int32 CountryIndex,
                                                                      const FString& FunctionName) const
{
	if (IsValid(CountryMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		FString::Printf(TEXT("M_CountryMeshActorPairs[%d].M_CountryMeshActor.StaticMeshComponent"), CountryIndex),
		FunctionName,
		this);
	return false;
}

bool UWorldCountryOccupationRegulator::GetIsValidSovietOccupiedMaterial() const
{
	if (IsValid(M_SovietOccupiedMaterial))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_SovietOccupiedMaterial"),
		TEXT("UWorldCountryOccupationRegulator::GetIsValidSovietOccupiedMaterial"),
		this);
	return false;
}

bool UWorldCountryOccupationRegulator::GetIsValidResistanceMaterial() const
{
	if (IsValid(M_ResistanceMaterial))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ResistanceMaterial"),
		TEXT("UWorldCountryOccupationRegulator::GetIsValidResistanceMaterial"),
		this);
	return false;
}

bool UWorldCountryOccupationRegulator::GetIsValidNeutralMaterial() const
{
	if (IsValid(M_NeutralMaterial))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_NeutralMaterial"),
		TEXT("UWorldCountryOccupationRegulator::GetIsValidNeutralMaterial"),
		this);
	return false;
}
