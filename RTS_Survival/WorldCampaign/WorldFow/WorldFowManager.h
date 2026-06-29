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
class UWorldMapFowComponent;

/**
 * @brief Spawned by the world player controller after campaign generation to calculate actor FOW states and the cloud mask.
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

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_RevealAll();

	UFUNCTION(CallInEditor, Category = "World Campaign|FOW|Debug")
	void Debug_HideAll();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool GetIsValidWorldGenerator() const;
	bool GetIsValidWorldFowCloud() const;
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
	void StampCircle(const FVector& WorldLocation, float Radius, float Falloff, int32 ChannelIndex);
	void StampLine(const FVector& Start, const FVector& End, float Width, float Falloff, int32 ChannelIndex);
	int32 GetLineRasterizationSampleCount(const FVector& Start, const FVector& End) const;
	void WriteMaskPixelChannel(FColor& Pixel, int32 ChannelIndex, uint8 Value) const;
	int32 GetMaskChannelForComponent(const UWorldMapFowComponent* FowComponent) const;
	int32 GetPixelRadius(float WorldRadius) const;
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
	TMap<TObjectKey<const AAnchorPoint>, EWorldMapFowState> M_AnchorStates;

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
