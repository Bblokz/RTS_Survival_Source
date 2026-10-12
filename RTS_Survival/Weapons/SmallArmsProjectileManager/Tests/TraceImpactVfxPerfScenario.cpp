#include "TraceImpactVfxPerfScenario.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RHIStats.h"
#include "UnrealClient.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/TraceImpactVfx/TraceImpactVfxBatcher.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/TraceImpactVfx/TraceImpactVfxPerfCounters.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTraceImpactVfx, Log, All);

namespace TraceImpactVfxPerfScenarioPrivate
{
	// Squads load their soldiers asynchronously; long enough for all of them to stand on the map.
	constexpr int32 SquadLoadFrames = 400;
	constexpr int32 FramesBetweenCounts = 30;
	constexpr int32 FramesBetweenHeals = 20;
	constexpr int32 StatDumpFrameMargin = 20;
	constexpr double MillisecondsPerSecond = 1000.0;
	constexpr double MicrosecondsPerSecond = 1000000.0;
	constexpr float SquadSpacing = 450.0f;
	constexpr int32 SquadsPerRow = 10;
	constexpr float RowDepth = 500.0f;
	constexpr float NavProjectionExtent = 1500.0f;
	constexpr float SpawnHeightOffset = 60.0f;
	constexpr float TankDistanceBehindLine = 900.0f;
	constexpr float TankSpacing = 1100.0f;
	constexpr float TankSpawnHeightOffset = 150.0f;
	constexpr float OverviewCameraHeight = 3500.0f;
	// The screenshot camera stands this far in front of the Soviet line and looks at it, slightly down.
	constexpr float CloseUpDistance = 600.0f;
	constexpr float CloseUpHeight = 350.0f;
	constexpr float CloseUpPitch = -25.0f;
	constexpr int32 FramesBetweenScreenshots = 300;
	// Showcase: a row of impact types across the picture, left to right in the order of ETraceImpactVfxType.
	constexpr float ShowcaseDistanceFromCamera = 650.0f;
	constexpr float ShowcaseSpacing = 180.0f;
	constexpr int32 ShowcaseFramesBetweenImpacts = 30;
	constexpr int32 ShowcaseFramesBetweenScreenshots = 90;
	// A picture this many frames after the impacts shows them well under way.
	constexpr int32 ShowcaseScreenshotDelayFrames = 8;
	constexpr float ShowcaseGroundTraceHalfHeight = 2000.0f;
	// Towards the camera, so the reference effect is in front of ours and nothing stands in the way.
	constexpr float ShowcaseReferenceDistance = -250.0f;
	const TCHAR* ShowcaseStoneReferencePath =
		TEXT("/Game/RTS_Survival/ImpactSfxVfx/1SmallCalibre/Stone/NS_SmallCaliber_StoneImpact.NS_SmallCaliber_StoneImpact");
	const TCHAR* ShowcaseSandReferencePath =
		TEXT("/Game/RTS_Survival/ImpactSfxVfx/0Bulllet/SandImpact/NS_SmallArms_SandImpact.NS_SmallArms_SandImpact");
	// A soldier with this much more health survives the whole measured fight between two heals.
	constexpr float MaxHealthMultiplier = 200.0f;
	constexpr uint8 GermanOwner = 1;
	constexpr uint8 SovietOwner = 2;

	// Squads whose soldiers carry trace weapons: rifles, SMGs and MGs.
	const TArray<const TCHAR*> GermanSquadClassPaths = {
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Squad_Ger_Mauser.Squad_Ger_Mauser_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Squad_Ger_MG34.Squad_Ger_MG34_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Squad_Ger_STG.Squad_Ger_STG_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/LMGSquad/Squad_Ger_LMG.Squad_Ger_LMG_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers.Squad_Ger_Scavengers_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/T2/PanzerGrenadiers/Squad_Ger_PanzerGrenadiers.Squad_Ger_PanzerGrenadiers_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/JagerTrupp/Squad_Ger_JagerTrupp.Squad_Ger_JagerTrupp_C")
	};
	const TArray<const TCHAR*> SovietSquadClassPaths = {
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_SVT.Rus_Squad_SVT_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Fedrov.Rus_Squad_Fedrov_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_LargeMixBasic.Rus_Squad_LargeMixBasic_C"),
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/T2/Rus_T2Squad_RedHammer.Rus_T2Squad_RedHammer_C")
	};

