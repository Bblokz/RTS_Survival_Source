// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TeamWeaponCrewAnimAudit.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/TeamWeapons/CrewPositions/CrewPosition.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTeamWeaponCrewAnimAudit, Log, All);

namespace TeamWeaponCrewAnimAuditPrivate
{
	// Mannequin bones tracked on every operator; the body bones also count as clipping when inside the gun.
	const FName TrackedBones[] = {
		TEXT("pelvis"), TEXT("spine_03"), TEXT("head"),
		TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
		TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
		TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
		TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")
	};
	const FName BodyBonesForClipping[] = {
		TEXT("pelvis"), TEXT("spine_03"), TEXT("head"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"),
		TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r")
	};
	const FName PelvisBone(TEXT("pelvis"));
	const FName HandBones[] = {TEXT("hand_l"), TEXT("hand_r")};
	const FName FootBones[] = {TEXT("foot_l"), TEXT("foot_r")};

	// Every team weapon squad subtype, so the montage table also covers guns the async spawner cannot spawn.
	const ESquadSubtype TeamWeaponSubtypes[] = {
		ESquadSubtype::Squad_Ger_PaK38, ESquadSubtype::Squad_Ger_PaK40, ESquadSubtype::Squad_Rus_Zis_57MM,
		ESquadSubtype::Squad_Rus_Zis_76MM, ESquadSubtype::Squad_Rus_53K_45MM, ESquadSubtype::Squad_Rus_BS3_100MM,
		ESquadSubtype::Squad_Ger_37mmFlak, ESquadSubtype::Squad_Ger_88mmFlak, ESquadSubtype::Squad_Rus_Bofors,
		ESquadSubtype::Squad_Rus_KS30_130MM, ESquadSubtype::Squad_Ger_GrW42_80mm, ESquadSubtype::Squad_Rus_80mm_Mortar,
		ESquadSubtype::Squad_Rus_120mm_Mortar, ESquadSubtype::Squad_Ger_LefH18, ESquadSubtype::Squad_Ger_210MM_Morser,
		ESquadSubtype::Squad_Ger_SFH18_150mm, ESquadSubtype::Squad_Ger_Nebelwerfer, ESquadSubtype::Squad_Rus_M1938_122mm,
		ESquadSubtype::Squad_Rus_B4_203mm, ESquadSubtype::Squad_Rus_ML20_152mm, ESquadSubtype::Squad_Rus_Maxim
	};
	const ECrewPositionType CrewRoles[] = {
		ECrewPositionType::Gunner, ECrewPositionType::Loader, ECrewPositionType::Spotter,
		ECrewPositionType::AdditionalLoader, ECrewPositionType::Commander
	};

	// Same clamp as USquadUnitAnimInstance::PlayTeamWeaponCrewTransition.
	constexpr float MinTransitionPlayRate = 0.6f;
	constexpr float MaxTransitionPlayRate = 1.75f;
	constexpr float FullWeight = 0.99f;
	constexpr float SwitchWindowSeconds = 0.35f;
	constexpr float HandContactCm = 6.f;
	// A foot counts as planted within this height of the lowest it was in the segment (the foot bone is the ankle).
	constexpr float PlantedFootAboveLowestCm = 3.f;
	constexpr float PlantedFootMaxVerticalSpeed = 20.f;
	// An actor moving further than this in one frame was teleported (crew snap); its feet did not skate.
	constexpr float TeleportFrameMoveCm = 15.f;
	constexpr float CrewPostMatchToleranceCm = 2.f;
	constexpr float MinSampleDeltaSeconds = 0.001f;
	constexpr int32 CsvFlushLineCount = 4000;
	constexpr int32 MaxWeaponCsvBones = 80;
	const TCHAR* const CrewCsvName = TEXT("crew_pose.csv");

	FString GetMontageName(const UAnimMontage* Montage)
	{
		return IsValid(Montage) ? Montage->GetName() : TEXT("none");
	}

	FString GetRoleName(const ECrewPositionType Role)
	{
		FString Name = UEnum::GetValueAsString(Role);
		Name.RemoveFromStart(TEXT("ECrewPositionType::"));
		return Name;
	}

	FString GetSubtypeName(const ESquadSubtype Subtype)
	{
		FString Name = UEnum::GetValueAsString(Subtype);
		Name.RemoveFromStart(TEXT("ESquadSubtype::"));
		return Name;
	}

	FString GetStateName(const ATeamWeaponController& Controller)
	{
		FString Name = UEnum::GetValueAsString(Controller.GetTeamWeaponState());
		Name.RemoveFromStart(TEXT("ETeamWeaponState::"));
		return Name;
	}

	FString DescribeMontage(const UAnimMontage* Montage)
	{
		if (not IsValid(Montage))
		{
			return TEXT("none");
		}
		return FString::Printf(TEXT("%s(len=%.2f rate_scale=%.2f blend_in=%.2f blend_out=%.2f sections=%d root_motion=%d)"),
		                       *Montage->GetName(), Montage->GetPlayLength(), Montage->RateScale,
		                       Montage->BlendIn.GetBlendTime(), Montage->BlendOut.GetBlendTime(),
		                       Montage->GetNumSections(), Montage->HasRootMotion() ? 1 : 0);
	}

	const FTeamWeaponCrewMontages* GetCrewMontages(const USquadUnitAnimInstance& AnimInstance)
	{
		// Protected designer data; the audit only reads it.
		const FStructProperty* Property = FindFProperty<FStructProperty>(
			USquadUnitAnimInstance::StaticClass(), TEXT("TeamWeaponCrewMontages"));
		if (Property == nullptr)
		{
			return nullptr;
		}
		return Property->ContainerPtrToValuePtr<FTeamWeaponCrewMontages>(
			const_cast<USquadUnitAnimInstance*>(&AnimInstance));
	}

	const FTeamWeaponCrewTransitionMontages* GetTransitionMontages(const USquadUnitAnimInstance& AnimInstance)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(
			USquadUnitAnimInstance::StaticClass(), TEXT("TeamWeaponCrewTransitionMontages"));
		if (Property == nullptr)
		{
			return nullptr;
		}
		return Property->ContainerPtrToValuePtr<FTeamWeaponCrewTransitionMontages>(
			const_cast<USquadUnitAnimInstance*>(&AnimInstance));
	}

