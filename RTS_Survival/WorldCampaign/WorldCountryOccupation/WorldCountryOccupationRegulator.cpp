// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRules.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignMapContentSettings.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"
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
	// Trace starts this far above and ends this far below the country bounds so flat meshes are always crossed.
	constexpr float CountryShapeTraceHeightMargin = 1000.f;
	constexpr float CountryLiberationChangeTolerance = KINDA_SMALL_NUMBER;
	constexpr float FullCountryLiberation = 1.f;
	constexpr int32 CountryTriangleCornerCount = 3;

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
	 * Missing materials only disable the look; country membership and liberation are still built because build
	 * nodes and the campaign rules depend on them.
	 */
	ResetRuntimeMappings();
	M_WorldGenerator = WorldGenerator;
	bM_CanApplyCountryLook = GetIsValidSovietOccupiedMaterial()
		&& GetIsValidResistanceMaterial()
		&& GetIsValidNeutralMaterial();

	const FWorldCampaignPlacementState& PlacementState = WorldGenerator->GetPlacementState();
	InitializeRuntimeCountryEntries();
	InitializeAnchorMappings(PlacementState);
	InitializeItemTypeCache(PlacementState);
	InitializeCountryLiberationData();
	CacheLiberationLookParameters();
	RecomputeAllCountryLiberation();
}

void UWorldCountryOccupationRegulator::InitializeCampaignProgress(AWorldPlayerController* WorldPlayerController)
{
	if (not IsValid(WorldPlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("Country occupation campaign progress failed: controller is invalid."));
		return;
	}

	UnbindFromCampaignProgress();
	M_WorldCampaignProgressManager = WorldPlayerController->FindComponentByClass<UWorldCampaignProgressManager>();
	bM_IsCampaignProgressBound = true;
	if (not GetIsValidWorldCampaignProgressManager())
	{
		bM_IsCampaignProgressBound = false;
		return;
	}

	M_AnchorClearedHandle = M_WorldCampaignProgressManager->OnAnchorCleared.AddUObject(
		this, &UWorldCountryOccupationRegulator::OnAnchorCleared);

	// Liberation is derived from the saved cleared anchors, so a loaded campaign is correct after this pass.
	RecomputeAllCountryLiberation();
}

float UWorldCountryOccupationRegulator::GetCountryLiberation(const FName CountryId) const
{
	const FWorldCountryLiberationData* LiberationData = FindLiberationDataByCountryId(CountryId);
	if (LiberationData == nullptr)
	{
		return 0.f;
	}

	return FMath::Max(0.f, LiberationData->Liberation);
}

FName UWorldCountryOccupationRegulator::GetCountryIdForAnchorKey(const FGuid& AnchorKey) const
{
	const int32* CountryIndex = M_CountryIndexByAnchorKey.Find(AnchorKey);
	if (CountryIndex == nullptr)
	{
		return NAME_None;
	}

	const int32* LiberationDataIndex = M_LiberationDataIndexByCountryIndex.Find(*CountryIndex);
	if (LiberationDataIndex == nullptr || not M_CountryLiberationData.IsValidIndex(*LiberationDataIndex))
	{
		return NAME_None;
	}

	return M_CountryLiberationData[*LiberationDataIndex].CountryId;
}

TArray<FName> UWorldCountryOccupationRegulator::GetCountryIds() const
{
	TArray<FName> CountryIds;
	CountryIds.Reserve(M_CountryLiberationData.Num());
	for (const FWorldCountryLiberationData& LiberationData : M_CountryLiberationData)
	{
		CountryIds.Add(LiberationData.CountryId);
	}
	return CountryIds;
}

int32 UWorldCountryOccupationRegulator::GetClearedTargetCount(const FName CountryId) const
{
	const FWorldCountryLiberationData* LiberationData = FindLiberationDataByCountryId(CountryId);
	return LiberationData != nullptr ? LiberationData->TargetTally.GetClearedTargets() : 0;
}

int32 UWorldCountryOccupationRegulator::GetTargetCount(const FName CountryId) const
{
	const FWorldCountryLiberationData* LiberationData = FindLiberationDataByCountryId(CountryId);
	return LiberationData != nullptr ? LiberationData->TargetTally.GetTotalTargets() : 0;
}

