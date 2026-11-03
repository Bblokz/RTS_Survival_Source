// Copyright (C) Bas Blokzijl - All rights reserved.

// The -RagdollPoseBake mode of URagdollDemoSubsystem: candidate poses for the battlefield corpse library.

#include "RagdollDemoSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "PhysicsEngine/PhysicsAsset.h"
#if WITH_EDITOR
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#endif
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "RTS_Survival/Units/Ragdoll/RTSPooledRagdoll.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSettings.h"
#include "RTS_Survival/Units/Ragdoll/Tests/CorpsePoseBaker.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnitRagdoll.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSRagdollPoseBake, Log, All);

namespace RagdollPoseBakePrivate
{
	const TCHAR* const CountSwitch = TEXT("RagdollPoseCount=");
	const TCHAR* const SeedSwitch = TEXT("RagdollPoseSeed=");
	constexpr int32 DefaultPosesPerSide = 16;
	constexpr int32 DefaultSeed = 1;
	constexpr int32 SideCount = 2;
	constexpr int32 Columns = 8;
	// Bodies do not collide with each other, so the spacing only keeps neighbours out of each picture.
	constexpr float SlotSpacingCm = 1400.f;
	constexpr float PlatformMarginCm = 1200.f;
	constexpr float PlatformThicknessCm = 50.f;
	// High above the map, so terrain and props never touch a body.
	constexpr float PlatformLiftCm = 3000.f;
	constexpr float CubeSizeCm = 100.f;
	const TCHAR* const PlatformMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const PlatformMaterialPath = TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial");
	const FName PlatformCollisionProfile(TEXT("BlockAll"));
	constexpr float StartHeightCm = 250.f;
	constexpr float FullCircleDegrees = 360.f;
	constexpr float MinSettleSeconds = 1.f;
	constexpr float MaxSettleSeconds = 8.f;
	const TCHAR* const PackageFolder = TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/CorpsePoses/Candidates/");
	const TCHAR* const SidePrefixes[SideCount] = {TEXT("Scav"), TEXT("Mosin")};
	const FName PelvisBone(TEXT("pelvis"));

	enum class EPoseShot : uint8
	{
		Ragdoll,
		Baked,
		BakedSide,
		Count
	};

	const TCHAR* const ShotNames[] = {TEXT("ragdoll"), TEXT("baked"), TEXT("side")};
	// The capture renders a frame late; a few frames also let auto exposure settle after the jump.
	constexpr int32 FramesPerShot = 6;
	constexpr float InstantExposureSpeed = 100.f;
	constexpr float ShotFieldOfView = 40.f;
	constexpr float TopDistanceCm = 450.f;
	constexpr float TopPitchDegrees = -55.f;
	constexpr float TopYawDegrees = 90.f;
	constexpr float SideDistanceCm = 400.f;
	constexpr float SidePitchDegrees = -12.f;
	constexpr float SideYawDegrees = 0.f;
	constexpr float AimHeightCm = 30.f;
}

bool URagdollDemoSubsystem::TryStartPoseBake()
{
	using namespace RagdollPoseBakePrivate;
	if (not SpawnPosePlatform())
	{
		Finish(TEXT("no_pose_platform"));
		return false;
	}
	ThrowPoseBodies();
	return true;
}

bool URagdollDemoSubsystem::SpawnPosePlatform()
{
	using namespace RagdollPoseBakePrivate;
	float GroundZ = 0.f;
	if (not TryGetGroundZ(M_DemoLocation, GroundZ))
	{
		return false;
	}
	M_PoseBake.PlatformTop = FVector(M_DemoLocation.X, M_DemoLocation.Y, GroundZ + PlatformLiftCm);
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, PlatformMeshPath);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Platform = GetWorld()->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (not IsValid(Platform) || not IsValid(CubeMesh))
	{
		return false;
	}
	const int32 Rows = SideCount * FMath::DivideAndRoundUp(DefaultPosesPerSide, Columns);
	const FVector PlatformSize((Columns - 1) * SlotSpacingCm + 2.f * PlatformMarginCm,
	                           (Rows - 1) * SlotSpacingCm + 2.f * PlatformMarginCm, PlatformThicknessCm);
	UStaticMeshComponent* PlatformComponent = Platform->GetStaticMeshComponent();
	PlatformComponent->SetMobility(EComponentMobility::Movable);
	PlatformComponent->SetStaticMesh(CubeMesh);
	PlatformComponent->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, PlatformMaterialPath));
	PlatformComponent->SetCollisionProfileName(PlatformCollisionProfile);
	Platform->SetActorScale3D(PlatformSize / CubeSizeCm);
	Platform->SetActorLocation(M_PoseBake.PlatformTop - FVector(0.f, 0.f, PlatformThicknessCm * 0.5f));
	return true;
}

