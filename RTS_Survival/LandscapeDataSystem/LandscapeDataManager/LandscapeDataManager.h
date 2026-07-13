// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataTypes.h"
#include "UObject/ObjectKey.h"

#include "LandscapeDataManager.generated.h"

class ALandscape;
class ALandscapeProxy;
class UPCGComponent;
class UPCGSpatialData;
class ULevel;
class ULandscapeComponent;
class UTexture2D;

/** @brief Cropped byte coverage retained after a PCG spatial input has been sampled. */
struct FRTSLandscapeDataRasterContribution
{
	FIntRect PixelBounds;
	TArray<uint8> Coverage;
};

/** @brief One node-stack contribution and the PCG component transaction that owns it. */
struct FRTSLandscapeDataManagerContribution
{
	FObjectKey SourceComponentKey;
	ERTSLandscapeDataChannel Channel = ERTSLandscapeDataChannel::Scorched;
	TArray<FRTSLandscapeDataPointStamp> PointStamps;
	TArray<FRTSLandscapeDataRasterContribution> Rasters;
};

/**
 * @brief Place one in a level to turn PCG footprints and volumes into a Landscape material mask.
 * It publishes immutable texture snapshots so rendering never reads storage while it is being written.
 * @note The Landscape material must expose the texture and mapping parameters configured on this actor.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API ALandscapeDataManager : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeDataManager();

	virtual void OnConstruction(const FTransform& Transform) override;

	/**
	 * @brief Finds the only manager in the supplied world so PCG never chooses one nondeterministically.
	 * @param WorldContextObject Object used to resolve the world.
	 * @param OutFailureReason Explains why no unique manager could be returned.
	 * @return The unique valid manager, or null.
	 */
	static ALandscapeDataManager* FindUniqueManager(
		const UObject* WorldContextObject,
		FString& OutFailureReason);

	/**
	 * @brief Atomically replaces one PCG node's point contribution without exposing partial results.
	 * @param InOutContributionId Stable handle owned by the node's managed PCG resource.
	 * @param Channel Texture channel that receives point density.
	 * @param PointStamps Value-only oriented footprints copied from PCG.
	 * @param SourceComponent Component whose completion triggers publication.
	 * @return True when the contribution was accepted.
	 */
	bool ReplacePointContribution(
		FGuid& InOutContributionId,
		ERTSLandscapeDataChannel Channel,
		const TArray<FRTSLandscapeDataPointStamp>& PointStamps,
		UPCGComponent* SourceComponent);

	/**
	 * @brief Samples graph-owned volumes immediately so no PCG UObject survives beyond execution.
	 * @param InOutContributionId Stable handle owned by the node's managed PCG resource.
	 * @param Channel Texture channel that receives volume density.
	 * @param SpatialData Volume inputs that remain valid only for this synchronous call.
	 * @param SourceComponent Component whose completion triggers publication.
	 * @return True when the contribution was accepted.
	 */
	bool ReplaceVolumeContribution(
		FGuid& InOutContributionId,
		ERTSLandscapeDataChannel Channel,
		const TArray<const UPCGSpatialData*>& SpatialData,
		UPCGComponent* SourceComponent);

	/**
	 * @brief Removes only the contribution still owned by the releasing PCG component instance.
	 * @param ContributionId Stable managed-resource handle to remove.
	 * @param ExpectedSourceComponentKey Guards transfers against cleanup from an obsolete component copy.
	 */
	void RemoveContribution(
		const FGuid& ContributionId,
		const FObjectKey& ExpectedSourceComponentKey);

	/**
	 * @brief Preserves the published contribution when Unreal reconstructs its source component.
	 * @param ContributionId Stable managed-resource handle to transfer.
	 * @param NewSourceComponent Reconstructed component that now owns the handle.
	 */
	void TransferContributionSource(
		const FGuid& ContributionId,
		UPCGComponent* NewSourceComponent);

	/** Rebuilds and atomically publishes the current contribution set. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Landscape Data")
	void RebuildLandscapeData();

	/** Refreshes the serialized editor bounds used by shipping builds, then publishes a blank texture. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Landscape Data")
	void RefreshBoundsFromLandscape();

	/** Clears registered data without discarding the Landscape material binding. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Landscape Data")
	void ClearLandscapeData();

	/** @return The immutable texture currently published to the Landscape material. */
	UFUNCTION(BlueprintPure, Category = "Landscape Data")
	UTexture2D* GetLandscapeDataTexture() const
	{
		return M_LandscapeDataTexture;
	}

	/** @return Mapping used by both CPU rasterization and the Landscape material. */
	UFUNCTION(BlueprintPure, Category = "Landscape Data")
	FRTSLandscapeDataTextureMapping GetTextureMapping() const
	{
		return M_TextureMapping;
	}

	/**
	 * @brief Provides a CPU-side query without a render-target readback.
	 * @param WorldPosition Position to sample in world space.
	 * @param Channel Logical data channel to read.
	 * @return Normalized mask value, or zero outside the mapped Landscape.
	 */
	UFUNCTION(BlueprintPure, Category = "Landscape Data")
	float GetLandscapeDataAtWorldPosition(
		const FVector& WorldPosition,
		ERTSLandscapeDataChannel Channel) const;

	/** @return Whether the manager has valid Landscape bounds, texture data, and material bindings. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Landscape Data")
	bool ValidateLandscapeDataSetup();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif

private:
	bool EnsureInitialized();

	/**
	 * @brief Separates graph execution from publication so cancellation cannot leak partial masks.
	 * @param ContributionId Stable managed-resource handle being replaced.
	 * @param Contribution Complete value-only replacement to stage or commit.
	 * @param SourceComponent Graph transaction owner, or null for an immediate native write.
	 * @return True when ownership and data were accepted.
	 */
	bool StoreContribution(
		const FGuid& ContributionId,
		FRTSLandscapeDataManagerContribution&& Contribution,
		UPCGComponent* SourceComponent);
	bool CommitStagedContributions(const FObjectKey& SourceComponentKey);
	bool ApplyPendingRemovals(const FObjectKey& SourceComponentKey);
	void DiscardStagedContributions(const FObjectKey& SourceComponentKey);
	void DiscardPendingRemovals(const FObjectKey& SourceComponentKey);
	bool GetIsUniqueManager(const bool bReportError) const;
	bool ResolveLandscape(bool bReportError);
	bool BuildTextureMapping();

	/**
	 * @brief Keeps distant volumes cropped independently to avoid sampling their empty union.
	 * @param SpatialData Graph-owned inputs borrowed only during this call.
	 * @param OutRasters Value-only cropped coverage generated for valid inputs.
	 */
	void BuildVolumeRasters(
		const TArray<const UPCGSpatialData*>& SpatialData,
		TArray<FRTSLandscapeDataRasterContribution>& OutRasters) const;

	/**
	 * @brief Samples one volume now so no graph-owned UObject is retained by the manager.
	 * @param SpatialData Volume whose density defines coverage.
	 * @param OutRaster Cropped Landscape-aligned coverage.
	 * @return True when the volume overlaps the mapped Landscape.
	 */
	bool BuildVolumeRaster(
		const UPCGSpatialData& SpatialData,
		FRTSLandscapeDataRasterContribution& OutRaster) const;

	/**
	 * @brief Extracts a row to keep synchronous UObject sampling shallow and auditable.
	 * @param SpatialData Volume sampled at its bounds-center Z plane.
	 * @param PixelY Destination texture row.
	 * @param InOutRaster Cropped raster receiving normalized density.
	 */
	void RasterizeVolumeRow(
		const UPCGSpatialData& SpatialData,
		int32 PixelY,
		FRTSLandscapeDataRasterContribution& InOutRaster) const;
	bool GetPixelRectForWorldBounds(const FBox& WorldBounds, FIntRect& OutPixelBounds) const;
	bool GetTexturePixelFromWorldPosition(const FVector& WorldPosition, FIntPoint& OutPixel) const;
	void TrackPCGComponent(UPCGComponent* SourceComponent);
	void StartInvalidSourceCleanupTimer();
	void StopInvalidSourceCleanupTimer();
	void HandleInvalidSourceCleanupTimer();
	void GatherInvalidSourceComponentKeys(TSet<FObjectKey>& OutSourceComponentKeys) const;
	void PruneInvalidBoundPCGComponents();
	void TrackPCGOwner(AActor& SourceOwner);
	void UnbindPCGComponents();
	void UnbindPCGOwners();
	bool RemoveContributionsForSource(const FObjectKey& SourceComponentKey);
	void BindPublishedTextureToLandscape();
	void BindPublishedTextureToLandscapeProxy(ALandscapeProxy& LandscapeProxy) const;
	bool GetAreLandscapeDynamicMaterialInstancesEnabled(bool bReportError) const;
	bool GetDoesLandscapeProxyHaveRequiredMaterialSetup(const ALandscapeProxy& LandscapeProxy) const;
	bool GetDoesLandscapeComponentHaveRequiredMaterialSetup(
		const ULandscapeComponent& LandscapeComponent) const;
	bool GetIsPublishedTextureBoundToLandscape() const;
	bool GetDoesLandscapeProxyUsePublishedMaterialValues(
		const ALandscapeProxy& LandscapeProxy) const;
	bool GetDoesLandscapeComponentUsePublishedMaterialValues(
		const ULandscapeComponent& LandscapeComponent) const;
	void BindLevelStreaming();
	void EndPlay_UnbindLevelStreaming();
	void EndPlay_ClearPublishedTexture(EEndPlayReason::Type EndPlayReason);
	void RasterizeContributions(TArray<FColor>& OutPixels) const;

	/**
	 * @brief Projects the transformed local point bounds without losing their orientation.
	 * @param PointStamp Value-only PCG point footprint.
	 * @param Channel Logical texture channel receiving coverage.
	 * @param InOutPixels Full-resolution CPU mask being composed.
	 */
	void RasterizePointStamp(
		const FRTSLandscapeDataPointStamp& PointStamp,
		ERTSLandscapeDataChannel Channel,
		TArray<FColor>& InOutPixels) const;

	/**
	 * @brief Keeps each oriented-footprint scan row small enough for safe bounds checking.
	 * @param PointStamp Value-only PCG point footprint.
	 * @param Channel Logical texture channel receiving coverage.
	 * @param PixelBounds Clipped axis-aligned scan rectangle.
	 * @param PixelY Destination texture row.
	 * @param InOutPixels Full-resolution CPU mask being composed.
	 */
	void RasterizePointStampRow(
		const FRTSLandscapeDataPointStamp& PointStamp,
		ERTSLandscapeDataChannel Channel,
		const FIntRect& PixelBounds,
		int32 PixelY,
		TArray<FColor>& InOutPixels) const;

	/**
	 * @brief Composites cropped coverage by maximum so overlapping writers remain independent.
	 * @param Contribution Owner and destination-channel metadata.
	 * @param Raster Cropped byte coverage to composite.
	 * @param InOutPixels Full-resolution CPU mask being composed.
	 */
	void ApplyRasterContribution(
		const FRTSLandscapeDataManagerContribution& Contribution,
		const FRTSLandscapeDataRasterContribution& Raster,
		TArray<FColor>& InOutPixels) const;

	/**
	 * @brief Isolates index validation from the outer raster row loop.
	 * @param Contribution Owner and destination-channel metadata.
	 * @param Raster Cropped byte coverage to composite.
	 * @param LocalY Row inside the cropped raster.
	 * @param InOutPixels Full-resolution CPU mask being composed.
	 */
	void ApplyRasterContributionRow(
		const FRTSLandscapeDataManagerContribution& Contribution,
		const FRTSLandscapeDataRasterContribution& Raster,
		int32 LocalY,
		TArray<FColor>& InOutPixels) const;
	/**
	 * @brief Builds every mip before the new immutable texture becomes visible to rendering.
	 * @param BasePixels Complete semantic RGBA base image.
	 * @param Resolution Dimensions matching BasePixels.
	 * @return An unpublished transient texture with a complete mip chain, or null.
	 */
	UTexture2D* CreatePublishedTexture(
		const TArray<FColor>& BasePixels,
		const FIntPoint& Resolution);
	UTexture2D* CreateBlankPublishedTexture();
	bool PublishInitialBlankPixels(TArray<FColor>&& BlankPixels);
	bool PublishPixels(TArray<FColor>&& Pixels);
	FVector GetWorldPositionForPixel(const FIntPoint& Pixel, double WorldZ) const;
	static void ApplyChannelMaximum(FColor& Pixel, ERTSLandscapeDataChannel Channel, uint8 Value);
	static uint8 GetChannelValue(const FColor& Pixel, ERTSLandscapeDataChannel Channel);
	bool GetIsValidLandscape() const;
	bool GetIsValidLandscapeDataTexture() const;

