// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "TeamWeaponRecoilProbe.h"

class ASquadUnit;
class ATeamWeapon;
class ATeamWeaponController;
class UAnimMontage;
class USkeletalMeshComponent;
class USquadUnitAnimInstance;

/** Min, max and mean of one measured quantity without keeping the samples; segments are many and short. */
struct FTWCrewAnimStat
{
	void Add(const float Value);
	bool GetHasValues() const { return Count > 0; }
	float GetMean() const { return Count > 0 ? Sum / Count : 0.f; }
	FString Describe() const;

	float Min = 0.f;
	float Max = 0.f;
	float Sum = 0.f;
	int32 Count = 0;
};

/** One play of a FullBody montage (or of no montage) on one operator, from its first to its last sampled frame. */
struct FTWCrewAnimSegment
{
	FString MontageName;
	float StartSeconds = 0.f;
	float LastSeconds = 0.f;
	float MontageLength = 0.f;
	FTWCrewAnimStat PlayRate;
	// Seconds until the montage weight first reached full; negative when it never did.
	float BlendInSeconds = -1.f;
	float LastPosition = 0.f;
	float LastWeight = 0.f;
	FString StateAtStart;
	FString StateAtEnd;
	int32 Samples = 0;
	// Distance of each hand (left, right) to the closest weapon collision.
	FTWCrewAnimStat HandGapCm[2];
	int32 HandContactSamples[2] = {0, 0};
	// Samples a body bone was inside the weapon collision, per bone name.
	TMap<FName, int32> BoneInsideWeaponSamples;
	// Horizontal speed of a planted foot; a planted foot that moves reads as skating.
	FTWCrewAnimStat PlantedFootSlideCmPerSec;
	float PlantedFootSlideCm = 0.f;
	FTWCrewAnimStat FootHeightCm;
	// Largest one-frame jump of any tracked bone in mesh space in the first frames after the switch to this segment.
	float SwitchPopCm = 0.f;
	FString SwitchFrom;
	// Largest one-frame jump after the switch window; loop seams and section jumps show up here.
	float MidPopCm = 0.f;
	float MidPopAtPosition = 0.f;
	// Largest one-frame actor yaw change in the switch window: the crew snap turning the soldier.
	float SwitchActorYawSnapDegrees = 0.f;
	float SwitchActorMoveSnapCm = 0.f;
	// Pelvis offset from its start position in mesh space: an in-place clip should keep this small.
	FVector PelvisStartMeshSpace = FVector::ZeroVector;
	float MaxPelvisDriftCm = 0.f;
	float CapsuleTravelCm = 0.f;
	FVector CapsuleStart = FVector::ZeroVector;
	// Actor yaw minus weapon yaw at the start and the end; a crew montage should end facing downrange.
	float YawToWeaponAtStart = 0.f;
	float YawToWeaponAtEnd = 0.f;
};

/** One crew member of an audited team weapon and the montage segment it is in. */
struct FTWCrewAnimOperatorTrack
{
	TWeakObjectPtr<ASquadUnit> Operator;
	FString Label;
	FString Role;
	bool bIsOnRightSide = false;
	TWeakObjectPtr<UAnimMontage> ActiveMontage;
	FTWCrewAnimSegment Segment;
	bool bHasSegment = false;
	// Mesh space bone positions of the previous sample, in the audit's tracked bone order.
	TArray<FVector> LastMeshSpaceBones;
	FVector LastFootWorld[2];
	FVector LastActorLocation = FVector::ZeroVector;
	float LastActorYaw = 0.f;
	float LastSampleSeconds = 0.f;
	bool bHasLastSample = false;
	int32 ReactStarts = 0;
};

