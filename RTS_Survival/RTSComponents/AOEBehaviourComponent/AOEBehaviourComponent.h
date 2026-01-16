// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "AOEBehaviourComponent.generated.h"

class UBehaviour;
class UBehaviourComp;
class UAnimatedTextWidgetPoolManager;
class URTSComponent;

namespace AOEBehaviourComponentConstants
{
	constexpr float DefaultIntervalSeconds = 2.25f;
	constexpr float DefaultRadius = 450.f;
	constexpr float DefaultTextOffsetZ = 120.f;
	constexpr float DefaultWrapAt = 320.f;
	constexpr float MinimumSweepRadius = 85.f;
}

UENUM(BlueprintType)
enum class EInAOEBehaviourApplyStrategy : uint8
{
	ApplyEveryTick,
	ApplyOnlyOnEnter
};

/**
 * @brief Text layout settings for the AOE behaviour component.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FAOEBehaviourTextSettings
{
	GENERATED_BODY()

	/** When enabled, pooled text is shown above affected units each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Animated Text")
	bool bUseText = false;

	/** Text shown above affected units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Animated Text")
	FString TextOnSubjects = TEXT("Aura");

	/** Local offset from the target actor root component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Animated Text")
	FVector TextOffset = FVector(0.f, 0.f, AOEBehaviourComponentConstants::DefaultTextOffsetZ);

	/** When true the text will auto-wrap at InWrapAt (set by the component). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Animated Text")
	bool bAutoWrap = true;

	/** Width in px when auto-wrap is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Animated Text")
	float InWrapAt = AOEBehaviourComponentConstants::DefaultWrapAt;

	/** Text justification for the rich text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Animated Text")
	TEnumAsByte<ETextJustify::Type> InJustification = ETextJustify::Center;

	/** Animation timings and vertical motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour|Animated Text")
	FRTSVerticalAnimTextSettings InSettings;
};

/**
 * @brief Settings for the AOE behaviour component.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FAOEBehaviourSettings
{
	GENERATED_BODY()

	/** Interval in seconds between async AOE checks. Must be > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	float IntervalSeconds = AOEBehaviourComponentConstants::DefaultIntervalSeconds;

	/** Radius in centimeters used for the async sphere sweep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	float Radius = AOEBehaviourComponentConstants::DefaultRadius;

	/** Behaviour classes to add/remove on overlapping behaviour components. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	TArray<TSubclassOf<UBehaviour>> BehaviourTypes;

	// Defines when the behaviours are applied to the beh components within our range and that pass IsValidTarget.
	// Either every tick apply new behaviours or only when they enter the radius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	EInAOEBehaviourApplyStrategy ApplyStrategy = EInAOEBehaviourApplyStrategy::ApplyEveryTick;

	/** Animated text settings for affected units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	FAOEBehaviourTextSettings TextSettings;

	/** Whether the component searches for distructables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	bool bAddDestructiblesToOverlap = false;

	/** Whether the component is seen as a debuff behaviour provider; meaning it deals damager or is otherwise bad for the
	 * unit receiving it. When this is the case the component will search for units NOT Allied with it (if false as by default
	 * it will look for allied units)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	bool bSearchForEnemies = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	bool bSearchForBothAlliesAndEnemies = false;
};

/**
 * @brief Component that periodically applies behaviours to nearby units using async AOE sweeps.
 * It tracks which units are currently affected and removes behaviours when they leave the radius.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UAOEBehaviourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAOEBehaviourComponent();

	/** Enable or disable the AOE behaviour processing. */
	UFUNCTION(BlueprintCallable, Category="AOE Behaviour")
	void SetAOEEnabled(const bool bEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void FilterUniqueActorsInPlace(TArray<FHitResult>& InOutHitResults);

	/**
	 * @brief Extension point for derived components to react to each apply tick.
	 * @param BehaviourComponentsInRange Components currently within the AOE radius.
	 * @note IF application strategy is ApplyEveryTick then make sure to call super when overriding!
	 * @note this function will apply the behaviours to the targets when the strategy is ApplyEveryTick.
	 */
	virtual void OnTickComponentsInRange(const TArray<UBehaviourComp*>& BehaviourComponentsInRange);

	/**
	 * @brief Extension point for derived components to respond to newly affected units.
	 * @param NewlyAffected Components that just entered the radius.
	 */
	virtual void OnNewlyEnteredRadius(const TArray<UBehaviourComp*>& NewlyAffected);

	/**
	 * @brief Extension point for derived components to respond to units leaving the radius.
	 * @param NoLongerAffected Components that just left the radius.
	 */
	virtual void OnLeftRadius(const TArray<UBehaviourComp*>& NoLongerAffected);

	virtual bool IsValidTarget(AActor* ValidActor) const;

	float GetAOERadius() const;

	const FAOEBehaviourSettings& GetAoeBehaviourSettings() const;
	// ---- Settings ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AOE Behaviour")
	FAOEBehaviourSettings AOEBehaviourSettings;

private:
	// ---- Cached references ----
	UPROPERTY()
	TObjectPtr<URTSComponent> M_RTSComponent = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UAnimatedTextWidgetPoolManager> M_AnimatedTextWidgetPoolManager;

	// ---- Runtime state ----
	UPROPERTY()
	TSet<TWeakObjectPtr<UBehaviourComp>> M_TrackedBehaviourComponents;

	FTimerHandle M_AOEIntervalTimerHandle;

	bool bM_AOEEnabled = true;

	// ---- Timer flow ----
	void BeginPlay_SetupAnimatedTextWidgetPoolManager();
	void BeginPlay_StartAoeTimer();
	void HandleApplyTick();

	// ---- Async sweep helpers ----
	/**
	 * @brief Run an async sphere sweep from the epicenter using owner overlap logic.
	 * @param InstigatorActor Actor used for world access and self-ignoring.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the sphere sweep.
	 */
	void StartAsyncSphereSweep(AActor* InstigatorActor, const FVector& Epicenter, const float Radius);
	FCollisionObjectQueryParams BuildObjectQueryParams(const ETriggerOverlapLogic OverlapLogic);
	void HandleSweepComplete(TArray<FHitResult>&& HitResults);

	// ---- Behaviour tracking ----
	void ApplyBehavioursToTarget(UBehaviourComp& BehaviourComponent) const;
	void RemoveBehavioursFromTarget(UBehaviourComp& BehaviourComponent) const;
	/**
	 * @brief Compare current sweep targets with tracked state and return added/removed arrays.
	 * @param CurrentTargets Set of behaviour components found in the latest sweep.
	 * @param OutAddedTargets Components newly found in range.
	 * @param OutRemovedTargets Components that left the range.
	 */
	void UpdateTrackedComponents(
		const TSet<TWeakObjectPtr<UBehaviourComp>>& CurrentTargets,
		TArray<UBehaviourComp*>& OutAddedTargets,
		TArray<UBehaviourComp*>& OutRemovedTargets);
	void ApplyTextForTargets(const TArray<UBehaviourComp*>& BehaviourComponentsInRange) const;

	// ---- Validation ----
	bool GetIsValidRTSComponent() const;
	bool GetIsValidAnimatedTextWidgetPoolManager() const;
	bool GetIsValidSettings() const;

	ETriggerOverlapLogic GetOverlapLogicForOwner() const;
};
