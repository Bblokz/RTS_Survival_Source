// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GunPitCrewComponent.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * @brief Animations of one crew member of a gun pit; plain skeletal meshes running UGunPitCrewAnimInstance,
 * not squad units.
 */
USTRUCT(BlueprintType)
struct FGunPitCrewMemberAnimations
{
	GENERATED_BODY()

	// Loops between reloads; keeps the crew member at its own spot in the pit.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Crew")
	TObjectPtr<UAnimSequenceBase> IdleAnimation = nullptr;

	// DefaultSlot montage played once per reload, stretched to the reload time. Null: this member only idles.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Crew")
	TObjectPtr<UAnimMontage> ReloadMontage = nullptr;

	// Optional DefaultSlot montage started on a shot unless it still plays, so a burst keeps one shake going (a
	// machine gunner holding the grips). Null: this member does not react to shots.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Crew")
	TObjectPtr<UAnimMontage> FireMontage = nullptr;
};

/** @brief Gun montages and crew animations of a gun pit. */
USTRUCT(BlueprintType)
struct FGunPitReloadSettings
{
	GENERATED_BODY()

	// Optional loading drill of the gun itself (the B-4's crane reload); its recoil matches GunFireMontage, so a
	// drill can continue a recoil that is already playing. Null: only the crew animate the reload.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	TObjectPtr<UAnimMontage> GunReloadMontage = nullptr;

	// Recoil only; used when a shot is not followed by a gun reload montage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	TObjectPtr<UAnimMontage> GunFireMontage = nullptr;

	// One entry per crew mesh passed to InitPitCrew, in the same order.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	TArray<FGunPitCrewMemberAnimations> CrewAnimations;

	// Shorter reloads (e.g. a reload-speed buff stacked very high) would rush the drill; then only the recoil plays.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit", meta = (ClampMin = "0.1"))
	float MinReloadTimeForLoadingSequence = 6.f;
};

/** A round's pose in the space of the gun bone the crew ride on. */
USTRUCT(BlueprintType)
struct FGunPitRoundPose
{
	GENERATED_BODY()

	// Middle of the round.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds")
	FVector Location = FVector::ZeroVector;

	// Where the nose points (the round mesh's +X).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds")
	FVector Axis = FVector::ForwardVector;
};

/**
 * @brief How the rounds change hands during a drill: the handler holds the next round out to the loader, the
 * loader rams it, and meanwhile the handler fetches another round from the ready crate. Fractions are of the drill
 * and come from the crew animation bake (gerpit_*_carry.json).
 */
USTRUCT(BlueprintType)
struct FGunPitShellCarrySettings
{
	GENERATED_BODY()

	// Static mesh components with this tag are the two rounds (sorted by name). None found: nothing is carried.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds")
	FName RoundTag = TEXT("PitRound");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds")
	int32 HandlerCrewIndex = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds")
	int32 LoaderCrewIndex = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds")
	FName HandBone = TEXT("hand_r");

	// The loader's hands close on the round the handler holds out.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds", meta = (ClampMin = "0", ClampMax = "1"))
	float HandoverFraction = 0.2f;

	// The round disappears into the breech.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds", meta = (ClampMin = "0", ClampMax = "1"))
	float RamFraction = 0.8f;

	// The handler lifts the next round from the ready crate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds", meta = (ClampMin = "0", ClampMax = "1"))
	float PickFraction = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds")
	FGunPitRoundPose CrateRound;

	// Where the handler holds his round while he waits; the first round starts there.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds")
	FGunPitRoundPose IdleRound;

	// The first round is put in the handler's hands once his idle pose has played for a moment.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit Rounds", meta = (ClampMin = "0"))
	float FirstRoundDelaySeconds = 0.3f;
};