	float GetPercentile(const TArray<float>& SortedValues, const float Percentile)
	{
		if (SortedValues.IsEmpty())
		{
			return 0.0f;
		}
		const int32 Index = FMath::Clamp(
			FMath::FloorToInt(Percentile * (SortedValues.Num() - 1)),
			0,
			SortedValues.Num() - 1);
		return SortedValues[Index];
	}

	// Calls Visit for every living soldier of the squads.
	template <typename VisitType>
	void ForEachSoldier(const TArray<TWeakObjectPtr<ASquadController>>& Squads, VisitType&& Visit)
	{
		for (const TWeakObjectPtr<ASquadController>& WeakSquad : Squads)
		{
			ASquadController* Squad = WeakSquad.Get();
			if (not IsValid(Squad))
			{
				continue;
			}
			for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
			{
				if (IsValid(Soldier) && Soldier->IsUnitAlive())
				{
					Visit(*Soldier);
				}
			}
		}
	}

	// Alive/spawned particles of every emitter of the trace impact system, empty when it is not in use.
	FString DescribeTraceImpactEmitters(UWorld& World)
	{
		for (TActorIterator<ASmallArmsProjectileManager> ManagerIterator(&World); ManagerIterator; ++ManagerIterator)
		{
			const UTraceImpactVfxBatcher* ImpactVfxBatcher = ManagerIterator->GetTraceImpactVfxBatcher();
			return IsValid(ImpactVfxBatcher) && ImpactVfxBatcher->GetIsReadyToDraw()
				? ImpactVfxBatcher->DescribeEmitterParticleCounts()
				: FString();
		}
		return FString();
	}

	bool GetIsDrawingCentralised(UWorld& World)
	{
		for (TActorIterator<ASmallArmsProjectileManager> ManagerIterator(&World); ManagerIterator; ++ManagerIterator)
		{
			const UTraceImpactVfxBatcher* ImpactVfxBatcher = ManagerIterator->GetTraceImpactVfxBatcher();
			return IsValid(ImpactVfxBatcher) && ImpactVfxBatcher->GetIsReadyToDraw();
		}
		return false;
	}
}

void FTraceImpactVfxPerfScenario::Start()
{
	M_Phase = ETraceImpactVfxPerfPhase::WaitingForCoverScan;
	bM_DumpStats = FParse::Param(FCommandLine::Get(), TEXT("TraceImpactVfxStatDump"));
	bM_TakeScreenshots = FParse::Param(FCommandLine::Get(), TEXT("TraceImpactVfxScreenshots"));
	bM_ShowcaseImpactTypes = FParse::Param(FCommandLine::Get(), TEXT("TraceImpactVfxShowcase"));
	FParse::Value(FCommandLine::Get(), TEXT("TraceImpactVfxSquads="), M_SquadsPerSide);
	FParse::Value(FCommandLine::Get(), TEXT("TraceImpactVfxFrames="), M_FightFrames);
	FParse::Value(FCommandLine::Get(), TEXT("TraceImpactVfxLineDistance="), M_LineDistance);
	FParse::Value(FCommandLine::Get(), TEXT("TraceImpactVfxTanks="), M_TanksPerSide);
	UE_LOG(LogRTSTraceImpactVfx, Display, TEXT("RTS_TRACE_IMPACT_VFX scheduled squads_per_side=%d"), M_SquadsPerSide);
}