	USquadUnitAnimInstance* GetAnimInstance(const ASquadUnit* Operator)
	{
		return IsValid(Operator) ? Operator->GetAnimBP_SquadUnit() : nullptr;
	}

	UWeaponState* GetPrimaryWeapon(ATeamWeapon& Weapon)
	{
		TArray<UWeaponState*> Weapons = Weapon.GetWeapons();
		return Weapons.Num() > 0 ? Weapons[0] : nullptr;
	}

	float GetStretchedRate(const UAnimMontage* Montage, const float Seconds, const float RateMultiplier)
	{
		if (not IsValid(Montage) || Seconds <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}
		const float SafeRateScale = FMath::Max(Montage->RateScale, KINDA_SMALL_NUMBER);
		return Montage->GetPlayLength() / (Seconds * SafeRateScale) * RateMultiplier;
	}

	float GetYawDelta(const float FromYaw, const float ToYaw)
	{
		return FRotator::NormalizeAxis(ToYaw - FromYaw);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Stats

void FTWCrewAnimStat::Add(const float Value)
{
	Min = Count == 0 ? Value : FMath::Min(Min, Value);
	Max = Count == 0 ? Value : FMath::Max(Max, Value);
	Sum += Value;
	Count++;
}

FString FTWCrewAnimStat::Describe() const
{
	if (not GetHasValues())
	{
		return TEXT("n=0");
	}
	return FString::Printf(TEXT("min=%.1f mean=%.1f max=%.1f"), Min, GetMean(), Max);
}

// ---------------------------------------------------------------------------------------------------------------
// Public

void FTeamWeaponCrewAnimAudit::Initialise(const FString& OutputDirectory)
{
	M_OutputDirectory = OutputDirectory;
	if (M_OutputDirectory.IsEmpty())
	{
		return;
	}
	IFileManager::Get().MakeDirectory(*M_OutputDirectory, true);
	IFileManager::Get().Delete(*FPaths::Combine(M_OutputDirectory, TeamWeaponCrewAnimAuditPrivate::CrewCsvName));
}

void FTeamWeaponCrewAnimAudit::RegisterSubject(const FString& Name, const ESquadSubtype SquadSubtype,
                                               ATeamWeaponController* Controller)
{
	FTWCrewAnimSubjectTrack& Subject = M_Subjects.AddDefaulted_GetRef();
	Subject.Name = Name;
	Subject.SquadSubtype = SquadSubtype;
	Subject.Controller = Controller;
}

void FTeamWeaponCrewAnimAudit::Tick(const float GameSeconds)
{
	for (FTWCrewAnimSubjectTrack& Subject : M_Subjects)
	{
		TickSubject(Subject, GameSeconds);
	}
	FlushCsv(false);
}

void FTeamWeaponCrewAnimAudit::Finish(const float GameSeconds)
{
	for (FTWCrewAnimSubjectTrack& Subject : M_Subjects)
	{
		ATeamWeaponController* Controller = Subject.Controller.Get();
		const ATeamWeapon* Weapon = IsValid(Controller) ? Controller->GetControlledTeamWeaponNoReport() : nullptr;
		for (FTWCrewAnimOperatorTrack& Track : Subject.Operators)
		{
			CloseSegment(Subject, Track, Weapon);
			UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
			       TEXT("RTS_TWQC crew_summary subject=%s op=%s role=%s side=%s react_starts=%d"),
			       *Subject.Name, *Track.Label, *Track.Role, Track.bIsOnRightSide ? TEXT("R") : TEXT("L"),
			       Track.ReactStarts);
		}
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display, TEXT("RTS_TWQC crew_summary subject=%s reloads=%d t=%.1f"),
		       *Subject.Name, Subject.ReloadCount, GameSeconds);
	}
	FlushCsv(true);
}

int32 FTeamWeaponCrewAnimAudit::GetReloadCount(const FString& SubjectName) const
{
	for (const FTWCrewAnimSubjectTrack& Subject : M_Subjects)
	{
		if (Subject.Name == SubjectName)
		{
			return Subject.ReloadCount;
		}
	}
	return INDEX_NONE;
}

