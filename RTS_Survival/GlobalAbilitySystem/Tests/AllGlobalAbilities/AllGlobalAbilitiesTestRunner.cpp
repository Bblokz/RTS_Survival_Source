// Copyright (C) Bas Blokzijl - All rights reserved.

// Global ability test (-RTSGlobalAbilityTest), RTS_SurvivalShippingGlobalAbilityTests only. On the map it is started on
// (a mission, survival map or test map) it uses the player's own loaded abilities, which that target unlocks (every
// template, nothing blocks them), and drives them the way a player does: page the global ability panel, press the
// item's button, then click the ground once (reticle) or twice (strip) through ACPPController's primary click handler.
//
// Sweep: the abilities in waves of a few at once on separate spots around the player's camera; each one must arm,
// execute, end exactly once (all but the instant spies) and bring its payload (drops) or hit a spawned enemy T-34
// (strikes, a warning only). Errors logged while a wave runs are reported with the wave.
// Stress: random abilities fired at random places (enemies, own units, map edges, outside the landscape, buildings)
// mixed with cancelled aims, bursts of one ability on one spot, aircraft and parachuting payload killed mid-flight,
// pause, time dilation, garbage collection every frame and attack-moves; then the map is reloaded while abilities are
// in flight and a few are fired in the fresh world.
//
// Progress goes to Saved/GlobalAbilityTests/<RunId>.txt line by line, so a crash or a mission defeat costs only the
// running wave: a new process (or the reloaded world) continues where the file ends and blames a crash on the wave
// that was running. Log lines start with RTS_GA_TEST.
//
// Options: -RTSGATestMode=Sweep|Stress|Both (default Both), -RTSGATestOnly=GA_A+GA_B, -RTSGATestShard=i
// -RTSGATestShardCount=n, -RTSGATestWave=<abilities per wave, default 3>, -RTSGATestStressSeconds=<default 600>,
// -RTSGATestRunId=<name>, -RTSGATestSeed=<n>, -RTSGATestNoTravel, -RTSTestExitOnComplete.
// Run script: Source/Tools/GlobalAbilityTests/run_global_ability_tests.ps1.

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/Tests/AllGlobalAbilities/AllGlobalAbilitiesUnlock.h"

#if RTS_WITH_ALL_GLOBAL_ABILITIES

#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "NavigationSystem.h"
#include "Tickable.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/CriticalSection.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/CooldownItem/W_CoolDownItem.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/TutorialOvertakeWidget/W_TutorialOvertake.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityCostState.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftBombing/GA_AircraftBombing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftDrop/GA_AircraftDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Barrage/GA_Barrage.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/BuildingDrop/GA_BuildingParachuteDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/CompositeDrop/GA_CompositeDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/FireStrikes/GA_IncendiaryAirStrike.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GliderDrop/GA_GliderTankDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/RadixiteNuke/GA_RadixiteNuke.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ResourceDrop/GA_ResourceDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ResourceDrop/ParachutedResourcePayload.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Sonnengewehr/GA_Sonnengewehr.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/SpiesInfiltration/GA_SpiesInfiltration.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/GlobalAbilityPanel/W_GlobalAbilityPanel.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/W_GA_Item/W_GA_Item.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpCharacterObjectsMaster.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Weapons/BombComponent/BombActor/BombActor.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "Stats/Stats.h"
#include "UObject/GCObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSGlobalAbilityTest, Log, All);

/** Friend of UGlobalAbility and ACPPController: reads the state a player sees but the classes keep private. */
struct FAllGlobalAbilitiesTestAccess
{
	static bool GetIsArmed(const UGlobalAbility& Ability)
	{
		return Ability.M_AbilityState == EGlobalAbilityState::Activated;
	}

	static bool GetIsGameLocked(const ACPPController& Controller)
	{
		return Controller.M_PauseGameState.bM_IsGameLocked;
	}

	static bool GetIsCinematicActive(const ACPPController& Controller)
	{
		return Controller.bM_IsCinematicTakeOverActive || Controller.M_ActiveMissionCinematicTakeOverSession.IsValid();
	}

	static bool GetHasArmedGlobalAbility(const ACPPController& Controller)
	{
		return Controller.M_ActiveGlobalAbility.IsValid();
	}

	/** @return The soldier's controller pointer as stored, without the validity report a getter logs. */
	static ASquadController* GetRawSquadController(const ASquadUnit& SquadUnit)
	{
		return SquadUnit.M_SquadController.Get();
	}

	static const TArray<ASquadUnit*>& GetSquadUnitList(const ASquadController& SquadController)
	{
		return SquadController.M_TSquadUnits;
	}
};

namespace RTS::AllGlobalAbilitiesTest
{
	constexpr uint8 PlayerIndex = 1;
	constexpr uint8 EnemyIndex = 2;
	constexpr float StartDelaySeconds = 8.f;
	constexpr float NotReadyTimeoutSeconds = 150.f;
	constexpr float LandscapeEdgeMargin = 2500.f;
	constexpr float SpotRingRadius = 7000.f;
	constexpr float SpotRingStep = 2500.f;
	constexpr float AnchorShiftTowardCentre = 6000.f;
	constexpr float MinSpotDistanceToOwnUnits = 3500.f;
	constexpr float PayloadRadius = 4500.f;
	constexpr float AbilityTimeoutSeconds = 320.f;
	constexpr float SettleSeconds = 14.f;
	constexpr float GameOverLockSeconds = 8.f;
	constexpr float TargetSpawnAboveGroundCm = 120.f;
	constexpr float GroundToleranceCm = 450.f;
	constexpr int32 MaxErrorSamples = 6;
	constexpr int32 DefaultWaveSize = 3;
	constexpr float DefaultStressSeconds = 600.f;
	constexpr int32 StressMaxLiveAircraft = 45;
	constexpr int32 StressMaxLivePayload = 120;
	constexpr float TravelInFlightSeconds = 4.f;
	constexpr float TravelArrivalSettleSeconds = 20.f;
	constexpr float TravelFireWaitSeconds = 150.f;
	const TCHAR* const StrikeTargetClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	// Strafing aircraft fire machine guns and light cannon, which do not get through a T-34: a soft rocket truck.
	const TCHAR* const SoftStrikeTargetClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/BM13/Rus_ArmoredCar_BM13.Rus_ArmoredCar_BM13_C");
	constexpr int32 ErrorKeyLength = 140;
	const FName NormalDifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const FName TutorialContinueFunctionName(TEXT("OnClickedContinue"));
	const FName AbilityButtonPropertyName(TEXT("AbilityButton"));

	// Survives world changes in this process: tells a reloaded world from a relaunched process (a crash).
	bool GHasRunInThisProcess = false;

	enum class EAbilityKind : uint8
	{
		// Ends right away without an end broadcast.
		Instant,
		// Brings units, buildings or resources to the spot.
		Drop,
		// Damages what is at the spot; gets an enemy T-34 there.
		Strike,
		// Recon, flares, mines: ends, no payload check.
		Other
	};

	enum class ERunPhase : uint8
	{
		WaitingForStart,
		Sweep,
		Stress,
		TravelInFlight,
		AfterTravel,
		Done
	};

	enum class EWaveStage : uint8
	{
		None,
		WaitingForEnd,
		Settling
	};

	struct FWaveCase
	{
		TObjectPtr<UGlobalAbility> Ability = nullptr;
		EGlobalAbility Type = EGlobalAbility::GA_None;
		EAbilityKind Kind = EAbilityKind::Other;
		FVector Spot = FVector::ZeroVector;
		FVector LineEnd = FVector::ZeroVector;
		bool bIsStrip = false;
		int32 EndedCount = 0;
		float EndedAtSeconds = -1.f;
		// Judged a few seconds after its own end: a payload (a harvester) may drive off while the wave still runs.
		bool bEvaluated = false;
		// Drops: payload seen on the ground near the aim at any time, so a unit killed or driving off later still
		// counts as delivered. Owner counted when it arrived.
		TSet<TWeakObjectPtr<AActor>> Arrived;
		TSet<FString> ArrivedClasses;
		int32 ArrivedPlayerOwned = 0;
		int32 ArrivedOtherOwned = 0;
		TWeakObjectPtr<AActor> StrikeTarget;
		float StrikeTargetStartHealth = 0.f;
		TArray<FString> Fails;
		TArray<FString> Warns;
		TArray<FString> Notes;
	};

	/** @return 2D distance to the aim point, or to the aimed strip: a glider's tank comes out at the strip's end. */
	float GetDistanceToAim(const FWaveCase& Case, const FVector& Location)
	{
		const FVector Flat(Location.X, Location.Y, 0.f);
		const FVector Start(Case.Spot.X, Case.Spot.Y, 0.f);
		if (not Case.bIsStrip)
		{
			return FVector::Dist(Flat, Start);
		}
		const FVector End(Case.LineEnd.X, Case.LineEnd.Y, 0.f);
		return FMath::PointDistToSegment(Flat, Start, End);
	}

	struct FProgress
	{
		FString MapPackage;
		TSet<FString> Finished;
		TArray<FString> OpenWave;
		TMap<FString, int32> WaveStarts;
		bool bSweepDone = false;
		bool bStressDone = false;
		bool bTravelStarted = false;
		bool bRunDone = false;
	};

	FString GetAbilityName(const EGlobalAbility Ability)
	{
		return StaticEnum<EGlobalAbility>()->GetNameStringByValue(static_cast<int64>(Ability));
	}

	EAbilityKind GetKind(const UGlobalAbility& Ability)
	{
		if (Ability.IsA<UGA_SpiesInfiltration>())
		{
			return EAbilityKind::Instant;
		}
		if (Ability.IsA<UGA_AircraftDrop>() || Ability.IsA<UGA_Paratroopers>() || Ability.IsA<UGA_GliderTankDrop>()
			|| Ability.IsA<UGA_CompositeDrop>() || Ability.IsA<UGA_BuildingParachuteDrop>()
			|| Ability.IsA<UGA_ResourceDrop>())
		{
			return EAbilityKind::Drop;
		}
		if (Ability.IsA<UGA_AircraftBombing>() || Ability.IsA<UGA_AircraftStrafing>() || Ability.IsA<UGA_Barrage>()
			|| Ability.IsA<UGA_RadixiteNuke>() || Ability.IsA<UGA_Sonnengewehr>()
			|| Ability.IsA<UGA_IncendiaryAirStrike>())
		{
			return EAbilityKind::Strike;
		}
		return EAbilityKind::Other;
	}

	const TCHAR* GetKindName(const EAbilityKind Kind)
	{
		switch (Kind)
		{
		case EAbilityKind::Instant:
			return TEXT("instant");
		case EAbilityKind::Drop:
			return TEXT("drop");
		case EAbilityKind::Strike:
			return TEXT("strike");
		case EAbilityKind::Other:
			return TEXT("other");
		}
		return TEXT("other");
	}

	struct FWaveCase;
	float GetDistanceToAim(const FWaveCase& Case, const FVector& Location);

	uint8 GetOwnerIndex(const AActor* Actor)
	{
		const URTSComponent* RTSComponent = IsValid(Actor) ? Actor->FindComponentByClass<URTSComponent>() : nullptr;
		return IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : 0;
	}

	float GetHealth(const AActor* Actor)
	{
		const UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		return IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
	}

	/** @brief Kills through the health path a real hit uses (death effects, wrecks, unit manager), not Destroy. */
	void KillActor(AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return;
		}
		if (AHpPawnMaster* Pawn = Cast<AHpPawnMaster>(Actor))
		{
			Pawn->TakeFatalDamage();
			return;
		}
		if (ASquadController* Squad = Cast<ASquadController>(Actor))
		{
			for (ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
			{
				if (IsValid(Unit))
				{
					Unit->TakeFatalDamage();
				}
			}
			return;
		}
		if (ACharacterObjectsMaster* Character = Cast<ACharacterObjectsMaster>(Actor))
		{
			Character->TakeFatalDamage();
			return;
		}
		if (AHPActorObjectsMaster* HpActor = Cast<AHPActorObjectsMaster>(Actor))
		{
			HpActor->TakeFatalDamage();
		}
	}