void FTraceImpactVfxPerfScenario::SpawnSquadLine(
	UWorld& World,
	const TArray<const TCHAR*>& SquadClassPaths,
	const float LineOffset,
	TArray<TWeakObjectPtr<ASquadController>>& OutSquads) const
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(&World);
	// Away from the other side for every further row.
	const float RowDirection = LineOffset < 0.0f ? -1.0f : 1.0f;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 SquadIndex = 0; SquadIndex < M_SquadsPerSide; ++SquadIndex)
	{
		UClass* SquadClass = LoadClass<ASquadController>(
			nullptr, SquadClassPaths[SquadIndex % SquadClassPaths.Num()]);
		if (SquadClass == nullptr)
		{
			UE_LOG(LogRTSTraceImpactVfx, Warning, TEXT("RTS_TRACE_IMPACT_VFX could not load squad class %s"),
			       SquadClassPaths[SquadIndex % SquadClassPaths.Num()]);
			continue;
		}
		const int32 Row = SquadIndex / SquadsPerRow;
		const int32 Column = SquadIndex % SquadsPerRow;
		FVector SpawnLocation = M_FightCenter + FVector(
			LineOffset + RowDirection * Row * RowDepth,
			(Column - (SquadsPerRow - 1) * 0.5f) * SquadSpacing,
			0.0f);
		FNavLocation NavLocation;
		const bool bIsOnNavMesh = IsValid(NavigationSystem) && NavigationSystem->ProjectPointToNavigation(
			SpawnLocation, NavLocation, FVector(NavProjectionExtent));
		if (bIsOnNavMesh)
		{
			SpawnLocation = NavLocation.Location;
		}
		SpawnLocation.Z += SpawnHeightOffset;
		ASquadController* Squad = World.SpawnActor<ASquadController>(
			SquadClass, FTransform(SpawnLocation), SpawnParameters);
		if (not IsValid(Squad))
		{
			continue;
		}
		OutSquads.Add(Squad);
		UE_LOG(
			LogRTSTraceImpactVfx,
			Display,
			TEXT("RTS_TRACE_IMPACT_VFX spawned squad=%s owner=%d on_nav=%d at=%s"),
			*Squad->GetName(),
			IsValid(Squad->GetRTSComponent()) ? Squad->GetRTSComponent()->GetOwningPlayer() : -1,
			bIsOnNavMesh ? 1 : 0,
			*SpawnLocation.ToCompactString());
	}
}

void FTraceImpactVfxPerfScenario::SpawnTanksBehindLine(
	UWorld& World,
	const uint8 OwningPlayer,
	const float LineOffset) const
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	// The tanks the map already has for this side are the templates; one of each class first.
	TArray<ATrackedTankMaster*> Templates;
	for (TActorIterator<ATrackedTankMaster> TankIterator(&World); TankIterator; ++TankIterator)
	{
		ATrackedTankMaster* Tank = *TankIterator;
		const bool bIsTemplate = IsValid(Tank) && IsValid(Tank->GetRTSComponent())
			&& Tank->GetRTSComponent()->GetOwningPlayer() == OwningPlayer
			&& not Templates.ContainsByPredicate([Tank](const ATrackedTankMaster* Known)
			{
				return Known->GetClass() == Tank->GetClass();
			});
		if (bIsTemplate)
		{
			Templates.Add(Tank);
		}
	}
	const float AwayFromFight = LineOffset < 0.0f ? -1.0f : 1.0f;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 TankIndex = 0; TankIndex < M_TanksPerSide && not Templates.IsEmpty(); ++TankIndex)
	{
		const ATrackedTankMaster* Template = Templates[TankIndex % Templates.Num()];
		const FVector SpawnLocation = M_FightCenter + FVector(
			LineOffset + AwayFromFight * TankDistanceBehindLine,
			(TankIndex - (M_TanksPerSide - 1) * 0.5f) * TankSpacing,
			TankSpawnHeightOffset);
		// Facing the other side.
		const FRotator SpawnRotation(0.0f, AwayFromFight > 0.0f ? 180.0f : 0.0f, 0.0f);
		const ATrackedTankMaster* SpawnedTank = World.SpawnActor<ATrackedTankMaster>(
			Template->GetClass(), FTransform(SpawnRotation, SpawnLocation), SpawnParameters);
		UE_LOG(
			LogRTSTraceImpactVfx,
			Display,
			TEXT("RTS_TRACE_IMPACT_VFX spawned tank=%s wanted_owner=%d owner=%d"),
			*GetNameSafe(SpawnedTank),
			OwningPlayer,
			IsValid(SpawnedTank) && IsValid(SpawnedTank->GetRTSComponent())
				? SpawnedTank->GetRTSComponent()->GetOwningPlayer()
				: -1);
	}
}

