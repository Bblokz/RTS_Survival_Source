// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalConvoyTest.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/SurvivalMode/Core/SurvivalModeTypes.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/Events/SurvivalEventSettings.h"
#include "RTS_Survival/SurvivalMode/Events/SurvivalEventSubsystem.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveDirectorComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurvivalConvoyTestConstants
{
	constexpr double SampleIntervalSeconds = 0.25;
	constexpr double SettleAfterGraceSeconds = 6.0;
	constexpr double LingerAfterEndSeconds = 10.0;
	constexpr double ShotSettleSeconds = 0.2;
	constexpr double ShotWriteTimeoutSeconds = 4.0;
	constexpr float FollowFieldOfView = 60.f;
	constexpr float FollowMarginCm = 4000.f;
	constexpr float MinFollowSpanCm = 9000.f;
	constexpr int32 FollowFrames = 12;
	constexpr double FollowFirstSeconds = 30.0;
	constexpr double FollowStepSeconds = 5.0;
	constexpr int32 RtsFrames = 8;
	constexpr double RtsFirstSeconds = 50.0;
	constexpr double RtsStepSeconds = 2.0;
	constexpr float RtsArm = 3000.f;
	constexpr float RtsPitch = -70.f;
	constexpr float RtsFieldOfView = 90.f;
	constexpr int32 MarchFrames = 6;
	constexpr double MarchFirstSeconds = 40.0;
	constexpr double MarchStepSeconds = 0.5;
	constexpr float MarchSideDistance = 1200.f;
	constexpr float MarchEyeHeight = 200.f;
	constexpr float MarchFieldOfView = 40.f;
	// Calibration.
	constexpr float CalibrationLaneLength = 8000.f;
	constexpr float CalibrationLaneSpacing = 1000.f;
	constexpr float CalibrationMiddleStart = 0.2f;
	constexpr float CalibrationMiddleEnd = 0.8f;
	constexpr double CalibrationLegTimeoutSeconds = 60.0;
	constexpr double CalibrationSpawnWaitSeconds = 8.0;
	constexpr float CalibrationArriveDistance = 700.f;
	constexpr float CalibrationRestrictedFraction = 0.5f;
	// T2 / T4 scripted actions (rubric: one truck at 40 % health; kills at 30 / 40 / 50 % of the route).
	constexpr double ScriptedFirstActionSeconds = 3.0;
	constexpr float T2TruckHealthFraction = 0.4f;
	constexpr float T4KillTruckProgress = 0.3f;
	constexpr float T4KillEscortProgress = 0.4f;
	constexpr float T4KillSquadProgress = 0.5f;
	constexpr float ScriptedKillDamage = 1.0e7f;
	// PalmCrush: a light then a medium tank driven through one oasis palm (SM_Desert_01, round 7).
	constexpr float PalmStartDistance = 1500.f;
	constexpr float PalmOvershootDistance = 900.f;
	constexpr double PalmSpawnWaitSeconds = 6.0;
	constexpr double PalmDriveSeconds = 16.0;
	constexpr double PalmShotStepSeconds = 1.0;
	constexpr float PalmCameraArm = 2200.f;
	constexpr float PalmToppledTiltDegrees = 30.f;
	constexpr int32 SweepRounds = 20;
	constexpr int32 SweepDefaultSeeds = 200;
	constexpr int32 SweepRouteSeedSalt = 7919;
}

namespace
{
	FString SurvivalConvoyTest_Json(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		FJsonSerializer::Serialize(Object, Writer);
		return Text;
	}

	TArray<TSharedPtr<FJsonValue>> SurvivalConvoyTest_Vector(const FVector& Vector)
	{
		return {MakeShared<FJsonValueNumber>(Vector.X), MakeShared<FJsonValueNumber>(Vector.Y),
		        MakeShared<FJsonValueNumber>(Vector.Z)};
	}

	FString SurvivalConvoyTest_Command(AActor* Actor)
	{
		ICommands* Commands = Cast<ICommands>(Actor);
		return Commands != nullptr ? Global_GetAbilityIDAsString(Commands->GetActiveCommandID()) : TEXT("none");
	}

	float SurvivalConvoyTest_Health(const AActor* Actor)
	{
		const UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		return Health != nullptr and Health->GetMaxHealth() > 0.f ? Health->GetHealthPercentage() : -1.f;
	}

	float SurvivalConvoyTest_Median(TArray<float> Values)
	{
		if (Values.IsEmpty())
		{
			return 0.f;
		}
		Values.Sort();
		return Values[Values.Num() / 2];
	}

	float SurvivalConvoyTest_Percentile(TArray<float> Values, const float Fraction)
	{
		if (Values.IsEmpty())
		{
			return 0.f;
		}
		Values.Sort();
		return Values[FMath::Clamp(FMath::FloorToInt(Fraction * (Values.Num() - 1)), 0, Values.Num() - 1)];
	}
}

/** One calibration lane: a unit driving or walking a straight back and forth. */
struct FSurvivalConvoyCalibrationUnit
{
	FString Name;
	FTrainingOption Option;
	TSoftClassPtr<AActor> ExactClass;
	bool bIsSquad = false;
	TWeakObjectPtr<AActor> Actor;
	FVector LaneStart = FVector::ZeroVector;
	FVector LaneEnd = FVector::ZeroVector;
	FVector PreviousLocation = FVector::ZeroVector;
	double PreviousTime = -1.0;
	float NominalSpeed = 0.f;
	TArray<float> Speeds;
	bool bArrived = false;
};

class FSurvivalConvoyTestRunner : public TSharedFromThis<FSurvivalConvoyTestRunner>
{
public:
	void Start(USurvivalEventSubsystem* Subsystem);

private:
	enum class EStep : uint8
	{
		WaitForGrace,
		Settle,
		Run,
		Linger,
		Finished
	};

	enum class EShotKind : uint8
	{
		Follow,
		Rts,
		MarchClose
	};

	struct FShot
	{
		double AtConvoySeconds = 0.0;
		FString FileName;
		EShotKind Kind = EShotKind::Follow;
	};

	TWeakObjectPtr<USurvivalEventSubsystem> M_Subsystem;
	FTSTicker::FDelegateHandle M_TickerHandle;
	FString M_Mode;
	FString M_Directory;
	double M_TimeoutRealSeconds = 900.0;
	double M_StartRealSeconds = 0.0;
	EStep M_Step = EStep::WaitForGrace;
	double M_StepStartWorld = 0.0;
	bool bM_PressedStart = false;

	// T1
	TWeakObjectPtr<AConvoy> M_Convoy;
	double M_ConvoyStartWorld = 0.0;
	double M_NextSampleWorld = 0.0;
	FString M_UnitsCsv;
	FString M_EventsCsv;
	TArray<TWeakObjectPtr<AActor>> M_SeenUnits;
	FSurvivalConvoyRoutePlan M_Plan;
	bool bM_TakeShots = false;
	TArray<FShot> M_Shots;
	int32 M_NextShot = 0;
	int32 M_ShotPhase = 0;
	double M_ShotPhaseStartReal = 0.0;
	TWeakObjectPtr<ACameraActor> M_Camera;
	FVector M_ShotEye = FVector::ZeroVector;
	FRotator M_ShotRotation = FRotator::ZeroRotator;
	float M_ShotFieldOfView = 0.f;
	double M_ShotConvoySeconds = 0.0;
	TArray<TSharedPtr<FJsonValue>> M_CameraLog;
	FString M_Outcome = TEXT("none");

	// T0
	TArray<FSurvivalConvoyCalibrationUnit> M_CalibrationUnits;
	int32 M_CalibrationLeg = 0;
	double M_LegStartWorld = 0.0;
	int32 M_PendingCalibrationSpawns = 0;
	FString M_CalibrationCsv;
	float M_CalibrationPace = 0.f;
	float M_CalibrationLaneLength = SurvivalConvoyTestConstants::CalibrationLaneLength;

	// T2 / T4: scripted damage on top of the T1 setup, each action once.
	bool bM_DidDamageTruck = false;
	bool bM_DidKillTruck = false;
	bool bM_DidKillEscort = false;
	bool bM_DidKillSquad = false;

	// PalmCrush
	TWeakObjectPtr<AActor> M_Palm;
	TWeakObjectPtr<AActor> M_PalmTank;
	FVector M_PalmOutward = FVector::ZeroVector;
	int32 M_PalmPhase = 0;
	double M_PalmPhaseStart = 0.0;
	double M_NextPalmShot = 0.0;
	int32 M_PalmShotIndex = 0;
	float M_PalmClosest = TNumericLimits<float>::Max();

