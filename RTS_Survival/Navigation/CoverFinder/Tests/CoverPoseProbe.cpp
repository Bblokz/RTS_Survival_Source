#include "CoverPoseProbe.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverPose, Log, All);

namespace CoverPoseProbePrivate
{
	constexpr float PoseLogIntervalSeconds = 1.0f;
	constexpr float CloseUpIntervalSeconds = 2.5f;
	constexpr int32 MaximumCloseUps = 60;
	constexpr int32 FramesBeforeScreenshot = 4;
	constexpr int32 PlayerOwnedTeam = 1;
	constexpr int32 SubjectFamilyCount = 3;
	constexpr float TargetEyeHeight = 150.0f;

	FVector GetBoneLocationByNamePart(const USkeletalMeshComponent& UnitMesh, const TCHAR* NamePart)
	{
		for (int32 BoneIndex = 0; BoneIndex < UnitMesh.GetNumBones(); ++BoneIndex)
		{
			if (UnitMesh.GetBoneName(BoneIndex).ToString().Contains(NamePart))
			{
				return UnitMesh.GetBoneTransform(BoneIndex).GetLocation();
			}
		}
		return UnitMesh.GetComponentLocation();
	}

	bool GetIsUnitAiming(const ASquadUnit& SquadUnit)
	{
		AInfantryWeaponMaster* Weapon = SquadUnit.GetInfantryWeapon();
		return IsValid(Weapon) && IsValid(Weapon->GetCurrentTargetActor());
	}

	// 0 crouch cover with a target, 1 standing cover stepped out, 2 any other occupied cover.
	int32 GetSubjectFamily(const ASquadUnit& SquadUnit)
	{
		const FSquadUnitCoverRuntimeState& CoverState = SquadUnit.GetCoverRuntimeState();
		if (CoverState.AssignedCoverPoint.CoverType == ERTSCoverType::Crouch && GetIsUnitAiming(SquadUnit))
		{
			return 0;
		}
		return CoverState.State == ESquadUnitCoverState::Exposed ? 1 : 2;
	}
}

void FCoverPoseProbe::Start()
{
	bM_IsEnabled = FParse::Param(FCommandLine::Get(), TEXT("CoverFinderCloseUps"));
	M_NextPoseLogSeconds = 0.0f;
	M_NextCloseUpSeconds = 0.0f;
	M_FramesUntilScreenshot = 0;
	M_CloseUpCount = 0;
	M_SubjectRotation = 0;
}

void FCoverPoseProbe::Tick(UWorld& World, const float PhaseSeconds)
{
	using namespace CoverPoseProbePrivate;
	if (not bM_IsEnabled)
	{
		return;
	}
	if (PhaseSeconds >= M_NextPoseLogSeconds)
	{
		M_NextPoseLogSeconds = PhaseSeconds + PoseLogIntervalSeconds;
		LogCoverPoses(World, PhaseSeconds);
	}
	if (M_FramesUntilScreenshot > 0)
	{
		TickPendingScreenshot(World, PhaseSeconds);
		return;
	}
	if (PhaseSeconds < M_NextCloseUpSeconds || M_CloseUpCount >= MaximumCloseUps)
	{
		return;
	}
	ASquadUnit* Subject = FindCloseUpSubject(World);
	if (IsValid(Subject))
	{
		StartCloseUp(World, *Subject, PhaseSeconds);
	}
}

void FCoverPoseProbe::LogCoverPoses(UWorld& World, const float PhaseSeconds) const
{
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		if (IsValid(SquadUnit) && SquadUnit->IsUnitAlive() && SquadUnit->GetIsOccupyingCover())
		{
			LogCoverPose(*SquadUnit, PhaseSeconds, TEXT("sample"));
		}
	}
}