void FTraceImpactVfxPerfScenario::SpawnSquads(UWorld& World)
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	FVector CenterFromCommandLine = FVector::ZeroVector;
	FString CenterText;
	const bool bHasCenterOverride = FParse::Value(FCommandLine::Get(), TEXT("TraceImpactVfxCenter="), CenterText)
		&& CenterFromCommandLine.InitFromString(CenterText);
	const ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	const APawn* CameraPawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
	M_FightCenter = bHasCenterOverride
		? CenterFromCommandLine
		: (IsValid(CameraPawn) ? CameraPawn->GetActorLocation() : FVector::ZeroVector);
	UE_LOG(LogRTSTraceImpactVfx, Display, TEXT("RTS_TRACE_IMPACT_VFX fight_center=%s"),
	       *M_FightCenter.ToCompactString());

	SpawnSquadLine(World, GermanSquadClassPaths, -M_LineDistance * 0.5f, M_GermanSquads);
	SpawnSquadLine(World, SovietSquadClassPaths, M_LineDistance * 0.5f, M_SovietSquads);
	SpawnTanksBehindLine(World, GermanOwner, -M_LineDistance * 0.5f);
	SpawnTanksBehindLine(World, SovietOwner, M_LineDistance * 0.5f);
}

bool FTraceImpactVfxPerfScenario::GetHaveAllSquadsLoaded() const
{
	return M_FramesLeftInPhase <= 0;
}

void FTraceImpactVfxPerfScenario::MakeSoldiersHardToKill() const
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	const auto RaiseMaxHealth = [](ASquadUnit& Soldier)
	{
		UHealthComponent* HealthComponent = Soldier.GetHealthComponent();
		if (IsValid(HealthComponent))
		{
			HealthComponent->SetMaxHealth(HealthComponent->GetMaxHealth() * MaxHealthMultiplier);
		}
	};
	ForEachSoldier(M_GermanSquads, RaiseMaxHealth);
	ForEachSoldier(M_SovietSquads, RaiseMaxHealth);
}

void FTraceImpactVfxPerfScenario::HealSoldiers() const
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	const auto HealToFull = [](ASquadUnit& Soldier)
	{
		UHealthComponent* HealthComponent = Soldier.GetHealthComponent();
		if (IsValid(HealthComponent))
		{
			HealthComponent->SetCurrentHealth(HealthComponent->GetMaxHealth());
		}
	};
	ForEachSoldier(M_GermanSquads, HealToFull);
	ForEachSoldier(M_SovietSquads, HealToFull);
}

void FTraceImpactVfxPerfScenario::OrderSquadsToAttack(
	const TArray<TWeakObjectPtr<ASquadController>>& Attackers,
	const TArray<TWeakObjectPtr<ASquadController>>& Targets) const
{
	if (Targets.IsEmpty())
	{
		return;
	}
	for (int32 AttackerIndex = 0; AttackerIndex < Attackers.Num(); ++AttackerIndex)
	{
		ASquadController* Attacker = Attackers[AttackerIndex].Get();
		ASquadController* TargetSquad = Targets[AttackerIndex % Targets.Num()].Get();
		if (not IsValid(Attacker) || not IsValid(TargetSquad))
		{
			continue;
		}
		const TArray<ASquadUnit*> TargetSoldiers = TargetSquad->GetSquadUnitsChecked();
		if (TargetSoldiers.IsEmpty() || not IsValid(TargetSoldiers[0]))
		{
			continue;
		}
		Attacker->AttackActor(TargetSoldiers[0], true);
	}
}

void FTraceImpactVfxPerfScenario::StartFight(UWorld& World)
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	int32 GermanSoldierCount = 0;
	int32 SovietSoldierCount = 0;
	int32 WrongOwnerCount = 0;
	ForEachSoldier(M_GermanSquads, [&GermanSoldierCount, &WrongOwnerCount](ASquadUnit& Soldier)
	{
		++GermanSoldierCount;
		WrongOwnerCount += Soldier.GetOwningPlayer() == GermanOwner ? 0 : 1;
	});
	ForEachSoldier(M_SovietSquads, [&SovietSoldierCount, &WrongOwnerCount](ASquadUnit& Soldier)
	{
		++SovietSoldierCount;
		WrongOwnerCount += Soldier.GetOwningPlayer() == SovietOwner ? 0 : 1;
	});
	MakeSoldiersHardToKill();
	HealSoldiers();
	OrderSquadsToAttack(M_GermanSquads, M_SovietSquads);
	OrderSquadsToAttack(M_SovietSquads, M_GermanSquads);

	const bool bIsCentralised = GetIsDrawingCentralised(World);
	UE_LOG(
		LogRTSTraceImpactVfx,
		Display,
		TEXT("RTS_TRACE_IMPACT_VFX fight_start mode=%s german_squads=%d german_soldiers=%d soviet_squads=%d soviet_soldiers=%d soldiers_with_unexpected_owner=%d"),
		bIsCentralised ? TEXT("centralised") : TEXT("legacy"),
		M_GermanSquads.Num(),
		GermanSoldierCount,
		M_SovietSquads.Num(),
		SovietSoldierCount,
		WrongOwnerCount);

	M_Phase = ETraceImpactVfxPerfPhase::Fighting;
	M_FramesLeftInPhase = M_FightFrames;
	M_Samples = FTraceImpactVfxPerfSamples();
	M_Samples.FrameMilliseconds.Reserve(M_FightFrames);
	FTraceImpactVfxPerfCounters::Reset();
	FTraceImpactVfxPerfCounters::bIsGathering = true;
	if (bM_DumpStats && GEngine != nullptr)
	{
		GEngine->Exec(&World, *FString::Printf(TEXT("stat dumpave -num=%d -ms=0.02"), M_FightFrames - StatDumpFrameMargin));
	}
	M_LastTickSeconds = FPlatformTime::Seconds();
}

