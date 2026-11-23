// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignMapContentSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace WorldBuildNodeVisualConstants
{
	const TCHAR* const DefaultVisualMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* const DefaultVisualMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FName DefaultVisualColorParameterName = TEXT("Color");
	constexpr int32 DefaultVisualMaterialIndex = 0;
	// The engine cylinder is 100 units wide and tall; a flat wide disc reads as a site on the map.
	const FVector DefaultVisualScale(3.f, 3.f, 0.4f);
	const FLinearColor LockedColor(0.25f, 0.25f, 0.25f);
	const FLinearColor UnlockedColor(0.95f, 0.75f, 0.15f);
	const FLinearColor BuiltColor(0.2f, 0.7f, 0.25f);
}

AWorldBuildNode::AWorldBuildNode()
{
	PrimaryActorTick.bCanEverTick = false;

	M_RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_RootSceneComponent);

	M_DefaultVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DefaultVisualMesh"));
	M_DefaultVisualMesh->SetupAttachment(M_RootSceneComponent);
	M_DefaultVisualMesh->SetRelativeScale3D(WorldBuildNodeVisualConstants::DefaultVisualScale);
	// Query only on visibility so the world controller's cursor trace can find the node.
	M_DefaultVisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	M_DefaultVisualMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_DefaultVisualMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	M_DefaultVisualMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultVisualMeshFinder(
		WorldBuildNodeVisualConstants::DefaultVisualMeshPath);
	if (DefaultVisualMeshFinder.Succeeded())
	{
		M_DefaultVisualMesh->SetStaticMesh(DefaultVisualMeshFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultVisualMaterialFinder(
		WorldBuildNodeVisualConstants::DefaultVisualMaterialPath);
	if (DefaultVisualMaterialFinder.Succeeded())
	{
		M_DefaultVisualMesh->SetMaterial(WorldBuildNodeVisualConstants::DefaultVisualMaterialIndex,
		                                 DefaultVisualMaterialFinder.Object);
	}
}

void AWorldBuildNode::InitializeFromDefinition(const FWorldBuildNodeDefinition& BuildNodeDefinition)
{
	M_BuildNodeId = BuildNodeDefinition.BuildNodeId;
	M_CountryId = BuildNodeDefinition.CountryId;
	M_UnlockOrder = FMath::Max(0, BuildNodeDefinition.UnlockOrder);
}

void AWorldBuildNode::SetNodeState(const EWorldBuildNodeState NewState)
{
	if (static_cast<uint8>(NewState) <= static_cast<uint8>(M_NodeState))
	{
		return;
	}

	M_NodeState = NewState;
	UpdateDefaultVisual();
	BP_OnNodeStateChanged(M_NodeState, M_Building);
}

void AWorldBuildNode::SetBuilding(const EWorldCampaignBuilding NewBuilding)
{
	if (NewBuilding == EWorldCampaignBuilding::None || M_Building != EWorldCampaignBuilding::None)
	{
		return;
	}

	M_Building = NewBuilding;
	M_NodeState = EWorldBuildNodeState::Built;
	UpdateDefaultVisual();
	BP_OnNodeStateChanged(M_NodeState, M_Building);
	BP_OnBuildingBuilt(M_Building);
}

void AWorldBuildNode::BeginPlay()
{
	Super::BeginPlay();

	if (not GetIsValidDefaultVisualMesh())
	{
		return;
	}

	M_DefaultVisualMesh->SetVisibility(bM_UseDefaultVisual);
	if (bM_UseDefaultVisual)
	{
		M_DefaultVisualMaterial = M_DefaultVisualMesh->CreateDynamicMaterialInstance(
			WorldBuildNodeVisualConstants::DefaultVisualMaterialIndex);
	}

	UpdateDefaultVisual();
	BP_OnNodeStateChanged(M_NodeState, M_Building);
}

void AWorldBuildNode::UpdateDefaultVisual() const
{
	if (not bM_UseDefaultVisual || not IsValid(M_DefaultVisualMaterial))
	{
		return;
	}

	M_DefaultVisualMaterial->SetVectorParameterValue(WorldBuildNodeVisualConstants::DefaultVisualColorParameterName,
	                                                 GetDefaultVisualColor());
}

FLinearColor AWorldBuildNode::GetDefaultVisualColor() const
{
	switch (M_NodeState)
	{
	case EWorldBuildNodeState::Unlocked:
		return WorldBuildNodeVisualConstants::UnlockedColor;
	case EWorldBuildNodeState::Built:
		return WorldBuildNodeVisualConstants::BuiltColor;
	case EWorldBuildNodeState::Locked:
	default:
		return WorldBuildNodeVisualConstants::LockedColor;
	}
}

bool AWorldBuildNode::GetIsValidDefaultVisualMesh() const
{
	if (IsValid(M_DefaultVisualMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_DefaultVisualMesh"),
		TEXT("GetIsValidDefaultVisualMesh"),
		this
	);
	return false;
}
