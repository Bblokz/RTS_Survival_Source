// Copyright (C) Bas Blokzijl - All rights reserved.

// -DirectionFindingValidate on Ger_OderWarthe: checks the direction-finding hunt end to end on the placed
// ADirectionFindingManager. First a solver sweep without the world: for every command post and every set of two and
// three stations, with random wedge errors, the command post must stay inside every wedge, three narrowed stations
// must fix it, two must never be enough. Then the live manager: the command post targets spawn hidden at the picked
// Werk, a captured station opens a wedge of the difficulty's width, attack-order, routine and reply bursts narrow it
// by their factors (a second report of the same site within the cooldown makes no reply), the second station raises
// the "noticed" state and an overlap polygon, a lost station keeps its wedge but stops narrowing and continues from
// the same width when retaken, and attack orders on three stations reach the fix: one candidate, the targets visible
// and collidable, no more narrowing. -DirectionFindingPictures saves screenshots (with the UI, for the minimap) at the
// first wedge, the overlap and the fix to Saved/Screenshots. Logs RTS_DF_TEST lines and quits.
// Everything lives in this file, behind RTS_WITH_MAP_TESTS.

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Missions/DirectionFinding/DirectionFindingManager.h"
#include "RTS_Survival/Missions/DirectionFinding/DirectionFindingSolver.h"
#include "RTS_Survival/Missions/DirectionFinding/DirectionFindingSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "Misc/Paths.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/SupplyPoints/SupplyPoint.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSDirectionFindingTest, Display, All);

namespace DirectionFindingMapTestPrivate
{
	constexpr float StepTimeoutSeconds = 60.f;
	constexpr float ReplyWaitSeconds = 30.f;
	constexpr float SettleSeconds = 1.5f;
	constexpr float GameStartSeconds = 4.f;
	constexpr float AngleTolerance = 0.01f;
	// Targets are placed on the Werk (porch, mast, portal); all of them lie within this of its centre.
	constexpr float TargetMaxDistanceFromWerkCm = 8000.f;
	constexpr int32 MaxAttackOrdersToFix = 12;
	constexpr int32 SweepSeedsPerSet = 24;
	constexpr int32 SweepBurstsToMinimum = 12;
	constexpr float SweepStartHalfAngle = 25.f;
	constexpr int32 RandomSweepSeed = 1976;
	const FName PacklitzSite(TEXT("TestSite_Packlitz"));
	const TCHAR* const PicturesSwitch = TEXT("DirectionFindingPictures");

	enum class EStep : uint8
	{
		WaitingForManager,
		CaptureFirst,
		Bursts,
		WaitReply,
		CaptureSecond,
		LoseAndRetake,
		CaptureThird,
		AfterFix,
		Finished
	};

	UWorld* FindGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (IsValid(World) && World->IsGameWorld() && World->HasBegunPlay())
			{
				return World;
			}
		}
		return nullptr;
	}
}

/** @brief Runs the hunt from a core ticker, so nothing in the game has to know about this test. */
class FDirectionFindingMapTest
{
public:
	bool Tick()
	{
		using namespace DirectionFindingMapTestPrivate;
		UWorld* World = FindGameWorld();
		if (not IsValid(World))
		{
			return true;
		}
		ACPPController* const PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
		FGrenadeTestHelpers::KeepGameRunning(*World, PlayerController);
		// The map boots behind the start-game widget; start it as the player's "Press To Start" does.
		UPlayerStartGameControl* const StartGameControl = IsValid(PlayerController)
			                                                  ? PlayerController->FindComponentByClass<
				                                                  UPlayerStartGameControl>()
			                                                  : nullptr;
		if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
		{
			StartGameControl->PlayerStartedGame();
		}
		if (M_Step != EStep::Finished && GetStepSeconds(*World) > StepTimeoutSeconds + ReplyWaitSeconds)
		{
			Check(false, TEXT("step_timeout"), FString::Printf(TEXT("step=%d"), static_cast<int32>(M_Step)));
			FinishTest();
			return false;
		}
		if (GetStepSeconds(*World) < SettleSeconds && M_Step != EStep::WaitingForManager)
		{
			return true;
		}
		switch (M_Step)
		{
		case EStep::WaitingForManager:
			TickWaitingForManager(*World);
			break;
		case EStep::CaptureFirst:
			TickCaptureFirst(*World);
			break;
		case EStep::Bursts:
			TickBursts(*World);
			break;
		case EStep::WaitReply:
			TickWaitReply(*World);
			break;
		case EStep::CaptureSecond:
			TickCaptureSecond(*World);
			break;
		case EStep::LoseAndRetake:
			TickLoseAndRetake(*World);
			break;
		case EStep::CaptureThird:
			TickCaptureThird(*World);
			break;
		case EStep::AfterFix:
			TickAfterFix();
			break;
		case EStep::Finished:
			return false;
		}
		return M_Step != EStep::Finished;
	}

private:
	DirectionFindingMapTestPrivate::EStep M_Step = DirectionFindingMapTestPrivate::EStep::WaitingForManager;
	double M_StepStartSeconds = -1.0;
	int32 M_FailedChecks = 0;
	TWeakObjectPtr<ADirectionFindingManager> M_Manager;
	TWeakObjectPtr<AFowManager> M_FowManager;
	float M_StartHalfAngle = 0.f;
	float M_HalfAngleBeforeReply = 0.f;
	int32 M_ThirdStationIndex = INDEX_NONE;
	int32 M_PictureCount = 0;
	bool bM_TakePictures = false;

