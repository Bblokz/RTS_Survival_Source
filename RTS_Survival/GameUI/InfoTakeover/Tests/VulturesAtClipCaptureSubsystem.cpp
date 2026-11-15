// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VulturesAtClipCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/EnemyUnitAbilityAI/EnemyUnitAbilityAISettings.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSVulturesAtClip, Log, All);

namespace VulturesAtClipPrivate
{
	const TCHAR* const CaptureSwitch = TEXT("VulturesAtClipCapture");
	const TCHAR* const SpotSwitch = TEXT("VulturesAtClipAt=");
	const TCHAR* const DirectorySwitch = TEXT("VulturesAtClipDir=");
	const TCHAR* const VulturesSquad = TEXT(
		"/Game/RTS_Survival/Blueprints/Squads/Ger/ArmoryUnlocked/Squad_Ger_HazmatVultures.Squad_Ger_HazmatVultures_C");
	const TCHAR* const T26Tank = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Light/T26/Rus_Vehicle_T26.Rus_Vehicle_T26_C");
	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;
	constexpr int32 MinLivingSoldiers = 4;

	// The squad stands on the spot facing +X; the tank drives in along +Y from the left of the picture and stops in
	// front of the squad's right, so the throw, the stuck grenades and the wreck all stay in frame.
	const FRotator SquadRotation(0.f, 0.f, 0.f);
	const FRotator TankRotation(0.f, 90.f, 0.f);
	const FVector TankStartOffset(700.f, -1500.f, 0.f);
	const FVector TankEndOffset(700.f, 450.f, 0.f);
	// The throw is ordered as soon as the tank is in range; the stick grenade reaches 800.
	constexpr float ThrowDistance = 790.f;

	// Three-quarter view from behind the squad's left, so soldiers, grenades and the tank all read at 480 px.
	const FVector CameraLookAtOffset(420.f, 120.f, 60.f);
	const FRotator CameraRotation(-34.f, 20.f, 0.f);
	constexpr float CameraDistance = 1750.f;
	constexpr float CameraFieldOfView = 55.f;

	// Seconds the units stand before the tank starts: Ger_Trains keeps rebuilding navmesh tiles (trains, crop
	// modifiers) for a while after loading, and a tank ordered off before that crawls in short hops.
	constexpr double UnitSettleSeconds = 20.0;
	const TCHAR* const SettleSwitch = TEXT("VulturesAtClipSettle=");
	const FVector SpotNavExtent(3000.f, 3000.f, 2000.f);
	constexpr double MaxUnitWaitSeconds = 60.0;
	constexpr double MaxApproachSeconds = 20.0;
	// Frames before the throw order are kept too: the clip opens on the tank rolling in.
	constexpr double SecondsAfterTankDeath = 2.5;
	constexpr double MaxRecordingSeconds = 14.0;
	// One position line per game second at -fps=24, to tell a blocked tank from a slow one.
	constexpr int32 ProgressLogEveryFrames = 24;
}

bool UVulturesAtClipCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), VulturesAtClipPrivate::CaptureSwitch);
}

void UVulturesAtClipCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The enemy AI would otherwise use the T-26's abilities; the clip only shows the Vultures' grenades.
	GetMutableDefault<UEnemyUnitAbilityAISettings>()->bEnabled = false;
}

void UVulturesAtClipCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("InfoTakeover/VulturesAt/Frames");
	FParse::Value(FCommandLine::Get(), VulturesAtClipPrivate::DirectorySwitch, M_OutputDirectory);
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EVulturesAtClipPhase::WaitingForGameStart);
	UE_LOG(LogRTSVulturesAtClip, Display, TEXT("VULTURES_AT_CLIP started, frames to %s"), *M_OutputDirectory);
}

void UVulturesAtClipCaptureSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UVulturesAtClipCaptureSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EVulturesAtClipPhase::Inactive || M_Phase == EVulturesAtClipPhase::Done || not IsValid(GetWorld()))
	{
		return;
	}
	if (M_Phase != EVulturesAtClipPhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
		DisableTankTurrets();
	}
	switch (M_Phase)
	{
	case EVulturesAtClipPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EVulturesAtClipPhase::WaitingForUnits:
		TickWaitingForUnits();
		break;
	case EVulturesAtClipPhase::WaitingForTankApproach:
		TickWaitingForTankApproach();
		break;
	case EVulturesAtClipPhase::Recording:
		TickRecording();
		break;
	default:
		break;
	}
}

TStatId UVulturesAtClipCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVulturesAtClipCaptureSubsystem, STATGROUP_Tickables);
}

void UVulturesAtClipCaptureSubsystem::EnterPhase(const EVulturesAtClipPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartGameSeconds = GetGameSeconds();
}

double UVulturesAtClipCaptureSubsystem::GetGameSeconds() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

double UVulturesAtClipCaptureSubsystem::GetSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