	UWorld* GetWorld() const;
	double GetWorldSeconds() const;
	bool Tick(float DeltaSeconds);
	void Finish(const FString& Reason);
	void TryPressStart();
	void AdvanceTo(EStep Step);
	void StartRun();
	void LogLine(const FString& Name, const FString& Detail);

	// T1
	void StartConvoyRun();
	void TickConvoyRun();
	void SampleConvoy(double ConvoySeconds);
	void AppendSoldierRows(const FConvoyMemberSnapshot& Snapshot, double ConvoySeconds, const AConvoy& Convoy);
	void WriteRouteFile() const;
	void TickScriptedActions(double ConvoySeconds);
	bool ApplyScriptedAction(EConvoyUnitRole Role, bool bPickLead, float HealthFraction, const TCHAR* Label);
	void WriteCensus() const;
	void WriteFiles() const;
	void QueueShots();
	void TickShots();
	bool ComputeShotView(const FShot& Shot);
	void SetHeldView();

	// T0
	void StartCalibration();
	void BuildCalibrationUnits(const FVector& Origin, const FVector& Direction);
	void SpawnCalibrationUnits();
	void TickCalibration();
	void StartCalibrationLeg(int32 Leg);
	bool SampleCalibrationLeg();
	void WriteCalibrationLeg(int32 Leg);
	bool FindFlatStretch(FVector& OutOrigin, FVector& OutDirection) const;

	// Sweep
	void RunSweep();

	// PalmCrush
	void StartPalmCrush();
	void TickPalmCrush();
	void SpawnPalmTank(ETankSubtype Subtype);
	float GetPalmTilt() const;
	void LogPalmState(const TCHAR* Label);
};

UWorld* FSurvivalConvoyTestRunner::GetWorld() const
{
	return M_Subsystem.IsValid() ? M_Subsystem->GetWorld() : nullptr;
}

double FSurvivalConvoyTestRunner::GetWorldSeconds() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0;
}

void FSurvivalConvoyTestRunner::LogLine(const FString& Name, const FString& Detail)
{
	UE_LOG(LogSurvivalEvents, Display, TEXT("CONVOY_TEST %s %s"), *Name, *Detail);
	M_EventsCsv += FString::Printf(TEXT("%.3f,-1,%s,-1,\"%s\"\n"), GetWorldSeconds() - M_ConvoyStartWorld, *Name,
	                               *Detail.Replace(TEXT("\""), TEXT("'")));
}

void FSurvivalConvoyTestRunner::Start(USurvivalEventSubsystem* Subsystem)
{
	M_Subsystem = Subsystem;
	FParse::Value(FCommandLine::Get(), TEXT("ConvoyTest="), M_Mode);
	if (not FParse::Value(FCommandLine::Get(), TEXT("ConvoyTestDir="), M_Directory))
	{
		M_Directory = FPaths::ProjectSavedDir() / TEXT("ConvoyTest");
	}
	FParse::Value(FCommandLine::Get(), TEXT("ConvoyTestTimeout="), M_TimeoutRealSeconds);
	bM_TakeShots = FParse::Param(FCommandLine::Get(), TEXT("ConvoyShots"));
	FParse::Value(FCommandLine::Get(), TEXT("ConvoyCalibrationLane="), M_CalibrationLaneLength);
	IFileManager::Get().MakeDirectory(*M_Directory, true);
	M_StartRealSeconds = FPlatformTime::Seconds();
	M_EventsCsv = TEXT("t,convoy_id,event,unit_id,detail\n");
	M_UnitsCsv = TEXT("t,convoy_id,unit_id,class,role,slot,x,y,z,speed2d,calib_speed,applied_speed_mult,active_command,"
		"convoy_state,slot_x,slot_y,slot_err,path_state,health,member_state,wanted_speed,route_s,slot_s,march_montage\n");
	TWeakPtr<FSurvivalConvoyTestRunner> WeakRunner = AsShared();
	Subsystem->OnEventLog.AddLambda([WeakRunner](const FString& Name, const FString& Detail)
	{
		if (const TSharedPtr<FSurvivalConvoyTestRunner> Runner = WeakRunner.Pin())
		{
			Runner->LogLine(Name, Detail);
		}
	});
	M_TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &FSurvivalConvoyTestRunner::Tick));
	UE_LOG(LogSurvivalEvents, Display, TEXT("CONVOY_TEST started mode=%s dir=%s"), *M_Mode, *M_Directory);
}

void FSurvivalConvoyTestRunner::TryPressStart()
{
	if (bM_PressedStart)
	{
		return;
	}
	ACPPController* Controller = Cast<ACPPController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	UPlayerStartGameControl* StartControl = IsValid(Controller) ? Controller->FindComponentByClass<UPlayerStartGameControl>() : nullptr;
	if (not IsValid(StartControl))
	{
		return;
	}
	if (StartControl->GetIsWaitingForPlayerToStart())
	{
		// As the player's "Press To Start" does (MapSmokeTestSubsystem::TryStartGame).
		StartControl->PlayerStartedGame();
	}
	bM_PressedStart = true;
}

void FSurvivalConvoyTestRunner::AdvanceTo(const EStep Step)
{
	M_Step = Step;
	M_StepStartWorld = GetWorldSeconds();
}

bool FSurvivalConvoyTestRunner::Tick(float)
{
	using namespace SurvivalConvoyTestConstants;
	UWorld* World = GetWorld();
	if (World == nullptr or not M_Subsystem.IsValid())
	{
		Finish(TEXT("world gone"));
		return false;
	}
	if (FPlatformTime::Seconds() - M_StartRealSeconds > M_TimeoutRealSeconds)
	{
		Finish(TEXT("timeout"));
		return false;
	}
	TryPressStart();
	if (World->IsPaused())
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
	const ASurvivalModeDirector* Director = M_Subsystem->GetDirector();
	switch (M_Step)
	{
	case EStep::WaitForGrace:
		if (IsValid(Director) and Director->GetPhase() >= ESurvivalPhase::GracePeriod)
		{
			AdvanceTo(EStep::Settle);
		}
		break;
	case EStep::Settle:
		if (GetWorldSeconds() - M_StepStartWorld >= SettleAfterGraceSeconds)
		{
			StartRun();
		}
		break;
	case EStep::Run:
		if (M_Mode == TEXT("T0"))
		{
			TickCalibration();
		}
		else if (M_Mode == TEXT("Watch"))
		{
			// Only the scheduler's own log lines; the run ends at the timeout.
		}
		else if (M_Mode == TEXT("PalmCrush"))
		{
			TickPalmCrush();
		}
		else
		{
			TickConvoyRun();
		}
		break;
	case EStep::Linger:
		TickConvoyRun();
		if (GetWorldSeconds() - M_StepStartWorld >= LingerAfterEndSeconds)
		{
			WriteCensus();
			Finish(M_Outcome);
			return false;
		}
		break;
	default:
		return false;
	}
	return M_Step != EStep::Finished;
}

void FSurvivalConvoyTestRunner::StartRun()
{
	AdvanceTo(EStep::Run);
	if (GEngine != nullptr)
	{
		// Epic everywhere, as the other tracks' views; the fog of war is hidden only in held views.
		GEngine->Exec(GetWorld(), TEXT("scalability 3"));
		GEngine->Exec(GetWorld(), TEXT("r.TextureStreaming 0"));
	}
	if (M_Mode == TEXT("T0"))
	{
		StartCalibration();
		return;
	}
	if (M_Mode == TEXT("Sweep"))
	{
		RunSweep();
		Finish(TEXT("sweep done"));
		return;
	}
	if (M_Mode == TEXT("Watch"))
	{
		LogLine(TEXT("WatchStarted"), TEXT("scheduler runs unforced"));
		return;
	}
	if (M_Mode == TEXT("PalmCrush"))
	{
		StartPalmCrush();
		return;
	}
	StartConvoyRun();
}

void FSurvivalConvoyTestRunner::Finish(const FString& Reason)
{
	if (M_Step == EStep::Finished)
	{
		return;
	}
	M_Step = EStep::Finished;
	LogLine(TEXT("TestFinished"), Reason);
	WriteFiles();
	FTSTicker::GetCoreTicker().RemoveTicker(M_TickerHandle);
	UE_LOG(LogSurvivalEvents, Display, TEXT("CONVOY_TEST finished %s"), *Reason);
	FPlatformMisc::RequestExit(false, TEXT("SurvivalConvoyTest"));
}

