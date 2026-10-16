// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * @brief Test helper for a pit crew member who stays put while the gun turns: the Flak 88 emplacement's ammo runner
 * walks a loop from the ammo niche to a ready-round crate with a round on his shell bone. Find him once the turret
 * is up, then Sample every tick; it reads whether his round reached the niche and the crate and showed only on the
 * way. Used by the gun pit test scenario (-GerFlak88PitValidate).
 */
struct FGunPitRunnerWatch
{
	// The round counts as at the niche or the crate within this distance of the baked pick or lay pose.
	static constexpr float SpotToleranceCentimetres = 25.f;
	// The runner's loop is 11 s; a little over two loops are watched.
	static constexpr float WatchSeconds = 24.f;
	// The bake shrinks his shell bone to 0.001 while his hands are empty; above this scale the round shows.
	static constexpr float ShownScale = 0.5f;

	// Tag of the runner's skeletal mesh; none: the pit has no runner.
	FName RunnerTag = NAME_None;
	// Gun socket on the static root bone the runner stands on.
	FName StaticSocket = NAME_None;
	// The static mesh component (child of the runner) that is his round.
	FName RoundName = NAME_None;
	// The baked round centres when he takes it from the niche and lays it on the crate, in StaticSocket's frame.
	FVector NicheRound = FVector::ZeroVector;
	FVector DropRound = FVector::ZeroVector;
	bool bChecked = false;

	bool GetIsUsed() const { return not RunnerTag.IsNone(); }

	/** @return False when the turret has no mesh tagged RunnerTag or no RoundName component on it. */
	bool Find(const AActor& Turret);

	USkeletalMeshComponent* GetRunner() const { return M_Runner.Get(); }
	bool GetIsOnStaticSocket(const USkeletalMeshComponent& GunMesh) const;

	/**
	 * @brief Records where the runner's round is in the static frame and whether it shows.
	 * @param NowSeconds World time; the first sample starts the watch.
	 * @param GunMesh The turret mesh holding StaticSocket.
	 */
	void Sample(float NowSeconds, const USkeletalMeshComponent& GunMesh);

	bool GetIsWatchOver(const float NowSeconds) const
	{
		return M_StartSeconds >= 0.f && NowSeconds - M_StartSeconds >= WatchSeconds;
	}

	bool GetReachedNiche() const { return M_ClosestToNiche <= SpotToleranceCentimetres; }
	bool GetReachedDrop() const { return M_ClosestToDrop <= SpotToleranceCentimetres; }
	bool GetShowedAndHidRound() const { return M_ShownSamples > 0 && M_HiddenSamples > 0; }
	FString Describe() const;

private:
	TWeakObjectPtr<USkeletalMeshComponent> M_Runner;
	TWeakObjectPtr<UStaticMeshComponent> M_Round;
	float M_StartSeconds = -1.f;
	float M_ClosestToNiche = TNumericLimits<float>::Max();
	float M_ClosestToDrop = TNumericLimits<float>::Max();
	int32 M_ShownSamples = 0;
	int32 M_HiddenSamples = 0;
};
