// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RagdollDemoSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Animation/SkeletalMeshActor.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Ragdoll/RTSPooledRagdoll.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSettings.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSubsystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSRagdollDemo, Log, All);

namespace RagdollDemoPrivate
{
	const TCHAR* const DemoSwitch = TEXT("RagdollDemo");
	const TCHAR* const DirectorySwitch = TEXT("RagdollDemoDir=");
	const TCHAR* const LocationSwitch = TEXT("RagdollDemoAt=");
	const TCHAR* const SquadSwitch = TEXT("RagdollDemoSquad=");
	const TCHAR* const BenchSwitch = TEXT("RagdollBench");
	const TCHAR* const CrewSwitch = TEXT("RagdollCrew");
	constexpr int32 CrewTankCount = 2;
	constexpr float CrewTankOffsetCm = 700.f;
	constexpr float CrewTankSettleSeconds = 4.f;
	constexpr float CrewSecondsBetweenKills = 4.f;
	constexpr float CrewStressSeconds = 3.f;
	constexpr float CrewStressDrainSeconds = 4.f;
	// A dying tank every five frames at 30 fps: six a second.
	constexpr int32 CrewStressFramesPerRequest = 5;
	constexpr int32 CrewStressSlots = 16;
	constexpr float CrewStressHeightCm = 150.f;
	constexpr float CrewStressSpreadCm = 80.f;
	constexpr int32 CrewLogFrameInterval = 6;
	const FVector CrewLookOffset(0.f, -3500.f, 0.f);
	constexpr float CrewLookDistanceScale = 0.6f;
	constexpr int32 CrewLookShots = 4;
	constexpr float CrewLookShotSeconds = 2.5f;
	constexpr float PreRollSecondsForLook = 0.5f;
	const TCHAR* const BenchMeshPath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/SK_InfantryMaster.SK_InfantryMaster");
	const FName BenchCollisionProfile(TEXT("RagdollCorpse"));
	constexpr int32 BenchPoolSize = 60;
	constexpr int32 BenchFramesToSample = 60;
	constexpr int32 BenchBatchCount = 10;
	constexpr int32 BenchBodiesPerBatch = 5;
	constexpr int32 BenchFramesPerBatch = 20;
	constexpr int32 BenchSimulationSizes[] = {8, 24, 48};
	constexpr int32 BenchSimulationFrames = 45;
	constexpr int32 BenchSettleFrames = 20;
	constexpr float BenchSlotSpacingCm = 300.f;
	constexpr float BenchSpawnHeightCm = 150.f;
	constexpr float BenchParkDepthCm = 30000.f;
	const FVector BenchLaunchVelocity(250.f, 100.f, 500.f);
	const TCHAR* const DefaultSquadSubtype = TEXT("Squad_Rus_Mosin");

	const FName PelvisBone(TEXT("pelvis"));
	const FName HeadBone(TEXT("head"));
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const TCHAR* const KillNames[] = {TEXT("direct_hit"), TEXT("blast"), TEXT("fire"), TEXT("no_causer")};
	constexpr int32 KillCount = UE_ARRAY_COUNT(KillNames);

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxRunRealSeconds = 600.0;
	constexpr float SpawnTimeoutGameSeconds = 30.f;
	constexpr float SettleGameSeconds = 4.f;
	constexpr int32 PreRollFrames = 15;
	constexpr float SecondsBetweenKills = 1.5f;
	// Long enough for the last soldier to fall, freeze, sink and be destroyed.
	constexpr float RecordAfterLastKillSeconds = 7.f;
	constexpr float LogIntervalSeconds = 0.25f;
	constexpr float BlastCauserDistanceCm = 150.f;

	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	constexpr float PictureFieldOfView = 50.f;
	constexpr float CameraDistanceCm = 2000.f;
	constexpr float CameraHeightCm = 650.f;
	constexpr float GroundTraceUpCm = 20000.f;
	constexpr float GroundTraceDownCm = 40000.f;
}

bool URagdollDemoSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), RagdollDemoPrivate::DemoSwitch);
}

void URagdollDemoSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace RagdollDemoPrivate;
	Super::OnWorldBeginPlay(InWorld);
	FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory);
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText, /*bShouldStopOnSeparator=*/false))
	{
		FString XText;
		FString YText;
		LocationText.Split(TEXT(","), &XText, &YText);
		M_DemoLocation = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
	}
	bM_IsBench = FParse::Param(FCommandLine::Get(), BenchSwitch);
	bM_IsCrew = FParse::Param(FCommandLine::Get(), CrewSwitch);
	bM_IsPoseBake = FParse::Param(FCommandLine::Get(), TEXT("RagdollPoseBake"));
	bM_IsCorpseBench = FParse::Param(FCommandLine::Get(), TEXT("RagdollCorpseBench"));
	M_StartRealSeconds = FPlatformTime::Seconds();
	bM_IsActive = true;
	UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGD started dir=%s at=%s"), *M_OutputDirectory,
	       *M_DemoLocation.ToCompactString());
}

TStatId URagdollDemoSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URagdollDemoSubsystem, STATGROUP_Tickables);
}

