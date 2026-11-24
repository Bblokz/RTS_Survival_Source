// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DigInClipCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/EnemyUnitAbilityAI/EnemyUnitAbilityAISettings.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInWall/DigInWall.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSDigInClip, Log, All);

namespace DigInClipPrivate
{
	const TCHAR* const CaptureSwitch = TEXT("DigInClipCapture");
	const TCHAR* const SpotSwitch = TEXT("AbilityClipAt=");
	const TCHAR* const TankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H."
		"Ger_Vehicle_PanzerIV_H_C");
	const TCHAR* const T26Tank = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Light/T26/Rus_Vehicle_T26.Rus_Vehicle_T26_C");
	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;

	// The Panzer IV faces +X with two T-26s parked ahead, facing it head on. One T-26 died 1.4 s after the attack
	// order after a single hit on the sandbags; two keep the Panzer IV busy long enough to show several hits.
	const FRotator TankRotation(0.f, 0.f, 0.f);
	const TArray<FVector> TargetOffsets = {FVector(1500.f, -300.f, 0.f), FVector(1600.f, 300.f, 0.f)};
	const FRotator TargetRotation(0.f, 180.f, 0.f);

	// Close camera: front three-quarter view from the tank's left, low enough to see the sandbags rise.
	const FVector CloseLookAtOffset(200.f, 0.f, 60.f);
	const FRotator CloseCameraRotation(-25.f, -50.f, 0.f);
	constexpr float CloseCameraDistance = 1300.f;
	// Wide camera: side view across the firing line, the Panzer IV on the left and the T-26 on the right.
	// From 2100 away the Panzer IV was cut off at the left edge.
	const FVector WideLookAtOffset(800.f, 0.f, 60.f);
	const FRotator WideCameraRotation(-30.f, -85.f, 0.f);
	constexpr float WideCameraDistance = 2700.f;
	constexpr float CameraFieldOfView = 55.f;

	constexpr double UnitSettleSeconds = 4.0;
	const FVector SpotNavExtent(3000.f, 3000.f, 2000.f);
	constexpr double MaxUnitWaitSeconds = 60.0;
	// A medium tank's wall takes 12 s; the wall rises once a second, so 1.5 s without growth means it is done.
	constexpr double MaxDigInSeconds = 25.0;
	constexpr double WallDoneAfterStableSeconds = 1.5;
	constexpr double MaxUnderFireSeconds = 16.0;
	constexpr double SecondsAfterTargetDeath = 2.5;
}

bool UDigInClipCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), DigInClipPrivate::CaptureSwitch);
}

void UDigInClipCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The enemy AI would otherwise use the T-26s' abilities; the clip only shows the wall.
	GetMutableDefault<UEnemyUnitAbilityAISettings>()->bEnabled = false;
}

void UDigInClipCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EDigInClipPhase::WaitingForGameStart);
	UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP started"));
}

void UDigInClipCaptureSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UDigInClipCaptureSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EDigInClipPhase::Inactive || M_Phase == EDigInClipPhase::Done || not IsValid(GetWorld()))
	{
		return;
	}
	if (M_Phase != EDigInClipPhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
	}
	if (M_Phase == EDigInClipPhase::WaitingForUnits || M_Phase == EDigInClipPhase::DiggingIn)
	{
		// Nobody fires before the wall stands; the dig-in itself keeps the Panzer IV's guns off while it builds.
		for (const TWeakObjectPtr<AActor>& Target : M_Targets)
		{
			DisableTurretsOf(Target.Get());
		}
	}
	if (M_Phase == EDigInClipPhase::WaitingForUnits)
	{
		DisableTurretsOf(M_Tank.Get());
	}
	switch (M_Phase)
	{
	case EDigInClipPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EDigInClipPhase::WaitingForUnits:
		TickWaitingForUnits();
		break;
	case EDigInClipPhase::DiggingIn:
		TickDiggingIn();
		break;
	case EDigInClipPhase::UnderFire:
		TickUnderFire();
		break;
	default:
		break;
	}
}

TStatId UDigInClipCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDigInClipCaptureSubsystem, STATGROUP_Tickables);
}

void UDigInClipCaptureSubsystem::EnterPhase(const EDigInClipPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartGameSeconds = GetGameSeconds();
}

double UDigInClipCaptureSubsystem::GetGameSeconds() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

double UDigInClipCaptureSubsystem::GetSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

