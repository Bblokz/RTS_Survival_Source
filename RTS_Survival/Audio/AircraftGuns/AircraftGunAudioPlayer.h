// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AircraftGunAudioDataAsset.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "AircraftGunAudioPlayer.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundBase;
class USoundCue;
enum class EAircraftStrafeImpactSurface : uint8;

/**
 * @brief The sound of one aircraft gun (all its sockets): a looping fire voice on the aircraft for as long as volleys
 * keep coming, a looping impact voice that follows the beaten zone for as long as rounds keep arriving, a tail when
 * either stops, and a clang cluster when rounds hit a unit.
 * @details Owned by the weapon state (outer). A burst ends when no volley came for StopAfterMissedVolleys volley
 * intervals, measured while firing; a looping UObject timer checks that only while a loop plays. Replaces the per-volley
 * launch sound and the per-round impact sound; RandomBurst bursts then sound like bursts at the real rate of fire.
 */
UCLASS()
class RTS_SURVIVAL_API UAircraftGunAudioPlayer : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Makes a player when the RTS Audio Settings hold aircraft gun sounds for this kind.
	 * @param Outer The weapon state; the player and its timer die with it.
	 * @param Kind Which sound set.
	 * @param AttachTo The aircraft mesh the fire loop follows.
	 * @param GunCount Fire sockets of the weapon; picks the few- or many-guns loops.
	 * @param WeaponLabel Shown in the configured log line.
	 * @return nullptr when there is no data or no fire loop for this kind; the weapon then keeps its own sounds.
	 */
	static UAircraftGunAudioPlayer* Create(UObject* Outer, EAircraftGunAudioKind Kind, USceneComponent* AttachTo,
	                                       int32 GunCount, const FString& WeaponLabel);

	/** Every volley (all guns firing once); starts or keeps the fire loop. */
	void OnVolleyFired();

	/**
	 * @brief A strafe round (or a visual spurt group) arrived at the ground or a unit.
	 * @param Location Where it landed.
	 * @param Surface Ground surface of the zone; Metal for a unit hit.
	 * @param bHitUnit The round hit a unit: adds a clang cluster (throttled).
	 */
	void OnRoundArrived(const FVector& Location, EAircraftStrafeImpactSurface Surface, bool bHitUnit);

	/** The per-projectile ground impact sound of the cannon set, or nullptr. */
	USoundCue* GetGroundImpactPerProjectile() const;

	EAircraftGunAudioKind GetKind() const { return M_Kind; }
	bool GetUsesManyGunLoops() const { return bM_UseManyGunLoops; }

	/** Stops both loops at once (weapon disabled or owner dying); no tails. */
	void StopAll();

#if RTS_WITH_MAP_TESTS
	/** Process-wide counts for headless tests (a -nosound run cannot listen). */
	struct FTestCounters
	{
		int32 PlayersCreated = 0;
		int32 Volleys = 0;
		int32 FireLoopStarts = 0;
		int32 FireTails = 0;
		int32 RoundsArrived = 0;
		int32 ImpactLoopStarts = 0;
		int32 ImpactLoopSwitches = 0;
		int32 ImpactTails = 0;
		int32 UnitHitClusters = 0;
	};
	static FTestCounters& GetTestCounters();
#endif

private:
	// Held here so the data asset stays loaded while any aircraft gun exists.
	UPROPERTY()
	TObjectPtr<const UAircraftGunAudioDataAsset> M_Data;
	EAircraftGunAudioKind M_Kind = EAircraftGunAudioKind::MachineGun;
	TWeakObjectPtr<USceneComponent> M_AttachTo;
	bool bM_UseManyGunLoops = false;

	UPROPERTY()
	TObjectPtr<UAudioComponent> M_FireLoop;

	UPROPERTY()
	TObjectPtr<UAudioComponent> M_ImpactLoop;

	// The loop the impact voice plays now, so a change of ground switches it.
	UPROPERTY()
	TObjectPtr<USoundBase> M_ImpactLoopSound;

	bool bM_IsFiring = false;
	bool bM_IsImpacting = false;
	float M_LastVolleyTime = -1.f;
	float M_VolleyIntervalSeconds = -1.f;
	float M_LastArrivalTime = -1.f;
	float M_LastUnitHitSoundTime = -1.f;
	FVector M_ImpactCentre = FVector::ZeroVector;
	FTimerHandle M_StopCheckTimer;

	const FAircraftGunSoundSet* GetSet() const;
	UWorld* GetPlayerWorld() const;
	float GetNow() const;
	float GetStopGapSeconds() const;

	void StartFireLoop(const FAircraftGunSoundSet& Set);
	void StartOrMoveImpactLoop(const FAircraftGunSoundSet& Set, const FVector& Location, EAircraftStrafeImpactSurface Surface);
	USoundBase* GetImpactLoopFor(const FAircraftGunSoundSet& Set, EAircraftStrafeImpactSurface Surface) const;
	void PlayUnitHitCluster(const FAircraftGunSoundSet& Set, const FVector& Location);

	void EnsureStopCheckTimer();
	void CheckForBurstEnd();
	void EndFire(const FAircraftGunSoundSet& Set);
	void EndImpacts(const FAircraftGunSoundSet& Set);

	UAudioComponent* MakeLoopComponent(USoundBase* Sound, bool bAttachToAircraft, const FVector& Location) const;
	void PlayOneShot(USoundBase* Sound, const FVector& Location, float Volume, bool bIsFire) const;
	static USoundBase* PickRandom(const TArray<TObjectPtr<USoundBase>>& Sounds);
	float PickPitch(const FAircraftGunSoundSet& Set) const;
};
