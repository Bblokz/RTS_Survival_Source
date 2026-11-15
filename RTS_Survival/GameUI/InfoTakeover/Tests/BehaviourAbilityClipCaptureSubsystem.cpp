// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourAbilityClipCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Enemy/EnemyUnitAbilityAI/EnemyUnitAbilityAISettings.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAbilityClip, Log, All);

namespace BehaviourAbilityClipPrivate
{
	const TCHAR* const SpotSwitch = TEXT("AbilityClipAt=");
	const TCHAR* const DirectorySwitch = TEXT("AbilityClipDir=");
	const TCHAR* const SettleSwitch = TEXT("AbilityClipSettle=");
	const TCHAR* const T26Tank = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Light/T26/Rus_Vehicle_T26.Rus_Vehicle_T26_C");
	constexpr uint8 PlayerOwner = 1;
	constexpr uint8 EnemyOwner = 2;

	/**
	 * Rattle-Feed (Panzer II F, BP_Beh_RapidFire_Autocannon): on the moment the first 10-round magazine has reloaded.
	 * The T-26 at 135 deg lives through both Rattle-Feed magazines; at 110-130 deg it died 2-3 s in, too soon to see the
	 * burst, and at 140-150 deg it lived through 12 s of it.
	 */
	FBehaviourAbilityClipScenario MakeRattleFeedScenario()
	{
		FBehaviourAbilityClipScenario Scenario;
		Scenario.CaptureSwitch = TEXT("RattleFeedClipCapture");
		Scenario.FolderName = TEXT("RattleFeed");
		Scenario.TankClassPath = TEXT(
			"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/PanzerII/Ger_Vehicle_PanzerII_F."
			"Ger_Vehicle_PanzerII_F_C");
		Scenario.AbilityType = EBehaviourAbilityType::RapidFireAutoCannon;
		Scenario.BehaviourNamePart = TEXT("RapidFire_Autocannon");
		Scenario.ReloadsBeforeAbility = 1;
		Scenario.TargetYaw = 135.f;
		return Scenario;
	}

	/**
	 * Adrenaline Shot (Panzer 38(t), Beh_WeaponVFX_Pz38AdrenalineShot): the 37 mm holds one round and reloads in 3.3 s,
	 * so two normal shots and reloads come first. The T-26 faces the gun head on to last through the faster shots.
	 */
	FBehaviourAbilityClipScenario MakeAdrenalineShotScenario()
	{
		FBehaviourAbilityClipScenario Scenario;
		Scenario.CaptureSwitch = TEXT("AdrenalineShotClipCapture");
		Scenario.FolderName = TEXT("AdrenalineShot");
		Scenario.TankClassPath = TEXT(
			"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T."
			"Ger_Vehicle_Panzer38T_C");
		Scenario.AbilityType = EBehaviourAbilityType::AdrenalineShot;
		Scenario.BehaviourNamePart = TEXT("AdrenalineShot");
		Scenario.ReloadsBeforeAbility = 2;
		Scenario.TargetYaw = 180.f;
		return Scenario;
	}

	// The tank stands on the spot facing +X, seen in profile; the T-26 is parked in front of it, well inside the gun's
	// range, so the shots cross the picture left to right.
	const FRotator TankRotation(0.f, 0.f, 0.f);
	const FVector TargetOffset(1300.f, 0.f, 0.f);

	// Side view from the tank's left-hand flank (+Y), looking across the firing line at its middle, so the tank stands
	// on the left of the picture.
	const FVector CameraLookAtOffset(650.f, 0.f, 60.f);
	const FRotator CameraRotation(-30.f, -80.f, 0.f);
	constexpr float CameraDistance = 2000.f;
	constexpr float CameraFieldOfView = 55.f;

	constexpr double UnitSettleSeconds = 4.0;
	const FVector SpotNavExtent(3000.f, 3000.f, 2000.f);
	constexpr double MaxUnitWaitSeconds = 60.0;
	constexpr double MaxNormalFireSeconds = 20.0;
	constexpr double SecondsAfterTargetDeath = 2.5;
	// Both abilities last 12-15 s; the clip stops earlier if the T-26 holds out.
	constexpr double MaxAbilitySeconds = 12.0;
	// The behaviour may be added a tick or two after the order; it must be on the tank by then.
	constexpr double MaxBehaviourAppearSeconds = 1.0;
}

