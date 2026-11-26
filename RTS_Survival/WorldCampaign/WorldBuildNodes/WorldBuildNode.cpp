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
	const FVector DefaultVisualScale(1.2f, 1.2f, 0.2f);
	const TCHAR* const HexPlateMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* const HexPlateMaterialPath =
		TEXT("/Game/RTS_Survival/WorldCampaign/BuildSites/M_BuildSiteHexPlate.M_BuildSiteHexPlate");
	constexpr int32 HexPlateMaterialIndex = 0;
	// The engine plane is 100 units wide.
	constexpr float HexPlateMeshWidth = 100.f;
	// The material draws the plate over this share of the plane; the rest is room for shadow and glow.
	constexpr float HexPlateShareOfPlane = 0.52f;
	// Above the country fill and the roads, below the map pieces.
	constexpr float HexPlateHeightAboveGround = 6.f;
	// Pieces are modelled with their front on -X; the map camera sees +Y as the near side.
	constexpr float BuildingPieceYaw = -90.f;
	// The hidden cylinder is the click target; this makes it as wide as the plate.
	constexpr float ClickTargetHeightScale = 0.2f;
	const FName HexPlateStateParameterName = TEXT("State");
	const FName HexPlateSignParameterName = TEXT("SignIndex");
	const FName HexPlateHoverParameterName = TEXT("Hover");
	constexpr float HexPlateSignLock = 0.f;
	constexpr float HexPlateSignPlus = 1.f;
	constexpr float HexPlateSignWatchPost = 2.f;
	constexpr float HexPlateSignSupplyDepot = 3.f;
	constexpr float HexPlateSignAirfield = 4.f;
	constexpr float HexPlateSignBattery = 5.f;
	// Empty centre: the building's model stands there instead.
	constexpr float HexPlateSignNone = 6.f;
	const TCHAR* const WatchPostPieceMeshPath = TEXT(
		"/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/WatchPost/SM_BuildSitePiece_WatchPost.SM_BuildSitePiece_WatchPost");
	const TCHAR* const AirfieldPieceMeshPath = TEXT(
		"/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/SM_BuildSitePiece_Airfield.SM_BuildSitePiece_Airfield");
	const TCHAR* const BatteryPieceMeshPath = TEXT(
		"/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Battery/SM_BuildSitePiece_Battery.SM_BuildSitePiece_Battery");
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

	M_HexPlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HexPlateMesh"));
	M_HexPlateMesh->SetupAttachment(M_RootSceneComponent);
	M_HexPlateMesh->SetRelativeLocation(FVector(0.f, 0.f, WorldBuildNodeVisualConstants::HexPlateHeightAboveGround));
	M_HexPlateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_HexPlateMesh->SetGenerateOverlapEvents(false);
	M_HexPlateMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HexPlateMeshFinder(
		WorldBuildNodeVisualConstants::HexPlateMeshPath);
	if (HexPlateMeshFinder.Succeeded())
	{
		M_HexPlateMesh->SetStaticMesh(HexPlateMeshFinder.Object);
	}

	M_HexPlateMaterialAsset = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(WorldBuildNodeVisualConstants::HexPlateMaterialPath));

	M_BuildingPieceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingPieceMesh"));
	M_BuildingPieceMesh->SetupAttachment(M_RootSceneComponent);
	M_BuildingPieceMesh->SetRelativeLocation(
		FVector(0.f, 0.f, WorldBuildNodeVisualConstants::HexPlateHeightAboveGround));
	M_BuildingPieceMesh->SetRelativeRotation(
		FRotator(0.f, WorldBuildNodeVisualConstants::BuildingPieceYaw, 0.f));
	// The hidden cylinder below stays the click target, so the cursor trace must pass through the model.
	M_BuildingPieceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_BuildingPieceMesh->SetGenerateOverlapEvents(false);
	M_BuildingPieceMesh->SetVisibility(false);

	M_BuildingPieceMeshes.Add(EWorldCampaignBuilding::WatchPost, TSoftObjectPtr<UStaticMesh>(
		                          FSoftObjectPath(WorldBuildNodeVisualConstants::WatchPostPieceMeshPath)));
	M_BuildingPieceMeshes.Add(EWorldCampaignBuilding::Airfield, TSoftObjectPtr<UStaticMesh>(
		                          FSoftObjectPath(WorldBuildNodeVisualConstants::AirfieldPieceMeshPath)));
	M_BuildingPieceMeshes.Add(EWorldCampaignBuilding::Battery, TSoftObjectPtr<UStaticMesh>(
		                          FSoftObjectPath(WorldBuildNodeVisualConstants::BatteryPieceMeshPath)));
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
	UpdateHexPlate();
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
	UpdateBuildingPiece();
	UpdateHexPlate();
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

	BeginPlay_InitHexPlate();
	UpdateDefaultVisual();
	UpdateBuildingPiece();
	UpdateHexPlate();
	BP_OnNodeStateChanged(M_NodeState, M_Building);
}