void UWorldCountryOccupationRegulator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromCampaignProgress();
	Super::EndPlay(EndPlayReason);
}

void UWorldCountryOccupationRegulator::UnbindFromCampaignProgress()
{
	if (M_WorldCampaignProgressManager.IsValid() && M_AnchorClearedHandle.IsValid())
	{
		M_WorldCampaignProgressManager->OnAnchorCleared.Remove(M_AnchorClearedHandle);
	}

	M_AnchorClearedHandle.Reset();
	bM_IsCampaignProgressBound = false;
}

void UWorldCountryOccupationRegulator::OnAnchorCleared(const FGuid& AnchorKey)
{
	const int32* CountryIndex = M_CountryIndexByAnchorKey.Find(AnchorKey);
	if (CountryIndex == nullptr)
	{
		return;
	}

	const int32* LiberationDataIndex = M_LiberationDataIndexByCountryIndex.Find(*CountryIndex);
	if (LiberationDataIndex == nullptr)
	{
		return;
	}

	RecomputeCountryLiberation(*LiberationDataIndex);
}

void UWorldCountryOccupationRegulator::InitializeCountryLiberationData()
{
	M_CountryLiberationData.Reset();
	M_LiberationDataIndexByCountryId.Reset();
	M_LiberationDataIndexByCountryIndex.Reset();

	for (const int32 CountryIndex : M_TraceableCountryIndices)
	{
		const FName CountryId = ResolveCountryId(CountryIndex);
		if (CountryId.IsNone())
		{
			continue;
		}

		if (M_LiberationDataIndexByCountryId.Contains(CountryId))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Country occupation: country id %s is used by two country meshes; the second is ignored."),
				*CountryId.ToString()));
			continue;
		}

		FWorldCountryLiberationData LiberationData;
		LiberationData.CountryId = CountryId;
		LiberationData.CountryIndex = CountryIndex;
		const int32 LiberationDataIndex = M_CountryLiberationData.Add(LiberationData);
		M_LiberationDataIndexByCountryId.Add(CountryId, LiberationDataIndex);
		M_LiberationDataIndexByCountryIndex.Add(CountryIndex, LiberationDataIndex);
	}
}

FName UWorldCountryOccupationRegulator::ResolveCountryId(const int32 CountryIndex) const
{
	// Ids are fixed when the runtime list is built: config CountryId, else the mesh actor's FName.
	if (not M_RuntimeCountryIds.IsValidIndex(CountryIndex) || not IsValid(GetCountryMeshActorForIndex(CountryIndex)))
	{
		return NAME_None;
	}

	return M_RuntimeCountryIds[CountryIndex];
}

void UWorldCountryOccupationRegulator::RecomputeAllCountryLiberation()
{
	for (int32 LiberationDataIndex = 0; LiberationDataIndex < M_CountryLiberationData.Num(); LiberationDataIndex++)
	{
		RecomputeCountryLiberation(LiberationDataIndex);
	}
}