#if WITH_EDITOR
	bool Editor_RefreshCachedCompleteLandscapeBounds();
#endif

	UFUNCTION()
	void HandlePCGGraphGenerated(UPCGComponent* PCGComponent);

	UFUNCTION()
	void HandlePCGGraphCleaned(UPCGComponent* PCGComponent);

	UFUNCTION()
	void HandlePCGGraphCancelled(UPCGComponent* PCGComponent);

	/**
	 * @brief Removes masks before a PCG owner's managed resources can disappear without Release.
	 * @param Actor Source owner ending play.
	 * @param EndPlayReason Determines whether republishing is still useful for this world.
	 */
	UFUNCTION()
	void HandleTrackedSourceActorEndPlay(
		AActor* Actor,
		EEndPlayReason::Type EndPlayReason);

	/**
	 * @brief Rebinds immutable data to Landscape proxies streamed after initial publication.
	 * @param Level Newly visible level whose actors are ready to bind.
	 * @param World World receiving the level.
	 */
	void HandleLevelAddedToWorld(ULevel* Level, UWorld* World);

	/** Main Landscape whose loaded proxies receive the runtime material parameters. */
	UPROPERTY(EditInstanceOnly, Category = "Landscape Data|Landscape", meta = (DisplayName = "Landscape"))
	TWeakObjectPtr<ALandscape> M_Landscape;

	// Editor-computed complete bounds make World Partition coverage available in shipping builds.
	UPROPERTY(VisibleInstanceOnly, Category = "Landscape Data|Landscape", meta = (DisplayName = "Cached Complete Landscape Bounds"))
	FBox M_CachedCompleteLandscapeBounds;

	UPROPERTY(EditAnywhere, Category = "Landscape Data|Texture", meta = (ClampMin = "1.0", Units = "cm", DisplayName = "Desired World Units Per Texel"))
	float M_DesiredWorldUnitsPerTexel = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Landscape Data|Texture", meta = (ClampMin = "32", ClampMax = "4096", DisplayName = "Maximum Texture Resolution"))
	int32 M_MaximumTextureResolution = 2048;

	UPROPERTY(EditAnywhere, Category = "Landscape Data|Material", meta = (DisplayName = "Texture Parameter Name"))
	FName M_TextureParameterName = TEXT("LandscapeDataTexture");

	UPROPERTY(EditAnywhere, Category = "Landscape Data|Material", meta = (DisplayName = "World Mapping Parameter Name"))
	FName M_WorldMappingParameterName = TEXT("LandscapeDataWorldMinAndInvSize");

	UPROPERTY(EditAnywhere, Category = "Landscape Data|Material", meta = (DisplayName = "Texture Metrics Parameter Name"))
	FName M_TextureMetricsParameterName = TEXT("LandscapeDataTextureMetrics");

	UPROPERTY(Transient, VisibleAnywhere, Category = "Landscape Data|Runtime")
	TObjectPtr<UTexture2D> M_LandscapeDataTexture;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Landscape Data|Runtime")
	FRTSLandscapeDataTextureMapping M_TextureMapping;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UPCGComponent>> M_BoundPCGComponents;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> M_BoundPCGOwners;

	// Only this committed set is ever rasterized into a published texture snapshot.
	TMap<FGuid, FRTSLandscapeDataManagerContribution> M_Contributions;

	// Per-source staging keeps concurrent or cancelled PCG graphs out of the visible mask.
	TMap<FGuid, FRTSLandscapeDataManagerContribution> M_StagedContributions;

	// Hard-released resources are applied with their source's successful graph transaction.
	TMap<FGuid, FObjectKey> M_PendingRemovals;

	// CPU authority supports recomposition and gameplay queries without GPU readback.
	TArray<FColor> M_CombinedPixels;
	FDelegateHandle M_LevelAddedToWorldHandle;
	FTimerHandle M_InvalidSourceCleanupTimerHandle;
	bool bM_IsInitialized = false;
	bool bM_IsDirty = false;
	bool bM_IsRebuilding = false;
};