void AWorldBuildNode::BeginPlay_InitHexPlate()
{
	using namespace WorldBuildNodeVisualConstants;
	if (not GetIsValidHexPlateMesh())
	{
		return;
	}

	UMaterialInterface* HexPlateMaterial = bM_UseDefaultVisual ? M_HexPlateMaterialAsset.LoadSynchronous() : nullptr;
	if (not IsValid(HexPlateMaterial))
	{
		// No plate material (or a Blueprint look): the cylinder, when in use, keeps showing the state.
		M_HexPlateMesh->SetVisibility(false);
		return;
	}

	const float PlaneScale = M_HexPlateWidth / (HexPlateShareOfPlane * HexPlateMeshWidth);
	M_HexPlateMesh->SetRelativeScale3D(FVector(PlaneScale, PlaneScale, 1.f));
	M_HexPlateMesh->SetMaterial(HexPlateMaterialIndex, HexPlateMaterial);
	M_HexPlateMaterial = M_HexPlateMesh->CreateDynamicMaterialInstance(HexPlateMaterialIndex);
	M_HexPlateMesh->SetVisibility(true);

	// The plate replaces the cylinder's look; the cylinder stays as the click target, as wide as the plate.
	M_DefaultVisualMesh->SetVisibility(false);
	const float ClickTargetScale = M_HexPlateWidth / HexPlateMeshWidth;
	M_DefaultVisualMesh->SetRelativeScale3D(FVector(ClickTargetScale, ClickTargetScale, ClickTargetHeightScale));
}

bool AWorldBuildNode::TryGetBuildingPieceSocketLocation(const FName SocketName, FVector& OutLocation) const
{
	// Quiet on purpose: a node without a model is a normal case for the caller's fallback.
	const bool bHasSocket = bM_ShowsBuildingPiece && IsValid(M_BuildingPieceMesh) && not SocketName.IsNone()
		&& M_BuildingPieceMesh->DoesSocketExist(SocketName);
	if (not bHasSocket)
	{
		return false;
	}

	OutLocation = M_BuildingPieceMesh->GetSocketLocation(SocketName);
	return true;
}

void AWorldBuildNode::SetIsHovered(const bool bIsHovered)
{
	if (not IsValid(M_HexPlateMaterial))
	{
		return;
	}

	M_HexPlateMaterial->SetScalarParameterValue(WorldBuildNodeVisualConstants::HexPlateHoverParameterName,
	                                            bIsHovered ? 1.f : 0.f);
}

void AWorldBuildNode::UpdateHexPlate() const
{
	if (not IsValid(M_HexPlateMaterial))
	{
		return;
	}

	M_HexPlateMaterial->SetScalarParameterValue(WorldBuildNodeVisualConstants::HexPlateStateParameterName,
	                                            static_cast<float>(M_NodeState));
	M_HexPlateMaterial->SetScalarParameterValue(WorldBuildNodeVisualConstants::HexPlateSignParameterName,
	                                            GetHexPlateSignIndex());
}

void AWorldBuildNode::UpdateBuildingPiece()
{
	bM_ShowsBuildingPiece = false;
	if (not GetIsValidBuildingPieceMesh())
	{
		return;
	}

	// A Blueprint look draws its own building, and without the plate there is nothing for the model to stand on.
	const TSoftObjectPtr<UStaticMesh>* PieceMeshAsset = IsValid(M_HexPlateMaterial)
		                                                    ? M_BuildingPieceMeshes.Find(M_Building)
		                                                    : nullptr;
	UStaticMesh* PieceMesh = PieceMeshAsset != nullptr ? PieceMeshAsset->LoadSynchronous() : nullptr;
	bM_ShowsBuildingPiece = IsValid(PieceMesh);
	M_BuildingPieceMesh->SetStaticMesh(PieceMesh);
	M_BuildingPieceMesh->SetVisibility(bM_ShowsBuildingPiece);
}

float AWorldBuildNode::GetHexPlateSignIndex() const
{
	using namespace WorldBuildNodeVisualConstants;
	if (M_NodeState == EWorldBuildNodeState::Locked)
	{
		return HexPlateSignLock;
	}

	if (bM_ShowsBuildingPiece)
	{
		return HexPlateSignNone;
	}

	switch (M_Building)
	{
	case EWorldCampaignBuilding::WatchPost:
		return HexPlateSignWatchPost;
	case EWorldCampaignBuilding::SupplyDepot:
		return HexPlateSignSupplyDepot;
	case EWorldCampaignBuilding::Airfield:
		return HexPlateSignAirfield;
	case EWorldCampaignBuilding::Battery:
		return HexPlateSignBattery;
	case EWorldCampaignBuilding::None:
	default:
		return HexPlateSignPlus;
	}
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

bool AWorldBuildNode::GetIsValidBuildingPieceMesh() const
{
	if (IsValid(M_BuildingPieceMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_BuildingPieceMesh"),
		TEXT("GetIsValidBuildingPieceMesh"),
		this
	);
	return false;
}

bool AWorldBuildNode::GetIsValidHexPlateMesh() const
{
	if (IsValid(M_HexPlateMesh))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_HexPlateMesh"),
		TEXT("GetIsValidHexPlateMesh"),
		this
	);
	return false;
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