void FSurvivalConvoyTestRunner::WriteFiles() const
{
	FFileHelper::SaveStringToFile(M_UnitsCsv, *(M_Directory / TEXT("units.csv")));
	FFileHelper::SaveStringToFile(M_EventsCsv, *(M_Directory / TEXT("events.csv")));
	if (not M_CalibrationCsv.IsEmpty())
	{
		FFileHelper::SaveStringToFile(M_CalibrationCsv, *(M_Directory / TEXT("calibration.csv")));
	}
	const TSharedRef<FJsonObject> Cameras = MakeShared<FJsonObject>();
	Cameras->SetArrayField(TEXT("shots"), M_CameraLog);
	FFileHelper::SaveStringToFile(SurvivalConvoyTest_Json(Cameras), *(M_Directory / TEXT("cameras.json")));
}

// ---------------------------------------------------------------------------
// T1: one convoy on a planned route
// ---------------------------------------------------------------------------

void FSurvivalConvoyTestRunner::StartConvoyRun()
{
	USurvivalEventSubsystem* Subsystem = M_Subsystem.Get();
	int32 DefinitionIndex = 0;
	int32 Tier = 1;
	FString Entry;
	FString Exit;
	FParse::Value(FCommandLine::Get(), TEXT("ConvoyDefinition="), DefinitionIndex);
	FParse::Value(FCommandLine::Get(), TEXT("ConvoyTier="), Tier);
	FParse::Value(FCommandLine::Get(), TEXT("ConvoyEntry="), Entry);
	FParse::Value(FCommandLine::Get(), TEXT("ConvoyExit="), Exit);
	if (not Subsystem->GetDefinitions().IsValidIndex(DefinitionIndex))
	{
		Finish(TEXT("no such definition"));
		return;
	}
	UConvoyDefinition* Definition = Subsystem->GetDefinitions()[DefinitionIndex];
	M_Plan = Subsystem->PlanRoute(Subsystem->GetMatchSeed(), Definition, Entry, Exit);
	LogLine(TEXT("TestRoute"), FString::Printf(TEXT("valid=%d id=%s length=%.0f closest=%.0f soviet=%.0f exclusion=%.0f notes=%s"),
	                                         M_Plan.bIsValid ? 1 : 0, *M_Plan.Route.RouteId.ToString(),
	                                         M_Plan.Metrics.Length, M_Plan.Metrics.ClosestApproach,
	                                         M_Plan.Metrics.MinSovietBaseDistance, M_Plan.Metrics.MinExclusionDistance,
	                                         *M_Plan.Notes));
	WriteRouteFile();
	if (not M_Plan.bIsValid)
	{
		Finish(TEXT("no valid route"));
		return;
	}
	M_ConvoyStartWorld = GetWorldSeconds();
	AConvoy* Convoy = Subsystem->StartConvoyEvent(Definition, Tier, 1.f, M_Plan, 0, true);
	if (not IsValid(Convoy))
	{
		Finish(TEXT("convoy did not start"));
		return;
	}
	M_Convoy = Convoy;
	TWeakPtr<FSurvivalConvoyTestRunner> WeakRunner = AsShared();
	Convoy->OnConvoyEventNative.AddLambda([WeakRunner](const FConvoyEvent& Event)
	{
		const TSharedPtr<FSurvivalConvoyTestRunner> Runner = WeakRunner.Pin();
		if (not Runner.IsValid())
		{
			return;
		}
		Runner->M_EventsCsv += FString::Printf(TEXT("%.3f,%d,%s,%d,\"%s\"\n"),
		                                       Event.WorldSeconds - Runner->M_ConvoyStartWorld, Event.ConvoyId,
		                                       *ConvoyNames::GetEventName(Event.Type), Event.MemberId,
		                                       *Event.Detail.Replace(TEXT("\""), TEXT("'")));
		if (Event.Type == EConvoyEventType::Finished)
		{
			Runner->M_Outcome = Event.Detail;
		}
	});
	if (bM_TakeShots)
	{
		QueueShots();
	}
}

void FSurvivalConvoyTestRunner::WriteRouteFile() const
{
	const TSharedRef<FJsonObject> Route = MakeShared<FJsonObject>();
	Route->SetStringField(TEXT("id"), M_Plan.Route.RouteId.ToString());
	Route->SetStringField(TEXT("entry"), M_Plan.EntryName);
	Route->SetStringField(TEXT("exit"), M_Plan.ExitName);
	Route->SetNumberField(TEXT("length"), M_Plan.Metrics.Length);
	Route->SetNumberField(TEXT("closest_approach"), M_Plan.Metrics.ClosestApproach);
	Route->SetNumberField(TEXT("min_soviet_base_distance"), M_Plan.Metrics.MinSovietBaseDistance);
	Route->SetNumberField(TEXT("min_exclusion_clearance"), M_Plan.Metrics.MinExclusionDistance);
	Route->SetStringField(TEXT("notes"), M_Plan.Notes);
	TArray<TSharedPtr<FJsonValue>> Points;
	for (const FVector& Point : M_Plan.Route.Points)
	{
		Points.Add(MakeShared<FJsonValueArray>(SurvivalConvoyTest_Vector(Point)));
	}
	Route->SetArrayField(TEXT("points"), Points);
	if (const ASurvivalModeDirector* Director = M_Subsystem.IsValid() ? M_Subsystem->GetDirector() : nullptr)
	{
		const ASurvivalStartLocation* Start = Director->GetChosenStartLocation();
		Route->SetArrayField(TEXT("base_centre"), SurvivalConvoyTest_Vector(IsValid(Start) ? Start->GetDeployCenter() : FVector::ZeroVector));
	}
	FFileHelper::SaveStringToFile(SurvivalConvoyTest_Json(Route), *(M_Directory / TEXT("route.json")));
}

void FSurvivalConvoyTestRunner::TickConvoyRun()
{
	using namespace SurvivalConvoyTestConstants;
	const double Now = GetWorldSeconds();
	const double ConvoySeconds = Now - M_ConvoyStartWorld;
	if (Now >= M_NextSampleWorld)
	{
		M_NextSampleWorld = Now + SampleIntervalSeconds;
		SampleConvoy(ConvoySeconds);
	}
	TickShots();
	if (M_Step == EStep::Run)
	{
		TickScriptedActions(ConvoySeconds);
	}
	const AConvoy* Convoy = M_Convoy.Get();
	const bool bEnded = not IsValid(Convoy) or Convoy->GetOutcome() != EConvoyOutcome::None;
	if (M_Step == EStep::Run and bEnded)
	{
		LogLine(TEXT("ConvoyEnded"), M_Outcome);
		AdvanceTo(EStep::Linger);
	}
}

void FSurvivalConvoyTestRunner::TickScriptedActions(const double ConvoySeconds)
{
	using namespace SurvivalConvoyTestConstants;
	const AConvoy* Convoy = M_Convoy.Get();
	if (not IsValid(Convoy) or ConvoySeconds < ScriptedFirstActionSeconds or M_Plan.Metrics.Length <= 0.f)
	{
		return;
	}
	if (M_Mode == TEXT("T2") and not bM_DidDamageTruck)
	{
		// The rearmost truck at 40 % health from the start; damage instigated by no player.
		bM_DidDamageTruck = ApplyScriptedAction(EConvoyUnitRole::Truck, false, T2TruckHealthFraction, TEXT("TestDamage"));
		return;
	}
	if (M_Mode != TEXT("T4"))
	{
		return;
	}
	TArray<FConvoyMemberSnapshot> Snapshots;
	Convoy->GetMemberSnapshots(Snapshots);
	float LeadDistance = 0.f;
	for (const FConvoyMemberSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.Role == EConvoyUnitRole::Truck and Snapshot.State != EConvoyMemberState::Dead)
		{
			LeadDistance = FMath::Max(LeadDistance, Snapshot.RouteDistance);
		}
	}
	const float Progress = LeadDistance / M_Plan.Metrics.Length;
	if (not bM_DidKillTruck and Progress >= T4KillTruckProgress)
	{
		bM_DidKillTruck = ApplyScriptedAction(EConvoyUnitRole::Truck, true, 0.f, TEXT("TestKill"));
	}
	if (not bM_DidKillEscort and Progress >= T4KillEscortProgress)
	{
		bM_DidKillEscort = ApplyScriptedAction(EConvoyUnitRole::EscortVehicle, true, 0.f, TEXT("TestKill"));
	}
	if (not bM_DidKillSquad and Progress >= T4KillSquadProgress)
	{
		bM_DidKillSquad = ApplyScriptedAction(EConvoyUnitRole::EscortSquad, true, 0.f, TEXT("TestKill"));
	}
}

