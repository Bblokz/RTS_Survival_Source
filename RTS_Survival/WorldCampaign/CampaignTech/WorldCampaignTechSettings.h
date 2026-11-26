// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "WorldCampaignTechSettings.generated.h"

class UTechnologyEffect;
class UTexture2D;

/**
 * @brief Branches of the world-map tech tree. They follow the buildings that train the units a tech improves.
 * Append only: saved in config by value.
 */
UENUM(BlueprintType)
enum class EWorldCampaignTechBranch : uint8
{
	None,
	Infantry,
	LightArmour,
	HeavyArmour,
	// Shown as SUPPORT: its technologies are anti-air guns and utility upgrades (UI sketch sheet 13).
	Air UMETA(DisplayName = "Support"),
	Economy
};

/**
 * @brief Where one technology sits in the world-map tree and what it costs there.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignTechDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	EWorldCampaignTechBranch Branch = EWorldCampaignTechBranch::None;

	/** 0 is the root row of the branch; techs in a row are ordered by their enum value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech", meta = (ClampMin = "0"))
	int32 Row = 0;

	/** Price in world-map resources (blueprints and radixite). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	FWorldMapResources Cost;

	/** Every one of these must be researched first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	TArray<ETechnology> Prerequisites;
};

/**
 * @brief The whole world-map tech tree as plain data, so the rules in WorldCampaignTechRules can be tested without a
 * world. The constructor holds the starting layout of the 24 technologies.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldCampaignTechTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech")
	TMap<ETechnology, FWorldCampaignTechDefinition> Technologies;

	FWorldCampaignTechTable();

	const FWorldCampaignTechDefinition* FindDefinition(const ETechnology Technology) const
	{
		return Technologies.Find(Technology);
	}

private:
	void AddDefinition(ETechnology Technology, EWorldCampaignTechBranch Branch, int32 Row,
	                   const TArray<ETechnology>& Prerequisites);
};

/**
 * @brief What a node of the campaign tech wheel is. Append only: saved in config by value.
 */
UENUM(BlueprintType)
enum class EWorldTechWheelNodeKind : uint8
{
	// The HQ in the centre; owned from the start of the campaign.
	Start,
	// A small pip on the path; its bonus is not designed yet.
	Minor,
	Technology,
	// A technology on the outer ring, drawn larger.
	Rim
};

/**
 * @brief One node of the campaign tech wheel. Path neighbours say how the node is reached; RequiredTechnology is an
 * extra key from any branch. The wheel is the campaign's authority for what can be bought.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldTechWheelNode
{
	GENERATED_BODY()

	// Stable id: stored in the campaign save when bought.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	EWorldCampaignTechBranch Branch = EWorldCampaignTechBranch::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	EWorldTechWheelNodeKind Kind = EWorldTechWheelNodeKind::Minor;

	// Tech_NONE for the start and for minor nodes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	ETechnology Technology = ETechnology::Tech_NONE;

	// Layout only: 0 is the centre, every ring further out adds one.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel", meta = (ClampMin = "0"))
	int32 Ring = 0;

	// Layout only: screen angle in degrees, 0 points right and angles grow clockwise (-90 is the top).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	float AngleDegrees = 0.f;

	// Path links; every link must be listed on both nodes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	TArray<FName> Neighbours;

	// The cross-branch key: must be researched before this node can be bought. Tech_NONE when there is none.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	ETechnology RequiredTechnology = ETechnology::Tech_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	FWorldMapResources Cost;

	bool GetIsTechnologyNode() const
	{
		return Kind == EWorldTechWheelNodeKind::Technology || Kind == EWorldTechWheelNodeKind::Rim;
	}
};

/**
 * @brief The whole campaign tech wheel as plain data, so WorldCampaignTechWheel's rules can be tested without a world.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldTechWheel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	TArray<FWorldTechWheelNode> Nodes;

	/** @return nullptr when no node has this id. */
	const FWorldTechWheelNode* FindNode(const FName NodeId) const;

	/** @return nullptr when the technology is not on the wheel. */
	const FWorldTechWheelNode* FindNodeByTechnology(const ETechnology Technology) const;

	/** @return NAME_None when the wheel has no start node. */
	FName GetStartNodeId() const;

	int32 GetTechnologyNodeCount() const;
};

/**
 * @brief How a technology is shown on the tech wheel. The in-mission texts and icons live in Blueprint data tables,
 * so the wheel keeps its own copy here.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldTechWheelTechPresentation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	FText Description;

	// Hexagon badge from Textures/GameUI/TechTree/GerTech.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * @brief Names, descriptions and icons of the 24 technologies for the tech wheel; the constructor holds the defaults.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldTechWheelPresentationTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tech Wheel")
	TMap<ETechnology, FWorldTechWheelTechPresentation> Technologies;

	FWorldTechWheelPresentationTable();

	const FWorldTechWheelTechPresentation* FindPresentation(const ETechnology Technology) const
	{
		return Technologies.Find(Technology);
	}

private:
	void AddPresentation(ETechnology Technology, const FText& DisplayName, const FText& Description,
	                     const TCHAR* IconAssetName);
};

/**
 * @brief Project settings page for the world-map tech tree (Config/DefaultGame.ini). Read through GetTechTable.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign Tech Tree"))
class RTS_SURVIVAL_API UWorldCampaignTechSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldCampaignTechSettings();

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "World Campaign Tech Tree")
	FWorldCampaignTechTable TechTable;

	static const FWorldCampaignTechTable& GetTechTable();

	// On: the campaign uses the wheel generated in code (WorldCampaignTechWheel::MakeDefaultWheel), laid out as UI
	// sketch sheet 13. Off: CustomWheel is used, or the generated wheel while CustomWheel has no nodes.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tech Wheel")
	bool bUseGeneratedDefaultWheel = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tech Wheel",
		meta = (EditCondition = "!bUseGeneratedDefaultWheel"))
	FWorldTechWheel CustomWheel;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tech Wheel")
	FWorldTechWheelPresentationTable WheelPresentation;

	// The effect a mission applies for each technology; the wheel reads the affected units and weapon changes from
	// it. In a mission the player controller Blueprint holds the same list. A technology without an entry shows
	// its description only.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tech Wheel")
	TMap<ETechnology, TSoftClassPtr<UTechnologyEffect>> WheelEffectClasses;

	// The mission's unit spawner Blueprint (an ARTSAsyncSpawner): the only table from a unit type to its class.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Tech Wheel")
	TSoftClassPtr<AActor> UnitClassSpawnerClass;

	/** The wheel the campaign uses; see bUseGeneratedDefaultWheel. */
	static const FWorldTechWheel& GetWheel();

	static const FWorldTechWheelPresentationTable& GetWheelPresentation();
};
