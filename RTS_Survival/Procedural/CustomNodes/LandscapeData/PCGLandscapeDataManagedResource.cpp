// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PCGLandscapeDataManagedResource.h"

#include "RTS_Survival/LandscapeDataSystem/LandscapeDataManager/LandscapeDataManager.h"

#include "PCGComponent.h"
#include "Data/PCGSpatialData.h"

void UPCGLandscapeDataManagedResource::PostEditImport()
{
	Super::PostEditImport();
	M_LandscapeDataManager.Reset();
	M_ContributionId.Invalidate();
}

void UPCGLandscapeDataManagedResource::PostApplyToComponent()
{
	ALandscapeDataManager* LandscapeDataManager = M_LandscapeDataManager.Get();
	UPCGComponent* SourceComponent = GetTypedOuter<UPCGComponent>();
	if (LandscapeDataManager == nullptr
		|| not IsValid(SourceComponent)
		|| not M_ContributionId.IsValid())
	{
		return;
	}

	LandscapeDataManager->TransferContributionSource(M_ContributionId, SourceComponent);
}

UPCGLandscapeDataManagedResource* UPCGLandscapeDataManagedResource::FindReusableResource(
	UPCGComponent& SourceComponent,
	const uint64 SettingsUID,
	const FPCGCrc& StackCRC)
{
	UPCGLandscapeDataManagedResource* ReusableResource = nullptr;
	SourceComponent.ForEachManagedResource(
		[&ReusableResource, SettingsUID, &StackCRC](UPCGManagedResource* ManagedResource)
		{
			if (ReusableResource != nullptr)
			{
				return;
			}

			UPCGLandscapeDataManagedResource* LandscapeDataResource =
				Cast<UPCGLandscapeDataManagedResource>(ManagedResource);
			if (not IsValid(LandscapeDataResource) || not LandscapeDataResource->CanBeUsed())
			{
				return;
			}

			if (LandscapeDataResource->MatchesIdentity(SettingsUID, StackCRC))
			{
				ReusableResource = LandscapeDataResource;
			}
		});

	return ReusableResource;
}

void UPCGLandscapeDataManagedResource::InitializeIdentity(
	const uint64 SettingsUID,
	const FPCGCrc& StackCRC)
{
	M_SettingsUID = SettingsUID;
	M_StackCRC = StackCRC;
	SetCrc(StackCRC);
}

bool UPCGLandscapeDataManagedResource::ReplacePointContribution(
	ALandscapeDataManager& LandscapeDataManager,
	const ERTSLandscapeDataChannel Channel,
	const TArray<FRTSLandscapeDataPointStamp>& PointStamps,
	UPCGComponent* SourceComponent)
{
	ALandscapeDataManager* RegisteredManager = M_LandscapeDataManager.Get();
	if (RegisteredManager != nullptr && RegisteredManager != &LandscapeDataManager)
	{
		return false;
	}

	FGuid ReplacementContributionId = GetReplacementContributionId(LandscapeDataManager);
	if (not LandscapeDataManager.ReplacePointContribution(
		ReplacementContributionId,
		Channel,
		PointStamps,
		SourceComponent))
	{
		return false;
	}

	CommitSuccessfulReplacement(LandscapeDataManager, ReplacementContributionId);
	return true;
}

bool UPCGLandscapeDataManagedResource::ReplaceVolumeContribution(
	ALandscapeDataManager& LandscapeDataManager,
	const ERTSLandscapeDataChannel Channel,
	const TArray<const UPCGSpatialData*>& SpatialData,
	const FRTSLandscapeDataPaintConfiguration& PaintConfiguration,
	UPCGComponent* SourceComponent)
{
	ALandscapeDataManager* RegisteredManager = M_LandscapeDataManager.Get();
	if (RegisteredManager != nullptr && RegisteredManager != &LandscapeDataManager)
	{
		return false;
	}

	FGuid ReplacementContributionId = GetReplacementContributionId(LandscapeDataManager);
	if (not LandscapeDataManager.ReplaceVolumeContribution(
		ReplacementContributionId,
		Channel,
		SpatialData,
		PaintConfiguration,
		SourceComponent))
	{
		return false;
	}

	CommitSuccessfulReplacement(LandscapeDataManager, ReplacementContributionId);
	return true;
}

bool UPCGLandscapeDataManagedResource::Release(
	const bool bHardRelease,
	TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete)
{
	if (not Super::Release(bHardRelease, OutActorsToDelete))
	{
		return false;
	}

	RemoveRegisteredContribution();
	return true;
}

bool UPCGLandscapeDataManagedResource::MatchesIdentity(
	const uint64 SettingsUID,
	const FPCGCrc& StackCRC) const
{
	return M_SettingsUID == SettingsUID
		&& M_StackCRC.IsValid()
		&& StackCRC.IsValid()
		&& M_StackCRC == StackCRC;
}

FGuid UPCGLandscapeDataManagedResource::GetReplacementContributionId(
	const ALandscapeDataManager& LandscapeDataManager) const
{
	return M_LandscapeDataManager.Get() == &LandscapeDataManager
		? M_ContributionId
		: FGuid();
}

void UPCGLandscapeDataManagedResource::CommitSuccessfulReplacement(
	ALandscapeDataManager& LandscapeDataManager,
	const FGuid& ContributionId)
{
	M_LandscapeDataManager = &LandscapeDataManager;
	M_ContributionId = ContributionId;
	MarkAsUsed();
}

void UPCGLandscapeDataManagedResource::RemoveRegisteredContribution()
{
	ALandscapeDataManager* LandscapeDataManager = M_LandscapeDataManager.Get();
	const FObjectKey SourceComponentKey(GetTypedOuter<UPCGComponent>());
	if (LandscapeDataManager != nullptr && M_ContributionId.IsValid())
	{
		LandscapeDataManager->RemoveContribution(M_ContributionId, SourceComponentKey);
	}

	M_ContributionId.Invalidate();
	M_LandscapeDataManager.Reset();
}
