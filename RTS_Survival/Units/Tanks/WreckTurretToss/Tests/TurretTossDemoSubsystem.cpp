// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TurretTossDemoSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "UObject/UObjectIterator.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSettings.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckEngineFireDeathComponent.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckTurretTossComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTurretTossDemo, Log, All);

namespace TurretTossDemoPrivate
{
	const TCHAR* const DemoSwitch = TEXT("TurretTossDemo");
	const TCHAR* const DirectorySwitch = TEXT("TurretTossDemoDir=");
	const TCHAR* const LocationSwitch = TEXT("TurretTossDemoAt=");
	const TCHAR* const TankSwitch = TEXT("TurretTossDemoTank=");
	// CookOff (default), EngineFire or Both.
	const TCHAR* const DeathSwitch = TEXT("TurretTossDemoDeath=");
	// Regular deaths: Standard (default), ChainReaction or Debris, and the pretend killing projectile's yaw.
	const TCHAR* const RegularVariantSwitch = TEXT("RegularDeathVariant=");
	const TCHAR* const KillShotYawSwitch = TEXT("KillShotYaw=");
	const TCHAR* const DefaultTankSubtype = TEXT("Tank_PzIV_H");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxRunRealSeconds = 900.0;
	constexpr float SpawnTimeoutGameSeconds = 30.f;
	// Lets the tank finish its module setup and settle on its suspension before it dies.
	constexpr float SettleGameSeconds = 4.f;
	constexpr int32 PreRollFrames = 20;
	constexpr float RecordAfterDeathGameSeconds = 9.f;
	constexpr float TurretLogIntervalSeconds = 0.25f;
	constexpr int32 FramesPerStill = 4;

	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	constexpr float PictureFieldOfView = 60.f;
	// Film camera: rear-right quarter, high enough to see the turret arc and the open hull ring.
	constexpr float FilmOrbitYawDegrees = 140.f;
	constexpr float FilmDistanceCm = 2500.f;
	constexpr float FilmHeightCm = 650.f;
	// High look-at so the whole turret arc (about 13 m) stays in frame.
	const FVector FilmLookAtOffset(0.f, 0.f, 600.f);
	// -TurretTossDemoCloseFilm: near and low enough to see small debris, for the regular deaths.
	const TCHAR* const CloseFilmSwitch = TEXT("TurretTossDemoCloseFilm");
	// -TurretTossDemoRecordSeconds=<s>: film longer after the death, e.g. to watch regular-death debris sink away.
	const TCHAR* const RecordSecondsSwitch = TEXT("TurretTossDemoRecordSeconds=");
	constexpr float CloseFilmDistanceCm = 1300.f;
	constexpr float CloseFilmHeightCm = 450.f;
	const FVector CloseFilmLookAtOffset(0.f, 0.f, 150.f);
	const FVector StillLookAtOffset(0.f, 0.f, 80.f);
	// Stills after the turret landed: orbit yaw, distance and height per picture.
	const FVector StillCameras[] = {
		FVector(40.f, 1500.f, 900.f),
		FVector(-110.f, 1700.f, 500.f),
		FVector(180.f, 900.f, 1400.f)
	};

	const TCHAR* const AuditSwitch = TEXT("TurretTossAudit");
	constexpr int32 AuditBatchColumns = 4;
	constexpr int32 AuditBatchSize = AuditBatchColumns * AuditBatchColumns;
	// Far enough apart that a thrown turret never lands on a neighbour's wreck.
	constexpr float AuditSpacingCm = 4500.f;
	constexpr float AuditSpawnTimeoutGameSeconds = 25.f;
	constexpr float AuditObserveGameSeconds = 10.f;
	// A wreck within this distance of where its tank stood belongs to that tank.
	constexpr float AuditWreckSearchRadiusCm = 1500.f;
	// A turret that rose this far above its tank's origin was really thrown.
	constexpr float AuditMinThrowHeightCm = 300.f;
	// An engine deck that rose this far above its tank's origin was really thrown (it starts on the hull at ~2 m).
	constexpr float AuditMinDeckThrowHeightCm = 400.f;
	const FName EngineDeckComponentName = TEXT("EngineDeck");

	constexpr float GroundTraceUpCm = 20000.f;
	constexpr float GroundTraceDownCm = 40000.f;
}

bool UTurretTossDemoSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TurretTossDemoPrivate::DemoSwitch);
}

void UTurretTossDemoSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace TurretTossDemoPrivate;
	Super::OnWorldBeginPlay(InWorld);
	FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory);
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText))
	{
		FString XText;
		FString YText;
		LocationText.Split(TEXT(","), &XText, &YText);
		M_DemoLocation = FVector(FCString::Atof(*XText), FCString::Atof(*YText), 0.f);
	}
	bM_IsAudit = FParse::Param(FCommandLine::Get(), AuditSwitch);
	bM_IsCloseFilm = FParse::Param(FCommandLine::Get(), CloseFilmSwitch);
	M_RecordAfterDeathGameSeconds = RecordAfterDeathGameSeconds;
	FParse::Value(FCommandLine::Get(), RecordSecondsSwitch, M_RecordAfterDeathGameSeconds);
	FString DeathText;
	if (FParse::Value(FCommandLine::Get(), DeathSwitch, DeathText))
	{
		M_DeathKind = DeathText.Equals(TEXT("EngineFire"), ESearchCase::IgnoreCase)
			              ? ETurretTossDemoDeath::EngineFire
			              : DeathText.Equals(TEXT("Both"), ESearchCase::IgnoreCase)
			              ? ETurretTossDemoDeath::Both
			              : DeathText.Equals(TEXT("LeakDrive"), ESearchCase::IgnoreCase)
			              ? ETurretTossDemoDeath::LeakDrive
			              : DeathText.Equals(TEXT("Regular"), ESearchCase::IgnoreCase)
			              ? ETurretTossDemoDeath::Regular
			              : ETurretTossDemoDeath::CookOff;
	}
	ParseRegularDeathSwitches();
	M_StartRealSeconds = FPlatformTime::Seconds();
	bM_IsActive = true;
	UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTD started dir=%s at=%s"), *M_OutputDirectory,
	       *M_DemoLocation.ToCompactString());
}

TStatId UTurretTossDemoSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTurretTossDemoSubsystem, STATGROUP_Tickables);
}

void UTurretTossDemoSubsystem::Tick(float DeltaTime)
{
	using namespace TurretTossDemoPrivate;
	if (not bM_IsActive || M_Step == ETurretTossDemoStep::Done || not IsValid(GetWorld()))
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
	case ETurretTossDemoStep::StartGame:
		if (bM_IsAudit && RealSeconds >= HudBuildSeconds && TryStartGame())
		{
			BuildAuditEntries();
			M_Step = ETurretTossDemoStep::AuditSpawnBatch;
			break;
		}
		if (not bM_IsAudit && RealSeconds >= HudBuildSeconds && TryStartGame() && TrySpawnTank())
		{
			M_Step = ETurretTossDemoStep::WaitForTank;
			M_StepStartGameSeconds = GetGameSeconds();
		}
		break;
	case ETurretTossDemoStep::WaitForTank:
		KeepGameRunning();
		if (M_Tank.IsValid())
		{
			M_Step = ETurretTossDemoStep::Settle;
			M_StepStartGameSeconds = GetGameSeconds();
		}
		else if (GetGameSeconds() - M_StepStartGameSeconds > SpawnTimeoutGameSeconds)
		{
			Finish(TEXT("tank_not_spawned"));
		}
		break;
	case ETurretTossDemoStep::Settle:
		TickSettle();
		break;
	case ETurretTossDemoStep::Record:
		TickRecord();
		break;
	case ETurretTossDemoStep::Stills:
		TickStills();
		break;
	case ETurretTossDemoStep::AuditSpawnBatch:
		TickAuditSpawnBatch();
		break;
	case ETurretTossDemoStep::AuditSettle:
		TickAuditSettle();
		break;
	case ETurretTossDemoStep::AuditObserve:
		TickAuditObserve();
		break;
	default:
		break;
	}
}

bool UTurretTossDemoSubsystem::TryStartGame()
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

void UTurretTossDemoSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(TurretTossDemoPrivate::DifficultyButtonName));
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

bool UTurretTossDemoSubsystem::TrySpawnTank()
{
	using namespace TurretTossDemoPrivate;
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
	if (not IsValid(AsyncSpawner))
	{
		return false;
	}
	FString TankSubtypeName = DefaultTankSubtype;
	FParse::Value(FCommandLine::Get(), TankSwitch, TankSubtypeName);
	const int64 TankSubtypeValue = StaticEnum<ETankSubtype>()->GetValueByNameString(TankSubtypeName);
	if (TankSubtypeValue == INDEX_NONE)
	{
		Finish(TEXT("unknown_tank_subtype"));
		return false;
	}

	float GroundZ = 0.f;
	if (not TryGetGroundZ(M_DemoLocation, GroundZ))
	{
		Finish(TEXT("no_ground_at_demo_location"));
		return false;
	}
	M_DemoLocation.Z = GroundZ;
	const FTrainingOption TrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtypeValue));
	TWeakObjectPtr<UTurretTossDemoSubsystem> WeakThis(this);
	UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTD spawn tank=%s at=%s"), *TankSubtypeName,
	       *M_DemoLocation.ToCompactString());
	return AsyncSpawner->AsyncSpawnOptionAtLocation(
		TrainingOption, M_DemoLocation, this, 0,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->M_Tank = Cast<ATankMaster>(SpawnedActor);
			}
		},
		FRotator::ZeroRotator);
}