void FTraceImpactVfxPerfScenario::KeepCameraOnFight(UWorld& World)
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (not IsValid(PlayerController))
	{
		return;
	}
	if (not M_OverviewCamera.IsValid())
	{
		M_OverviewCamera = World.SpawnActor<ACameraActor>();
	}
	if (not M_OverviewCamera.IsValid())
	{
		return;
	}
	if (bM_TakeScreenshots)
	{
		// Close to the Soviet line, so the impacts on and around the soldiers can be told apart in a picture.
		M_OverviewCamera->SetActorLocationAndRotation(
			M_FightCenter + FVector(M_LineDistance * 0.5f - CloseUpDistance, 0.0f, CloseUpHeight),
			FRotator(CloseUpPitch, 0.0f, 0.0f));
	}
	else
	{
		M_OverviewCamera->SetActorLocationAndRotation(
			M_FightCenter + FVector(0.0f, 0.0f, OverviewCameraHeight),
			FRotator(-90.0f, 0.0f, 0.0f));
	}
	// The game's own camera takes the view back; it is handed to the overview camera again every frame.
	PlayerController->SetViewTarget(M_OverviewCamera.Get());
}

void FTraceImpactVfxPerfScenario::SampleCounts(UWorld& World)
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	int32 ActiveNiagaraComponents = 0;
	for (TObjectIterator<UNiagaraComponent> ComponentIterator; ComponentIterator; ++ComponentIterator)
	{
		const UNiagaraComponent* NiagaraComponent = *ComponentIterator;
		ActiveNiagaraComponents += (NiagaraComponent->GetWorld() == &World && NiagaraComponent->IsActive()) ? 1 : 0;
	}
	M_Samples.ActiveNiagaraComponentSamples += ActiveNiagaraComponents;
	M_Samples.DrawCallSamples += GNumDrawCallsRHI[0];

	const auto CountFiring = [this](ASquadUnit& Soldier)
	{
		AInfantryWeaponMaster* Weapon = Soldier.GetInfantryWeapon();
		M_Samples.FiringSoldierSamples += IsValid(Weapon) && IsValid(Weapon->GetCurrentTargetActor()) &&
			Weapon->GetIsCurrentTargetInRange() ? 1 : 0;
	};
	ForEachSoldier(M_GermanSquads, CountFiring);
	ForEachSoldier(M_SovietSquads, CountFiring);
	++M_Samples.CountSamples;
}

