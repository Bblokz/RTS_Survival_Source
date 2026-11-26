// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldMapFowTypes.h"
#include "UObject/ObjectKey.h"
#include "WorldFowManager.generated.h"

class AAnchorPoint;
class AConnection;
class AGeneratorWorldCampaign;
class AWorldFowCloud;
class AWorldMapObject;
class AWorldPlayerController;
class UTexture2D;
class UWorldCampaignProgressManager;
class UWorldMapFowComponent;

/**
 * @brief Spawned by the world player controller after campaign generation to calculate actor FOW states and the cloud mask.
 * Refreshes itself when an anchor is cleared; call RefreshWorldFow after changing reveal sources (watch posts).
 */
UCLASS()
class RTS_SURVIVAL_API AWorldFowManager : public AActor
{
	GENERATED_BODY()

public:
	AWorldFowManager();

	/**
	 * @brief Keeps FOW refreshes centralized after the campaign graph exists.
	 * @param WorldPlayerController Controller that owns the world campaign flow.
	 * @param WorldGenerator Generator that owns the authoritative campaign graph state.
	 */
	void InitializeWorldFow(AWorldPlayerController* WorldPlayerController, AGeneratorWorldCampaign* WorldGenerator);

	UFUNCTION(BlueprintCallable, Category = "World Campaign|FOW")
	void UpdateFowFromCurrentWorldState();

	/**
	 * @brief Re-runs the reveal rules and rebuilds the cloud mask. Reveals only grow: anchors already Visible stay
	 * Visible, cleared anchors and anchors inside a reveal source become Visible.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|FOW")
	void RefreshWorldFow();

	/**
	 * @brief Adds or replaces one extra reveal circle (matched by SourceId). Does not refresh; call RefreshWorldFow.
	 * @param RevealSource Circle to reveal; a radius of zero removes nothing and reveals nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|FOW")
	void SetRevealSource(const FWorldFowRevealSource& RevealSource);

	/** Replaces every extra reveal circle at once. Does not refresh; call RefreshWorldFow. */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|FOW")
	void SetRevealSources(const TArray<FWorldFowRevealSource>& RevealSources);

	UFUNCTION(BlueprintCallable, Category = "World Campaign|FOW")
	void RemoveRevealSource(FName SourceId);

	UFUNCTION(BlueprintPure, Category = "World Campaign|FOW")
	TArray<FWorldFowRevealSource> GetRevealSources() const;

	/**
	 * @brief Whether the fog is open at a place, read from the visible channel of the mask the cloud draws with.
	 * For things that are not fog participants themselves, such as enemy divisions.
	 * @param WorldLocation Place on the map.
	 * @return true when the place is revealed, and also when no mask or cloud exists (then nothing is hidden).
	 */
	UFUNCTION(BlueprintPure, Category = "World Campaign|FOW")
	bool GetIsWorldLocationRevealed(const FVector& WorldLocation) const;

	/**
	 * @brief Whether the cloud is drawn open at a place: revealed, or inside the opening around an unidentified
	 * mission. For what is drawn only (dim roads); game rules ask GetIsWorldLocationRevealed.
	 * @param WorldLocation Place on the map.
	 * @return true when the cloud is open there, and also when no mask or cloud exists.
	 */
	bool GetIsFogDrawnOpenAt(const FVector& WorldLocation) const;

	// Fired after every fog pass, once the states are applied and the mask is rebuilt; for what draws itself
	// differently under fog, such as the dim roads of the connection spline renderer.
	FSimpleMulticastDelegate OnWorldFowRefreshed;

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_RevealAll();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_HideAll();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllExplorableConnections();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllVisibleConnections();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllExplorableAnchors();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllExplorableEnemyObjects();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllVisibleEnemyObjects();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllExplorableMissionObjects();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllVisibleMissionObjects();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllConnectionsState();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllAnchorsState();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_AllEnemyObjectsState();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool GetIsValidWorldGenerator() const;
	bool GetIsValidWorldFowCloud() const;
	bool GetIsValidWorldCampaignProgressManager() const;
	void BindToCampaignProgress();
	void UnbindFromCampaignProgress();
	void OnAnchorCleared(const FGuid& AnchorKey);

	/**
	 * @brief Marks cleared anchors and anchors inside a reveal source Visible before the flood, so they seed it and
	 * the fog opens as the player advances.
	 */
	void ApplyPersistentRevealsToAnchors();
	bool GetIsAnchorCleared(const AAnchorPoint* AnchorPoint) const;
	bool GetIsAnchorInsideRevealSource(const AAnchorPoint* AnchorPoint) const;
	void StampRevealSourcesToMask();
	void NotifyFowRefreshed();

	/**
	 * @brief Gives every mission its fog hint for this pass and, with a mask, cuts the openings that show an
	 * unidentified mission's real piece. The openings are drawn only: no fog state and no reveal answer changes.
	 */
	void ApplyMissionFogHints();

	/** Copies the visible channel before the mission openings are cut, as the answer to what is really revealed. */
	void CacheRevealedPixels();

	/** @param OutLocations Receives where the enemy objects and divisions stand that the fog hides now. */
	void GatherHiddenEnemyPieceLocations(TArray<FVector>& OutLocations) const;