void UVulturesAtClipCaptureSubsystem::TickWaitingForGameStart()
{
	UWorld& World = *GetWorld();
	if (not FGrenadeTestHelpers::TryStartGame(World, M_PlayerController))
	{
		return;
	}
	if (not FGrenadeTestHelpers::FindTestSpot(World, VulturesAtClipPrivate::SpotSwitch, M_Spot))
	{
		Fail(TEXT("no spot: pass -VulturesAtClipAt=X,Y or use a map with a player start"));
		return;
	}
	if (not SpawnUnits() || not CreateCamera())
	{
		Fail(TEXT("could not spawn the Vultures, the T-26 or the capture camera"));
		return;
	}
	EnterPhase(EVulturesAtClipPhase::WaitingForUnits);
}

void UVulturesAtClipCaptureSubsystem::TickWaitingForUnits()
{
	using namespace VulturesAtClipPrivate;
	if (not GetAreUnitsReady())
	{
		if (GetSecondsInPhase() > MaxUnitWaitSeconds)
		{
			Fail(TEXT("the Vultures or the T-26 never finished loading"));
		}
		return;
	}
	double SettleSeconds = UnitSettleSeconds;
	FParse::Value(FCommandLine::Get(), SettleSwitch, SettleSeconds);
	if (GetSecondsInPhase() < SettleSeconds)
	{
		return;
	}
	ICommands* const TankCommands = Cast<ICommands>(M_Tank.Get());
	const ECommandQueueError MoveResult = TankCommands != nullptr
		                                      ? TankCommands->MoveToLocation(M_Spot + TankEndOffset, true, TankRotation)
		                                      : ECommandQueueError::AbilityNotAllowed;
	if (MoveResult != ECommandQueueError::NoError)
	{
		Fail(TEXT("the T-26 refused the move order"));
		return;
	}
	UE_LOG(LogRTSVulturesAtClip, Display, TEXT("VULTURES_AT_CLIP the T-26 drives off at %.2f s"), GetGameSeconds());
	EnterPhase(EVulturesAtClipPhase::WaitingForTankApproach);
}

void UVulturesAtClipCaptureSubsystem::TickWaitingForTankApproach()
{
	using namespace VulturesAtClipPrivate;
	SaveFrame();
	if (not M_Tank.IsValid() || not M_VulturesSquad.IsValid())
	{
		Fail(TEXT("a unit vanished before the throw"));
		return;
	}
	const float TankDistance = FVector::Dist2D(M_Tank->GetActorLocation(), M_VulturesSquad->GetActorLocation());
	if (M_FramesSaved % ProgressLogEveryFrames == 0)
	{
		UE_LOG(LogRTSVulturesAtClip, Display, TEXT("VULTURES_AT_CLIP tank at %s (offset %s), speed %.0f, distance %.0f"),
		       *M_Tank->GetActorLocation().ToCompactString(),
		       *(M_Tank->GetActorLocation() - M_Spot).ToCompactString(), M_Tank->GetVelocity().Size2D(),
		       TankDistance);
	}
	if (TankDistance <= ThrowDistance)
	{
		OrderThrow();
		return;
	}
	if (GetSecondsInPhase() > MaxApproachSeconds)
	{
		Fail(FString::Printf(TEXT("the T-26 never came within %.0f (last %.0f)"), ThrowDistance, TankDistance));
	}
}

void UVulturesAtClipCaptureSubsystem::TickRecording()
{
	using namespace VulturesAtClipPrivate;
	SaveFrame();
	if (M_TankDeathGameSeconds < 0.0 && not GetIsTankAlive())
	{
		M_TankDeathGameSeconds = GetGameSeconds();
		UE_LOG(LogRTSVulturesAtClip, Display, TEXT("VULTURES_AT_CLIP the T-26 died %.2f s after the throw order"),
		       GetSecondsInPhase());
	}
	const bool bHasShownDeath = M_TankDeathGameSeconds >= 0.0 &&
		GetGameSeconds() - M_TankDeathGameSeconds >= SecondsAfterTankDeath;
	if (bHasShownDeath)
	{
		Finish(true);
		return;
	}
	if (GetSecondsInPhase() > MaxRecordingSeconds)
	{
		UE_LOG(LogRTSVulturesAtClip, Warning, TEXT("VULTURES_AT_CLIP the T-26 survived the volley"));
		Finish(true);
	}
}