void UTurretTossDemoSubsystem::TickSettle()
{
	using namespace TurretTossDemoPrivate;
	KeepGameRunning();
	if (not M_Tank.IsValid())
	{
		Finish(TEXT("tank_lost_before_death"));
		return;
	}
	if (GetGameSeconds() - M_StepStartGameSeconds < SettleGameSeconds)
	{
		return;
	}
	if (not CreateCaptureRig())
	{
		Finish(TEXT("no_capture_rig"));
		return;
	}
	M_DeathLocation = M_Tank->GetActorLocation();
	M_DeathYaw = M_Tank->GetActorRotation().Yaw;
	M_Step = ETurretTossDemoStep::Record;
	M_StepStartGameSeconds = GetGameSeconds();
}

void UTurretTossDemoSubsystem::TickRecord()
{
	using namespace TurretTossDemoPrivate;
	KeepGameRunning();
	if (bM_IsCloseFilm)
	{
		AimCapture(FilmOrbitYawDegrees, CloseFilmDistanceCm, CloseFilmHeightCm, M_DeathLocation + CloseFilmLookAtOffset);
	}
	else
	{
		AimCapture(FilmOrbitYawDegrees, FilmDistanceCm, FilmHeightCm, M_DeathLocation + FilmLookAtOffset);
	}
	ExportFrame(FString::Printf(TEXT("frame_%04d.png"), M_FrameIndex));
	++M_FrameIndex;

	if (not bM_HasKilledTank && M_FrameIndex >= PreRollFrames)
	{
		bM_HasKilledTank = true;
		if (not M_Tank.IsValid())
		{
			Finish(TEXT("tank_lost_before_death"));
			return;
		}
		UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTD kill t=%.2f frame=%d"), GetGameSeconds(), M_FrameIndex);
		KillTank(*M_Tank);
		M_StepStartGameSeconds = GetGameSeconds();
		M_NextTurretLogGameSeconds = GetGameSeconds();
		return;
	}
	if (not bM_HasKilledTank)
	{
		return;
	}
	// The burning tank drives on; keep it in frame and leave the stills looking at where it ended up.
	if (M_DeathKind == ETurretTossDemoDeath::LeakDrive && M_Tank.IsValid())
	{
		M_DeathLocation = M_Tank->GetActorLocation();
	}
	if (GetGameSeconds() >= M_NextTurretLogGameSeconds)
	{
		M_NextTurretLogGameSeconds += TurretLogIntervalSeconds;
		LogTossedTurrets();
		LogEngineFireWrecks();
	}
	if (GetGameSeconds() - M_StepStartGameSeconds >= M_RecordAfterDeathGameSeconds)
	{
		// Keep the kill time as step start so the last turret log still reads as seconds after death.
		M_Step = ETurretTossDemoStep::Stills;
		M_FrameIndex = 0;
	}
}

void UTurretTossDemoSubsystem::TickStills()
{
	using namespace TurretTossDemoPrivate;
	if (M_StillIndex >= UE_ARRAY_COUNT(StillCameras))
	{
		LogTossedTurrets();
		LogEngineFireWrecks();
		Finish(TEXT("done"));
		return;
	}
	const FVector& StillCamera = StillCameras[M_StillIndex];
	AimCapture(StillCamera.X, StillCamera.Y, StillCamera.Z, M_DeathLocation + StillLookAtOffset);
	// The capture renders the frame after it moved; wait a few frames before exporting.
	if (++M_FrameIndex < FramesPerStill)
	{
		return;
	}
	ExportFrame(FString::Printf(TEXT("still_%02d.png"), M_StillIndex));
	M_FrameIndex = 0;
	++M_StillIndex;
}

bool UTurretTossDemoSubsystem::CreateCaptureRig()
{
	using namespace TurretTossDemoPrivate;
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
	// The fog of war is a post-process material; the film shows the wreck, not what the player may see.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	return true;
}

void UTurretTossDemoSubsystem::AimCapture(const float OrbitYawDegrees, const float DistanceCm, const float HeightCm,
                                          const FVector& LookAt) const
{
	ASceneCapture2D* Capture = M_Capture.Get();
	if (not IsValid(Capture))
	{
		return;
	}
	const FVector Horizontal = FRotator(0.f, M_DeathYaw + OrbitYawDegrees, 0.f).Vector() * DistanceCm;
	FVector CameraLocation = M_DeathLocation + Horizontal + FVector(0.f, 0.f, HeightCm);
	float GroundZ = 0.f;
	if (TryGetGroundZ(CameraLocation, GroundZ))
	{
		CameraLocation.Z = FMath::Max(CameraLocation.Z, GroundZ + HeightCm * 0.5f);
	}
	Capture->SetActorLocationAndRotation(CameraLocation, (LookAt - CameraLocation).Rotation());
}