bool FSurvivalConvoyTestRunner::ApplyScriptedAction(const EConvoyUnitRole Role, const bool bPickLead,
                                                    const float HealthFraction, const TCHAR* Label)
{
	using namespace SurvivalConvoyTestConstants;
	const AConvoy* Convoy = M_Convoy.Get();
	TArray<FConvoyMemberSnapshot> Snapshots;
	Convoy->GetMemberSnapshots(Snapshots);
	const FConvoyMemberSnapshot* Picked = nullptr;
	for (const FConvoyMemberSnapshot& Snapshot : Snapshots)
	{
		const bool bUsable = Snapshot.Role == Role and Snapshot.Actor.IsValid() and Snapshot.State != EConvoyMemberState::Dead
			and Snapshot.State != EConvoyMemberState::Released;
		const bool bBetter = Picked == nullptr or (bPickLead ? Snapshot.RouteDistance > Picked->RouteDistance
			                                                  : Snapshot.RouteDistance < Picked->RouteDistance);
		if (bUsable and bBetter)
		{
			Picked = &Snapshot;
		}
	}
	if (Picked == nullptr)
	{
		return false;
	}
	// A squad dies soldier by soldier; vehicles take one hit. No instigator: no player gets credit.
	TArray<AActor*> Targets;
	if (ASquadController* Squad = Cast<ASquadController>(Picked->Actor.Get()))
	{
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			Targets.Add(Soldier);
		}
	}
	else
	{
		Targets.Add(Picked->Actor.Get());
	}
	for (AActor* Target : Targets)
	{
		const UHealthComponent* Health = IsValid(Target) ? Target->FindComponentByClass<UHealthComponent>() : nullptr;
		const float Damage = HealthFraction > 0.f and Health != nullptr
			                     ? FMath::Max(0.f, Health->GetCurrentHealth() - HealthFraction * Health->GetMaxHealth())
			                     : ScriptedKillDamage;
		UGameplayStatics::ApplyDamage(Target, Damage, nullptr, nullptr, UDamageType::StaticClass());
	}
	LogLine(Label, FString::Printf(TEXT("m%d %s route_s=%.0f health_after=%.2f"), Picked->MemberId,
	                               *Picked->Actor->GetClass()->GetName(), Picked->RouteDistance,
	                               SurvivalConvoyTest_Health(Picked->Actor.Get())));
	return true;
}

void FSurvivalConvoyTestRunner::SampleConvoy(const double ConvoySeconds)
{
	const AConvoy* Convoy = M_Convoy.Get();
	if (not IsValid(Convoy))
	{
		return;
	}
	TArray<FConvoyMemberSnapshot> Snapshots;
	Convoy->GetMemberSnapshots(Snapshots);
	const FString ConvoyState = ConvoyNames::GetStateName(Convoy->GetConvoyState());
	for (const FConvoyMemberSnapshot& Snapshot : Snapshots)
	{
		AActor* Actor = Snapshot.Actor.Get();
		if (not IsValid(Actor) or Snapshot.State == EConvoyMemberState::Dead or Snapshot.State == EConvoyMemberState::Released)
		{
			continue;
		}
		M_SeenUnits.AddUnique(Actor);
		const FString Role = Snapshot.Role == EConvoyUnitRole::Truck
			                     ? TEXT("truck")
			                     : Snapshot.Role == EConvoyUnitRole::EscortVehicle ? TEXT("escort_vehicle") : TEXT("squad");
		const float SlotError = FVector::Dist2D(Snapshot.Location, Snapshot.SlotLocation);
		const FString PathState = Snapshot.bHasPartialPath ? TEXT("partial") : TEXT("full");
		const float Speed = Actor->GetVelocity().Size2D();
		M_UnitsCsv += FString::Printf(
			TEXT("%.3f,%d,m%d,%s,%s,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%s,%s,%.1f,%.1f,%.1f,%s,%.3f,%s,%.1f,%.1f,%.1f,\n"),
			ConvoySeconds, Convoy->GetConvoyId(), Snapshot.MemberId, *Actor->GetClass()->GetName(), *Role,
			*Snapshot.SlotName, Snapshot.Location.X, Snapshot.Location.Y, Snapshot.Location.Z,
			Snapshot.Role == EConvoyUnitRole::EscortSquad ? 0.f : Speed, Snapshot.NominalSpeed,
			Snapshot.AppliedSpeedFraction, *SurvivalConvoyTest_Command(Actor), *ConvoyState, Snapshot.SlotLocation.X,
			Snapshot.SlotLocation.Y, SlotError, *PathState, SurvivalConvoyTest_Health(Actor),
			*ConvoyNames::GetMemberStateName(Snapshot.State), Snapshot.WantedSpeed, Snapshot.RouteDistance,
			Snapshot.SlotRouteDistance);
		if (Snapshot.Role == EConvoyUnitRole::EscortSquad)
		{
			AppendSoldierRows(Snapshot, ConvoySeconds, *Convoy);
		}
	}
}

void FSurvivalConvoyTestRunner::AppendSoldierRows(const FConvoyMemberSnapshot& Snapshot, const double ConvoySeconds,
                                                  const AConvoy& Convoy)
{
	ASquadController* Squad = Cast<ASquadController>(Snapshot.Actor.Get());
	if (not IsValid(Squad))
	{
		return;
	}
	int32 SoldierIndex = 0;
	for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		if (not IsValid(Soldier))
		{
			continue;
		}
		const USquadUnitAnimInstance* Anim = Soldier->GetMesh() != nullptr
			                                     ? Cast<USquadUnitAnimInstance>(Soldier->GetMesh()->GetAnimInstance())
			                                     : nullptr;
		const UCharacterMovementComponent* Movement = Soldier->GetCharacterMovement();
		const FVector Location = Soldier->GetActorLocation();
		M_UnitsCsv += FString::Printf(
			TEXT("%.3f,%d,m%d_s%d,%s,soldier,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%s,%s,,,,,%.3f,,,,,%d\n"),
			ConvoySeconds, Convoy.GetConvoyId(), Snapshot.MemberId, SoldierIndex++, *Soldier->GetClass()->GetName(),
			*Snapshot.SlotName, Location.X, Location.Y, Location.Z, Soldier->GetVelocity().Size2D(),
			Snapshot.NominalSpeed, Movement != nullptr ? Movement->MaxWalkSpeed : 0.f,
			*Global_GetAbilityIDAsString(Soldier->GetActiveCommand()), *ConvoyNames::GetStateName(Convoy.GetConvoyState()),
			SurvivalConvoyTest_Health(Soldier), Anim != nullptr and Anim->GetIsMarchAnimationPlaying() ? 1 : 0);
	}
}

void FSurvivalConvoyTestRunner::WriteCensus() const
{
	int32 AliveUnits = 0;
	FString Lines;
	for (const TWeakObjectPtr<AActor>& Unit : M_SeenUnits)
	{
		if (Unit.IsValid() and RTSFunctionLibrary::RTSIsValid(Unit.Get()))
		{
			++AliveUnits;
			Lines += FString::Printf(TEXT("alive %s at %s command=%s\n"), *Unit->GetName(),
			                         *Unit->GetActorLocation().ToCompactString(), *SurvivalConvoyTest_Command(Unit.Get()));
		}
	}
	int32 OpenCaches = 0;
	if (const AConvoy* Convoy = M_Convoy.Get())
	{
		for (const FConvoyCargoCache& Cache : Convoy->GetCargoCaches())
		{
			OpenCaches += Cache.Cache.IsValid() ? 1 : 0;
		}
	}
	const FString Text = FString::Printf(TEXT("convoy_units_seen=%d\nconvoy_units_alive=%d\nconvoy_actor_alive=%d\ncargo_caches_alive=%d\n%s"),
	                                     M_SeenUnits.Num(), AliveUnits, M_Convoy.IsValid() ? 1 : 0, OpenCaches, *Lines);
	FFileHelper::SaveStringToFile(Text, *(M_Directory / TEXT("census_after.txt")));
}

// ---------------------------------------------------------------------------
// Frames
// ---------------------------------------------------------------------------