void URagdollDemoSubsystem::Tick(float DeltaTime)
{
	using namespace RagdollDemoPrivate;
	if (not bM_IsActive || M_Step == ERagdollDemoStep::Done || not IsValid(GetWorld()))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (RealSeconds > MaxRunRealSeconds)
	{
		Finish(TEXT("timeout"));
		return;
	}

	switch (M_Step)
	{
	case ERagdollDemoStep::StartGame:
		if (bM_IsBench && RealSeconds >= HudBuildSeconds && TryStartGame())
		{
			M_BenchMesh = LoadObject<USkeletalMesh>(nullptr, BenchMeshPath);
			float GroundZ = 0.f;
			TryGetGroundZ(M_DemoLocation, GroundZ);
			M_DemoLocation.Z = GroundZ;
			M_LastBenchTickSeconds = FPlatformTime::Seconds();
			M_Step = ERagdollDemoStep::Bench;
			break;
		}
		if (bM_IsCrew && RealSeconds >= HudBuildSeconds && TryStartGame() && TrySpawnCrewTanks())
		{
			M_Step = ERagdollDemoStep::CrewSpawn;
			M_StepStartGameSeconds = GetGameSeconds();
			break;
		}
		if (bM_IsPoseBake && RealSeconds >= HudBuildSeconds && TryStartGame() && TryStartPoseBake())
		{
			M_Step = ERagdollDemoStep::PoseSettle;
			M_StepStartGameSeconds = GetGameSeconds();
			break;
		}
		if (bM_IsCorpseBench && RealSeconds >= HudBuildSeconds && TryStartGame() && TryStartCorpseBench())
		{
			M_Step = ERagdollDemoStep::CorpseBench;
			break;
		}
		if (not bM_IsBench && not bM_IsCrew && not bM_IsPoseBake && not bM_IsCorpseBench && RealSeconds >= HudBuildSeconds
			&& TryStartGame() && TrySpawnSquad())
		{
			M_Step = ERagdollDemoStep::WaitForSquad;
			M_StepStartGameSeconds = GetGameSeconds();
		}
		break;
	case ERagdollDemoStep::WaitForSquad:
		KeepGameRunning();
		if (TryCollectSubjects())
		{
			M_Step = ERagdollDemoStep::Settle;
			M_StepStartGameSeconds = GetGameSeconds();
		}
		else if (GetGameSeconds() - M_StepStartGameSeconds > SpawnTimeoutGameSeconds)
		{
			Finish(TEXT("squad_not_spawned"));
		}
		break;
	case ERagdollDemoStep::Settle:
		TickSettle();
		break;
	case ERagdollDemoStep::Record:
		TickRecord();
		break;
	case ERagdollDemoStep::Bench:
		TickBench();
		break;
	case ERagdollDemoStep::CrewSpawn:
		TickCrewSpawn();
		break;
	case ERagdollDemoStep::CrewFilm:
		TickCrewFilm();
		break;
	case ERagdollDemoStep::CrewLook:
		TickCrewLook();
		break;
	case ERagdollDemoStep::PoseSettle:
		TickPoseSettle();
		break;
	case ERagdollDemoStep::PoseShoot:
		TickPoseShoot();
		break;
	case ERagdollDemoStep::CorpseBench:
		TickCorpseBench();
		break;
	case ERagdollDemoStep::CrewStress:
	{
		const double NowSeconds = FPlatformTime::Seconds();
		const double FrameMs = (NowSeconds - M_LastBenchTickSeconds) * 1000.0;
		M_LastBenchTickSeconds = NowSeconds;
		TickCrewStress(FrameMs);
		break;
	}
	default:
		break;
	}
}

bool URagdollDemoSubsystem::TryStartGame()
{
	M_PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not M_PlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	KeepGameRunning();
	return true;
}

void URagdollDemoSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(RagdollDemoPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* PlayerController = M_PlayerController.Get();
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool URagdollDemoSubsystem::TrySpawnSquad()
{
	using namespace RagdollDemoPrivate;
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
	if (not IsValid(AsyncSpawner))
	{
		return false;
	}
	FString SquadSubtypeName = DefaultSquadSubtype;
	FParse::Value(FCommandLine::Get(), SquadSwitch, SquadSubtypeName);
	const int64 SquadSubtypeValue = StaticEnum<ESquadSubtype>()->GetValueByNameString(SquadSubtypeName);
	if (SquadSubtypeValue == INDEX_NONE)
	{
		Finish(TEXT("unknown_squad_subtype"));
		return false;
	}
	float GroundZ = 0.f;
	if (not TryGetGroundZ(M_DemoLocation, GroundZ))
	{
		Finish(TEXT("no_ground_at_demo_location"));
		return false;
	}
	M_DemoLocation.Z = GroundZ;
	const FTrainingOption TrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtypeValue));
	UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGD spawn squad=%s at=%s"), *SquadSubtypeName,
	       *M_DemoLocation.ToCompactString());
	return AsyncSpawner->AsyncSpawnOptionAtLocation(
		TrainingOption, M_DemoLocation, this, 0,
		[](const FTrainingOption&, AActor*, const int32)
		{
		},
		FRotator::ZeroRotator);
}