bool UBehaviourAbilityClipCaptureSubsystem::FindScenarioOnCommandLine(FBehaviourAbilityClipScenario& OutScenario)
{
	using namespace BehaviourAbilityClipPrivate;
	for (const FBehaviourAbilityClipScenario& Scenario : {MakeRattleFeedScenario(), MakeAdrenalineShotScenario()})
	{
		if (FParse::Param(FCommandLine::Get(), Scenario.CaptureSwitch))
		{
			OutScenario = Scenario;
			return true;
		}
	}
	return false;
}

bool UBehaviourAbilityClipCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	FBehaviourAbilityClipScenario Scenario;
	return FindScenarioOnCommandLine(Scenario);
}

void UBehaviourAbilityClipCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FindScenarioOnCommandLine(M_Scenario);
	// The enemy AI would otherwise use the T-26's abilities; the clip only shows the player tank's gun.
	GetMutableDefault<UEnemyUnitAbilityAISettings>()->bEnabled = false;
}

void UBehaviourAbilityClipCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / M_Scenario.FolderName / TEXT("Frames");
	FParse::Value(FCommandLine::Get(), BehaviourAbilityClipPrivate::DirectorySwitch, M_OutputDirectory);
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EBehaviourAbilityClipPhase::WaitingForGameStart);
	UE_LOG(LogRTSAbilityClip, Display, TEXT("ABILITY_CLIP %s started, frames to %s"), M_Scenario.FolderName,
	       *M_OutputDirectory);
}

void UBehaviourAbilityClipCaptureSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UBehaviourAbilityClipCaptureSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EBehaviourAbilityClipPhase::Inactive || M_Phase == EBehaviourAbilityClipPhase::Done ||
		not IsValid(GetWorld()))
	{
		return;
	}
	if (M_Phase != EBehaviourAbilityClipPhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
		// The T-26 never shoots back; the clip is about the player tank's gun, not who wins the duel.
		DisableTurretsOf(M_Target);
	}
	if (M_Phase == EBehaviourAbilityClipPhase::WaitingForUnits)
	{
		// The tank would open fire on its own while the units settle; the attack order re-enables its guns.
		DisableTurretsOf(M_Tank);
	}
	if (M_Phase == EBehaviourAbilityClipPhase::NormalFire || M_Phase == EBehaviourAbilityClipPhase::AbilityActive)
	{
		TrackMainGun();
	}
	switch (M_Phase)
	{
	case EBehaviourAbilityClipPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EBehaviourAbilityClipPhase::WaitingForUnits:
		TickWaitingForUnits();
		break;
	case EBehaviourAbilityClipPhase::NormalFire:
		TickNormalFire();
		break;
	case EBehaviourAbilityClipPhase::AbilityActive:
		TickAbilityActive();
		break;
	default:
		break;
	}
}

TStatId UBehaviourAbilityClipCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBehaviourAbilityClipCaptureSubsystem, STATGROUP_Tickables);
}

void UBehaviourAbilityClipCaptureSubsystem::EnterPhase(const EBehaviourAbilityClipPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartGameSeconds = GetGameSeconds();
}

double UBehaviourAbilityClipCaptureSubsystem::GetGameSeconds() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

double UBehaviourAbilityClipCaptureSubsystem::GetSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

void UBehaviourAbilityClipCaptureSubsystem::TickWaitingForGameStart()
{
	UWorld& World = *GetWorld();
	if (not FGrenadeTestHelpers::TryStartGame(World, M_PlayerController))
	{
		return;
	}
	if (not FGrenadeTestHelpers::FindTestSpot(World, BehaviourAbilityClipPrivate::SpotSwitch, M_Spot))
	{
		Fail(TEXT("no spot: pass -AbilityClipAt=X,Y or use a map with a player start"));
		return;
	}
	if (not SpawnUnits() || not CreateCamera())
	{
		Fail(TEXT("could not spawn the tank, the T-26 or the capture camera"));
		return;
	}
	EnterPhase(EBehaviourAbilityClipPhase::WaitingForUnits);
}

