// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DamageEvents.h"
#include "GeometryDamageSettings.h"

#include "GeometryDamageComponent.generated.h"

class UGeometryCollectionComponent;
class UHealthComponent;

namespace GeometryDamageConstants
{
	constexpr float ImpactMergeDistance = 60.f;
	constexpr float MinValidMaxHealth = 1.f;
}

/**
 * @brief Drives health-relative, point-local Chaos reactions during the alive-and-being-hit phase of an actor.
 * The bound collection stays kinematic until strain frees pieces, then bounded force ticks push and quiesce them.
 * @note InitGeometryDamage: call in Blueprint to bind the Geometry Collection before damage can be forwarded.
 * @note OnDamageTaken: call in Blueprint after health is applied so threshold crossings use post-hit health.
 * @note StopGeometryDamage: call before handing the collection to FRTS_Collapse on death.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UGeometryDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGeometryDamageComponent();

	/**
	 * @brief Makes the explicitly selected collection ready for break-and-push reactions without integrating damage.
	 * @param InGeometryCollection The same-owner Geometry Collection that receives every physics field.
	 * @param InHealthComponent Optional health source; when null the owner is searched for UHealthComponent.
	 * @return Whether the collection configuration is valid and the component can accept impacts.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
	bool InitGeometryDamage(UGeometryCollectionComponent* InGeometryCollection,
		UHealthComponent* InHealthComponent = nullptr);

	/**
	 * @brief Converts an already-applied damage result into a health-relative geometry reaction.
	 * @param Hit World-space impact and optional explicit health context.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
	void OnDamageTaken(const FGeometryDamageHit& Hit);

	/**
	 * @brief Normalises the engine point-damage payload and preserves the causer direction fallback.
	 * @param DamageAmount Raw applied damage.
	 * @param DamageEvent Engine point hit data.
	 * @param DamageCauser Optional source used only when shot direction and impact normal are unavailable.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
	void OnDamageTakenFromEvent(float DamageAmount, const FPointDamageEvent& DamageEvent, AActor* DamageCauser);

	/** @brief Clears reaction state without sleeping pieces so a death-collapse system can take ownership safely. */
	UFUNCTION(BlueprintCallable, Category = "GeometryDamage")
	void StopGeometryDamage();

	/**
	 * @brief Fires a synthetic impact so the collection can be tuned before damage-pipeline integration exists.
	 * @param WorldLocation World-space centre of the test fields.
	 * @param DamageAmount Raw damage normalised against DebugMaxHealthOverride.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GeometryDamage|Debug")
	void Debug_FireTestImpact(const FVector WorldLocation, const float DamageAmount);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Debug", meta = (ClampMin = "1.0"))
	float DebugMaxHealthOverride = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Scaling")
	FGeometryDamageScalingSettings ScalingSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Forces")
	FGeometryDamageForceProfile DefaultForceProfile;

	// Per-unit force trim applied after profile interpolation and threshold escalation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Forces", meta = (ClampMin = "0.0"))
	float GlobalForceMultiplier = 1.f;

	// Per-unit strain trim applied after profile interpolation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Forces", meta = (ClampMin = "0.0"))
	float GlobalStrainMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Thresholds")
	TArray<FGeometryDamageThresholdStage> ThresholdStages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "GeometryDamage|Performance")
	FGeometryDamageSimulationBudget SimulationBudget;

private:
	/**
	 * @brief Shared implementation keeps causer lifetime use on the receipt frame and out of the tick path.
	 * @param Hit Resolved world-space hit and health overrides.
	 * @param DamageCauser Optional source for the third direction fallback.
	 */
	void HandleDamageTaken(const FGeometryDamageHit& Hit, const AActor* DamageCauser);
	bool ValidateAndPrepareGeometryCollectionForInitialisation();

	/**
	 * @brief Resolves explicit health overrides before consulting the weak health component.
	 * @param Hit Impact carrying optional health overrides.
	 * @param OutMaxHealth Receives current maximum health.
	 * @param OutHealthPercentAfter Receives post-impact health in the 0..1 range.
	 * @return Whether both required health values were available and valid.
	 */
	bool ResolveHealthContext(const FGeometryDamageHit& Hit, float& OutMaxHealth,
		float& OutHealthPercentAfter) const;

	/** @return Severity in 0..1, or zero when damage is inside the configured deadzone. */
	float CalculateSeverity(const float DamageAmount, const float MaxHealth) const;

	/**
	 * @brief Applies the documented shot, normal, causer, then radial fallback chain once at impact receipt.
	 * @param Hit Impact direction data.
	 * @param DamageCauser Optional source actor for positional direction recovery.
	 * @param OutDirection Receives a normalised direction when one can be resolved.
	 * @return Whether a directional graph is safe to dispatch.
	 */
	bool ResolveShotDirection(const FGeometryDamageHit& Hit, const AActor* DamageCauser,
		FVector& OutDirection) const;

	void DispatchStrainField(const FGeometryDamageActiveImpact& Impact,
		const FGeometryDamageForceProfile& Profile);
	void DispatchForceFields(const FGeometryDamageActiveImpact& Impact,
		const FGeometryDamageForceProfile& Profile, const float DecayWeight);
	void DispatchArchetypeGraph(const FGeometryDamageActiveImpact& Impact,
		const FGeometryDamageForceProfile& Profile, const EGeometryDamageForceArchetype Archetype,
		const float Magnitude);

	void EvaluateThresholdCrossings(const FGeometryDamageHit& Hit,
		const FGeometryDamageActiveImpact& Impact, const float MaxHealth,
		const float HealthPercentAfter);
	/**
	 * @brief Keeps overshoot and heal-rearm bookkeeping isolated from the ordered stage traversal.
	 * @param StageIndex Index-aligned entry in ThresholdStages and M_FiredThresholdStages.
	 * @param Hit Impact location and normal used by crossing FX.
	 * @param Impact Resolved reaction data used by a crossing burst.
	 * @param HealthPercentBefore Reconstructed pre-impact health percentage.
	 * @param HealthPercentAfter Post-impact health percentage.
	 */
	void EvaluateThresholdStage(const int32 StageIndex, const FGeometryDamageHit& Hit,
		const FGeometryDamageActiveImpact& Impact, const float HealthPercentBefore,
		const float HealthPercentAfter);
	void FireThresholdStage(const int32 StageIndex, const FGeometryDamageHit& Hit,
		const FGeometryDamageActiveImpact& Impact);
	void PlayThresholdFX(const FGeometryDamageFX& FX, const FGeometryDamageHit& Hit,
		const float Severity);
	float GetSustainedForceMultiplier(const float HealthPercentAfter) const;
	static float GetHealthLevelThreshold(const EHealthLevel HealthLevel);

	void OpenOrResetSimulationWindow();
	void QuiesceSimulation();
	void Tick_ResolvePendingImpacts();
	void Tick_ReissueSustainedForces(const float DeltaTime);
	void MergePendingImpact(TArray<FGeometryDamageActiveImpact>& InOutMergedImpacts,
		const FGeometryDamageActiveImpact& PendingImpact) const;
	static void MergeImpact(FGeometryDamageActiveImpact& InOutImpact,
		const FGeometryDamageActiveImpact& NewImpact);
	/**
	 * @brief Applies the per-frame dispatch cap while retaining nearby impacts for sustained re-issue.
	 * @param Impact Coalesced impact to activate.
	 * @param bDispatchInitialFields Whether this impact is inside the per-frame initial dispatch cap.
	 * @param EffectiveImpulseWindow Clamped duration for retaining sustained force state.
	 */
	void ActivateMergedImpact(const FGeometryDamageActiveImpact& Impact,
		const bool bDispatchInitialFields, const float EffectiveImpulseWindow);

	bool GetShouldDispatchFieldsAtLocation(const FVector& WorldLocation) const;
	void ValidateConfigurationWarnings() const;
	void ValidateThresholdFXWarnings(const FGeometryDamageThresholdStage& Stage) const;

	UPROPERTY()
	TObjectPtr<UGeometryCollectionComponent> M_GeometryCollection = nullptr;
	bool GetIsValidGeometryCollection() const;

	UPROPERTY()
	TWeakObjectPtr<UHealthComponent> M_HealthComponent;
	bool GetIsValidHealthComponent() const;

	UPROPERTY()
	TArray<FGeometryDamageActiveImpact> M_ActiveImpacts;

	UPROPERTY()
	TArray<FGeometryDamageActiveImpact> M_PendingImpacts;

	TBitArray<> M_FiredThresholdStages;

	float M_WindowExpiryTime = 0.f;
	float M_LastThresholdFXTime = 0.f;
	bool bM_IsInitialised = false;
};
