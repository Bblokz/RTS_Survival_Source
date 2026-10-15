#include "RedLineTourScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "LandscapeProxy.h"
#include "Misc/CommandLine.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSRedLine, Log, All);

namespace RedLineTourPrivate
{
	constexpr int32 EnemyPlayer = 2;
	constexpr int32 HumanPlayer = 1;
	const TCHAR* const PillboxMarker = TEXT("Rus_BXP_RL_TurretPillbox");
	const TCHAR* const TrenchMarker = TEXT("BP_RL_Trench_Straight");
	const TCHAR* const GunBayMarker = TEXT("BP_RL_GunBay");
	const TCHAR* const MGNestMarker = TEXT("BP_RL_MGNest");
	const TCHAR* const B4PitMarker = TEXT("Rus_BXP_B4HowitzerPit");
	const TCHAR* const PillboxWreckMarker = TEXT("BP_Obstacle_RL_PillboxDestroyed");
	constexpr float PillboxWreckTimeoutSeconds = 20.f;
	constexpr float WreckSearchRadius = 1500.f;
	constexpr float WreckWatchSeconds = 15.f;
	constexpr float WreckCaptureIntervalSeconds = 3.f;
	const TCHAR* const AssaultTankClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.Ger_Vehicle_PanzerIV_G_C");
	constexpr int32 ExpectedPillboxCount = 12;
	constexpr int32 ExpectedTrenchCount = 32;
	constexpr int32 CargoSocketsPerTrench = 8;
	// The cover finder may drop a few sockets it cannot project onto the navmesh.
	constexpr float MinimumTrenchCoverFraction = 0.9f;
	constexpr int32 MinimumSoldiersInTrenches = 12;

	constexpr float StartDelaySeconds = 3.f;
	constexpr float SettleSeconds = 75.f;
	constexpr float AssaultTimeoutSeconds = 180.f;
	constexpr float StatusIntervalSeconds = 10.f;
	constexpr float AssaultStatusIntervalSeconds = 4.f;
	constexpr float AssaultCaptureIntervalSeconds = 8.f;

	// The line bows forward to a salient at x = 0; these match the layout script (Tools/RedLine).
	constexpr float SalientLineY = 1800.f;
	constexpr float SalientLineSlope = 1200.f / 19500.f;
	constexpr float LineHalfWidth = 21000.f;
	constexpr float TrenchCoverBand = 900.f;
	constexpr float WesternLaneX = -6750.f;
	constexpr float AssaultStartDistance = 9000.f;
	constexpr float AssaultObjectiveDistance = 3500.f;
	constexpr int32 AssaultTankCount = 4;
	constexpr float AssaultTankSpacing = 500.f;
	constexpr float AssaultFacingYaw = 90.f;
	constexpr float GroundTraceHalfHeight = 20000.f;
	constexpr float SpawnHeightOffset = 60.f;
	constexpr int32 TrenchPictureCount = 3;
	constexpr int32 GunBayPictureCount = 2;
	// Turrets this far beyond their range are not worth a status line.
	constexpr float TurretReportRangeFraction = 1.5f;

	float GetLineY(const float X)
	{
		return SalientLineY + FMath::Abs(X) * SalientLineSlope;
	}

	int32 GetOwningPlayer(const AActor* Actor)
	{
		const URTSComponent* RTSComponent = IsValid(Actor) ? Actor->FindComponentByClass<URTSComponent>() : nullptr;
		return IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : INDEX_NONE;
	}

	bool GetClassContains(const AActor* Actor, const TCHAR* Marker)
	{
		return IsValid(Actor) && Actor->GetClass()->GetName().Contains(Marker);
	}

	bool TraceGround(const UWorld& World, const FVector& Location, FVector& OutGround)
	{
		FHitResult Hit;
		const FVector Up(0.f, 0.f, GroundTraceHalfHeight);
		if (not World.LineTraceSingleByChannel(Hit, Location + Up, Location - Up, ECC_Visibility) ||
			not IsValid(Hit.GetActor()) || not Hit.GetActor()->IsA<ALandscapeProxy>())
		{
			return false;
		}
		OutGround = Hit.Location;
		return true;
	}

