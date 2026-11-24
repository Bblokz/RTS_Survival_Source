// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WespeBarrageClipCaptureSubsystem.h"

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
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSWespeBarrageClip, Log, All);

namespace WespeBarrageClipPrivate
{
	const TCHAR* const CaptureSwitch = TEXT("WespeBarrageClipCapture");
	const TCHAR* const SpotSwitch = TEXT("AbilityClipAt=");
	const TCHAR* const WespeClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Wespe/Ger_Vehicle_Wespe."
		"Ger_Vehicle_Wespe_C");
	const TCHAR* const T26Tank = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Light/T26/Rus_Vehicle_T26.Rus_Vehicle_T26_C");
	const TCHAR* const MosinSquad = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");
	// A player tank short of the target area gives vision on it: in the fog of war the targets would be hidden and only
	// flash up in the blasts. It stands out of the target camera's view and never fires.
	const TCHAR* const SpotterTank = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/PanzerII/Ger_Vehicle_PanzerII_F."
		"Ger_Vehicle_PanzerII_F_C");
	const FVector SpotterOffsetFromTarget(-2000.f, 0.f, 0.f);
	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;

	// One Wespe fires a shell about every 5 s even in a barrage, so a battery of three makes the clip read as one.
	const TArray<FVector> WespeOffsets = {FVector(0.f, -450.f, 0.f), FVector(-150.f, 0.f, 0.f), FVector(0.f, 450.f, 0.f)};
	const FRotator WespeRotation(0.f, 0.f, 0.f);
	// The target area, far inside the leFH 18's 9680 range; the barrage spreads shells over a 1500 radius around it.
	const FVector TargetCenterOffset(4500.f, 0.f, 0.f);
	const FVector T26Offset(0.f, -250.f, 0.f);
	const FVector SquadOffset(150.f, 300.f, 0.f);
	const FRotator TargetRotation(0.f, 200.f, 0.f);

	// Battery camera: behind the battery's left, looking the way the guns fire, so the shells climb away from the
	// camera. The muzzle smoke cloud is huge: from 1700-1900 away it filled the picture on either side of the guns.
	const FVector BatteryLookAtOffset(500.f, 0.f, 150.f);
	const FRotator BatteryCameraRotation(-35.f, -55.f, 0.f);
	constexpr float BatteryCameraDistance = 3500.f;
	// Target camera: high three-quarter view over the target area, wide enough for the spread.
	const FRotator TargetCameraRotation(-38.f, 35.f, 0.f);
	constexpr float TargetCameraDistance = 3200.f;
	constexpr float CameraFieldOfView = 55.f;

	constexpr double UnitSettleSeconds = 4.0;
	const FVector SpotNavExtent(3000.f, 3000.f, 2000.f);
	constexpr double MaxUnitWaitSeconds = 60.0;
	// The barrage behaviour lasts 20 s; shells still in the air land a few seconds later.
	constexpr double RecordingSeconds = 24.0;
	constexpr float HowitzerMinimalCalibre = 100.f;
}

bool UWespeBarrageClipCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), WespeBarrageClipPrivate::CaptureSwitch);
}

void UWespeBarrageClipCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The enemy AI would otherwise use the targets' abilities; the clip only shows the barrage.
	GetMutableDefault<UEnemyUnitAbilityAISettings>()->bEnabled = false;
}

void UWespeBarrageClipCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EWespeBarrageClipPhase::WaitingForGameStart);
	UE_LOG(LogRTSWespeBarrageClip, Display, TEXT("WESPE_BARRAGE_CLIP started"));
}

void UWespeBarrageClipCaptureSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UWespeBarrageClipCaptureSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EWespeBarrageClipPhase::Inactive || M_Phase == EWespeBarrageClipPhase::Done || not IsValid(GetWorld()))
	{
		return;
	}
	if (M_Phase != EWespeBarrageClipPhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
		DisableTargetTurrets();
		DisableTurretsOf(M_Spotter.Get());
	}
	switch (M_Phase)
	{
	case EWespeBarrageClipPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EWespeBarrageClipPhase::WaitingForUnits:
		TickWaitingForUnits();
		break;
	case EWespeBarrageClipPhase::Recording:
		TickRecording();
		break;
	default:
		break;
	}
}

TStatId UWespeBarrageClipCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWespeBarrageClipCaptureSubsystem, STATGROUP_Tickables);
}

void UWespeBarrageClipCaptureSubsystem::EnterPhase(const EWespeBarrageClipPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartGameSeconds = GetGameSeconds();
}