void UWorldCountryOccupationRegulator::RecomputeCountryLiberation(const int32 LiberationDataIndex)
{
	if (not M_CountryLiberationData.IsValidIndex(LiberationDataIndex))
	{
		return;
	}

	const int32 CountryIndex = M_CountryLiberationData[LiberationDataIndex].CountryIndex;
	const TArray<TWeakObjectPtr<AAnchorPoint>>* CountryAnchors = FindCountryAnchorsByIndex(CountryIndex);
	const UWorldCampaignProgressManager* ProgressManager =
		bM_IsCampaignProgressBound && GetIsValidWorldCampaignProgressManager()
			? M_WorldCampaignProgressManager.Get()
			: nullptr;

	TArray<FWorldCountryAnchorTargetInput> AnchorInputs;
	if (CountryAnchors != nullptr)
	{
		AnchorInputs.Reserve(CountryAnchors->Num());
		for (const TWeakObjectPtr<AAnchorPoint>& AnchorPoint : *CountryAnchors)
		{
			AnchorInputs.Add(BuildAnchorTargetInput(AnchorPoint.Get(), ProgressManager));
		}
	}

	FWorldCountryLiberationData& LiberationData = M_CountryLiberationData[LiberationDataIndex];
	LiberationData.TargetTally = WorldCountryLiberationRules::TallyCountryTargets(AnchorInputs);
	const FWorldCountryTargetTally& TargetTally = LiberationData.TargetTally;
	const float NewLiberation = WorldCampaignRules::GetCountryLiberation(
		UWorldCampaignRulesSettings::GetNumbers(),
		TargetTally.StoryMissionsTotal,
		TargetTally.StoryMissionsCleared,
		TargetTally.OtherTargetsTotal,
		TargetTally.OtherTargetsCleared);

	const bool bHasChanged = not FMath::IsNearlyEqual(NewLiberation, LiberationData.Liberation,
	                                                  CountryLiberationChangeTolerance);
	LiberationData.Liberation = NewLiberation;
	ApplyCountryLiberationLook(LiberationData);

	if (not bHasChanged)
	{
		return;
	}

	// Copy before broadcasting: listeners may re-enter and the array element must not be held across the call.
	const FName ChangedCountryId = LiberationData.CountryId;
	OnCountryLiberationChanged.Broadcast(ChangedCountryId, NewLiberation);
}

FWorldCountryAnchorTargetInput UWorldCountryOccupationRegulator::BuildAnchorTargetInput(
	const AAnchorPoint* AnchorPoint,
	const UWorldCampaignProgressManager* ProgressManager) const
{
	FWorldCountryAnchorTargetInput AnchorInput;
	if (not IsValid(AnchorPoint))
	{
		return AnchorInput;
	}

	const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
	const EMapItemType MapItemType = GetCachedMapItemTypeForAnchor(AnchorPoint);
	AnchorInput.bIsCleared = ProgressManager != nullptr && ProgressManager->GetIsAnchorCleared(AnchorKey);
	AnchorInput.bIsTarget = GetIsSovietOccupationItemType(MapItemType) || AnchorInput.bIsCleared;
	if (not AnchorInput.bIsTarget || not M_WorldGenerator.IsValid())
	{
		return AnchorInput;
	}

	AnchorInput.bIsStoryMission = M_WorldGenerator->GetIsStoryMissionAnchorKey(AnchorKey);
	return AnchorInput;
}

void UWorldCountryOccupationRegulator::CacheLiberationLookParameters()
{
	M_OccupiedLookParameters = FWorldCountryMaterialParameterSet();
	M_LiberatedLookParameters = FWorldCountryMaterialParameterSet();
	if (not bM_CanApplyCountryLook)
	{
		return;
	}

	ReadMaterialParameterSet(M_SovietOccupiedMaterial, M_OccupiedLookParameters);
	ReadMaterialParameterSet(M_ResistanceMaterial, M_LiberatedLookParameters);
}

void UWorldCountryOccupationRegulator::ReadMaterialParameterSet(
	const UMaterialInterface* Material,
	FWorldCountryMaterialParameterSet& OutParameterSet) const
{
	if (not IsValid(Material))
	{
		return;
	}

	for (const FName& VectorParameterName : M_LiberationVectorParameterNames)
	{
		FLinearColor VectorValue;
		if (Material->GetVectorParameterValue(FHashedMaterialParameterInfo(VectorParameterName), VectorValue))
		{
			OutParameterSet.VectorValues.Add(VectorParameterName, VectorValue);
		}
	}

	for (const FName& ScalarParameterName : M_LiberationScalarParameterNames)
	{
		float ScalarValue = 0.f;
		if (Material->GetScalarParameterValue(FHashedMaterialParameterInfo(ScalarParameterName), ScalarValue))
		{
			OutParameterSet.ScalarValues.Add(ScalarParameterName, ScalarValue);
		}
	}
}