void FTraceImpactVfxPerfScenario::DrawShowcaseImpacts(UWorld& World) const
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	UTraceImpactVfxBatcher* ImpactVfxBatcher = nullptr;
	for (TActorIterator<ASmallArmsProjectileManager> ManagerIterator(&World); ManagerIterator; ++ManagerIterator)
	{
		ImpactVfxBatcher = ManagerIterator->GetTraceImpactVfxBatcher();
		break;
	}
	if (not IsValid(ImpactVfxBatcher) || not ImpactVfxBatcher->GetIsReadyToDraw())
	{
		return;
	}
	constexpr int32 TypeCount = static_cast<int32>(ETraceImpactVfxType::Count);
	const bool bDrawsReferences = FParse::Param(FCommandLine::Get(), TEXT("TraceImpactVfxShowcaseReference"));
	UNiagaraSystem* StoneReferenceSystem = bDrawsReferences
		? LoadObject<UNiagaraSystem>(nullptr, ShowcaseStoneReferencePath)
		: nullptr;
	UNiagaraSystem* SandReferenceSystem = bDrawsReferences
		? LoadObject<UNiagaraSystem>(nullptr, ShowcaseSandReferencePath)
		: nullptr;
	const float RowX = M_FightCenter.X + M_LineDistance * 0.5f - CloseUpDistance + ShowcaseDistanceFromCamera;
	// Slides the row sideways, to look at a type against another piece of ground.
	float RowSideOffset = 0.0f;
	FParse::Value(FCommandLine::Get(), TEXT("TraceImpactVfxShowcaseOffset="), RowSideOffset);
	for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
	{
		FVector ImpactLocation(
			RowX,
			M_FightCenter.Y + RowSideOffset + (TypeIndex - (TypeCount - 1) * 0.5f) * ShowcaseSpacing,
			M_FightCenter.Z);
		FHitResult GroundHit;
		const FVector TraceOffset(0.0f, 0.0f, ShowcaseGroundTraceHalfHeight);
		if (World.LineTraceSingleByChannel(
			GroundHit, ImpactLocation + TraceOffset, ImpactLocation - TraceOffset, ECC_WorldStatic))
		{
			ImpactLocation = GroundHit.ImpactPoint;
		}
		ImpactVfxBatcher->AddImpact(static_cast<ETraceImpactVfxType>(TypeIndex), ImpactLocation, FVector::UpVector, 1.0f);
		const ETraceImpactVfxType ImpactType = static_cast<ETraceImpactVfxType>(TypeIndex);
		UNiagaraSystem* ReferenceSystem = ImpactType == ETraceImpactVfxType::Stone
			? StoneReferenceSystem
			: (ImpactType == ETraceImpactVfxType::Sand ? SandReferenceSystem : nullptr);
		if (IsValid(ReferenceSystem))
		{
			// The effect the emitters were taken from, one step in front of ours to compare the two.
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				&World, ReferenceSystem, ImpactLocation + FVector(ShowcaseReferenceDistance, 0.0f, 0.0f));
		}
	}
}

void FTraceImpactVfxPerfScenario::RecordFrame(UWorld& World)
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	const double NowSeconds = FPlatformTime::Seconds();
	M_Samples.FrameMilliseconds.Add(static_cast<float>((NowSeconds - M_LastTickSeconds) * MillisecondsPerSecond));
	if (M_FramesLeftInPhase % FramesBetweenCounts == 0)
	{
		SampleCounts(World);
	}
	if (M_FramesLeftInPhase % FramesBetweenHeals == 0)
	{
		HealSoldiers();
	}
	if (bM_ShowcaseImpactTypes && M_FramesLeftInPhase % ShowcaseFramesBetweenImpacts == 0)
	{
		DrawShowcaseImpacts(World);
	}
	const bool bIsScreenshotFrame = bM_ShowcaseImpactTypes
		? M_FramesLeftInPhase % ShowcaseFramesBetweenScreenshots ==
		ShowcaseFramesBetweenScreenshots - ShowcaseScreenshotDelayFrames
		: M_FramesLeftInPhase % FramesBetweenScreenshots == 0;
	if (bM_TakeScreenshots && bIsScreenshotFrame)
	{
		const FString FileName = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(
			TEXT("TraceImpactVfx_%s_%02d.png"),
			bM_ShowcaseImpactTypes
				? TEXT("showcase")
				: (GetIsDrawingCentralised(World) ? TEXT("centralised") : TEXT("legacy")),
			M_ScreenshotCount++);
		FScreenshotRequest::RequestScreenshot(FileName, false, false);
		UE_LOG(LogRTSTraceImpactVfx, Display, TEXT("RTS_TRACE_IMPACT_VFX screenshot file=%s"), *FileName);
	}
	// Taken after the test's own work, so that is not part of the next frame's time.
	M_LastTickSeconds = FPlatformTime::Seconds();
}