	// ---- Bookkeeping ----

	double GetStepSeconds(const UWorld& World)
	{
		if (M_StepStartSeconds < 0.0)
		{
			M_StepStartSeconds = World.GetTimeSeconds();
		}
		return World.GetTimeSeconds() - M_StepStartSeconds;
	}

	void EnterStep(const DirectionFindingMapTestPrivate::EStep NewStep)
	{
		M_Step = NewStep;
		M_StepStartSeconds = -1.0;
		UE_LOG(LogRTSDirectionFindingTest, Display, TEXT("RTS_DF_TEST step=%d"), static_cast<int32>(NewStep));
	}

	void Check(const bool bOk, const TCHAR* CheckName, const FString& Detail)
	{
		M_FailedChecks += bOk ? 0 : 1;
		UE_LOG(LogRTSDirectionFindingTest, Display, TEXT("RTS_DF_TEST %s check='%s' %s"),
		       bOk ? TEXT("PASS") : TEXT("FAIL"), CheckName, *Detail);
	}

	void FinishTest()
	{
		UE_LOG(LogRTSDirectionFindingTest, Display, TEXT("RTS_DF_TEST RESULT %s failed_checks=%d"),
		       M_FailedChecks == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedChecks);
		EnterStep(DirectionFindingMapTestPrivate::EStep::Finished);
		FPlatformMisc::RequestExit(false);
	}

	void TakePicture(const TCHAR* Name)
	{
		if (not bM_TakePictures)
		{
			return;
		}
		M_PictureCount++;
		const FString FileName = FPaths::ProjectSavedDir() / FString::Printf(
			TEXT("DirectionFindingTest/Pictures/DirectionFinding_%02d_%s.png"), M_PictureCount, Name);
		FScreenshotRequest::RequestScreenshot(FileName, true, false);
		UE_LOG(LogRTSDirectionFindingTest, Display, TEXT("RTS_DF_TEST picture %s"), *FileName);
	}

	ASupplyPoint* GetStation(const int32 StationIndex) const
	{
		const ADirectionFindingManager* const Manager = M_Manager.Get();
		return IsValid(Manager) && Manager->GetStations().IsValidIndex(StationIndex)
			       ? Manager->GetStations()[StationIndex].Get()
			       : nullptr;
	}

	void SetStationOwner(const int32 StationIndex, const uint8 Owner) const
	{
		if (ASupplyPoint* const Station = GetStation(StationIndex))
		{
			Station->ForceCaptureByPlayer(Owner);
		}
	}

