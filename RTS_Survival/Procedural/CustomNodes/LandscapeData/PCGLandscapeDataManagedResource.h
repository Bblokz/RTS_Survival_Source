// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGManagedResource.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataTypes.h"

#include "PCGLandscapeDataManagedResource.generated.h"

class ALandscapeDataManager;
class UPCGComponent;
class UPCGSpatialData;

/**
 * @brief Keeps one PCG node-stack contribution registered until that generated resource is hard-released.
 * Soft release deliberately preserves the previous texture contribution while PCG regenerates its replacement.
 */
UCLASS()
class RTS_SURVIVAL_API UPCGLandscapeDataManagedResource final : public UPCGManagedResource
{
	GENERATED_BODY()

public:
	virtual void PostEditImport() override;
	virtual void PostApplyToComponent() override;

	/**
	 * @brief Finds the contribution owned by this exact node instance so subgraphs and loops stay isolated.
	 * @param SourceComponent Component whose managed resources are searched.
	 * @param SettingsUID Stable UID of the node settings.
	 * @param StackCRC CRC identifying the current graph-stack invocation.
	 * @return A reusable resource, or null when this node-stack pair has not contributed yet.
	 */
	static UPCGLandscapeDataManagedResource* FindReusableResource(
		UPCGComponent& SourceComponent,
		uint64 SettingsUID,
		const FPCGCrc& StackCRC);

	/** Initializes the stable node-stack identity before the resource is registered with PCG. */
	void InitializeIdentity(uint64 SettingsUID, const FPCGCrc& StackCRC);

	/**
	 * @brief Replaces copied point stamps through the manager before committing this resource's new handle.
	 * @param LandscapeDataManager Manager receiving the complete replacement contribution.
	 * @param Channel Logical texture channel to paint.
	 * @param PointStamps Value-only copies that never retain graph-owned PCG objects.
	 * @param SourceComponent Component used by the manager to defer texture publication safely.
	 * @return True when the manager accepted the complete replacement.
	 */
	bool ReplacePointContribution(
		ALandscapeDataManager& LandscapeDataManager,
		ERTSLandscapeDataChannel Channel,
		const TArray<FRTSLandscapeDataPointStamp>& PointStamps,
		UPCGComponent* SourceComponent);

	/**
	 * @brief Lets the manager synchronously rasterize volumes while graph-owned data is still alive.
	 * @param LandscapeDataManager Manager receiving the complete replacement contribution.
	 * @param Channel Logical texture channel to paint.
	 * @param SpatialData Borrowed volume data valid only for the duration of this call.
	 * @param SourceComponent Component used by the manager to defer texture publication safely.
	 * @return True when the manager sampled and accepted the complete replacement.
	 */
	bool ReplaceVolumeContribution(
		ALandscapeDataManager& LandscapeDataManager,
		ERTSLandscapeDataChannel Channel,
		const TArray<const UPCGSpatialData*>& SpatialData,
		UPCGComponent* SourceComponent);

	//~Begin UPCGManagedResource interface
	virtual bool Release(
		bool bHardRelease,
		TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	//~End UPCGManagedResource interface

private:
	bool MatchesIdentity(uint64 SettingsUID, const FPCGCrc& StackCRC) const;
	FGuid GetReplacementContributionId(const ALandscapeDataManager& LandscapeDataManager) const;

	/** Commits ownership only after the manager has accepted a complete replacement. */
	void CommitSuccessfulReplacement(
		ALandscapeDataManager& LandscapeDataManager,
		const FGuid& ContributionId);

	void RemoveRegisteredContribution();

	UPROPERTY(Transient)
	TWeakObjectPtr<ALandscapeDataManager> M_LandscapeDataManager;

	UPROPERTY(Transient)
	FGuid M_ContributionId;

	UPROPERTY(Transient)
	FPCGCrc M_StackCRC;

	UPROPERTY(Transient)
	uint64 M_SettingsUID = MAX_uint64;
};