bool URagdollDemoSubsystem::TryCollectSubjects()
{
	using namespace RagdollDemoPrivate;
	TArray<ASquadUnit*> Units;
	for (TActorIterator<ASquadUnit> It(GetWorld()); It; ++It)
	{
		// Test maps hold other soldiers; only the ones with ragdoll switched on are the subject of this film.
		if (IsValid(*It) && It->GetRagdollSettings().GetCanRagdoll(It->GetMesh()))
		{
			Units.Add(*It);
		}
	}
	if (Units.Num() < KillCount)
	{
		return false;
	}
	M_Subjects.Reset();
	for (int32 KillIndex = 0; KillIndex < KillCount; ++KillIndex)
	{
		FRagdollDemoSubject Subject;
		Subject.Unit = Units[KillIndex];
		Subject.KillName = KillNames[KillIndex];
		const USkeletalMeshComponent* Mesh = Units[KillIndex]->GetMesh();
		UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGD subject=%s class=%s kill=%s ragdoll=%d physics_asset=%s"),
		       *Units[KillIndex]->GetName(), *Units[KillIndex]->GetClass()->GetName(), KillNames[KillIndex],
		       Units[KillIndex]->GetRagdollSettings().GetCanRagdoll(Mesh) ? 1 : 0,
		       *GetNameSafe(IsValid(Mesh) ? Mesh->GetPhysicsAsset() : nullptr));
		M_Subjects.Add(Subject);
	}
	return true;
}

void URagdollDemoSubsystem::TickSettle()
{
	using namespace RagdollDemoPrivate;
	KeepGameRunning();
	if (GetGameSeconds() - M_StepStartGameSeconds < SettleGameSeconds)
	{
		return;
	}
	if (not CreateCaptureRig())
	{
		Finish(TEXT("no_capture_rig"));
		return;
	}
	FVector SubjectsCentre = FVector::ZeroVector;
	for (const FRagdollDemoSubject& Subject : M_Subjects)
	{
		if (Subject.Unit.IsValid())
		{
			SubjectsCentre += Subject.Unit->GetActorLocation() / M_Subjects.Num();
		}
	}
	M_FilmCentre = SubjectsCentre;
	AimCapture();
	M_Step = ERagdollDemoStep::Record;
	M_StepStartGameSeconds = GetGameSeconds();
}

void URagdollDemoSubsystem::TickRecord()
{
	using namespace RagdollDemoPrivate;
	KeepGameRunning();
	// Fog of war hides enemy soldiers from the capture; the film has to show them anyway.
	for (const FRagdollDemoSubject& Subject : M_Subjects)
	{
		if (Subject.Unit.IsValid())
		{
			Subject.Unit->SetActorHiddenInGame(false);
		}
	}
	if (not M_OutputDirectory.IsEmpty() && IsValid(M_RenderTarget))
	{
		UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), M_RenderTarget, M_OutputDirectory,
		                                            FString::Printf(TEXT("frame_%04d.png"), M_FrameIndex));
	}
	++M_FrameIndex;
	if (M_FrameIndex < PreRollFrames)
	{
		return;
	}
	const float GameSeconds = GetGameSeconds();
	if (M_NextKillIndex < M_Subjects.Num() && GameSeconds >= M_NextKillGameSeconds)
	{
		KillSubject(M_NextKillIndex);
		++M_NextKillIndex;
		M_NextKillGameSeconds = GameSeconds + SecondsBetweenKills;
		M_StepStartGameSeconds = GameSeconds;
	}
	if (GameSeconds >= M_NextLogGameSeconds)
	{
		M_NextLogGameSeconds = GameSeconds + LogIntervalSeconds;
		LogSubjects();
	}
	if (M_NextKillIndex >= M_Subjects.Num() && GameSeconds - M_StepStartGameSeconds >= RecordAfterLastKillSeconds)
	{
		ReportSubjects();
		Finish(TEXT("done"));
	}
}

void URagdollDemoSubsystem::KillSubject(const int32 SubjectIndex)
{
	using namespace RagdollDemoPrivate;
	FRagdollDemoSubject& Subject = M_Subjects[SubjectIndex];
	ASquadUnit* Unit = Subject.Unit.Get();
	if (not IsValid(Unit))
	{
		return;
	}
	const USkeletalMeshComponent* Mesh = Unit->GetMesh();
	Subject.DeathLocation = Mesh->GetBoneLocation(PelvisBone);
	Subject.DeathHeadHeightCm = Mesh->GetBoneLocation(HeadBone).Z - M_DemoLocation.Z;
	const UHealthComponent* HealthComponent = Unit->FindComponentByClass<UHealthComponent>();
	const float FatalDamage = IsValid(HealthComponent) ? HealthComponent->GetMaxHealth() * 2.f : 100000.f;
	UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGD kill=%s t=%.2f frame=%d"), *Subject.KillName, GetGameSeconds(),
	       M_FrameIndex);

	if (Subject.KillName == KillNames[0])
	{
		FPointDamageEvent DamageEvent;
		DamageEvent.DamageTypeClass = FRTSWeaponHelpers::GetDamageTypeClass(ERTSDamageType::Kinetic);
		AActor* FakeProjectile = M_Capture.Get();
		FakeProjectile->SetActorRotation(FRotator::ZeroRotator);
		Unit->TakeDamage(FatalDamage, DamageEvent, nullptr, FakeProjectile);
		AimCapture();
		// Bodies only exist once the corpse has collision, so this cannot be asked of a living soldier.
		FString BodyNames;
		for (const FBodyInstance* Body : Mesh->Bodies)
		{
			BodyNames += Body != nullptr && Body->BodySetup.IsValid()
				             ? Body->BodySetup->BoneName.ToString() + TEXT(" ")
				             : TEXT("? ");
		}
		UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGD bodies=%s kick_bone_has_body=%d"), *BodyNames,
		       Mesh->GetBodyInstance(Unit->GetRagdollSettings().ImpulseBoneName) != nullptr ? 1 : 0);
		return;
	}
	if (Subject.KillName == KillNames[1])
	{
		FRTSBlastDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBlastDamageEvent(
			ERTSDamageType::Kinetic, Unit->GetActorLocation() + FVector(BlastCauserDistanceCm, 0.f, 0.f));
		Unit->TakeDamage(FatalDamage, DamageEvent, nullptr, nullptr);
		return;
	}
	if (Subject.KillName == KillNames[2])
	{
		FDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(ERTSDamageType::Fire);
		Unit->TakeDamage(FatalDamage, DamageEvent, nullptr, nullptr);
		return;
	}
	FDamageEvent DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(ERTSDamageType::Kinetic);
	Unit->TakeDamage(FatalDamage, DamageEvent, nullptr, nullptr);
}