double UWespeBarrageClipCaptureSubsystem::GetGameSeconds() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

double UWespeBarrageClipCaptureSubsystem::GetSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

void UWespeBarrageClipCaptureSubsystem::TickWaitingForGameStart()
{
	UWorld& World = *GetWorld();
	if (not FGrenadeTestHelpers::TryStartGame(World, M_PlayerController))
	{
		return;
	}
	if (not FGrenadeTestHelpers::FindTestSpot(World, WespeBarrageClipPrivate::SpotSwitch, M_Spot))
	{
		Fail(TEXT("no spot: pass -AbilityClipAt=X,Y or use a map with a player start"));
		return;
	}
	if (not SpawnUnits() || not CreateCameras())
	{
		Fail(TEXT("could not spawn the Wespes, the targets or the capture cameras"));
		return;
	}
	EnterPhase(EWespeBarrageClipPhase::WaitingForUnits);
}

void UWespeBarrageClipCaptureSubsystem::TickWaitingForUnits()
{
	using namespace WespeBarrageClipPrivate;
	// The Wespes would shell the targets on their own while the units settle.
	HoldBatteryFire();
	if (not GetAreUnitsReady())
	{
		if (GetSecondsInPhase() > MaxUnitWaitSeconds)
		{
			Fail(TEXT("the Wespes or the targets never finished loading"));
		}
		return;
	}
	if (GetSecondsInPhase() < UnitSettleSeconds)
	{
		return;
	}
	OrderBarrage();
}

void UWespeBarrageClipCaptureSubsystem::TickRecording()
{
	SaveFrames();
	TrackShots();
	if (GetSecondsInPhase() < WespeBarrageClipPrivate::RecordingSeconds)
	{
		return;
	}
	FString ShotFrames;
	for (const int32 ShotFrame : M_ShotFrames)
	{
		ShotFrames += FString::Printf(TEXT(" %d"), ShotFrame);
	}
	UE_LOG(LogRTSWespeBarrageClip, Display, TEXT("WESPE_BARRAGE_CLIP %d battery shots; frames:%s"), M_ShotFrames.Num(),
	       *ShotFrames);
	UE_LOG(LogRTSWespeBarrageClip, Display, TEXT("WESPE_BARRAGE_CLIP %d of %d targets alive at the end"),
	       CountLivingTargets(), M_Targets.Num());
	Finish(true);
}

bool UWespeBarrageClipCaptureSubsystem::SpawnUnits()
{
	using namespace WespeBarrageClipPrivate;
	UWorld& World = *GetWorld();
	// Map anchors can sit at camera height over empty space; the bench's wide search finds the walkable ground.
	if (not FGrenadeTestHelpers::ProjectOntoNavMesh(World, M_Spot, SpotNavExtent))
	{
		UE_LOG(LogRTSWespeBarrageClip, Warning, TEXT("WESPE_BARRAGE_CLIP no navmesh near the spot %s"),
		       *M_Spot.ToCompactString());
	}
	M_TargetCenter = M_Spot + TargetCenterOffset;
	if (not FGrenadeTestHelpers::ProjectOntoNavMesh(World, M_TargetCenter, SpotNavExtent))
	{
		UE_LOG(LogRTSWespeBarrageClip, Warning, TEXT("WESPE_BARRAGE_CLIP no navmesh near the target area %s"),
		       *M_TargetCenter.ToCompactString());
	}
	for (const FVector& WespeOffset : WespeOffsets)
	{
		M_Wespes.Add(FGrenadeTestHelpers::SpawnUnitForPlayer(
			World, WespeClassPath, FTransform(WespeRotation, M_Spot + WespeOffset), PlayerOwner));
		M_LastHowitzerRounds.Add(INDEX_NONE);
	}
	M_Spotter = FGrenadeTestHelpers::SpawnUnitForPlayer(World, SpotterTank,
	                                                    FTransform(WespeRotation, M_TargetCenter + SpotterOffsetFromTarget),
	                                                    PlayerOwner);
	M_Targets.Add(FGrenadeTestHelpers::SpawnUnitForPlayer(World, T26Tank,
	                                                      FTransform(TargetRotation, M_TargetCenter + T26Offset),
	                                                      EnemyOwner));
	M_Targets.Add(FGrenadeTestHelpers::SpawnUnitForPlayer(World, MosinSquad,
	                                                      FTransform(TargetRotation, M_TargetCenter + SquadOffset),
	                                                      EnemyOwner));
	UE_LOG(LogRTSWespeBarrageClip, Display, TEXT("WESPE_BARRAGE_CLIP battery at %s, targets at %s"),
	       *M_Spot.ToCompactString(), *M_TargetCenter.ToCompactString());
	for (const TWeakObjectPtr<AActor>& Unit : M_Wespes)
	{
		if (not Unit.IsValid())
		{
			return false;
		}
	}
	for (const TWeakObjectPtr<AActor>& Unit : M_Targets)
	{
		if (not Unit.IsValid())
		{
			return false;
		}
	}
	return true;
}

