#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/WidgetComponent.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "AnimatedTextWidgetPoolManager.generated.h"

class AActor;
class UAnimatedTextSettings;
class UW_RTSVerticalAnimatedText;
class AAnimatedTextPoolActor;

/**
 * @brief Internal per-instance data tracked by the pool manager.
 */
USTRUCT()
struct RTS_SURVIVAL_API FAnimatedTextInstance
{
	GENERATED_BODY()

	UPROPERTY()
	UWidgetComponent* Component = nullptr;

	UPROPERTY()
	UW_RTSVerticalAnimatedText* Widget = nullptr;

	// timing & movement (no UPROPERTY needed)
	float ActivatedAtSeconds = -1.0f;
	FVector StartWorldLocation = FVector::ZeroVector;

	float StartT = 0.0f;
	float VisibleEndT = 0.0f;
	float FadeEndT = 0.0f;
	float InvTotal = 0.0f;
	float InvFade = 0.0f;
	float LastAppliedZ = 0.0f;

	uint8 bActive : 1;
	uint8 bAttachedToActor : 1;

	UPROPERTY()
	TWeakObjectPtr<AActor> AttachedActor;

	FVector AttachOffset = FVector::ZeroVector;

	// keep your settings last
	UPROPERTY()
	FRTSVerticalAnimTextSettings Settings;
};

/**
 * @brief Aggregates state for one widget pool (owner actor, class, instance arrays and freelists).
 */
USTRUCT()
struct RTS_SURVIVAL_API FAnimatedTextPool
{
	GENERATED_BODY()

	/** Invisible owner for this pool's widget components. */
	UPROPERTY()
	AAnimatedTextPoolActor* OwnerActor = nullptr;

	/** Cached BP class to spawn widgets of this pool. */
	UPROPERTY()
	UClass* WidgetBPClass = nullptr;

	/** Pooled instances. */
	UPROPERTY()
	TArray<FAnimatedTextInstance> Instances;

	/** Free indices available for allocation. */
	TArray<int32> FreeList;

	/** Active indices currently animating. */
	TArray<int32> ActiveList;

	/** Human-readable name for logging. */
	UPROPERTY()
	FName DebugName = NAME_None;
};

/**
 * @brief Pooled manager that owns and animates all screen-space animated text widgets.
 * The world subsystem creates and ticks this manager; widgets never tick themselves.
 */
UCLASS()
class RTS_SURVIVAL_API UAnimatedTextWidgetPoolManager : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize and pre-spawn the widget pools. Must be called once after world load. */
	void Init(UWorld* InWorld);

	/** Destroy pool content. Called by the subsystem on world unload. */
	void Shutdown();

	/** Per-frame update driven by the world subsystem. */
	void Tick(const float DeltaTime);

	/**
	 * @brief Show an animated text in the Regular pool. Reuses oldest active if full.
	 * @param InText Rich text (string) to display.
	 * @param InWorldStartLocation World location to project to screen from.
	 * @param bInAutoWrap Auto-wrap toggle.
	 * @param InWrapAt Wrap width in pixels when auto-wrap is true.
	 * @param InJustification Text justification.
	 * @param InSettings Animation timings and vertical delta.
	 * @return true if a widget was (re)used successfully, false on error (missing class, world, etc.).
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	bool ShowAnimatedText(const FString& InText,
	                      const FVector& InWorldStartLocation,
	                      const bool bInAutoWrap,
	                      const float InWrapAt,
	                      const TEnumAsByte<ETextJustify::Type> InJustification,
	                      const FRTSVerticalAnimTextSettings& InSettings);

	/**
	 * @brief Show an animated text in the Regular pool attached to an actor so it follows movement.
	 * @param InText Rich text (string) to display.
	 * @param InAttachActor Actor to attach the widget component to.
	 * @param InAttachOffset Local offset from the actor root component.
	 * @param bInAutoWrap Auto-wrap toggle.
	 * @param InWrapAt Wrap width in pixels when auto-wrap is true.
	 * @param InJustification Text justification.
	 * @param InSettings Animation timings and vertical delta.
	 * @return true if a widget was (re)used successfully, false on error (missing class, world, etc.).
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	bool ShowAnimatedTextAttachedToActor(const FString& InText,
	                                     AActor* InAttachActor,
	                                     const FVector& InAttachOffset,
	                                     const bool bInAutoWrap,
	                                     const float InWrapAt,
	                                     const TEnumAsByte<ETextJustify::Type> InJustification,
	                                     const FRTSVerticalAnimTextSettings& InSettings);

	/**
	 * @brief Show a single resource animated text in the Resource pool.
	 * @param ResourceSettings Resource type and delta amount (positive=blueprint, negative=red).
	 * @param InWorldStartLocation Anchor location in world.
	 * @param bInAutoWrap Auto-wrap toggle.
	 * @param InWrapAt Wrap width in pixels when auto-wrap is true.
	 * @param InJustification Text justification.
	 * @param InSettings Animation timings and vertical delta.
	 * @return true on success, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	bool ShowSingleAnimatedResourceText(
		FRTSVerticalSingleResourceTextSettings ResourceSettings,
		const FVector& InWorldStartLocation,
		const bool bInAutoWrap,
		const float InWrapAt,
		const TEnumAsByte<ETextJustify::Type> InJustification,
		const FRTSVerticalAnimTextSettings& InSettings);

	/**
	 * @brief Show a double resource animated text in the Resource pool.
	 * @param ResourceSettings Two resource types and amounts.
	 * @param InWorldStartLocation Anchor location in world.
	 * @param bInAutoWrap Auto-wrap toggle.
	 * @param InWrapAt Wrap width in pixels when auto-wrap is true.
	 * @param InJustification Text justification.
	 * @param InSettings Animation timings and vertical delta.
	 * @return true on success, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	bool ShowDoubleAnimatedResourceText(
		FRTSVerticalDoubleResourceTextSettings ResourceSettings,
		const FVector& InWorldStartLocation,
		const bool bInAutoWrap,
		const float InWrapAt,
		const TEnumAsByte<ETextJustify::Type> InJustification,
		const FRTSVerticalAnimTextSettings& InSettings);

	/** Pool size for debugging / queries (Regular pool). */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	int32 GetPoolSize() const { return M_RegularPool.Instances.Num(); }

	/** Pool size for debugging / queries (Resource pool). */
	UFUNCTION(BlueprintCallable, Category="Animated Text")
	int32 GetResourcePoolSize() const { return M_ResourcePool.Instances.Num(); }