	FTransform GetUnscaledTransform(const AActor& Actor)
	{
		FTransform Frame = Actor.GetActorTransform();
		Frame.SetScale3D(FVector::OneVector);
		return Frame;
	}

	TArray<FB4HowitzerCameraView> GetOverviewViews()
	{
		// World space; the enemy attacks from -Y.
		const float LaneLineY = GetLineY(WesternLaneX);
		return {
			{TEXT("salient"), FVector(0.f, -9000.f, 6000.f), FVector(0.f, 2500.f, 0.f), 75.f},
			{TEXT("west_wing"), FVector(-24000.f, -7000.f, 4500.f), FVector(-11000.f, 2800.f, 0.f), 70.f},
			{TEXT("east_wing"), FVector(24000.f, -7000.f, 4500.f), FVector(11000.f, 2800.f, 0.f), 70.f},
			{TEXT("west_lane"), FVector(WesternLaneX, LaneLineY - 5500.f, 1600.f), FVector(WesternLaneX, LaneLineY + 1500.f, 100.f), 60.f},
			{TEXT("hedgehog_belt"), FVector(-12000.f, -6500.f, 700.f), FVector(-9000.f, -2500.f, 50.f), 60.f},
			{TEXT("gun_line_from_rear"), FVector(-3000.f, 9500.f, 2500.f), FVector(0.f, 3000.f, 0.f), 70.f},
		};
	}

	// In the actor's own space: emplacements face +X.
	const FB4HowitzerCameraView PillboxView = {TEXT("front"), FVector(950.f, -650.f, 520.f), FVector(0.f, 0.f, 170.f), 55.f};
	const FB4HowitzerCameraView TrenchView = {TEXT("along"), FVector(-380.f, -650.f, 300.f), FVector(60.f, 250.f, 80.f), 65.f};
	const FB4HowitzerCameraView GunBayView = {TEXT("rear"), FVector(-1000.f, -700.f, 700.f), FVector(0.f, 0.f, 80.f), 55.f};
	const FB4HowitzerCameraView GunBayCornerView = {TEXT("corner"), FVector(900.f, 900.f, 650.f), FVector(325.f, 350.f, 80.f), 55.f};
	const FB4HowitzerCameraView MGNestView = {TEXT("close"), FVector(-500.f, -650.f, 520.f), FVector(0.f, 0.f, 60.f), 55.f};
	const FB4HowitzerCameraView B4PitView = {TEXT("side"), FVector(-1400.f, -1000.f, 1100.f), FVector(0.f, 0.f, 100.f), 60.f};
	const FB4HowitzerCameraView TankChaseView = {TEXT("chase"), FVector(-1100.f, -500.f, 600.f), FVector(1200.f, 0.f, 100.f), 65.f};

	void KeepAnimating(USkeletalMeshComponent* Mesh)
	{
		if (not IsValid(Mesh))
		{
			return;
		}
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

void FRedLineTourScenario::Start()
{
	M_Phase = ERedLineTourPhase::WaitingForStart;
	M_PhaseStartSeconds = 0.f;
	FParse::Value(FCommandLine::Get(), TEXT("RedLineCapture="), M_CaptureDirectory);
	bM_RunAssault = FParse::Param(FCommandLine::Get(), TEXT("RedLineAssault"));
	UE_LOG(LogRTSRedLine, Display, TEXT("RTS_REDLINE started assault=%d"), bM_RunAssault ? 1 : 0);
}

void FRedLineTourScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == ERedLineTourPhase::Inactive || not IsValid(World))
	{
		return;
	}
	switch (M_Phase)
	{
	case ERedLineTourPhase::WaitingForStart:
		TickWaitingForStart(*World);
		break;
	case ERedLineTourPhase::Settling:
		TickSettling(*World, CoverSubsystem);
		break;
	case ERedLineTourPhase::Assault:
		TickAssault(*World);
		break;
	case ERedLineTourPhase::WaitingForPillboxWreck:
		TickWaitingForPillboxWreck(*World);
		break;
	default:
		break;
	}
}

void FRedLineTourScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSRedLine, Display, TEXT("RTS_REDLINE check=%s %s %s"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), Description, *Details);
}