void URagdollDemoSubsystem::LogSubjects()
{
	using namespace RagdollDemoPrivate;
	for (FRagdollDemoSubject& Subject : M_Subjects)
	{
		const ASquadUnit* Unit = Subject.Unit.Get();
		if (not IsValid(Unit) || Subject.DeathLocation.IsZero())
		{
			continue;
		}
		const USkeletalMeshComponent* Mesh = Unit->GetMesh();
		const FVector Pelvis = Mesh->GetBoneLocation(PelvisBone);
		const float Height = Pelvis.Z - M_DemoLocation.Z;
		const float Distance = FVector::Dist2D(Pelvis, Subject.DeathLocation);
		Subject.MaxHeightCm = FMath::Max(Subject.MaxHeightCm, Height);
		Subject.MaxDistanceCm = FMath::Max(Subject.MaxDistanceCm, Distance);
		UE_LOG(LogRTSRagdollDemo, Display,
		       TEXT("RTS_RGD body=%s t=%.2f pelvis_h=%.0f head_h=%.0f dist=%.0f sim=%d attached=%d"),
		       *Subject.KillName, GetGameSeconds(), Height, Mesh->GetBoneLocation(HeadBone).Z - M_DemoLocation.Z,
		       Distance, Mesh->IsSimulatingPhysics() ? 1 : 0, Mesh->GetAttachParent() != nullptr ? 1 : 0);
	}
}

void URagdollDemoSubsystem::ReportSubjects() const
{
	for (const FRagdollDemoSubject& Subject : M_Subjects)
	{
		UE_LOG(LogRTSRagdollDemo, Display,
		       TEXT("RTS_RGD SUMMARY body=%s max_pelvis_h=%.0f max_dist=%.0f removed=%d"),
		       *Subject.KillName, Subject.MaxHeightCm, Subject.MaxDistanceCm, Subject.Unit.IsValid() ? 0 : 1);
	}
}

bool URagdollDemoSubsystem::CreateCaptureRig()
{
	using namespace RagdollDemoPrivate;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* Capture = GetWorld()->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
	                                                                   FTransform::Identity, SpawnParameters);
	M_RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), PictureWidth, PictureHeight,
	                                                               RTF_RGBA8_SRGB);
	if (not IsValid(Capture) || not IsValid(M_RenderTarget))
	{
		return false;
	}
	M_Capture = Capture;
	USceneCaptureComponent2D* CaptureComponent = Capture->GetCaptureComponent2D();
	CaptureComponent->TextureTarget = M_RenderTarget;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->FOVAngle = PictureFieldOfView;
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the film shows the bodies, not what the player may see.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	return true;
}

void URagdollDemoSubsystem::AimCapture() const
{
	using namespace RagdollDemoPrivate;
	ASceneCapture2D* Capture = M_Capture.Get();
	if (not IsValid(Capture))
	{
		return;
	}
	const FVector CameraLocation = M_FilmCentre + FVector(0.f, -CameraDistanceCm, CameraHeightCm) * M_FilmDistanceScale;
	Capture->SetActorLocationAndRotation(CameraLocation, (M_FilmCentre - CameraLocation).Rotation());
}

bool URagdollDemoSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	using namespace RagdollDemoPrivate;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RagdollDemoGround), false);
	FHitResult GroundHit;
	const FVector TraceStart(Location.X, Location.Y, Location.Z + GroundTraceUpCm);
	const FVector TraceEnd(Location.X, Location.Y, Location.Z - GroundTraceDownCm);
	if (not GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE,
	                                             QueryParams))
	{
		return false;
	}
	OutGroundZ = GroundHit.ImpactPoint.Z;
	return true;
}

float URagdollDemoSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

void URagdollDemoSubsystem::Finish(const TCHAR* Reason)
{
	M_Step = ERagdollDemoStep::Done;
	UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGD RESULT reason=%s game_t=%.1f real_s=%.0f"), Reason,
	       GetGameSeconds(), FPlatformTime::Seconds() - M_StartRealSeconds);
	FPlatformMisc::RequestExit(false, TEXT("RagdollDemo"));
}

