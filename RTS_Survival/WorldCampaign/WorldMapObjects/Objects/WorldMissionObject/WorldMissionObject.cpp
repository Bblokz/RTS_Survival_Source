// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintSettings.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldFortificationModificationsComponent.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowCloud.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldUnknownMissionMarkerComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldCampaignMissionNames.h"

AWorldMissionObject::AWorldMissionObject()
{
	M_FortificationModificationsComponent =
		CreateDefaultSubobject<UWorldFortificationModificationsComponent>(TEXT("FortificationModifications"));
}

namespace WorldMissionObjectConstants
{
	// Component tag of the tall landmark beside a scripted mission (set by Tools/WorldMapPieces/build_landmarks.py).
	const FName LandmarkComponentTag = TEXT("WorldLandmark");
}

void AWorldMissionObject::OnClearedByPlayer()
{
	Super::OnClearedByPlayer();
	// A won mission is never unidentified, whatever the fog says at that moment.
	ApplyFogPresentation(EWorldMissionFogPresentation::AsFogState);
}

void AWorldMissionObject::ApplyFogPresentation(const EWorldMissionFogPresentation NewPresentation)
{
	if (NewPresentation == M_FogPresentation)
	{
		return;
	}

	M_FogPresentation = NewPresentation;
	if (NewPresentation == EWorldMissionFogPresentation::PlainMarker)
	{
		ShowUnknownMarker();
		return;
	}

	// Seen through an opening in the fog or by its fog state: the piece itself is whole in both cases.
	ShowRealPiece();
}

UWorldUnknownMissionMarkerComponent* AWorldMissionObject::GetOrCreateUnknownMarker()
{
	if (IsValid(M_UnknownMarker) || bM_HasUnknownMarkerFailed)
	{
		return M_UnknownMarker;
	}

	UWorldUnknownMissionMarkerComponent* const NewMarker = NewObject<UWorldUnknownMissionMarkerComponent>(
		this, TEXT("UnknownMissionMarker"));
	if (USceneComponent* const PieceRoot = GetRootComponent())
	{
		NewMarker->SetupAttachment(PieceRoot);
	}
	NewMarker->RegisterComponent();

	// A map without a fog cloud hides nothing; the marker then lies on the height of the mission itself.
	float FogTopZ = GetActorLocation().Z;
	AWorldFowCloud::TryGetFogTopZ(GetWorld(), FogTopZ);
	if (not NewMarker->SetupUnknownMarker(GetActorLocation(), FogTopZ))
	{
		bM_HasUnknownMarkerFailed = true;
		NewMarker->DestroyComponent();
		return nullptr;
	}

	M_UnknownMarker = NewMarker;
	return M_UnknownMarker;
}

void AWorldMissionObject::ShowUnknownMarker()
{
	UWorldUnknownMissionMarkerComponent* const UnknownMarker = GetOrCreateUnknownMarker();
	if (not IsValid(UnknownMarker))
	{
		return;
	}

	TInlineComponentArray<USceneComponent*> PieceComponents(this);
	for (USceneComponent* const PieceComponent : PieceComponents)
	{
		// A landmark stands above the fog on purpose and stays, also while the mission itself is unknown.
		const bool bIsLandmark = IsValid(PieceComponent)
			&& PieceComponent->ComponentHasTag(WorldMissionObjectConstants::LandmarkComponentTag);
		if (IsValid(PieceComponent) && PieceComponent != UnknownMarker && not bIsLandmark)
		{
			HidePieceComponentForFog(PieceComponent);
		}
	}

	UnknownMarker->SetIsMarkerShown(true);
}