void FSurvivalConvoyTestRunner::QueueShots()
{
	using namespace SurvivalConvoyTestConstants;
	for (int32 Index = 0; Index < FollowFrames; ++Index)
	{
		M_Shots.Add({FollowFirstSeconds + Index * FollowStepSeconds, FString::Printf(TEXT("raw/follow_%02d.png"), Index), EShotKind::Follow});
	}
	for (int32 Index = 0; Index < MarchFrames; ++Index)
	{
		M_Shots.Add({MarchFirstSeconds + Index * MarchStepSeconds, FString::Printf(TEXT("raw/march_%02d.png"), Index), EShotKind::MarchClose});
	}
	for (int32 Index = 0; Index < RtsFrames; ++Index)
	{
		M_Shots.Add({RtsFirstSeconds + Index * RtsStepSeconds, FString::Printf(TEXT("raw/rts_%02d.png"), Index), EShotKind::Rts});
	}
	M_Shots.Sort([](const FShot& A, const FShot& B)
	{
		return A.AtConvoySeconds < B.AtConvoySeconds;
	});
	IFileManager::Get().MakeDirectory(*(M_Directory / TEXT("raw")), true);
}

bool FSurvivalConvoyTestRunner::ComputeShotView(const FShot& Shot)
{
	using namespace SurvivalConvoyTestConstants;
	const AConvoy* Convoy = M_Convoy.Get();
	if (not IsValid(Convoy))
	{
		return false;
	}
	TArray<FConvoyMemberSnapshot> Snapshots;
	Convoy->GetMemberSnapshots(Snapshots);
	FBox Bounds(ForceInit);
	const FConvoyMemberSnapshot* FirstSquad = nullptr;
	for (const FConvoyMemberSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.Actor.IsValid() and Snapshot.State != EConvoyMemberState::Dead)
		{
			Bounds += Snapshot.Location;
			FirstSquad = FirstSquad == nullptr and Snapshot.Role == EConvoyUnitRole::EscortSquad ? &Snapshot : FirstSquad;
		}
	}
	if (not Bounds.IsValid)
	{
		return false;
	}
	const FVector Centre = Bounds.GetCenter();
	const FVector Heading = Convoy->GetRoutePolyline().GetDirectionAtDistance(Convoy->GetColumnReferenceDistance());
	switch (Shot.Kind)
	{
	case EShotKind::Follow:
	{
		const float Span = FMath::Max(MinFollowSpanCm, static_cast<float>(Bounds.GetSize().GetMax()) + FollowMarginCm);
		const float Height = 0.5f * Span / FMath::Tan(FMath::DegreesToRadians(FollowFieldOfView * 0.5f));
		M_ShotEye = FVector(Centre.X, Centre.Y, Bounds.Max.Z + Height);
		// Straight down, +X (north) up in the frame: the overlay projects world XY straight onto the image.
		M_ShotRotation = FRotator(-90.f, 0.f, 0.f);
		M_ShotFieldOfView = FollowFieldOfView;
		return true;
	}
	case EShotKind::Rts:
	{
		const FRotator Rotation(RtsPitch, Heading.Rotation().Yaw, 0.f);
		M_ShotEye = Centre - Rotation.Vector() * RtsArm;
		M_ShotRotation = Rotation;
		M_ShotFieldOfView = RtsFieldOfView;
		return true;
	}
	default:
	{
		if (FirstSquad == nullptr)
		{
			return false;
		}
		const FVector Side(-Heading.Y, Heading.X, 0.f);
		M_ShotEye = FirstSquad->Location + Side * MarchSideDistance + FVector(0.f, 0.f, MarchEyeHeight);
		M_ShotRotation = (FirstSquad->Location - M_ShotEye).Rotation();
		M_ShotFieldOfView = MarchFieldOfView;
		return true;
	}
	}
}

void FSurvivalConvoyTestRunner::SetHeldView()
{
	UWorld* World = GetWorld();
	if (not M_Camera.IsValid())
	{
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Parameters);
		if (not IsValid(Camera))
		{
			return;
		}
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		M_Camera = Camera;
		if (GEngine != nullptr)
		{
			// The fog of war is a post-process material: held views show the ground.
			GEngine->Exec(World, TEXT("ShowFlag.PostProcessMaterial 0"));
		}
	}
	M_Camera->SetActorLocationAndRotation(M_ShotEye, M_ShotRotation);
	M_Camera->GetCameraComponent()->SetFieldOfView(M_ShotFieldOfView);
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	if (IsValid(PlayerController) and PlayerController->GetViewTarget() != M_Camera.Get())
	{
		PlayerController->SetViewTarget(M_Camera.Get());
	}
}

void FSurvivalConvoyTestRunner::TickShots()
{
	using namespace SurvivalConvoyTestConstants;
	if (not M_Shots.IsValidIndex(M_NextShot))
	{
		return;
	}
	const FShot& Shot = M_Shots[M_NextShot];
	const double ConvoySeconds = GetWorldSeconds() - M_ConvoyStartWorld;
	const double RealNow = FPlatformTime::Seconds();
	const FString Path = M_Directory / Shot.FileName;
	switch (M_ShotPhase)
	{
	case 0:
		if (ConvoySeconds < Shot.AtConvoySeconds)
		{
			return;
		}
		if (not ComputeShotView(Shot))
		{
			++M_NextShot;
			return;
		}
		SetHeldView();
		M_ShotPhase = 1;
		M_ShotPhaseStartReal = RealNow;
		return;
	case 1:
		if (RealNow - M_ShotPhaseStartReal < ShotSettleSeconds)
		{
			return;
		}
		// Recompute right before the request so the logged t matches the frame.
		ComputeShotView(Shot);
		SetHeldView();
		M_ShotConvoySeconds = ConvoySeconds;
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		M_ShotPhase = 2;
		M_ShotPhaseStartReal = RealNow;
		return;
	default:
		if (not IFileManager::Get().FileExists(*Path) and RealNow - M_ShotPhaseStartReal < ShotWriteTimeoutSeconds)
		{
			return;
		}
		{
			const TSharedRef<FJsonObject> Camera = MakeShared<FJsonObject>();
			Camera->SetStringField(TEXT("file"), Shot.FileName);
			Camera->SetNumberField(TEXT("t"), M_ShotConvoySeconds);
			Camera->SetArrayField(TEXT("eye"), SurvivalConvoyTest_Vector(M_ShotEye));
			Camera->SetArrayField(TEXT("rotation_pyr"), SurvivalConvoyTest_Vector(
				                      FVector(M_ShotRotation.Pitch, M_ShotRotation.Yaw, M_ShotRotation.Roll)));
			Camera->SetNumberField(TEXT("fov_horizontal"), M_ShotFieldOfView);
			Camera->SetBoolField(TEXT("written"), IFileManager::Get().FileExists(*Path));
			M_CameraLog.Add(MakeShared<FJsonValueObject>(Camera));
		}
		M_ShotPhase = 0;
		++M_NextShot;
		return;
	}
}

// ---------------------------------------------------------------------------
// T0: calibration
// ---------------------------------------------------------------------------

bool FSurvivalConvoyTestRunner::FindFlatStretch(FVector& OutOrigin, FVector& OutDirection) const
{
	using namespace SurvivalConvoyTestConstants;
	const TArray<FVector>& Points = M_Plan.Route.Points;
	FConvoyRoutePolyline Polyline;
	Polyline.Build(Points);
	if (Polyline.GetLength() < M_CalibrationLaneLength * 2.f)
	{
		return false;
	}
	// The straightest, flattest lane (8 000 cm unless -ConvoyCalibrationLane=) of the route, away from its ends.
	constexpr float SearchStep = 500.f;
	constexpr int32 HeightSamples = 9;
	float BestScore = TNumericLimits<float>::Max();
	for (float Start = M_CalibrationLaneLength; Start + M_CalibrationLaneLength < Polyline.GetLength() - M_CalibrationLaneLength;
	     Start += SearchStep)
	{
		const FVector A = Polyline.GetPointAtDistance(Start);
		const FVector B = Polyline.GetPointAtDistance(Start + M_CalibrationLaneLength);
		float MinZ = TNumericLimits<float>::Max();
		float MaxZ = -TNumericLimits<float>::Max();
		float Deviation = 0.f;
		for (int32 Sample = 0; Sample <= HeightSamples; ++Sample)
		{
			const FVector P = Polyline.GetPointAtDistance(Start + M_CalibrationLaneLength * Sample / HeightSamples);
			MinZ = FMath::Min(MinZ, static_cast<float>(P.Z));
			MaxZ = FMath::Max(MaxZ, static_cast<float>(P.Z));
			Deviation = FMath::Max(Deviation, static_cast<float>(FMath::PointDistToSegment(P, A, B)));
		}
		const float Score = (MaxZ - MinZ) + Deviation;
		if (Score < BestScore)
		{
			BestScore = Score;
			OutOrigin = A;
			OutDirection = (B - A).GetSafeNormal2D();
		}
	}
	return BestScore < TNumericLimits<float>::Max();
}