void UTurretTossDemoSubsystem::ExportFrame(const FString& FileName) const
{
	if (M_OutputDirectory.IsEmpty() || not IsValid(M_RenderTarget))
	{
		return;
	}
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), M_RenderTarget, M_OutputDirectory, FileName);
}

void UTurretTossDemoSubsystem::LogTossedTurrets() const
{
	for (TObjectIterator<UWreckTurretTossComponent> It; It; ++It)
	{
		const UWreckTurretTossComponent* TossComponent = *It;
		if (not IsValid(TossComponent) || TossComponent->GetWorld() != GetWorld() || not IsValid(TossComponent->GetOwner()))
		{
			continue;
		}
		TArray<UStaticMeshComponent*> MeshComponents;
		TossComponent->GetOwner()->GetComponents<UStaticMeshComponent>(MeshComponents);
		// Non-const: the engine's GetPhysicsLinearVelocity is not const.
		for (UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			if (not IsValid(MeshComponent) || MeshComponent->GetFName() != TEXT("Turret"))
			{
				continue;
			}
			const FVector Location = MeshComponent->GetComponentLocation();
			UE_LOG(LogRTSTurretTossDemo, Display,
			       TEXT("RTS_TTD turret t=%.2f wreck=%s height_cm=%.0f dist2d_cm=%.0f simulating=%d speed=%.0f mobility=%d"),
			       GetGameSeconds() - M_StepStartGameSeconds, *TossComponent->GetOwner()->GetName(),
			       Location.Z - M_DeathLocation.Z, FVector::Dist2D(Location, M_DeathLocation),
			       MeshComponent->IsSimulatingPhysics() ? 1 : 0,
			       MeshComponent->IsSimulatingPhysics() ? MeshComponent->GetPhysicsLinearVelocity().Size() : 0.f,
			       static_cast<int32>(MeshComponent->Mobility.GetValue()));
		}
	}
}

void UTurretTossDemoSubsystem::LogEngineFireWrecks() const
{
	for (TObjectIterator<UWreckEngineFireDeathComponent> It; It; ++It)
	{
		const UWreckEngineFireDeathComponent* EngineFireComponent = *It;
		const AActor* Wreck = IsValid(EngineFireComponent) ? EngineFireComponent->GetOwner() : nullptr;
		if (not IsValid(Wreck) || EngineFireComponent->GetWorld() != GetWorld())
		{
			continue;
		}
		const UStaticMeshComponent* Hull = Wreck->FindComponentByClass<UStaticMeshComponent>();
		float MaxPieceHeight = 0.f;
		float MaxPieceDistance = 0.f;
		for (const UActorComponent* Component : Wreck->GetInstanceComponents())
		{
			const UStaticMeshComponent* Piece = Cast<UStaticMeshComponent>(Component);
			if (IsValid(Piece))
			{
				MaxPieceHeight = FMath::Max(MaxPieceHeight, Piece->GetComponentLocation().Z - M_DeathLocation.Z);
				MaxPieceDistance = FMath::Max(MaxPieceDistance,
				                              FVector::Dist2D(Piece->GetComponentLocation(), M_DeathLocation));
			}
		}
		UE_LOG(LogRTSTurretTossDemo, Display,
		       TEXT("RTS_TTD engine t=%.2f wreck=%s pieces=%d max_piece_height_cm=%.0f max_piece_dist_cm=%.0f overlay=%s"),
		       GetGameSeconds() - M_StepStartGameSeconds, *Wreck->GetName(), CountThrownDebris(Wreck), MaxPieceHeight,
		       MaxPieceDistance,
		       IsValid(Hull) && IsValid(Hull->GetOverlayMaterial()) ? *Hull->GetOverlayMaterial()->GetName() : TEXT("none"));
		// Non-const: the engine's GetPhysicsLinearVelocity is not const.
		UStaticMeshComponent* Deck = FindWreckEngineDeck(Wreck);
		if (IsValid(Deck))
		{
			UE_LOG(LogRTSTurretTossDemo, Display,
			       TEXT("RTS_TTD deck t=%.2f wreck=%s height_cm=%.0f dist2d_cm=%.0f simulating=%d speed=%.0f"),
			       GetGameSeconds() - M_StepStartGameSeconds, *Wreck->GetName(),
			       Deck->GetComponentLocation().Z - M_DeathLocation.Z,
			       FVector::Dist2D(Deck->GetComponentLocation(), M_DeathLocation), Deck->IsSimulatingPhysics() ? 1 : 0,
			       Deck->IsSimulatingPhysics() ? Deck->GetPhysicsLinearVelocity().Size() : 0.f);
		}
	}
}