bool FTeamWeaponCrewAnimAudit::GetIsReloading(const FString& SubjectName) const
{
	for (const FTWCrewAnimSubjectTrack& Subject : M_Subjects)
	{
		if (Subject.Name == SubjectName)
		{
			return Subject.bWasReloading;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------
// Subjects

void FTeamWeaponCrewAnimAudit::TickSubject(FTWCrewAnimSubjectTrack& Subject, const float GameSeconds)
{
	ATeamWeaponController* Controller = Subject.Controller.Get();
	ATeamWeapon* Weapon = IsValid(Controller) ? Controller->GetControlledTeamWeaponNoReport() : nullptr;
	if (not IsValid(Weapon))
	{
		return;
	}
	const FString State = TeamWeaponCrewAnimAuditPrivate::GetStateName(*Controller);
	if (State != Subject.LastState)
	{
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display, TEXT("RTS_TWQC crew_state subject=%s %s->%s after=%.2f t=%.2f"),
		       *Subject.Name, *Subject.LastState, *State, GameSeconds - Subject.LastStateChangeSeconds, GameSeconds);
		Subject.LastState = State;
		Subject.LastStateChangeSeconds = GameSeconds;
	}
	if (State == TEXT("Ready_Deployed") && not Subject.bLoggedDeployedGeometry)
	{
		LogDeployedGeometry(Subject, *Weapon);
	}
	RefreshOperators(Subject);
	if (not Subject.bLoggedSetup && Subject.Operators.Num() > 0)
	{
		LogSubjectSetup(Subject, *Weapon);
	}
	TickReload(Subject, *Weapon, GameSeconds);
	TickGunMontage(Subject, *Weapon, GameSeconds);
	bool bAnyCrewMontage = false;
	for (FTWCrewAnimOperatorTrack& Track : Subject.Operators)
	{
		SampleOperator(Subject, Track, *Weapon, GameSeconds);
		bAnyCrewMontage |= Track.ActiveMontage.IsValid();
	}
	if (bAnyCrewMontage || Subject.GunMontage.IsValid())
	{
		WriteWeaponCsvRow(Subject, *Weapon, GameSeconds);
	}
}

void FTeamWeaponCrewAnimAudit::RefreshOperators(FTWCrewAnimSubjectTrack& Subject) const
{
	using namespace TeamWeaponCrewAnimAuditPrivate;
	const ATeamWeaponController* Controller = Subject.Controller.Get();
	const ATeamWeapon* Weapon = Controller->GetControlledTeamWeaponNoReport();
	TArray<ASquadUnit*> Operators;
	Controller->GetCrewOperators(Operators);
	TInlineComponentArray<UCrewPosition*> CrewPositions(Weapon);
	for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
	{
		ASquadUnit* Operator = Operators[OperatorIndex];
		if (not IsValid(Operator))
		{
			continue;
		}
		const bool bIsTracked = Subject.Operators.ContainsByPredicate([Operator](const FTWCrewAnimOperatorTrack& Track)
		{
			return Track.Operator.Get() == Operator;
		});
		if (bIsTracked)
		{
			continue;
		}
		FTWCrewAnimOperatorTrack& Track = Subject.Operators.AddDefaulted_GetRef();
		Track.Operator = Operator;
		Track.Label = FString::FromInt(OperatorIndex);
		Track.Role = TEXT("None");
		FVector PostLocation;
		if (not Controller->TryGetOperatorCrewPositionLocation(OperatorIndex, PostLocation))
		{
			continue;
		}
		for (const UCrewPosition* CrewPosition : CrewPositions)
		{
			if (FVector::Dist(CrewPosition->GetComponentLocation(), PostLocation) > CrewPostMatchToleranceCm)
			{
				continue;
			}
			Track.Role = GetRoleName(CrewPosition->GetCrewPositionType());
			const FVector LocalPost = Weapon->GetActorTransform().InverseTransformPosition(PostLocation);
			Track.bIsOnRightSide = LocalPost.Y > 0.f;
			UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
			       TEXT("RTS_TWQC crew_post subject=%s op=%d role=%s side=%s local=(%.0f,%.0f,%.0f) yaw_local=%.0f"),
			       *Subject.Name, OperatorIndex, *Track.Role, Track.bIsOnRightSide ? TEXT("R") : TEXT("L"),
			       LocalPost.X, LocalPost.Y, LocalPost.Z,
			       GetYawDelta(Weapon->GetActorRotation().Yaw, CrewPosition->GetComponentRotation().Yaw));
			break;
		}
	}
}

void FTeamWeaponCrewAnimAudit::TickReload(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon,
                                          const float GameSeconds) const
{
	UWeaponState* PrimaryWeapon = TeamWeaponCrewAnimAuditPrivate::GetPrimaryWeapon(const_cast<ATeamWeapon&>(Weapon));
	if (not IsValid(PrimaryWeapon))
	{
		return;
	}
	const bool bIsReloading = PrimaryWeapon->GetIsReloading();
	if (bIsReloading == Subject.bWasReloading)
	{
		return;
	}
	Subject.bWasReloading = bIsReloading;
	if (bIsReloading)
	{
		Subject.ReloadCount++;
		Subject.ReloadStartSeconds = GameSeconds;
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display, TEXT("RTS_TWQC crew_reload subject=%s start=%d t=%.2f"),
		       *Subject.Name, Subject.ReloadCount, GameSeconds);
		return;
	}
	UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display, TEXT("RTS_TWQC crew_reload subject=%s end=%d seconds=%.2f t=%.2f"),
	       *Subject.Name, Subject.ReloadCount, GameSeconds - Subject.ReloadStartSeconds, GameSeconds);
}

void FTeamWeaponCrewAnimAudit::TickGunMontage(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon,
                                              const float GameSeconds) const
{
	const USkeletalMeshComponent* Mesh = Weapon.FindComponentByClass<USkeletalMeshComponent>();
	UAnimInstance* GunAnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontage* GunMontage = IsValid(GunAnimInstance) ? GunAnimInstance->GetCurrentActiveMontage() : nullptr;
	if (GunMontage == Subject.GunMontage.Get())
	{
		return;
	}
	if (Subject.GunMontage.IsValid())
	{
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display, TEXT("RTS_TWQC crew_gun subject=%s end=%s played=%.2f t=%.2f"),
		       *Subject.Name, *Subject.GunMontage->GetName(), GameSeconds - Subject.GunMontageStartSeconds,
		       GameSeconds);
	}
	Subject.GunMontage = GunMontage;
	Subject.GunMontageStartSeconds = GameSeconds;
	if (IsValid(GunMontage))
	{
		const FAnimMontageInstance* Instance = GunAnimInstance->GetActiveInstanceForMontage(GunMontage);
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
		       TEXT("RTS_TWQC crew_gun subject=%s start=%s rate=%.2f state=%s t=%.2f"),
		       *Subject.Name, *TeamWeaponCrewAnimAuditPrivate::DescribeMontage(GunMontage),
		       Instance != nullptr ? Instance->GetPlayRate() : 0.f, *Subject.LastState, GameSeconds);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Operators

void FTeamWeaponCrewAnimAudit::SampleOperator(FTWCrewAnimSubjectTrack& Subject, FTWCrewAnimOperatorTrack& Track,
                                              const ATeamWeapon& Weapon, const float GameSeconds)
{
	ASquadUnit* Operator = Track.Operator.Get();
	USquadUnitAnimInstance* AnimInstance = TeamWeaponCrewAnimAuditPrivate::GetAnimInstance(Operator);
	const USkeletalMeshComponent* Mesh = IsValid(Operator) ? Operator->GetMesh() : nullptr;
	if (not IsValid(AnimInstance) || not IsValid(Mesh) || not Operator->IsUnitAlive())
	{
		return;
	}
	const FAnimMontageInstance* Instance = AnimInstance->GetActiveMontageInstance();
	UAnimMontage* Montage = Instance != nullptr ? Instance->Montage.Get() : nullptr;
	if (not Track.bHasSegment || Montage != Track.ActiveMontage.Get())
	{
		const FString PreviousName = Track.bHasSegment ? Track.Segment.MontageName : TEXT("start");
		CloseSegment(Subject, Track, &Weapon);
		OpenSegment(Subject, Track, Montage, Weapon, GameSeconds);
		Track.Segment.SwitchFrom = PreviousName;
	}
	FTWCrewAnimSegment& Segment = Track.Segment;
	Segment.LastSeconds = GameSeconds;
	Segment.StateAtEnd = Subject.LastState;
	if (Instance != nullptr)
	{
		Segment.PlayRate.Add(Instance->GetPlayRate());
		Segment.LastPosition = Instance->GetPosition();
		Segment.LastWeight = Instance->GetWeight();
		if (Segment.BlendInSeconds < 0.f && Instance->GetWeight() >= TeamWeaponCrewAnimAuditPrivate::FullWeight)
		{
			Segment.BlendInSeconds = GameSeconds - Segment.StartSeconds;
		}
	}
	SamplePose(Subject, Track, Weapon, *Mesh, GameSeconds);
}

