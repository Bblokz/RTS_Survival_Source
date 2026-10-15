// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GameUI/MiniMap/MapObjectFilters/RTSMapObjectFilterTypes.h"
#include "RTSMapObjectFilterSubsystem.generated.h"

class AFowManager;
class AScavengeableObject;
class UResourceComponent;
class URTSMapObjectFilterSettings;
struct FRTSExploredFogReadback;

/** Objects of one filter split by discovery. Discovery never reverts, so each object is found only once. */
USTRUCT()
struct FRTSMapObjectFilterTrack
{
	GENERATED_BODY()

	ERTSMapObjectFilterMode M_Mode = ERTSMapObjectFilterMode::Off;

	// Checked against the explored fog on each refresh while the filter is shown; moved out once discovered.
	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> M_UndiscoveredObjects;

	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> M_DiscoveredObjects;

	// Minimap UVs of the discovered objects that are still on the map; rebuilt per refresh, drawn every frame.
	TArray<FVector2D> M_DiscoveredObjectUVs;
};

/**
 * @brief Decides which radixite, metal and scavenge objects the player has discovered, for the map object
 * filters the tactical map toggles. Objects register at BeginPlay; UW_MiniMap draws the discovered ones.
 * With every filter off (or only on for a closed tactical map) nothing runs: no ticker, no GPU readback.
 * While a filter is shown, the explored fog render target is copied to the CPU asynchronously about once per
 * second and only objects not yet discovered are checked against it.
 */
UCLASS()
class RTS_SURVIVAL_API URTSMapObjectFilterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Only radixite and metal resources are tracked; other resource types are ignored. */
	void RegisterResource(UResourceComponent& Resource);
	void RegisterScavengeable(AScavengeableObject& Scavengeable);

	void SetFilterMode(const ERTSMapObjectFilter Filter, const ERTSMapObjectFilterMode NewMode);
	ERTSMapObjectFilterMode GetFilterMode(const ERTSMapObjectFilter Filter) const;

	/** TacticalMapOnly filters only do work while the tactical map is open. */
	void SetIsTacticalMapOpen(const bool bIsOpen);

	bool GetShouldDrawFilter(const ERTSMapObjectFilter Filter, const bool bIsTacticalMapView) const;
	bool GetShouldDrawAnyFilter(const bool bIsTacticalMapView) const;

	const TArray<FVector2D>& GetDiscoveredObjectUVs(const ERTSMapObjectFilter Filter) const;

	/** Review tooling (UI snapshots): how many objects of a filter are registered and how many are discovered. */
	void GetObjectCounts(const ERTSMapObjectFilter Filter, int32& OutRegistered, int32& OutDiscovered) const;

private:
	UPROPERTY()
	FRTSMapObjectFilterTrack M_Tracks[static_cast<int32>(ERTSMapObjectFilter::Count)];

	bool bM_IsTacticalMapOpen = false;

	// Only registered while a filter is shown somewhere; core ticker so it also runs while the game is paused.
	FTSTicker::FDelegateHandle M_RefreshTickerHandle;

	double M_LastReadbackRequestSeconds = -DBL_MAX;

	// Copy of the explored fog in flight on the render thread; shared so it outlives this subsystem if needed.
	TSharedPtr<FRTSExploredFogReadback, ESPMode::ThreadSafe> M_PendingReadback;

	// Last explored fog values (0..255, row major) read from the GPU; reused at once when a filter turns on.
	TArray<uint8> M_ExploredValues;
	FIntPoint M_ExploredSize = FIntPoint::ZeroValue;

	TWeakObjectPtr<AFowManager> M_FowManager;

	bool bM_HasReportedUnsupportedFogFormat = false;
	bool bM_HasReportedMissingFowManager = false;

	FRTSMapObjectFilterTrack& GetTrack(const ERTSMapObjectFilter Filter);
	const FRTSMapObjectFilterTrack& GetTrack(const ERTSMapObjectFilter Filter) const;

	bool GetIsFilterTracked(const ERTSMapObjectFilter Filter) const;
	bool GetIsAnyFilterTracked() const;

	/** Starts or stops the refresh ticker to match the shown filters; a newly shown filter refreshes at once. */
	void UpdateRefreshTicker();

	bool OnRefreshTicker(float DeltaTime);

	void RequestExploredFogReadback();
	void PollExploredFogReadback();

	void ApplyDiscoveryToTrackedFilters();

	/**
	 * @brief Moves newly discovered objects over and drops ones that left the map, then rebuilds the dots.
	 * @param Filter Decides which objects still count as on the map.
	 * @param FowManager Converts world locations to minimap UVs.
	 * @param Settings Discovery threshold and sample radius.
	 */
	void ApplyDiscovery(const ERTSMapObjectFilter Filter, const AFowManager& FowManager,
	                    const URTSMapObjectFilterSettings& Settings);

	void MoveDiscoveredObjects(FRTSMapObjectFilterTrack& Track, const ERTSMapObjectFilter Filter,
	                           const AFowManager& FowManager, const URTSMapObjectFilterSettings& Settings) const;

	void RebuildDiscoveredObjectUVs(FRTSMapObjectFilterTrack& Track, const ERTSMapObjectFilter Filter,
	                                const AFowManager& FowManager) const;

	bool GetIsUVDiscovered(const FVector2D& MiniMapUV, const URTSMapObjectFilterSettings& Settings) const;

	static bool GetIsObjectStillOnMap(const UObject& Object, const ERTSMapObjectFilter Filter);

	// Disabled scavenge objects stay tracked (they may be enabled later) but are not drawn.
	static bool GetIsObjectShown(const UObject& Object, const ERTSMapObjectFilter Filter);

	static bool GetObjectMiniMapUV(const UObject& Object, const AFowManager& FowManager, FVector2D& OutUV);

	const AFowManager* GetIsValidFowManager();
};