void FCoverPoseProbe::LogCoverPose(
	const ASquadUnit& SquadUnit,
	const float PhaseSeconds,
	const TCHAR* Occasion) const
{
	using namespace CoverPoseProbePrivate;
	const USkeletalMeshComponent* UnitMesh = SquadUnit.GetMesh();
	const UCapsuleComponent* UnitCapsule = SquadUnit.GetCapsuleComponent();
	const USquadUnitAnimInstance* UnitAnimation = SquadUnit.GetAnimBP_SquadUnit();
	UWorld* World = SquadUnit.GetWorld();
	if (not IsValid(UnitMesh) || not IsValid(UnitCapsule) || not IsValid(UnitAnimation) || not IsValid(World))
	{
		return;
	}
	const FSquadUnitCoverRuntimeState& CoverState = SquadUnit.GetCoverRuntimeState();
	const float FeetHeight = SquadUnit.GetActorLocation().Z - UnitCapsule->GetScaledCapsuleHalfHeight();
	const FVector HeadLocation = GetBoneLocationByNamePart(*UnitMesh, TEXT("head"));
	const FVector PelvisLocation = GetBoneLocationByNamePart(*UnitMesh, TEXT("pelvis"));
	// How far the cover animation carries the body away from the capsule, in the soldier's cover frame.
	const FVector TowardCoverDirection = -CoverState.AssignedCoverPoint.CoverNormal.GetSafeNormal2D();
	const FVector RightDirection = FVector::CrossProduct(FVector::UpVector, TowardCoverDirection);
	const FVector PelvisOffset = PelvisLocation - SquadUnit.GetActorLocation();
	AInfantryWeaponMaster* Weapon = SquadUnit.GetInfantryWeapon();
	const AActor* TargetActor = IsValid(Weapon) ? Weapon->GetCurrentTargetActor() : nullptr;
	int32 HeadSeenByTarget = -1;
	if (IsValid(TargetActor))
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CoverPoseProbe), false, &SquadUnit);
		QueryParams.AddIgnoredActor(TargetActor);
		FHitResult Hit;
		const FVector TargetEye = TargetActor->GetActorLocation() + FVector(0.0, 0.0, TargetEyeHeight);
		HeadSeenByTarget = World->LineTraceSingleByChannel(Hit, TargetEye, HeadLocation, ECC_Visibility, QueryParams)
			? 0
			: 1;
	}
	UE_LOG(
		LogRTSCoverPose,
		Display,
		TEXT("RTS_COVER_POSE %s t=%.1f unit=%s owner=%d type=%s state=%s has_target=%d head_seen_by_target=%d head_cm=%.0f pelvis_cm=%.0f pelvis_toward_cover_cm=%.0f pelvis_right_cm=%.0f mesh_visible=%d actor_hidden=%d rendered=%d bounds_radius=%.0f mesh_rel=%s facing_cover_dot=%.2f %s"),
		Occasion,
		PhaseSeconds,
		*SquadUnit.GetName(),
		SquadUnit.GetOwningPlayer(),
		*UEnum::GetValueAsString(CoverState.AssignedCoverPoint.CoverType),
		*UEnum::GetValueAsString(CoverState.State),
		IsValid(TargetActor) ? 1 : 0,
		HeadSeenByTarget,
		HeadLocation.Z - FeetHeight,
		PelvisLocation.Z - FeetHeight,
		FVector::DotProduct(PelvisOffset, TowardCoverDirection),
		FVector::DotProduct(PelvisOffset, RightDirection),
		UnitMesh->IsVisible() ? 1 : 0,
		SquadUnit.IsHidden() ? 1 : 0,
		UnitMesh->WasRecentlyRendered(0.5f) ? 1 : 0,
		UnitMesh->Bounds.SphereRadius,
		*UnitMesh->GetRelativeLocation().ToCompactString(),
		FVector::DotProduct(SquadUnit.GetActorForwardVector(), TowardCoverDirection),
		*UnitAnimation->GetPoseDebugString());
}

ASquadUnit* FCoverPoseProbe::FindCloseUpSubject(UWorld& World)
{
	using namespace CoverPoseProbePrivate;
	const int32 WantedFamily = M_SubjectRotation % SubjectFamilyCount;
	++M_SubjectRotation;
	ASquadUnit* FallbackSubject = nullptr;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		const bool bIsCandidate = IsValid(SquadUnit) && SquadUnit->IsUnitAlive() &&
			SquadUnit->GetIsOccupyingCover() && SquadUnit->GetOwningPlayer() == PlayerOwnedTeam &&
			SquadUnit != M_PhotographedUnit.Get();
		if (not bIsCandidate)
		{
			continue;
		}
		if (GetSubjectFamily(*SquadUnit) == WantedFamily)
		{
			return SquadUnit;
		}
		FallbackSubject = GetSubjectFamily(*SquadUnit) == 0 || not IsValid(FallbackSubject)
			? SquadUnit
			: FallbackSubject;
	}
	return FallbackSubject;
}

