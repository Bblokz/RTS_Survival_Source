#include "CoverStressPerfScenario.h"

#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "RTS_Survival/Behaviours/Derived/InfantryStress/InfantryStressBehaviour.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderDeveloperSettings.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Navigation/CoverFinder/Tests/CoverStressTestScenario.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "UObject/UObjectArray.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCoverStressPerf, Log, All);

namespace CoverStressPerfScenarioPrivate
{
	// Measured as soon as this many soldiers hold cover, or with whoever does when the wait is over.
	constexpr int32 WantedSoldierCount = 24;
	constexpr float WaitForSoldiersSeconds = 45.0f;
	// Small enough that hundreds of them neither kill nor stress anybody.
	constexpr float MeterOnlyHealthLoss = 0.0001f;
	constexpr float HitHealthRatio = 0.002f;
	constexpr int32 MeterCallsPerSample = 2000;
	constexpr int32 HitsPerSample = 10;
	constexpr int32 TicksPerSample = 200;
	constexpr int32 SprayRestartsPerSample = 20;
	constexpr int32 RelocationSearchesPerSample = 5;
	constexpr double NanosecondsPerSecond = 1.0e9;

	int64 GetLiveObjectCount()
	{
		return GUObjectArray.GetObjectArrayNumMinusAvailable();
	}

	/**
	 * @brief Times a batch of calls and adds it to a metric as one sample.
	 * @param Metric Receives the time, the calls and the objects the calls left behind.
	 * @param CallCount How often Call is run.
	 * @param Call The work to time.
	 */
	template <typename CallType>
	void AddSample(FCoverStressPerfMetric& Metric, const int32 CallCount, CallType&& Call)
	{
		const int64 ObjectCountBefore = GetLiveObjectCount();
		const uint64 StartCycles = FPlatformTime::Cycles64();
		for (int32 CallIndex = 0; CallIndex < CallCount; ++CallIndex)
		{
			Call();
		}
		const double Nanoseconds = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartCycles) *
			NanosecondsPerSecond;
		Metric.TotalNanoseconds += Nanoseconds;
		Metric.FastestSampleNanoseconds = FMath::Min(Metric.FastestSampleNanoseconds, Nanoseconds / CallCount);
		Metric.CallCount += CallCount;
		Metric.CreatedObjectCount += FMath::Max<int64>(0, GetLiveObjectCount() - ObjectCountBefore);
	}
}

void FCoverStressPerfScenario::Start()
{
	bM_IsWaitingForCoverScan = true;
	UE_LOG(LogRTSCoverStressPerf, Display, TEXT("RTS_STRESS_PERF scheduled"));
}

void FCoverStressPerfScenario::Report(const FCoverStressPerfMetric& Metric) const
{
	UE_LOG(
		LogRTSCoverStressPerf,
		Display,
		TEXT("RTS_STRESS_PERF metric=%s calls=%lld avg_ns=%.0f fastest_ns=%.0f objects_per_call=%.3f"),
		Metric.Name,
		Metric.CallCount,
		Metric.CallCount > 0 ? Metric.TotalNanoseconds / Metric.CallCount : 0.0,
		Metric.CallCount > 0 ? Metric.FastestSampleNanoseconds : 0.0,
		Metric.CallCount > 0 ? static_cast<double>(Metric.CreatedObjectCount) / Metric.CallCount : 0.0);
}