void FTeamWeaponCrewAnimAudit::OpenSegment(FTWCrewAnimSubjectTrack& Subject, FTWCrewAnimOperatorTrack& Track,
                                           UAnimMontage* Montage, const ATeamWeapon& Weapon,
                                           const float GameSeconds) const
{
	Track.ActiveMontage = Montage;
	Track.bHasSegment = true;
	Track.Segment = FTWCrewAnimSegment();
	FTWCrewAnimSegment& Segment = Track.Segment;
	Segment.MontageName = TeamWeaponCrewAnimAuditPrivate::GetMontageName(Montage);
	Segment.MontageLength = IsValid(Montage) ? Montage->GetPlayLength() : 0.f;
	Segment.StartSeconds = GameSeconds;
	Segment.LastSeconds = GameSeconds;
	Segment.StateAtStart = Subject.LastState;
	Segment.StateAtEnd = Subject.LastState;
	const ASquadUnit* Operator = Track.Operator.Get();
	Segment.CapsuleStart = Operator->GetActorLocation();
	Segment.YawToWeaponAtStart = TeamWeaponCrewAnimAuditPrivate::GetYawDelta(
		Weapon.GetActorRotation().Yaw, Operator->GetActorRotation().Yaw);
	if (const FString* Kind = Subject.MontageKinds.Find(Segment.MontageName))
	{
		Track.ReactStarts += Kind->StartsWith(TEXT("React")) ? 1 : 0;
	}
	LogOperatorAnimState(Subject, Track, GameSeconds);
}

void FTeamWeaponCrewAnimAudit::LogOperatorAnimState(const FTWCrewAnimSubjectTrack& Subject,
                                                    const FTWCrewAnimOperatorTrack& Track, const float GameSeconds)
{
	const USquadUnitAnimInstance* AnimInstance = TeamWeaponCrewAnimAuditPrivate::GetAnimInstance(Track.Operator.Get());
	if (not IsValid(AnimInstance))
	{
		return;
	}
	// Every montage instance with its slot: a second montage in a later slot can hide the crew montage.
	FString Instances;
	for (const FAnimMontageInstance* Instance : AnimInstance->MontageInstances)
	{
		if (Instance == nullptr || not IsValid(Instance->Montage))
		{
			continue;
		}
		const FName SlotName = Instance->Montage->SlotAnimTracks.Num() > 0
			                       ? Instance->Montage->SlotAnimTracks[0].SlotName
			                       : NAME_None;
		Instances += FString::Printf(TEXT("%s@%s:w%.2f:pos%.2f:%s,"), *Instance->Montage->GetName(),
		                             *SlotName.ToString(), Instance->GetWeight(), Instance->GetPosition(),
		                             Instance->IsActive() ? TEXT("active") : TEXT("out"));
	}
	const USkeletalMeshComponent* Mesh = Track.Operator->GetMesh();
	UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
	       TEXT("RTS_TWQC crew_animstate subject=%s op=%s t=%.2f instances={%s} lod=%d uro=%d tick_opt=%d %s"),
	       *Subject.Name, *Track.Label, GameSeconds, *Instances,
	       IsValid(Mesh) ? Mesh->GetPredictedLODLevel() : -1,
	       IsValid(Mesh) ? (Mesh->bEnableUpdateRateOptimizations ? 1 : 0) : -1,
	       IsValid(Mesh) ? static_cast<int32>(Mesh->VisibilityBasedAnimTickOption) : -1,
	       *AnimInstance->GetPoseDebugString());
}

void FTeamWeaponCrewAnimAudit::CloseSegment(const FTWCrewAnimSubjectTrack& Subject, FTWCrewAnimOperatorTrack& Track,
                                            const ATeamWeapon* Weapon) const
{
	if (not Track.bHasSegment)
	{
		return;
	}
	Track.bHasSegment = false;
	FTWCrewAnimSegment& Segment = Track.Segment;
	const ASquadUnit* Operator = Track.Operator.Get();
	if (IsValid(Operator) && IsValid(Weapon))
	{
		Segment.CapsuleTravelCm = FVector::Dist2D(Operator->GetActorLocation(), Segment.CapsuleStart);
		Segment.YawToWeaponAtEnd = TeamWeaponCrewAnimAuditPrivate::GetYawDelta(
			Weapon->GetActorRotation().Yaw, Operator->GetActorRotation().Yaw);
	}
	FString InsideBones;
	for (const TPair<FName, int32>& Inside : Segment.BoneInsideWeaponSamples)
	{
		InsideBones += FString::Printf(TEXT("%s:%d,"), *Inside.Key.ToString(), Inside.Value);
	}
	const FString* Kind = Subject.MontageKinds.Find(Segment.MontageName);
	const float SafeSamples = FMath::Max(1, Segment.Samples);
	UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
	       TEXT("RTS_TWQC crew_seg subject=%s op=%s role=%s side=%s montage=%s kind=%s t=%.2f-%.2f dur=%.2f len=%.2f "
		       "rate[%s] blend_in=%.2f end_pos=%.2f end_weight=%.2f state=%s->%s switch_from=%s switch_pop_cm=%.1f "
		       "snap_yaw=%.1f snap_move=%.1f mid_pop_cm=%.1f@%.2f hand_l[%s] hand_r[%s] contact_l=%.2f contact_r=%.2f "
		       "inside={%s} foot_slide_cmps[%s] slide_cm=%.1f foot_h[%s] pelvis_drift=%.1f capsule_travel=%.1f "
		       "yaw_to_weapon=%.0f->%.0f samples=%d"),
	       *Subject.Name, *Track.Label, *Track.Role, Track.bIsOnRightSide ? TEXT("R") : TEXT("L"), *Segment.MontageName,
	       Kind != nullptr ? **Kind : TEXT("Other"), Segment.StartSeconds, Segment.LastSeconds,
	       Segment.LastSeconds - Segment.StartSeconds, Segment.MontageLength, *Segment.PlayRate.Describe(),
	       Segment.BlendInSeconds, Segment.LastPosition, Segment.LastWeight, *Segment.StateAtStart,
	       *Segment.StateAtEnd, *Segment.SwitchFrom, Segment.SwitchPopCm, Segment.SwitchActorYawSnapDegrees,
	       Segment.SwitchActorMoveSnapCm, Segment.MidPopCm, Segment.MidPopAtPosition,
	       *Segment.HandGapCm[0].Describe(), *Segment.HandGapCm[1].Describe(),
	       Segment.HandContactSamples[0] / SafeSamples, Segment.HandContactSamples[1] / SafeSamples, *InsideBones,
	       *Segment.PlantedFootSlideCmPerSec.Describe(), Segment.PlantedFootSlideCm, *Segment.FootHeightCm.Describe(),
	       Segment.MaxPelvisDriftCm, Segment.CapsuleTravelCm, Segment.YawToWeaponAtStart, Segment.YawToWeaponAtEnd,
	       Segment.Samples);
}

