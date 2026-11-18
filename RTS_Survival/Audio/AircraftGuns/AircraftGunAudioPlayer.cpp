// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftGunAudioPlayer.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/AircraftStrafeImpactBatcher.h"

DEFINE_LOG_CATEGORY_STATIC(LogAircraftGunAudio, Log, All);

namespace AircraftGunAudioPrivate
{
	// How often the burst-end check runs while a loop plays.
	constexpr float StopCheckIntervalSeconds = 0.03f;
	// A volley gap longer than this is a new burst, not a slow rate of fire: it does not update the interval.
	constexpr float MaxMeasuredVolleyIntervalSeconds = 0.6f;
	// Weight of a new volley gap in the running interval.
	constexpr float VolleyIntervalBlend = 0.3f;
}

#if RTS_WITH_MAP_TESTS
UAircraftGunAudioPlayer::FTestCounters& UAircraftGunAudioPlayer::GetTestCounters()
{
	static FTestCounters Counters;
	return Counters;
}
#define RTS_AIRCRAFT_GUN_AUDIO_COUNT(Field) (++GetTestCounters().Field)
#else
#define RTS_AIRCRAFT_GUN_AUDIO_COUNT(Field)
#endif

UAircraftGunAudioPlayer* UAircraftGunAudioPlayer::Create(
	UObject* Outer,
	const EAircraftGunAudioKind Kind,
	USceneComponent* AttachTo,
	const int32 GunCount,
	const FString& WeaponLabel)
{
	const UAircraftGunAudioDataAsset* const Data = UAircraftGunAudioDataAsset::Get();
	if (Data == nullptr || not IsValid(Outer) || not IsValid(AttachTo))
	{
		return nullptr;
	}
	const FAircraftGunSoundSet& Set = Data->GetSet(Kind);
	if (not Set.HasFireLoop())
	{
		return nullptr;
	}
	UAircraftGunAudioPlayer* const Player = NewObject<UAircraftGunAudioPlayer>(Outer);
	Player->M_Data = Data;
	Player->M_Kind = Kind;
	Player->M_AttachTo = AttachTo;
	Player->bM_UseManyGunLoops = GunCount >= Set.ManyGunsFromSockets;
	RTS_AIRCRAFT_GUN_AUDIO_COUNT(PlayersCreated);
	UE_LOG(LogAircraftGunAudio, Log, TEXT("RTS_AIRCRAFT_GUN_AUDIO configured weapon=%s kind=%s guns=%d loops=%s"),
	       *WeaponLabel, *StaticEnum<EAircraftGunAudioKind>()->GetNameStringByValue(static_cast<int64>(Kind)), GunCount,
	       Player->bM_UseManyGunLoops ? TEXT("many") : TEXT("few"));
	return Player;
}

const FAircraftGunSoundSet* UAircraftGunAudioPlayer::GetSet() const
{
	const UAircraftGunAudioDataAsset* const Data = M_Data.Get();
	return Data != nullptr ? &Data->GetSet(M_Kind) : nullptr;
}

UWorld* UAircraftGunAudioPlayer::GetPlayerWorld() const
{
	const USceneComponent* const AttachTo = M_AttachTo.Get();
	return IsValid(AttachTo) ? AttachTo->GetWorld() : nullptr;
}

float UAircraftGunAudioPlayer::GetNow() const
{
	const UWorld* const World = GetPlayerWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.f;
}

float UAircraftGunAudioPlayer::GetStopGapSeconds() const
{
	const FAircraftGunSoundSet* const Set = GetSet();
	if (Set == nullptr)
	{
		return 0.2f;
	}
	if (M_VolleyIntervalSeconds <= 0.f)
	{
		return Set->MinStopGapSeconds;
	}
	return FMath::Clamp(M_VolleyIntervalSeconds * Set->StopAfterMissedVolleys, Set->MinStopGapSeconds,
	                    Set->MaxStopGapSeconds);
}

