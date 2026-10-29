// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#endif
#include "RadixiteBlightSubsystem.generated.h"

class ALandscapeProxy;
class UMaterialParameterCollection;

/** One blighted area: a circle on the ground that holds one or more radixite crystals. */
struct FRadixiteBlightBlob
{
	FVector2D Centre = FVector2D::ZeroVector;
	float Radius = 0.f;
};

#if WITH_EDITOR
/**
 * Editor-only bookkeeping of URadixiteBlightSubsystem: the delegates that keep the blight live while crystals are
 * edited, and what the landscape grass was baked with.
 */
struct FRadixiteBlightEditorState
{
	FDelegateHandle MapOpenedHandle;
	FDelegateHandle ActorAddedHandle;
	FDelegateHandle ActorDeletedHandle;
	FDelegateHandle ActorMovedHandle;
	FDelegateHandle OnScreenMessagesHandle;
	FTSTicker::FDelegateHandle DeferredRefreshHandle;

	// Fingerprint of the circles now in the collection; compared with the one the landscape's grass was baked with.
	uint32 CurrentFingerprint = 0;

	// The landscape whose material decides whether a warning is needed and whose tags record the baked fingerprint.
	TWeakObjectPtr<ALandscapeProxy> Landscape;
};
#endif

/**
 * @brief Kills the landscape around radixite crystals: groups the level's radixite resource actors into a few circles
 * and writes them into MPC_RadixiteBlight, which MF_RTS_LS_RadixiteBlight reads (Docs/Environment/RadixiteBlight.md).
 * A collection rather than material instance parameters, because the landscape instances are shared by many maps.
 * Fills the collection when a game world begins play; in the editor when a map opens and whenever a radixite actor is
 * added, moved or deleted. The editor then shows "RADIXITE GRASS NEEDS TO BE REBUILT" until RebuildBlightGrass
 * (console RTS.Radixite.RebuildGrass) bakes the landscape grass with the current blight.
 */
UCLASS()
class RTS_SURVIVAL_API URadixiteBlightSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/**
	 * @brief Recomputes the blight of the world the context lives in; for scripts and the editor after crystals moved.
	 * @param WorldContextObject Any object in the world to refresh.
	 * @return The number of blight circles written, or -1 when that world has no blight subsystem.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Radixite", meta = (WorldContext = "WorldContextObject"))
	static int32 RefreshBlight(const UObject* WorldContextObject);

	/**
	 * @brief Grass maps are baked in the editor and stored in the level, so dead ground keeps or loses its grass only
	 * after a rebuild; this rebuilds them with the current blight and records it, which clears the editor warning.
	 * @param WorldContextObject Any object in the editor world to rebuild.
	 * @return False outside the editor or when that world has no blight subsystem. Save the map afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "RTS|Radixite", meta = (WorldContext = "WorldContextObject"))
	static bool RebuildBlightGrass(const UObject* WorldContextObject);

	/** @return The number of blight circles written to the collection. */
	int32 Refresh();

private:
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> M_ParameterCollection = nullptr;

	bool GetIsValidParameterCollection() const;

	// One circle per radixite crystal: its ground position and the radius of dead ground around it.
	TArray<FRadixiteBlightBlob> GatherCrystalBlobs() const;

	/**
	 * @brief The shader tests every circle per pixel, so crystals are merged pairwise, cheapest merge first, until at
	 * most the shader's circle count remain.
	 * @param Blobs One circle per crystal; merged in place.
	 */
	static void MergeToShaderLimit(TArray<FRadixiteBlightBlob>& Blobs);

	// Actor iteration order differs between editor, commandlet and game worlds; position order does not.
	static void SortByPosition(TArray<FRadixiteBlightBlob>& Blobs);

	static FRadixiteBlightBlob EnclosingBlob(const FRadixiteBlightBlob& First, const FRadixiteBlightBlob& Second);

	void WriteBlobs(const TArray<FRadixiteBlightBlob>& Blobs) const;

	// Rounded to 10 cm, so an unchanged map always gives the same value; 0 when there are no circles.
	static uint32 ComputeFingerprint(const TArray<FRadixiteBlightBlob>& Blobs);

#if WITH_EDITOR
	FRadixiteBlightEditorState M_EditorState;

	void Initialize_RegisterEditorDelegates();
	void Deinitialize_UnregisterEditorDelegates();
	void OnEditorMapOpened(const FString& Filename, bool bAsTemplate);
	void OnEditorActorChanged(AActor* Actor);

	// Actor events arrive before a deleted actor leaves the world and many times per drag: refresh once, next frame.
	void RequestDeferredRefresh();

	bool RebuildGrass();

	/**
	 * @brief Adds the red viewport warning while the landscape grows grass on blighted ground, as "lighting needs to be
	 * rebuilt" does for lighting.
	 * @param OutMessages The viewport's on-screen messages.
	 */
	void AddGrassWarning(FCoreDelegates::FSeverityMessageMap& OutMessages) const;

	// True when the landscape material shows both the blight and landscape grass, so stale grass would be visible.
	bool GetDoesLandscapeGrowBlightedGrass(const ALandscapeProxy& Landscape) const;

	ALandscapeProxy* FindLandscape() const;
	static uint32 ReadBakedFingerprint(const ALandscapeProxy& Landscape);
	void StoreBakedFingerprint(ALandscapeProxy& Landscape) const;
#endif
};