void FSurvivalConvoyTestRunner::BuildCalibrationUnits(const FVector& Origin, const FVector& Direction)
{
	using namespace SurvivalConvoyTestConstants;
	TArray<FTrainingOption> SeenOptions;
	auto AddUnit = [&](const FTrainingOption& Option, const TSoftClassPtr<AActor>& ExactClass)
	{
		if (SeenOptions.Contains(Option))
		{
			return;
		}
		SeenOptions.Add(Option);
		FSurvivalConvoyCalibrationUnit& Unit = M_CalibrationUnits.AddDefaulted_GetRef();
		Unit.Name = Option.GetTrainingName();
		Unit.Option = Option;
		Unit.ExactClass = ExactClass;
		Unit.bIsSquad = Option.UnitType == EAllUnitType::UNType_Squad;
	};
	for (const UConvoyDefinition* Definition : M_Subsystem->GetDefinitions())
	{
		for (const FConvoyTruckSpec& Truck : Definition->Trucks)
		{
			AddUnit(Truck.TrainingOption, Truck.TruckClass);
		}
		for (const FConvoyGuardTier& Tier : Definition->GuardTiers)
		{
			for (const FConvoyEscortSpec& Escort : Tier.Escorts)
			{
				AddUnit(Escort.Unit, Escort.ClassOverride);
			}
		}
	}
	const FVector Side(-Direction.Y, Direction.X, 0.f);
	const int32 Count = M_CalibrationUnits.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FSurvivalConvoyCalibrationUnit& Unit = M_CalibrationUnits[Index];
		const float Lateral = (Index - 0.5f * (Count - 1)) * CalibrationLaneSpacing;
		Unit.LaneStart = Origin + Side * Lateral;
		Unit.LaneEnd = Unit.LaneStart + Direction * M_CalibrationLaneLength;
		UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), Unit.LaneStart, Unit.LaneStart);
		UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), Unit.LaneEnd, Unit.LaneEnd);
	}
}

void FSurvivalConvoyTestRunner::StartCalibration()
{
	USurvivalEventSubsystem* Subsystem = M_Subsystem.Get();
	if (Subsystem->GetDefinitions().IsEmpty())
	{
		Finish(TEXT("no definitions"));
		return;
	}
	M_Plan = Subsystem->PlanRoute(Subsystem->GetMatchSeed(), Subsystem->GetDefinitions()[0], FString(), FString());
	FVector Origin;
	FVector Direction;
	if (not M_Plan.bIsValid or not FindFlatStretch(Origin, Direction))
	{
		Finish(TEXT("no route for a calibration stretch"));
		return;
	}
	M_CalibrationPace = Subsystem->EstimatePaceSpeed(Subsystem->GetDefinitions()[0]);
	BuildCalibrationUnits(Origin, Direction);
	LogLine(TEXT("CalibrationStretch"), FString::Printf(TEXT("origin=%s direction=%s units=%d pace=%.0f"),
	                                                    *Origin.ToCompactString(), *Direction.ToCompactString(),
	                                                    M_CalibrationUnits.Num(), M_CalibrationPace));
	M_CalibrationCsv = TEXT("name,kind,leg,nominal_speed,cruise_p50,cruise_p10,cruise_p90,samples,ratio_to_nominal,arrived\n");
	SpawnCalibrationUnits();
	M_ConvoyStartWorld = GetWorldSeconds();
	M_CalibrationLeg = -1;
	M_LegStartWorld = GetWorldSeconds();
}

void FSurvivalConvoyTestRunner::SpawnCalibrationUnits()
{
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
	if (not IsValid(Spawner))
	{
		Finish(TEXT("no async spawner"));
		return;
	}
	TWeakPtr<FSurvivalConvoyTestRunner> WeakRunner = AsShared();
	for (int32 Index = 0; Index < M_CalibrationUnits.Num(); ++Index)
	{
		const FSurvivalConvoyCalibrationUnit& Unit = M_CalibrationUnits[Index];
		auto OnSpawned = [WeakRunner, Index](const FTrainingOption&, AActor* Spawned, const int32)
		{
			const TSharedPtr<FSurvivalConvoyTestRunner> Runner = WeakRunner.Pin();
			if (Runner.IsValid() and Runner->M_CalibrationUnits.IsValidIndex(Index))
			{
				Runner->M_CalibrationUnits[Index].Actor = Spawned;
			}
		};
		const FRotator Facing = (Unit.LaneEnd - Unit.LaneStart).Rotation();
		if (Unit.ExactClass.IsNull())
		{
			Spawner->AsyncSpawnOptionAtLocation(Unit.Option, Unit.LaneStart, M_Subsystem.Get(), Index, OnSpawned, Facing);
		}
		else
		{
			Spawner->AsyncSpawnClassAtLocation(Unit.ExactClass, Unit.Option, Unit.LaneStart, M_Subsystem.Get(), Index,
			                                   OnSpawned, Facing);
		}
	}
}

void FSurvivalConvoyTestRunner::StartCalibrationLeg(const int32 Leg)
{
	using namespace SurvivalConvoyTestConstants;
	M_CalibrationLeg = Leg;
	M_LegStartWorld = GetWorldSeconds();
	// Leg 0: free, start → end. Leg 1: restricted (vehicles at 0.5, squads march), end → start.
	// Leg 2: squads march at the convoy pace (the convoy's march hook), start → end.
	for (FSurvivalConvoyCalibrationUnit& Unit : M_CalibrationUnits)
	{
		Unit.Speeds.Reset();
		Unit.bArrived = false;
		Unit.PreviousTime = -1.0;
		AActor* Actor = Unit.Actor.Get();
		ICommands* Commands = Cast<ICommands>(Actor);
		if (Commands == nullptr or (Leg == 2 and not Unit.bIsSquad))
		{
			Unit.bArrived = true;
			continue;
		}
		const FVector Target = Leg == 1 ? Unit.LaneStart : Unit.LaneEnd;
		ATankMaster* Tank = Cast<ATankMaster>(Actor);
		ASquadController* Squad = Cast<ASquadController>(Actor);
		if (Tank != nullptr)
		{
			if (Leg == 1)
			{
				Tank->SetMobilityRestriction(M_Subsystem.Get(), CalibrationRestrictedFraction, 1.f, 1.f);
			}
			Commands->MoveToLocation(Target, true, (Target - Actor->GetActorLocation()).Rotation());
			continue;
		}
		if (Squad == nullptr)
		{
			continue;
		}
		Squad->SetMarchSpeedOverride(Leg == 2 ? M_CalibrationPace : 0.f);
		if (Leg == 0)
		{
			Commands->MoveToLocation(Target, true, FRotator::ZeroRotator);
		}
		else
		{
			Commands->MarchToLocation(Target, true);
		}
	}
}

bool FSurvivalConvoyTestRunner::SampleCalibrationLeg()
{
	using namespace SurvivalConvoyTestConstants;
	const double Now = GetWorldSeconds();
	bool bAllArrived = true;
	for (FSurvivalConvoyCalibrationUnit& Unit : M_CalibrationUnits)
	{
		const AActor* Actor = Unit.Actor.Get();
		if (Unit.bArrived or not IsValid(Actor))
		{
			continue;
		}
		const ASquadController* Squad = Cast<ASquadController>(Actor);
		const FVector Location = Squad != nullptr ? const_cast<ASquadController*>(Squad)->GetSquadLocation() : Actor->GetActorLocation();
		const FVector From = M_CalibrationLeg == 1 ? Unit.LaneEnd : Unit.LaneStart;
		const FVector To = M_CalibrationLeg == 1 ? Unit.LaneStart : Unit.LaneEnd;
		const float Progress = FVector::Dist2D(Location, From) / FMath::Max(1.f, static_cast<float>(FVector::Dist2D(From, To)));
		if (Unit.PreviousTime >= 0.0 and Now - Unit.PreviousTime >= SampleIntervalSeconds)
		{
			const float Speed = FVector::Dist2D(Location, Unit.PreviousLocation) / (Now - Unit.PreviousTime);
			if (Progress >= CalibrationMiddleStart and Progress <= CalibrationMiddleEnd)
			{
				Unit.Speeds.Add(Speed);
			}
			Unit.PreviousTime = Now;
			Unit.PreviousLocation = Location;
		}
		else if (Unit.PreviousTime < 0.0)
		{
			Unit.PreviousTime = Now;
			Unit.PreviousLocation = Location;
		}
		Unit.bArrived = FVector::Dist2D(Location, To) < CalibrationArriveDistance or Progress > CalibrationMiddleEnd + 0.1f;
		bAllArrived &= Unit.bArrived;
	}
	return bAllArrived or Now - M_LegStartWorld > CalibrationLegTimeoutSeconds;
}