/** A spawned team weapon squad whose operators the audit samples every frame. */
struct FTWCrewAnimSubjectTrack
{
	FString Name;
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;
	TWeakObjectPtr<ATeamWeaponController> Controller;
	TArray<FTWCrewAnimOperatorTrack> Operators;
	TWeakObjectPtr<UAnimMontage> GunMontage;
	float GunMontageStartSeconds = 0.f;
	bool bWasReloading = false;
	float ReloadStartSeconds = 0.f;
	int32 ReloadCount = 0;
	FString LastState;
	float LastStateChangeSeconds = 0.f;
	bool bLoggedSetup = false;
	bool bLoggedDeployedGeometry = false;
	// Montage name to what it is for this subject (Loop_Gunner, React_Loader, Deploy_R, ...); from the setup table.
	TMap<FString, FString> MontageKinds;
	bool bWroteWeaponCsvHeader = false;
	TArray<FName> WeaponBoneNames;
	// Per-shot fire feedback of the gun mesh, measured against the barrel direction.
	FTeamWeaponRecoilProbe RecoilProbe;
};

/**
 * @brief Measures the crew montages of team weapons without rendering: which clip plays when, how it is stretched,
 * how it blends, where the hands are against the gun, skating feet and pose pops at every switch.
 * Owned by the team weapon quality test (-TWQualityCrewAnims); writes RTS_TWQC crew_* lines and pose CSVs.
 */
class FTeamWeaponCrewAnimAudit
{
public:
	/** @param OutputDirectory Folder for the pose CSVs; empty writes only log lines. */
	void Initialise(const FString& OutputDirectory);
	void RegisterSubject(const FString& Name, const ESquadSubtype SquadSubtype, ATeamWeaponController* Controller);
	void Tick(const float GameSeconds);
	void Finish(const float GameSeconds);

	/** @return Reload starts of weapon 0 seen on the subject so far; -1 when the subject is unknown. */
	int32 GetReloadCount(const FString& SubjectName) const;
	bool GetIsReloading(const FString& SubjectName) const;

private:
	FString M_OutputDirectory;
	TArray<FTWCrewAnimSubjectTrack> M_Subjects;
	TArray<FString> M_PendingCrewCsvLines;
	TMap<FString, TArray<FString>> M_PendingWeaponCsvLines;
	TSet<FString> M_LoggedAnimClasses;
	bool bM_WroteCrewCsvHeader = false;

	void TickSubject(FTWCrewAnimSubjectTrack& Subject, const float GameSeconds);
	void RefreshOperators(FTWCrewAnimSubjectTrack& Subject) const;
	void TickReload(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon, const float GameSeconds) const;
	void TickGunMontage(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon, const float GameSeconds) const;

	/**
	 * @brief Samples one operator: closes and opens montage segments on a switch and adds the pose measurements.
	 * @param Subject Owning subject, for names, state and the weapon.
	 * @param Track Operator to sample.
	 * @param Weapon The subject's gun.
	 * @param GameSeconds Current game time.
	 */
	void SampleOperator(FTWCrewAnimSubjectTrack& Subject, FTWCrewAnimOperatorTrack& Track, const ATeamWeapon& Weapon,
	                    const float GameSeconds);
	void OpenSegment(FTWCrewAnimSubjectTrack& Subject, FTWCrewAnimOperatorTrack& Track, UAnimMontage* Montage,
	                 const ATeamWeapon& Weapon, const float GameSeconds) const;
	void CloseSegment(const FTWCrewAnimSubjectTrack& Subject, FTWCrewAnimOperatorTrack& Track,
	                  const ATeamWeapon* Weapon) const;
	void SamplePose(FTWCrewAnimSubjectTrack& Subject, FTWCrewAnimOperatorTrack& Track, const ATeamWeapon& Weapon,
	                const USkeletalMeshComponent& Mesh, const float GameSeconds);
	static float GetDistanceToWeaponCollision(const ATeamWeapon& Weapon, const FVector& WorldPoint);
	// Pose state, montage instances per slot and mesh update settings when a segment starts; explains frozen clips.
	static void LogOperatorAnimState(const FTWCrewAnimSubjectTrack& Subject, const FTWCrewAnimOperatorTrack& Track,
	                                 const float GameSeconds);

	void LogSubjectSetup(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon);
	void LogAnimClassTable(const USquadUnitAnimInstance& AnimInstance);
	// Physics bodies, collision meshes and sockets of the deployed gun in its local space, for the pose plots.
	void LogDeployedGeometry(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon);
	void WriteWeaponCsvRow(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon, const float GameSeconds);
	void FlushCsv(const bool bForce);
};