bool UWespeBarrageClipCaptureSubsystem::CreateCameras()
{
	using namespace WespeBarrageClipPrivate;
	return CreateCamera(M_Spot + BatteryLookAtOffset, BatteryCameraRotation, BatteryCameraDistance, TEXT("Frames"),
	                    M_BatteryCamera) &&
		CreateCamera(M_TargetCenter, TargetCameraRotation, TargetCameraDistance, TEXT("FramesTarget"), M_TargetCamera);
}

bool UWespeBarrageClipCaptureSubsystem::CreateCamera(const FVector& LookAt, const FRotator& Rotation,
                                                     const float Distance, const TCHAR* FolderName,
                                                     FWespeBarrageClipCamera& OutCamera) const
{
	ASceneCapture2D* Capture = nullptr;
	UTextureRenderTarget2D* RenderTarget = nullptr;
	if (not FGrenadeTestHelpers::CreateCaptureCamera(*GetWorld(), WespeBarrageClipPrivate::CameraFieldOfView, Capture,
	                                                 RenderTarget))
	{
		return false;
	}
	Capture->SetActorLocationAndRotation(LookAt - Rotation.Vector() * Distance, Rotation);
	OutCamera.Capture = Capture;
	OutCamera.RenderTarget = RenderTarget;
	OutCamera.OutputDirectory = FPaths::ProjectSavedDir() / TEXT("InfoTakeover/WespeBarrage") / FolderName;
	return true;
}

bool UWespeBarrageClipCaptureSubsystem::GetAreUnitsReady() const
{
	for (const TWeakObjectPtr<AActor>& Wespe : M_Wespes)
	{
		AActor* const WespeActor = Wespe.Get();
		if (not IsValid(WespeActor) || not WespeActor->IsUnitAlive() || not IsValid(FindHowitzer(WespeActor)))
		{
			return false;
		}
	}
	ASquadController* const Squad = M_Targets.Num() > 1 ? Cast<ASquadController>(M_Targets[1].Get()) : nullptr;
	return IsValid(Squad) && Squad->GetIsSquadFullyLoadedAndInitialized() && CountLivingTargets() == M_Targets.Num();
}

int32 UWespeBarrageClipCaptureSubsystem::CountLivingTargets() const
{
	int32 LivingTargets = 0;
	for (const TWeakObjectPtr<AActor>& Target : M_Targets)
	{
		AActor* const TargetActor = Target.Get();
		if (not IsValid(TargetActor))
		{
			continue;
		}
		ASquadController* const Squad = Cast<ASquadController>(TargetActor);
		const bool bIsAlive = IsValid(Squad)
			                      ? FGrenadeTestHelpers::GetLivingSoldierCount(*Squad) > 0
			                      : TargetActor->IsUnitAlive();
		LivingTargets += bIsAlive ? 1 : 0;
	}
	return LivingTargets;
}

void UWespeBarrageClipCaptureSubsystem::DisableTargetTurrets() const
{
	for (const TWeakObjectPtr<AActor>& Target : M_Targets)
	{
		const ATankMaster* const Tank = Cast<ATankMaster>(Target.Get());
		if (not IsValid(Tank))
		{
			continue;
		}
		for (ACPPTurretsMaster* Turret : Tank->GetTurrets())
		{
			if (IsValid(Turret))
			{
				Turret->DisableTurret();
			}
		}
	}
}

void UWespeBarrageClipCaptureSubsystem::HoldBatteryFire() const
{
	for (const TWeakObjectPtr<AActor>& Wespe : M_Wespes)
	{
		DisableTurretsOf(Wespe.Get());
	}
}

void UWespeBarrageClipCaptureSubsystem::DisableTurretsOf(const AActor* Unit) const
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

