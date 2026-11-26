// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeTypes.h"
#include "WorldBuildNode.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FWorldBuildNodeDefinition;

/**
 * @brief A place inside a country where the player can put up one building. Hand-place it in the campaign level
 * (set the id and country) or let UWorldBuildNodeManager spawn it from the BuildNodes config; the manager drives its
 * state. A hex plate drawn on a flat plane shows the state (lock, plus or the building's sign); the plain cylinder
 * is the fallback when the plate material is missing, and stays as the click target either way.
 * A building with a model in Building Piece Meshes stands on the plate as that model, without the sign.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldBuildNode : public AActor
{
	GENERATED_BODY()

public:
	AWorldBuildNode();

	/** Copies id, country and unlock order from config; used for spawned nodes. */
	void InitializeFromDefinition(const FWorldBuildNodeDefinition& BuildNodeDefinition);

	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Node")
	FName GetBuildNodeId() const { return M_BuildNodeId; }

	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Node")
	FName GetCountryId() const { return M_CountryId; }

	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Node")
	int32 GetUnlockOrder() const { return M_UnlockOrder; }

	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Node")
	EWorldBuildNodeState GetNodeState() const { return M_NodeState; }

	UFUNCTION(BlueprintPure, Category = "World Campaign|Build Node")
	EWorldCampaignBuilding GetBuilding() const { return M_Building; }

	/** Only moves forward; a lower state is ignored. */
	void SetNodeState(EWorldBuildNodeState NewState);

	/** Stores the building and marks the node Built; ignored when a building already stands here. */
	void SetBuilding(EWorldCampaignBuilding NewBuilding);

	/**
	 * @brief Where something leaves the building's model, e.g. the shells of a battery.
	 * @param SocketName Socket on the building piece mesh.
	 * @param OutLocation World location of the socket.
	 * @return false when no model is shown or it has no such socket.
	 */
	bool TryGetBuildingPieceSocketLocation(FName SocketName, FVector& OutLocation) const;

	/** Brightens the plate while the cursor is over the node. */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Build Node")
	void SetIsHovered(bool bIsHovered);

protected:
	virtual void BeginPlay() override;

	/** Called on every state change and once at BeginPlay so the Blueprint look always matches. */
	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Build Node")
	void BP_OnNodeStateChanged(EWorldBuildNodeState NewState, EWorldCampaignBuilding Building);

	/** Called once when a building is put up, also when it is restored from a save. */
	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Build Node")
	void BP_OnBuildingBuilt(EWorldCampaignBuilding Building);

private:
	void UpdateDefaultVisual() const;
	FLinearColor GetDefaultVisualColor() const;
	bool GetIsValidDefaultVisualMesh() const;
	bool GetIsValidHexPlateMesh() const;

	/** Puts the hex plate material on the plate; without the material the cylinder keeps showing the state. */
	void BeginPlay_InitHexPlate();
	void UpdateHexPlate() const;
	float GetHexPlateSignIndex() const;

	/** Shows the model of the building that stands here, or hides the piece when it has none. */
	void UpdateBuildingPiece();
	bool GetIsValidBuildingPieceMesh() const;

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|Build Node")
	TObjectPtr<USceneComponent> M_RootSceneComponent;

	// Engine cylinder tinted by state; hide it when a Blueprint draws its own look.
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|Build Node")
	TObjectPtr<UStaticMeshComponent> M_DefaultVisualMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> M_DefaultVisualMaterial;

	// Flat plane just above the ground that the hex plate material draws on.
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|Build Node")
	TObjectPtr<UStaticMeshComponent> M_HexPlateMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> M_HexPlateMaterial;

	// Small model of the building, standing on the plate; empty until a building with a model is put up.
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|Build Node")
	TObjectPtr<UStaticMeshComponent> M_BuildingPieceMesh;

	// True while a model shows the building, so the plate leaves its sign out.
	bool bM_ShowsBuildingPiece = false;

	/** Model per building, placed at scale 1 on the plate. A building without one keeps its sign. */
	UPROPERTY(EditAnywhere, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true", DisplayName = "Building Piece Meshes"))
	TMap<EWorldCampaignBuilding, TSoftObjectPtr<UStaticMesh>> M_BuildingPieceMeshes;

	/** Built by Source/Tools/WorldCampaign/create_build_site_materials.py. */
	UPROPERTY(EditAnywhere, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true", DisplayName = "Hex Plate Material"))
	TSoftObjectPtr<UMaterialInterface> M_HexPlateMaterialAsset;

	/** Width of the plate across its flat sides, in map units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true", ClampMin = "10.0", ClampMax = "200.0", DisplayName = "Hex Plate Width"))
	float M_HexPlateWidth = 57.6f;

	/** Stable id used in saves; must be unique on the map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true", DisplayName = "Build Node Id"))
	FName M_BuildNodeId = NAME_None;

	/** Id of the country this node belongs to (UWorldCountryOccupationRegulator::GetCountryIds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true", DisplayName = "Country Id"))
	FName M_CountryId = NAME_None;

	/** 0 = the first node of the country to unlock, 1 = the second, and so on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true", ClampMin = "0", DisplayName = "Unlock Order"))
	int32 M_UnlockOrder = 0;

	/** Off = the cylinder is hidden and the Blueprint events draw the node. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true", DisplayName = "Use Default Visual"))
	bool bM_UseDefaultVisual = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true"))
	EWorldBuildNodeState M_NodeState = EWorldBuildNodeState::Locked;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "World Campaign|Build Node",
		meta = (AllowPrivateAccess = "true"))
	EWorldCampaignBuilding M_Building = EWorldCampaignBuilding::None;
};