	/** @brief Counts error and ensure lines other code logs, with a few samples, so a run that "works" while
	 * spamming errors is still reported. */
	class FErrorCounter final : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			const ELogVerbosity::Type Level = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
			const bool bIsEnsure = FCString::Strifind(Message, TEXT("Ensure condition failed")) != nullptr;
			if ((Level != ELogVerbosity::Error && Level != ELogVerbosity::Fatal && not bIsEnsure)
				|| Category == LogRTSGlobalAbilityTest.GetCategoryName())
			{
				return;
			}
			FScopeLock Lock(&M_Lock);
			++M_ErrorCount;
			// One line per sample: the progress file is read line by line.
			FString Key = Category.ToString() + TEXT(": ") + FString(Message).Left(ErrorKeyLength);
			Key.ReplaceCharInline(TEXT('\n'), TEXT(' '));
			Key.ReplaceCharInline(TEXT('\r'), TEXT(' '));
			M_Distinct.FindOrAdd(Key)++;
			// #18: a soldier whose squad controller is gone reports it from GetIsValidSquadController; collect who.
			const TCHAR* ObjectStart = FCString::Strstr(Message, TEXT("Object:"));
			if (ObjectStart && FCString::Strstr(Message, TEXT("M_SquadController"))
				&& FCString::Strstr(Message, TEXT("ASquadUnit::GetIsValidSquadController")))
			{
				FString ObjectName(ObjectStart + 7);
				int32 LineEnd = INDEX_NONE;
				if (ObjectName.FindChar(TEXT('\n'), LineEnd))
				{
					ObjectName.LeftInline(LineEnd);
				}
				ObjectName.TrimStartAndEndInline();
				if (not M_ReportedOrphanErrorObjects.Contains(ObjectName))
				{
					M_ReportedOrphanErrorObjects.Add(ObjectName);
					M_NewOrphanErrorObjects.Add(ObjectName);
				}
			}
		}

		TArray<FString> TakeNewOrphanErrorObjects()
		{
			FScopeLock Lock(&M_Lock);
			TArray<FString> NewObjects = MoveTemp(M_NewOrphanErrorObjects);
			M_NewOrphanErrorObjects.Reset();
			return NewObjects;
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }

		// The most frequent distinct messages first, each with its count.
		int32 TakeErrors(TArray<FString>& OutSamples)
		{
			FScopeLock Lock(&M_Lock);
			const int32 Count = M_ErrorCount;
			M_Distinct.ValueSort([](const int32 A, const int32 B) { return A > B; });
			OutSamples.Reset();
			for (const TPair<FString, int32>& Pair : M_Distinct)
			{
				if (OutSamples.Num() >= MaxErrorSamples)
				{
					break;
				}
				OutSamples.Add(FString::Printf(TEXT("%dx %s"), Pair.Value, *Pair.Key));
			}
			M_Distinct.Reset();
			M_ErrorCount = 0;
			return Count;
		}

	private:
		FCriticalSection M_Lock;
		int32 M_ErrorCount = 0;
		TMap<FString, int32> M_Distinct;
		TSet<FString> M_ReportedOrphanErrorObjects;
		TArray<FString> M_NewOrphanErrorObjects;
	};

	class FAllGlobalAbilitiesTestRunner final : public FTickableGameObject, public FGCObject
	{
	public:
		explicit FAllGlobalAbilitiesTestRunner(UWorld& InWorld);
		virtual ~FAllGlobalAbilitiesTestRunner() override;

		virtual void Tick(float DeltaTime) override;
		virtual TStatId GetStatId() const override;
		virtual bool IsTickable() const override { return M_Phase != ERunPhase::Done && M_World.IsValid(); }
		virtual bool IsTickableWhenPaused() const override { return true; }
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FAllGlobalAbilitiesTestRunner"); }

		bool GetIsForWorld(const UWorld* World) const { return M_World.Get() == World; }

	private:
		// ----- Setup, progress file -----
		void TickWaitingForStart();
		void KeepMissionPlayable();
		bool TickGameOver();
		void PrepareRun();
		void ReadProgress();
		void AppendProgress(const FString& Line);
		void Log(const FString& Line) const;
		void Result(const FString& Verdict, const FString& Name, const FString& Details);
		void FinishRun(const FString& Summary);
		void ReloadMap(const FString& Reason);
		float GetNow() const;
		float GetRealNow() const;

		// ----- Player click path -----
		ACPPController* GetController() const;
		UGlobalAbilitiesManager* GetManager() const;
		UGlobalAbility* FindLoadedAbility(EGlobalAbility Type) const;
		UW_GA_Item* ShowAbilityInPanel(const UGlobalAbility& Ability) const;
		UButton* GetItemButton(const UW_GA_Item& Item) const;
		// Instant abilities (spies) execute on the press itself: true with bOutExecutedOnPress, no ground click.
		bool PressAbilityButton(UGlobalAbility& Ability, FString& OutProblem, bool& bOutExecutedOnPress) const;
		bool FireAbility(UGlobalAbility& Ability, const FVector& Spot, const FVector& LineEnd, bool& bOutIsStrip,
		                 FString& OutProblem) const;
		bool ClickGround(UGlobalAbility& Ability, const FVector& Spot, const FVector& LineEnd, bool& bOutIsStrip,
		                 FString& OutProblem) const;

		// ----- Sweep -----
		void BuildSweepQueue();
		void TickSweep();
		void StartWave();
		void TickWaveWaiting();
		TArray<AActor*> GetLiveWaveSpawned() const;
		void TrackArrivals(FWaveCase& Case) const;
		void TrackBombImpacts();
		void ScanForOrphanedSoldiers();
		void StartGroundRecoveryChecks();
		void TickGroundRecoveryChecks();
		FString DescribeNearestWaveUnit(const FWaveCase& Case, const TArray<AActor*>& WaveSpawned) const;
		static bool GetIsPayloadCandidate(const AActor& Actor);
		void EvaluateWave();
		void EvaluateCase(FWaveCase& Case, const TArray<AActor*>& WaveSpawned);
		void CleanupWave();
		bool FindSpot(int32 SpotIndex, FVector& OutSpot) const;
		// Units the player had before the wave (the base, the mission's own units), not ability payload.
		void CacheOwnUnitLocations();
		bool GetIsNearOwnUnit(const FVector& Location) const;
		FVector GetLineEnd(const FVector& Spot, const UGlobalAbility& Ability) const;
		AActor* SpawnStrikeTarget(const FVector& Spot, bool bSoftTarget, uint8 TargetOwner);

		// ----- Stress -----
		void StartStress();
		void TickStress();
		void StressAction();
		void StressFire(bool bBurst);
		void StressCancelAim(bool bStripAnchorFirst);
		void StressKillRandom(bool bAircraft);
		void StressKillEnemy();
		void StressAttackMove();
		void StressPause();
		void StressTimeDilation();
		void StressGarbageBurst();
		FVector GetStressTarget(FString& OutLabel);
		FVector GetRandomStripEnd(const FVector& Start, const UGlobalAbility& Ability);
		void TickStressEffects();
		void ReportStress(const FString& HowItEnded);
		void EndStress();

		// ----- Travel -----
		void StartTravelInFlight();
		void TickTravelInFlight();
		void TickAfterTravel();

		// ----- Helpers -----
		void HandleActorSpawned(AActor* Actor);
		void HandleAbilityEnded(UGlobalAbility* Ability);
		void BindEndedDelegates();
		FBox FindLandscapeBounds() const;
		FVector KeepInsideLandscape(const FVector& Location, float Margin) const;
		bool GetIsOnGround(const AActor& Actor) const;
		TArray<AActor*> GetLiveTracked(bool bAircraft) const;
		void RemoveDeadTracked();

		TWeakObjectPtr<UWorld> M_World;
		FDelegateHandle M_ActorSpawnedHandle;
		TSharedPtr<FErrorCounter> M_ErrorCounter;
		FRandomStream M_Random;
		FString M_RunId;
		FString M_ProgressFile;
		FProgress M_Progress;
		ERunPhase M_Phase = ERunPhase::WaitingForStart;
		float M_StartSeconds = -1.f;
		float M_LockedSinceRealSeconds = -1.f;
		bool bM_ExitOnComplete = false;
		bool bM_DoSweep = true;
		bool bM_DoStress = true;
		bool bM_DoTravel = true;
		FVector M_Anchor = FVector::ZeroVector;
		FBox M_LandscapeBounds = FBox(ForceInit);
		int32 M_PassCount = 0;
		int32 M_WarnCount = 0;
		int32 M_FailCount = 0;
		// Ability ends counted per loaded ability (an ability can run several times at once in the stress phase).
		TMap<TWeakObjectPtr<UGlobalAbility>, int32> M_EndedCounts;
		TArray<TWeakObjectPtr<UGlobalAbility>> M_BoundAbilities;

		// Sweep.
		TArray<EGlobalAbility> M_SweepQueue;
		int32 M_WaveSize = DefaultWaveSize;
		TArray<FWaveCase> M_Wave;
		EWaveStage M_WaveStage = EWaveStage::None;
		float M_WaveStartSeconds = 0.f;
		float M_StageStartSeconds = 0.f;
		TArray<TWeakObjectPtr<AActor>> M_WaveSpawned;
		TArray<TWeakObjectPtr<AActor>> M_SweepAircraft;
		TArray<FVector> M_OwnUnitLocations;
		// Orphaned soldiers: alive, but their squad controller is gone. The last sample of each soldier's controller
		// tells which controller it was and whether that controller still listed the soldier.
		struct FSoldierControllerSample
		{
			TWeakObjectPtr<ASquadController> Controller;
			FString ControllerName;
			FString ControllerClass;
			bool bWasInControllerList = false;
			int32 ControllerListSize = 0;
		};
		TMap<TWeakObjectPtr<ASquadUnit>, FSoldierControllerSample> M_SoldierControllers;
		TSet<TWeakObjectPtr<ASquadUnit>> M_ReportedOrphans;
		TSet<TWeakObjectPtr<ASquadUnit>> M_ReportedFallenUnits;
		// Ground recovery (FRTSUnitGroundRecovery, URTSKillZSubsystem): a tank spawned inside the landscape, and a tank
		// and a soldier dropped below the KillZ, must all end up standing on the ground.
		struct FGroundRecoveryCheck
		{
			FString Label;
			TWeakObjectPtr<AActor> Unit;
			double DropAtReal = -1.0;
			double EvaluateAtReal = 0.0;
			bool bDone = false;
		};
		TArray<FGroundRecoveryCheck> M_GroundChecks;
		bool bM_GroundChecksStarted = false;
		double M_LastOrphanScanReal = -1.0;
		FString M_LastStressAction;
		double M_LastStressActionReal = 0.0;
		// Pooled bombs of the wave: shown while falling, hidden again when they hit. The last shown location of each
		// fall is its impact, so a carpet that misses its aim point shows by how much.
		TMap<TWeakObjectPtr<AActor>, FVector> M_BombLastSeen;
		TArray<FVector> M_BombImpacts;

		// Stress.
		float M_StressSeconds = DefaultStressSeconds;
		float M_StressStartReal = 0.f;
		float M_NextStressActionReal = 0.f;
		int32 M_StressActions = 0;
		int32 M_StressFired = 0;
		int32 M_StressRejected = 0;
		TMap<FString, int32> M_StressActionCounts;
		float M_UnpauseAtReal = -1.f;
		float M_ResetDilationAtReal = -1.f;
		float M_EndGcBurstAtReal = -1.f;
		TArray<TWeakObjectPtr<AActor>> M_StressAircraft;
		TArray<TWeakObjectPtr<AActor>> M_StressPayload;
		bool bM_TrackStress = false;

		// Travel.
		float M_TravelStepStart = 0.f;
		bool bM_AfterTravelFired = false;
	};

	FAllGlobalAbilitiesTestRunner::FAllGlobalAbilitiesTestRunner(UWorld& InWorld)
		: M_World(&InWorld)
		, M_ErrorCounter(MakeShared<FErrorCounter>())
	{
		M_ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateRaw(this, &FAllGlobalAbilitiesTestRunner::HandleActorSpawned));
		GLog->AddOutputDevice(M_ErrorCounter.Get());
		int32 Seed = static_cast<int32>(FDateTime::Now().GetTicks() % 100000);
		FParse::Value(FCommandLine::Get(), TEXT("RTSGATestSeed="), Seed);
		M_Random.Initialize(Seed);
		bM_ExitOnComplete = FParse::Param(FCommandLine::Get(), TEXT("RTSTestExitOnComplete"));
		bM_DoTravel = not FParse::Param(FCommandLine::Get(), TEXT("RTSGATestNoTravel"));
		FString Mode = TEXT("Both");
		FParse::Value(FCommandLine::Get(), TEXT("RTSGATestMode="), Mode);
		bM_DoSweep = Mode.Equals(TEXT("Both"), ESearchCase::IgnoreCase) || Mode.Equals(TEXT("Sweep"), ESearchCase::IgnoreCase);
		bM_DoStress = Mode.Equals(TEXT("Both"), ESearchCase::IgnoreCase) || Mode.Equals(TEXT("Stress"), ESearchCase::IgnoreCase);
		FParse::Value(FCommandLine::Get(), TEXT("RTSGATestWave="), M_WaveSize);
		M_WaveSize = FMath::Clamp(M_WaveSize, 1, 6);
		FParse::Value(FCommandLine::Get(), TEXT("RTSGATestStressSeconds="), M_StressSeconds);
		M_RunId = InWorld.GetMapName();
		M_RunId.RemoveFromStart(InWorld.StreamingLevelsPrefix);
		FParse::Value(FCommandLine::Get(), TEXT("RTSGATestRunId="), M_RunId);
		M_ProgressFile = FPaths::ProjectSavedDir() / TEXT("GlobalAbilityTests") / (M_RunId + TEXT(".txt"));
		UE_LOG(LogRTSGlobalAbilityTest, Display, TEXT("RTS_GA_TEST RUNNER_CREATED map=%s run=%s seed=%d progress=%s"),
		       *InWorld.GetMapName(), *M_RunId, Seed, *M_ProgressFile);
	}

	FAllGlobalAbilitiesTestRunner::~FAllGlobalAbilitiesTestRunner()
	{
		GLog->RemoveOutputDevice(M_ErrorCounter.Get());
		if (UWorld* World = M_World.Get())
		{
			World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
		}
		for (const TWeakObjectPtr<UGlobalAbility>& Ability : M_BoundAbilities)
		{
			if (Ability.IsValid())
			{
				Ability->OnGlobalAbilityEnded.RemoveAll(this);
			}
		}
	}

	TStatId FAllGlobalAbilitiesTestRunner::GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAllGlobalAbilitiesTestRunner, STATGROUP_Tickables);
	}

	void FAllGlobalAbilitiesTestRunner::AddReferencedObjects(FReferenceCollector& Collector)
	{
		for (FWaveCase& Case : M_Wave)
		{
			Collector.AddReferencedObject(Case.Ability);
		}
	}

	float FAllGlobalAbilitiesTestRunner::GetNow() const
	{
		const UWorld* World = M_World.Get();
		return IsValid(World) ? World->GetTimeSeconds() : 0.f;
	}

	float FAllGlobalAbilitiesTestRunner::GetRealNow() const
	{
		const UWorld* World = M_World.Get();
		return IsValid(World) ? World->GetRealTimeSeconds() : 0.f;
	}

	void FAllGlobalAbilitiesTestRunner::Log(const FString& Line) const
	{
		UE_LOG(LogRTSGlobalAbilityTest, Display, TEXT("RTS_GA_TEST %s"), *Line);
	}

	void FAllGlobalAbilitiesTestRunner::AppendProgress(const FString& Line)
	{
		// Another process holding the file (a tail -F on it) makes appends fail; retry once, then say so loudly
		// (every RESULT line is in the game log as well).
		for (int32 Attempt = 0; Attempt < 2; ++Attempt)
		{
			if (FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *M_ProgressFile,
			                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(),
			                                  FILEWRITE_Append))
			{
				return;
			}
			FPlatformProcess::Sleep(0.05f);
		}
		UE_LOG(LogRTSGlobalAbilityTest, Error, TEXT("RTS_GA_TEST PROGRESS_WRITE_FAILED %s line=%s"), *M_ProgressFile,
		       *Line.Left(120));
	}

	void FAllGlobalAbilitiesTestRunner::Result(const FString& Verdict, const FString& Name, const FString& Details)
	{
		if (Verdict == TEXT("PASS"))
		{
			++M_PassCount;
		}
		else if (Verdict == TEXT("WARN"))
		{
			++M_WarnCount;
		}
		else
		{
			++M_FailCount;
		}
		const FString Line = FString::Printf(TEXT("RESULT %s %s %s"), *Verdict, *Name, *Details);
		if (Verdict == TEXT("FAIL"))
		{
			UE_LOG(LogRTSGlobalAbilityTest, Warning, TEXT("RTS_GA_TEST %s"), *Line);
		}
		else
		{
			Log(Line);
		}
		AppendProgress(Line);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Start, mission state, progress
	// ---------------------------------------------------------------------------------------------------------------

	void FAllGlobalAbilitiesTestRunner::Tick(float DeltaTime)
	{
		const UWorld* World = M_World.Get();
		if (not IsValid(World) || not World->HasBegunPlay())
		{
			return;
		}
		if (M_Phase == ERunPhase::WaitingForStart)
		{
			TickWaitingForStart();
			return;
		}
		KeepMissionPlayable();
		if (TickGameOver())
		{
			return;
		}
		ScanForOrphanedSoldiers();
		if (M_Phase == ERunPhase::Sweep && not bM_GroundChecksStarted)
		{
			StartGroundRecoveryChecks();
		}
		TickGroundRecoveryChecks();
		switch (M_Phase)
		{
		case ERunPhase::Sweep:
			TickSweep();
			return;
		case ERunPhase::Stress:
			TickStress();
			return;
		case ERunPhase::TravelInFlight:
			TickTravelInFlight();
			return;
		case ERunPhase::AfterTravel:
			TickAfterTravel();
			return;
		default:
			return;
		}
	}

	void FAllGlobalAbilitiesTestRunner::TickWaitingForStart()
	{
		KeepMissionPlayable();
		if (M_StartSeconds < 0.f)
		{
			M_StartSeconds = GetRealNow();
			return;
		}
		// Real time: a mission can hold game time still behind its start widget.
		if (GetRealNow() - M_StartSeconds < StartDelaySeconds)
		{
			return;
		}
		if (not IsValid(GetManager()) || GetManager()->GetLoadedAbilities().Num() == 0)
		{
			if (GetRealNow() - M_StartSeconds > NotReadyTimeoutSeconds)
			{
				// A mission's end can travel to a map without the RTS player (main menu, campaign).
				ReadProgress();
				const FString ThisMap = M_World->GetOutermost()->GetName();
				if (not M_Progress.MapPackage.IsEmpty() && M_Progress.MapPackage != ThisMap)
				{
					ReloadMap(TEXT("not_ready_on_") + ThisMap);
					return;
				}
				Result(TEXT("FAIL"), TEXT("RUN"), FString::Printf(TEXT("map_never_ready map=%s manager=%d abilities=%d"),
				                                                 *ThisMap, IsValid(GetManager()) ? 1 : 0,
				                                                 IsValid(GetManager()) ? GetManager()->GetLoadedAbilities().Num() : 0));
				FinishRun(TEXT("not_ready"));
			}
			return;
		}
		const ACPPController* Controller = GetController();
		if (IsValid(Controller) && FAllGlobalAbilitiesTestAccess::GetIsCinematicActive(*Controller))
		{
			return;
		}
		PrepareRun();
	}

	void FAllGlobalAbilitiesTestRunner::KeepMissionPlayable()
	{
		ACPPController* Controller = GetController();
		if (not IsValid(Controller))
		{
			return;
		}
		// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
		UPlayerStartGameControl* StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
		if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
		{
			StartGameControl->PlayerStartedGame();
		}
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(M_World.Get(), Widgets, UW_GameDifficultyPicker::StaticClass(), false);
		for (UUserWidget* Picker : Widgets)
		{
			UButton* NormalButton = IsValid(Picker) && Picker->IsInViewport() && IsValid(Picker->WidgetTree)
				                        ? Cast<UButton>(Picker->WidgetTree->FindWidget(NormalDifficultyButtonName))
				                        : nullptr;
			if (IsValid(NormalButton))
			{
				NormalButton->OnClicked.Broadcast();
				Log(TEXT("picked_normal_difficulty"));
			}
		}
		Widgets.Reset();
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(M_World.Get(), Widgets, UW_TutorialOvertake::StaticClass(), false);
		for (UUserWidget* Overtake : Widgets)
		{
			UFunction* Continue = IsValid(Overtake) && Overtake->IsInViewport()
				                      ? Overtake->FindFunction(TutorialContinueFunctionName)
				                      : nullptr;
			if (Continue != nullptr)
			{
				Overtake->ProcessEvent(Continue, nullptr);
			}
		}
		// The stress phase pauses on purpose and unpauses itself.
		const bool bStressPause = M_UnpauseAtReal > 0.f;
		if (not bStressPause && Controller->GetIsGameOnPause() && not FAllGlobalAbilitiesTestAccess::GetIsGameLocked(*Controller)
			&& not FAllGlobalAbilitiesTestAccess::GetIsCinematicActive(*Controller))
		{
			Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		}
	}

	bool FAllGlobalAbilitiesTestRunner::TickGameOver()
	{
		const ACPPController* Controller = GetController();
		const bool bLocked = IsValid(Controller) && FAllGlobalAbilitiesTestAccess::GetIsGameLocked(*Controller);
		if (not bLocked)
		{
			M_LockedSinceRealSeconds = -1.f;
			return false;
		}
		if (M_LockedSinceRealSeconds < 0.f)
		{
			M_LockedSinceRealSeconds = GetRealNow();
			return true;
		}
		if (GetRealNow() - M_LockedSinceRealSeconds < GameOverLockSeconds)
		{
			return true;
		}
		if (M_Phase == ERunPhase::AfterTravel)
		{
			// The reloaded world was lost (or won) before its abilities ended: reloading again would only loop.
			Result(TEXT("WARN"), TEXT("TRAVEL"), TEXT("game_over_in_reloaded_world_before_abilities_ended"));
			FinishRun(TEXT("game_over_after_travel"));
			return true;
		}
		if (M_Phase == ERunPhase::Stress)
		{
			// Strikes on its own units can lose the mission; that ends the stress phase, not the run.
			ReportStress(TEXT("ended_by_game_over"));
			AppendProgress(TEXT("STRESS_DONE game_over"));
			M_Progress.bStressDone = true;
			bM_TrackStress = false;
		}
		// Defeat or victory locks the game; an open sweep wave is retried in the reloaded world.
		ReloadMap(TEXT("game_over_lock"));
		return true;
	}

	void FAllGlobalAbilitiesTestRunner::ReloadMap(const FString& Reason)
	{
		UWorld* World = M_World.Get();
		const FString Map = M_Progress.MapPackage.IsEmpty() ? World->GetOutermost()->GetName() : M_Progress.MapPackage;
		Log(FString::Printf(TEXT("RELOAD_MAP reason=%s map=%s at=%.0fs"), *Reason, *Map, GetNow()));
		AppendProgress(FString::Printf(TEXT("RELOAD %s"), *Reason));
		M_Phase = ERunPhase::Done;
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
		UGameplayStatics::OpenLevel(World, FName(*Map));
	}

	void FAllGlobalAbilitiesTestRunner::ReadProgress()
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *M_ProgressFile);
		for (const FString& Line : Lines)
		{
			TArray<FString> Parts;
			Line.ParseIntoArrayWS(Parts);
			if (Parts.Num() == 0)
			{
				continue;
			}
			if (Parts[0] == TEXT("MAP") && Parts.Num() > 1)
			{
				M_Progress.MapPackage = Parts[1];
			}
			else if (Parts[0] == TEXT("WAVE_START") && Parts.Num() > 1)
			{
				Parts[1].ParseIntoArray(M_Progress.OpenWave, TEXT(","), true);
				for (const FString& Name : M_Progress.OpenWave)
				{
					M_Progress.WaveStarts.FindOrAdd(Name)++;
				}
			}
			else if (Parts[0] == TEXT("RESULT") && Parts.Num() > 2)
			{
				M_Progress.Finished.Add(Parts[2]);
				M_Progress.OpenWave.Remove(Parts[2]);
			}
			else if (Parts[0] == TEXT("SWEEP_DONE"))
			{
				M_Progress.bSweepDone = true;
			}
			else if (Parts[0] == TEXT("STRESS_DONE"))
			{
				M_Progress.bStressDone = true;
			}
			else if (Parts[0] == TEXT("TRAVEL_START"))
			{
				M_Progress.bTravelStarted = true;
			}
			else if (Parts[0] == TEXT("RUN_DONE"))
			{
				M_Progress.bRunDone = true;
			}
		}
	}

	void FAllGlobalAbilitiesTestRunner::PrepareRun()
	{
		UWorld* World = M_World.Get();
		ReadProgress();
		const bool bIsNewProcess = not GHasRunInThisProcess;
		GHasRunInThisProcess = true;
		const FString ThisMap = World->GetOutermost()->GetName();
		if (M_Progress.MapPackage.IsEmpty())
		{
			M_Progress.MapPackage = ThisMap;
			AppendProgress(TEXT("MAP ") + ThisMap);
		}
		else if (M_Progress.MapPackage != ThisMap)
		{
			// A mission's victory or defeat travelled elsewhere (menu, campaign): back to the map under test.
			ReloadMap(TEXT("wrong_map_") + ThisMap);
			return;
		}
		if (M_Progress.bRunDone)
		{
			Log(TEXT("RUN_ALREADY_DONE delete the progress file to run again"));
			FinishRun(TEXT("already_done"));
			return;
		}
		M_LandscapeBounds = FindLandscapeBounds();
		const APawn* CameraPawn = IsValid(GetController()) ? GetController()->GetPawn() : nullptr;
		const FVector CameraLocation = IsValid(CameraPawn) ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
		// Missions often start the camera at a map edge, where a ring of spots around it would be clamped onto one
		// point: the ring centre moves toward the middle of the map.
		const FVector ToCentre = M_LandscapeBounds.IsValid
			                         ? (M_LandscapeBounds.GetCenter() - CameraLocation).GetSafeNormal2D()
			                         : FVector::ZeroVector;
		M_Anchor = KeepInsideLandscape(CameraLocation + ToCentre * AnchorShiftTowardCentre,
		                               LandscapeEdgeMargin + SpotRingRadius);
		BindEndedDelegates();
		Log(FString::Printf(TEXT("START map=%s abilities=%d anchor=%s landscape=%s new_process=%d open_wave=%d"),
		                    *ThisMap, GetManager()->GetLoadedAbilities().Num(), *M_Anchor.ToCompactString(),
		                    *M_LandscapeBounds.ToString(), bIsNewProcess ? 1 : 0, M_Progress.OpenWave.Num()));
		AppendProgress(FString::Printf(TEXT("SESSION new_process=%d"), bIsNewProcess ? 1 : 0));

		// A wave left open: the process died under it (crash or hang) or the world ended under it (game over).
		for (const FString& Name : M_Progress.OpenWave)
		{
			const int32 Starts = M_Progress.WaveStarts.FindRef(Name);
			if (bIsNewProcess)
			{
				Result(TEXT("FAIL"), Name, FString::Printf(TEXT("crash_or_hang_while_running wave=[%s]"),
				                                           *FString::Join(M_Progress.OpenWave, TEXT(","))));
				M_Progress.Finished.Add(Name);
			}
			else if (Starts >= 2)
			{
				Result(TEXT("FAIL"), Name, TEXT("world_ended_twice_while_running"));
				M_Progress.Finished.Add(Name);
			}
		}
		if (M_Progress.bTravelStarted && not M_Progress.bRunDone)
		{
			if (bIsNewProcess)
			{
				Result(TEXT("FAIL"), TEXT("TRAVEL"), TEXT("crash_or_hang_during_reload_with_abilities_in_flight"));
				FinishRun(TEXT("travel_crashed"));
				return;
			}
			Log(TEXT("TRAVEL_ARRIVED"));
			AppendProgress(TEXT("TRAVEL_ARRIVED"));
			M_Phase = ERunPhase::AfterTravel;
			M_TravelStepStart = GetNow();
			return;
		}
		if (M_Progress.bStressDone && not M_Progress.bTravelStarted && bM_DoTravel)
		{
			if (bIsNewProcess)
			{
				// The stress phase ended but the process died before the travel started: retry the travel.
				Log(TEXT("STRESS_DONE_TRAVEL_PENDING"));
			}
			StartTravelInFlight();
			return;
		}
		if (bM_DoSweep && not M_Progress.bSweepDone)
		{
			BuildSweepQueue();
			M_Phase = ERunPhase::Sweep;
			return;
		}
		if (bM_DoStress && not M_Progress.bStressDone)
		{
			if (not M_Progress.Finished.Contains(TEXT("STRESS")))
			{
				StartStress();
				return;
			}
			// The stress phase was running when the process died (FAIL STRESS above); the previous log's last
			// STRESS_ACTION line names what it was doing.
			AppendProgress(TEXT("STRESS_DONE crashed"));
			M_Progress.bStressDone = true;
		}
		if (bM_DoTravel && bM_DoStress && not M_Progress.bTravelStarted)
		{
			StartTravelInFlight();
			return;
		}
		FinishRun(TEXT("complete"));
	}

	void FAllGlobalAbilitiesTestRunner::FinishRun(const FString& Summary)
	{
		M_Phase = ERunPhase::Done;
		UWorld* World = M_World.Get();
		if (IsValid(World))
		{
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
		}
		const FString Line = FString::Printf(TEXT("RUN_DONE %s pass=%d warn=%d fail=%d (this session)"), *Summary, M_PassCount,
		                                     M_WarnCount, M_FailCount);
		Log(Line);
		AppendProgress(Line);
		if (bM_ExitOnComplete)
		{
			FPlatformMisc::RequestExit(false);
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Player click path
	// ---------------------------------------------------------------------------------------------------------------

	ACPPController* FAllGlobalAbilitiesTestRunner::GetController() const
	{
		return FRTS_Statics::GetRTSController(M_World.Get());
	}

	UGlobalAbilitiesManager* FAllGlobalAbilitiesTestRunner::GetManager() const
	{
		const ACPPController* Controller = GetController();
		return IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	}

	UGlobalAbility* FAllGlobalAbilitiesTestRunner::FindLoadedAbility(const EGlobalAbility Type) const
	{
		const UGlobalAbilitiesManager* Manager = GetManager();
		return IsValid(Manager) ? Manager->FindLoadedAbilityByType(Type) : nullptr;
	}

	UW_GA_Item* FAllGlobalAbilitiesTestRunner::ShowAbilityInPanel(const UGlobalAbility& Ability) const
	{
		UGlobalAbilitiesManager* Manager = GetManager();
		ACPPController* Controller = GetController();
		UMainGameUI* MainUI = IsValid(Controller) ? Controller->GetMainMenuUI() : nullptr;
		UW_GlobalAbilityPanel* Panel = IsValid(MainUI) ? MainUI->GetGlobalAbilityPanel().Get() : nullptr;
		if (not IsValid(Manager) || not IsValid(Panel))
		{
			return nullptr;
		}
		const int32 AbilityIndex = Manager->GetLoadedAbilities().IndexOfByKey(&Ability);
		if (AbilityIndex == INDEX_NONE)
		{
			return nullptr;
		}
		const int32 Page = AbilityIndex / RTS::AllGlobalAbilities::PanelPageSize;
		if (Manager->GetAbilityPanelPage() != Page)
		{
			Manager->ShowAbilityPanelPage(Page);
		}
		for (UW_GA_Item* Item : Panel->GetAbilityItems())
		{
			if (IsValid(Item) && Item->GetLoadedAbility() == &Ability)
			{
				return Item;
			}
		}
		return nullptr;
	}

	UButton* FAllGlobalAbilitiesTestRunner::GetItemButton(const UW_GA_Item& Item) const
	{
		const FObjectProperty* Property = CastField<FObjectProperty>(
			UW_GA_Item::StaticClass()->FindPropertyByName(AbilityButtonPropertyName));
		const UW_CoolDownItem* CooldownItem = Property != nullptr
			                                      ? Cast<UW_CoolDownItem>(Property->GetObjectPropertyValue_InContainer(&Item))
			                                      : nullptr;
		return IsValid(CooldownItem) ? CooldownItem->GetButton() : nullptr;
	}

	bool FAllGlobalAbilitiesTestRunner::PressAbilityButton(UGlobalAbility& Ability, FString& OutProblem,
	                                                       bool& bOutExecutedOnPress) const
	{
		bOutExecutedOnPress = false;
		UW_GA_Item* Item = ShowAbilityInPanel(Ability);
		if (not IsValid(Item))
		{
			OutProblem = TEXT("no_panel_item");
			return false;
		}
		if (not Item->IsVisible())
		{
			OutProblem = TEXT("panel_item_hidden");
			return false;
		}
		UButton* Button = GetItemButton(*Item);
		if (not IsValid(Button))
		{
			OutProblem = TEXT("panel_item_has_no_button");
			return false;
		}
		if (not Button->GetIsEnabled())
		{
			OutProblem = TEXT("panel_button_disabled");
			return false;
		}
		// What a mouse click on the enabled button does.
		Button->OnClicked.Broadcast();
		if (GetKind(Ability) == EAbilityKind::Instant)
		{
			bOutExecutedOnPress = not FAllGlobalAbilitiesTestAccess::GetIsArmed(Ability);
			if (not bOutExecutedOnPress)
			{
				OutProblem = TEXT("instant_ability_stayed_armed");
				return false;
			}
			return true;
		}
		if (not FAllGlobalAbilitiesTestAccess::GetIsArmed(Ability))
		{
			OutProblem = TEXT("button_did_not_arm_ability");
			return false;
		}
		return true;
	}

	bool FAllGlobalAbilitiesTestRunner::FireAbility(UGlobalAbility& Ability, const FVector& Spot, const FVector& LineEnd,
	                                                bool& bOutIsStrip, FString& OutProblem) const
	{
		bool bExecutedOnPress = false;
		bOutIsStrip = false;
		if (not PressAbilityButton(Ability, OutProblem, bExecutedOnPress))
		{
			return false;
		}
		return bExecutedOnPress || ClickGround(Ability, Spot, LineEnd, bOutIsStrip, OutProblem);
	}

	bool FAllGlobalAbilitiesTestRunner::ClickGround(UGlobalAbility& Ability, const FVector& Spot, const FVector& LineEnd,
	                                                bool& bOutIsStrip, FString& OutProblem) const
	{
		ACPPController* Controller = GetController();
		bOutIsStrip = Ability.GetAimSettings().AreaAim.Shape == EGlobalAbilityAimShape::Strip;
		if (not IsValid(Controller) || not Controller->TryHandleActiveGlobalAbilityPrimaryClick(Spot))
		{
			OutProblem = TEXT("primary_click_not_taken");
			return false;
		}
		if (bOutIsStrip)
		{
			// The first click anchors the strip; the ability is still armed for the end click.
			if (not FAllGlobalAbilitiesTestAccess::GetHasArmedGlobalAbility(*Controller))
			{
				OutProblem = TEXT("strip_executed_on_first_click");
				return false;
			}
			Controller->TryHandleActiveGlobalAbilityPrimaryClick(LineEnd);
		}
		if (FAllGlobalAbilitiesTestAccess::GetHasArmedGlobalAbility(*Controller) || FAllGlobalAbilitiesTestAccess::GetIsArmed(Ability))
		{
			OutProblem = TEXT("still_armed_after_ground_click");
			Controller->TryCancelActiveGlobalAbility();
			return false;
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Sweep
	// ---------------------------------------------------------------------------------------------------------------

	void FAllGlobalAbilitiesTestRunner::BuildSweepQueue()
	{
		FString OnlyList;
		TArray<FString> OnlyNames;
		if (FParse::Value(FCommandLine::Get(), TEXT("RTSGATestOnly="), OnlyList))
		{
			OnlyList.ParseIntoArray(OnlyNames, TEXT("+"), true);
		}
		int32 Shard = 0;
		int32 ShardCount = 1;
		FParse::Value(FCommandLine::Get(), TEXT("RTSGATestShard="), Shard);
		FParse::Value(FCommandLine::Get(), TEXT("RTSGATestShardCount="), ShardCount);
		ShardCount = FMath::Max(1, ShardCount);
		const TArray<TObjectPtr<UGlobalAbility>>& Loaded = GetManager()->GetLoadedAbilities();
		for (int32 Index = 0; Index < Loaded.Num(); ++Index)
		{
			const UGlobalAbility* Ability = Loaded[Index];
			if (not IsValid(Ability) || Index % ShardCount != Shard)
			{
				continue;
			}
			const FString Name = GetAbilityName(Ability->GetAbilityType());
			if ((OnlyNames.Num() > 0 && not OnlyNames.Contains(Name)) || M_Progress.Finished.Contains(Name))
			{
				continue;
			}
			M_SweepQueue.Add(Ability->GetAbilityType());
		}
		Log(FString::Printf(TEXT("SWEEP_QUEUE %d abilities left (shard %d/%d, wave %d)"), M_SweepQueue.Num(), Shard,
		                    ShardCount, M_WaveSize));
	}

	void FAllGlobalAbilitiesTestRunner::TickSweep()
	{
		if (M_WaveStage == EWaveStage::None)
		{
			if (M_SweepQueue.Num() == 0)
			{
				AppendProgress(TEXT("SWEEP_DONE"));
				M_Progress.bSweepDone = true;
				Log(FString::Printf(TEXT("SWEEP_DONE pass=%d warn=%d fail=%d"), M_PassCount, M_WarnCount, M_FailCount));
				if (bM_DoStress && not M_Progress.bStressDone)
				{
					StartStress();
					return;
				}
				if (bM_DoTravel && bM_DoStress)
				{
					StartTravelInFlight();
					return;
				}
				FinishRun(TEXT("sweep_complete"));
				return;
			}
			StartWave();
			return;
		}
		if (M_WaveStage == EWaveStage::WaitingForEnd)
		{
			TickWaveWaiting();
			return;
		}
		// Every case was judged after its own end; a short pause lets late error lines of the wave arrive.
		constexpr float ReportDelaySeconds = 2.f;
		if (GetNow() - M_StageStartSeconds >= ReportDelaySeconds)
		{
			EvaluateWave();
			CleanupWave();
		}
	}

	void FAllGlobalAbilitiesTestRunner::CacheOwnUnitLocations()
	{
		M_OwnUnitLocations.Reset();
		for (TActorIterator<AActor> It(M_World.Get()); It; ++It)
		{
			if ((It->IsA<APawn>() || It->IsA<AHPActorObjectsMaster>()) && not It->IsA<AAircraftMaster>()
				&& GetOwnerIndex(*It) == PlayerIndex)
			{
				M_OwnUnitLocations.Add(It->GetActorLocation());
			}
		}
	}

	bool FAllGlobalAbilitiesTestRunner::GetIsNearOwnUnit(const FVector& Location) const
	{
		for (const FVector& OwnUnit : M_OwnUnitLocations)
		{
			if (FVector::DistSquared2D(OwnUnit, Location) < FMath::Square(MinSpotDistanceToOwnUnits))
			{
				return true;
			}
		}
		return false;
	}

	bool FAllGlobalAbilitiesTestRunner::FindSpot(const int32 SpotIndex, FVector& OutSpot) const
	{
		// Spots on rings around the camera, far enough apart that each payload can be told from the others.
		const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(M_World.Get());
		for (int32 Ring = 0; Ring < 4; ++Ring)
		{
			for (int32 Try = 0; Try < 8; ++Try)
			{
				const float Angle = 2.f * PI * (SpotIndex + Try * 0.37f) / FMath::Max(1, M_WaveSize) + Ring * 0.6f;
				const float Radius = SpotRingRadius + Ring * SpotRingStep;
				const FVector Candidate = M_Anchor + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
				FVector Ground = KeepInsideLandscape(Candidate, LandscapeEdgeMargin);
				if (not GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), Ground, Ground))
				{
					continue;
				}
				FNavLocation NavLocation;
				if (IsValid(NavSystem) && not NavSystem->ProjectPointToNavigation(Ground, NavLocation, FVector(400.f, 400.f, 800.f)))
				{
					continue;
				}
				// Strikes and strike targets next to the player's own units lose missions (a dead commander
				// vehicle is a defeat); the first rings must keep their distance, the last ring takes what it gets.
				if (Ring < 3 && GetIsNearOwnUnit(Ground))
				{
					continue;
				}
				OutSpot = Ground;
				return true;
			}
		}
		// Separate fallbacks, so strike targets of one wave never spawn inside each other.
		constexpr float FallbackSpotSpacing = 2000.f;
		OutSpot = KeepInsideLandscape(M_Anchor + FVector(SpotIndex * FallbackSpotSpacing, 0.f, 0.f), LandscapeEdgeMargin);
		return false;
	}

	FVector FAllGlobalAbilitiesTestRunner::GetLineEnd(const FVector& Spot, const UGlobalAbility& Ability) const
	{
		// The player's second click: the run continues away from the camera.
		FVector Direction = (Spot - M_Anchor).GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = FVector(1.f, 0.f, 0.f);
		}
		const FGlobalAbilityAreaAimSettings& AreaAim = Ability.GetAimSettings().AreaAim;
		const float Length = FMath::Clamp(AreaAim.MaxStripLength * 0.8f, AreaAim.MinStripLength, AreaAim.MaxStripLength);
		FVector End = KeepInsideLandscape(Spot + Direction * Length, LandscapeEdgeMargin);
		// Near the map edge the clamp can pull the end back onto the spot; the aim then picks its own direction and the
		// payload check would look along a line the ability never used. Run toward the middle of the map instead.
		if (FVector::Dist2D(End, Spot) < AreaAim.MinStripLength && M_LandscapeBounds.IsValid)
		{
			const FVector ToCentre = (M_LandscapeBounds.GetCenter() - Spot).GetSafeNormal2D();
			End = KeepInsideLandscape(Spot + ToCentre * Length, LandscapeEdgeMargin);
		}
		// The playable ground need not fill the landscape's bounding box: like a player, end the strip on playable
		// ground (the click handler refuses a point with neither landscape below nor navmesh close by) and no shorter
		// than the minimum (the aim lengthens a shorter strip, which can carry the end off it again). Other directions
		// are tried when the first one leaves the playable ground too soon.
		constexpr float StepBackCm = 500.f;
		constexpr int32 DirectionCount = 8;
		const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(M_World.Get());
		auto GetIsPlayable = [this, NavSystem](const FVector& Location)
		{
			FVector Ground;
			FNavLocation NavLocation;
			return GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), Location, Ground)
				|| (NavSystem && NavSystem->ProjectPointToNavigation(Location, NavLocation, FVector(500.f, 500.f, 2000.f)));
		};
		const FVector FirstDirection = (End - Spot).GetSafeNormal2D();
		const float FirstLength = FMath::Max(FVector::Dist2D(End, Spot), AreaAim.MinStripLength);
		bool bFoundPlayableEnd = false;
		for (int32 DirectionIndex = 0; DirectionIndex < DirectionCount && not bFoundPlayableEnd; ++DirectionIndex)
		{
			const FVector TryDirection = FirstDirection.RotateAngleAxis(360.f / DirectionCount * DirectionIndex,
			                                                            FVector::UpVector);
			for (float TryLength = FirstLength; TryLength >= AreaAim.MinStripLength; TryLength -= StepBackCm)
			{
				const FVector Candidate = Spot + TryDirection * TryLength;
				if (GetIsPlayable(Candidate))
				{
					End = Candidate;
					bFoundPlayableEnd = true;
					break;
				}
			}
		}
		GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), End, End);
		return End;
	}

	AActor* FAllGlobalAbilitiesTestRunner::SpawnStrikeTarget(const FVector& Spot, const bool bSoftTarget,
	                                                         const uint8 TargetOwner)
	{
		UWorld* World = M_World.Get();
		UClass* TargetClass = bSoftTarget ? LoadClass<AActor>(nullptr, SoftStrikeTargetClassPath) : nullptr;
		if (not IsValid(TargetClass))
		{
			TargetClass = LoadClass<AActor>(nullptr, StrikeTargetClassPath);
		}
		if (not IsValid(World) || not IsValid(TargetClass))
		{
			return nullptr;
		}
		AActor* Target = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
			*World, TargetClass, FTransform(Spot + FVector(0.f, 0.f, TargetSpawnAboveGroundCm)), TargetOwner);
		M_WaveSpawned.RemoveAll([Target](const TWeakObjectPtr<AActor>& Spawned) { return Spawned.Get() == Target; });
		return Target;
	}

	void FAllGlobalAbilitiesTestRunner::StartWave()
	{
		M_Wave.Reset();
		M_WaveSpawned.Reset();
		M_BombLastSeen.Reset();
		M_BombImpacts.Reset();
		TArray<FString> Names;
		while (M_Wave.Num() < M_WaveSize && M_SweepQueue.Num() > 0)
		{
			const EGlobalAbility Type = M_SweepQueue[0];
			M_SweepQueue.RemoveAt(0);
			UGlobalAbility* Ability = FindLoadedAbility(Type);
			if (not IsValid(Ability))
			{
				Result(TEXT("FAIL"), GetAbilityName(Type), TEXT("not_loaded_by_player_manager"));
				continue;
			}
			FWaveCase& Case = M_Wave.AddDefaulted_GetRef();
			Case.Ability = Ability;
			Case.Type = Type;
			Case.Kind = GetKind(*Ability);
			Names.Add(GetAbilityName(Type));
		}
		if (M_Wave.Num() == 0)
		{
			return;
		}
		AppendProgress(TEXT("WAVE_START ") + FString::Join(Names, TEXT(",")));
		CacheOwnUnitLocations();
		TArray<FString> DiscardedSamples;
		M_ErrorCounter->TakeErrors(DiscardedSamples);
		for (int32 Index = 0; Index < M_Wave.Num(); ++Index)
		{
			FWaveCase& Case = M_Wave[Index];
			if (not FindSpot(Index, Case.Spot))
			{
				Case.Notes.Add(TEXT("no_navigable_spot_found_used_camera"));
			}
			Case.LineEnd = GetLineEnd(Case.Spot, *Case.Ability);
			if (Case.Kind == EAbilityKind::Strike)
			{
				// Machine guns, light cannon and fire do not get through a T-34: those strikes get the soft truck.
				const bool bSoftTarget = Case.Ability->IsA<UGA_AircraftStrafing>()
					|| Case.Ability->IsA<UGA_IncendiaryAirStrike>();
				// The PE-8 bombers are Soviet Blueprints owned by the enemy (these are enemy AI abilities, the aircraft
				// keeps its Blueprint owner), and bomb splash only hurts the other side of the bomb's owner: their
				// target is the player's, or a hit could never show.
				const bool bEnemyOwnedBomber = GetAbilityName(Case.Type).Contains(TEXT("_PE8_"));
				AActor* Target = SpawnStrikeTarget(Case.Spot, bSoftTarget, bEnemyOwnedBomber ? PlayerIndex : EnemyIndex);
				Case.StrikeTarget = Target;
				Case.StrikeTargetStartHealth = GetHealth(Target);
				if (not IsValid(Target))
				{
					Case.Warns.Add(TEXT("strike_target_not_spawned"));
				}
			}
		}
		// Fire them one after another as a player would: button, ground click (two for strips).
		for (FWaveCase& Case : M_Wave)
		{
			M_EndedCounts.FindOrAdd(Case.Ability) = 0;
			FString Problem;
			if (not FireAbility(*Case.Ability, Case.Spot, Case.LineEnd, Case.bIsStrip, Problem))
			{
				Case.Fails.Add(Problem);
				Case.Notes.Add(FString::Printf(TEXT("clicked=%s line_end=%s"), *Case.Spot.ToCompactString(),
				                               *Case.LineEnd.ToCompactString()));
				continue;
			}
			if (Case.Kind == EAbilityKind::Instant)
			{
				Case.EndedAtSeconds = GetNow();
			}
			Log(FString::Printf(TEXT("FIRED %s kind=%s class=%s spot=%s%s"), *GetAbilityName(Case.Type), GetKindName(Case.Kind),
			                    *Case.Ability->GetClass()->GetName(), *Case.Spot.ToCompactString(),
			                    Case.bIsStrip ? *(TEXT(" strip_end=") + Case.LineEnd.ToCompactString()) : TEXT("")));
		}
		M_WaveStage = EWaveStage::WaitingForEnd;
		M_WaveStartSeconds = GetNow();
		M_StageStartSeconds = GetNow();
	}

	void FAllGlobalAbilitiesTestRunner::TickWaveWaiting()
	{
		const bool bTimedOut = GetNow() - M_WaveStartSeconds >= AbilityTimeoutSeconds;
		TrackBombImpacts();
		bool bAllEvaluated = true;
		for (FWaveCase& Case : M_Wave)
		{
			if (Case.bEvaluated)
			{
				continue;
			}
			if (Case.Fails.Num() == 0 && Case.Kind != EAbilityKind::Instant)
			{
				Case.EndedCount = M_EndedCounts.FindRef(Case.Ability);
				if (Case.EndedCount > 0 && Case.EndedAtSeconds < 0.f)
				{
					Case.EndedAtSeconds = GetNow();
					Log(FString::Printf(TEXT("ENDED %s after=%.1fs"), *GetAbilityName(Case.Type), GetNow() - M_WaveStartSeconds));
				}
			}
			if (Case.Kind == EAbilityKind::Drop)
			{
				TrackArrivals(Case);
			}
			const bool bSettled = Case.EndedAtSeconds >= 0.f && GetNow() - Case.EndedAtSeconds >= SettleSeconds;
			if (Case.Fails.Num() > 0 || bSettled || bTimedOut)
			{
				EvaluateCase(Case, GetLiveWaveSpawned());
				Case.bEvaluated = true;
				continue;
			}
			bAllEvaluated = false;
		}
		if (bAllEvaluated)
		{
			M_WaveStage = EWaveStage::Settling;
			M_StageStartSeconds = GetNow();
		}
	}

	void FAllGlobalAbilitiesTestRunner::ScanForOrphanedSoldiers()
	{
		constexpr double ScanIntervalSeconds = 0.5;
		constexpr int32 MaxReportedOrphans = 12;
		const double Now = GetRealNow();
		if (Now - M_LastOrphanScanReal < ScanIntervalSeconds || not M_World.IsValid())
		{
			return;
		}
		M_LastOrphanScanReal = Now;
		for (TActorIterator<ASquadUnit> It(M_World.Get()); It; ++It)
		{
			ASquadUnit* SquadUnit = *It;
			if (not IsValid(SquadUnit))
			{
				continue;
			}
			// Dead soldiers count too: a corpse that still runs code needing its squad logs the same orphan errors.
			const bool bIsAlive = SquadUnit->IsUnitAlive();
			const TWeakObjectPtr<ASquadUnit> WeakUnit(SquadUnit);
			// A soldier far under the lowest landscape fell out of the world (no ground below, or no collision).
			constexpr float FallenBelowLandscapeCm = 2000.f;
			if (bIsAlive && M_LandscapeBounds.IsValid && SquadUnit->GetActorLocation().Z < M_LandscapeBounds.Min.Z - FallenBelowLandscapeCm
				&& not M_ReportedFallenUnits.Contains(WeakUnit) && M_ReportedFallenUnits.Num() < MaxReportedOrphans)
			{
				M_ReportedFallenUnits.Add(WeakUnit);
				ASquadController* FallenController = FAllGlobalAbilitiesTestAccess::GetRawSquadController(*SquadUnit);
				// What is under the soldier's X/Y: the landscape (hole?) and the first thing a visibility trace hits.
				const FVector UnitLocation = SquadUnit->GetActorLocation();
				FVector LandscapeGround;
				const bool bHasLandscape = GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), UnitLocation,
				                                                                       LandscapeGround);
				FHitResult VisibilityHit;
				FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(RTSGATestFallenUnitGround), false, SquadUnit);
				const bool bHasVisibilityHit = M_World->LineTraceSingleByChannel(
					VisibilityHit, FVector(UnitLocation.X, UnitLocation.Y, M_LandscapeBounds.Max.Z + 5000.f),
					FVector(UnitLocation.X, UnitLocation.Y, M_LandscapeBounds.Min.Z - 5000.f), ECC_Visibility,
					VisibilityParams);
				Result(TEXT("FAIL"), TEXT("UNIT_FELL_OUT_OF_WORLD"), FString::Printf(
					       TEXT("unit=%s at=%s landscape_min_z=%.0f squad=%s hidden=%d collision=%d phase=%d "
					            "landscape_below=%s first_visibility_hit=%s"),
					       *SquadUnit->GetName(), *SquadUnit->GetActorLocation().ToCompactString(),
					       M_LandscapeBounds.Min.Z,
					       IsValid(FallenController) ? *FallenController->GetClass()->GetName() : TEXT("none"),
					       SquadUnit->IsHidden() ? 1 : 0, SquadUnit->GetActorEnableCollision() ? 1 : 0,
					       static_cast<int32>(M_Phase),
					       bHasLandscape ? *FString::Printf(TEXT("z%.0f"), LandscapeGround.Z) : TEXT("none"),
					       bHasVisibilityHit
						       ? *FString::Printf(TEXT("%s@z%.0f"), *GetNameSafe(VisibilityHit.GetActor()), VisibilityHit.ImpactPoint.Z)
						       : TEXT("none")));
			}
			ASquadController* Controller = FAllGlobalAbilitiesTestAccess::GetRawSquadController(*SquadUnit);
			if (IsValid(Controller))
			{
				FSoldierControllerSample& Sample = M_SoldierControllers.FindOrAdd(WeakUnit);
				if (Sample.Controller.Get() != Controller)
				{
					Sample.Controller = Controller;
					Sample.ControllerName = Controller->GetName();
					Sample.ControllerClass = Controller->GetClass()->GetName();
				}
				const TArray<ASquadUnit*>& Units = FAllGlobalAbilitiesTestAccess::GetSquadUnitList(*Controller);
				Sample.bWasInControllerList = Units.Contains(SquadUnit);
				Sample.ControllerListSize = Units.Num();
				continue;
			}
			const FSoldierControllerSample* Sample = M_SoldierControllers.Find(WeakUnit);
			// A dead soldier outlives its squad by design (corpse); it only matters when it logs errors (below).
			if (not bIsAlive || Sample == nullptr || M_ReportedOrphans.Contains(WeakUnit)
				|| M_ReportedOrphans.Num() >= MaxReportedOrphans)
			{
				continue;
			}
			M_ReportedOrphans.Add(WeakUnit);
			Result(TEXT("FAIL"), TEXT("ORPHAN_SOLDIER"), FString::Printf(
				       TEXT("unit=%s alive=%d hidden=%d at=%s lost_controller=%s class=%s was_in_its_list=%d its_list_size=%d "
				            "phase=%d last_stress_action=%s(%.1fs_ago)"),
				       *SquadUnit->GetName(), bIsAlive ? 1 : 0, SquadUnit->IsHidden() ? 1 : 0,
				       *SquadUnit->GetActorLocation().ToCompactString(), *Sample->ControllerName,
				       *Sample->ControllerClass, Sample->bWasInControllerList ? 1 : 0, Sample->ControllerListSize,
				       static_cast<int32>(M_Phase), *M_LastStressAction, Now - M_LastStressActionReal));
		}
		for (const FString& ObjectName : M_ErrorCounter->TakeNewOrphanErrorObjects())
		{
			ASquadUnit* Found = nullptr;
			for (TActorIterator<ASquadUnit> It(M_World.Get()); It; ++It)
			{
				if (It->GetName() == ObjectName)
				{
					Found = *It;
					break;
				}
			}
			const FSoldierControllerSample* Sample = Found ? M_SoldierControllers.Find(Found) : nullptr;
			Result(TEXT("FAIL"), TEXT("ORPHAN_ERRORS"), FString::Printf(
				       TEXT("unit=%s found=%d alive=%d hidden=%d at=%s last_controller=%s class=%s was_in_its_list=%d "
				            "phase=%d last_stress_action=%s(%.1fs_ago)"),
				       *ObjectName, Found ? 1 : 0, Found && Found->IsUnitAlive() ? 1 : 0,
				       Found && Found->IsHidden() ? 1 : 0,
				       Found ? *Found->GetActorLocation().ToCompactString() : TEXT("-"),
				       Sample ? *Sample->ControllerName : TEXT("never_sampled"),
				       Sample ? *Sample->ControllerClass : TEXT("-"), Sample && Sample->bWasInControllerList ? 1 : 0,
				       static_cast<int32>(M_Phase), *M_LastStressAction, Now - M_LastStressActionReal));
		}
	}

	void FAllGlobalAbilitiesTestRunner::StartGroundRecoveryChecks()
	{
		bM_GroundChecksStarted = true;
		UWorld* World = M_World.Get();
		const AWorldSettings* WorldSettings = IsValid(World) ? World->GetWorldSettings() : nullptr;
		UClass* TankClass = LoadClass<AActor>(nullptr, StrikeTargetClassPath);
		if (not IsValid(WorldSettings) || not IsValid(TankClass) || WorldSettings->KillZ < -0.5f * UE_OLD_HALF_WORLD_MAX1)
		{
			Log(TEXT("GROUND_RECOVERY skipped (no tank class or no landscape KillZ on this map)"));
			return;
		}
		constexpr double DropAfterSeconds = 3.0;
		constexpr double EvaluateAfterSeconds = 20.0;
		constexpr float UnderSurfaceCm = 100.f;
		const double Now = GetRealNow();
		auto SpawnTank = [this, World, TankClass](const FVector& Offset, const float BelowSurface) -> AActor*
		{
			FVector Ground = KeepInsideLandscape(M_Anchor + Offset, LandscapeEdgeMargin);
			GlobalAbilityHelpers::TryFindGroundLocation(World, Ground, Ground);
			AActor* Tank = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
				*World, TankClass, FTransform(Ground - FVector(0.f, 0.f, BelowSurface)), PlayerIndex);
			M_WaveSpawned.RemoveAll([Tank](const TWeakObjectPtr<AActor>& Spawned) { return Spawned.Get() == Tank; });
			return Tank;
		};
		FGroundRecoveryCheck& Underground = M_GroundChecks.AddDefaulted_GetRef();
		Underground.Label = TEXT("tank_spawned_inside_landscape");
		Underground.Unit = SpawnTank(FVector(2500.f, -2500.f, 0.f), UnderSurfaceCm);
		Underground.EvaluateAtReal = Now + EvaluateAfterSeconds;

		// A second spot: tells a problem of one place on the map from one of the recovery.
		FGroundRecoveryCheck& UndergroundElsewhere = M_GroundChecks.AddDefaulted_GetRef();
		UndergroundElsewhere.Label = TEXT("tank_spawned_inside_landscape_spot2");
		UndergroundElsewhere.Unit = SpawnTank(FVector(-4000.f, -4000.f, 0.f), UnderSurfaceCm);
		UndergroundElsewhere.EvaluateAtReal = Now + EvaluateAfterSeconds;

		FGroundRecoveryCheck& FallenTank = M_GroundChecks.AddDefaulted_GetRef();
		FallenTank.Label = TEXT("tank_below_killz");
		FallenTank.Unit = SpawnTank(FVector(-2500.f, 2500.f, 0.f), -TargetSpawnAboveGroundCm);
		FallenTank.DropAtReal = Now + DropAfterSeconds;
		FallenTank.EvaluateAtReal = Now + DropAfterSeconds + EvaluateAfterSeconds;

		// A living, visible soldier of the player (the map's own units: the mission's starting squads).
		ASquadUnit* Soldier = nullptr;
		for (TActorIterator<ASquadUnit> It(World); It; ++It)
		{
			if (IsValid(*It) && It->IsUnitAlive() && not It->IsHidden() && GetOwnerIndex(*It) == PlayerIndex)
			{
				Soldier = *It;
				break;
			}
		}
		FGroundRecoveryCheck& FallenSoldier = M_GroundChecks.AddDefaulted_GetRef();
		FallenSoldier.Label = TEXT("soldier_below_killz");
		FallenSoldier.Unit = Soldier;
		FallenSoldier.DropAtReal = Now + DropAfterSeconds;
		FallenSoldier.EvaluateAtReal = Now + DropAfterSeconds + EvaluateAfterSeconds;
		Log(FString::Printf(TEXT("GROUND_RECOVERY started killz=%.0f soldier=%s"), WorldSettings->KillZ,
		                    *GetNameSafe(Soldier)));
	}

	void FAllGlobalAbilitiesTestRunner::TickGroundRecoveryChecks()
	{
		UWorld* World = M_World.Get();
		if (not IsValid(World))
		{
			return;
		}
		const double Now = GetRealNow();
		for (FGroundRecoveryCheck& Check : M_GroundChecks)
		{
			if (Check.bDone)
			{
				continue;
			}
			AActor* Unit = Check.Unit.Get();
			if (not IsValid(Unit))
			{
				Check.bDone = true;
				Result(TEXT("FAIL"), TEXT("GROUND_RECOVERY_") + Check.Label, TEXT("unit_missing_or_destroyed"));
				continue;
			}
			if (Check.DropAtReal >= 0.0 && Now >= Check.DropAtReal)
			{
				// Straight below the KillZ, as if it had fallen through the world.
				Check.DropAtReal = -1.0;
				const FVector Location = Unit->GetActorLocation();
				Unit->SetActorLocation(FVector(Location.X, Location.Y, World->GetWorldSettings()->KillZ - 200.f), false,
				                       nullptr, ETeleportType::ResetPhysics);
				Log(TEXT("GROUND_RECOVERY dropped ") + Check.Label + TEXT(" ") + Unit->GetName());
				continue;
			}
			if (Check.DropAtReal >= 0.0 || Now < Check.EvaluateAtReal)
			{
				continue;
			}
			Check.bDone = true;
			const FVector Location = Unit->GetActorLocation();
			FVector Ground = Location;
			const bool bHasGround = GlobalAbilityHelpers::TryFindGroundLocation(World, Location, Ground);
			// Standing on it: the origin is above the surface and not floating far over it.
			constexpr float MaxOriginAboveGroundCm = 600.f;
			const float AboveGround = static_cast<float>(Location.Z - Ground.Z);
			const bool bOnGround = bHasGround && AboveGround >= 0.f && AboveGround <= MaxOriginAboveGroundCm;
			const bool bAlive = Unit->IsUnitAlive();
			Result(bOnGround && bAlive ? TEXT("PASS") : TEXT("FAIL"), TEXT("GROUND_RECOVERY_") + Check.Label,
			       FString::Printf(TEXT("unit=%s alive=%d at=%s ground_z=%s origin_above_ground=%.0f"), *Unit->GetName(),
			                       bAlive ? 1 : 0, *Location.ToCompactString(),
			                       bHasGround ? *FString::Printf(TEXT("%.0f"), Ground.Z) : TEXT("none"), AboveGround));
			if (not Unit->IsA<ASquadUnit>())
			{
				// The test tanks would stand in the way of later waves.
				KillActor(Unit);
			}
		}
	}

	void FAllGlobalAbilitiesTestRunner::TrackBombImpacts()
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : M_WaveSpawned)
		{
			const AActor* Bomb = WeakActor.Get();
			if (not IsValid(Bomb) || not Bomb->IsA<ABombActor>())
			{
				continue;
			}
			if (not Bomb->IsHidden())
			{
				M_BombLastSeen.FindOrAdd(WeakActor) = Bomb->GetActorLocation();
				continue;
			}
			// Hidden again after a fall: that fall ended where it was last seen.
			if (const FVector* LastSeen = M_BombLastSeen.Find(WeakActor))
			{
				M_BombImpacts.Add(*LastSeen);
				M_BombLastSeen.Remove(WeakActor);
			}
		}
	}

	bool FAllGlobalAbilitiesTestRunner::GetIsPayloadCandidate(const AActor& Actor)
	{
		// A squad controller stays where it was spawned (at the aircraft); its soldiers are what lands.
		if (Actor.IsA<AAircraftMaster>() || Actor.IsA<ASquadController>())
		{
			return false;
		}
		if (Actor.IsA<AParachutedResourcePayload>())
		{
			return true;
		}
		return Actor.FindComponentByClass<URTSComponent>() != nullptr
			&& (Actor.IsA<APawn>() || Actor.IsA<AHPActorObjectsMaster>());
	}

	void FAllGlobalAbilitiesTestRunner::TrackArrivals(FWaveCase& Case) const
	{
		for (const TWeakObjectPtr<AActor>& WeakActor : M_WaveSpawned)
		{
			const AActor* Actor = WeakActor.Get();
			if (not IsValid(Actor) || Case.Arrived.Contains(WeakActor) || not GetIsPayloadCandidate(*Actor)
				|| GetDistanceToAim(Case, Actor->GetActorLocation()) > PayloadRadius)
			{
				continue;
			}
			const bool bIsResource = Actor->IsA<AParachutedResourcePayload>();
			if (not bIsResource && not GetIsOnGround(*Actor))
			{
				// Still under a canopy or hanging under the aircraft.
				continue;
			}
			Case.Arrived.Add(WeakActor);
			Case.ArrivedClasses.Add(Actor->GetClass()->GetName());
			if (bIsResource || GetOwnerIndex(Actor) == PlayerIndex)
			{
				++Case.ArrivedPlayerOwned;
			}
			else
			{
				++Case.ArrivedOtherOwned;
			}
		}
	}

	FString FAllGlobalAbilitiesTestRunner::DescribeNearestWaveUnit(const FWaveCase& Case,
	                                                                 const TArray<AActor*>& WaveSpawned) const
	{
		// Where did the payload go instead? The closest unit the wave spawned, of any kind.
		const AActor* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();
		for (const AActor* Actor : WaveSpawned)
		{
			if (not IsValid(Actor) || Actor->IsA<AAircraftMaster>() || Actor->FindComponentByClass<URTSComponent>() == nullptr)
			{
				continue;
			}
			const float Distance = GetDistanceToAim(Case, Actor->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = Actor;
			}
		}
		if (Nearest == nullptr)
		{
			return TEXT("nearest_wave_unit=none_alive");
		}
		FVector Ground = Nearest->GetActorLocation();
		GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), Nearest->GetActorLocation(), Ground);
		return FString::Printf(TEXT("nearest_wave_unit=%s,dist=%.0f,owner=%d,above_ground=%.0f"),
		                       *Nearest->GetClass()->GetName(), NearestDistance, GetOwnerIndex(Nearest),
		                       Nearest->GetActorLocation().Z - Ground.Z);
	}

	TArray<AActor*> FAllGlobalAbilitiesTestRunner::GetLiveWaveSpawned() const
	{
		TArray<AActor*> Spawned;
		for (const TWeakObjectPtr<AActor>& Actor : M_WaveSpawned)
		{
			if (Actor.IsValid())
			{
				Spawned.Add(Actor.Get());
			}
		}
		return Spawned;
	}

	void FAllGlobalAbilitiesTestRunner::EvaluateWave()
	{
		TArray<FString> ErrorSamples;
		const int32 Errors = M_ErrorCounter->TakeErrors(ErrorSamples);
		for (FWaveCase& Case : M_Wave)
		{
			if (Errors > 0)
			{
				Case.Warns.Add(FString::Printf(TEXT("wave_logged_%d_errors[%s]"), Errors,
				                               *FString::Join(ErrorSamples, TEXT(" | "))));
			}
			const FString Verdict = Case.Fails.Num() > 0 ? TEXT("FAIL") : Case.Warns.Num() > 0 ? TEXT("WARN") : TEXT("PASS");
			Result(Verdict, GetAbilityName(Case.Type), FString::Printf(
				       TEXT("kind=%s class=%s ended=%d after=%.0fs fails=[%s] warns=[%s] notes=[%s]"), GetKindName(Case.Kind),
				       *Case.Ability->GetClass()->GetName(), Case.EndedCount,
				       Case.EndedAtSeconds >= 0.f ? Case.EndedAtSeconds - M_WaveStartSeconds : -1.f,
				       *FString::Join(Case.Fails, TEXT(";")), *FString::Join(Case.Warns, TEXT(";")),
				       *FString::Join(Case.Notes, TEXT(";"))));
		}
	}

	void FAllGlobalAbilitiesTestRunner::EvaluateCase(FWaveCase& Case, const TArray<AActor*>& WaveSpawned)
	{
		if (Case.Fails.Num() > 0)
		{
			// It never executed; nothing else to judge.
			return;
		}
		if (Case.Kind != EAbilityKind::Instant)
		{
			if (Case.EndedCount == 0)
			{
				Case.Fails.Add(FString::Printf(TEXT("never_ended_within_%.0fs"), AbilityTimeoutSeconds));
				// What is still flying tells why: an aircraft stuck in a landing state, one that never got an order...
				for (AActor* Actor : WaveSpawned)
				{
					if (const AAircraftMaster* Aircraft = Cast<AAircraftMaster>(Actor))
					{
						Case.Notes.Add(FString::Printf(TEXT("live_aircraft=%s@%s,landed_state=%s,owner=%d"),
						                               *Aircraft->GetClass()->GetName(),
						                               *Aircraft->GetActorLocation().ToCompactString(),
						                               *UEnum::GetValueAsString(Aircraft->GetLandedState()),
						                               GetOwnerIndex(Aircraft)));
					}
				}
			}
			else if (Case.EndedCount > 1)
			{
				Case.Fails.Add(FString::Printf(TEXT("ended_%d_times"), Case.EndedCount));
			}
		}
		if (Case.Kind == EAbilityKind::Drop)
		{
			TrackArrivals(Case);
			const bool bIsSovietAbility = GetAbilityName(Case.Type).Contains(TEXT("Soviet"));
			if (Case.ArrivedPlayerOwned == 0)
			{
				if (Case.Ability->IsA<UGA_ResourceDrop>())
				{
					// The crate may be collected before it is seen; reported, not failed.
					Case.Warns.Add(TEXT("no_resource_payload_near_spot"));
				}
				else if (bIsSovietAbility && Case.ArrivedOtherOwned > 0)
				{
					// Payload spawned through the async spawner keeps its Blueprint's owner: Soviet Blueprints belong
					// to the enemy, and the Soviet abilities are the enemy AI's. Known limitation, not failed.
					Case.Warns.Add(FString::Printf(TEXT("payload_not_player_owned=%d(soviet_blueprint_owner)"),
					                               Case.ArrivedOtherOwned));
				}
				else
				{
					Case.Fails.Add(TEXT("no_payload_near_spot"));
					Case.Notes.Add(DescribeNearestWaveUnit(Case, WaveSpawned));
					// Delivered but killed before it was counted (an aim point next to enemy armour): corpses near the spot.
					int32 DeadNearSpot = 0;
					for (const TWeakObjectPtr<AActor>& WeakActor : M_WaveSpawned)
					{
						ASquadUnit* SquadUnit = Cast<ASquadUnit>(WeakActor.Get());
						if (IsValid(SquadUnit) && not SquadUnit->IsUnitAlive()
							&& GetDistanceToAim(Case, SquadUnit->GetActorLocation()) <= PayloadRadius)
						{
							++DeadNearSpot;
						}
					}
					Case.Notes.Add(FString::Printf(TEXT("dead_soldiers_near_spot=%d"), DeadNearSpot));
				}
			}
			// Enemy units the map spawns nearby are not the ability's; only counted.
			Case.Notes.Add(FString::Printf(TEXT("arrived=%d[%s]other_owned=%d"), Case.ArrivedPlayerOwned,
			                               *FString::Join(Case.ArrivedClasses.Array(), TEXT(",")), Case.ArrivedOtherOwned));
		}
		if (Case.Kind == EAbilityKind::Strike)
		{
			AActor* Target = Case.StrikeTarget.Get();
			const float HealthNow = IsValid(Target) ? GetHealth(Target) : 0.f;
			const bool bDamaged = not IsValid(Target) || HealthNow < Case.StrikeTargetStartHealth;
			Case.Notes.Add(FString::Printf(TEXT("target_hp=%.0f->%.0f%s"), Case.StrikeTargetStartHealth, HealthNow,
			                               IsValid(Target) ? TEXT("") : TEXT("(dead)")));
			if (IsValid(Target))
			{
				// An untouched target that drove off the aim point was out of the strike, not missed by it.
				Case.Notes.Add(FString::Printf(TEXT("target_moved=%.0f"),
				                               FVector::Dist2D(Target->GetActorLocation(), Case.Spot)));
			}
			if (Case.Ability->IsA<UGA_AircraftBombing>())
			{
				// Bombs of the whole wave (several bombers may share it): the closest impact to this aim point.
				float ClosestImpact = TNumericLimits<float>::Max();
				for (const FVector& Impact : M_BombImpacts)
				{
					ClosestImpact = FMath::Min(ClosestImpact, static_cast<float>(FVector::Dist2D(Impact, Case.Spot)));
				}
				Case.Notes.Add(M_BombImpacts.Num() > 0
					               ? FString::Printf(TEXT("closest_bomb_impact=%.0f_of_%d"), ClosestImpact, M_BombImpacts.Num())
					               : FString(TEXT("no_bomb_impacts_seen")));
			}
			if (not bDamaged)
			{
				Case.Warns.Add(TEXT("strike_did_not_damage_target_at_spot"));
			}
		}
		int32 Aircraft = 0;
		for (AActor* Actor : WaveSpawned)
		{
			Aircraft += Actor->IsA<AAircraftMaster>() ? 1 : 0;
		}
		Case.Notes.Add(FString::Printf(TEXT("wave_aircraft_alive=%d"), Aircraft));
	}

	void FAllGlobalAbilitiesTestRunner::CleanupWave()
	{
		for (FWaveCase& Case : M_Wave)
		{
			KillActor(Case.StrikeTarget.Get());
		}
		for (const TWeakObjectPtr<AActor>& Actor : M_WaveSpawned)
		{
			if (not Actor.IsValid())
			{
				continue;
			}
			if (Actor->IsA<AAircraftMaster>())
			{
				// Aircraft are left to fly home; a leak shows at the end of the sweep.
				M_SweepAircraft.Add(Actor);
				continue;
			}
			if (Actor->IsA<APawn>() || Actor->IsA<ASquadController>() || Actor->IsA<AHPActorObjectsMaster>())
			{
				KillActor(Actor.Get());
			}
		}
		M_Wave.Reset();
		M_WaveSpawned.Reset();
		M_WaveStage = EWaveStage::None;
		M_SweepAircraft.RemoveAll([](const TWeakObjectPtr<AActor>& Actor) { return not Actor.IsValid(); });
		if (M_SweepQueue.Num() == 0 && M_SweepAircraft.Num() > 0)
		{
			FString Classes;
			for (const TWeakObjectPtr<AActor>& Actor : M_SweepAircraft)
			{
				Classes += Actor->GetClass()->GetName() + TEXT("@") + Actor->GetActorLocation().ToCompactString() + TEXT(" ");
			}
			// Aircraft of earlier waves that are still here minutes later never left.
			Result(TEXT("WARN"), TEXT("SWEEP_AIRCRAFT_LEFT"), FString::Printf(TEXT("alive=%d [%s]"), M_SweepAircraft.Num(), *Classes));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Stress
	// ---------------------------------------------------------------------------------------------------------------

	void FAllGlobalAbilitiesTestRunner::StartStress()
	{
		M_Phase = ERunPhase::Stress;
		M_StressStartReal = GetRealNow();
		M_NextStressActionReal = GetRealNow() + 1.f;
		bM_TrackStress = true;
		AppendProgress(TEXT("WAVE_START STRESS"));
		TArray<FString> DiscardedSamples;
		M_ErrorCounter->TakeErrors(DiscardedSamples);
		Log(FString::Printf(TEXT("STRESS_START seconds=%.0f"), M_StressSeconds));
	}

	void FAllGlobalAbilitiesTestRunner::TickStress()
	{
		TickStressEffects();
		if (GetRealNow() - M_StressStartReal >= M_StressSeconds)
		{
			EndStress();
			return;
		}
		if (GetRealNow() < M_NextStressActionReal)
		{
			return;
		}
		M_NextStressActionReal = GetRealNow() + M_Random.FRandRange(0.25f, 1.5f);
		StressAction();
	}

	void FAllGlobalAbilitiesTestRunner::TickStressEffects()
	{
		UWorld* World = M_World.Get();
		ACPPController* Controller = GetController();
		if (M_UnpauseAtReal > 0.f && GetRealNow() >= M_UnpauseAtReal)
		{
			M_UnpauseAtReal = -1.f;
			if (IsValid(Controller) && Controller->GetIsGameOnPause())
			{
				Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
			}
		}
		if (M_ResetDilationAtReal > 0.f && GetRealNow() >= M_ResetDilationAtReal)
		{
			M_ResetDilationAtReal = -1.f;
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
		}
		if (M_EndGcBurstAtReal > 0.f)
		{
			if (GetRealNow() >= M_EndGcBurstAtReal)
			{
				M_EndGcBurstAtReal = -1.f;
			}
			else
			{
				GEngine->ForceGarbageCollection(true);
			}
		}
	}

	void FAllGlobalAbilitiesTestRunner::StressAction()
	{
		RemoveDeadTracked();
		const float Roll = M_Random.FRand();
		FString Action;
		if (Roll < 0.50f)
		{
			Action = TEXT("fire");
			StressFire(false);
		}
		else if (Roll < 0.55f)
		{
			Action = TEXT("burst_same_spot");
			StressFire(true);
		}
		else if (Roll < 0.60f)
		{
			Action = TEXT("arm_then_cancel");
			StressCancelAim(false);
		}
		else if (Roll < 0.64f)
		{
			Action = TEXT("strip_anchor_then_cancel");
			StressCancelAim(true);
		}
		else if (Roll < 0.71f)
		{
			Action = TEXT("kill_ability_aircraft");
			StressKillRandom(true);
		}
		else if (Roll < 0.78f)
		{
			Action = TEXT("kill_ability_payload");
			StressKillRandom(false);
		}
		else if (Roll < 0.82f)
		{
			Action = TEXT("kill_enemy");
			StressKillEnemy();
		}
		else if (Roll < 0.87f)
		{
			Action = TEXT("attack_move_all");
			StressAttackMove();
		}
		else if (Roll < 0.91f)
		{
			Action = TEXT("pause");
			StressPause();
		}
		else if (Roll < 0.95f)
		{
			Action = TEXT("time_dilation");
			StressTimeDilation();
		}
		else if (Roll < 0.98f)
		{
			Action = TEXT("gc_every_frame_burst");
			StressGarbageBurst();
		}
		else
		{
			Action = TEXT("page_panel");
			if (UGlobalAbilitiesManager* Manager = GetManager())
			{
				Manager->ShowAbilityPanelPage(M_Random.RandRange(0, Manager->GetAbilityPanelPageCount() - 1));
			}
		}
		++M_StressActions;
		M_StressActionCounts.FindOrAdd(Action)++;
		M_LastStressAction = Action;
		M_LastStressActionReal = GetRealNow();
		// Flushed per action: the last line before a crash names what the game was doing.
		UE_LOG(LogRTSGlobalAbilityTest, Display, TEXT("RTS_GA_TEST STRESS_ACTION %d %s t=%.0f aircraft=%d payload=%d"),
		       M_StressActions, *Action, GetRealNow() - M_StressStartReal, GetLiveTracked(true).Num(),
		       GetLiveTracked(false).Num());
		GLog->Flush();
	}

	FVector FAllGlobalAbilitiesTestRunner::GetStressTarget(FString& OutLabel)
	{
		UWorld* World = M_World.Get();
		const float Roll = M_Random.FRand();
		auto PickActor = [this, World](const uint8 Owner) -> AActor*
		{
			TArray<AActor*> Candidates;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if ((It->IsA<APawn>() || It->IsA<ASquadController>() || It->IsA<AHPActorObjectsMaster>())
					&& GetOwnerIndex(*It) == Owner)
				{
					Candidates.Add(*It);
				}
			}
			return Candidates.Num() > 0 ? Candidates[M_Random.RandRange(0, Candidates.Num() - 1)] : nullptr;
		};
		FVector Target = M_Anchor;
		if (Roll < 0.30f)
		{
			OutLabel = TEXT("enemy");
			if (const AActor* Enemy = PickActor(EnemyIndex))
			{
				Target = Enemy->GetActorLocation();
			}
		}
		else if (Roll < 0.45f)
		{
			OutLabel = TEXT("own_unit");
			if (const AActor* Own = PickActor(PlayerIndex))
			{
				Target = Own->GetActorLocation();
			}
		}
		else if (Roll < 0.75f && M_LandscapeBounds.IsValid)
		{
			OutLabel = TEXT("random_ground");
			Target = FVector(M_Random.FRandRange(M_LandscapeBounds.Min.X, M_LandscapeBounds.Max.X),
			                 M_Random.FRandRange(M_LandscapeBounds.Min.Y, M_LandscapeBounds.Max.Y), 0.f);
		}
		else if (Roll < 0.88f && M_LandscapeBounds.IsValid)
		{
			OutLabel = TEXT("map_edge");
			// A point on the landscape border, 3 m inside.
			const FVector Centre = M_LandscapeBounds.GetCenter();
			const float Angle = M_Random.FRandRange(0.f, 2.f * PI);
			const FVector Far = Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 1.0e7f;
			Target = KeepInsideLandscape(Far, 300.f);
			// Grounded below like every other target: a real click carries the ground's height.
		}
		else if (Roll < 0.93f && M_LandscapeBounds.IsValid)
		{
			OutLabel = TEXT("outside_landscape");
			const FVector Centre = M_LandscapeBounds.GetCenter();
			const float Angle = M_Random.FRandRange(0.f, 2.f * PI);
			const FVector Extent = M_LandscapeBounds.GetExtent();
			Target = Centre + FVector(FMath::Cos(Angle) * (Extent.X + 4000.f), FMath::Sin(Angle) * (Extent.Y + 4000.f), 0.f);
			Target.Z = M_Anchor.Z;
			return Target;
		}
		else
		{
			OutLabel = TEXT("camera");
			Target = M_Anchor;
		}
		FVector Ground = Target;
		if (GlobalAbilityHelpers::TryFindGroundLocation(World, Target, Ground))
		{
			// Clicks on units hit the unit, a little above the ground.
			return OutLabel == TEXT("enemy") || OutLabel == TEXT("own_unit") ? Target : Ground;
		}
		return Target;
	}

	FVector FAllGlobalAbilitiesTestRunner::GetRandomStripEnd(const FVector& Start, const UGlobalAbility& Ability)
	{
		const FGlobalAbilityAreaAimSettings& AreaAim = Ability.GetAimSettings().AreaAim;
		// Includes a zero-length strip and one far longer than allowed: the aim must clamp both.
		const float Roll = M_Random.FRand();
		const float Length = Roll < 0.1f ? 0.f : Roll < 0.2f ? AreaAim.MaxStripLength * 3.f
			                                     : M_Random.FRandRange(AreaAim.MinStripLength * 0.5f, AreaAim.MaxStripLength);
		const float Angle = M_Random.FRandRange(0.f, 2.f * PI);
		FVector End = Start + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Length;
		GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), End, End);
		return End;
	}

	void FAllGlobalAbilitiesTestRunner::StressFire(const bool bBurst)
	{
		UGlobalAbilitiesManager* Manager = GetManager();
		if (not IsValid(Manager) || Manager->GetLoadedAbilities().Num() == 0)
		{
			return;
		}
		if (GetLiveTracked(true).Num() >= StressMaxLiveAircraft)
		{
			StressKillRandom(true);
			return;
		}
		if (GetLiveTracked(false).Num() >= StressMaxLivePayload)
		{
			StressKillRandom(false);
			return;
		}
		const TArray<TObjectPtr<UGlobalAbility>>& Loaded = Manager->GetLoadedAbilities();
		UGlobalAbility* Ability = Loaded[M_Random.RandRange(0, Loaded.Num() - 1)];
		if (not IsValid(Ability))
		{
			return;
		}
		FString Label;
		const FVector Spot = GetStressTarget(Label);
		const int32 Count = bBurst ? 4 : 1;
		for (int32 Shot = 0; Shot < Count; ++Shot)
		{
			FString Problem;
			bool bIsStrip = false;
			const FVector End = GetRandomStripEnd(Spot, *Ability);
			if (not FireAbility(*Ability, Spot, End, bIsStrip, Problem))
			{
				++M_StressRejected;
				UE_LOG(LogRTSGlobalAbilityTest, Warning, TEXT("RTS_GA_TEST STRESS_REJECTED %s target=%s problem=%s"),
				       *GetAbilityName(Ability->GetAbilityType()), *Label, *Problem);
				return;
			}
			++M_StressFired;
		}
		Log(FString::Printf(TEXT("STRESS_FIRE %s x%d target=%s at=%s"), *GetAbilityName(Ability->GetAbilityType()), Count,
		                    *Label, *Spot.ToCompactString()));
	}

	void FAllGlobalAbilitiesTestRunner::StressCancelAim(const bool bStripAnchorFirst)
	{
		UGlobalAbilitiesManager* Manager = GetManager();
		ACPPController* Controller = GetController();
		if (not IsValid(Manager) || not IsValid(Controller) || Manager->GetLoadedAbilities().Num() == 0)
		{
			return;
		}
		const TArray<TObjectPtr<UGlobalAbility>>& Loaded = Manager->GetLoadedAbilities();
		UGlobalAbility* Ability = Loaded[M_Random.RandRange(0, Loaded.Num() - 1)];
		FString Problem;
		bool bExecutedOnPress = false;
		if (not IsValid(Ability) || not PressAbilityButton(*Ability, Problem, bExecutedOnPress) || bExecutedOnPress)
		{
			return;
		}
		// While armed: pressing another ability's button must be refused.
		UGlobalAbility* Other = Loaded[M_Random.RandRange(0, Loaded.Num() - 1)];
		if (IsValid(Other) && Other != Ability)
		{
			FString OtherProblem;
			bool bOtherExecuted = false;
			// An instant ability (spies) never arms, so a refused press looks like an executed one here: only an
			// ability that arms can be judged.
			PressAbilityButton(*Other, OtherProblem, bOtherExecuted);
			if (FAllGlobalAbilitiesTestAccess::GetIsArmed(*Other))
			{
				Result(TEXT("FAIL"), TEXT("STRESS_SECOND_ABILITY_ARMED"), FString::Printf(
					       TEXT("%s armed while %s was aiming"), *GetAbilityName(Other->GetAbilityType()),
					       *GetAbilityName(Ability->GetAbilityType())));
				Other->CancelAbilityActivation();
			}
		}
		if (bStripAnchorFirst && Ability->GetAimSettings().AreaAim.Shape == EGlobalAbilityAimShape::Strip)
		{
			FString Label;
			Controller->TryHandleActiveGlobalAbilityPrimaryClick(GetStressTarget(Label));
		}
		Controller->TryCancelActiveGlobalAbility();
		if (FAllGlobalAbilitiesTestAccess::GetIsArmed(*Ability) || FAllGlobalAbilitiesTestAccess::GetHasArmedGlobalAbility(*Controller))
		{
			Result(TEXT("FAIL"), TEXT("STRESS_CANCEL_LEFT_ARMED"), GetAbilityName(Ability->GetAbilityType()));
		}
	}

	void FAllGlobalAbilitiesTestRunner::StressKillRandom(const bool bAircraft)
	{
		TArray<AActor*> Live = GetLiveTracked(bAircraft);
		if (Live.Num() == 0)
		{
			return;
		}
		AActor* Victim = Live[M_Random.RandRange(0, Live.Num() - 1)];
		Log(FString::Printf(TEXT("STRESS_KILL %s %s at=%s"), bAircraft ? TEXT("aircraft") : TEXT("payload"),
		                    *Victim->GetClass()->GetName(), *Victim->GetActorLocation().ToCompactString()));
		KillActor(Victim);
	}

	void FAllGlobalAbilitiesTestRunner::StressKillEnemy()
	{
		TArray<AActor*> Enemies;
		for (TActorIterator<AActor> It(M_World.Get()); It; ++It)
		{
			if ((It->IsA<APawn>() || It->IsA<ASquadController>()) && GetOwnerIndex(*It) == EnemyIndex)
			{
				Enemies.Add(*It);
			}
		}
		if (Enemies.Num() > 0)
		{
			AActor* Victim = Enemies[M_Random.RandRange(0, Enemies.Num() - 1)];
			Log(TEXT("STRESS_KILL enemy ") + Victim->GetClass()->GetName());
			KillActor(Victim);
		}
	}

	void FAllGlobalAbilitiesTestRunner::StressAttackMove()
	{
		FString Label;
		const FVector Target = GetStressTarget(Label);
		int32 Ordered = 0;
		for (TActorIterator<AActor> It(M_World.Get()); It; ++It)
		{
			if (GetOwnerIndex(*It) != PlayerIndex || It->IsA<AAircraftMaster>())
			{
				continue;
			}
			if (ICommands* Commands = Cast<ICommands>(*It))
			{
				const FVector Offset(M_Random.FRandRange(-800.f, 800.f), M_Random.FRandRange(-800.f, 800.f), 0.f);
				(void)Commands->AttackMoveToLocation(Target + Offset, true, FRotator::ZeroRotator);
				++Ordered;
			}
		}
		Log(FString::Printf(TEXT("STRESS_ATTACK_MOVE units=%d target=%s"), Ordered, *Label));
	}

	void FAllGlobalAbilitiesTestRunner::StressPause()
	{
		ACPPController* Controller = GetController();
		if (not IsValid(Controller) || Controller->GetIsGameOnPause())
		{
			return;
		}
		Controller->PauseGame(ERTSPauseGameOptions::ForcePause);
		M_UnpauseAtReal = GetRealNow() + M_Random.FRandRange(0.2f, 3.f);
	}

	void FAllGlobalAbilitiesTestRunner::StressTimeDilation()
	{
		const float Dilation = M_Random.FRand() < 0.5f ? M_Random.FRandRange(0.2f, 0.8f) : M_Random.FRandRange(2.f, 4.f);
		UGameplayStatics::SetGlobalTimeDilation(M_World.Get(), Dilation);
		M_ResetDilationAtReal = GetRealNow() + M_Random.FRandRange(2.f, 8.f);
		Log(FString::Printf(TEXT("STRESS_DILATION %.2f"), Dilation));
	}

	void FAllGlobalAbilitiesTestRunner::StressGarbageBurst()
	{
		M_EndGcBurstAtReal = GetRealNow() + M_Random.FRandRange(2.f, 6.f);
	}

	void FAllGlobalAbilitiesTestRunner::ReportStress(const FString& HowItEnded)
	{
		TArray<FString> ErrorSamples;
		const int32 Errors = M_ErrorCounter->TakeErrors(ErrorSamples);
		FString Counts;
		for (const TPair<FString, int32>& Pair : M_StressActionCounts)
		{
			Counts += FString::Printf(TEXT("%s=%d "), *Pair.Key, Pair.Value);
		}
		Result(Errors > 0 ? TEXT("WARN") : TEXT("PASS"), TEXT("STRESS"), FString::Printf(
			       TEXT("%s after=%.0fs actions=%d fired=%d rejected=%d errors=%d [%s] actions=[%s] aircraft_alive=%d payload_alive=%d"),
			       *HowItEnded, GetRealNow() - M_StressStartReal, M_StressActions, M_StressFired, M_StressRejected, Errors,
			       *FString::Join(ErrorSamples, TEXT(" | ")), *Counts, GetLiveTracked(true).Num(), GetLiveTracked(false).Num()));
	}

	void FAllGlobalAbilitiesTestRunner::EndStress()
	{
		UGameplayStatics::SetGlobalTimeDilation(M_World.Get(), 1.f);
		M_EndGcBurstAtReal = -1.f;
		M_UnpauseAtReal = -1.f;
		ACPPController* Controller = GetController();
		if (IsValid(Controller))
		{
			Controller->TryCancelActiveGlobalAbility();
			if (Controller->GetIsGameOnPause())
			{
				Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
			}
		}
		ReportStress(TEXT("ran_full_time"));
		AppendProgress(TEXT("STRESS_DONE"));
		M_Progress.bStressDone = true;
		bM_TrackStress = false;
		if (bM_DoTravel)
		{
			StartTravelInFlight();
			return;
		}
		FinishRun(TEXT("stress_complete"));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Travel with abilities in flight
	// ---------------------------------------------------------------------------------------------------------------

	void FAllGlobalAbilitiesTestRunner::StartTravelInFlight()
	{
		M_Phase = ERunPhase::TravelInFlight;
		M_TravelStepStart = GetRealNow();
		bM_TrackStress = true;
		// A handful of long abilities in the air (and on the ground) when the map unloads.
		for (int32 Shot = 0; Shot < 6; ++Shot)
		{
			StressFire(false);
		}
		Log(TEXT("TRAVEL_FIRED_ABILITIES"));
	}

	void FAllGlobalAbilitiesTestRunner::TickTravelInFlight()
	{
		if (GetRealNow() - M_TravelStepStart < TravelInFlightSeconds)
		{
			return;
		}
		AppendProgress(TEXT("TRAVEL_START"));
		ReloadMap(TEXT("travel_with_abilities_in_flight"));
	}

	void FAllGlobalAbilitiesTestRunner::TickAfterTravel()
	{
		if (not bM_AfterTravelFired)
		{
			if (GetNow() - M_TravelStepStart < TravelArrivalSettleSeconds)
			{
				return;
			}
			bM_AfterTravelFired = true;
			M_TravelStepStart = GetNow();
			// Fresh world: three abilities through the panel, one of each kind where possible.
			M_Wave.Reset();
			M_EndedCounts.Reset();
			for (const TObjectPtr<UGlobalAbility>& Ability : GetManager()->GetLoadedAbilities())
			{
				if (M_Wave.Num() >= 3 || not IsValid(Ability))
				{
					break;
				}
				const EAbilityKind Kind = GetKind(*Ability);
				if (Kind == EAbilityKind::Instant || M_Wave.ContainsByPredicate([Kind](const FWaveCase& Case)
				{
					return Case.Kind == Kind;
				}))
				{
					continue;
				}
				FWaveCase& Case = M_Wave.AddDefaulted_GetRef();
				Case.Ability = Ability;
				Case.Type = Ability->GetAbilityType();
				Case.Kind = Kind;
				FindSpot(M_Wave.Num() - 1, Case.Spot);
				Case.LineEnd = GetLineEnd(Case.Spot, *Ability);
				FString Problem;
				if (not FireAbility(*Ability, Case.Spot, Case.LineEnd, Case.bIsStrip, Problem))
				{
					Case.Fails.Add(Problem);
				}
			}
			return;
		}
		bool bAllEnded = true;
		for (FWaveCase& Case : M_Wave)
		{
			Case.EndedCount = M_EndedCounts.FindRef(Case.Ability);
			bAllEnded &= Case.Fails.Num() > 0 || Case.EndedCount > 0;
		}
		if (not bAllEnded && GetNow() - M_TravelStepStart < TravelFireWaitSeconds)
		{
			return;
		}
		TArray<FString> Problems;
		for (const FWaveCase& Case : M_Wave)
		{
			if (Case.Fails.Num() > 0 || Case.EndedCount != 1)
			{
				Problems.Add(FString::Printf(TEXT("%s:ended=%d:%s"), *GetAbilityName(Case.Type), Case.EndedCount,
				                             *FString::Join(Case.Fails, TEXT(";"))));
			}
		}
		Result(Problems.Num() > 0 ? TEXT("FAIL") : TEXT("PASS"), TEXT("TRAVEL"),
		       FString::Printf(TEXT("reloaded_with_abilities_in_flight fired_after=%d problems=[%s]"), M_Wave.Num(),
		                       *FString::Join(Problems, TEXT(" "))));
		M_Wave.Reset();
		FinishRun(TEXT("complete"));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------------------------------------------------

	void FAllGlobalAbilitiesTestRunner::BindEndedDelegates()
	{
		UGlobalAbilitiesManager* Manager = GetManager();
		if (not IsValid(Manager))
		{
			return;
		}
		for (const TObjectPtr<UGlobalAbility>& Ability : Manager->GetLoadedAbilities())
		{
			if (IsValid(Ability))
			{
				Ability->OnGlobalAbilityEnded.AddRaw(this, &FAllGlobalAbilitiesTestRunner::HandleAbilityEnded);
				M_BoundAbilities.Add(Ability);
			}
		}
	}

	void FAllGlobalAbilitiesTestRunner::HandleAbilityEnded(UGlobalAbility* Ability)
	{
		M_EndedCounts.FindOrAdd(Ability)++;
	}

	void FAllGlobalAbilitiesTestRunner::HandleActorSpawned(AActor* Actor)
	{
		if (not IsValid(Actor))
		{
			return;
		}
		if (M_Phase == ERunPhase::Sweep && M_WaveStage != EWaveStage::None)
		{
			M_WaveSpawned.Add(Actor);
		}
		if (bM_TrackStress)
		{
			if (Actor->IsA<AAircraftMaster>())
			{
				M_StressAircraft.Add(Actor);
			}
			else if ((Actor->IsA<APawn>() || Actor->IsA<ASquadController>()) && not Actor->IsA<ASquadUnit>())
			{
				M_StressPayload.Add(Actor);
			}
		}
	}

	TArray<AActor*> FAllGlobalAbilitiesTestRunner::GetLiveTracked(const bool bAircraft) const
	{
		TArray<AActor*> Live;
		for (const TWeakObjectPtr<AActor>& Actor : bAircraft ? M_StressAircraft : M_StressPayload)
		{
			if (Actor.IsValid() && (bAircraft || GetOwnerIndex(Actor.Get()) == PlayerIndex))
			{
				Live.Add(Actor.Get());
			}
		}
		return Live;
	}

	void FAllGlobalAbilitiesTestRunner::RemoveDeadTracked()
	{
		M_StressAircraft.RemoveAll([](const TWeakObjectPtr<AActor>& Actor) { return not Actor.IsValid(); });
		M_StressPayload.RemoveAll([](const TWeakObjectPtr<AActor>& Actor) { return not Actor.IsValid(); });
	}

	FBox FAllGlobalAbilitiesTestRunner::FindLandscapeBounds() const
	{
		FBox Bounds(ForceInit);
		for (TActorIterator<ALandscapeProxy> It(M_World.Get()); It; ++It)
		{
			Bounds += It->GetComponentsBoundingBox(true);
		}
		return Bounds;
	}

	FVector FAllGlobalAbilitiesTestRunner::KeepInsideLandscape(const FVector& Location, const float Margin) const
	{
		FVector Clamped = Location;
		if (M_LandscapeBounds.IsValid)
		{
			const float MarginX = FMath::Min(Margin, M_LandscapeBounds.GetExtent().X * 0.5f);
			const float MarginY = FMath::Min(Margin, M_LandscapeBounds.GetExtent().Y * 0.5f);
			Clamped.X = FMath::Clamp(Location.X, M_LandscapeBounds.Min.X + MarginX, M_LandscapeBounds.Max.X - MarginX);
			Clamped.Y = FMath::Clamp(Location.Y, M_LandscapeBounds.Min.Y + MarginY, M_LandscapeBounds.Max.Y - MarginY);
		}
		FVector OnGround = Clamped;
		GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), Clamped, OnGround);
		return OnGround;
	}

	bool FAllGlobalAbilitiesTestRunner::GetIsOnGround(const AActor& Actor) const
	{
		FVector Ground;
		if (not GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), Actor.GetActorLocation(), Ground))
		{
			return true;
		}
		return FMath::Abs(Actor.GetActorLocation().Z - Ground.Z) <= GroundToleranceCm;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Registration
	// ---------------------------------------------------------------------------------------------------------------

	TArray<TUniquePtr<FAllGlobalAbilitiesTestRunner>> GActiveRunners;

	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues)
	{
		if (not IsValid(World) || not World->IsGameWorld()
			|| not FParse::Param(FCommandLine::Get(), TEXT("RTSGlobalAbilityTest")))
		{
			return;
		}
		GActiveRunners.Add(MakeUnique<FAllGlobalAbilitiesTestRunner>(*World));
	}

	void HandleWorldCleanup(UWorld* World, bool, bool)
	{
		GActiveRunners.RemoveAll([World](const TUniquePtr<FAllGlobalAbilitiesTestRunner>& Runner)
		{
			return not Runner.IsValid() || Runner->GetIsForWorld(World);
		});
	}

	struct FAllGlobalAbilitiesTestRegistration
	{
		FDelegateHandle PostWorldInitializationHandle;
		FDelegateHandle WorldCleanupHandle;

		FAllGlobalAbilitiesTestRegistration()
		{
			PostWorldInitializationHandle =
				FWorldDelegates::OnPostWorldInitialization.AddStatic(&HandlePostWorldInitialization);
			WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&HandleWorldCleanup);
		}

		~FAllGlobalAbilitiesTestRegistration()
		{
			FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationHandle);
			FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		}
	};

	FAllGlobalAbilitiesTestRegistration GRegistration;
}

#endif
