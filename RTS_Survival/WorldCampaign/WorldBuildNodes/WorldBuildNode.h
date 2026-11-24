// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeTypes.h"
#include "WorldBuildNode.generated.h"

class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
struct FWorldBuildNodeDefinition;

/**
 * @brief A place inside a country where the player can put up one building. Hand-place it in the campaign level
 * (set the id and country) or let UWorldBuildNodeManager spawn it from the BuildNodes config; the manager drives its
 * state. A plain cylinder shows the state when no Blueprint look is set up.
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

	UPROPERTY(VisibleAnywhere, Category = "World Campaign|Build Node")
	TObjectPtr<USceneComponent> M_RootSceneComponent;

	// Engine cylinder tinted by state; hide it when a Blueprint draws its own look.
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|Build Node")
	TObjectPtr<UStaticMeshComponent> M_DefaultVisualMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> M_DefaultVisualMaterial;

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