void UAircraftGunAudioPlayer::OnVolleyFired()
{
	const FAircraftGunSoundSet* const Set = GetSet();
	if (Set == nullptr)
	{
		return;
	}
	RTS_AIRCRAFT_GUN_AUDIO_COUNT(Volleys);
	const float Now = GetNow();
	if (M_LastVolleyTime >= 0.f)
	{
		const float Gap = Now - M_LastVolleyTime;
		if (Gap > UE_KINDA_SMALL_NUMBER && Gap <= AircraftGunAudioPrivate::MaxMeasuredVolleyIntervalSeconds)
		{
			M_VolleyIntervalSeconds = M_VolleyIntervalSeconds <= 0.f
				                          ? Gap
				                          : FMath::Lerp(M_VolleyIntervalSeconds, Gap,
				                                        AircraftGunAudioPrivate::VolleyIntervalBlend);
		}
	}
	M_LastVolleyTime = Now;
	if (not bM_IsFiring)
	{
		StartFireLoop(*Set);
	}
	EnsureStopCheckTimer();
}

void UAircraftGunAudioPlayer::OnRoundArrived(
	const FVector& Location,
	const EAircraftStrafeImpactSurface Surface,
	const bool bHitUnit)
{
	const FAircraftGunSoundSet* const Set = GetSet();
	if (Set == nullptr)
	{
		return;
	}
	RTS_AIRCRAFT_GUN_AUDIO_COUNT(RoundsArrived);
	M_LastArrivalTime = GetNow();
	if (Set->HasImpactLoop())
	{
		StartOrMoveImpactLoop(*Set, Location, Surface);
		EnsureStopCheckTimer();
	}
	if (bHitUnit)
	{
		PlayUnitHitCluster(*Set, Location);
	}
}

USoundCue* UAircraftGunAudioPlayer::GetGroundImpactPerProjectile() const
{
	const FAircraftGunSoundSet* const Set = GetSet();
	return Set != nullptr ? Set->GroundImpactPerProjectile.Get() : nullptr;
}

void UAircraftGunAudioPlayer::StartFireLoop(const FAircraftGunSoundSet& Set)
{
	USoundBase* const Loop = bM_UseManyGunLoops && Set.FireLoopManyGuns != nullptr
		                         ? Set.FireLoopManyGuns.Get()
		                         : (Set.FireLoopFewGuns != nullptr ? Set.FireLoopFewGuns.Get() : Set.FireLoopManyGuns.Get());
	if (Loop == nullptr)
	{
		return;
	}
	// The burst state runs without an audio device too (-nosound), so the burst logic is testable headless.
	bM_IsFiring = true;
	RTS_AIRCRAFT_GUN_AUDIO_COUNT(FireLoopStarts);
	if (not IsValid(M_FireLoop))
	{
		M_FireLoop = MakeLoopComponent(Loop, true, FVector::ZeroVector);
		if (not IsValid(M_FireLoop))
		{
			return;
		}
	}
	M_FireLoop->SetVolumeMultiplier(Set.FireVolume);
	M_FireLoop->SetPitchMultiplier(PickPitch(Set));
	// A random start point: two aircraft firing together, or two bursts in a row, never start on the same round.
	M_FireLoop->Play(FMath::FRandRange(0.f, FMath::Max(Loop->GetDuration() - 0.05f, 0.f)));
}

USoundBase* UAircraftGunAudioPlayer::GetImpactLoopFor(
	const FAircraftGunSoundSet& Set,
	const EAircraftStrafeImpactSurface Surface) const
{
	const bool bIsHard = Surface == EAircraftStrafeImpactSurface::Stone;
	USoundBase* const Soft = bM_UseManyGunLoops && Set.ImpactLoopSoftManyGuns != nullptr
		                         ? Set.ImpactLoopSoftManyGuns.Get()
		                         : (Set.ImpactLoopSoftFewGuns != nullptr
			                            ? Set.ImpactLoopSoftFewGuns.Get()
			                            : Set.ImpactLoopSoftManyGuns.Get());
	if (not bIsHard)
	{
		return Soft;
	}
	USoundBase* const Hard = bM_UseManyGunLoops && Set.ImpactLoopHardManyGuns != nullptr
		                         ? Set.ImpactLoopHardManyGuns.Get()
		                         : Set.ImpactLoopHardFewGuns.Get();
	return Hard != nullptr ? Hard : Soft;
}

