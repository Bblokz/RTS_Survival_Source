// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
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
		if (IsValid(PieceComponent) && PieceComponent != UnknownMarker)
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