// ---------------------------------------------------------------------------------------------------------------
// Crew bodies out of dying tanks

bool URagdollDemoSubsystem::TrySpawnCrewTanks()
{
	using namespace RagdollDemoPrivate;
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
	if (not IsValid(AsyncSpawner))
	{
		return false;
	}
	float GroundZ = 0.f;
	if (not TryGetGroundZ(M_DemoLocation, GroundZ))
	{
		Finish(TEXT("no_ground_at_demo_location"));
		return false;
	}
	M_DemoLocation.Z = GroundZ;
	M_FilmCentre = M_DemoLocation;
	M_CrewTanks.SetNum(CrewTankCount);
	// The film has to show every death, whatever the player's camera is looking at.
	GetMutableDefault<URTSRagdollSettings>()->bSkipUnseenVehicles = false;
	const TWeakObjectPtr<URagdollDemoSubsystem> WeakThis(this);
	for (int32 TankIndex = 0; TankIndex < CrewTankCount; ++TankIndex)
	{
		const FVector Location = M_DemoLocation + FVector(TankIndex == 0 ? -CrewTankOffsetCm : CrewTankOffsetCm, 0.f, 0.f);
		const FTrainingOption TrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzIV_H));
		AsyncSpawner->AsyncSpawnOptionAtLocation(
			TrainingOption, Location, this, TankIndex,
			[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32 SpawnedIndex)
			{
				if (WeakThis.IsValid() && WeakThis->M_CrewTanks.IsValidIndex(SpawnedIndex))
				{
					WeakThis->M_CrewTanks[SpawnedIndex] = SpawnedActor;
				}
			},
			FRotator::ZeroRotator);
	}
	return true;
}

void URagdollDemoSubsystem::TickCrewSpawn()
{
	using namespace RagdollDemoPrivate;
	KeepGameRunning();
	const bool bHasTanks = M_CrewTanks.Num() == CrewTankCount && M_CrewTanks[0].IsValid() && M_CrewTanks[1].IsValid();
	if (not bHasTanks)
	{
		if (GetGameSeconds() - M_StepStartGameSeconds > SpawnTimeoutGameSeconds)
		{
			Finish(TEXT("tanks_not_spawned"));
		}
		return;
	}
	if (GetGameSeconds() - M_StepStartGameSeconds < CrewTankSettleSeconds)
	{
		return;
	}
	if (not CreateCaptureRig())
	{
		Finish(TEXT("no_capture_rig"));
		return;
	}
	for (int32 TankIndex = 0; TankIndex < CrewTankCount; ++TankIndex)
	{
		URTSComponent* RTSComponent = M_CrewTanks[TankIndex]->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent))
		{
			RTSComponent->SetOwningPlayerRuntime(TankIndex == 0 ? 1 : 2);
		}
	}
	AimCapture();
	M_Step = ERagdollDemoStep::CrewFilm;
	M_StepStartGameSeconds = GetGameSeconds();
}

void URagdollDemoSubsystem::TickCrewFilm()
{
	using namespace RagdollDemoPrivate;
	KeepGameRunning();
	for (const TWeakObjectPtr<AActor>& Tank : M_CrewTanks)
	{
		if (Tank.IsValid())
		{
			Tank->SetActorHiddenInGame(false);
		}
	}
	if (not M_OutputDirectory.IsEmpty() && IsValid(M_RenderTarget))
	{
		UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), M_RenderTarget, M_OutputDirectory,
		                                            FString::Printf(TEXT("frame_%04d.png"), M_FrameIndex));
	}
	++M_FrameIndex;
	if (M_FrameIndex < PreRollFrames)
	{
		return;
	}
	if (M_FrameIndex % CrewLogFrameInterval == 0)
	{
		LogWokenCrewBodies();
	}
	const float SecondsSinceKill = GetGameSeconds() - M_StepStartGameSeconds;
	if (M_CrewKills == 0)
	{
		// The player's tank dies in a cook-off, the enemy's by ordinary fire.
		if (ATankMaster* Tank = Cast<ATankMaster>(M_CrewTanks[0].Get()))
		{
			UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGD kill=player_cookoff t=%.2f frame=%d"), GetGameSeconds(), M_FrameIndex);
			Tank->Debug_DieDuringAmmoCookOff();
		}
		M_CrewKills = 1;
		M_StepStartGameSeconds = GetGameSeconds();
		return;
	}
	if (M_CrewKills == 1 && SecondsSinceKill >= CrewSecondsBetweenKills)
	{
		if (ATankMaster* Tank = Cast<ATankMaster>(M_CrewTanks[1].Get()))
		{
			UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGD kill=enemy_ordinary t=%.2f frame=%d"), GetGameSeconds(), M_FrameIndex);
			Tank->TakeFatalDamage();
		}
		M_CrewKills = 2;
		M_StepStartGameSeconds = GetGameSeconds();
		return;
	}
	if (M_CrewKills == 2 && SecondsSinceKill >= CrewSecondsBetweenKills)
	{
		M_LastBenchTickSeconds = FPlatformTime::Seconds();
		M_StepStartGameSeconds = GetGameSeconds();
		M_Step = ERagdollDemoStep::CrewStress;
	}
}