void UBehaviourAbilityClipCaptureSubsystem::TickWaitingForUnits()
{
	using namespace BehaviourAbilityClipPrivate;
	if (not GetIsActorAlive(M_Tank) || not GetIsActorAlive(M_Target))
	{
		if (GetSecondsInPhase() > MaxUnitWaitSeconds)
		{
			Fail(TEXT("the tank or the T-26 never finished loading"));
		}
		return;
	}
	double SettleSeconds = UnitSettleSeconds;
	FParse::Value(FCommandLine::Get(), SettleSwitch, SettleSeconds);
	if (GetSecondsInPhase() < SettleSeconds)
	{
		return;
	}
	OrderAttack();
}

void UBehaviourAbilityClipCaptureSubsystem::TickNormalFire()
{
	SaveFrame();
	if (not GetIsActorAlive(M_Tank) || not GetIsActorAlive(M_Target))
	{
		Fail(TEXT("a tank died before the ability went on; turn the T-26 to face the gun more"));
		return;
	}
	if (M_ReloadsFinished >= M_Scenario.ReloadsBeforeAbility)
	{
		ActivateAbility();
		return;
	}
	if (GetSecondsInPhase() > BehaviourAbilityClipPrivate::MaxNormalFireSeconds)
	{
		Fail(TEXT("the main gun never finished its reloads at the normal pace"));
	}
}

void UBehaviourAbilityClipCaptureSubsystem::TickAbilityActive()
{
	using namespace BehaviourAbilityClipPrivate;
	SaveFrame();
	CheckAbilityBehaviourAppeared();
	if (M_Phase != EBehaviourAbilityClipPhase::AbilityActive)
	{
		return;
	}
	if (M_TargetDeathGameSeconds < 0.0 && not GetIsActorAlive(M_Target))
	{
		M_TargetDeathGameSeconds = GetGameSeconds();
		UE_LOG(LogRTSAbilityClip, Display, TEXT("ABILITY_CLIP the T-26 died %.2f s after the ability (frame %d)"),
		       GetSecondsInPhase(), M_FramesSaved);
	}
	const bool bHasShownDeath = M_TargetDeathGameSeconds >= 0.0 &&
		GetGameSeconds() - M_TargetDeathGameSeconds >= SecondsAfterTargetDeath;
	if (bHasShownDeath)
	{
		Finish(true);
		return;
	}
	if (GetSecondsInPhase() > MaxAbilitySeconds)
	{
		UE_LOG(LogRTSAbilityClip, Warning, TEXT("ABILITY_CLIP the T-26 survived %.0f s of the ability"),
		       MaxAbilitySeconds);
		Finish(true);
	}
}

void UBehaviourAbilityClipCaptureSubsystem::CheckAbilityBehaviourAppeared()
{
	if (bM_HasSeenAbilityBehaviour)
	{
		return;
	}
	bM_HasSeenAbilityBehaviour = GetHasAbilityBehaviour();
	if (bM_HasSeenAbilityBehaviour)
	{
		UE_LOG(LogRTSAbilityClip, Display, TEXT("ABILITY_CLIP behaviour on the tank at frame %d"), M_FramesSaved);
		return;
	}
	if (GetSecondsInPhase() > BehaviourAbilityClipPrivate::MaxBehaviourAppearSeconds)
	{
		Fail(TEXT("the ability was accepted but its behaviour never appeared on the tank"));
	}
}