void FTraceImpactVfxPerfScenario::ReportFight(UWorld& World) const
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	TArray<float> SortedFrames = M_Samples.FrameMilliseconds;
	SortedFrames.Sort();
	double TotalMilliseconds = 0.0;
	for (const float FrameMilliseconds : SortedFrames)
	{
		TotalMilliseconds += FrameMilliseconds;
	}
	const int32 FrameCount = FMath::Max(1, SortedFrames.Num());
	const int32 CountSamples = FMath::Max(1, M_Samples.CountSamples);
	const int32 ImpactCount = FTraceImpactVfxPerfCounters::ImpactCount;
	const double DispatchSeconds = FPlatformTime::ToSeconds64(FTraceImpactVfxPerfCounters::DispatchCycles);
	UE_LOG(
		LogRTSTraceImpactVfx,
		Display,
		TEXT("RTS_TRACE_IMPACT_VFX phase=fight mode=%s frames=%d avg_ms=%.3f p50_ms=%.3f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f impacts=%d impacts_per_frame=%.2f dispatch_us_per_impact=%.2f dispatch_ms_per_frame=%.4f avg_active_niagara_components=%lld avg_draw_calls=%lld avg_soldiers_firing=%d"),
		GetIsDrawingCentralised(World) ? TEXT("centralised") : TEXT("legacy"),
		SortedFrames.Num(),
		TotalMilliseconds / FrameCount,
		GetPercentile(SortedFrames, 0.5f),
		GetPercentile(SortedFrames, 0.95f),
		GetPercentile(SortedFrames, 0.99f),
		SortedFrames.IsEmpty() ? 0.0f : SortedFrames.Last(),
		ImpactCount,
		static_cast<double>(ImpactCount) / FrameCount,
		ImpactCount > 0 ? DispatchSeconds * MicrosecondsPerSecond / ImpactCount : 0.0,
		DispatchSeconds * MillisecondsPerSecond / FrameCount,
		M_Samples.ActiveNiagaraComponentSamples / CountSamples,
		M_Samples.DrawCallSamples / CountSamples,
		M_Samples.FiringSoldierSamples / CountSamples);
}

void FTraceImpactVfxPerfScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	using namespace TraceImpactVfxPerfScenarioPrivate;
	UWorld* World = CoverSubsystem.GetWorld();
	if (M_Phase == ETraceImpactVfxPerfPhase::Inactive || not IsValid(World))
	{
		return;
	}
	if (M_Phase == ETraceImpactVfxPerfPhase::WaitingForCoverScan)
	{
		// The map boots paused behind the start-game widget.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
		if (CoverSubsystem.GetHasCompletedFullScan())
		{
			SpawnSquads(*World);
			M_Phase = ETraceImpactVfxPerfPhase::WaitingForSquads;
			M_FramesLeftInPhase = SquadLoadFrames;
		}
		return;
	}
	KeepCameraOnFight(*World);
	if (M_Phase == ETraceImpactVfxPerfPhase::WaitingForSquads)
	{
		--M_FramesLeftInPhase;
		if (GetHaveAllSquadsLoaded())
		{
			StartFight(*World);
		}
		return;
	}
	RecordFrame(*World);
	if (--M_FramesLeftInPhase > 0)
	{
		return;
	}
	ReportFight(*World);
	FString ImpactsByOwner;
	for (const TPair<FName, int32>& OwnerImpacts : FTraceImpactVfxPerfCounters::ImpactCountByOwnerClass)
	{
		ImpactsByOwner += FString::Printf(TEXT("%s=%d "), *OwnerImpacts.Key.ToString(), OwnerImpacts.Value);
	}
	UE_LOG(LogRTSTraceImpactVfx, Display, TEXT("RTS_TRACE_IMPACT_VFX impacts by weapon owner: %s"), *ImpactsByOwner);
	UE_LOG(LogRTSTraceImpactVfx, Display, TEXT("RTS_TRACE_IMPACT_VFX emitters alive/spawned: %s"),
	       *DescribeTraceImpactEmitters(*World));
	FTraceImpactVfxPerfCounters::bIsGathering = false;
	M_Phase = ETraceImpactVfxPerfPhase::Inactive;
	UE_LOG(LogRTSTraceImpactVfx, Display, TEXT("RTS_TRACE_IMPACT_VFX RESULT DONE map=%s"), *World->GetName());
	if (FApp::IsUnattended())
	{
		// Nothing left to measure; do not sit out the rest of the run.
		FPlatformMisc::RequestExit(false);
	}
}

#endif // RTS_WITH_MAP_TESTS