void URagdollDemoSubsystem::TickCrewStress(const double FrameMs)
{
	using namespace RagdollDemoPrivate;
	KeepGameRunning();
	URTSRagdollSubsystem* RagdollSubsystem = GetWorld()->GetSubsystem<URTSRagdollSubsystem>();
	if (not IsValid(RagdollSubsystem))
	{
		Finish(TEXT("no_ragdoll_subsystem"));
		return;
	}
	M_MaxSimulating = FMath::Max(M_MaxSimulating, RagdollSubsystem->GetSimulatingRagdollCount());
	M_SampleTotalMs += FrameMs;
	M_SampleWorstMs = FMath::Max(M_SampleWorstMs, FrameMs);
	++M_SampleCount;
	const float Elapsed = GetGameSeconds() - M_StepStartGameSeconds;
	if (Elapsed < CrewStressSeconds && M_SampleCount % CrewStressFramesPerRequest == 0)
	{
		FRTSCrewBodyRequest Request;
		Request.TopCentre = GetBenchSlot(M_CrewRequests % CrewStressSlots) + FVector(0.f, 0.f, CrewStressHeightCm);
		Request.SpreadRadius = CrewStressSpreadCm;
		Request.Side = M_CrewRequests % 2 == 0 ? ERTSCrewSide::German : ERTSCrewSide::Soviet;
		Request.bIsCookOff = M_CrewRequests % 4 == 0;
		RagdollSubsystem->RequestCrewBodies(Request);
		++M_CrewRequests;
	}
	if (Elapsed < CrewStressSeconds + CrewStressDrainSeconds)
	{
		return;
	}
	UE_LOG(LogRTSRagdollDemo, Display,
	       TEXT("RTS_RGC STRESS requests=%d max_simulating=%d refused=%d mean_ms=%.2f worst_ms=%.2f parked_ger=%d parked_rus=%d simulating_end=%d"),
	       M_CrewRequests, M_MaxSimulating, RagdollSubsystem->GetBodiesRefused(), M_SampleTotalMs / M_SampleCount,
	       M_SampleWorstMs, RagdollSubsystem->GetParkedBodyCount(ERTSCrewSide::German),
	       RagdollSubsystem->GetParkedBodyCount(ERTSCrewSide::Soviet), RagdollSubsystem->GetSimulatingRagdollCount());
	// Everything from here on is a body in its second life or later.
	M_FilmCentre = M_DemoLocation + CrewLookOffset;
	float GroundZ = 0.f;
	if (TryGetGroundZ(M_FilmCentre, GroundZ))
	{
		M_FilmCentre.Z = GroundZ;
	}
	M_FilmDistanceScale = CrewLookDistanceScale;
	AimCapture();
	M_FrameIndex = 0;
	M_StepStartGameSeconds = GetGameSeconds();
	M_Step = ERagdollDemoStep::CrewLook;
}

void URagdollDemoSubsystem::TickCrewLook()
{
	using namespace RagdollDemoPrivate;
	KeepGameRunning();
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), M_RenderTarget, M_OutputDirectory,
	                                            FString::Printf(TEXT("look_%04d.png"), M_FrameIndex));
	++M_FrameIndex;
	if (M_FrameIndex % CrewLogFrameInterval == 0)
	{
		LogWokenCrewBodies();
	}
	URTSRagdollSubsystem* RagdollSubsystem = GetWorld()->GetSubsystem<URTSRagdollSubsystem>();
	const float Elapsed = GetGameSeconds() - M_StepStartGameSeconds;
	if (M_CrewLookRequests < CrewLookShots && Elapsed >= PreRollSecondsForLook + M_CrewLookRequests * CrewLookShotSeconds)
	{
		FRTSCrewBodyRequest Request;
		Request.TopCentre = M_FilmCentre + FVector(0.f, 0.f, CrewStressHeightCm);
		Request.SpreadRadius = CrewStressSpreadCm;
		Request.Side = M_CrewLookRequests < CrewLookShots / 2 ? ERTSCrewSide::German : ERTSCrewSide::Soviet;
		Request.bIsCookOff = M_CrewLookRequests % 2 == 0;
		RagdollSubsystem->RequestCrewBodies(Request);
		++M_CrewLookRequests;
	}
	if (Elapsed >= PreRollSecondsForLook + CrewLookShots * CrewLookShotSeconds)
	{
		Finish(TEXT("crew_done"));
	}
}