void UDigInClipCaptureSubsystem::TickWaitingForGameStart()
{
	UWorld& World = *GetWorld();
	if (not FGrenadeTestHelpers::TryStartGame(World, M_PlayerController))
	{
		return;
	}
	if (not FGrenadeTestHelpers::FindTestSpot(World, DigInClipPrivate::SpotSwitch, M_Spot))
	{
		Fail(TEXT("no spot: pass -AbilityClipAt=X,Y or use a map with a player start"));
		return;
	}
	if (not SpawnUnits() || not CreateCameras())
	{
		Fail(TEXT("could not spawn the Panzer IV, the T-26 or the capture cameras"));
		return;
	}
	EnterPhase(EDigInClipPhase::WaitingForUnits);
}

void UDigInClipCaptureSubsystem::TickWaitingForUnits()
{
	using namespace DigInClipPrivate;
	if (not GetIsActorAlive(M_Tank) || CountLivingTargets() < M_Targets.Num())
	{
		if (GetSecondsInPhase() > MaxUnitWaitSeconds)
		{
			Fail(TEXT("the Panzer IV or a T-26 never finished loading"));
		}
		return;
	}
	if (GetSecondsInPhase() < UnitSettleSeconds)
	{
		return;
	}
	OrderDigIn();
}

void UDigInClipCaptureSubsystem::TickDiggingIn()
{
	using namespace DigInClipPrivate;
	SaveFrames();
	const UHealthComponent* const WallHealth = GetWallHealth();
	if (not IsValid(WallHealth))
	{
		if (GetSecondsInPhase() > 1.0)
		{
			Fail(TEXT("the dig-in order was accepted but no wall appeared"));
		}
		return;
	}
	const float WallMaxHealth = WallHealth->GetMaxHealth();
	if (WallMaxHealth > M_LastWallMaxHealth)
	{
		UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP wall max health %.0f at frame %d"), WallMaxHealth,
		       M_FramesSaved);
		M_LastWallMaxHealth = WallMaxHealth;
		M_LastWallGrowthGameSeconds = GetGameSeconds();
	}
	if (GetGameSeconds() - M_LastWallGrowthGameSeconds >= WallDoneAfterStableSeconds)
	{
		UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP wall done (%.0f hp) after %.2f s, at frame %d"),
		       WallMaxHealth, GetSecondsInPhase(), M_FramesSaved);
		OrderTargetAttack();
		return;
	}
	if (GetSecondsInPhase() > MaxDigInSeconds)
	{
		Fail(TEXT("the wall never finished building"));
	}
}

void UDigInClipCaptureSubsystem::TickUnderFire()
{
	using namespace DigInClipPrivate;
	SaveFrames();
	TrackHits();
	if (not GetIsActorAlive(M_Tank))
	{
		UE_LOG(LogRTSDigInClip, Warning, TEXT("DIGIN_CLIP the Panzer IV died at frame %d"), M_FramesSaved);
		Finish(true);
		return;
	}
	const int32 TargetsDead = M_Targets.Num() - CountLivingTargets();
	if (TargetsDead > M_TargetsDead)
	{
		M_TargetsDead = TargetsDead;
		UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP T-26 %d died %.2f s in (frame %d)"), M_TargetsDead,
		       GetSecondsInPhase(), M_FramesSaved);
		if (M_TargetsDead == M_Targets.Num())
		{
			M_AllTargetsDeadGameSeconds = GetGameSeconds();
		}
	}
	if (M_AllTargetsDeadGameSeconds >= 0.0 && GetGameSeconds() - M_AllTargetsDeadGameSeconds >= SecondsAfterTargetDeath)
	{
		Finish(true);
		return;
	}
	if (GetSecondsInPhase() > MaxUnderFireSeconds)
	{
		UE_LOG(LogRTSDigInClip, Warning, TEXT("DIGIN_CLIP %d T-26s survived %.0f s"), CountLivingTargets(),
		       MaxUnderFireSeconds);
		Finish(true);
	}
}

