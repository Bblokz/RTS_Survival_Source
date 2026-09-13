#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AnimatedIconTypes.h"
#include "AnimatedIconWidgetPoolManager.generated.h"

class AAnimatedIconPoolActor;
class UAnimatedIconDataAsset;
class UWidgetComponent;
class UW_RTSVerticalAnimatedIcon;
struct FRTSVerticalAnimatedIconDefinition;
struct FStreamableHandle;
class SWidget;

/** @brief Keeps each reusable slot's lifetime and attachment independent of the displayed icon type. */
USTRUCT()
struct FAnimatedIconPoolInstance
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<UWidgetComponent> Component;

	UPROPERTY(Transient)
	TWeakObjectPtr<UW_RTSVerticalAnimatedIcon> Widget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> AttachedActor;

	// Keep the Slate tree alive when the component is removed from the screen layer between activations.
	TSharedPtr<SWidget> CachedSlateWidget;
	uint64 ActivationOrder = 0;
	FRTSVerticalAnimIconSettings AnimationSettings;
	FVector StartWorldLocation = FVector::ZeroVector;
	double ActivatedAtSeconds = 0.0;
	float LastAppliedWorldOffsetZ = 0.0f;
	bool bAttachedToActor = false;
};

/** @brief Obtain this manager from the world subsystem to show icons without allocating widgets during gameplay. */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UAnimatedIconWidgetPoolManager : public UObject
{
	GENERATED_BODY()

public:
	bool Init(UWorld* World);
	void Shutdown();
	void Tick();

	/**
	 * @brief Uses the catalog defaults so gameplay callers only choose the event and its location.
	 * @param IconType Catalog entry; None deliberately does nothing.
	 * @param WorldLocation Fixed world anchor from which the icon rises.
	 * @return True when a pooled slot was activated.
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Icons")
	bool ShowAnimatedIcon(ERTSVerticalAnimatedIcon IconType, const FVector& WorldLocation);

	/**
	 * @brief Overrides timing and world-Z travel for an individual occurrence of a shared icon.
	 * @param IconType Catalog entry to display.
	 * @param WorldLocation Fixed world anchor from which the icon rises.
	 * @param AnimationSettings Nonnegative durations with a positive combined lifetime.
	 * @return True when a pooled slot was activated.
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Icons")
	bool ShowAnimatedIconWithSettings(ERTSVerticalAnimatedIcon IconType, const FVector& WorldLocation,
		const FRTSVerticalAnimIconSettings& AnimationSettings);

	/**
	 * @brief Follows a moving subject while retaining the same world-Z animation as vertical text.
	 * @param IconType Catalog entry whose default animation settings are used.
	 * @param AttachActor Subject to follow; its destruction recycles the icon.
	 * @param LocalOffset Offset relative to the subject's root, including its rotation and scale.
	 * @return True when a pooled slot was activated.
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Icons")
	bool ShowAnimatedIconAttachedToActor(ERTSVerticalAnimatedIcon IconType, AActor* AttachActor,
		const FVector& LocalOffset);

	/**
	 * @brief Allows an attached occurrence to use custom timing without editing shared defaults.
	 * @param IconType Catalog entry to display.
	 * @param AttachActor Subject to follow; its destruction recycles the icon.
	 * @param LocalOffset Offset relative to the subject's root.
	 * @param AnimationSettings Nonnegative durations with a positive combined lifetime.
	 * @return True when a pooled slot was activated.
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Icons")
	bool ShowAnimatedIconAttachedToActorWithSettings(ERTSVerticalAnimatedIcon IconType, AActor* AttachActor,
		const FVector& LocalOffset, const FRTSVerticalAnimIconSettings& AnimationSettings);

	UFUNCTION(BlueprintPure, Category="Animated Icons")
	int32 GetPoolSize() const;

	UFUNCTION(BlueprintPure, Category="Animated Icons")
	int32 GetActiveIconCount() const;

	UFUNCTION(BlueprintPure, Category="Animated Icons")
	bool IsReady() const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> M_World;

	UPROPERTY(Transient)
	TWeakObjectPtr<AAnimatedIconPoolActor> M_OwnerActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAnimatedIconDataAsset> M_IconDataAsset;

	// These handles own asset residency; weak references alone would allow unused icons to be collected.
	TSharedPtr<FStreamableHandle> M_CatalogLoadHandle;
	TSharedPtr<FStreamableHandle> M_IconAssetsLoadHandle;

	UPROPERTY(Transient)
	TArray<FAnimatedIconPoolInstance> M_Instances;

	// Slot storage is fixed after prewarming; only these index lists change during playback.
	TArray<int32> M_FreeIndices;
	TArray<int32> M_ActiveIndices;
	bool bM_IsReady = false;
	// Visibility delegates in custom Blueprint widgets must not reenter allocation or recycling.
	bool bM_IsUpdatingPool = false;
	uint64 M_ActivationCounter = 0;

	bool Init_LoadCatalog();
	bool Init_PreloadIconAssets();
	bool Init_CreatePool();
	bool Init_AddPoolInstance(UClass* WidgetClass);
	bool GetIsValidWorld() const;
	bool GetIsValidOwnerActor() const;
	bool GetIsValidIconDataAsset() const;
	bool GetIsValidInstance(const int32 InstanceIndex) const;
	const FRTSVerticalAnimatedIconDefinition* FindDefinition(const ERTSVerticalAnimatedIcon IconType) const;
	int32 FindOldestActiveIndex() const;
	void ResetInstance(const int32 InstanceIndex);
	void ReleaseActiveInstance(const int32 ActiveListIndex);
	bool AnimateInstance(const int32 InstanceIndex, const double NowSeconds);

	/**
	 * @brief Validates a request before consuming a slot, then atomically replaces any previous display.
	 * @param IconType Catalog entry to display.
	 * @param Location World position, or local offset when attaching.
	 * @param AttachActor Optional subject; null requests a fixed world anchor.
	 * @param OverrideSettings Optional per-request animation; null uses catalog defaults.
	 * @return True if activation completed; failure preserves free/active list membership.
	 */
	bool ShowIconInternal(const ERTSVerticalAnimatedIcon IconType, const FVector& Location,
		AActor* AttachActor, const FRTSVerticalAnimIconSettings* OverrideSettings);

	/**
	 * @brief Configures a hidden slot before making the new image visible.
	 * @param InstanceIndex Validated slot to reuse.
	 * @param Definition Snapshot of the selected catalog entry.
	 * @param Location World anchor or actor-local offset.
	 * @param AttachActor Optional actor to follow.
	 * @param AnimationSettings Validated settings for this activation.
	 * @return Whether the widget and component could both be activated.
	 */
	bool ActivateInstance(const int32 InstanceIndex, const FRTSVerticalAnimatedIconDefinition& Definition,
		const FVector& Location, AActor* AttachActor, const FRTSVerticalAnimIconSettings& AnimationSettings);
};
