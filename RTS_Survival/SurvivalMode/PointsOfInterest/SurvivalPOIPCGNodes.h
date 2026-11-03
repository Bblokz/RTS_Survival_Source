// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGContext.h"
#include "PCGSettings.h"

#include "SurvivalPOIPCGNodes.generated.h"

class UBoxComponent;
class UPCGComponent;
class UPCGGraphInterface;
class USurvivalPointsOfInterestSubsystem;

/**
 * @brief The actor whose PCG component runs the points-of-interest graph once per match. The survival POI subsystem
 * spawns it over the whole landscape and hands it the graph; the graph's nodes ask the subsystem for the plan.
 * @note Spawned at runtime only; never placed in a level.
 */
UCLASS(NotPlaceable, Transient)
class RTS_SURVIVAL_API ASurvivalPOIPCGVolume : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalPOIPCGVolume();

	/**
	 * @brief Starts the graph.
	 * @param HalfExtent Half size of the volume (the landscape's), so the component has bounds to generate in.
	 */
	void StartGeneration(USurvivalPointsOfInterestSubsystem* Subsystem, UPCGGraphInterface* Graph, int32 Seed,
	                     const FVector& HalfExtent);

	USurvivalPointsOfInterestSubsystem* GetSubsystem() const { return M_Subsystem.Get(); }

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> M_Bounds = nullptr;

	UPROPERTY()
	TObjectPtr<UPCGComponent> M_PCGComponent = nullptr;

	TWeakObjectPtr<USurvivalPointsOfInterestSubsystem> M_Subsystem;
};

/**
 * @brief First node: finds this match's sites on the real terrain (over several frames) and outputs one point per
 * site: attributes SiteIndex, POIType, Landmark, Sector, OwnerStart, SubLayout, Radius.
 * @note Does nothing outside a graph run by ASurvivalPOIPCGVolume.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSurvivalPOISitesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SurvivalPOISites")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/**
 * @brief Lays out the composition of every site point it receives and outputs one point per object: attributes
 * AssetKey, Role, SiteIndex, Sink. A site filtered out before this node gets no composition.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSurvivalPOIComposeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SurvivalPOICompose")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/**
 * @brief Places herds and flocks (the oasis bank, over the site points it receives, over the sectors) and outputs one
 * point per group: attributes Entry, Count, SiteIndex, FlightHeight, SemiX, SemiY, GroupSeed.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSurvivalWildlifeSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SurvivalWildlife")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/**
 * @brief Last node: spawns exactly the objects and groups whose points reach it, in batches over frames, and finishes
 * when everything stands.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSurvivalPOISpawnSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SurvivalPOISpawn")); }
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

/** @brief Runs the site search a slice per call until the sites are chosen. */
class FPCGSurvivalPOISitesElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

class FPCGSurvivalPOIComposeElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

class FPCGSurvivalWildlifeElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

/** @brief Asleep while the subsystem loads and spawns in batches. */
class FPCGSurvivalPOISpawnElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