void UTurretTossDemoSubsystem::KillTank(ATankMaster& Tank) const
{
	switch (M_DeathKind)
	{
	case ETurretTossDemoDeath::EngineFire:
		Tank.Debug_DieDuringEngineFire();
		return;
	case ETurretTossDemoDeath::LeakDrive:
		StartLeakDrive(Tank);
		return;
	case ETurretTossDemoDeath::Regular:
		KillTankWithRegularDeath(Tank);
		return;
	case ETurretTossDemoDeath::Both:
		Tank.SetIsEngineBurning(true);
		Tank.Debug_DieDuringAmmoCookOff();
		return;
	default:
		Tank.Debug_DieDuringAmmoCookOff();
	}
}

void UTurretTossDemoSubsystem::ParseRegularDeathSwitches()
{
	using namespace TurretTossDemoPrivate;
	FString VariantText;
	if (FParse::Value(FCommandLine::Get(), RegularVariantSwitch, VariantText))
	{
		const int64 VariantValue = StaticEnum<ERegularDeathVariant>()->GetValueByNameString(VariantText);
		M_RegularDeathVariant = VariantValue == INDEX_NONE
			                        ? ERegularDeathVariant::Standard
			                        : static_cast<ERegularDeathVariant>(VariantValue);
	}
	float KillShotYaw = 0.f;
	if (FParse::Value(FCommandLine::Get(), KillShotYawSwitch, KillShotYaw))
	{
		M_KillShotYaw = KillShotYaw;
	}
	// The film camera is a scene capture, not the player's view; without this every death counts as unseen.
	if (M_DeathKind == ETurretTossDemoDeath::Regular)
	{
		GetMutableDefault<URTSRagdollSettings>()->bSkipUnseenVehicles = false;
	}
}

void UTurretTossDemoSubsystem::KillTankWithRegularDeath(ATankMaster& Tank) const
{
	const FVector KillShotDirection = M_KillShotYaw.IsSet()
		                                  ? Tank.GetActorForwardVector().RotateAngleAxis(
			                                  M_KillShotYaw.GetValue(), FVector::UpVector)
		                                  : FVector::ZeroVector;
	UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTD regular_death variant=%s kill_shot=%s"),
	       *UEnum::GetValueAsString(M_RegularDeathVariant), *KillShotDirection.ToCompactString());
	Tank.Debug_DieRegularDeath(M_RegularDeathVariant, KillShotDirection);
}

void UTurretTossDemoSubsystem::StartLeakDrive(ATankMaster& Tank) const
{
	constexpr float LeakDriveDistanceCm = 3000.f;
	Tank.Debug_StartEngineFire();
	const FVector Goal = Tank.GetActorLocation() + Tank.GetActorForwardVector() * LeakDriveDistanceCm;
	const ECommandQueueError MoveError = Tank.MoveToLocation(Goal, true, Tank.GetActorRotation());
	UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTD leak_drive move_error=%d"), static_cast<int32>(MoveError));
}

void UTurretTossDemoSubsystem::ReportEngineFireAuditEntry(const FTurretTossAuditEntry& Entry, AActor* Wreck)
{
	const UWreckEngineFireDeathComponent* EngineFireComponent =
		IsValid(Wreck) ? Wreck->FindComponentByClass<UWreckEngineFireDeathComponent>() : nullptr;
	const FString WreckName = IsValid(Wreck) ? Wreck->GetClass()->GetName() : TEXT("none");
	if (EngineFireComponent == nullptr)
	{
		++M_AuditCountsFailed;
		UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTA vehicle=%s result=no_fuel_death wreck=%s"),
		       *Entry.SubtypeName, *WreckName);
		return;
	}
	const int32 PieceCount = CountThrownDebris(Wreck);
	const UStaticMeshComponent* Deck = FindWreckEngineDeck(Wreck);
	if (IsValid(Deck))
	{
		// Hero wrecks throw their own deck: it must have flown and come to rest as a static part again.
		const bool bDeckThrown = Entry.MaxDeckHeightCm >= TurretTossDemoPrivate::AuditMinDeckThrowHeightCm
			&& not Deck->IsSimulatingPhysics();
		bDeckThrown ? ++M_AuditCountsTossed : ++M_AuditCountsFailed;
		UE_LOG(LogRTSTurretTossDemo, Display,
		       TEXT("RTS_TTA vehicle=%s result=%s wreck=%s pieces=%d deck_max_height_cm=%.0f deck_landed_dist_cm=%.0f"),
		       *Entry.SubtypeName, bDeckThrown ? TEXT("fuel_death_deck") : TEXT("deck_not_thrown"), *WreckName,
		       PieceCount, Entry.MaxDeckHeightCm, FVector::Dist2D(Deck->GetComponentLocation(), Entry.Location));
		return;
	}
	PieceCount > 0 ? ++M_AuditCountsTossed : ++M_AuditCountsFailed;
	UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTA vehicle=%s result=%s wreck=%s pieces=%d"),
	       *Entry.SubtypeName, PieceCount > 0 ? TEXT("fuel_death") : TEXT("no_debris"), *WreckName, PieceCount);
}