void URagdollDemoSubsystem::LogPosePhysicsBodies(const USkeletalMesh* CrewMesh)
{
	const UPhysicsAsset* PhysicsAsset = IsValid(CrewMesh) ? CrewMesh->GetPhysicsAsset() : nullptr;
	if (not IsValid(PhysicsAsset))
	{
		return;
	}
	UE_LOG(LogRTSRagdollPoseBake, Display, TEXT("RTS_RGP physics mesh=%s asset=%s bodies=%d"), *CrewMesh->GetName(),
	       *PhysicsAsset->GetName(), PhysicsAsset->SkeletalBodySetups.Num());
#if WITH_EDITOR
	// Vertices led by a bone without a body are carried by no simulated part and dangle when the body falls.
	const FSkeletalMeshModel* ImportedModel = CrewMesh->GetImportedModel();
	const FSkeletalMeshLODModel* Lod = ImportedModel && ImportedModel->LODModels.Num() > 0
		                                   ? &ImportedModel->LODModels[0]
		                                   : nullptr;
	for (int32 SectionIndex = 0; Lod && SectionIndex < Lod->Sections.Num(); ++SectionIndex)
	{
		const FSkelMeshSection& Section = Lod->Sections[SectionIndex];
		TMap<FName, int32> UnsimulatedCounts;
		FBox UnsimulatedBounds(ForceInit);
		for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
		{
			int32 MainInfluence = 0;
			for (int32 Influence = 1; Influence < MAX_TOTAL_INFLUENCES; ++Influence)
			{
				MainInfluence = Vertex.InfluenceWeights[Influence] > Vertex.InfluenceWeights[MainInfluence]
					                ? Influence
					                : MainInfluence;
			}
			const int32 BoneIndex = Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[MainInfluence])
				                        ? Section.BoneMap[Vertex.InfluenceBones[MainInfluence]]
				                        : INDEX_NONE;
			const FName BoneName = CrewMesh->GetRefSkeleton().IsValidIndex(BoneIndex)
				                       ? CrewMesh->GetRefSkeleton().GetBoneName(BoneIndex)
				                       : NAME_None;
			if (BoneIndex == 0)
			{
				++UnsimulatedCounts.FindOrAdd(BoneName);
				UnsimulatedBounds += FVector(Vertex.Position);
			}
		}
		for (const TPair<FName, int32>& Count : UnsimulatedCounts)
		{
			UE_LOG(LogRTSRagdollPoseBake, Display,
			       TEXT("RTS_RGP root_vertices section=%d material=%d verts=%d of %d bounds=%s"), SectionIndex,
			       Section.MaterialIndex, Count.Value, Section.SoftVertices.Num(), *UnsimulatedBounds.ToString());
		}
	}