void FTeamWeaponCrewAnimAudit::SamplePose(FTWCrewAnimSubjectTrack& Subject, FTWCrewAnimOperatorTrack& Track,
                                          const ATeamWeapon& Weapon, const USkeletalMeshComponent& Mesh,
                                          const float GameSeconds)
{
	using namespace TeamWeaponCrewAnimAuditPrivate;
	FTWCrewAnimSegment& Segment = Track.Segment;
	const ASquadUnit* Operator = Track.Operator.Get();
	const FTransform MeshTransform = Mesh.GetComponentTransform();
	const FTransform WeaponTransform = Weapon.GetActorTransform();
	const float DeltaSeconds = FMath::Max(GameSeconds - Track.LastSampleSeconds, MinSampleDeltaSeconds);
	const bool bIsSwitchWindow = GameSeconds - Segment.StartSeconds <= SwitchWindowSeconds;

	TArray<FVector> MeshSpaceBones;
	MeshSpaceBones.Reserve(UE_ARRAY_COUNT(TrackedBones));
	FString CsvBones;
	float LargestJumpCm = 0.f;
	for (int32 BoneIndex = 0; BoneIndex < UE_ARRAY_COUNT(TrackedBones); ++BoneIndex)
	{
		const FVector World = Mesh.GetBoneIndex(TrackedBones[BoneIndex]) != INDEX_NONE
			                      ? Mesh.GetBoneLocation(TrackedBones[BoneIndex], EBoneSpaces::WorldSpace)
			                      : MeshTransform.GetLocation();
		const FVector MeshSpace = MeshTransform.InverseTransformPosition(World);
		MeshSpaceBones.Add(MeshSpace);
		if (Track.bHasLastSample && Track.LastMeshSpaceBones.IsValidIndex(BoneIndex))
		{
			LargestJumpCm = FMath::Max(LargestJumpCm,
			                           static_cast<float>(FVector::Dist(MeshSpace, Track.LastMeshSpaceBones[BoneIndex])));
		}
		const FVector WeaponLocal = WeaponTransform.InverseTransformPosition(World);
		CsvBones += FString::Printf(TEXT(",%.1f,%.1f,%.1f"), WeaponLocal.X, WeaponLocal.Y, WeaponLocal.Z);
	}

	const FVector ActorLocation = Operator->GetActorLocation();
	const float ActorYaw = Operator->GetActorRotation().Yaw;
	const float ActorFrameMoveCm = Track.bHasLastSample
		                               ? static_cast<float>(FVector::Dist(ActorLocation, Track.LastActorLocation))
		                               : 0.f;
	if (Track.bHasLastSample)
	{
		if (bIsSwitchWindow)
		{
			Segment.SwitchPopCm = FMath::Max(Segment.SwitchPopCm, LargestJumpCm);
			Segment.SwitchActorMoveSnapCm = FMath::Max(Segment.SwitchActorMoveSnapCm, ActorFrameMoveCm);
			Segment.SwitchActorYawSnapDegrees = FMath::Max(Segment.SwitchActorYawSnapDegrees,
			                                               FMath::Abs(GetYawDelta(Track.LastActorYaw, ActorYaw)));
		}
		else if (LargestJumpCm > Segment.MidPopCm)
		{
			Segment.MidPopCm = LargestJumpCm;
			Segment.MidPopAtPosition = Segment.LastPosition;
		}
	}

	// Pelvis drift from where it started in this segment.
	const int32 PelvisIndex = 0;
	if (Segment.Samples == 0)
	{
		Segment.PelvisStartMeshSpace = MeshSpaceBones[PelvisIndex];
	}
	Segment.MaxPelvisDriftCm = FMath::Max(Segment.MaxPelvisDriftCm, static_cast<float>(FVector::Dist2D(
		                                      MeshSpaceBones[PelvisIndex], Segment.PelvisStartMeshSpace)));

	// Hands against the gun.
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		if (Mesh.GetBoneIndex(HandBones[HandIndex]) == INDEX_NONE)
		{
			continue;
		}
		const float GapCm = GetDistanceToWeaponCollision(
			Weapon, Mesh.GetBoneLocation(HandBones[HandIndex], EBoneSpaces::WorldSpace));
		if (GapCm < 0.f)
		{
			continue;
		}
		Segment.HandGapCm[HandIndex].Add(GapCm);
		Segment.HandContactSamples[HandIndex] += GapCm <= HandContactCm ? 1 : 0;
	}
	for (const FName& BodyBone : BodyBonesForClipping)
	{
		if (Mesh.GetBoneIndex(BodyBone) == INDEX_NONE)
		{
			continue;
		}
		if (GetDistanceToWeaponCollision(Weapon, Mesh.GetBoneLocation(BodyBone, EBoneSpaces::WorldSpace)) == 0.f)
		{
			Segment.BoneInsideWeaponSamples.FindOrAdd(BodyBone)++;
		}
	}

	// Feet: height over the capsule bottom and skating while planted.
	const UCapsuleComponent* Capsule = Operator->GetCapsuleComponent();
	const float CapsuleBottomZ = ActorLocation.Z - (IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
	for (int32 FootIndex = 0; FootIndex < 2; ++FootIndex)
	{
		const FVector FootWorld = Mesh.GetBoneLocation(FootBones[FootIndex], EBoneSpaces::WorldSpace);
		const float FootHeightCm = FootWorld.Z - CapsuleBottomZ;
		Segment.FootHeightCm.Add(FootHeightCm);
		if (Track.bHasLastSample && ActorFrameMoveCm < TeleportFrameMoveCm)
		{
			const float VerticalSpeed = FMath::Abs(FootWorld.Z - Track.LastFootWorld[FootIndex].Z) / DeltaSeconds;
			if (FootHeightCm <= Segment.FootHeightCm.Min + PlantedFootAboveLowestCm && VerticalSpeed <= PlantedFootMaxVerticalSpeed)
			{
				const float SlideCm = FVector::Dist2D(FootWorld, Track.LastFootWorld[FootIndex]);
				Segment.PlantedFootSlideCmPerSec.Add(SlideCm / DeltaSeconds);
				Segment.PlantedFootSlideCm += SlideCm;
			}
		}
		Track.LastFootWorld[FootIndex] = FootWorld;
	}

	Track.LastMeshSpaceBones = MoveTemp(MeshSpaceBones);
	Track.LastActorLocation = ActorLocation;
	Track.LastActorYaw = ActorYaw;
	Track.LastSampleSeconds = GameSeconds;
	Track.bHasLastSample = true;
	Segment.Samples++;

	if (M_OutputDirectory.IsEmpty() || not Track.ActiveMontage.IsValid())
	{
		return;
	}
	const FString* Kind = Subject.MontageKinds.Find(Segment.MontageName);
	const FVector ActorLocal = WeaponTransform.InverseTransformPosition(ActorLocation);
	M_PendingCrewCsvLines.Add(FString::Printf(TEXT("%.3f,%s,%s,%s,%s,%s,%s,%.3f,%.3f,%.3f,%s,%.1f,%.1f,%.1f,%.1f%s"),
	                                          GameSeconds, *Subject.Name, *Track.Label, *Track.Role,
	                                          Track.bIsOnRightSide ? TEXT("R") : TEXT("L"), *Segment.MontageName,
	                                          Kind != nullptr ? **Kind : TEXT("Other"), Segment.LastPosition,
	                                          Segment.PlayRate.Count > 0 ? Segment.PlayRate.Max : 0.f,
	                                          Segment.LastWeight, *Subject.LastState, ActorLocal.X, ActorLocal.Y,
	                                          ActorLocal.Z, GetYawDelta(Weapon.GetActorRotation().Yaw, ActorYaw),
	                                          *CsvBones));
}