void UWespeBarrageClipCaptureSubsystem::OrderBarrage()
{
	int32 OrdersAccepted = 0;
	for (const TWeakObjectPtr<AActor>& Wespe : M_Wespes)
	{
		ICommands* const WespeCommands = Cast<ICommands>(Wespe.Get());
		// As a plain click on the card and then the ground: the aim ability runs at once.
		const ECommandQueueError OrderResult = WespeCommands != nullptr
			                                       ? WespeCommands->AimAbility(
				                                       M_TargetCenter, true, EAimAbilityType::HeArtilleryBarrage)
			                                       : ECommandQueueError::AbilityNotAllowed;
		if (OrderResult == ECommandQueueError::NoError)
		{
			++OrdersAccepted;
			continue;
		}
		UE_LOG(LogRTSWespeBarrageClip, Error, TEXT("WESPE_BARRAGE_CLIP a Wespe refused the barrage (error %d)"),
		       static_cast<int32>(OrderResult));
	}
	if (OrdersAccepted == 0)
	{
		Fail(TEXT("no Wespe accepted the barrage order"));
		return;
	}
	UE_LOG(LogRTSWespeBarrageClip, Display, TEXT("WESPE_BARRAGE_CLIP barrage ordered on %d Wespes at %.2f s"),
	       OrdersAccepted, GetGameSeconds());
	EnterPhase(EWespeBarrageClipPhase::Recording);
}

UWeaponState* UWespeBarrageClipCaptureSubsystem::FindHowitzer(const AActor* Wespe) const
{
	const ATankMaster* const Tank = Cast<ATankMaster>(Wespe);
	if (not IsValid(Tank))
	{
		return nullptr;
	}
	for (ACPPTurretsMaster* const Turret : Tank->GetTurrets())
	{
		if (not IsValid(Turret))
		{
			continue;
		}
		const TArray<UWeaponState*> TurretWeapons = Turret->GetWeapons();
		for (UWeaponState* const Weapon : TurretWeapons)
		{
			if (IsValid(Weapon) && Weapon->GetRawWeaponData().WeaponCalibre >= WespeBarrageClipPrivate::HowitzerMinimalCalibre)
			{
				return Weapon;
			}
		}
	}
	return nullptr;
}

void UWespeBarrageClipCaptureSubsystem::TrackShots()
{
	for (int32 WespeIndex = 0; WespeIndex < M_Wespes.Num() && WespeIndex < M_LastHowitzerRounds.Num(); ++WespeIndex)
	{
		const UWeaponState* const Howitzer = FindHowitzer(M_Wespes[WespeIndex].Get());
		if (not IsValid(Howitzer))
		{
			continue;
		}
		const int32 RoundsInMagazine = Howitzer->GetCurrentMagCapacity();
		if (M_LastHowitzerRounds[WespeIndex] != INDEX_NONE && RoundsInMagazine < M_LastHowitzerRounds[WespeIndex])
		{
			M_ShotFrames.Add(M_FramesSaved);
		}
		M_LastHowitzerRounds[WespeIndex] = RoundsInMagazine;
	}
}

void UWespeBarrageClipCaptureSubsystem::SaveFrames()
{
	const FString FileName = FString::Printf(TEXT("Frame_%04d.png"), M_FramesSaved);
	for (const FWespeBarrageClipCamera* const Camera : {&M_BatteryCamera, &M_TargetCamera})
	{
		if (IsValid(Camera->RenderTarget))
		{
			FGrenadeTestHelpers::SaveCapture(*GetWorld(), *Camera->RenderTarget, Camera->OutputDirectory, FileName);
		}
	}
	++M_FramesSaved;
}

void UWespeBarrageClipCaptureSubsystem::Fail(const FString& Reason)
{
	UE_LOG(LogRTSWespeBarrageClip, Error, TEXT("WESPE_BARRAGE_CLIP FAIL: %s"), *Reason);
	Finish(false);
}

void UWespeBarrageClipCaptureSubsystem::Finish(const bool bSucceeded)
{
	EnterPhase(EWespeBarrageClipPhase::Done);
	UE_LOG(LogRTSWespeBarrageClip, Display, TEXT("WESPE_BARRAGE_CLIP done: %d frames -> %s"), M_FramesSaved,
	       bSucceeded ? TEXT("OK") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("WespeBarrageClipCapture"));
}

#else

bool UWespeBarrageClipCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UWespeBarrageClipCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UWespeBarrageClipCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UWespeBarrageClipCaptureSubsystem::Deinitialize() { Super::Deinitialize(); }
void UWespeBarrageClipCaptureSubsystem::Tick(float DeltaTime) {}

TStatId UWespeBarrageClipCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWespeBarrageClipCaptureSubsystem, STATGROUP_Tickables);
}

#endif