private:
	// === State ===
	UPROPERTY()
	UWorld* M_World = nullptr;

	/** Regular vertical animated text pool. */
	UPROPERTY()
	FAnimatedTextPool M_RegularPool;

	/** Resource vertical animated text pool (uses ResourceWidgetClass). */
	UPROPERTY()
	FAnimatedTextPool M_ResourcePool;

	/** Projects a world location to absolute screen pixels for our screen-space widget usage. */
	bool ProjectWorldToScreen(const FVector& WorldLocation, FVector2D& OutScreen) const;

	// === Validity helpers as per rule 0.5 ===
	bool GetIsValidWorld() const;

	// === Internal generic helpers (work on any pool) ===
	void Init_CreateOwnerActor(FAnimatedTextPool& Pool, const FName OwnerName);
	void Init_SpawnPool(FAnimatedTextPool& Pool, const int32 PoolSize, UClass* WidgetBPClass);
	int32 FindOldestActiveIndex(const FAnimatedTextPool& Pool) const;
	bool AnimateInstance(FAnimatedTextInstance& Instance, const float NowSeconds) const;
	void ResetInstance(FAnimatedTextPool& Pool, FAnimatedTextInstance& Instance) const;

	/**
	 * @brief Core allocator/activator for a given pool.
	 * @param Pool Target pool (regular/resource).
	 * @param InText Rich text to render.
	 * @param InWorldStartLocation World anchor.
	 * @param bInAutoWrap Auto wrap flag.
	 * @param InWrapAt Wrap width in px.
	 * @param InJustification Justification in text block.
	 * @param InSettings Animation settings.
	 * @return true if a widget instance was allocated and activated.
	 */
	bool ShowAnimatedTextInPool(FAnimatedTextPool& Pool,
	                            const FString& InText,
	                            const FVector& InWorldStartLocation,
	                            const bool bInAutoWrap,
	                            const float InWrapAt,
	                            const TEnumAsByte<ETextJustify::Type> InJustification,
	                            const FRTSVerticalAnimTextSettings& InSettings,
	                            AActor* InAttachActor = nullptr,
	                            const FVector& InAttachOffset = FVector::ZeroVector);

	/** Resource string helpers. */
	FString GetSingleResourceText(const ERTSResourceType ResType, const int32 AddOrSubtractAmount) const;
	FString GetDoubleResourceText(const FRTSVerticalDoubleResourceTextSettings& ResourceSettings) const;
};