void URagdollDemoSubsystem::LogWokenCrewBodies() const
{
	using namespace RagdollDemoPrivate;
	for (TActorIterator<ARTSPooledRagdoll> It(GetWorld()); It; ++It)
	{
		if (It->IsHidden())
		{
			continue;
		}
		const USkeletalMeshComponent* Mesh = It->FindComponentByClass<USkeletalMeshComponent>();
		const FVector Pelvis = Mesh->GetBoneLocation(PelvisBone);
		UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGC body=%s t=%.2f pelvis_h=%.0f head_h=%.0f x=%.0f y=%.0f sim=%d"),
		       *It->GetName(), GetGameSeconds(), Pelvis.Z - M_DemoLocation.Z,
		       Mesh->GetBoneLocation(HeadBone).Z - M_DemoLocation.Z, Pelvis.X - M_DemoLocation.X,
		       Pelvis.Y - M_DemoLocation.Y, Mesh->IsSimulatingPhysics() ? 1 : 0);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Spawn against pool benchmark

void URagdollDemoSubsystem::TickBench()
{
	using namespace RagdollDemoPrivate;
	KeepGameRunning();
	const double NowSeconds = FPlatformTime::Seconds();
	const double FrameMs = (NowSeconds - M_LastBenchTickSeconds) * 1000.0;
	M_LastBenchTickSeconds = NowSeconds;
	if (not IsValid(M_BenchMesh))
	{
		Finish(TEXT("bench_mesh_missing"));
		return;
	}
	switch (M_BenchPhase)
	{
	case 0:
		M_BenchPhase += SampleFrames(FrameMs, TEXT("no_pool"), BenchFramesToSample) ? 1 : 0;
		break;
	case 1:
		PrewarmBenchPool(M_PoolWithoutBodies, false, TEXT("without_bodies"));
		++M_BenchPhase;
		break;
	case 2:
		M_BenchPhase += SampleFrames(FrameMs, TEXT("idle_pool_without_bodies"), BenchFramesToSample) ? 1 : 0;
		break;
	case 3:
		PrewarmBenchPool(M_PoolWithBodies, true, TEXT("with_bodies"));
		++M_BenchPhase;
		break;
	case 4:
		M_BenchPhase += SampleFrames(FrameMs, TEXT("idle_both_pools"), BenchFramesToSample) ? 1 : 0;
		break;
	case 5:
		TickBenchBatches();
		break;
	default:
		TickBenchSimulation(FrameMs);
		break;
	}
}

bool URagdollDemoSubsystem::SampleFrames(const double FrameMs, const TCHAR* Label, const int32 Count)
{
	// The frames right after a change still carry its hitch.
	if (++M_BenchFrame <= RagdollDemoPrivate::BenchSettleFrames)
	{
		return false;
	}
	M_SampleTotalMs += FrameMs;
	M_SampleWorstMs = FMath::Max(M_SampleWorstMs, FrameMs);
	if (++M_SampleCount < Count)
	{
		return false;
	}
	UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGB frames=%s mean_ms=%.2f worst_ms=%.2f"), Label,
	       M_SampleTotalMs / M_SampleCount, M_SampleWorstMs);
	M_SampleTotalMs = 0.0;
	M_SampleWorstMs = 0.0;
	M_SampleCount = 0;
	M_BenchFrame = 0;
	return true;
}

ASkeletalMeshActor* URagdollDemoSubsystem::SpawnBenchBody(const bool bKeepBodiesReady) const
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkeletalMeshActor* Body = GetWorld()->SpawnActor<ASkeletalMeshActor>(
		ASkeletalMeshActor::StaticClass(), FTransform(M_DemoLocation), SpawnParameters);
	USkeletalMeshComponent* Mesh = Body->GetSkeletalMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetSkeletalMeshAsset(M_BenchMesh);
	Mesh->bPauseAnims = true;
	Mesh->bBlendPhysics = true;
	ParkBenchBody(Body, bKeepBodiesReady);
	return Body;
}

void URagdollDemoSubsystem::ParkBenchBody(ASkeletalMeshActor* Body, const bool bKeepBodiesReady) const
{
	using namespace RagdollDemoPrivate;
	USkeletalMeshComponent* Mesh = Body->GetSkeletalMeshComponent();
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionProfileName(bKeepBodiesReady ? BenchCollisionProfile : FName(TEXT("NoCollision")));
	Mesh->SetComponentTickEnabled(false);
	Body->SetActorHiddenInGame(true);
	Body->SetActorLocation(M_DemoLocation - FVector(0.f, 0.f, BenchParkDepthCm), false, nullptr,
	                       ETeleportType::ResetPhysics);
}

void URagdollDemoSubsystem::WakeBenchBody(ASkeletalMeshActor* Body, const FVector& Location) const
{
	using namespace RagdollDemoPrivate;
	USkeletalMeshComponent* Mesh = Body->GetSkeletalMeshComponent();
	Body->SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);
	Mesh->UpdateKinematicBonesToAnim(Mesh->GetComponentSpaceTransforms(), ETeleportType::TeleportPhysics, false,
	                                 EAllowKinematicDeferral::DisallowDeferral);
	Body->SetActorHiddenInGame(false);
	Mesh->SetComponentTickEnabled(true);
	Mesh->SetCollisionProfileName(BenchCollisionProfile);
	Mesh->SetSimulatePhysics(true);
}

void URagdollDemoSubsystem::PrewarmBenchPool(TArray<TObjectPtr<ASkeletalMeshActor>>& OutPool,
                                             const bool bKeepBodiesReady, const TCHAR* Label)
{
	using namespace RagdollDemoPrivate;
	const double StartSeconds = FPlatformTime::Seconds();
	for (int32 BodyIndex = 0; BodyIndex < BenchPoolSize; ++BodyIndex)
	{
		OutPool.Add(SpawnBenchBody(bKeepBodiesReady));
	}
	UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGB prewarm=%s ms_per_body=%.3f bodies_on_parked=%d"), Label,
	       (FPlatformTime::Seconds() - StartSeconds) * 1000.0 / BenchPoolSize,
	       OutPool[0]->GetSkeletalMeshComponent()->Bodies.Num());
}