void FRedLineTourScenario::EnterPhase(const UWorld& World, const ERedLineTourPhase NewPhase, const float TimeoutSeconds)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = World.GetTimeSeconds();
	M_PhaseDeadlineSeconds = M_PhaseStartSeconds + TimeoutSeconds;
	M_NextStatusSeconds = M_PhaseStartSeconds;
	M_NextCaptureSeconds = M_PhaseStartSeconds;
}

void FRedLineTourScenario::Finish(const UWorld& World)
{
	M_Phase = ERedLineTourPhase::Inactive;
	UE_LOG(LogRTSRedLine, Display, TEXT("RTS_REDLINE RESULT %s failed_checks=%d map=%s"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount, *World.GetName());
	FPlatformMisc::RequestExit(false, TEXT("RedLineTour"));
}

void FRedLineTourScenario::TickWaitingForStart(UWorld& World)
{
	// The map boots paused behind the start-game widget.
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_PhaseStartSeconds <= 0.f)
	{
		M_PhaseStartSeconds = World.GetTimeSeconds();
	}
	if (World.GetTimeSeconds() - M_PhaseStartSeconds < RedLineTourPrivate::StartDelaySeconds)
	{
		return;
	}
	if (not M_CaptureDirectory.IsEmpty())
	{
		M_CaptureRig.Init(World, M_CaptureDirectory);
	}
	InventoryLine(World);
	CollectGarrisonSoldiers(World);
	EnterPhase(World, ERedLineTourPhase::Settling, RedLineTourPrivate::SettleSeconds);
}

void FRedLineTourScenario::InventoryLine(UWorld& World)
{
	using namespace RedLineTourPrivate;
	int32 PillboxCount = 0;
	int32 EnemyPillboxCount = 0;
	int32 EnemySquadCount = 0;
	int32 NonEnemySquadsInLine = 0;
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		AActor* Actor = *It;
		if (GetClassContains(Actor, PillboxMarker))
		{
			++PillboxCount;
			EnemyPillboxCount += GetOwningPlayer(Actor) == EnemyPlayer ? 1 : 0;
		}
		else if (GetClassContains(Actor, TrenchMarker))
		{
			M_TrenchActors.Add(Actor);
		}
		else if (GetClassContains(Actor, GunBayMarker))
		{
			M_GunBayActors.Add(Actor);
		}
		else if (GetClassContains(Actor, B4PitMarker))
		{
			M_B4Pit = Actor;
		}
		const ASquadController* Squad = Cast<ASquadController>(Actor);
		if (IsValid(Squad) && FMath::Abs(Squad->GetActorLocation().X) < LineHalfWidth)
		{
			const bool bIsEnemy = GetOwningPlayer(Squad) == EnemyPlayer;
			EnemySquadCount += bIsEnemy ? 1 : 0;
			NonEnemySquadsInLine += bIsEnemy ? 0 : 1;
		}
	}
	Check(PillboxCount >= ExpectedPillboxCount, TEXT("pillboxes_placed"), FString::Printf(TEXT("count=%d"), PillboxCount));
	Check(EnemyPillboxCount == PillboxCount, TEXT("pillboxes_owned_by_enemy"),
	      FString::Printf(TEXT("enemy=%d of %d"), EnemyPillboxCount, PillboxCount));
	Check(M_TrenchActors.Num() == ExpectedTrenchCount, TEXT("trenches_placed"),
	      FString::Printf(TEXT("count=%d"), M_TrenchActors.Num()));
	Check(EnemySquadCount > 0, TEXT("garrison_squads_owned_by_enemy"),
	      FString::Printf(TEXT("enemy=%d other=%d"), EnemySquadCount, NonEnemySquadsInLine));
	int32 MountedTurrets = 0;
	WatchPillboxTurrets(World, MountedTurrets);
	Check(MountedTurrets == PillboxCount, TEXT("pillbox_turrets_mounted"),
	      FString::Printf(TEXT("mounted=%d of %d"), MountedTurrets, PillboxCount));
}