void UAircraftGunAudioPlayer::StartOrMoveImpactLoop(
	const FAircraftGunSoundSet& Set,
	const FVector& Location,
	const EAircraftStrafeImpactSurface Surface)
{
	// Unit hits keep the ground loop of the zone; only a ground round can change it.
	USoundBase* const WantedLoop = Surface == EAircraftStrafeImpactSurface::Metal && M_ImpactLoopSound != nullptr
		                               ? M_ImpactLoopSound.Get()
		                               : GetImpactLoopFor(Set, Surface);
	if (WantedLoop == nullptr)
	{
		return;
	}
	M_ImpactCentre = bM_IsImpacting ? FMath::Lerp(M_ImpactCentre, Location, Set.ImpactCentreFollowShare) : Location;
	if (IsValid(M_ImpactLoop))
	{
		M_ImpactLoop->SetWorldLocation(M_ImpactCentre);
	}
	if (bM_IsImpacting && WantedLoop == M_ImpactLoopSound)
	{
		return;
	}
	if (bM_IsImpacting)
	{
		RTS_AIRCRAFT_GUN_AUDIO_COUNT(ImpactLoopSwitches);
	}
	else
	{
		RTS_AIRCRAFT_GUN_AUDIO_COUNT(ImpactLoopStarts);
	}
	// The burst state runs without an audio device too (-nosound), so the burst logic is testable headless.
	bM_IsImpacting = true;
	M_ImpactLoopSound = WantedLoop;
	if (not IsValid(M_ImpactLoop))
	{
		M_ImpactLoop = MakeLoopComponent(WantedLoop, false, M_ImpactCentre);
		if (not IsValid(M_ImpactLoop))
		{
			return;
		}
	}
	M_ImpactLoop->SetSound(WantedLoop);
	M_ImpactLoop->SetVolumeMultiplier(Set.ImpactVolume);
	M_ImpactLoop->SetPitchMultiplier(PickPitch(Set));
	M_ImpactLoop->Play(FMath::FRandRange(0.f, FMath::Max(WantedLoop->GetDuration() - 0.05f, 0.f)));
}

void UAircraftGunAudioPlayer::PlayUnitHitCluster(const FAircraftGunSoundSet& Set, const FVector& Location)
{
	const float Now = GetNow();
	if (M_LastUnitHitSoundTime >= 0.f && Now - M_LastUnitHitSoundTime < Set.UnitHitMinIntervalSeconds)
	{
		return;
	}
	USoundBase* const Cluster = PickRandom(Set.UnitHitClusters);
	if (Cluster == nullptr)
	{
		return;
	}
	M_LastUnitHitSoundTime = Now;
	PlayOneShot(Cluster, Location, Set.ImpactVolume, false);
	RTS_AIRCRAFT_GUN_AUDIO_COUNT(UnitHitClusters);
}

void UAircraftGunAudioPlayer::EnsureStopCheckTimer()
{
	UWorld* const World = GetPlayerWorld();
	if (World == nullptr)
	{
		return;
	}
	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(M_StopCheckTimer))
	{
		return;
	}
	// Bound to this object: the timer manager drops it if the player is destroyed.
	TimerManager.SetTimer(M_StopCheckTimer,
	                      FTimerDelegate::CreateUObject(this, &UAircraftGunAudioPlayer::CheckForBurstEnd),
	                      AircraftGunAudioPrivate::StopCheckIntervalSeconds, true);
}

void UAircraftGunAudioPlayer::CheckForBurstEnd()
{
	const FAircraftGunSoundSet* const Set = GetSet();
	const USceneComponent* const AttachTo = M_AttachTo.Get();
	if (Set == nullptr || not IsValid(AttachTo))
	{
		StopAll();
		return;
	}
	const float Now = GetNow();
	const float StopGap = GetStopGapSeconds();
	if (bM_IsFiring && Now - M_LastVolleyTime > StopGap)
	{
		EndFire(*Set);
	}
	// Rounds of one volley arrive spread over the zone; the impact voice gets one interval more before it ends.
	if (bM_IsImpacting && Now - M_LastArrivalTime > StopGap + FMath::Max(M_VolleyIntervalSeconds, 0.f))
	{
		EndImpacts(*Set);
	}
	if (not bM_IsFiring && not bM_IsImpacting)
	{
		if (UWorld* const World = GetPlayerWorld())
		{
			World->GetTimerManager().ClearTimer(M_StopCheckTimer);
		}
	}
}