#endif
	for (const USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		if (not IsValid(BodySetup))
		{
			continue;
		}
		FString Shapes;
		for (const FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
		{
			Shapes += FString::Printf(TEXT(" capsule r=%.1f l=%.1f at=%s"), Capsule.Radius, Capsule.Length,
			                          *Capsule.Center.ToCompactString());
		}
		for (const FKSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
		{
			Shapes += FString::Printf(TEXT(" sphere r=%.1f at=%s"), Sphere.Radius, *Sphere.Center.ToCompactString());
		}
		for (const FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
		{
			Shapes += FString::Printf(TEXT(" box %.0fx%.0fx%.0f at=%s"), Box.X, Box.Y, Box.Z,
			                          *Box.Center.ToCompactString());
		}
		Shapes += FString::Printf(TEXT(" convex=%d"), BodySetup->AggGeom.ConvexElems.Num());
		UE_LOG(LogRTSRagdollPoseBake, Display, TEXT("RTS_RGP body bone=%s collision=%d%s"),
		       *BodySetup->BoneName.ToString(), static_cast<int32>(BodySetup->DefaultInstance.GetCollisionEnabled()),
		       *Shapes);
	}
}

void URagdollDemoSubsystem::ThrowPoseBodies()
{
	using namespace RagdollPoseBakePrivate;
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	int32 PosesPerSide = DefaultPosesPerSide;
	int32 Seed = DefaultSeed;
	FParse::Value(FCommandLine::Get(), CountSwitch, PosesPerSide);
	FParse::Value(FCommandLine::Get(), SeedSwitch, Seed);
	PosesPerSide = FMath::Clamp(PosesPerSide, 1, DefaultPosesPerSide);
	FRandomStream Random(Seed);
	UPhysicalMaterial* CorpseMaterial = FSquadUnitRagdoll::CreateCorpseMaterial(this);
	const FRTSCrewBodyType* CrewTypes[SideCount] = {&Settings->GermanCrew, &Settings->SovietCrew};
	const int32 RowsPerSide = FMath::DivideAndRoundUp(DefaultPosesPerSide, Columns);
	const float HalfWidth = (Columns - 1) * SlotSpacingCm * 0.5f;
	const float HalfDepth = (SideCount * RowsPerSide - 1) * SlotSpacingCm * 0.5f;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Side = 0; Side < SideCount; ++Side)
	{
		USkeletalMesh* CrewMesh = CrewTypes[Side]->Mesh.LoadSynchronous();
		LogPosePhysicsBodies(CrewMesh);
		// German bodies wear the plain Scavenger look so the pictures compare poses, not uniforms.
		UMaterialInterface* Look = CrewTypes[Side]->Materials.IsEmpty()
			                           ? nullptr
			                           : CrewTypes[Side]->Materials[0].LoadSynchronous();
		for (int32 PoseIndex = 0; PoseIndex < PosesPerSide; ++PoseIndex)
		{
			ARTSPooledRagdoll* Body = GetWorld()->SpawnActor<ARTSPooledRagdoll>(
				ARTSPooledRagdoll::StaticClass(), FTransform::Identity, SpawnParameters);
			if (not IsValid(Body) || not IsValid(CrewMesh))
			{
				continue;
			}
			const int32 Row = Side * RowsPerSide + PoseIndex / Columns;
			const FVector Slot = M_PoseBake.PlatformTop + FVector((PoseIndex % Columns) * SlotSpacingCm - HalfWidth,
			                                                      Row * SlotSpacingCm - HalfDepth, StartHeightCm);
			Body->InitializeForPool(CrewMesh, CorpseMaterial);
			Body->Wake(Slot, FRotator(0.f, Random.FRandRange(0.f, FullCircleDegrees), 0.f), Look);
			// Half thrown like an ordinary death, half like a cook-off, as in play.
			const FRTSCrewThrow& Throw = PoseIndex % 2 == 0 ? Settings->OrdinaryDeath : Settings->CookOffDeath;
			const FVector Outward = FRotator(0.f, Random.FRandRange(0.f, FullCircleDegrees), 0.f).Vector();
			const FVector Velocity = Outward * Random.FRandRange(Throw.OutwardSpeed.X, Throw.OutwardSpeed.Y)
				+ FVector::UpVector * Random.FRandRange(Throw.UpwardSpeed.X, Throw.UpwardSpeed.Y);
			const FVector Spin = Random.VRand() * Throw.MaxSpinDegrees;
			const TWeakObjectPtr<ARTSPooledRagdoll> WeakBody = Body;
			GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakBody, Velocity, Spin]()
			{
				if (WeakBody.IsValid())
				{
					WeakBody->Launch(Velocity, Spin);
				}
			}));
			M_PoseBake.Bodies.Add(Body);
			M_PoseBake.PoseNames.Add(FString::Printf(TEXT("%s_%02d"), SidePrefixes[Side], PoseIndex + 1));
		}
	}
	UE_LOG(LogRTSRagdollPoseBake, Display, TEXT("RTS_RGP thrown=%d seed=%d platform=%s"), M_PoseBake.Bodies.Num(),
	       Seed, *M_PoseBake.PlatformTop.ToCompactString());
}