void FRedLineTourScenario::WatchPillboxTurrets(UWorld& World, int32& OutMountedTurrets)
{
	OutMountedTurrets = 0;
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		AActor* Pillbox = *It;
		if (not RedLineTourPrivate::GetClassContains(Pillbox, RedLineTourPrivate::PillboxMarker))
		{
			continue;
		}
		TArray<UChildActorComponent*> ChildActorComponents;
		Pillbox->GetComponents<UChildActorComponent>(ChildActorComponents);
		ACPPTurretsMaster* Turret = nullptr;
		for (const UChildActorComponent* ChildActorComponent : ChildActorComponents)
		{
			Turret = Turret ? Turret : Cast<ACPPTurretsMaster>(ChildActorComponent->GetChildActor());
		}
		UE_LOG(LogRTSRedLine, Display, TEXT("RTS_REDLINE pillbox %s turret=%s weapons=%d"), *Pillbox->GetClass()->GetName(),
		       IsValid(Turret) ? *Turret->GetClass()->GetName() : TEXT("none"), IsValid(Turret) ? Turret->GetWeaponCount() : 0);
		if (not IsValid(Turret))
		{
			continue;
		}
		++OutMountedTurrets;
		FRedLineTurretWatch Watch;
		Watch.Pillbox = Pillbox;
		Watch.Turret = Turret;
		M_TurretWatches.Add(Watch);
	}
}

void FRedLineTourScenario::CollectGarrisonSoldiers(UWorld& World)
{
	M_GarrisonSoldiers.Reset();
	for (TActorIterator<ASquadUnit> It(&World); It; ++It)
	{
		ASquadUnit* Soldier = *It;
		if (IsValid(Soldier) && Soldier->GetOwningPlayer() == RedLineTourPrivate::EnemyPlayer)
		{
			M_GarrisonSoldiers.Add(Soldier);
		}
	}
}

void FRedLineTourScenario::KeepGarrisonUpdating() const
{
	for (const TWeakObjectPtr<ASquadUnit>& WeakSoldier : M_GarrisonSoldiers)
	{
		const ASquadUnit* Soldier = WeakSoldier.Get();
		if (not IsValid(Soldier))
		{
			continue;
		}
		// The optimizer keeps resetting the interval for unseen soldiers, so it is set every frame.
		UCharacterMovementComponent* Movement = Soldier->GetCharacterMovement();
		if (IsValid(Movement))
		{
			Movement->SetComponentTickInterval(0.f);
		}
		RedLineTourPrivate::KeepAnimating(Soldier->GetMesh());
	}
}

void FRedLineTourScenario::TickSettling(UWorld& World, const URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	using namespace RedLineTourPrivate;
	KeepGarrisonUpdating();
	if (World.GetTimeSeconds() >= M_NextStatusSeconds)
	{
		// Squads spawn their soldiers over the first seconds of play.
		CollectGarrisonSoldiers(World);
		M_NextStatusSeconds = World.GetTimeSeconds() + StatusIntervalSeconds;
		UE_LOG(LogRTSRedLine, Display, TEXT("RTS_REDLINE settling t=%.1f garrison_soldiers=%d"), World.GetTimeSeconds(),
		       M_GarrisonSoldiers.Num());
	}
	if (World.GetTimeSeconds() < M_PhaseDeadlineSeconds)
	{
		return;
	}
	CheckTrenchCover(CoverSubsystem);
	CheckGarrisonInTrenches();
	CaptureTour(World);
	if (not bM_RunAssault)
	{
		KillOnePillbox(World);
		return;
	}
	SpawnAssault(World);
	EnterPhase(World, ERedLineTourPhase::Assault, AssaultTimeoutSeconds);
}

void FRedLineTourScenario::CheckTrenchCover(const URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	using namespace RedLineTourPrivate;
	int32 TrenchCoverInLine = 0;
	for (const FRTSCoverPoint& CoverPoint : CoverSubsystem.GetCoverPointsView())
	{
		const bool bIsInLine = FMath::Abs(CoverPoint.Location.X) < LineHalfWidth &&
			FMath::Abs(CoverPoint.Location.Y - GetLineY(CoverPoint.Location.X)) < TrenchCoverBand;
		TrenchCoverInLine += CoverPoint.CoverType == ERTSCoverType::TrenchStandUp && bIsInLine ? 1 : 0;
	}
	const int32 ExpectedCover = FMath::FloorToInt(ExpectedTrenchCount * CargoSocketsPerTrench * MinimumTrenchCoverFraction);
	Check(TrenchCoverInLine >= ExpectedCover, TEXT("trench_cover_registered"),
	      FString::Printf(TEXT("points=%d expected_at_least=%d"), TrenchCoverInLine, ExpectedCover));
}