void FSurvivalConvoyTestRunner::WriteCalibrationLeg(const int32 Leg)
{
	static const TCHAR* LegNames[] = {TEXT("free"), TEXT("restricted_or_march"), TEXT("march_at_convoy_pace")};
	for (FSurvivalConvoyCalibrationUnit& Unit : M_CalibrationUnits)
	{
		if (Leg == 2 and not Unit.bIsSquad)
		{
			continue;
		}
		const AActor* Actor = Unit.Actor.Get();
		if (const ATankMaster* Tank = Cast<ATankMaster>(Actor))
		{
			const URTSComponent* RTSComponent = Tank->FindComponentByClass<URTSComponent>();
			bool bIsValidData = false;
			const FTankData Data = RTSComponent != nullptr
				                       ? FRTS_Statics::BP_GetTankDataOfPlayer(RTSComponent->GetOwningPlayer(),
				                                                              RTSComponent->GetSubtypeAsTankSubtype(), Tank,
				                                                              bIsValidData)
				                       : FTankData();
			constexpr float KmhToCms = 100000.f / 3600.f;
			Unit.NominalSpeed = Data.VehicleMaxSpeedKmh * KmhToCms;
		}
		else if (const ASquadController* Squad = Cast<ASquadController>(Actor))
		{
			const TArray<ASquadUnit*> Soldiers = const_cast<ASquadController*>(Squad)->GetSquadUnitsChecked();
			const UCharacterMovementComponent* Movement = Soldiers.IsEmpty() ? nullptr : Soldiers[0]->GetCharacterMovement();
			Unit.NominalSpeed = Leg == 0 and Movement != nullptr ? Movement->MaxWalkSpeed : Unit.NominalSpeed;
		}
		const float Median = SurvivalConvoyTest_Median(Unit.Speeds);
		M_CalibrationCsv += FString::Printf(TEXT("%s,%s,%s,%.1f,%.1f,%.1f,%.1f,%d,%.3f,%d\n"), *Unit.Name,
		                                    Unit.bIsSquad ? TEXT("squad") : TEXT("vehicle"), LegNames[Leg],
		                                    Unit.NominalSpeed, Median, SurvivalConvoyTest_Percentile(Unit.Speeds, 0.1f),
		                                    SurvivalConvoyTest_Percentile(Unit.Speeds, 0.9f), Unit.Speeds.Num(),
		                                    Unit.NominalSpeed > 0.f ? Median / Unit.NominalSpeed : 0.f,
		                                    Unit.bArrived ? 1 : 0);
	}
	LogLine(TEXT("CalibrationLegDone"), LegNames[Leg]);
}

void FSurvivalConvoyTestRunner::TickCalibration()
{
	using namespace SurvivalConvoyTestConstants;
	constexpr int32 LegCount = 3;
	if (M_CalibrationLeg < 0)
	{
		if (GetWorldSeconds() - M_LegStartWorld >= CalibrationSpawnWaitSeconds)
		{
			StartCalibrationLeg(0);
		}
		return;
	}
	if (not SampleCalibrationLeg())
	{
		return;
	}
	WriteCalibrationLeg(M_CalibrationLeg);
	if (M_CalibrationLeg + 1 < LegCount)
	{
		StartCalibrationLeg(M_CalibrationLeg + 1);
		return;
	}
	for (FSurvivalConvoyCalibrationUnit& Unit : M_CalibrationUnits)
	{
		if (ATankMaster* Tank = Cast<ATankMaster>(Unit.Actor.Get()))
		{
			Tank->ClearMobilityRestriction(M_Subsystem.Get());
		}
		if (ASquadController* Squad = Cast<ASquadController>(Unit.Actor.Get()))
		{
			Squad->SetMarchSpeedOverride(0.f);
		}
	}
	Finish(TEXT("calibration done"));
}

// ---------------------------------------------------------------------------
// Sweep: the scheduler's decisions and routes, no spawning
// ---------------------------------------------------------------------------

void FSurvivalConvoyTestRunner::RunSweep()
{
	using namespace SurvivalConvoyTestConstants;
	USurvivalEventSubsystem* Subsystem = M_Subsystem.Get();
	int32 SeedCount = SweepDefaultSeeds;
	FParse::Value(FCommandLine::Get(), TEXT("ConvoySweepSeeds="), SeedCount);
	FString Csv = TEXT("seed,round,fires,forced,definition,tier,guard_points,cargo_total,cargo,route,length,closest,soviet,exclusion,valid,reason\n");
	const ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	for (int32 Seed = 1; Seed <= SeedCount; ++Seed)
	{
		int32 LastEventRound = -1000;
		int32 QuietRounds = 0;
		for (int32 Round = 1; Round <= SweepRounds; ++Round)
		{
			const FSurvivalEventRoll Roll = Subsystem->RollForRound(Round, LastEventRound, QuietRounds, Difficulty, Seed);
			if (not Roll.bFires)
			{
				QuietRounds += Round >= Subsystem->GetSettings()->FirstAllowedRound ? 1 : 0;
				Csv += FString::Printf(TEXT("%d,%d,0,0,,,,,,,,,,,,%s\n"), Seed, Round, *Roll.Reason);
				continue;
			}
			const UConvoyDefinition* Definition = Subsystem->GetDefinitions()[Roll.DefinitionIndex];
			const int32 RouteSeed = HashCombine(GetTypeHash(Seed), GetTypeHash(Round * SweepRouteSeedSalt));
			const FSurvivalConvoyRoutePlan Plan = Subsystem->PlanRoute(RouteSeed, Definition, FString(), FString());
			const FConvoyCargoValue Cargo = Definition->GetTotalCargo(Roll.GuardTier, Roll.CargoMultiplier);
			FString CargoText;
			for (const TPair<ERTSResourceType, int32>& Pair : Cargo.Resources)
			{
				CargoText += FString::Printf(TEXT("%s:%d "), *Global_GetResourceTypeAsString(Pair.Key), Pair.Value);
			}
			Csv += FString::Printf(TEXT("%d,%d,1,%d,%s,%d,%d,%d,%s,%s,%.0f,%.0f,%.0f,%.0f,%d,\"%s\"\n"), Seed, Round,
			                       Roll.bForced ? 1 : 0, *Definition->GetName(), Roll.GuardTier,
			                       Definition->GetGuardTrainingPoints(Roll.GuardTier), Cargo.GetTotal(), *CargoText,
			                       *Plan.Route.RouteId.ToString(), Plan.Metrics.Length, Plan.Metrics.ClosestApproach,
			                       Plan.Metrics.MinSovietBaseDistance, Plan.Metrics.MinExclusionDistance,
			                       Plan.bIsValid ? 1 : 0, *Plan.Notes.Replace(TEXT("\""), TEXT("'")));
			if (Plan.bIsValid)
			{
				LastEventRound = Round;
				QuietRounds = 0;
			}
			else
			{
				++QuietRounds;
			}
		}
	}
	FFileHelper::SaveStringToFile(Csv, *(M_Directory / TEXT("sweep.csv")));
}

// ---------------------------------------------------------------------------
// PalmCrush: a light tank, then a medium tank, ordered straight through one oasis palm
// ---------------------------------------------------------------------------