void UWorldCountryOccupationRegulator::ApplyCountryLiberationLook(FWorldCountryLiberationData& LiberationData)
{
	if (not bM_CanApplyCountryLook)
	{
		return;
	}

	if (LiberationData.TargetTally.GetTotalTargets() == 0)
	{
		ApplyCountryOccupationState(LiberationData.CountryIndex, EWorldCountryOccupationState::Neutral);
		return;
	}

	if (TryApplyBlendedLiberationLook(LiberationData))
	{
		return;
	}

	// Fallback: swap whole materials, Resistance only once the country is fully liberated.
	const EWorldCountryOccupationState FallbackState = LiberationData.Liberation >= FullCountryLiberation
		                                                   ? EWorldCountryOccupationState::ResistanceOccupied
		                                                   : EWorldCountryOccupationState::SovietOccupied;
	ApplyCountryOccupationState(LiberationData.CountryIndex, FallbackState);
}

bool UWorldCountryOccupationRegulator::TryApplyBlendedLiberationLook(FWorldCountryLiberationData& LiberationData)
{
	if (M_OccupiedLookParameters.GetIsEmpty() || M_LiberatedLookParameters.GetIsEmpty())
	{
		return false;
	}

	UMaterialInstanceDynamic* LiberationMaterial = GetOrCreateLiberationMaterial(LiberationData);
	if (not IsValid(LiberationMaterial))
	{
		return false;
	}

	const FWorldCountryMaterialParameterSet BlendedParameters = WorldCountryLiberationRules::BlendParameterSets(
		M_OccupiedLookParameters, M_LiberatedLookParameters, LiberationData.Liberation);
	for (const TPair<FName, FLinearColor>& VectorParameter : BlendedParameters.VectorValues)
	{
		LiberationMaterial->SetVectorParameterValue(VectorParameter.Key, VectorParameter.Value);
	}

	for (const TPair<FName, float>& ScalarParameter : BlendedParameters.ScalarValues)
	{
		LiberationMaterial->SetScalarParameterValue(ScalarParameter.Key, ScalarParameter.Value);
	}
	return true;
}

UMaterialInstanceDynamic* UWorldCountryOccupationRegulator::GetOrCreateLiberationMaterial(
	FWorldCountryLiberationData& LiberationData)
{
	UStaticMeshComponent* CountryMesh = GetCountryMeshForIndex(LiberationData.CountryIndex);
	if (not IsValid(CountryMesh) || not IsValid(M_SovietOccupiedMaterial))
	{
		return nullptr;
	}

	if (not IsValid(LiberationData.LiberationMaterial))
	{
		LiberationData.LiberationMaterial = UMaterialInstanceDynamic::Create(M_SovietOccupiedMaterial, this);
	}

	if (not IsValid(LiberationData.LiberationMaterial))
	{
		return nullptr;
	}

	// The neutral or fallback path may have swapped the slot; put the dynamic instance back.
	if (CountryMesh->GetMaterial(CountryOccupationMaterialIndex) != LiberationData.LiberationMaterial.Get())
	{
		CountryMesh->SetMaterial(CountryOccupationMaterialIndex, LiberationData.LiberationMaterial);
	}
	return LiberationData.LiberationMaterial;
}

const FWorldCountryLiberationData* UWorldCountryOccupationRegulator::FindLiberationDataByCountryId(
	const FName CountryId) const
{
	const int32* LiberationDataIndex = M_LiberationDataIndexByCountryId.Find(CountryId);
	if (LiberationDataIndex == nullptr || not M_CountryLiberationData.IsValidIndex(*LiberationDataIndex))
	{
		return nullptr;
	}

	return &M_CountryLiberationData[*LiberationDataIndex];
}

bool UWorldCountryOccupationRegulator::GetIsValidWorldCampaignProgressManager() const
{
	if (M_WorldCampaignProgressManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldCampaignProgressManager"),
		TEXT("UWorldCountryOccupationRegulator::GetIsValidWorldCampaignProgressManager"),
		this);
	return false;
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
	M_RuntimeCountryMeshActors.Reset();
	M_RuntimeCountryIds.Reset();
	M_CountryTrianglesByIndex.Reset();
}

