// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "RTS_Survival/SurvivalMode/MapContent/SurvivalMapSite.h"

#include "PCGSurvivalSitePicker.generated.h"

/** @brief How an actor picked for a site is turned. */
UENUM(BlueprintType)
enum class ESurvivalSiteYawMode : uint8
{
	Random,
	// Away from the map centre: gun pits look at the map edge the waves come from.
	FaceOutward,
	// The yaw of the site actor.
	KeepSiteYaw
};

/** @brief One actor a site may hold; the weights decide between the entries of a pool. */
USTRUCT(BlueprintType)
struct FSurvivalSiteActorEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site Actor")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site Actor", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// Added to the ground height: seats actors whose pivot is not at their lowest visible point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site Actor")
	float GroundOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site Actor")
	bool bAlignToGround = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site Actor")
	float YawOffsetDegrees = 0.f;

	// Long stiff props bridge a dip and hang in the air: members of this kind only take even ground. 0 = no check.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site Actor", meta = (ClampMin = "0.0"))
	float EvenGroundRadius = 0.f;

	// How far the ground under the centre may lie from the straight line across EvenGroundRadius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site Actor", meta = (ClampMin = "0.0"))
	float MaxGroundBend = 6.f;

	// Most of this actor per generation of the node; 0 = no limit. The rare guns stay rare whatever the seed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Site Actor", meta = (ClampMin = "0"))
	int32 MaxPerGeneration = 0;
};

/**
 * @brief Picks a seeded, evenly spread part of the map's ASurvivalMapSite actors of one category and outputs one point
 * per actor to spawn, with the class in the ActorClass attribute. Feed it to a Spawn Actor node set to spawn by that
 * attribute. Sites are vetted when the map is built, so every seed stands on good ground.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSurvivalSitePickerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SurvivalSitePicker")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sites")
	ESurvivalMapSiteCategory SiteCategory = ESurvivalMapSiteCategory::Capture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sites", meta = (ClampMin = "1"))
	int32 MinSites = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sites", meta = (ClampMin = "1"))
	int32 MaxSites = 6;

	// Picks go round these wedges of the compass, so every side of the map gets its share.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sites", meta = (ClampMin = "1"))
	int32 CompassSectors = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sites", meta = (ClampMin = "0.0"))
	float MinSiteSpacing = 3000.f;

	// Only sites in this ring round the map centre count: one node for the fields near the base, one for the far ones.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sites", meta = (ClampMin = "0.0"))
	float MinDistanceFromCentre = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sites", meta = (ClampMin = "0.0"))
	float MaxDistanceFromCentre = 100000.f;

	// One of these stands at the centre of every picked site; empty leaves the centre free.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors")
	TArray<FSurvivalSiteActorEntry> CentrePool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors")
	ESurvivalSiteYawMode CentreYawMode = ESurvivalSiteYawMode::Random;

	// Scattered round the centre inside the site radius, randomly turned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors")
	TArray<FSurvivalSiteActorEntry> MemberPool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors", meta = (ClampMin = "0"))
	int32 MinMembers = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors", meta = (ClampMin = "0"))
	int32 MaxMembers = 0;

	// All members of a site use one class: a herd of one animal instead of a mix.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors")
	bool bMembersShareOneClass = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors", meta = (ClampMin = "0.0"))
	float MemberSpacing = 300.f;

	// Members keep this far from the site centre (the footprint of the centre actor).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actors", meta = (ClampMin = "0.0"))
	float CentreClearance = 0.f;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/** @brief Reads the level's sites on the game thread; the output depends on the level, so it is never cached. */
class FPCGSurvivalSitePickerElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