void FRedLineTourScenario::CheckGarrisonInTrenches()
{
	int32 InTrenches = 0;
	int32 InOtherCover = 0;
	int32 Living = 0;
	for (const TWeakObjectPtr<ASquadUnit>& WeakSoldier : M_GarrisonSoldiers)
	{
		ASquadUnit* Soldier = WeakSoldier.Get();
		if (not IsValid(Soldier) || not Soldier->IsUnitAlive())
		{
			continue;
		}
		++Living;
		const FSquadUnitCoverRuntimeState& Cover = Soldier->GetCoverRuntimeState();
		const bool bIsInCover = Cover.State == ESquadUnitCoverState::Protected || Cover.State == ESquadUnitCoverState::Exposed;
		if (not bIsInCover)
		{
			continue;
		}
		const bool bIsTrench = Cover.AssignedCoverPoint.CoverType == ERTSCoverType::TrenchStandUp;
		InTrenches += bIsTrench ? 1 : 0;
		InOtherCover += bIsTrench ? 0 : 1;
		if (not bIsTrench)
		{
			const FVector& Location = Cover.AssignedCoverPoint.Location;
			const ASquadController* Squad = Soldier->GetSquadControllerChecked();
			UE_LOG(LogRTSRedLine, Display, TEXT("RTS_REDLINE other_cover type=%s at=%s from_line=%.0f squad=%s squad_at=%s"),
			       *UEnum::GetValueAsString(Cover.AssignedCoverPoint.CoverType), *Location.ToCompactString(),
			       Location.Y - RedLineTourPrivate::GetLineY(Location.X),
			       IsValid(Squad) ? *Squad->GetName() : TEXT("none"),
			       IsValid(Squad) ? *Squad->GetActorLocation().ToCompactString() : TEXT("-"));
		}
	}
	Check(InTrenches >= RedLineTourPrivate::MinimumSoldiersInTrenches, TEXT("garrison_settled_in_trenches"),
	      FString::Printf(TEXT("in_trenches=%d other_cover=%d living=%d"), InTrenches, InOtherCover, Living));
}

void FRedLineTourScenario::SpawnAssault(UWorld& World)
{
	using namespace RedLineTourPrivate;
	UClass* TankClass = LoadClass<APawn>(nullptr, AssaultTankClassPath);
	Check(IsValid(TankClass), TEXT("assault_tank_class_loaded"));
	if (not IsValid(TankClass))
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float LaneLineY = GetLineY(WesternLaneX);
	for (int32 TankIndex = 0; TankIndex < AssaultTankCount; ++TankIndex)
	{
		const float ColumnOffset = (TankIndex - (AssaultTankCount - 1) * 0.5f) * AssaultTankSpacing;
		FVector SpawnLocation(WesternLaneX + ColumnOffset, LaneLineY - AssaultStartDistance, 0.f);
		TraceGround(World, SpawnLocation, SpawnLocation);
		SpawnLocation.Z += SpawnHeightOffset;
		APawn* Tank = World.SpawnActor<APawn>(TankClass, FTransform(FRotator(0.f, AssaultFacingYaw, 0.f), SpawnLocation),
		                                      SpawnParameters);
		if (not IsValid(Tank))
		{
			continue;
		}
		if (not Tank->GetController())
		{
			Tank->SpawnDefaultController();
		}
		URTSComponent* RTSComponent = Tank->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() != HumanPlayer)
		{
			RTSComponent->SetOwningPlayerRuntime(HumanPlayer);
		}
		M_AssaultTanks.Add(Tank);
		const FVector Objective(WesternLaneX + ColumnOffset, LaneLineY + AssaultObjectiveDistance, SpawnLocation.Z);
		if (ICommands* Commands = Cast<ICommands>(Tank))
		{
			Commands->MoveToLocation(Objective, true, FRotator(0.f, AssaultFacingYaw, 0.f));
		}
	}
	Check(M_AssaultTanks.Num() == AssaultTankCount, TEXT("assault_tanks_spawned"),
	      FString::Printf(TEXT("count=%d"), M_AssaultTanks.Num()));
}

