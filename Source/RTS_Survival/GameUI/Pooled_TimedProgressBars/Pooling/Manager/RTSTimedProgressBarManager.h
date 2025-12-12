#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/WidgetComponent.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/RTSProgressBarType.h"
#include "RTSTimedProgressBarManager.generated.h"

class URTSTimedProgressBarSettings;
class UW_RTSTimedProgressBar;
class ARTSTimedProgressBarPoolActor;
class USceneComponent;

/**
 * @brief Internal per-instance data tracked by the pool manager.
 */
USTRUCT()
struct RTS_SURVIVAL_API FRTSTimedProgressBarInstance
{
	GENERATED_BODY()

	UPROPERTY() UWidgetComponent*       Component = nullptr;
	UPROPERTY() UW_RTSTimedProgressBar* Widget    = nullptr;

	// Identity
	uint64 ActivationID = 0;        // strictly increasing handle (unique over session)

	// Active state
	uint8  bActive   : 1;
	uint8  bAttached : 1;           // true when attached to a user-supplied scene component

	// Lifetime / fill math
	float  StartTime    = -1.0f;    // world seconds
	float  TimeTillFull = 0.0f;     // seconds to go from StartRatio -> 1.0
	float  InvDuration  = 0.0f;     // precomputed 1 / TimeTillFull (0 if TimeTillFull<=0)
	float  StartRatio   = 0.0f;     // 0..1 at activation time

	// Presentation
	uint8                bUsePercentageText : 1;
	UPROPERTY() ERTSProgressBarType  BarType = ERTSProgressBarType::Default;

	// World placement (non-attached mode)
	FVector WorldAnchor = FVector::ZeroVector;

	// Attached mode bookkeeping
	TWeakObjectPtr<USceneComponent> AttachedParent;
	FVector                         AttachOffset = FVector::ZeroVector;
};

/**
 * @brief Pooled manager that owns and updates all screen-space progress bar widgets.
 * The world subsystem creates and ticks this manager; widgets never tick themselves.
 */
UCLASS()
class RTS_SURVIVAL_API URTSTimedProgressBarManager : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize and pre-spawn the widget pool. Must be called once after world load. */
	void Init(UWorld* InWorld);

	/** Destroy pool content. Called by the subsystem on world unload. */
	void Shutdown();

	/** Per-frame update driven by the world subsystem. */
	void Tick(const float DeltaTime);

	/**
	 * @brief Activate a timed progress bar at a world location (non-attached).
	 * @return ActivationID (uint64) you can store to force-dormant later.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Timed Progress Bar")
	int ActivateTimedProgressBar(float RatioStart,
	                             float TimeTillFull,
	                             bool bUsePercentageText,
	                             ERTSProgressBarType BarType,
	                             const bool bUseDescriptionText, const FString& InText, const FVector& Location, const float ScaleMlt);

	/**
	 * @brief Activate a timed progress bar attached to a SceneComponent with a relative offset.
	 *        While attached, external position updates are disallowed.
	 * @return ActivationID (uint64)
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Timed Progress Bar")
	int ActivateTimedProgressBarAnchored(USceneComponent* AnchorComponent,
	                                     const FVector& AttachOffset,
	                                     float RatioStart,
	                                     float TimeTillFull,
	                                     bool bUsePercentageText,
	                                     ERTSProgressBarType BarType,
	                                     const bool bUseDescriptionText, const FString& InText, const float ScaleMlt);

	/**
	 * @brief Update the world location of a non-attached bar (sets its anchor immediately).
	 *        If the bar is attached, this does nothing and reports an error.
	 * @return true if updated; false if not found or disallowed.
	 */
	bool UpdateProgressBar(uint64 InActivationID, const FVector& NewWorldLocation);

	/** Force a specific bar dormant by ActivationID. No-op if not found. */
	void ForceProgressBarDormant(uint64 InActivationID);

	/** Pool size for debugging / queries. */
	int32 GetPoolSize() const { return M_Instances.Num(); }

private:
	// === State ===
	UPROPERTY() UWorld*                        M_World          = nullptr;
	UPROPERTY() ARTSTimedProgressBarPoolActor* M_PoolOwnerActor = nullptr;

	UPROPERTY() TArray<FRTSTimedProgressBarInstance> M_Instances;

	TArray<int32> M_FreeList;
	TArray<int32> M_ActiveList;

	uint64 M_NextActivationID = 1; // 0 reserved as "invalid"

	// === Validity helpers ===
	bool GetIsValidWorld() const;
	bool GetIsValidPoolOwnerActor() const;

	// === Internal helpers ===
	void Init_CreateOwnerActor();
	void Init_SpawnPool(const int32 PoolSize, UClass* WidgetBPClass);
	int32 FindOldestActiveIndex() const;

	/** Apply one instance update step; returns true if still active after the step. */
	bool AnimateInstance(FRTSTimedProgressBarInstance& Instance, const float NowSeconds) const;

	/** Transition an instance to dormant/hidden and (if needed) reattach it back to the pool owner. */
	void ResetInstance(FRTSTimedProgressBarInstance& Instance);
};