	/** @return Size of one mask pixel in map units along its largest side; 0 without a cloud. */
	float GetMaskPixelWorldSize() const;
	void NoteHiddenEnemyDistance(const FVector& MissionLocation, const TArray<FVector>& HiddenEnemyPieceLocations);
	bool GetIsMaskPixelSet(const TArray<uint8>& VisiblePixels, const FVector& WorldLocation) const;

	/** Logs what the fog hides and hints at after a fog pass (RTS_WORLDFOGHINTS); not in shipping builds. */
	void LogFogHintSummary() const;
	void Debug_DrawRadiusForConnections(EWorldMapFowState DebugState) const;
	void Debug_DrawRadiusForAnchors(EWorldMapFowState DebugState) const;
	void Debug_DrawRadiusForWorldObjects(EWorldMapFowState DebugState, UClass* WorldObjectClass) const;
	void Debug_DrawStateForConnections() const;
	void Debug_DrawStateForAnchors() const;
	void Debug_DrawStateForWorldObjects(UClass* WorldObjectClass) const;
	void Debug_DrawWorldString(const FVector& WorldLocation, const FString& DebugText, const FColor& Color) const;
	FVector Debug_GetConnectionTextLocation(const AConnection* Connection) const;
	FColor Debug_GetColorForState(EWorldMapFowState State) const;
	FString Debug_GetTextForState(EWorldMapFowState State) const;
	void CacheCloudActor();
	void InitializeMissingCampaignActorStates();
	void RemoveStaleAnchorStates();
	void ApplyRevealRulesFromVisibleAnchors();
	void ApplyWorldObjectStates();
	void ApplyConnectionStates();
	void RebuildMaskTexture();
	void StampAnchorsToMask();
	void StampConnectionsToMask();
	void StampWorldObjectsToMask();
	void PushMaskToCloudMaterial();
	void SetAnchorState(AAnchorPoint* AnchorPoint, EWorldMapFowState NewState);
	void SetObjectState(AWorldMapObject* WorldMapObject, EWorldMapFowState NewState);
	void SetConnectionState(AConnection* Connection, EWorldMapFowState NewState);
	void StampConnectionSegment(const AConnection* Connection, const AAnchorPoint* StartAnchor, const AAnchorPoint* EndAnchor);
	/**
	 * @brief Opens a soft circle in one mask channel; never lowers what is already open.
	 * @param Strength Mask value at the centre, 0 to 1; under 1 the fog only thins there.
	 */
	void StampCircle(const FVector& WorldLocation, float Radius, float Falloff, int32 ChannelIndex,
	                 float Strength = 1.f);
	void StampLine(const FVector& Start, const FVector& End, float Width, float Falloff, int32 ChannelIndex);
	int32 GetLineRasterizationSampleCount(const FVector& Start, const FVector& End) const;
	void WriteMaskPixelChannel(FColor& Pixel, int32 ChannelIndex, uint8 Value) const;
	int32 GetMaskChannelForComponent(const UWorldMapFowComponent* FowComponent) const;
	int32 GetPixelRadius(float WorldRadius) const;
	bool GetAnchorHoldsNeutralObject(const AAnchorPoint* AnchorPoint) const;
	EWorldMapFowState GetAnchorState(const AAnchorPoint* AnchorPoint) const;
	EWorldMapFowState GetConnectionStateFromConnectedAnchors(const AConnection* Connection) const;
	FIntPoint WorldToPixel(const FVector& WorldLocation) const;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;

	UPROPERTY()
	TWeakObjectPtr<AWorldFowCloud> M_WorldFowCloud;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> M_MaskTexture = nullptr;

	TArray<FColor> M_MaskPixels;

	// The visible channel as the reveal rules made it, without the drawn-only openings around unidentified missions.
	TArray<uint8> M_RevealedPixels;

	// For the RTS_WORLDFOGHINTS log line: closest hidden enemy piece to an unidentified mission; negative = none.
	float M_NearestHiddenEnemyToUnidentifiedMission = -1.f;
	TMap<TObjectKey<const AAnchorPoint>, EWorldMapFowState> M_AnchorStates;

	// Extra reveal circles (watch posts); stamped into the visible channel and they make anchors inside Visible.
	TMap<FName, FWorldFowRevealSource> M_RevealSourcesById;

	// Source of cleared anchors; found on the controller in InitializeWorldFow.
	UPROPERTY()
	TWeakObjectPtr<UWorldCampaignProgressManager> M_WorldCampaignProgressManager;

	FDelegateHandle M_AnchorClearedHandle;

	// False when the controller has no progress manager; cleared anchors are then simply not revealed.
	bool bM_HasCampaignProgress = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW", meta = (AllowPrivateAccess = "true"))
	int32 M_MaskResolution = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW Cloud", meta = (AllowPrivateAccess = "true"))
	int32 M_WorldFowCloudMaterialSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW Cloud", meta = (AllowPrivateAccess = "true"))
	FName M_WorldFowMaskParameterName = TEXT("WorldFowMask");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW|Debug", meta = (AllowPrivateAccess = "true"))
	bool bM_EnableWorldFowDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Campaign|FOW",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", DisplayName = "LinerasterizationSamplingMlt"))
	float M_LineRasterizationSamplingMlt = 1.0f;
};