void FRedLineTourScenario::CountTurretShots()
{
	for (FRedLineTurretWatch& Watch : M_TurretWatches)
	{
		ACPPTurretsMaster* Turret = Watch.Turret.Get();
		if (not IsValid(Turret))
		{
			continue;
		}
		const TArray<UWeaponState*> Weapons = Turret->GetWeapons();
		Watch.LastMagazineCounts.SetNum(Weapons.Num());
		for (int32 WeaponIndex = 0; WeaponIndex < Weapons.Num(); ++WeaponIndex)
		{
			const UWeaponState* Weapon = Weapons[WeaponIndex];
			const int32 Magazine = IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : 0;
			const int32 Fired = Watch.LastMagazineCounts[WeaponIndex] - Magazine;
			Watch.ShotCount += Fired > 0 ? Fired : 0;
			Watch.LastMagazineCounts[WeaponIndex] = Magazine;
		}
	}
}

int32 FRedLineTourScenario::GetTotalTurretShots(int32& OutTurretsThatFired) const
{
	int32 TotalShots = 0;
	OutTurretsThatFired = 0;
	for (const FRedLineTurretWatch& Watch : M_TurretWatches)
	{
		TotalShots += Watch.ShotCount;
		OutTurretsThatFired += Watch.ShotCount > 0 ? 1 : 0;
	}
	return TotalShots;
}

int32 FRedLineTourScenario::GetLivingAssaultTanks() const
{
	int32 Living = 0;
	for (const TWeakObjectPtr<APawn>& WeakTank : M_AssaultTanks)
	{
		const UHealthComponent* Health = WeakTank.IsValid() ? WeakTank->FindComponentByClass<UHealthComponent>() : nullptr;
		Living += IsValid(Health) && Health->GetCurrentHealth() > 0.f ? 1 : 0;
	}
	return Living;
}

void FRedLineTourScenario::TickAssault(UWorld& World)
{
	using namespace RedLineTourPrivate;
	KeepGarrisonUpdating();
	if (M_TurretWatches.Num() > 0 && M_TurretWatches[0].LastMagazineCounts.IsEmpty())
	{
		// The first count only takes the starting magazines.
		CountTurretShots();
		for (FRedLineTurretWatch& Watch : M_TurretWatches)
		{
			Watch.ShotCount = 0;
		}
	}
	CountTurretShots();
	ReportAssaultStatus(World);
	CaptureAssault(World);
	const bool bIsAssaultOver = GetLivingAssaultTanks() == 0 || World.GetTimeSeconds() >= M_PhaseDeadlineSeconds;
	if (not bIsAssaultOver)
	{
		return;
	}
	int32 TurretsThatFired = 0;
	const int32 TotalShots = GetTotalTurretShots(TurretsThatFired);
	Check(TotalShots > 0, TEXT("pillboxes_fire_on_assault"),
	      FString::Printf(TEXT("shots=%d turrets_that_fired=%d of %d"), TotalShots, TurretsThatFired, M_TurretWatches.Num()));
	UE_LOG(LogRTSRedLine, Display, TEXT("RTS_REDLINE assault_over t=%.1f tanks_left=%d of %d"),
	       World.GetTimeSeconds() - M_PhaseStartSeconds, GetLivingAssaultTanks(), M_AssaultTanks.Num());
	KillOnePillbox(World);
}

void FRedLineTourScenario::KillOnePillbox(UWorld& World)
{
	AHPActorObjectsMaster* Pillbox = nullptr;
	for (const FRedLineTurretWatch& Watch : M_TurretWatches)
	{
		Pillbox = Pillbox ? Pillbox : Cast<AHPActorObjectsMaster>(Watch.Pillbox.Get());
	}
	Check(IsValid(Pillbox), TEXT("pillbox_alive_before_kill"));
	if (not IsValid(Pillbox))
	{
		Finish(World);
		return;
	}
	M_PillboxDeathLocation = Pillbox->GetActorLocation();
	Pillbox->TakeFatalDamage();
	EnterPhase(World, ERedLineTourPhase::WaitingForPillboxWreck, RedLineTourPrivate::PillboxWreckTimeoutSeconds);
}