void UWorldCountryOccupationRegulator::InitializeRuntimeCountryEntries()
{
	/*
	 * The Countries config is the authority (the level's actor-pair list is mostly empty); the pairs are only
	 * used when no country is configured.
	 */
	if (not InitializeRuntimeCountryEntries_FromConfig())
	{
		InitializeRuntimeCountryEntries_FromActorPairs();
	}

	if (M_RuntimeCountryMeshActors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("WorldCountryOccupationRegulator found no country meshes (Countries config and actor pairs)."));
		return;
	}

	M_TraceableCountryIndices.Reserve(M_RuntimeCountryMeshActors.Num());
	M_CountryTrianglesByIndex.SetNum(M_RuntimeCountryMeshActors.Num());
	int32 CountriesWithoutTriangles = 0;
	for (int32 CountryIndex = 0; CountryIndex < M_RuntimeCountryMeshActors.Num(); CountryIndex++)
	{
		AStaticMeshActor* CountryMeshActor = GetCountryMeshActorForIndex(CountryIndex);
		UStaticMeshComponent* CountryMesh = GetCountryMeshForIndex(CountryIndex);
		if (not IsValid(CountryMeshActor) || not GetIsValidCountryMeshComponent(
			CountryMesh,
			CountryIndex,
			TEXT("UWorldCountryOccupationRegulator::InitializeRuntimeCountryEntries")))
		{
			continue;
		}

		BuildCountryTriangles(CountryMesh, M_CountryTrianglesByIndex[CountryIndex]);
		CountriesWithoutTriangles += M_CountryTrianglesByIndex[CountryIndex].Num() == 0 ? 1 : 0;
		M_TraceableCountryIndices.Add(CountryIndex);
		M_AnchorsByCountryMeshActor.FindOrAdd(TObjectKey<AStaticMeshActor>(CountryMeshActor));
	}

	if (CountriesWithoutTriangles > 0)
	{
		UE_LOG(LogRTS, Warning,
		       TEXT("WorldCountryOccupationRegulator: %d country meshes have no readable triangles (enable Allow CPU "
			       "Access on the mesh for cooked builds); they fall back to trace or bounds membership."),
		       CountriesWithoutTriangles);
	}
}

bool UWorldCountryOccupationRegulator::InitializeRuntimeCountryEntries_FromConfig()
{
	const UWorldCampaignMapContentSettings* MapContentSettings = UWorldCampaignMapContentSettings::Get();
	UWorld* World = GetWorld();
	if (MapContentSettings == nullptr || MapContentSettings->Countries.Num() == 0 || not IsValid(World))
	{
		return false;
	}

	TMap<FName, AStaticMeshActor*> StaticMeshActorsByName;
	for (TActorIterator<AStaticMeshActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		StaticMeshActorsByName.Add(ActorIterator->GetFName(), *ActorIterator);
	}

	for (const FWorldCountryDefinition& CountryDefinition : MapContentSettings->Countries)
	{
		AStaticMeshActor* const* CountryMeshActor = StaticMeshActorsByName.Find(CountryDefinition.CountryMeshActorName);
		if (CountryMeshActor == nullptr || not IsValid(*CountryMeshActor))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Country %s: no static mesh actor named %s in the campaign level."),
				*CountryDefinition.CountryId.ToString(), *CountryDefinition.CountryMeshActorName.ToString()));
			continue;
		}

		const FName CountryId = CountryDefinition.CountryId.IsNone()
			                        ? (*CountryMeshActor)->GetFName()
			                        : CountryDefinition.CountryId;
		AddRuntimeCountry(*CountryMeshActor, CountryId);
	}
	return true;
}

void UWorldCountryOccupationRegulator::InitializeRuntimeCountryEntries_FromActorPairs()
{
	for (const FWorldCountryOccupationActorPair& ActorPair : M_CountryMeshActorPairs)
	{
		// Empty pairs are common in the level and are skipped without an error.
		AStaticMeshActor* CountryMeshActor = ActorPair.M_CountryMeshActor.Get();
		if (not IsValid(CountryMeshActor))
		{
			continue;
		}

		AddRuntimeCountry(CountryMeshActor, CountryMeshActor->GetFName());
	}
}