float FTeamWeaponCrewAnimAudit::GetDistanceToWeaponCollision(const ATeamWeapon& Weapon, const FVector& WorldPoint)
{
	float SmallestDistance = -1.f;
	TInlineComponentArray<UPrimitiveComponent*> Primitives(&Weapon);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		// Only the gun's meshes: box and capsule components on team weapons are selection and overlap volumes.
		if (not IsValid(Primitive) || not Primitive->IsA<UMeshComponent>() || not Primitive->IsCollisionEnabled())
		{
			continue;
		}
		FVector ClosestPoint;
		const float Distance = Primitive->GetDistanceToCollision(WorldPoint, ClosestPoint);
		if (Distance < 0.f)
		{
			continue;
		}
		SmallestDistance = SmallestDistance < 0.f ? Distance : FMath::Min(SmallestDistance, Distance);
	}
	return SmallestDistance;
}

// ---------------------------------------------------------------------------------------------------------------
// Setup tables

void FTeamWeaponCrewAnimAudit::LogSubjectSetup(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon)
{
	using namespace TeamWeaponCrewAnimAuditPrivate;
	const USquadUnitAnimInstance* AnimInstance = GetAnimInstance(Subject.Operators[0].Operator.Get());
	if (not IsValid(AnimInstance))
	{
		return;
	}
	Subject.bLoggedSetup = true;
	LogAnimClassTable(*AnimInstance);
	const FTeamWeaponCrewMontages* CrewMontages = GetCrewMontages(*AnimInstance);
	const FTeamWeaponCrewTransitionMontages* TransitionMontages = GetTransitionMontages(*AnimInstance);
	if (CrewMontages == nullptr || TransitionMontages == nullptr)
	{
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Warning, TEXT("RTS_TWQC crew_setup subject=%s no montage data"),
		       *Subject.Name);
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives(&Weapon);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		FVector ClosestPoint;
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
		       TEXT("RTS_TWQC crew_weapon_part subject=%s part=%s class=%s collision=%d profile=%s extent=%s probe=%.1f"),
		       *Subject.Name, *Primitive->GetName(), *Primitive->GetClass()->GetName(),
		       static_cast<int32>(Primitive->GetCollisionEnabled()), *Primitive->GetCollisionProfileName().ToString(),
		       *Primitive->Bounds.BoxExtent.ToCompactString(),
		       Primitive->GetDistanceToCollision(Weapon.GetActorLocation(), ClosestPoint));
	}
	const float DeploySeconds = Weapon.GetDeploymentTime();
	UWeaponState* PrimaryWeapon = GetPrimaryWeapon(const_cast<ATeamWeapon&>(Weapon));
	const float ReloadSeconds = IsValid(PrimaryWeapon) ? PrimaryWeapon->GetRawWeaponData().ReloadSpeed : 0.f;
	if (IsValid(PrimaryWeapon))
	{
		const FWeaponData& Data = PrimaryWeapon->GetRawWeaponData();
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
		       TEXT("RTS_TWQC crew_setup subject=%s subtype=%s anim=%s deploy_s=%.2f reload_s=%.2f cooldown_s=%.2f "
			       "mag=%d flux=%d range=%.0f"),
		       *Subject.Name, *GetSubtypeName(Subject.SquadSubtype), *AnimInstance->GetClass()->GetName(),
		       DeploySeconds, Data.ReloadSpeed, Data.BaseCooldown, Data.MagCapacity, Data.CooldownFlux, Data.Range);
	}

	for (const ECrewPositionType Role : CrewRoles)
	{
		const FTeamWeaponCrewMontageEntry* Entry = CrewMontages->ResolveEntry(Role, Subject.SquadSubtype);
		if (Entry == nullptr)
		{
			continue;
		}
		const FString RoleName = GetRoleName(Role);
		if (IsValid(Entry->Montage))
		{
			Subject.MontageKinds.Add(Entry->Montage->GetName(),
			                         (Entry->bReactToWeaponFire ? TEXT("React_") : TEXT("Loop_")) + RoleName);
		}
		if (IsValid(Entry->IdleLoopMontage))
		{
			Subject.MontageKinds.Add(Entry->IdleLoopMontage->GetName(), TEXT("IdleLoop_") + RoleName);
		}
		const float ReactRate = Entry->bReactToWeaponFire
			                        ? GetStretchedRate(Entry->Montage, ReloadSeconds, Entry->PlayRate)
			                        : Entry->PlayRate;
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
		       TEXT("RTS_TWQC crew_setup subject=%s role=%s react=%d montage=%s play_rate=%.2f effective_rate=%.2f "
			       "idle_loop=%s idle_rate=%.2f"),
		       *Subject.Name, *RoleName, Entry->bReactToWeaponFire ? 1 : 0, *DescribeMontage(Entry->Montage),
		       Entry->PlayRate, ReactRate, *DescribeMontage(Entry->IdleLoopMontage), Entry->IdleLoopPlayRate);
	}

	const FTeamWeaponCrewTransitionMontageSet& Set = TransitionMontages->ResolveSet(Subject.SquadSubtype);
	const ETeamWeaponCrewTransition Transitions[] = {ETeamWeaponCrewTransition::Deploy, ETeamWeaponCrewTransition::Pack};
	for (const ETeamWeaponCrewTransition Transition : Transitions)
	{
		const FTeamWeaponCrewTransitionSideMontages& Sides = Set.GetSideMontages(Transition);
		const FString TransitionName = Transition == ETeamWeaponCrewTransition::Deploy ? TEXT("Deploy") : TEXT("Pack");
		for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
		{
			const bool bIsRight = SideIndex == 1;
			UAnimMontage* Montage = Sides.GetMontageForSide(bIsRight);
			if (IsValid(Montage))
			{
				Subject.MontageKinds.Add(Montage->GetName(), TransitionName + (bIsRight ? TEXT("_R") : TEXT("_L")));
			}
			const float Stretched = GetStretchedRate(Montage, DeploySeconds, Set.PlayRate);
			const float Clamped = FMath::Clamp(Stretched, MinTransitionPlayRate, MaxTransitionPlayRate);
			const bool bLeftFallsBack = not bIsRight && Sides.LeftSideMontage == nullptr;
			UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
			       TEXT("RTS_TWQC crew_setup subject=%s transition=%s side=%s montage=%s set_rate=%.2f "
				       "stretched_rate=%.2f played_rate=%.2f clamped=%d left_uses_right=%d ends_early_s=%.2f"),
			       *Subject.Name, *TransitionName, bIsRight ? TEXT("R") : TEXT("L"), *DescribeMontage(Montage),
			       Set.PlayRate, Stretched, Clamped, FMath::Abs(Stretched - Clamped) > KINDA_SMALL_NUMBER ? 1 : 0,
			       bLeftFallsBack ? 1 : 0,
			       IsValid(Montage) && Clamped > 0.f
				       ? DeploySeconds - Montage->GetPlayLength() / (Clamped * FMath::Max(Montage->RateScale,
					       KINDA_SMALL_NUMBER))
				       : 0.f);
		}
	}
}