void URagdollDemoSubsystem::TickPoseSettle()
{
	using namespace RagdollPoseBakePrivate;
	KeepGameRunning();
	const float SettleSeconds = GetGameSeconds() - M_StepStartGameSeconds;
	if (SettleSeconds < MinSettleSeconds)
	{
		return;
	}
	int32 Asleep = 0;
	for (const ARTSPooledRagdoll* Body : M_PoseBake.Bodies)
	{
		Asleep += IsValid(Body) && Body->GetIsAsleep() ? 1 : 0;
	}
	if (Asleep < M_PoseBake.Bodies.Num() && SettleSeconds < MaxSettleSeconds)
	{
		return;
	}
	UE_LOG(LogRTSRagdollPoseBake, Display, TEXT("RTS_RGP settled asleep=%d of %d after %.1f s"), Asleep,
	       M_PoseBake.Bodies.Num(), SettleSeconds);
	BakePoses();
	if (not CreateCaptureRig())
	{
		Finish(TEXT("no_capture_rig"));
		return;
	}
	USceneCaptureComponent2D* CaptureComponent = M_Capture->GetCaptureComponent2D();
	CaptureComponent->FOVAngle = ShotFieldOfView;
	// Each shot is only a few frames long; exposure must settle within them so all pictures compare.
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
	CaptureComponent->PostProcessSettings.AutoExposureSpeedUp = InstantExposureSpeed;
	CaptureComponent->PostProcessSettings.AutoExposureSpeedDown = InstantExposureSpeed;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* PreviewActor = GetWorld()->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (IsValid(PreviewActor))
	{
		PreviewActor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		PreviewActor->SetActorEnableCollision(false);
	}
	M_PoseBake.PreviewActor = PreviewActor;
	M_Step = ERagdollDemoStep::PoseShoot;
}

void URagdollDemoSubsystem::BakePoses()
{
	using namespace RagdollPoseBakePrivate;
	for (int32 PoseIndex = 0; PoseIndex < M_PoseBake.Bodies.Num(); ++PoseIndex)
	{
		ARTSPooledRagdoll* Body = M_PoseBake.Bodies[PoseIndex];
		const USkeletalMeshComponent* CrewMesh = IsValid(Body) ? Body->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		if (not IsValid(CrewMesh))
		{
			M_PoseBake.BakedMeshes.Add(nullptr);
			M_PoseBake.Pivots.Add(FVector::ZeroVector);
			continue;
		}
		const bool bAsleep = Body->GetIsAsleep();
		// Awake but slow means still settling; fast means bodies fight each other and the ragdoll twitches.
		const FBodyInstance* PelvisBody = CrewMesh->GetBodyInstance(PelvisBone);
		const float PelvisSpeed = PelvisBody != nullptr ? PelvisBody->GetUnrealWorldVelocity().Size() : 0.f;
		float FastestBodySpeed = 0.f;
		for (const FBodyInstance* BodyInstance : CrewMesh->Bodies)
		{
			if (BodyInstance != nullptr && BodyInstance->IsValidBodyInstance())
			{
				FastestBodySpeed = FMath::Max(FastestBodySpeed,
				                              static_cast<float>(BodyInstance->GetUnrealWorldVelocity().Size()));
			}
		}
		UE_LOG(LogRTSRagdollPoseBake, Display, TEXT("RTS_RGP motion pose=%s pelvis_speed=%.1f fastest_body=%.1f"),
		       *M_PoseBake.PoseNames[PoseIndex], PelvisSpeed, FastestBodySpeed);
		Body->Freeze();
		const bool bHasPelvis = CrewMesh->GetBoneIndex(PelvisBone) != INDEX_NONE;
		const FTransform Pelvis = bHasPelvis ? CrewMesh->GetBoneTransform(PelvisBone) : CrewMesh->GetComponentTransform();
		const FVector Pivot(Pelvis.GetLocation().X, Pelvis.GetLocation().Y, M_PoseBake.PlatformTop.Z);
		const FString PackagePath = FString(PackageFolder) + TEXT("SM_CorpsePose_") + M_PoseBake.PoseNames[PoseIndex];
#if WITH_EDITOR
		const FCorpsePoseBakeResult Result = CorpsePoseBaker::BakeAndSave(CrewMesh, Pivot, PackagePath);
#else
		const FCorpsePoseBakeResult Result;
#endif
		M_PoseBake.BakedMeshes.Add(Result.Mesh);
		M_PoseBake.Pivots.Add(Pivot);
		// Pelvis axes tell face up, face down or on the side apart; the library will match poses on them later.
		UE_LOG(LogRTSRagdollPoseBake, Display,
		       TEXT("RTS_RGP pose=%s asleep=%d saved=%d tris=%d lowest_z=%.1f lowest_bone=%s pelvis_h=%.1f pelvis_x=%s pelvis_y=%s pelvis_z=%s"),
		       *M_PoseBake.PoseNames[PoseIndex], bAsleep ? 1 : 0, Result.bSaved ? 1 : 0, Result.Triangles,
		       Result.LowestZ, *Result.LowestBone.ToString(), Pelvis.GetLocation().Z - Pivot.Z,
		       *Pelvis.GetUnitAxis(EAxis::X).ToCompactString(), *Pelvis.GetUnitAxis(EAxis::Y).ToCompactString(),
		       *Pelvis.GetUnitAxis(EAxis::Z).ToCompactString());
	}
}