/**
 * @brief Plays a gun pit's loading drill: when the turret starts a reload, every crew mesh plays its reload montage
 * (and the gun its own, if it has one), all stretched to the reload time so the round is home exactly when the gun
 * can fire again; the rounds move from the handler's hands to the loader's and into the breech on the way.
 * Crew meshes are skeletal mesh components tagged CrewTag (ordered by component name); props that ride on the gun
 * are tagged PropSocketTagPrefix + "<GunSocket>". Both are attached at runtime, because blueprint components cannot
 * be parented to a socket of the inherited turret mesh.
 * @note InitPitCrew: call in the turret blueprint's BeginPlay with the gun mesh.
 * @note OnReloadStarted: call from the turret blueprint's ReloadWeapon event.
 * @note OnGunFired: call from the turret blueprint's PlaySingleFireAnimation.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UGunPitCrewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGunPitCrewComponent();

	/**
	 * @brief Attaches the tagged crew, props and rounds to the gun and hands every crew anim instance its idle loop;
	 * the crew are custom animation players, not squad units.
	 * @param GunMesh Turret mesh running the pit anim blueprint (needs the DefaultSlot and CrewAttachSocket).
	 */
	UFUNCTION(BlueprintCallable, Category = "Gun Pit")
	void InitPitCrew(USkeletalMeshComponent* GunMesh);

	UFUNCTION(BlueprintCallable, Category = "Gun Pit")
	void OnGunFired();

	UFUNCTION(BlueprintCallable, Category = "Gun Pit")
	void OnReloadStarted(const float ReloadTime);

	UFUNCTION(BlueprintPure, Category = "Gun Pit")
	bool GetIsLoading() const { return bM_IsLoading; }

	// The round the loader is carrying or the handler is holding out; null when no rounds are carried.
	UStaticMeshComponent* GetRoundInFlight() const;

	FName GetCrewTag() const { return CrewTag; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	FGunPitReloadSettings ReloadSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	FGunPitShellCarrySettings ShellCarry;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	FName CrewTag = TEXT("PitCrew");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	FString PropSocketTagPrefix = TEXT("PitSocket:");

	// Gun socket or bone the crew ride on; the crew animations are authored in its frame.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	FName CrewAttachSocket = TEXT("PitOrigin");

	// False: the crew snap onto CrewAttachSocket (their animations carry their spot). True: the crew keep their
	// blueprint transform as an offset from it (team weapon crew animations played at a crew spot).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit")
	bool bKeepCrewBlueprintOffset = false;

	// How hard the crew brace at every shot (UGunPitCrewAnimInstance::PlayFireFlinch); 0 for a machine gun crew
	// whose gun fires in bursts.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gun Pit", meta = (ClampMin = "0"))
	float CrewFireFlinchStrength = 1.f;

private:
	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> M_GunMesh;

	UPROPERTY()
	TArray<TWeakObjectPtr<USkeletalMeshComponent>> M_CrewMeshes;

	// The two rounds that change hands, in name order.
	UPROPERTY()
	TArray<TWeakObjectPtr<UStaticMeshComponent>> M_Rounds;

	// Index in M_Rounds of the round the handler holds or the loader is carrying to the breech.
	int32 M_RoundInFlightIndex = 0;

	// True once this drill's round went into the gun; a drill may ram before the handler picks the next round
	// (the Flak loader pushes the clip in early), and the pick then takes the round now in flight.
	bool bM_RammedThisDrill = false;

	FTimerHandle M_LoadingFinishedTimer;
	FTimerHandle M_FirstRoundTimer;
	FTimerHandle M_HandoverTimer;
	FTimerHandle M_RamTimer;
	FTimerHandle M_PickTimer;

	bool bM_IsLoading = false;

	void InitPitCrew_GatherAndAttachCrew();
	void InitPitCrew_AttachTaggedProps() const;
	void InitPitCrew_GatherRounds();
	void PlayGunMontage(UAnimMontage* Montage, const float PlayRate, const float StartPosition) const;
	void PlayCrewLoadingMontages(const float DrillSeconds, const float StartPosition) const;
	void SetCrewIdleAnimations() const;
	void PlayCrewFireMontages() const;
	// The gun bucks (UGunPitAnimInstance::PlayFireKick) and the crew brace at a shot.
	void PlayGunFireKick() const;
	void PlayCrewFireFlinch() const;
	static void PlayMontageOnMesh(const USkeletalMeshComponent* Mesh, UAnimMontage* Montage, const float PlayRate,
	                              const float StartPosition);

	// Where the recoil montage of the last shot is; 0 when it is not playing.
	float GetPlayingFireMontagePosition() const;

	/** @return Play rate that plays the montage from StartPosition to its end in exactly Seconds. */
	static float GetStretchedPlayRate(const UAnimMontage& Montage, const float StartPosition, const float Seconds);

	/** @return The longest crew reload montage; the drill length when the gun has no reload montage of its own. */
	float GetLongestCrewMontageLength() const;
	void OnLoadingFinished();

	bool GetHasShellCarry() const;
	void ScheduleShellCarry(const float DrillSeconds);
	void PlaceFirstRound();
	void OnRoundHandedOver();
	void OnRoundRammed();
	void OnNextRoundPicked();
	void PlaceRoundOnGun(UStaticMeshComponent& Round, const FGunPitRoundPose& Pose) const;
	void AttachRoundToCrewHand(UStaticMeshComponent& Round, const int32 CrewIndex) const;

	bool GetIsValidGunMesh() const;
};