void FCoverStressPerfScenario::Tick(URTSCoverFinderWorldSubsystem& CoverSubsystem)
{
	using namespace CoverStressPerfScenarioPrivate;
	UWorld* World = CoverSubsystem.GetWorld();
	if (not IsValid(World) || (not bM_IsWaitingForCoverScan && not bM_IsWaitingForSoldiers))
	{
		return;
	}
	if (bM_IsWaitingForCoverScan)
	{
		// The map boots paused behind the start-game widget.
		ACPPController* PlayerController = FRTS_Statics::GetRTSController(World);
		if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
		{
			PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
		if (CoverSubsystem.GetHasCompletedFullScan())
		{
			bM_IsWaitingForCoverScan = false;
			bM_IsWaitingForSoldiers = true;
			M_MeasureDeadlineWorldSeconds = World->GetTimeSeconds() + WaitForSoldiersSeconds;
		}
		return;
	}
	const TArray<ASquadUnit*> Soldiers = FindSoldiersHoldingCover(*World);
	if (Soldiers.Num() < WantedSoldierCount && World->GetTimeSeconds() < M_MeasureDeadlineWorldSeconds)
	{
		return;
	}
	bM_IsWaitingForSoldiers = false;
	Measure(*World, CoverSubsystem, Soldiers);
}

TArray<ASquadUnit*> FCoverStressPerfScenario::FindSoldiersHoldingCover(UWorld& World) const
{
	TArray<ASquadUnit*> Soldiers;
	for (TActorIterator<ASquadUnit> UnitIterator(&World); UnitIterator; ++UnitIterator)
	{
		ASquadUnit* SquadUnit = *UnitIterator;
		const ASquadController* Squad = IsValid(SquadUnit) ? SquadUnit->GetSquadControllerChecked() : nullptr;
		const bool bIsUsableSoldier = IsValid(Squad) && Squad->GetStressTolerance() > 0.0f &&
			SquadUnit->IsUnitAlive() && not SquadUnit->GetIsStressed() &&
			SquadUnit->GetCoverRuntimeState().State == ESquadUnitCoverState::Protected &&
			IsValid(SquadUnit->GetWeaponState()) && IsValid(SquadUnit->GetAnimBP_SquadUnit()) &&
			IsValid(SquadUnit->GetHealthComponent()) &&
			SquadUnit->GetHealthComponent()->GetHealthPercentage() > 0.9f;
		if (bIsUsableSoldier)
		{
			Soldiers.Add(SquadUnit);
		}
	}
	return Soldiers;
}

void FCoverStressPerfScenario::Measure(
	UWorld& World,
	URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const TArray<ASquadUnit*>& Soldiers)
{
	UE_LOG(
		LogRTSCoverStressPerf,
		Display,
		TEXT("RTS_STRESS_PERF soldiers=%d cover_points=%d stress_state_bytes=%d map=%s"),
		Soldiers.Num(),
		CoverSubsystem.GetCoverPointsView().Num(),
		static_cast<int32>(sizeof(FSquadUnitStressState)),
		*World.GetName());
	if (Soldiers.IsEmpty())
	{
		UE_LOG(LogRTSCoverStressPerf, Display, TEXT("RTS_STRESS_PERF RESULT FAIL no soldier holds cover"));
		return;
	}
	MeasureDamagePath(Soldiers);
	MeasureStressLifetime(CoverSubsystem, Soldiers);
	UE_LOG(LogRTSCoverStressPerf, Display, TEXT("RTS_STRESS_PERF RESULT DONE"));
}

void FCoverStressPerfScenario::MeasureDamagePath(const TArray<ASquadUnit*>& Soldiers) const
{
	using namespace CoverStressPerfScenarioPrivate;
	FCoverStressPerfMetric MeterMetric{TEXT("stress_meter_per_hit")};
	FCoverStressPerfMetric HitWithoutStressMetric{TEXT("take_damage_without_stress")};
	FCoverStressPerfMetric HitWithStressMetric{TEXT("take_damage_with_stress")};
	const FDamageEvent DamageEvent;
	for (ASquadUnit* Soldier : Soldiers)
	{
		const float HitDamage = Soldier->GetHealthComponent()->GetMaxHealth() * HitHealthRatio;
		AddSample(MeterMetric, MeterCallsPerSample, [Soldier]()
		{
			Soldier->RecordHealthLossForStress(MeterOnlyHealthLoss);
		});
		// The same hits with and without the soldier's own TakeDamage, which is where the stress meter sits.
		AddSample(HitWithoutStressMetric, HitsPerSample, [Soldier, HitDamage, &DamageEvent]()
		{
			Soldier->ACharacterObjectsMaster::TakeDamage(HitDamage, DamageEvent, nullptr, nullptr);
		});
		AddSample(HitWithStressMetric, HitsPerSample, [Soldier, HitDamage, &DamageEvent]()
		{
			Soldier->TakeDamage(HitDamage, DamageEvent, nullptr, nullptr);
		});
	}
	Report(MeterMetric);
	Report(HitWithoutStressMetric);
	Report(HitWithStressMetric);
}

void FCoverStressPerfScenario::MeasureStressLifetime(
	URTSCoverFinderWorldSubsystem& CoverSubsystem,
	const TArray<ASquadUnit*>& Soldiers) const
{
	using namespace CoverStressPerfScenarioPrivate;
	// The two parts of a start that can be timed by themselves, to see where its time goes.
	FCoverStressPerfMetric BehaviourCreateMetric{TEXT("behaviour_object_acquire")};
	FCoverStressPerfMetric BehaviourWeaponMetric{TEXT("behaviour_on_and_off_weapon")};
	FCoverStressPerfMetric StartMetric{TEXT("start_stress")};
	FCoverStressPerfMetric TickMetric{TEXT("tick_stress")};
	FCoverStressPerfMetric SprayRestartMetric{TEXT("spray_animation_restart")};
	FCoverStressPerfMetric RelocationSearchMetric{TEXT("relocation_cover_search")};
	FCoverStressPerfMetric EndMetric{TEXT("end_stress")};
	for (ASquadUnit* Soldier : Soldiers)
	{
		USquadUnitAnimInstance* SoldierAnimation = Soldier->GetAnimBP_SquadUnit();
		FCoverStressTestScenario::GiveSoldierCrouchSprayAnimationsWhenMissing(*SoldierAnimation);
		UInfantryStressBehaviour* TimedBehaviour = nullptr;
		AddSample(BehaviourCreateMetric, 1, [&CoverSubsystem, &TimedBehaviour]()
		{
			TimedBehaviour = CoverSubsystem.AcquireInfantryStressBehaviour();
		});
		AddSample(BehaviourWeaponMetric, 1, [Soldier, TimedBehaviour]()
		{
			TimedBehaviour->StartOnSoldier(*Soldier);
			TimedBehaviour->EndOnSoldier();
		});
		CoverSubsystem.ReleaseInfantryStressBehaviour(TimedBehaviour);
		AddSample(StartMetric, 1, [Soldier]()
		{
			Soldier->StartStress();
		});
		AddSample(TickMetric, TicksPerSample, [Soldier]()
		{
			Soldier->TickStress();
		});
		// What happens every time one spray clip ends and the next one starts.
		const ESquadCoverSprayStance SprayStance = Soldier->GetStressSprayStance();
		AddSample(SprayRestartMetric, SprayRestartsPerSample, [SoldierAnimation, SprayStance]()
		{
			SoldierAnimation->SetCoverSpray(ESquadCoverSprayStance::None);
			SoldierAnimation->SetCoverSpray(SprayStance);
		});
		AddSample(RelocationSearchMetric, RelocationSearchesPerSample, [Soldier, &CoverSubsystem]()
		{
			FRTSCoverPoint FartherPoint;
			if (Soldier->TryReserveStressRelocationPoint(CoverSubsystem, FartherPoint))
			{
				CoverSubsystem.ReleaseCoverReservation(*Soldier, FartherPoint.PointId);
			}
		});
		AddSample(EndMetric, 1, [Soldier]()
		{
			Soldier->EndStress();
		});
	}
	Report(BehaviourCreateMetric);
	Report(BehaviourWeaponMetric);
	Report(StartMetric);
	Report(TickMetric);
	Report(SprayRestartMetric);
	Report(RelocationSearchMetric);
	Report(EndMetric);
	// What a fight costs per second while this many soldiers are stressed at once.
	const URTSCoverFinderDeveloperSettings* CoverSettings = URTSCoverFinderDeveloperSettings::Get();
	constexpr double StressTicksPerSecond = 5.0;
	constexpr double NanosecondsPerMicrosecond = 1000.0;
	const double TickNanoseconds = TickMetric.CallCount > 0 ? TickMetric.TotalNanoseconds / TickMetric.CallCount : 0.0;
	UE_LOG(
		LogRTSCoverStressPerf,
		Display,
		TEXT("RTS_STRESS_PERF steady_state stressed_soldiers=%d tick_us_per_second=%.1f stress_duration_s=%.0f"),
		Soldiers.Num(),
		Soldiers.Num() * StressTicksPerSecond * TickNanoseconds / NanosecondsPerMicrosecond,
		IsValid(CoverSettings) ? CoverSettings->M_InfantryStressDuration : 0.0f);
}