void UAircraftGunAudioPlayer::EndFire(const FAircraftGunSoundSet& Set)
{
	bM_IsFiring = false;
	if (IsValid(M_FireLoop))
	{
		M_FireLoop->FadeOut(Set.LoopFadeOutSeconds, 0.f);
	}
	const USceneComponent* const AttachTo = M_AttachTo.Get();
	if (USoundBase* const Tail = PickRandom(Set.FireTails); Tail != nullptr && IsValid(AttachTo))
	{
		PlayOneShot(Tail, AttachTo->GetComponentLocation(), Set.FireVolume, true);
		RTS_AIRCRAFT_GUN_AUDIO_COUNT(FireTails);
	}
}

void UAircraftGunAudioPlayer::EndImpacts(const FAircraftGunSoundSet& Set)
{
	bM_IsImpacting = false;
	if (IsValid(M_ImpactLoop))
	{
		M_ImpactLoop->FadeOut(Set.LoopFadeOutSeconds * 2.f, 0.f);
	}
	if (USoundBase* const Tail = PickRandom(Set.ImpactTails))
	{
		PlayOneShot(Tail, M_ImpactCentre, Set.ImpactVolume, false);
		RTS_AIRCRAFT_GUN_AUDIO_COUNT(ImpactTails);
	}
}

void UAircraftGunAudioPlayer::StopAll()
{
	bM_IsFiring = false;
	bM_IsImpacting = false;
	if (IsValid(M_FireLoop))
	{
		M_FireLoop->Stop();
	}
	if (IsValid(M_ImpactLoop))
	{
		M_ImpactLoop->Stop();
	}
	if (UWorld* const World = GetPlayerWorld())
	{
		World->GetTimerManager().ClearTimer(M_StopCheckTimer);
	}
}

UAudioComponent* UAircraftGunAudioPlayer::MakeLoopComponent(
	USoundBase* Sound,
	const bool bAttachToAircraft,
	const FVector& Location) const
{
	const UAircraftGunAudioDataAsset* const Data = M_Data.Get();
	USceneComponent* const AttachTo = M_AttachTo.Get();
	if (Data == nullptr || not IsValid(AttachTo) || Sound == nullptr)
	{
		return nullptr;
	}
	// Not auto-destroyed: the player plays and fades it per burst. Both are attached to the aircraft mesh so they die
	// with the aircraft; the impact voice uses an absolute location and stays at the zone.
	UAudioComponent* const Component = UGameplayStatics::SpawnSoundAttached(
		Sound, AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, 1.f, 1.f, 0.f,
		bAttachToAircraft ? Data->FireAttenuation : Data->ImpactAttenuation,
		bAttachToAircraft ? Data->FireConcurrency : Data->ImpactConcurrency, false);
	if (not IsValid(Component))
	{
		return nullptr;
	}
	Component->Stop();
	if (not bAttachToAircraft)
	{
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetWorldLocation(Location);
	}
	return Component;
}

void UAircraftGunAudioPlayer::PlayOneShot(
	USoundBase* Sound,
	const FVector& Location,
	const float Volume,
	const bool bIsFire) const
{
	const UAircraftGunAudioDataAsset* const Data = M_Data.Get();
	UWorld* const World = GetPlayerWorld();
	if (Data == nullptr || World == nullptr || Sound == nullptr)
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, Volume,
	                                      FMath::FRandRange(0.97f, 1.03f), 0.f,
	                                      bIsFire ? Data->FireAttenuation : Data->ImpactAttenuation,
	                                      bIsFire ? Data->FireConcurrency : Data->ImpactConcurrency);
}

USoundBase* UAircraftGunAudioPlayer::PickRandom(const TArray<TObjectPtr<USoundBase>>& Sounds)
{
	if (Sounds.IsEmpty())
	{
		return nullptr;
	}
	return Sounds[FMath::RandRange(0, Sounds.Num() - 1)].Get();
}

float UAircraftGunAudioPlayer::PickPitch(const FAircraftGunSoundSet& Set) const
{
	return FMath::FRandRange(FMath::Min(Set.PitchRange.X, Set.PitchRange.Y), FMath::Max(Set.PitchRange.X, Set.PitchRange.Y));
}