bool UDigInClipCaptureSubsystem::SpawnUnits()
{
	using namespace DigInClipPrivate;
	UWorld& World = *GetWorld();
	if (not FGrenadeTestHelpers::ProjectOntoNavMesh(World, M_Spot, SpotNavExtent))
	{
		UE_LOG(LogRTSDigInClip, Warning, TEXT("DIGIN_CLIP no navmesh near the spot %s"), *M_Spot.ToCompactString());
	}
	for (const FVector& TargetOffset : TargetOffsets)
	{
		M_Targets.Add(FGrenadeTestHelpers::SpawnUnitForPlayer(World, T26Tank,
		                                                      FTransform(TargetRotation, M_Spot + TargetOffset),
		                                                      EnemyOwner));
	}
	M_Tank = FGrenadeTestHelpers::SpawnUnitForPlayer(World, TankClassPath, FTransform(TankRotation, M_Spot),
	                                                 PlayerOwner);
	const bool bSpawnedAllTargets = not M_Targets.ContainsByPredicate(
		[](const TWeakObjectPtr<AActor>& Target) { return not Target.IsValid(); });
	UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP spot %s: tank=%s, all %d T-26s spawned=%d"),
	       *M_Spot.ToCompactString(), M_Tank.IsValid() ? *M_Tank->GetName() : TEXT("none"), M_Targets.Num(),
	       bSpawnedAllTargets ? 1 : 0);
	return M_Tank.IsValid() && bSpawnedAllTargets;
}

bool UDigInClipCaptureSubsystem::CreateCameras()
{
	using namespace DigInClipPrivate;
	return CreateCamera(M_Spot + CloseLookAtOffset, CloseCameraRotation, CloseCameraDistance, TEXT("Frames"),
	                    M_CloseCamera) &&
		CreateCamera(M_Spot + WideLookAtOffset, WideCameraRotation, WideCameraDistance, TEXT("FramesWide"), M_WideCamera);
}

bool UDigInClipCaptureSubsystem::CreateCamera(const FVector& LookAt, const FRotator& Rotation, const float Distance,
                                              const TCHAR* FolderName, FDigInClipCamera& OutCamera) const
{
	ASceneCapture2D* Capture = nullptr;
	UTextureRenderTarget2D* RenderTarget = nullptr;
	if (not FGrenadeTestHelpers::CreateCaptureCamera(*GetWorld(), DigInClipPrivate::CameraFieldOfView, Capture,
	                                                 RenderTarget))
	{
		return false;
	}
	Capture->SetActorLocationAndRotation(LookAt - Rotation.Vector() * Distance, Rotation);
	OutCamera.Capture = Capture;
	OutCamera.RenderTarget = RenderTarget;
	OutCamera.OutputDirectory = FPaths::ProjectSavedDir() / TEXT("InfoTakeover/DigIn") / FolderName;
	return true;
}

bool UDigInClipCaptureSubsystem::GetIsActorAlive(const TWeakObjectPtr<AActor>& Unit) const
{
	// IsUnitAlive is not const on AActor.
	AActor* const UnitActor = Unit.Get();
	return IsValid(UnitActor) && UnitActor->IsUnitAlive();
}

int32 UDigInClipCaptureSubsystem::CountLivingTargets() const
{
	int32 LivingTargets = 0;
	for (const TWeakObjectPtr<AActor>& Target : M_Targets)
	{
		LivingTargets += GetIsActorAlive(Target) ? 1 : 0;
	}
	return LivingTargets;
}

void UDigInClipCaptureSubsystem::DisableTurretsOf(const AActor* Unit) const
{
	const ATankMaster* const Tank = Cast<ATankMaster>(Unit);
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

void UDigInClipCaptureSubsystem::OrderDigIn()
{
	ICommands* const TankCommands = Cast<ICommands>(M_Tank.Get());
	const ECommandQueueError OrderResult = TankCommands != nullptr
		                                       ? TankCommands->DigIn(true)
		                                       : ECommandQueueError::AbilityNotAllowed;
	if (OrderResult != ECommandQueueError::NoError)
	{
		Fail(FString::Printf(TEXT("the Panzer IV refused the dig-in order (error %d)"), static_cast<int32>(OrderResult)));
		return;
	}
	UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP dig-in ordered at %.2f s"), GetGameSeconds());
	EnterPhase(EDigInClipPhase::DiggingIn);
}

void UDigInClipCaptureSubsystem::OrderTargetAttack()
{
	for (const TWeakObjectPtr<AActor>& Target : M_Targets)
	{
		ICommands* const TargetCommands = Cast<ICommands>(Target.Get());
		const ECommandQueueError OrderResult = TargetCommands != nullptr
			                                       ? TargetCommands->AttackActor(M_Tank.Get(), true)
			                                       : ECommandQueueError::AbilityNotAllowed;
		if (OrderResult != ECommandQueueError::NoError)
		{
			Fail(TEXT("a T-26 refused the attack order"));
			return;
		}
	}
	const UHealthComponent* const WallHealth = GetWallHealth();
	const UHealthComponent* const TankHealth = GetTankHealth();
	M_LastWallHealth = IsValid(WallHealth) ? WallHealth->GetCurrentHealth() : -1.f;
	M_LastTankHealth = IsValid(TankHealth) ? TankHealth->GetCurrentHealth() : -1.f;
	UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP T-26s attack at frame %d (wall %.0f hp, tank %.0f hp)"),
	       M_FramesSaved, M_LastWallHealth, M_LastTankHealth);
	EnterPhase(EDigInClipPhase::UnderFire);
}