bool UVulturesAtClipCaptureSubsystem::SpawnUnits()
{
	using namespace VulturesAtClipPrivate;
	UWorld& World = *GetWorld();
	// Map anchors can sit at camera height over empty space; the bench's wide search finds the walkable ground.
	if (not FGrenadeTestHelpers::ProjectOntoNavMesh(World, M_Spot, SpotNavExtent))
	{
		UE_LOG(LogRTSVulturesAtClip, Warning, TEXT("VULTURES_AT_CLIP no navmesh near the spot %s"),
		       *M_Spot.ToCompactString());
	}
	M_Tank = FGrenadeTestHelpers::SpawnUnitForPlayer(World, T26Tank, FTransform(TankRotation, M_Spot + TankStartOffset),
	                                                 EnemyOwner);
	M_VulturesSquad = Cast<ASquadController>(FGrenadeTestHelpers::SpawnUnitForPlayer(
		World, VulturesSquad, FTransform(SquadRotation, M_Spot), PlayerOwner));
	UE_LOG(LogRTSVulturesAtClip, Display, TEXT("VULTURES_AT_CLIP spot %s: squad=%s tank=%s"),
	       *M_Spot.ToCompactString(), M_VulturesSquad.IsValid() ? *M_VulturesSquad->GetName() : TEXT("none"),
	       M_Tank.IsValid() ? *M_Tank->GetName() : TEXT("none"));
	return M_VulturesSquad.IsValid() && M_Tank.IsValid();
}

bool UVulturesAtClipCaptureSubsystem::CreateCamera()
{
	using namespace VulturesAtClipPrivate;
	ASceneCapture2D* Capture = nullptr;
	UTextureRenderTarget2D* RenderTarget = nullptr;
	if (not FGrenadeTestHelpers::CreateCaptureCamera(*GetWorld(), CameraFieldOfView, Capture, RenderTarget))
	{
		return false;
	}
	const FVector LookAt = M_Spot + CameraLookAtOffset;
	Capture->SetActorLocationAndRotation(LookAt - CameraRotation.Vector() * CameraDistance, CameraRotation);
	M_Capture = Capture;
	M_RenderTarget = RenderTarget;
	return true;
}

bool UVulturesAtClipCaptureSubsystem::GetAreUnitsReady() const
{
	if (not M_VulturesSquad.IsValid() || not GetIsTankAlive())
	{
		return false;
	}
	return M_VulturesSquad->GetIsSquadFullyLoadedAndInitialized() &&
		FGrenadeTestHelpers::GetLivingSoldierCount(*M_VulturesSquad) >= VulturesAtClipPrivate::MinLivingSoldiers;
}

bool UVulturesAtClipCaptureSubsystem::GetIsTankAlive() const
{
	// IsUnitAlive is not const on AActor.
	AActor* const Tank = M_Tank.Get();
	return IsValid(Tank) && Tank->IsUnitAlive();
}

void UVulturesAtClipCaptureSubsystem::DisableTankTurrets() const
{
	// The T-26 would shell the Vultures; the clip is about the grenades, not who wins the duel.
	const ATankMaster* const Tank = Cast<ATankMaster>(M_Tank.Get());
	if (not IsValid(Tank))
	{
		return;
	}
	for (ACPPTurretsMaster* Turret : Tank->GetTurrets())
	{
		if (IsValid(Turret))
		{
			Turret->DisableTurret();
		}
	}
}

void UVulturesAtClipCaptureSubsystem::OrderThrow()
{
	const FVector AimPoint = M_Tank->GetActorLocation();
	const ECommandQueueError OrderResult = M_VulturesSquad->ThrowGrenade(
		AimPoint, true, EGrenadeAbilityType::StickGrenadeVultures);
	if (OrderResult != ECommandQueueError::NoError)
	{
		Fail(TEXT("the Vultures refused the AT stick grenade order"));
		return;
	}
	UE_LOG(LogRTSVulturesAtClip, Display, TEXT("VULTURES_AT_CLIP throw ordered at frame %d (%.2f s)"), M_FramesSaved,
	       GetGameSeconds());
	EnterPhase(EVulturesAtClipPhase::Recording);
}

void UVulturesAtClipCaptureSubsystem::SaveFrame()
{
	if (not IsValid(M_RenderTarget))
	{
		return;
	}
	FGrenadeTestHelpers::SaveCapture(*GetWorld(), *M_RenderTarget, M_OutputDirectory,
	                                 FString::Printf(TEXT("Frame_%04d.png"), M_FramesSaved));
	++M_FramesSaved;
}

void UVulturesAtClipCaptureSubsystem::Fail(const FString& Reason)
{
	UE_LOG(LogRTSVulturesAtClip, Error, TEXT("VULTURES_AT_CLIP FAIL: %s"), *Reason);
	Finish(false);
}

void UVulturesAtClipCaptureSubsystem::Finish(const bool bSucceeded)
{
	EnterPhase(EVulturesAtClipPhase::Done);
	UE_LOG(LogRTSVulturesAtClip, Display, TEXT("VULTURES_AT_CLIP done: %d frames -> %s"), M_FramesSaved,
	       bSucceeded ? TEXT("OK") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("VulturesAtClipCapture"));
}

#else

bool UVulturesAtClipCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UVulturesAtClipCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UVulturesAtClipCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UVulturesAtClipCaptureSubsystem::Deinitialize() { Super::Deinitialize(); }
void UVulturesAtClipCaptureSubsystem::Tick(float DeltaTime) {}

TStatId UVulturesAtClipCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVulturesAtClipCaptureSubsystem, STATGROUP_Tickables);
}

#endif