bool UBehaviourAbilityClipCaptureSubsystem::SpawnUnits()
{
	using namespace BehaviourAbilityClipPrivate;
	UWorld& World = *GetWorld();
	// Map anchors can sit at camera height over empty space; the bench's wide search finds the walkable ground.
	if (not FGrenadeTestHelpers::ProjectOntoNavMesh(World, M_Spot, SpotNavExtent))
	{
		UE_LOG(LogRTSAbilityClip, Warning, TEXT("ABILITY_CLIP no navmesh near the spot %s"), *M_Spot.ToCompactString());
	}
	const FRotator TargetRotation(0.f, M_Scenario.TargetYaw, 0.f);
	M_Target = FGrenadeTestHelpers::SpawnUnitForPlayer(World, T26Tank, FTransform(TargetRotation, M_Spot + TargetOffset),
	                                                   EnemyOwner);
	M_Tank = FGrenadeTestHelpers::SpawnUnitForPlayer(World, M_Scenario.TankClassPath, FTransform(TankRotation, M_Spot),
	                                                 PlayerOwner);
	UE_LOG(LogRTSAbilityClip, Display, TEXT("ABILITY_CLIP spot %s: tank=%s target=%s"), *M_Spot.ToCompactString(),
	       M_Tank.IsValid() ? *M_Tank->GetName() : TEXT("none"), M_Target.IsValid() ? *M_Target->GetName() : TEXT("none"));
	return M_Tank.IsValid() && M_Target.IsValid();
}

bool UBehaviourAbilityClipCaptureSubsystem::CreateCamera()
{
	using namespace BehaviourAbilityClipPrivate;
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

bool UBehaviourAbilityClipCaptureSubsystem::GetIsActorAlive(const TWeakObjectPtr<AActor>& Unit) const
{
	// IsUnitAlive is not const on AActor.
	AActor* const UnitActor = Unit.Get();
	return IsValid(UnitActor) && UnitActor->IsUnitAlive();
}

void UBehaviourAbilityClipCaptureSubsystem::DisableTurretsOf(const TWeakObjectPtr<AActor>& Unit) const
{
	const ATankMaster* const Tank = Cast<ATankMaster>(Unit.Get());
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

void UBehaviourAbilityClipCaptureSubsystem::OrderAttack()
{
	ICommands* const TankCommands = Cast<ICommands>(M_Tank.Get());
	const ECommandQueueError OrderResult = TankCommands != nullptr
		                                       ? TankCommands->AttackActor(M_Target.Get(), true)
		                                       : ECommandQueueError::AbilityNotAllowed;
	if (OrderResult != ECommandQueueError::NoError)
	{
		Fail(TEXT("the tank refused the attack order"));
		return;
	}
	UE_LOG(LogRTSAbilityClip, Display, TEXT("ABILITY_CLIP attack ordered at %.2f s"), GetGameSeconds());
	EnterPhase(EBehaviourAbilityClipPhase::NormalFire);
}

void UBehaviourAbilityClipCaptureSubsystem::ActivateAbility()
{
	ICommands* const TankCommands = Cast<ICommands>(M_Tank.Get());
	// As a plain click on the card: applied at once, without dropping the attack order (false would queue it behind).
	const ECommandQueueError AbilityResult = TankCommands != nullptr
		                                         ? TankCommands->ActivateBehaviourAbility(M_Scenario.AbilityType, true)
		                                         : ECommandQueueError::AbilityNotAllowed;
	if (AbilityResult != ECommandQueueError::NoError)
	{
		Fail(FString::Printf(TEXT("the tank refused the ability (error %d)"), static_cast<int32>(AbilityResult)));
		return;
	}
	UE_LOG(LogRTSAbilityClip, Display, TEXT("ABILITY_CLIP ability on at frame %d (%.2f s)"), M_FramesSaved,
	       GetGameSeconds());
	LogMainGunShots(TEXT("normal fire"));
	EnterPhase(EBehaviourAbilityClipPhase::AbilityActive);
}

bool UBehaviourAbilityClipCaptureSubsystem::GetHasAbilityBehaviour() const
{
	const AActor* const Tank = M_Tank.Get();
	const UBehaviourComp* const BehaviourComponent = IsValid(Tank) ? Tank->FindComponentByClass<UBehaviourComp>() : nullptr;
	if (not IsValid(BehaviourComponent))
	{
		return false;
	}
	for (const UBehaviour* const Behaviour : BehaviourComponent->GetBehaviours())
	{
		if (IsValid(Behaviour) && Behaviour->GetClass()->GetName().Contains(M_Scenario.BehaviourNamePart))
		{
			return true;
		}
	}
	return false;
}

UWeaponState* UBehaviourAbilityClipCaptureSubsystem::FindMainGun() const
{
	const ATankMaster* const Tank = Cast<ATankMaster>(M_Tank.Get());
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
			if (IsValid(Weapon) && Weapon->GetRawWeaponData().WeaponCalibre >= M_Scenario.MainGunMinimalCalibre)
			{
				return Weapon;
			}
		}
	}
	return nullptr;
}