int32 UTurretTossDemoSubsystem::CountThrownDebris(const AActor* Wreck)
{
	if (not IsValid(Wreck))
	{
		return 0;
	}
	int32 PieceCount = 0;
	for (const UActorComponent* Component : Wreck->GetInstanceComponents())
	{
		PieceCount += Cast<UStaticMeshComponent>(Component) != nullptr ? 1 : 0;
	}
	return PieceCount;
}

// ---------------------------------------------------------------------------------------------------------------
// Audit of every tank subtype

void UTurretTossDemoSubsystem::BuildAuditEntries()
{
	const UEnum* TankSubtypeEnum = StaticEnum<ETankSubtype>();
	// The last enum entry is the generated _MAX.
	for (int32 EnumIndex = 0; EnumIndex < TankSubtypeEnum->NumEnums() - 1; ++EnumIndex)
	{
		const FString SubtypeName = TankSubtypeEnum->GetNameStringByIndex(EnumIndex);
		if (SubtypeName == TEXT("Tank_None"))
		{
			continue;
		}
		FTurretTossAuditEntry Entry;
		Entry.SubtypeName = SubtypeName;
		Entry.SubtypeValue = static_cast<uint8>(TankSubtypeEnum->GetValueByIndex(EnumIndex));
		M_AuditEntries.Add(Entry);
	}
	UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTA audit vehicles=%d"), M_AuditEntries.Num());
	M_AuditBatchStart = 0;
	M_AuditBatchEnd = 0;
}

void UTurretTossDemoSubsystem::TickAuditSpawnBatch()
{
	using namespace TurretTossDemoPrivate;
	KeepGameRunning();
	if (M_AuditBatchEnd == M_AuditBatchStart)
	{
		if (M_AuditBatchStart >= M_AuditEntries.Num())
		{
			UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTA SUMMARY tossed=%d hull_only=%d failed=%d total=%d"),
			       M_AuditCountsTossed, M_AuditCountsHullOnly, M_AuditCountsFailed, M_AuditEntries.Num());
			Finish(TEXT("audit_done"));
			return;
		}
		StartAuditBatch();
		return;
	}

	bool bAllSpawned = true;
	for (int32 EntryIndex = M_AuditBatchStart; EntryIndex < M_AuditBatchEnd; ++EntryIndex)
	{
		const FTurretTossAuditEntry& Entry = M_AuditEntries[EntryIndex];
		bAllSpawned &= not Entry.bSpawnRequested || Entry.Tank.IsValid();
	}
	if (bAllSpawned || GetGameSeconds() - M_StepStartGameSeconds > AuditSpawnTimeoutGameSeconds)
	{
		M_Step = ETurretTossDemoStep::AuditSettle;
		M_StepStartGameSeconds = GetGameSeconds();
	}
}

void UTurretTossDemoSubsystem::StartAuditBatch()
{
	using namespace TurretTossDemoPrivate;
	M_AuditBatchEnd = FMath::Min(M_AuditBatchStart + AuditBatchSize, M_AuditEntries.Num());
	M_StepStartGameSeconds = GetGameSeconds();
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
	if (not IsValid(AsyncSpawner))
	{
		return;
	}
	TWeakObjectPtr<UTurretTossDemoSubsystem> WeakThis(this);
	for (int32 EntryIndex = M_AuditBatchStart; EntryIndex < M_AuditBatchEnd; ++EntryIndex)
	{
		FTurretTossAuditEntry& Entry = M_AuditEntries[EntryIndex];
		const int32 Slot = EntryIndex - M_AuditBatchStart;
		Entry.Location = M_DemoLocation + FVector((Slot % AuditBatchColumns) * AuditSpacingCm,
		                                          (Slot / AuditBatchColumns) * AuditSpacingCm, 0.f);
		float GroundZ = 0.f;
		if (not TryGetGroundZ(Entry.Location, GroundZ))
		{
			continue;
		}
		Entry.Location.Z = GroundZ;
		Entry.bSpawnRequested = AsyncSpawner->AsyncSpawnOptionAtLocation(
			FTrainingOption(EAllUnitType::UNType_Tank, Entry.SubtypeValue), Entry.Location, this, EntryIndex,
			[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32 SpawnedIndex)
			{
				if (WeakThis.IsValid() && WeakThis->M_AuditEntries.IsValidIndex(SpawnedIndex))
				{
					WeakThis->M_AuditEntries[SpawnedIndex].Tank = Cast<ATankMaster>(SpawnedActor);
				}
			},
			FRotator::ZeroRotator);
	}
}

