// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;

/**
 * @brief Test helper: how far a pit gun's kick bone and its idle crew's heads (against their pelvis) move in the
 * moments after a shot. Start it at the first shot, sample it every tick, read it once WatchSeconds have passed.
 * Used by the gun pit and B-4 pit test scenarios.
 */
struct FGunPitFireReactionWatch
{
	// The kick and the brace play out within this long after a shot.
	static constexpr float WatchSeconds = 0.7f;
	// Smallest moves that show the gun bucked (kicks are 3 to 7 cm) and the crew braced (the brace tips the head
	// about 10 cm forward of the pelvis; an idle head moves well under 1 cm).
	static constexpr float MinimumKickBoneMove = 1.5f;
	static constexpr float MinimumCrewHeadMove = 4.f;

	void Start(const float WorldSeconds, const USkeletalMeshComponent& GunMesh,
	           const TArray<TWeakObjectPtr<USkeletalMeshComponent>>& CrewMeshes);
	void Sample(const USkeletalMeshComponent& GunMesh, const TArray<TWeakObjectPtr<USkeletalMeshComponent>>& CrewMeshes);

	bool GetHasStarted() const { return StartSeconds >= 0.f; }
	bool GetIsWatchOver(const float WorldSeconds) const { return GetHasStarted() && WorldSeconds - StartSeconds >= WatchSeconds; }
	bool GetDidGunBuck() const { return not KickBone.IsNone() && MaxKickBoneMove >= MinimumKickBoneMove; }
	bool GetDidCrewBrace() const { return MaxCrewHeadMove >= MinimumCrewHeadMove; }
	FString Describe() const;

	// Crew from this index on only idle (spotter, commander); the loader and handler start their drill at the shot.
	int32 FirstIdleCrewIndex = 2;
	bool bChecked = false;
	// Pictures taken so far after the shot (the scenario captures the kick peak and the held brace).
	int32 CapturesTaken = 0;

	float GetSecondsSinceShot(const float WorldSeconds) const { return WorldSeconds - StartSeconds; }

private:
	float StartSeconds = -1.f;
	FName KickBone = NAME_None;
	FVector KickBoneAtShot = FVector::ZeroVector;
	TArray<FVector> CrewHeadsAtShot;
	float MaxKickBoneMove = 0.f;
	float MaxCrewHeadMove = 0.f;
};