FVector URagdollDemoSubsystem::GetBenchSlot(const int32 SlotIndex) const
{
	using namespace RagdollDemoPrivate;
	constexpr int32 SlotColumns = 8;
	return M_DemoLocation + FVector((SlotIndex % SlotColumns) * BenchSlotSpacingCm,
	                                (SlotIndex / SlotColumns) * BenchSlotSpacingCm, BenchSpawnHeightCm);
}

void URagdollDemoSubsystem::TickBenchBatches()
{
	using namespace RagdollDemoPrivate;
	static const TCHAR* const ModeNames[] = {
		TEXT("spawn_each_time"), TEXT("pool_without_bodies"), TEXT("pool_with_bodies")
	};
	const int32 Frame = M_BenchFrame++;
	if (Frame == 0)
	{
		for (int32 BodyIndex = 0; BodyIndex < BenchBodiesPerBatch; ++BodyIndex)
		{
			const FVector Slot = GetBenchSlot(BodyIndex);
			const double StartSeconds = FPlatformTime::Seconds();
			ASkeletalMeshActor* Body = M_BenchMode == 0
				                           ? SpawnBenchBody(false)
				                           : M_BenchMode == 1
				                           ? M_PoolWithoutBodies[BodyIndex].Get()
				                           : M_PoolWithBodies[BodyIndex].Get();
			WakeBenchBody(Body, Slot);
			const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
			M_ActivateTotalMs += ElapsedMs;
			M_ActivateWorstMs = FMath::Max(M_ActivateWorstMs, ElapsedMs);
			M_BenchActive.Add(Body);
		}
		return;
	}
	if (Frame == 1)
	{
		for (ASkeletalMeshActor* Body : M_BenchActive)
		{
			Body->GetSkeletalMeshComponent()->SetAllPhysicsLinearVelocity(BenchLaunchVelocity, false);
		}
		return;
	}
	if (Frame < BenchFramesPerBatch)
	{
		return;
	}
	const double StartSeconds = FPlatformTime::Seconds();
	for (ASkeletalMeshActor* Body : M_BenchActive)
	{
		if (M_BenchMode == 0)
		{
			Body->Destroy();
			continue;
		}
		ParkBenchBody(Body, M_BenchMode == 2);
	}
	M_ReleaseTotalMs += (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	M_BenchActive.Reset();
	M_BenchFrame = 0;
	if (++M_BenchBatch < BenchBatchCount)
	{
		return;
	}
	const int32 Bodies = BenchBatchCount * BenchBodiesPerBatch;
	UE_LOG(LogRTSRagdollDemo, Display,
	       TEXT("RTS_RGB mode=%s activate_ms_per_body=%.3f worst_ms=%.3f release_ms_per_body=%.3f"),
	       ModeNames[M_BenchMode], M_ActivateTotalMs / Bodies, M_ActivateWorstMs, M_ReleaseTotalMs / Bodies);
	M_ActivateTotalMs = 0.0;
	M_ActivateWorstMs = 0.0;
	M_ReleaseTotalMs = 0.0;
	M_BenchBatch = 0;
	if (++M_BenchMode >= UE_ARRAY_COUNT(ModeNames))
	{
		++M_BenchPhase;
	}
}

void URagdollDemoSubsystem::TickBenchSimulation(const double FrameMs)
{
	using namespace RagdollDemoPrivate;
	const int32 BodyCount = BenchSimulationSizes[M_BenchSizeIndex];
	if (M_BenchActive.IsEmpty())
	{
		for (int32 BodyIndex = 0; BodyIndex < BodyCount; ++BodyIndex)
		{
			ASkeletalMeshActor* Body = M_PoolWithBodies[BodyIndex];
			WakeBenchBody(Body, GetBenchSlot(BodyIndex));
			M_BenchActive.Add(Body);
		}
		return;
	}
	if (M_SampleCount == 0 && M_BenchFrame == 0)
	{
		for (ASkeletalMeshActor* Body : M_BenchActive)
		{
			Body->GetSkeletalMeshComponent()->SetAllPhysicsLinearVelocity(BenchLaunchVelocity, false);
		}
		// Measured straight away: flying and landing is the expensive part of a ragdoll's life.
		M_BenchFrame = BenchSettleFrames;
	}
	const FString Label = FString::Printf(TEXT("simulating_%d"), BodyCount);
	if (M_SampleCount == BenchSimulationFrames / 2)
	{
		const USkeletalMeshComponent* Mesh = M_BenchActive[0]->GetSkeletalMeshComponent();
		UE_LOG(LogRTSRagdollDemo, Display, TEXT("RTS_RGB body_check size=%d pelvis_h=%.0f"), BodyCount,
		       Mesh->GetBoneLocation(PelvisBone).Z - M_DemoLocation.Z);
	}
	if (not SampleFrames(FrameMs, *Label, BenchSimulationFrames))
	{
		return;
	}
	for (ASkeletalMeshActor* Body : M_BenchActive)
	{
		ParkBenchBody(Body, true);
	}
	M_BenchActive.Reset();
	if (++M_BenchSizeIndex >= UE_ARRAY_COUNT(BenchSimulationSizes))
	{
		Finish(TEXT("bench_done"));
	}
}

#else

bool URagdollDemoSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void URagdollDemoSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void URagdollDemoSubsystem::Tick(float DeltaTime)
{
}

TStatId URagdollDemoSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URagdollDemoSubsystem, STATGROUP_Tickables);
}

#endif