void UTurretTossDemoSubsystem::TickAuditSettle()
{
	using namespace TurretTossDemoPrivate;
	KeepGameRunning();
	if (GetGameSeconds() - M_StepStartGameSeconds < SettleGameSeconds)
	{
		return;
	}
	for (int32 EntryIndex = M_AuditBatchStart; EntryIndex < M_AuditBatchEnd; ++EntryIndex)
	{
		FTurretTossAuditEntry& Entry = M_AuditEntries[EntryIndex];
		if (not Entry.Tank.IsValid())
		{
			continue;
		}
		// The tank may have rolled a little while settling; judge the wreck where it actually died.
		Entry.Location = Entry.Tank->GetActorLocation();
		KillTank(*Entry.Tank);
		Entry.bKilled = true;
	}
	M_Step = ETurretTossDemoStep::AuditObserve;
	M_StepStartGameSeconds = GetGameSeconds();
	M_NextTurretLogGameSeconds = GetGameSeconds();
}

void UTurretTossDemoSubsystem::TickAuditObserve()
{
	using namespace TurretTossDemoPrivate;
	KeepGameRunning();
	if (GetGameSeconds() >= M_NextTurretLogGameSeconds)
	{
		M_NextTurretLogGameSeconds += TurretLogIntervalSeconds;
		SampleAuditTurretHeights();
	}
	if (GetGameSeconds() - M_StepStartGameSeconds < AuditObserveGameSeconds)
	{
		return;
	}
	for (int32 EntryIndex = M_AuditBatchStart; EntryIndex < M_AuditBatchEnd; ++EntryIndex)
	{
		ReportAuditEntry(M_AuditEntries[EntryIndex]);
	}
	ClearAuditBatch();
	M_AuditBatchStart = M_AuditBatchEnd;
	M_Step = ETurretTossDemoStep::AuditSpawnBatch;
}

void UTurretTossDemoSubsystem::SampleAuditTurretHeights()
{
	for (int32 EntryIndex = M_AuditBatchStart; EntryIndex < M_AuditBatchEnd; ++EntryIndex)
	{
		FTurretTossAuditEntry& Entry = M_AuditEntries[EntryIndex];
		if (not Entry.bKilled)
		{
			continue;
		}
		if (not Entry.Wreck.IsValid())
		{
			Entry.Wreck = FindWreckNear(Entry.Location);
		}
		const UStaticMeshComponent* TurretMesh = FindWreckTurret(Entry.Wreck.Get());
		if (IsValid(TurretMesh))
		{
			Entry.MaxTurretHeightCm = FMath::Max(Entry.MaxTurretHeightCm,
			                                     TurretMesh->GetComponentLocation().Z - Entry.Location.Z);
		}
		const UStaticMeshComponent* DeckMesh = FindWreckEngineDeck(Entry.Wreck.Get());
		if (IsValid(DeckMesh))
		{
			Entry.MaxDeckHeightCm = FMath::Max(Entry.MaxDeckHeightCm,
			                                   DeckMesh->GetComponentLocation().Z - Entry.Location.Z);
		}
	}
}