void UBehaviourAbilityClipCaptureSubsystem::TrackMainGun()
{
	const UWeaponState* const MainGun = FindMainGun();
	if (not IsValid(MainGun))
	{
		return;
	}
	const int32 RoundsInMagazine = MainGun->GetCurrentMagCapacity();
	if (M_LastMainGunRounds != INDEX_NONE && RoundsInMagazine < M_LastMainGunRounds)
	{
		M_MainGunShotFrames.Add(M_FramesSaved);
	}
	if (M_LastMainGunRounds == 0 && RoundsInMagazine > 0)
	{
		++M_ReloadsFinished;
		UE_LOG(LogRTSAbilityClip, Display, TEXT("ABILITY_CLIP reload %d finished at frame %d"), M_ReloadsFinished,
		       M_FramesSaved);
	}
	M_LastMainGunRounds = RoundsInMagazine;
}

void UBehaviourAbilityClipCaptureSubsystem::LogMainGunShots(const TCHAR* PhaseName)
{
	const UWeaponState* const MainGun = FindMainGun();
	FString ShotFrames;
	for (const int32 ShotFrame : M_MainGunShotFrames)
	{
		ShotFrames += FString::Printf(TEXT(" %d"), ShotFrame);
	}
	UE_LOG(LogRTSAbilityClip, Display,
	       TEXT("ABILITY_CLIP %s: %d main gun shots in %.2f s (cooldown %.2f s, magazine %d, reload %.2f s); frames:%s"),
	       PhaseName, M_MainGunShotFrames.Num(), GetSecondsInPhase(),
	       IsValid(MainGun) ? MainGun->GetRawWeaponData().BaseCooldown : -1.f,
	       IsValid(MainGun) ? MainGun->GetRawWeaponData().MagCapacity : -1,
	       IsValid(MainGun) ? MainGun->GetRawWeaponData().ReloadSpeed : -1.f, *ShotFrames);
	M_MainGunShotFrames.Reset();
}

void UBehaviourAbilityClipCaptureSubsystem::SaveFrame()
{
	if (not IsValid(M_RenderTarget))
	{
		return;
	}
	FGrenadeTestHelpers::SaveCapture(*GetWorld(), *M_RenderTarget, M_OutputDirectory,
	                                 FString::Printf(TEXT("Frame_%04d.png"), M_FramesSaved));
	++M_FramesSaved;
}

void UBehaviourAbilityClipCaptureSubsystem::Fail(const FString& Reason)
{
	UE_LOG(LogRTSAbilityClip, Error, TEXT("ABILITY_CLIP FAIL: %s"), *Reason);
	Finish(false);
}

void UBehaviourAbilityClipCaptureSubsystem::Finish(const bool bSucceeded)
{
	if (M_Phase == EBehaviourAbilityClipPhase::AbilityActive)
	{
		LogMainGunShots(TEXT("ability"));
	}
	EnterPhase(EBehaviourAbilityClipPhase::Done);
	UE_LOG(LogRTSAbilityClip, Display, TEXT("ABILITY_CLIP done: %d frames -> %s"), M_FramesSaved,
	       bSucceeded ? TEXT("OK") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("BehaviourAbilityClipCapture"));
}

#else

bool UBehaviourAbilityClipCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UBehaviourAbilityClipCaptureSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UBehaviourAbilityClipCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UBehaviourAbilityClipCaptureSubsystem::Deinitialize() { Super::Deinitialize(); }
void UBehaviourAbilityClipCaptureSubsystem::Tick(float DeltaTime) {}

TStatId UBehaviourAbilityClipCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBehaviourAbilityClipCaptureSubsystem, STATGROUP_Tickables);
}

#endif
