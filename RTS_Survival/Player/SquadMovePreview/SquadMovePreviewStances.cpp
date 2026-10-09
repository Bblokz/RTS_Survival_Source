#include "SquadMovePreviewStances.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Player/SquadMovePreview/SquadMovePlanner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace SquadMovePreviewStancesPrivate
{
	constexpr int32 StanceCount = static_cast<int32>(ESquadPreviewStance::Count);

	// An instance that shows nothing: no size, so it draws no pixel wherever it is.
	const FTransform ParkedInstanceTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);

	TSoftObjectPtr<UStaticMesh> GetStanceMesh(
		const URTSCoverFinderDeveloperSettings& Settings,
		const ESquadPreviewStance Stance)
	{
		switch (Stance)
		{
		case ESquadPreviewStance::CrouchCover:
			return Settings.M_PreviewStanceCrouchCover;
		case ESquadPreviewStance::HighCover:
			return Settings.M_PreviewStanceHighCover;
		case ESquadPreviewStance::ProneCover:
			return Settings.M_PreviewStanceProneCover;
		case ESquadPreviewStance::TrenchCover:
			return Settings.M_PreviewStanceTrenchCover;
		case ESquadPreviewStance::NoCover:
		default:
			return Settings.M_PreviewStanceNoCover;
		}
	}
}

ESquadPreviewStance FSquadMovePreviewStances::GetStanceForPosition(const FSquadUnitPlannedPosition& Position)
{
	if (not Position.GetIsCover())
	{
		return ESquadPreviewStance::NoCover;
	}
	switch (Position.CoverPoint.CoverType)
	{
	case ERTSCoverType::Crouch:
		return ESquadPreviewStance::CrouchCover;
	case ERTSCoverType::Prone:
		return ESquadPreviewStance::ProneCover;
	case ERTSCoverType::TrenchStandUp:
		return ESquadPreviewStance::TrenchCover;
	case ERTSCoverType::StandingLeft:
	case ERTSCoverType::StandingRight:
	default:
		return ESquadPreviewStance::HighCover;
	}
}

void FSquadMovePreviewStances::Setup(UWorld& World)
{
	using namespace SquadMovePreviewStancesPrivate;
	Destroy();
	const URTSCoverFinderDeveloperSettings* Settings = URTSCoverFinderDeveloperSettings::Get();
	M_StanceComponents.SetNum(StanceCount);
	M_ShownInstanceCounts.Init(0, StanceCount);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_StanceActor = World.SpawnActor<AActor>(SpawnParameters);
	if (not IsValid(Settings) || not IsValid(M_StanceActor))
	{
		return;
	}
	M_StanceYawOffsetDegrees = Settings->M_PreviewStanceYawOffsetDegrees;
	for (int32 StanceIndex = 0; StanceIndex < StanceCount; ++StanceIndex)
	{
		// Five small meshes, needed from the first selected squad on: loaded here rather than on first use.
		UStaticMesh* StanceMesh = GetStanceMesh(*Settings, static_cast<ESquadPreviewStance>(StanceIndex)).LoadSynchronous();
		if (not IsValid(StanceMesh))
		{
			RTSFunctionLibrary::ReportError(
				"Squad move preview has no stance mesh for stance index " + FString::FromInt(StanceIndex) +
				". Set it in Project Settings > Cover Finder > Squad Move Preview.");
			continue;
		}
		M_StanceComponents[StanceIndex] = CreateStanceComponent(
			*M_StanceActor,
			StanceMesh,
			Settings->M_PreviewStancePreloadCount);
	}
	bM_IsVisible = false;
}

UInstancedStaticMeshComponent* FSquadMovePreviewStances::CreateStanceComponent(
	AActor& Owner,
	UStaticMesh* StanceMesh,
	const int32 PreloadCount) const
{
	UInstancedStaticMeshComponent* StanceComponent = NewObject<UInstancedStaticMeshComponent>(&Owner);
	StanceComponent->SetStaticMesh(StanceMesh);
	StanceComponent->SetMobility(EComponentMobility::Movable);
	// Purely something to look at: nothing may bump into it, path around it, select it or be shadowed by it.
	StanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StanceComponent->SetGenerateOverlapEvents(false);
	StanceComponent->SetCanEverAffectNavigation(false);
	StanceComponent->SetCastShadow(false);
	StanceComponent->bReceivesDecals = false;
	StanceComponent->bSelectable = false;
	StanceComponent->SetVisibility(false);
	// Not attached to anything: instances are placed in world space.
	StanceComponent->RegisterComponent();
	TArray<FTransform> ParkedTransforms;
	ParkedTransforms.Init(SquadMovePreviewStancesPrivate::ParkedInstanceTransform, FMath::Max(0, PreloadCount));
	StanceComponent->AddInstances(ParkedTransforms, false, true);
	return StanceComponent;
}