void FTeamWeaponCrewAnimAudit::LogAnimClassTable(const USquadUnitAnimInstance& AnimInstance)
{
	using namespace TeamWeaponCrewAnimAuditPrivate;
	const FString ClassName = AnimInstance.GetClass()->GetName();
	if (M_LoggedAnimClasses.Contains(ClassName))
	{
		return;
	}
	M_LoggedAnimClasses.Add(ClassName);
	const FTeamWeaponCrewMontages* CrewMontages = GetCrewMontages(AnimInstance);
	const FTeamWeaponCrewTransitionMontages* TransitionMontages = GetTransitionMontages(AnimInstance);
	if (CrewMontages == nullptr || TransitionMontages == nullptr)
	{
		return;
	}
	for (const ESquadSubtype Subtype : TeamWeaponSubtypes)
	{
		FString Roles;
		for (const ECrewPositionType Role : CrewRoles)
		{
			const FTeamWeaponCrewMontageEntry* Entry = CrewMontages->ResolveEntry(Role, Subtype);
			if (Entry == nullptr)
			{
				continue;
			}
			Roles += FString::Printf(TEXT(" %s=%s%s/%s"), *GetRoleName(Role), Entry->bReactToWeaponFire ? TEXT("react:") : TEXT(""),
			                         *GetMontageName(Entry->Montage), *GetMontageName(Entry->IdleLoopMontage));
		}
		const FTeamWeaponCrewTransitionMontageSet& Set = TransitionMontages->ResolveSet(Subtype);
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display,
		       TEXT("RTS_TWQC crew_table anim=%s subtype=%s deploy=%s/%s pack=%s/%s set_rate=%.2f%s"),
		       *ClassName, *GetSubtypeName(Subtype), *DescribeMontage(Set.Deploy.RightSideMontage),
		       *GetMontageName(Set.Deploy.LeftSideMontage), *DescribeMontage(Set.Pack.RightSideMontage),
		       *GetMontageName(Set.Pack.LeftSideMontage), Set.PlayRate, *Roles);
	}
}