void UTurretTossDemoSubsystem::ReportAuditEntry(const FTurretTossAuditEntry& Entry)
{
	using namespace TurretTossDemoPrivate;
	if (not Entry.bKilled)
	{
		++M_AuditCountsFailed;
		UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTA vehicle=%s result=not_spawned requested=%d"),
		       *Entry.SubtypeName, Entry.bSpawnRequested ? 1 : 0);
		return;
	}
	AActor* Wreck = Entry.Wreck.IsValid() ? Entry.Wreck.Get() : FindWreckNear(Entry.Location);
	if (M_DeathKind == ETurretTossDemoDeath::EngineFire)
	{
		ReportEngineFireAuditEntry(Entry, Wreck);
		return;
	}
	const UWreckTurretTossComponent* TossComponent = IsValid(Wreck)
		                                                 ? Wreck->FindComponentByClass<UWreckTurretTossComponent>()
		                                                 : nullptr;
	const FString WreckName = IsValid(Wreck) ? Wreck->GetClass()->GetName() : TEXT("none");
	if (TossComponent == nullptr)
	{
		++M_AuditCountsFailed;
		UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTA vehicle=%s result=no_toss wreck=%s"), *Entry.SubtypeName,
		       *WreckName);
		return;
	}
	const UStaticMeshComponent* TurretMesh = FindWreckTurret(Wreck);
	if (not IsValid(TurretMesh))
	{
		++M_AuditCountsHullOnly;
		UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTA vehicle=%s result=hull_only wreck=%s"), *Entry.SubtypeName,
		       *WreckName);
		return;
	}

	const bool bWasThrown = Entry.MaxTurretHeightCm >= AuditMinThrowHeightCm;
	const bool bStillSimulating = TurretMesh->IsSimulatingPhysics();
	const bool bLandedAndStatic = bWasThrown && not bStillSimulating;
	bLandedAndStatic ? ++M_AuditCountsTossed : ++M_AuditCountsFailed;
	const TCHAR* Result = bWasThrown ? (bStillSimulating ? TEXT("still_flying") : TEXT("tossed")) : TEXT("not_thrown");
	UE_LOG(LogRTSTurretTossDemo, Display,
	       TEXT("RTS_TTA vehicle=%s result=%s wreck=%s max_height_cm=%.0f landed_dist_cm=%.0f landed_height_cm=%.0f"),
	       *Entry.SubtypeName, Result, *WreckName, Entry.MaxTurretHeightCm,
	       FVector::Dist2D(TurretMesh->GetComponentLocation(), Entry.Location),
	       TurretMesh->GetComponentLocation().Z - Entry.Location.Z);
}

void UTurretTossDemoSubsystem::ClearAuditBatch()
{
	for (int32 EntryIndex = M_AuditBatchStart; EntryIndex < M_AuditBatchEnd; ++EntryIndex)
	{
		FTurretTossAuditEntry& Entry = M_AuditEntries[EntryIndex];
		if (Entry.Wreck.IsValid())
		{
			Entry.Wreck->Destroy();
		}
		if (Entry.Tank.IsValid())
		{
			Entry.Tank->Destroy();
		}
	}
}

AActor* UTurretTossDemoSubsystem::FindWreckNear(const FVector& Location) const
{
	using namespace TurretTossDemoPrivate;
	AActor* BestWreck = nullptr;
	float BestDistance = AuditWreckSearchRadiusCm;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		// Wreck Blueprints are named Env_Dest_* / *Destroyed*.
		if (not IsValid(Candidate) || not Candidate->GetClass()->GetName().Contains(TEXT("Dest")))
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Candidate->GetActorLocation(), Location);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestWreck = Candidate;
		}
	}
	return BestWreck;
}

const UStaticMeshComponent* UTurretTossDemoSubsystem::FindWreckTurret(const AActor* Wreck)
{
	if (not IsValid(Wreck))
	{
		return nullptr;
	}
	TArray<UStaticMeshComponent*> MeshComponents;
	Wreck->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (const UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		// Same rule as UWreckTurretTossComponent: an empty Turret slot is no turret.
		if (IsValid(MeshComponent) && MeshComponent->GetFName() == TEXT("Turret")
			&& IsValid(MeshComponent->GetStaticMesh()))
		{
			return MeshComponent;
		}
	}
	return nullptr;
}

UStaticMeshComponent* UTurretTossDemoSubsystem::FindWreckEngineDeck(const AActor* Wreck)
{
	if (not IsValid(Wreck))
	{
		return nullptr;
	}
	TArray<UStaticMeshComponent*> MeshComponents;
	Wreck->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent) && MeshComponent->GetFName() == TurretTossDemoPrivate::EngineDeckComponentName
			&& IsValid(MeshComponent->GetStaticMesh()))
		{
			return MeshComponent;
		}
	}
	return nullptr;
}

bool UTurretTossDemoSubsystem::TryGetGroundZ(const FVector& Location, float& OutGroundZ) const
{
	using namespace TurretTossDemoPrivate;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TurretTossDemoGround), false);
	if (M_Tank.IsValid())
	{
		QueryParams.AddIgnoredActor(M_Tank.Get());
	}
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

float UTurretTossDemoSubsystem::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UTurretTossDemoSubsystem::Finish(const TCHAR* Reason)
{
	M_Step = ETurretTossDemoStep::Done;
	UE_LOG(LogRTSTurretTossDemo, Display, TEXT("RTS_TTD RESULT reason=%s game_t=%.1f real_s=%.0f"), Reason,
	       GetGameSeconds(), FPlatformTime::Seconds() - M_StartRealSeconds);
	FPlatformMisc::RequestExit(false, TEXT("TurretTossDemo"));
}

#else

bool UTurretTossDemoSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UTurretTossDemoSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UTurretTossDemoSubsystem::Tick(float DeltaTime)
{
}

TStatId UTurretTossDemoSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTurretTossDemoSubsystem, STATGROUP_Tickables);
}

#endif