void FSquadMovePreviewStances::Destroy()
{
	for (UInstancedStaticMeshComponent* StanceComponent : M_StanceComponents)
	{
		if (IsValid(StanceComponent))
		{
			StanceComponent->DestroyComponent();
		}
	}
	M_StanceComponents.Reset();
	M_ShownInstanceCounts.Reset();
	bM_IsVisible = false;
	if (IsValid(M_StanceActor))
	{
		M_StanceActor->Destroy();
	}
	M_StanceActor = nullptr;
}

void FSquadMovePreviewStances::ShowPlan(const FSquadMovePlan& Plan)
{
	using namespace SquadMovePreviewStancesPrivate;
	if (M_StanceComponents.Num() != StanceCount)
	{
		return;
	}
	TArray<FTransform> TransformsPerStance[StanceCount];
	for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
	{
		if (Position.Type == ESquadPlannedPositionType::None)
		{
			continue;
		}
		const FRotator StanceRotation(0.0f, Position.Facing.Rotation().Yaw + M_StanceYawOffsetDegrees, 0.0f);
		TransformsPerStance[static_cast<int32>(GetStanceForPosition(Position))].Emplace(StanceRotation, Position.Location);
	}
	for (int32 StanceIndex = 0; StanceIndex < StanceCount; ++StanceIndex)
	{
		ApplyStanceTransforms(StanceIndex, TransformsPerStance[StanceIndex]);
	}
	SetComponentsVisible(true);
}

void FSquadMovePreviewStances::ApplyStanceTransforms(const int32 StanceIndex, const TArray<FTransform>& Transforms)
{
	using namespace SquadMovePreviewStancesPrivate;
	UInstancedStaticMeshComponent* StanceComponent = M_StanceComponents[StanceIndex];
	if (not IsValid(StanceComponent))
	{
		return;
	}
	const int32 PreviouslyShownCount = M_ShownInstanceCounts[StanceIndex];
	if (Transforms.IsEmpty() && PreviouslyShownCount == 0)
	{
		return;
	}
	// A selection larger than the preloaded pool makes the pool grow once; it never shrinks again.
	const int32 MissingInstanceCount = Transforms.Num() - StanceComponent->GetInstanceCount();
	if (MissingInstanceCount > 0)
	{
		TArray<FTransform> ParkedTransforms;
		ParkedTransforms.Init(ParkedInstanceTransform, MissingInstanceCount);
		StanceComponent->AddInstances(ParkedTransforms, false, true);
	}
	// Only the instances that show something now or did a moment ago are written; the rest stay parked.
	TArray<FTransform> UpdatedTransforms = Transforms;
	for (int32 InstanceIndex = Transforms.Num(); InstanceIndex < PreviouslyShownCount; ++InstanceIndex)
	{
		UpdatedTransforms.Add(ParkedInstanceTransform);
	}
	StanceComponent->BatchUpdateInstancesTransforms(0, UpdatedTransforms, true, false, true);
	// Moving instances does not move the component's bounds by itself, and with the bounds still around the
	// parked instances the renderer culls the whole component.
	StanceComponent->UpdateBounds();
	StanceComponent->MarkRenderStateDirty();
	M_ShownInstanceCounts[StanceIndex] = Transforms.Num();
}

void FSquadMovePreviewStances::Hide()
{
	SetComponentsVisible(false);
}

void FSquadMovePreviewStances::SetComponentsVisible(const bool bVisible)
{
	if (bVisible == bM_IsVisible)
	{
		return;
	}
	bM_IsVisible = bVisible;
	for (UInstancedStaticMeshComponent* StanceComponent : M_StanceComponents)
	{
		if (IsValid(StanceComponent))
		{
			StanceComponent->SetVisibility(bVisible);
		}
	}
}

int32 FSquadMovePreviewStances::GetShownInstanceCount(const ESquadPreviewStance Stance) const
{
	const int32 StanceIndex = static_cast<int32>(Stance);
	return bM_IsVisible && M_ShownInstanceCounts.IsValidIndex(StanceIndex) ? M_ShownInstanceCounts[StanceIndex] : 0;
}

int32 FSquadMovePreviewStances::GetShownInstanceCount() const
{
	int32 TotalShownCount = 0;
	for (int32 StanceIndex = 0; StanceIndex < M_ShownInstanceCounts.Num(); ++StanceIndex)
	{
		TotalShownCount += GetShownInstanceCount(static_cast<ESquadPreviewStance>(StanceIndex));
	}
	return TotalShownCount;
}

bool FSquadMovePreviewStances::TryGetShownInstanceTransform(
	const ESquadPreviewStance Stance,
	const int32 ShownIndex,
	FTransform& OutTransform) const
{
	const int32 StanceIndex = static_cast<int32>(Stance);
	if (ShownIndex < 0 || ShownIndex >= GetShownInstanceCount(Stance) || not M_StanceComponents.IsValidIndex(StanceIndex))
	{
		return false;
	}
	const UInstancedStaticMeshComponent* StanceComponent = M_StanceComponents[StanceIndex];
	return IsValid(StanceComponent) && StanceComponent->GetInstanceTransform(ShownIndex, OutTransform, true);
}