void UWorldCountryOccupationRegulator::AddRuntimeCountry(AStaticMeshActor* CountryMeshActor, const FName CountryId)
{
	const bool bIsAlreadyAdded = M_RuntimeCountryMeshActors.ContainsByPredicate(
		[CountryMeshActor](const TWeakObjectPtr<AStaticMeshActor>& ExistingActor)
		{
			return ExistingActor.Get() == CountryMeshActor;
		});
	if (bIsAlreadyAdded)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Country mesh actor %s is configured twice; country %s is ignored."),
			*CountryMeshActor->GetName(), *CountryId.ToString()));
		return;
	}

	M_RuntimeCountryMeshActors.Add(CountryMeshActor);
	M_RuntimeCountryIds.Add(CountryId);
}

void UWorldCountryOccupationRegulator::BuildCountryTriangles(const UStaticMeshComponent* CountryMesh,
                                                             TArray<FWorldCountryTriangle2D>& OutTriangles) const
{
	OutTriangles.Reset();
	const UStaticMesh* StaticMesh = IsValid(CountryMesh) ? CountryMesh->GetStaticMesh() : nullptr;
	const FStaticMeshRenderData* RenderData = IsValid(StaticMesh) ? StaticMesh->GetRenderData() : nullptr;
	if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
	{
		return;
	}

	/*
	 * CPU copies of the render buffers exist in the editor; in a cooked build they are discarded unless the mesh
	 * allows CPU access. A null vertex pointer or an empty index view means the data is gone: fall back.
	 */
	const FStaticMeshLODResources& LODResources = RenderData->LODResources[0];
	const FPositionVertexBuffer& PositionBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
	const FIndexArrayView Indices = LODResources.IndexBuffer.GetArrayView();
	const uint32 VertexCount = PositionBuffer.GetNumVertices();
	if (PositionBuffer.GetVertexData() == nullptr || VertexCount == 0 || Indices.Num() < CountryTriangleCornerCount)
	{
		return;
	}

	const FTransform ComponentTransform = CountryMesh->GetComponentTransform();
	OutTriangles.Reserve(Indices.Num() / CountryTriangleCornerCount);
	for (int32 CornerIndex = 0; CornerIndex + 2 < Indices.Num(); CornerIndex += CountryTriangleCornerCount)
	{
		const uint32 IndexA = Indices[CornerIndex];
		const uint32 IndexB = Indices[CornerIndex + 1];
		const uint32 IndexC = Indices[CornerIndex + 2];
		if (IndexA >= VertexCount || IndexB >= VertexCount || IndexC >= VertexCount)
		{
			continue;
		}

		const FVector WorldA = ComponentTransform.TransformPosition(FVector(PositionBuffer.VertexPosition(IndexA)));
		const FVector WorldB = ComponentTransform.TransformPosition(FVector(PositionBuffer.VertexPosition(IndexB)));
		const FVector WorldC = ComponentTransform.TransformPosition(FVector(PositionBuffer.VertexPosition(IndexC)));
		FWorldCountryTriangle2D CountryTriangle;
		if (WorldCountryLiberationRules::TryMakeCountryTriangle2D(
			FVector2D(WorldA), FVector2D(WorldB), FVector2D(WorldC), CountryTriangle))
		{
			OutTriangles.Add(CountryTriangle);
		}
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
	/*
	 * The country look follows the liberation value now; the all-anchors-one-family rule in
	 * DetermineCountryOccupationState is no longer used to pick the material.
	 */
	const int32* LiberationDataIndex = M_LiberationDataIndexByCountryIndex.Find(CountryIndex);
	if (LiberationDataIndex == nullptr)
	{
		return;
	}

	RecomputeCountryLiberation(*LiberationDataIndex);
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
	if (not M_RuntimeCountryMeshActors.IsValidIndex(CountryIndex))
	{
		return nullptr;
	}

	return M_RuntimeCountryMeshActors[CountryIndex].Get();
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
	 * Bounds are useless on their own for this map (countries with overseas parts span the world), so the real
	 * shape decides: the anchor's XY is tested against each country mesh's flattened triangles (or a trace when
	 * no triangle data exists). A shape test that says Outside excludes the country. Only countries whose shape
	 * cannot be tested at all fall back to the bounds rule, and only when no shape test said Inside.
	 */
	int32 BoundsOnlyCountryIndex = INDEX_NONE;
	int32 ShapeConfirmedCountryIndex = INDEX_NONE;
	for (const int32 CountryIndex : M_TraceableCountryIndices)
	{
		UStaticMeshComponent* CountryMesh = GetCountryMeshForIndex(CountryIndex);
		if (not IsValid(CountryMesh))
		{
			continue;
		}

		const EWorldCountryShapeTestResult ShapeTestResult =
			TestCountryShapeForAnchor(CountryIndex, CountryMesh, AnchorPoint);
		if (ShapeTestResult == EWorldCountryShapeTestResult::Inside)
		{
			KeepSmallerBoundsCountry(CountryIndex, ShapeConfirmedCountryIndex);
			continue;
		}

		if (ShapeTestResult == EWorldCountryShapeTestResult::Untestable
			&& GetDoesCountryMeshBoundsContainAnchor(CountryMesh, AnchorPoint))
		{
			KeepSmallerBoundsCountry(CountryIndex, BoundsOnlyCountryIndex);
		}
	}

	OutCountryIndex = GetIsCountryIndexValid(ShapeConfirmedCountryIndex)
		                  ? ShapeConfirmedCountryIndex
		                  : BoundsOnlyCountryIndex;
	return GetIsCountryIndexValid(OutCountryIndex);
}

void UWorldCountryOccupationRegulator::KeepSmallerBoundsCountry(const int32 CandidateCountryIndex,
                                                                int32& InOutBestCountryIndex) const
{
	if (not GetIsCountryIndexValid(InOutBestCountryIndex))
	{
		InOutBestCountryIndex = CandidateCountryIndex;
		return;
	}

	const float CandidateBoundsArea = GetCountryMeshBoundsArea2D(GetCountryMeshForIndex(CandidateCountryIndex));
	if (CandidateBoundsArea < GetCountryMeshBoundsArea2D(GetCountryMeshForIndex(InOutBestCountryIndex)))
	{
		InOutBestCountryIndex = CandidateCountryIndex;
	}
}

UWorldCountryOccupationRegulator::EWorldCountryShapeTestResult
UWorldCountryOccupationRegulator::TestCountryShapeForAnchor(const int32 CountryIndex,
                                                            UStaticMeshComponent* CountryMesh,
                                                            const AAnchorPoint* AnchorPoint) const
{
	if (not bM_UseShapeContainmentTest || not IsValid(CountryMesh) || not IsValid(AnchorPoint))
	{
		return EWorldCountryShapeTestResult::Untestable;
	}

	const FVector AnchorLocation = AnchorPoint->GetActorLocation();
	if (M_CountryTrianglesByIndex.IsValidIndex(CountryIndex) && M_CountryTrianglesByIndex[CountryIndex].Num() > 0)
	{
		return WorldCountryLiberationRules::GetIsPointInCountryTriangles(
			       FVector2D(AnchorLocation), M_CountryTrianglesByIndex[CountryIndex])
			       ? EWorldCountryShapeTestResult::Inside
			       : EWorldCountryShapeTestResult::Outside;
	}

	// Without triangles and without query collision nothing can test the shape; the bounds rule stays in charge.
	if (not CountryMesh->IsQueryCollisionEnabled())
	{
		return EWorldCountryShapeTestResult::Untestable;
	}

	const FBox CountryBounds = CountryMesh->Bounds.GetBox();
	const FVector TraceStart(AnchorLocation.X, AnchorLocation.Y, CountryBounds.Max.Z + CountryShapeTraceHeightMargin);
	const FVector TraceEnd(AnchorLocation.X, AnchorLocation.Y, CountryBounds.Min.Z - CountryShapeTraceHeightMargin);
	// Complex tracing follows the visible country outline instead of a convex or box simple collision.
	constexpr bool bTraceComplex = true;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(WorldCountryShapeContainment), bTraceComplex);
	FHitResult HitResult;
	return CountryMesh->LineTraceComponent(HitResult, TraceStart, TraceEnd, TraceParams)
		       ? EWorldCountryShapeTestResult::Inside
		       : EWorldCountryShapeTestResult::Outside;
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
	return M_RuntimeCountryMeshActors.IsValidIndex(CountryIndex);
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
		FString::Printf(TEXT("M_RuntimeCountryMeshActors[%d].StaticMeshComponent"), CountryIndex),
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