void FCoverPoseProbe::StartCloseUp(UWorld& World, ASquadUnit& SquadUnit, const float PhaseSeconds)
{
	using namespace CoverPoseProbePrivate;
	M_PhotographedUnit = &SquadUnit;
	M_PendingCameraAngle = 0;
	M_NextCloseUpSeconds = PhaseSeconds + CloseUpIntervalSeconds;
	++M_CloseUpCount;
	AimCameraAtUnit(World, SquadUnit);
	M_FramesUntilScreenshot = FramesBeforeScreenshot;
}

void FCoverPoseProbe::AimCameraAtUnit(UWorld& World, const ASquadUnit& SquadUnit)
{
	ACameraActor* Camera = M_Camera.Get();
	if (not IsValid(Camera))
	{
		Camera = World.SpawnActor<ACameraActor>();
		M_Camera = Camera;
	}
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (not IsValid(Camera) || not IsValid(PlayerController))
	{
		return;
	}
	if (UCameraComponent* CameraComponent = Camera->GetCameraComponent())
	{
		CameraComponent->bConstrainAspectRatio = false;
	}
	const FRTSCoverPoint& CoverPoint = SquadUnit.GetCoverRuntimeState().AssignedCoverPoint;
	const FVector AwayFromCover = CoverPoint.CoverNormal.GetSafeNormal2D();
	const FVector RightDirection = FVector::CrossProduct(FVector::UpVector, -AwayFromCover);
	const FVector UnitLocation = SquadUnit.GetActorLocation();
	const FVector CameraLocation = M_PendingCameraAngle == 0
		? UnitLocation + AwayFromCover * 230.0 + RightDirection * 210.0 + FVector(0.0, 0.0, 110.0)
		: UnitLocation + AwayFromCover * 380.0 + FVector(0.0, 0.0, 650.0);
	Camera->SetActorLocationAndRotation(CameraLocation, (UnitLocation - CameraLocation).Rotation());
	PlayerController->SetViewTarget(Camera);
}

void FCoverPoseProbe::TickPendingScreenshot(UWorld& World, const float PhaseSeconds)
{
	using namespace CoverPoseProbePrivate;
	const ASquadUnit* SquadUnit = M_PhotographedUnit.Get();
	if (not IsValid(SquadUnit) || not SquadUnit->GetIsOccupyingCover())
	{
		M_FramesUntilScreenshot = 0;
		return;
	}
	// The soldier may have stepped out or back since the camera was placed.
	AimCameraAtUnit(World, *SquadUnit);
	--M_FramesUntilScreenshot;
	if (M_FramesUntilScreenshot > 0)
	{
		return;
	}
	const FSquadUnitCoverRuntimeState& CoverState = SquadUnit->GetCoverRuntimeState();
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CoverFinderDebug"), TEXT("CloseUps"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	FString TypeName = UEnum::GetValueAsString(CoverState.AssignedCoverPoint.CoverType);
	FString StateName = UEnum::GetValueAsString(CoverState.State);
	TypeName.Split(TEXT("::"), nullptr, &TypeName);
	StateName.Split(TEXT("::"), nullptr, &StateName);
	const FString FileName = FString::Printf(
		TEXT("%02d_%s_%s_%s_%s_%s.png"),
		M_CloseUpCount,
		*TypeName,
		*StateName,
		GetIsUnitAiming(*SquadUnit) ? TEXT("target") : TEXT("notarget"),
		*SquadUnit->GetName(),
		M_PendingCameraAngle == 0 ? TEXT("side") : TEXT("top"));
	FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, FileName), false, false);
	LogCoverPose(*SquadUnit, PhaseSeconds, *FString::Printf(TEXT("closeup=%s"), *FileName));
	if (M_PendingCameraAngle == 0)
	{
		M_PendingCameraAngle = 1;
		M_FramesUntilScreenshot = FramesBeforeScreenshot;
	}
}
