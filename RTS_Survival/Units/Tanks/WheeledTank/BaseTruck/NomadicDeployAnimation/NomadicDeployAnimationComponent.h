// Copyright Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "NomadicDeployAnimationComponent.generated.h"

class UNomadicDeployAnimationData;
class UStaticMeshComponent;
struct FNomadicDeployEffectBurst;

DECLARE_DELEGATE(FOnNomadicDeployAnimationFinished);

namespace NomadicDeployPrimitiveData
{
	// Custom primitive data float the deploy materials (MF_NomadicDeploy_VAT) read as progress 0..1.
	// 0 holds the faction flags, 4-7 the team colour, 8+ hole / panel masks.
	inline constexpr int32 ProgressIndex = 3;
}

/**
 * @brief Plays a nomadic building's baked VAT deploy (progress 0 -> 1) or pack-up (1 -> 0) on a separate deploy mesh
 * component and fires the building's smoke screen / landing dust bursts as the progress crosses them.
 * The real building mesh stays hidden while the deploy mesh animates; the owner shows it again in OnFinished.
 * CPU cost while playing: one custom primitive data write and a burst check at 30 Hz; idle it does not tick.
 * @note InitDeployAnimation: called by ANomadicVehicle::InitNomadicVehicle, and by building expansions that opt in
 * (ABuildingExpansion::DeployAnimationData), which play their construction / pack-up with it.
 */
UCLASS(ClassGroup = (Custom))
class RTS_SURVIVAL_API UNomadicDeployAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNomadicDeployAnimationComponent();

	/**
	 * @brief Attaches the deploy mesh to the building mesh so both share one transform.
	 * @param BuildingMeshComponent The real building mesh of the nomadic vehicle.
	 * @param DeployMeshComponent The component that shows the VAT deploy mesh while animating.
	 * @param DeployAnimationData The building's baked deploy; may be null (no animation, see GetHasDeployAnimation).
	 */
	void InitDeployAnimation(UStaticMeshComponent* BuildingMeshComponent, UStaticMeshComponent* DeployMeshComponent,
	                         UNomadicDeployAnimationData* DeployAnimationData);

	bool GetHasDeployAnimation() const;

	// Plays the deploy from the truck (progress 0) to the building over DurationSeconds.
	void PlayDeploy(const float DurationSeconds, const FOnNomadicDeployAnimationFinished& OnFinished);

	// Plays the pack-up from the building (progress 1) back into the truck over DurationSeconds.
	void PlayPackUp(const float DurationSeconds, const FOnNomadicDeployAnimationFinished& OnFinished);

	/**
	 * @brief Plays on from a progress the deploy mesh already showed, e.g. a building expansion that packs up during
	 * its construction, or resumes its construction after a cancelled pack-up.
	 * @param StartProgress Progress to continue from (0 = packed, 1 = built).
	 * @param bTowardBuilt Plays toward 1 when true, toward 0 otherwise.
	 * @param FullRangeDurationSeconds Time a full 0 -> 1 run takes; the remaining part plays at that rate.
	 * @param OnFinished Called once the end (1 or 0) is reached.
	 */
	void PlayFromProgress(const float StartProgress, const bool bTowardBuilt, const float FullRangeDurationSeconds,
	                      const FOnNomadicDeployAnimationFinished& OnFinished);

	// The progress last written to the deploy mesh (0 = packed, 1 = built).
	float GetProgress() const { return M_Progress; }

	// Stops without calling OnFinished and hides the deploy mesh (cancelled conversions).
	void StopAndHide();

	// Fired with every burst that asks for a construction sound.
	FSimpleDelegate OnPlayConstructionSound;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<UNomadicDeployAnimationData> M_DeployAnimationData;

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_BuildingMeshComponent;

	UPROPERTY()
	TWeakObjectPtr<UStaticMeshComponent> M_DeployMeshComponent;

	// +1 while deploying, -1 while packing up, 0 when idle.
	float M_Direction = 0.f;

	float M_Progress = 0.f;

	float M_DurationSeconds = 1.f;

	FOnNomadicDeployAnimationFinished M_OnFinished;

	bool GetIsValidDeployMeshComponent() const;

	void StartPlaying(const float StartProgress, const float Direction, const float DurationSeconds,
	                  const FOnNomadicDeployAnimationFinished& OnFinished);

	// Copies the building mesh's custom primitive data (team colour, flags) so the deploy mesh looks the same.
	void CopyBuildingPrimitiveData() const;

	void WriteProgress(const float Progress) const;

	// Plays every burst whose progress lies in (FromProgress, ToProgress], in either direction.
	void FireCrossedBursts(const float FromProgress, const float ToProgress);

	// Plays the bursts placed exactly on the start value (0 for a deploy, 1 for a pack-up), which no crossing reaches.
	void FireBurstsAtStartProgress(const float StartProgress);

	void SpawnBurst(const FNomadicDeployEffectBurst& Burst);

	void FinishPlaying();
};