void AWorldMissionObject::HidePieceComponentForFog(USceneComponent* PieceComponent)
{
	FWorldMissionFogHiddenComponent HiddenComponent;
	HiddenComponent.Component = PieceComponent;
	HiddenComponent.bWasVisible = PieceComponent->IsVisible();
	// Meshes and lights alike: nothing of the real piece may show or shine while it is unknown.
	PieceComponent->SetVisibility(false);

	// Only the marker may answer the cursor, or the hidden piece would be found by its shape.
	if (UPrimitiveComponent* const PiecePrimitive = Cast<UPrimitiveComponent>(PieceComponent))
	{
		HiddenComponent.bHadCollision = true;
		HiddenComponent.CollisionBeforeFog = PiecePrimitive->GetCollisionEnabled();
		PiecePrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	M_ComponentsHiddenForFog.Add(HiddenComponent);
}

void AWorldMissionObject::ShowRealPiece()
{
	if (IsValid(M_UnknownMarker))
	{
		M_UnknownMarker->SetIsMarkerShown(false);
	}

	for (const FWorldMissionFogHiddenComponent& HiddenComponent : M_ComponentsHiddenForFog)
	{
		USceneComponent* const PieceComponent = HiddenComponent.Component.Get();
		if (not IsValid(PieceComponent))
		{
			continue;
		}

		if (HiddenComponent.bWasVisible)
		{
			PieceComponent->SetVisibility(true);
		}

		UPrimitiveComponent* const PiecePrimitive = Cast<UPrimitiveComponent>(PieceComponent);
		if (HiddenComponent.bHadCollision && IsValid(PiecePrimitive))
		{
			PiecePrimitive->SetCollisionEnabled(HiddenComponent.CollisionBeforeFog);
		}
	}

	M_ComponentsHiddenForFog.Reset();
}

void AWorldMissionObject::InitializeForAnchorWithMissionType(AAnchorPoint* AnchorPoint, EMapMission MissionType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_MissionType = MissionType;
	CreateLandmark();
}

void AWorldMissionObject::CreateLandmark()
{
	const UWorldFogHintSettings* const Settings = UWorldFogHintSettings::Get();
	USceneComponent* const PieceRoot = GetRootComponent();
	if (IsValid(M_Landmark) || Settings == nullptr || not Settings->bShowMissionLandmarks || not IsValid(PieceRoot))
	{
		return;
	}

	const FWorldMissionLandmark* const Landmark = Settings->MissionLandmarks.Find(M_MissionType);
	UStaticMesh* const LandmarkMesh = Landmark != nullptr ? Landmark->Mesh.LoadSynchronous() : nullptr;
	if (not IsValid(LandmarkMesh))
	{
		return;
	}

	M_Landmark = NewObject<UStaticMeshComponent>(this, TEXT("MissionLandmark"));
	M_Landmark->SetStaticMesh(LandmarkMesh);
	// No collision: an unknown mission must not be found or clicked through its landmark.
	M_Landmark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_Landmark->SetGenerateOverlapEvents(false);
	M_Landmark->SetCanEverAffectNavigation(false);
	M_Landmark->ComponentTags.Add(WorldMissionObjectConstants::LandmarkComponentTag);
	M_Landmark->SetupAttachment(PieceRoot);
	M_Landmark->RegisterComponent();
	M_Landmark->SetRelativeLocationAndRotation(FVector(Landmark->OffsetBehind, Landmark->OffsetRight, 0.f),
	                                           FRotator(0.f, Landmark->Yaw, 0.f));
}

FText AWorldMissionObject::GetMissionDisplayName() const
{
	return WorldCampaignMissionNames::GetMissionDisplayName(M_MissionType);
}

FEnemyOrMissionMapItemUIData AWorldMissionObject::GetMapItemUIData() const
{
	return BuildMapItemUIData();
}

int32 AWorldMissionObject::GetBaseDifficultyPercentage() const
{
	if (const UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		return StrengthEstimationComponent->GetBaseFortificationStrengthPercentage();
	}

	return 0;
}

void AWorldMissionObject::ResetStrategicReport()
{
	if (UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		StrengthEstimationComponent->ResetStrategicSupportReport();
	}
}

void AWorldMissionObject::AddStrategicSupportReason(
	const FWorldStrengthReason& StrengthReason)
{
	if (UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		StrengthEstimationComponent->AddStrategicSupportReason(StrengthReason);
	}
}

FEnemyOrMissionMapItemUIData AWorldMissionObject::BuildMapItemUIData() const
{
	FEnemyOrMissionMapItemUIData UIData = M_MapItemUIData;
	// A story mission can stand in on another mission's Blueprint, so its own name wins over the authored title.
	const FText MissionName = GetMissionDisplayName();
	if (not MissionName.IsEmpty())
	{
		UIData.TitleText = WorldCampaignMissionNames::MakeMissionTitleText(M_MapItemUIData.TitleText, MissionName);
	}

	if (const UWorldStrengthEstimationComponent* StrengthEstimationComponent = GetWorldStrengthEstimationComponent())
	{
		UIData.SetStrengthEstimation(StrengthEstimationComponent->GetStrengthEstimationMessage());
	}

	return UIData;
}