bool FRedLineTourScenario::GetHasPillboxWreck(UWorld& World) const
{
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		const AActor* Actor = *It;
		if (RedLineTourPrivate::GetClassContains(Actor, RedLineTourPrivate::PillboxWreckMarker) &&
			FVector::Dist2D(Actor->GetActorLocation(), M_PillboxDeathLocation) < RedLineTourPrivate::WreckSearchRadius)
		{
			return true;
		}
	}
	return false;
}

void FRedLineTourScenario::TickWaitingForPillboxWreck(UWorld& World)
{
	using namespace RedLineTourPrivate;
	const bool bHasWreck = GetHasPillboxWreck(World);
	if (not bHasWreck && World.GetTimeSeconds() < M_PhaseDeadlineSeconds)
	{
		return;
	}
	if (M_WreckFoundSeconds < 0.f)
	{
		Check(bHasWreck, TEXT("destroyed_pillbox_leaves_wreck"));
		M_WreckFoundSeconds = World.GetTimeSeconds();
	}
	// A few pictures while the death effects play out and after they are gone.
	if (M_CaptureRig.GetIsEnabled() && World.GetTimeSeconds() >= M_NextCaptureSeconds)
	{
		M_NextCaptureSeconds = World.GetTimeSeconds() + WreckCaptureIntervalSeconds;
		const FTransform Frame(FRotator(0.f, -90.f, 0.f), M_PillboxDeathLocation);
		M_CaptureRig.CaptureViews(World, Frame, {PillboxView}, FString::Printf(
			                          TEXT("pillbox_wreck_t%04.1f"), World.GetTimeSeconds() - M_WreckFoundSeconds));
	}
	const float WreckAgeSeconds = World.GetTimeSeconds() - M_WreckFoundSeconds;
	if (bHasWreck && WreckAgeSeconds < WreckWatchSeconds)
	{
		return;
	}
	// The wreck is an obstacle in the line; it must not expire like a vehicle wreck.
	Check(bHasWreck, TEXT("pillbox_wreck_persists"), FString::Printf(TEXT("seconds=%.1f"), WreckAgeSeconds));
	Finish(World);
}

void FRedLineTourScenario::ReportAssaultStatus(const UWorld& World)
{
	if (World.GetTimeSeconds() < M_NextStatusSeconds)
	{
		return;
	}
	M_NextStatusSeconds = World.GetTimeSeconds() + RedLineTourPrivate::AssaultStatusIntervalSeconds;
	FString Tanks;
	for (const TWeakObjectPtr<APawn>& WeakTank : M_AssaultTanks)
	{
		const UHealthComponent* Health = WeakTank.IsValid() ? WeakTank->FindComponentByClass<UHealthComponent>() : nullptr;
		Tanks += IsValid(Health)
			         ? FString::Printf(TEXT(" [%s hp=%.0f]"), *WeakTank->GetActorLocation().ToCompactString(), Health->GetCurrentHealth())
			         : TEXT(" [dead]");
	}
	int32 TurretsThatFired = 0;
	const int32 TotalShots = GetTotalTurretShots(TurretsThatFired);
	UE_LOG(LogRTSRedLine, Display, TEXT("RTS_REDLINE assault t=%.1f pillbox_shots=%d turrets_firing=%d tanks:%s"),
	       World.GetTimeSeconds() - M_PhaseStartSeconds, TotalShots, TurretsThatFired, *Tanks);
	ReportTurretStates();
}

void FRedLineTourScenario::ReportTurretStates() const
{
	for (const FRedLineTurretWatch& Watch : M_TurretWatches)
	{
		const ACPPTurretsMaster* Turret = Watch.Turret.Get();
		if (not IsValid(Turret))
		{
			continue;
		}
		float ClosestTankDistance = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<APawn>& WeakTank : M_AssaultTanks)
		{
			if (WeakTank.IsValid())
			{
				ClosestTankDistance = FMath::Min(ClosestTankDistance,
				                                 FVector::Dist(WeakTank->GetActorLocation(), Turret->GetActorLocation()));
			}
		}
		if (ClosestTankDistance > Turret->GetMaxWeaponRange() * RedLineTourPrivate::TurretReportRangeFraction)
		{
			continue;
		}
		const AActor* Target = Turret->GetCurrentTargetActor();
		UE_LOG(LogRTSRedLine, Display,
		       TEXT("RTS_REDLINE turret %s closest_tank=%.0f range=%.0f target=%s rotated=%d shots=%d"),
		       *Turret->GetClass()->GetName(), ClosestTankDistance, Turret->GetMaxWeaponRange(),
		       IsValid(Target) ? *Target->GetName() : TEXT("none"), Turret->GetIsRotatedToEngage() ? 1 : 0, Watch.ShotCount);
	}
}

