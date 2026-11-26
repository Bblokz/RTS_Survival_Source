// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldFow/WorldUnknownMissionMarkerComponent.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintRules.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintSettings.h"

namespace WorldUnknownMissionMarkerConstants
{
	// The marker mesh is expected to be 100 units wide and high around its pivot, like the engine cylinder.
	constexpr float MarkerMeshSize = 100.f;
	constexpr int32 MarkerMaterialIndex = 0;
	const FName ColorParameterName = TEXT("Color");
	const FName RoughnessParameterName = TEXT("Roughness");
	const FName MetallicParameterName = TEXT("Metallic");
	// Matt and not metallic, so the puck shows no highlight that could be read as a shape.
	constexpr float MarkerRoughness = 0.9f;
	constexpr float MarkerMetallic = 0.f;
}

UWorldUnknownMissionMarkerComponent::UWorldUnknownMissionMarkerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
	// The mission's own scale and rotation must not reach the marker: it is the same on every mission.
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	SetCastShadow(false);
	SetReceivesDecals(false);
	SetGenerateOverlapEvents(false);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	// The world controller's cursor trace runs on the visibility channel.
	SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetVisibility(false);
}

bool UWorldUnknownMissionMarkerComponent::SetupUnknownMarker(const FVector& MapLocation, const float FogTopZ)
{
	using namespace WorldUnknownMissionMarkerConstants;
	const UWorldFogHintSettings* const Settings = UWorldFogHintSettings::Get();
	UStaticMesh* const MarkerMesh = Settings->UnknownMissionMarkerMesh.LoadSynchronous();
	if (not IsValid(MarkerMesh))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UWorldUnknownMissionMarkerComponent: the unknown mission marker mesh of the fog hint settings is missing."));
		return false;
	}

	SetStaticMesh(MarkerMesh);
	SetupMarkerMaterial();
	SetWorldScale3D(WorldFogHintRules::GetUnknownMarkerScale(
		Settings->UnknownMissionMarkerDiameter, Settings->UnknownMissionMarkerHeight, MarkerMeshSize));
	SetWorldRotation(FRotator::ZeroRotator);
	SetWorldLocation(FVector(MapLocation.X, MapLocation.Y, WorldFogHintRules::GetUnknownMarkerCenterZ(
		                         FogTopZ, Settings->HeightAboveFog, Settings->UnknownMissionMarkerHeight)));
	return true;
}

void UWorldUnknownMissionMarkerComponent::SetupMarkerMaterial()
{
	using namespace WorldUnknownMissionMarkerConstants;
	const UWorldFogHintSettings* const Settings = UWorldFogHintSettings::Get();
	UMaterialInterface* const MarkerMaterial = Settings->UnknownMissionMarkerMaterial.LoadSynchronous();
	if (not IsValid(MarkerMaterial))
	{
		// The mesh keeps its own plain material; the marker still shows.
		return;
	}

	M_MarkerMaterial = UMaterialInstanceDynamic::Create(MarkerMaterial, this);
	if (not IsValid(M_MarkerMaterial))
	{
		return;
	}

	M_MarkerMaterial->SetVectorParameterValue(ColorParameterName, Settings->UnknownMissionMarkerColor);
	M_MarkerMaterial->SetScalarParameterValue(RoughnessParameterName, MarkerRoughness);
	M_MarkerMaterial->SetScalarParameterValue(MetallicParameterName, MarkerMetallic);
	SetMaterial(MarkerMaterialIndex, M_MarkerMaterial);
}

void UWorldUnknownMissionMarkerComponent::SetIsMarkerShown(const bool bIsShown)
{
	bM_IsMarkerShown = bIsShown;
	SetVisibility(bIsShown);
	SetCollisionEnabled(bIsShown ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