void FTeamWeaponCrewAnimAudit::LogDeployedGeometry(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon)
{
	Subject.bLoggedDeployedGeometry = true;
	const FTransform WeaponTransform = Weapon.GetActorTransform();
	TArray<FString> Lines;
	Lines.Add(TEXT("kind,name,min_x,min_y,min_z,max_x,max_y,max_z"));
	TInlineComponentArray<UMeshComponent*> Meshes(&Weapon);
	for (const UMeshComponent* MeshComponent : Meshes)
	{
		const USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(MeshComponent);
		if (IsValid(SkeletalMesh))
		{
			for (const FBodyInstance* Body : SkeletalMesh->Bodies)
			{
				if (Body == nullptr || not Body->IsValidBodyInstance())
				{
					continue;
				}
				const FBox LocalBox = Body->GetBodyBounds().TransformBy(WeaponTransform.Inverse());
				const FName BoneName = SkeletalMesh->GetBoneName(Body->InstanceBoneIndex);
				Lines.Add(FString::Printf(TEXT("body,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f"), *BoneName.ToString(),
				                          LocalBox.Min.X, LocalBox.Min.Y, LocalBox.Min.Z, LocalBox.Max.X,
				                          LocalBox.Max.Y, LocalBox.Max.Z));
			}
		}
		else if (MeshComponent->IsCollisionEnabled())
		{
			const FBox LocalBox = MeshComponent->Bounds.GetBox().TransformBy(WeaponTransform.Inverse());
			Lines.Add(FString::Printf(TEXT("mesh,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f"), *MeshComponent->GetName(),
			                          LocalBox.Min.X, LocalBox.Min.Y, LocalBox.Min.Z, LocalBox.Max.X, LocalBox.Max.Y,
			                          LocalBox.Max.Z));
		}
		for (const FName& SocketName : MeshComponent->GetAllSocketNames())
		{
			const FVector Local = WeaponTransform.InverseTransformPosition(MeshComponent->GetSocketLocation(SocketName));
			Lines.Add(FString::Printf(TEXT("socket,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f"), *SocketName.ToString(),
			                          Local.X, Local.Y, Local.Z, Local.X, Local.Y, Local.Z));
		}
	}
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		UE_LOG(LogRTSTeamWeaponCrewAnimAudit, Display, TEXT("RTS_TWQC crew_geom subject=%s %s"), *Subject.Name,
		       *Lines[LineIndex]);
	}
	if (not M_OutputDirectory.IsEmpty())
	{
		FFileHelper::SaveStringArrayToFile(Lines, *FPaths::Combine(M_OutputDirectory,
		                                                           TEXT("geom_") + Subject.Name + TEXT(".csv")));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// CSV

void FTeamWeaponCrewAnimAudit::WriteWeaponCsvRow(FTWCrewAnimSubjectTrack& Subject, const ATeamWeapon& Weapon,
                                                 const float GameSeconds)
{
	using namespace TeamWeaponCrewAnimAuditPrivate;
	if (M_OutputDirectory.IsEmpty())
	{
		return;
	}
	const USkeletalMeshComponent* Mesh = Weapon.FindComponentByClass<USkeletalMeshComponent>();
	if (not IsValid(Mesh))
	{
		return;
	}
	TArray<FString>& Lines = M_PendingWeaponCsvLines.FindOrAdd(Subject.Name);
	if (not Subject.bWroteWeaponCsvHeader)
	{
		Subject.bWroteWeaponCsvHeader = true;
		const USkinnedAsset* SkinnedAsset = Mesh->GetSkinnedAsset();
		const int32 BoneCount = IsValid(SkinnedAsset)
			                        ? FMath::Min(SkinnedAsset->GetRefSkeleton().GetNum(), MaxWeaponCsvBones)
			                        : 0;
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			Subject.WeaponBoneNames.Add(SkinnedAsset->GetRefSkeleton().GetBoneName(BoneIndex));
		}
		FString Header = TEXT("t,state,gun_montage");
		for (const FName& BoneName : Subject.WeaponBoneNames)
		{
			Header += FString::Printf(TEXT(",%s_x,%s_y,%s_z"), *BoneName.ToString(), *BoneName.ToString(),
			                          *BoneName.ToString());
		}
		Lines.Add(Header);
	}
	const FTransform WeaponTransform = Weapon.GetActorTransform();
	FString Row = FString::Printf(TEXT("%.3f,%s,%s"), GameSeconds, *Subject.LastState,
	                              *GetMontageName(Subject.GunMontage.Get()));
	for (const FName& BoneName : Subject.WeaponBoneNames)
	{
		const FVector Local = WeaponTransform.InverseTransformPosition(
			Mesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace));
		Row += FString::Printf(TEXT(",%.1f,%.1f,%.1f"), Local.X, Local.Y, Local.Z);
	}
	Lines.Add(Row);
}

void FTeamWeaponCrewAnimAudit::FlushCsv(const bool bForce)
{
	using namespace TeamWeaponCrewAnimAuditPrivate;
	if (M_OutputDirectory.IsEmpty())
	{
		return;
	}
	int32 PendingLines = M_PendingCrewCsvLines.Num();
	for (const TPair<FString, TArray<FString>>& Pending : M_PendingWeaponCsvLines)
	{
		PendingLines += Pending.Value.Num();
	}
	if (not bForce && PendingLines < CsvFlushLineCount)
	{
		return;
	}
	if (not bM_WroteCrewCsvHeader)
	{
		bM_WroteCrewCsvHeader = true;
		FString Header = TEXT("t,subject,op,role,side,montage,kind,pos,rate,weight,state,actor_x,actor_y,actor_z,actor_yaw");
		for (const FName& Bone : TrackedBones)
		{
			Header += FString::Printf(TEXT(",%s_x,%s_y,%s_z"), *Bone.ToString(), *Bone.ToString(), *Bone.ToString());
		}
		M_PendingCrewCsvLines.Insert(Header, 0);
	}
	if (M_PendingCrewCsvLines.Num() > 0)
	{
		FFileHelper::SaveStringArrayToFile(M_PendingCrewCsvLines, *FPaths::Combine(M_OutputDirectory, CrewCsvName),
		                                   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(),
		                                   FILEWRITE_Append);
		M_PendingCrewCsvLines.Reset();
	}
	for (TPair<FString, TArray<FString>>& Pending : M_PendingWeaponCsvLines)
	{
		if (Pending.Value.Num() == 0)
		{
			continue;
		}
		FFileHelper::SaveStringArrayToFile(Pending.Value,
		                                   *FPaths::Combine(M_OutputDirectory, TEXT("weapon_") + Pending.Key + TEXT(".csv")),
		                                   FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(),
		                                   FILEWRITE_Append);
		Pending.Value.Reset();
	}
}

#endif