void FRedLineTourScenario::CaptureTour(UWorld& World)
{
	using namespace RedLineTourPrivate;
	if (not M_CaptureRig.GetIsEnabled())
	{
		return;
	}
	RevealLine(World);
	M_CaptureRig.CaptureViews(World, FTransform::Identity, GetOverviewViews(), TEXT("overview"));
	for (int32 WatchIndex = 0; WatchIndex < M_TurretWatches.Num(); ++WatchIndex)
	{
		const AActor* Pillbox = M_TurretWatches[WatchIndex].Pillbox.Get();
		if (IsValid(Pillbox))
		{
			M_CaptureRig.CaptureViews(World, GetUnscaledTransform(*Pillbox), {PillboxView},
			                          FString::Printf(TEXT("pillbox%02d_%s"), WatchIndex, *Pillbox->GetClass()->GetName()));
		}
	}
	for (int32 TrenchIndex = 0; TrenchIndex < FMath::Min(TrenchPictureCount, M_TrenchActors.Num()); ++TrenchIndex)
	{
		// Every few trenches along the line, so the pictures show different garrisons.
		const int32 Spread = M_TrenchActors.Num() / TrenchPictureCount;
		const AActor* Trench = M_TrenchActors[TrenchIndex * Spread].Get();
		if (IsValid(Trench))
		{
			M_CaptureRig.CaptureViews(World, GetUnscaledTransform(*Trench), {TrenchView},
			                          FString::Printf(TEXT("trench%02d"), TrenchIndex * Spread));
		}
	}
	for (int32 BayIndex = 0; BayIndex < FMath::Min(GunBayPictureCount, M_GunBayActors.Num()); ++BayIndex)
	{
		const AActor* GunBay = M_GunBayActors[BayIndex].Get();
		if (IsValid(GunBay))
		{
			M_CaptureRig.CaptureViews(World, GetUnscaledTransform(*GunBay), {GunBayView, GunBayCornerView},
			                          FString::Printf(TEXT("gunbay%02d"), BayIndex));
		}
	}
	if (M_B4Pit.IsValid())
	{
		M_CaptureRig.CaptureViews(World, GetUnscaledTransform(*M_B4Pit.Get()), {B4PitView}, TEXT("b4pit"));
	}
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		if (GetClassContains(*It, MGNestMarker))
		{
			M_CaptureRig.CaptureViews(World, GetUnscaledTransform(**It), {MGNestView}, TEXT("mgnest"));
			break;
		}
	}
}

void FRedLineTourScenario::CaptureAssault(UWorld& World)
{
	using namespace RedLineTourPrivate;
	if (not M_CaptureRig.GetIsEnabled() || World.GetTimeSeconds() < M_NextCaptureSeconds)
	{
		return;
	}
	M_NextCaptureSeconds = World.GetTimeSeconds() + AssaultCaptureIntervalSeconds;
	RevealLine(World);
	const FString Label = FString::Printf(TEXT("assault_t%05.1f"), World.GetTimeSeconds() - M_PhaseStartSeconds);
	M_CaptureRig.CaptureViews(World, FTransform::Identity, {GetOverviewViews()[3]}, Label);
	for (const TWeakObjectPtr<APawn>& WeakTank : M_AssaultTanks)
	{
		if (WeakTank.IsValid())
		{
			M_CaptureRig.CaptureViews(World, GetUnscaledTransform(*WeakTank.Get()), {TankChaseView}, Label);
			return;
		}
	}
}

void FRedLineTourScenario::RevealLine(UWorld& World) const
{
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		AActor* Actor = *It;
		const bool bIsUnitOrTurret = Actor->FindComponentByClass<URTSComponent>() != nullptr ||
			Actor->IsA<ACPPTurretsMaster>() || Actor->IsA<ASquadUnit>();
		if (bIsUnitOrTurret)
		{
			Actor->SetActorHiddenInGame(false);
		}
	}
}

#endif