	FDirectionFindingDrawData GetDrawData() const
	{
		FDirectionFindingDrawData DrawData;
		const AFowManager* const FowManager = M_FowManager.Get();
		const ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(FowManager) || not IsValid(Manager))
		{
			return DrawData;
		}
		const FVector2D Origin(FowManager->GetActorLocation());
		const FVector2D HalfSize(FowManager->GetMapExtent());
		Manager->GetDrawData(FBox2D(Origin - HalfSize, Origin + HalfSize), DrawData);
		return DrawData;
	}

	bool GetIsNear(const float Value, const float Expected) const
	{
		return FMath::IsNearlyEqual(Value, Expected, DirectionFindingMapTestPrivate::AngleTolerance);
	}

	// ---- Steps ----

	void TickWaitingForManager(UWorld& World)
	{
		// Let the started game bring up its UI (the minimap) before the first picture.
		if (GetStepSeconds(World) < DirectionFindingMapTestPrivate::GameStartSeconds)
		{
			return;
		}
		const UDirectionFindingSubsystem* const Subsystem = World.GetSubsystem<UDirectionFindingSubsystem>();
		ADirectionFindingManager* const Manager = IsValid(Subsystem) ? Subsystem->GetDirectionFindingManager() : nullptr;
		if (not IsValid(Manager) || not Manager->GetHasStarted())
		{
			if (GetStepSeconds(World) > DirectionFindingMapTestPrivate::StepTimeoutSeconds)
			{
				Check(false, TEXT("manager_started"), IsValid(Manager) ? TEXT("not started") : TEXT("no manager"));
				FinishTest();
			}
			return;
		}
		M_Manager = Manager;
		for (TActorIterator<AFowManager> It(&World); It; ++It)
		{
			M_FowManager = *It;
		}
		bM_TakePictures = FParse::Param(FCommandLine::Get(), DirectionFindingMapTestPrivate::PicturesSwitch);
		Check(M_FowManager.IsValid(), TEXT("fow_manager"), FString());
		RunSolverSweep(*Manager);
		CheckSetup(*Manager);
		EnterStep(DirectionFindingMapTestPrivate::EStep::CaptureFirst);
	}

	void CheckSetup(const ADirectionFindingManager& Manager)
	{
		using namespace DirectionFindingMapTestPrivate;
		const int32 CommandPost = Manager.GetCommandPostWerkIndex();
		Check(Manager.GetStations().Num() == 5, TEXT("five_stations"),
		      FString::Printf(TEXT("stations=%d"), Manager.GetStations().Num()));
		Check(Manager.GetWerke().Num() == 5, TEXT("five_werke"),
		      FString::Printf(TEXT("werke=%d"), Manager.GetWerke().Num()));
		Check(Manager.GetWerke().IsValidIndex(CommandPost), TEXT("command_post_picked"),
		      FString::Printf(TEXT("index=%d id=%s"), CommandPost, *Manager.GetCommandPostWerkId().ToString()));
		int32 HeldAtStart = 0;
		for (const TObjectPtr<ASupplyPoint>& Station : Manager.GetStations())
		{
			HeldAtStart += IsValid(Station) && Station->GetOwningPlayer() == RTSPlayers::HumanPlayer ? 1 : 0;
		}
		Check(HeldAtStart == 0 && Manager.GetHeldStationCount() == 0 && GetDrawData().Wedges.IsEmpty(),
		      TEXT("no_wedges_at_start"), FString::Printf(TEXT("player_held=%d"), HeldAtStart));
		const TArray<AActor*> Targets = Manager.GetCommandPostTargets();
		Check(Targets.Num() == 3, TEXT("three_targets"), FString::Printf(TEXT("targets=%d"), Targets.Num()));
		if (not Manager.GetWerke().IsValidIndex(CommandPost))
		{
			return;
		}
		const FVector WerkCentre = Manager.GetWerke()[CommandPost].Centre;
		for (const AActor* const Target : Targets)
		{
			const float Distance = FVector::Dist2D(Target->GetActorLocation(), WerkCentre);
			Check(Target->IsHidden() && not Target->GetActorEnableCollision()
			      && Distance < TargetMaxDistanceFromWerkCm, TEXT("target_hidden_at_werk"),
			      FString::Printf(TEXT("%s hidden=%d collision=%d dist=%.0f"), *Target->GetClass()->GetName(),
			                      Target->IsHidden() ? 1 : 0, Target->GetActorEnableCollision() ? 1 : 0, Distance));
		}
	}

	void TickCaptureFirst(UWorld& World)
	{
		ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(Manager))
		{
			FinishTest();
			return;
		}
		if (Manager->GetHeldStationCount() == 0)
		{
			SetStationOwner(0, RTSPlayers::HumanPlayer);
			return;
		}
		M_StartHalfAngle = Manager->GetStationHalfAngle(0);
		const FDirectionFindingDrawData DrawData = GetDrawData();
		Check(GetIsNear(M_StartHalfAngle, Manager->GetSolver().GetWedge(0)->HalfAngleDegrees)
		      && M_StartHalfAngle >= 19.f, TEXT("first_wedge_width"),
		      FString::Printf(TEXT("half_angle=%.2f"), M_StartHalfAngle));
		Check(DrawData.Wedges.Num() == 1 && DrawData.Wedges[0].Polygon.Num() >= 3 && DrawData.Overlap.IsEmpty(),
		      TEXT("first_wedge_drawn"), FString::Printf(TEXT("wedges=%d points=%d overlap=%d"),
		                                                  DrawData.Wedges.Num(),
		                                                  DrawData.Wedges.IsEmpty()
			                                                  ? 0
			                                                  : DrawData.Wedges[0].Polygon.Num(),
		                                                  DrawData.Overlap.Num()));
		Check(not Manager->GetHasNoticedPlayer() && Manager->GetCandidateWerkCount() >= 1, TEXT("one_station_state"),
		      FString::Printf(TEXT("candidates=%d"), Manager->GetCandidateWerkCount()));
		TakePicture(TEXT("first_wedge"));
		EnterStep(DirectionFindingMapTestPrivate::EStep::Bursts);
	}

	void TickBursts(UWorld& World)
	{
		ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(Manager))
		{
			FinishTest();
			return;
		}
		const FDirectionFindingSettings Defaults;
		Manager->ForceBurst(EDirectionFindingBurst::AttackOrder);
		const float AfterAttack = Manager->GetStationHalfAngle(0);
		Check(GetIsNear(AfterAttack, M_StartHalfAngle * Defaults.AttackOrderFactor), TEXT("attack_order_narrows"),
		      FString::Printf(TEXT("%.2f -> %.2f"), M_StartHalfAngle, AfterAttack));
		Manager->ForceBurst(EDirectionFindingBurst::Routine);
		const float AfterRoutine = Manager->GetStationHalfAngle(0);
		Check(GetIsNear(AfterRoutine, AfterAttack * Defaults.RoutineFactor), TEXT("routine_narrows"),
		      FString::Printf(TEXT("%.2f -> %.2f"), AfterAttack, AfterRoutine));
		M_HalfAngleBeforeReply = AfterRoutine;
		const bool bFirstReport = Manager->NotifyPlayerAttackedSite(DirectionFindingMapTestPrivate::PacklitzSite);
		const bool bSecondReport = Manager->NotifyPlayerAttackedSite(DirectionFindingMapTestPrivate::PacklitzSite);
		Check(bFirstReport && not bSecondReport, TEXT("reply_cooldown"),
		      FString::Printf(TEXT("first=%d second=%d"), bFirstReport ? 1 : 0, bSecondReport ? 1 : 0));
		EnterStep(DirectionFindingMapTestPrivate::EStep::WaitReply);
	}

	void TickWaitReply(UWorld& World)
	{
		const ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(Manager))
		{
			FinishTest();
			return;
		}
		const FDirectionFindingSettings Defaults;
		const float HalfAngle = Manager->GetStationHalfAngle(0);
		// A routine burst may land while waiting; the reply is the 0.7 step on top of whatever came before it.
		const bool bReplied = HalfAngle < M_HalfAngleBeforeReply - DirectionFindingMapTestPrivate::AngleTolerance;
		if (not bReplied && GetStepSeconds(World) < DirectionFindingMapTestPrivate::ReplyWaitSeconds)
		{
			return;
		}
		const double Seconds = GetStepSeconds(World);
		Check(bReplied && Seconds >= Defaults.ReplyDelaySeconds.X - DirectionFindingMapTestPrivate::SettleSeconds,
		      TEXT("reply_narrows_after_delay"),
		      FString::Printf(TEXT("%.2f -> %.2f after %.1f s (expected x%.2f)"), M_HalfAngleBeforeReply, HalfAngle,
		                      Seconds, Defaults.ReplyFactor));
		EnterStep(DirectionFindingMapTestPrivate::EStep::CaptureSecond);
	}

	void TickCaptureSecond(UWorld& World)
	{
		const ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(Manager))
		{
			FinishTest();
			return;
		}
		if (Manager->GetHeldStationCount() < 2)
		{
			SetStationOwner(1, RTSPlayers::HumanPlayer);
			return;
		}
		const FDirectionFindingDrawData DrawData = GetDrawData();
		Check(Manager->GetHasNoticedPlayer(), TEXT("noticed_at_two_stations"), FString());
		Check(DrawData.Wedges.Num() == 2 && DrawData.Overlap.Num() >= 3, TEXT("overlap_drawn"),
		      FString::Printf(TEXT("wedges=%d overlap_points=%d"), DrawData.Wedges.Num(), DrawData.Overlap.Num()));
		Check(not Manager->GetIsFixed(), TEXT("two_stations_no_fix"),
		      FString::Printf(TEXT("candidates=%d"), Manager->GetCandidateWerkCount()));
		TakePicture(TEXT("overlap"));
		EnterStep(DirectionFindingMapTestPrivate::EStep::LoseAndRetake);
	}

	void TickLoseAndRetake(UWorld& World)
	{
		ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(Manager))
		{
			FinishTest();
			return;
		}
		const float FirstBefore = Manager->GetStationHalfAngle(0);
		const float SecondBefore = Manager->GetStationHalfAngle(1);
		SetStationOwner(0, RTSPlayers::FirstAIPlayer);
		const FDirectionFindingDrawData LostDraw = GetDrawData();
		const bool bLostKept = LostDraw.Wedges.Num() == 2 && LostDraw.Overlap.IsEmpty()
			&& Manager->GetHeldStationCount() == 1;
		Manager->ForceBurst(EDirectionFindingBurst::AttackOrder);
		const FDirectionFindingSettings Defaults;
		Check(bLostKept && GetIsNear(Manager->GetStationHalfAngle(0), FirstBefore)
		      && GetIsNear(Manager->GetStationHalfAngle(1), FMath::Max(Defaults.MinHalfAngle,
		                                                               SecondBefore * Defaults.AttackOrderFactor)),
		      TEXT("lost_station_keeps_wedge"),
		      FString::Printf(TEXT("kept=%d first %.2f -> %.2f, second %.2f -> %.2f"), bLostKept ? 1 : 0,
		                      FirstBefore, Manager->GetStationHalfAngle(0), SecondBefore,
		                      Manager->GetStationHalfAngle(1)));
		SetStationOwner(0, RTSPlayers::HumanPlayer);
		Check(Manager->GetHeldStationCount() == 2 && GetIsNear(Manager->GetStationHalfAngle(0), FirstBefore),
		      TEXT("retaken_station_continues"),
		      FString::Printf(TEXT("held=%d half_angle=%.2f"), Manager->GetHeldStationCount(),
		                      Manager->GetStationHalfAngle(0)));
		EnterStep(DirectionFindingMapTestPrivate::EStep::CaptureThird);
	}

	void TickCaptureThird(UWorld& World)
	{
		using namespace DirectionFindingMapTestPrivate;
		ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(Manager))
		{
			FinishTest();
			return;
		}
		if (M_ThirdStationIndex == INDEX_NONE)
		{
			// DF 3 Kalau heath, the default third station.
			M_ThirdStationIndex = 2;
			SetStationOwner(M_ThirdStationIndex, RTSPlayers::HumanPlayer);
			UE_LOG(LogRTSDirectionFindingTest, Display, TEXT("RTS_DF_TEST third station held, fixed=%d candidates=%d"),
			       Manager->GetIsFixed() ? 1 : 0, Manager->GetCandidateWerkCount());
		}
		int32 AttackOrders = 0;
		while (not Manager->GetIsFixed() && AttackOrders < MaxAttackOrdersToFix)
		{
			Manager->ForceBurst(EDirectionFindingBurst::AttackOrder);
			AttackOrders++;
		}
		Check(Manager->GetIsFixed(), TEXT("three_stations_fix"),
		      FString::Printf(TEXT("attack_orders=%d candidates=%d"), AttackOrders, Manager->GetCandidateWerkCount()));
		Check(Manager->GetCandidateWerkCount() == 1, TEXT("fix_one_candidate"),
		      FString::Printf(TEXT("candidates=%d command_post=%s"), Manager->GetCandidateWerkCount(),
		                      *Manager->GetCommandPostWerkId().ToString()));
		for (const AActor* const Target : Manager->GetCommandPostTargets())
		{
			Check(not Target->IsHidden() && Target->GetActorEnableCollision(), TEXT("target_revealed"),
			      Target->GetClass()->GetName());
		}
		TakePicture(TEXT("fix"));
		EnterStep(EStep::AfterFix);
	}

	void TickAfterFix()
	{
		ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(Manager))
		{
			FinishTest();
			return;
		}
		const float Before = Manager->GetStationHalfAngle(1);
		const FDirectionFindingSettings Defaults;
		Manager->ForceBurst(EDirectionFindingBurst::AttackOrder);
		Check(GetIsNear(Manager->GetStationHalfAngle(1), Before) || GetIsNear(Before, Defaults.MinHalfAngle),
		      TEXT("no_narrowing_after_fix"),
		      FString::Printf(TEXT("%.2f -> %.2f"), Before, Manager->GetStationHalfAngle(1)));
		FinishTest();
	}

	// ---- Solver sweep (no world) ----

	void RunSolverSweep(const ADirectionFindingManager& Manager)
	{
		using namespace DirectionFindingMapTestPrivate;
		TArray<FVector2D> StationLocations;
		for (const TObjectPtr<ASupplyPoint>& Station : Manager.GetStations())
		{
			StationLocations.Add(IsValid(Station) ? FVector2D(Station->GetActorLocation()) : FVector2D::ZeroVector);
		}
		TArray<FVector2D> WerkCentres;
		TArray<float> WerkRadii;
		for (const FDirectionFindingWerk& Werk : Manager.GetWerke())
		{
			WerkCentres.Add(FVector2D(Werk.Centre));
			WerkRadii.Add(Werk.RadiusCm);
		}
		const FDirectionFindingSettings Defaults;
		FRandomStream Random(RandomSweepSeed);
		int32 Runs = 0;
		int32 CommandPostOutside = 0;
		int32 ThreeNoFix = 0;
		int32 TwoFixed = 0;
		int32 TwoIsolatedAtMinimum = 0;
		int32 TwoRuns = 0;
		const int32 StationCount = StationLocations.Num();
		for (int32 CommandPost = 0; CommandPost < WerkCentres.Num(); ++CommandPost)
		{
			for (int32 Mask = 1; Mask < (1 << StationCount); ++Mask)
			{
				const int32 Held = FMath::CountBits(static_cast<uint64>(Mask));
				if (Held != 2 && Held != 3)
				{
					continue;
				}
				for (int32 Seed = 0; Seed < SweepSeedsPerSet; ++Seed)
				{
					FDirectionFindingSolver Solver;
					Solver.Init(StationLocations, WerkCentres, WerkRadii, CommandPost);
					for (int32 StationIndex = 0; StationIndex < StationCount; ++StationIndex)
					{
						if ((Mask & (1 << StationIndex)) != 0)
						{
							Solver.SetStationHeld(StationIndex, true, SweepStartHalfAngle,
							                      Random.FRandRange(-Defaults.MaxErrorFraction,
							                                        Defaults.MaxErrorFraction));
						}
					}
					Runs++;
					CommandPostOutside += Solver.GetCandidateWerke().Contains(CommandPost) ? 0 : 1;
					for (int32 Burst = 0; Burst < SweepBurstsToMinimum; ++Burst)
					{
						Solver.ApplyBurst(Defaults.AttackOrderFactor, Defaults.MinHalfAngle, 0.0);
						CommandPostOutside += Solver.GetCandidateWerke().Contains(CommandPost) ? 0 : 1;
					}
					if (Held == 3)
					{
						ThreeNoFix += Solver.GetIsFix(Defaults.StationsNeededForFix) ? 0 : 1;
						continue;
					}
					TwoRuns++;
					TwoFixed += Solver.GetIsFix(Defaults.StationsNeededForFix) ? 1 : 0;
					TwoIsolatedAtMinimum += Solver.GetCandidateWerke().Num() == 1 ? 1 : 0;
				}
			}
		}
		Check(Runs > 0 && CommandPostOutside == 0, TEXT("sweep_command_post_always_inside"),
		      FString::Printf(TEXT("runs=%d outside=%d"), Runs, CommandPostOutside));
		Check(ThreeNoFix == 0, TEXT("sweep_three_stations_fix"), FString::Printf(TEXT("no_fix=%d"), ThreeNoFix));
		Check(TwoFixed == 0, TEXT("sweep_two_stations_never_fix"), FString::Printf(TEXT("fixed=%d"), TwoFixed));
		// Not a check: how often two fully narrowed wedges already single out one Werk (a design number).
		UE_LOG(LogRTSDirectionFindingTest, Display,
		       TEXT("RTS_DF_TEST info two_stations_isolated_at_min=%d of %d"), TwoIsolatedAtMinimum, TwoRuns);
	}
};

static FDelayedAutoRegisterHelper GRegisterDirectionFindingMapTest(EDelayedRegisterRunPhase::EndOfEngineInit, []()
{
	if (not FParse::Param(FCommandLine::Get(), TEXT("DirectionFindingValidate")))
	{
		return;
	}
	const TSharedRef<FDirectionFindingMapTest> Test = MakeShared<FDirectionFindingMapTest>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float)
	{
		return Test->Tick();
	}));
});

#endif