ADigInWall* UDigInClipCaptureSubsystem::FindWall()
{
	if (M_Wall.IsValid())
	{
		return M_Wall.Get();
	}
	const AActor* const Tank = M_Tank.Get();
	if (not IsValid(Tank))
	{
		return nullptr;
	}
	for (TActorIterator<ADigInWall> WallIt(GetWorld()); WallIt; ++WallIt)
	{
		if (WallIt->GetOwner() == Tank)
		{
			M_Wall = *WallIt;
			UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP wall %s at %s"), *WallIt->GetName(),
			       *WallIt->GetActorLocation().ToCompactString());
			return *WallIt;
		}
	}
	return nullptr;
}

UHealthComponent* UDigInClipCaptureSubsystem::GetWallHealth()
{
	const ADigInWall* const Wall = FindWall();
	return IsValid(Wall) ? Wall->GetHealthComponent() : nullptr;
}

UHealthComponent* UDigInClipCaptureSubsystem::GetTankHealth() const
{
	const AActor* const Tank = M_Tank.Get();
	return IsValid(Tank) ? Tank->FindComponentByClass<UHealthComponent>() : nullptr;
}

void UDigInClipCaptureSubsystem::TrackHits()
{
	const UHealthComponent* const WallHealth = M_Wall.IsValid() ? M_Wall->GetHealthComponent() : nullptr;
	if (IsValid(WallHealth) && GetIsActorAlive(TWeakObjectPtr<AActor>(M_Wall.Get())))
	{
		const float Health = WallHealth->GetCurrentHealth();
		if (Health < M_LastWallHealth)
		{
			++M_WallHits;
			UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP wall hit %d at frame %d: -%.0f -> %.0f hp"), M_WallHits,
			       M_FramesSaved, M_LastWallHealth - Health, Health);
		}
		M_LastWallHealth = Health;
	}
	else if (not bM_HasLoggedWallDeath && M_LastWallHealth >= 0.f)
	{
		bM_HasLoggedWallDeath = true;
		UE_LOG(LogRTSDigInClip, Warning, TEXT("DIGIN_CLIP the wall is down at frame %d"), M_FramesSaved);
	}
	const UHealthComponent* const TankHealth = GetTankHealth();
	if (IsValid(TankHealth))
	{
		const float Health = TankHealth->GetCurrentHealth();
		if (Health < M_LastTankHealth)
		{
			++M_TankHits;
			UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP tank hit %d at frame %d: -%.0f -> %.0f hp"), M_TankHits,
			       M_FramesSaved, M_LastTankHealth - Health, Health);
		}
		M_LastTankHealth = Health;
	}
}

void UDigInClipCaptureSubsystem::SaveFrames()
{
	const FString FileName = FString::Printf(TEXT("Frame_%04d.png"), M_FramesSaved);
	for (const FDigInClipCamera* const Camera : {&M_CloseCamera, &M_WideCamera})
	{
		if (IsValid(Camera->RenderTarget))
		{
			FGrenadeTestHelpers::SaveCapture(*GetWorld(), *Camera->RenderTarget, Camera->OutputDirectory, FileName);
		}
	}
	++M_FramesSaved;
}

void UDigInClipCaptureSubsystem::Fail(const FString& Reason)
{
	UE_LOG(LogRTSDigInClip, Error, TEXT("DIGIN_CLIP FAIL: %s"), *Reason);
	Finish(false);
}

void UDigInClipCaptureSubsystem::Finish(const bool bSucceeded)
{
	EnterPhase(EDigInClipPhase::Done);
	UE_LOG(LogRTSDigInClip, Display, TEXT("DIGIN_CLIP done: %d frames, %d wall hits, %d tank hits -> %s"),
	       M_FramesSaved, M_WallHits, M_TankHits, bSucceeded ? TEXT("OK") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("DigInClipCapture"));
}

#else

bool UDigInClipCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UDigInClipCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UDigInClipCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UDigInClipCaptureSubsystem::Deinitialize() { Super::Deinitialize(); }
void UDigInClipCaptureSubsystem::Tick(float DeltaTime) {}

TStatId UDigInClipCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDigInClipCaptureSubsystem, STATGROUP_Tickables);
}

#endif