void URagdollDemoSubsystem::TickPoseShoot()
{
	using namespace RagdollPoseBakePrivate;
	KeepGameRunning();
	constexpr int32 ShotsPerPose = static_cast<int32>(EPoseShot::Count);
	if (M_PoseBake.ShotIndex >= M_PoseBake.Bodies.Num() * ShotsPerPose)
	{
		Finish(TEXT("pose_bake_done"));
		return;
	}
	const int32 PoseIndex = M_PoseBake.ShotIndex / ShotsPerPose;
	const int32 ShotKind = M_PoseBake.ShotIndex % ShotsPerPose;
	if (M_PoseBake.ShotFrame == 0)
	{
		SetUpPoseShot(PoseIndex, ShotKind);
	}
	++M_PoseBake.ShotFrame;
	if (M_PoseBake.ShotFrame < FramesPerShot)
	{
		return;
	}
	if (not M_OutputDirectory.IsEmpty() && IsValid(M_RenderTarget))
	{
		UKismetRenderingLibrary::ExportRenderTarget(
			GetWorld(), M_RenderTarget, M_OutputDirectory,
			FString::Printf(TEXT("pose_%s_%s.png"), *M_PoseBake.PoseNames[PoseIndex], ShotNames[ShotKind]));
	}
	M_PoseBake.ShotFrame = 0;
	++M_PoseBake.ShotIndex;
}

void URagdollDemoSubsystem::SetUpPoseShot(const int32 PoseIndex, const int32 ShotKind) const
{
	using namespace RagdollPoseBakePrivate;
	ASceneCapture2D* Capture = M_Capture.Get();
	AStaticMeshActor* PreviewActor = M_PoseBake.PreviewActor.Get();
	ARTSPooledRagdoll* Body = M_PoseBake.Bodies[PoseIndex];
	if (not IsValid(Capture) || not IsValid(PreviewActor) || not IsValid(Body))
	{
		return;
	}
	const bool bShowsRagdoll = ShotKind == static_cast<int32>(EPoseShot::Ragdoll);
	const FVector Pivot = M_PoseBake.Pivots[PoseIndex];
	// Thrown bodies can land a few metres apart; each picture shows one pose only.
	for (ARTSPooledRagdoll* OtherBody : M_PoseBake.Bodies)
	{
		if (IsValid(OtherBody))
		{
			OtherBody->SetActorHiddenInGame(true);
		}
	}
	Body->SetActorHiddenInGame(not bShowsRagdoll);
	PreviewActor->GetStaticMeshComponent()->SetStaticMesh(M_PoseBake.BakedMeshes[PoseIndex]);
	PreviewActor->SetActorLocation(Pivot);
	PreviewActor->SetActorHiddenInGame(bShowsRagdoll);

	const bool bIsSideShot = ShotKind == static_cast<int32>(EPoseShot::BakedSide);
	const FRotator CameraRotation(bIsSideShot ? SidePitchDegrees : TopPitchDegrees,
	                              bIsSideShot ? SideYawDegrees : TopYawDegrees, 0.f);
	const FVector AimPoint = Pivot + FVector(0.f, 0.f, AimHeightCm);
	const float Distance = bIsSideShot ? SideDistanceCm : TopDistanceCm;
	Capture->SetActorLocationAndRotation(AimPoint - CameraRotation.Vector() * Distance, CameraRotation);
}

#endif