void FSurvivalConvoyTestRunner::StartPalmCrush()
{
	using namespace SurvivalConvoyTestConstants;
	// The big palms are BP_SMD_Palm_0N; pick the one with the most room around its drive line.
	TArray<AActor*> Palms;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().StartsWith(TEXT("BP_SMD_Palm")))
		{
			Palms.Add(*It);
		}
	}
	const FVector Flat(1.f, 1.f, 0.f);
	constexpr int32 Directions = 16;
	constexpr float WaterZ = 380.f;
	constexpr float MaxProjectionShift = 300.f;
	float BestRoom = -1.f;
	for (AActor* Palm : Palms)
	{
		if (not Palm->GetClass()->GetName().StartsWith(TEXT("BP_SMD_Palm_0")))
		{
			continue;
		}
		for (int32 Index = 0; Index < Directions; ++Index)
		{
			const float Angle = 2.f * PI * Index / Directions;
			const FVector Outward(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			FVector A = Palm->GetActorLocation() + Outward * PalmStartDistance;
			FVector B = Palm->GetActorLocation() - Outward * PalmOvershootDistance;
			FVector ProjectedA;
			FVector ProjectedB;
			// Both ends on dry, drivable ground: otherwise the order ends before the trunk.
			if (not UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), A, ProjectedA)
				or not UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), B, ProjectedB)
				or FVector::Dist2D(A, ProjectedA) > MaxProjectionShift or FVector::Dist2D(B, ProjectedB) > MaxProjectionShift
				or ProjectedA.Z < WaterZ or ProjectedB.Z < WaterZ)
			{
				continue;
			}
			float Room = TNumericLimits<float>::Max();
			for (const AActor* Other : Palms)
			{
				if (Other != Palm)
				{
					Room = FMath::Min(Room, static_cast<float>(FMath::PointDistToSegment(
						                                           Other->GetActorLocation() * Flat, A * Flat, B * Flat)));
				}
			}
			if (Room > BestRoom)
			{
				BestRoom = Room;
				M_Palm = Palm;
				M_PalmOutward = Outward;
			}
		}
	}
	if (not M_Palm.IsValid())
	{
		Finish(TEXT("no palm"));
		return;
	}
	LogLine(TEXT("PalmTarget"), FString::Printf(TEXT("%s at %s room=%.0f outward=%s palms=%d"), *M_Palm->GetName(),
	                                            *M_Palm->GetActorLocation().ToCompactString(), BestRoom,
	                                            *M_PalmOutward.ToCompactString(), Palms.Num()));
	// A held camera beside the drive line, looking at the trunk.
	const FVector Side(-M_PalmOutward.Y, M_PalmOutward.X, 0.f);
	const FVector Focus = M_Palm->GetActorLocation() + M_PalmOutward * 300.f + FVector(0.f, 0.f, 350.f);
	const FVector Eye = Focus + Side * PalmCameraArm + M_PalmOutward * 300.f + FVector(0.f, 0.f, PalmCameraArm * 0.45f);
	ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Focus - Eye).Rotation());
	APlayerController* Controller = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (IsValid(Camera) and IsValid(Controller))
	{
		Camera->GetCameraComponent()->SetFieldOfView(60.f);
		Controller->SetViewTarget(Camera);
		M_Camera = Camera;
	}
	SpawnPalmTank(ETankSubtype::Tank_PzII_Luchs);
	M_PalmPhase = 0;
	M_PalmPhaseStart = GetWorldSeconds();
	LogPalmState(TEXT("PalmBefore"));
}

void FSurvivalConvoyTestRunner::SpawnPalmTank(const ETankSubtype Subtype)
{
	using namespace SurvivalConvoyTestConstants;
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(GetWorld());
	if (not IsValid(Spawner) or not M_Palm.IsValid())
	{
		Finish(TEXT("no async spawner"));
		return;
	}
	FVector Start = M_Palm->GetActorLocation() + M_PalmOutward * PalmStartDistance;
	UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), Start, Start);
	TWeakPtr<FSurvivalConvoyTestRunner> WeakRunner = AsShared();
	auto OnSpawned = [WeakRunner](const FTrainingOption&, AActor* Spawned, const int32)
	{
		if (const TSharedPtr<FSurvivalConvoyTestRunner> Runner = WeakRunner.Pin())
		{
			Runner->M_PalmTank = Spawned;
		}
	};
	M_PalmClosest = TNumericLimits<float>::Max();
	Spawner->AsyncSpawnOptionAtLocation(FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(Subtype)), Start,
	                                    M_Subsystem.Get(), 0, OnSpawned, (-M_PalmOutward).Rotation());
}

float FSurvivalConvoyTestRunner::GetPalmTilt() const
{
	const AActor* Palm = M_Palm.Get();
	if (not IsValid(Palm))
	{
		return 90.f;
	}
	const USceneComponent* Tree = nullptr;
	for (const UActorComponent* Component : Palm->GetComponents())
	{
		if (Component->GetName() == TEXT("Tree"))
		{
			Tree = Cast<USceneComponent>(Component);
		}
	}
	const FVector Up = IsValid(Tree) ? Tree->GetUpVector() : Palm->GetActorUpVector();
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(Up.Z), -1.f, 1.f)));
}

void FSurvivalConvoyTestRunner::LogPalmState(const TCHAR* Label)
{
	const AActor* Palm = M_Palm.Get();
	const AActor* Tank = M_PalmTank.Get();
	LogLine(Label, FString::Printf(TEXT("palm_valid=%d tilt=%.1f health=%.2f tank=%s tank_closest_2d=%.0f"),
	                               IsValid(Palm) and not Palm->IsActorBeingDestroyed() ? 1 : 0, GetPalmTilt(),
	                               SurvivalConvoyTest_Health(Palm), IsValid(Tank) ? *Tank->GetClass()->GetName() : TEXT("none"),
	                               M_PalmClosest));
}

void FSurvivalConvoyTestRunner::TickPalmCrush()
{
	using namespace SurvivalConvoyTestConstants;
	const double Now = GetWorldSeconds();
	const double InPhase = Now - M_PalmPhaseStart;
	AActor* Tank = M_PalmTank.Get();
	if (IsValid(Tank) and M_Palm.IsValid())
	{
		M_PalmClosest = FMath::Min(M_PalmClosest, static_cast<float>(FVector::Dist2D(Tank->GetActorLocation(), M_Palm->GetActorLocation())));
	}
	// Phases: 0 light spawn wait, 1 light drive, 2 medium spawn wait, 3 medium drive.
	const bool bWaiting = M_PalmPhase == 0 or M_PalmPhase == 2;
	if (bWaiting and InPhase >= PalmSpawnWaitSeconds)
	{
		ICommands* Commands = Cast<ICommands>(Tank);
		if (Commands != nullptr and M_Palm.IsValid())
		{
			const FVector Target = M_Palm->GetActorLocation() - M_PalmOutward * PalmOvershootDistance;
			Commands->MoveToLocation(Target, true, (-M_PalmOutward).Rotation());
		}
		++M_PalmPhase;
		M_PalmPhaseStart = Now;
		M_NextPalmShot = Now;
		M_PalmShotIndex = 0;
		return;
	}
	if (bWaiting)
	{
		return;
	}
	if (Now >= M_NextPalmShot)
	{
		M_NextPalmShot = Now + PalmShotStepSeconds;
		const FString File = M_Directory / FString::Printf(TEXT("%s_%02d.png"), M_PalmPhase == 1 ? TEXT("palm_light") : TEXT("palm_medium"),
		                                                  M_PalmShotIndex++);
		FScreenshotRequest::RequestScreenshot(File, false, false);
		LogPalmState(M_PalmPhase == 1 ? TEXT("PalmLightSample") : TEXT("PalmMediumSample"));
	}
	if (InPhase < PalmDriveSeconds)
	{
		return;
	}
	const bool bToppled = GetPalmTilt() > PalmToppledTiltDegrees or not M_Palm.IsValid() or M_Palm->IsActorBeingDestroyed();
	if (M_PalmPhase == 1)
	{
		LogPalmState(bToppled ? TEXT("PalmLightResult_TOPPLED") : TEXT("PalmLightResult_STANDING"));
		if (IsValid(Tank))
		{
			Tank->Destroy();
		}
		SpawnPalmTank(ETankSubtype::Tank_PzIV_H);
		M_PalmPhase = 2;
		M_PalmPhaseStart = Now;
		return;
	}
	LogPalmState(bToppled ? TEXT("PalmMediumResult_TOPPLED") : TEXT("PalmMediumResult_STANDING"));
	Finish(bToppled ? TEXT("palm crush: medium toppled") : TEXT("palm crush: medium did not topple"));
}

// ---------------------------------------------------------------------------

void FSurvivalConvoyTest::StartIfRequested(USurvivalEventSubsystem* Subsystem)
{
	FString Mode;
	if (not IsValid(Subsystem) or not FParse::Value(FCommandLine::Get(), TEXT("ConvoyTest="), Mode))
	{
		return;
	}
	// Owned by the ticker delegate (CreateSP keeps a weak ref); a static keeps it alive for the run.
	static TSharedPtr<FSurvivalConvoyTestRunner> Runner;
	Runner = MakeShared<FSurvivalConvoyTestRunner>();
	Runner->Start(Subsystem);
}

#endif
