// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalPOIPreview.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraActor.h"
#include "RenderCore.h"
#include "RHI.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "RTS_Survival/CaptureMechanic/CaptureActor/CaptureActor.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOINav.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOIPlanner.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOISettings.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPointsOfInterestSubsystem.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartContentSettings.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace SurvivalPOIPreviewConstants
{
	constexpr float TickSeconds = 0.f;
	constexpr double SettleSeconds = 4.0;
	constexpr double DefaultCaptureAtSeconds = 90.0;
	constexpr double SampleStepSeconds = 5.0;
	constexpr int32 SampleCount = 3;
	constexpr double ShotSettleSeconds = 0.9;
	constexpr double ShotWriteTimeoutSeconds = 4.0;
	constexpr double GenerationTimeoutSeconds = 240.0;
	constexpr int32 PerfFrames = 600;
	constexpr double ExitDelaySeconds = 2.0;
	// The critic's fixed views (poi_review/critic_rubric.md).
	constexpr float TopFieldOfView = 60.f;
	constexpr double TopMargin = 3000.0;
	constexpr double RtsArm = 3000.0;
	constexpr double RtsPitch = 70.0;
	constexpr double RtsFarArm = 5200.0;
	constexpr double RtsFarPitch = 55.0;
	constexpr float RtsFieldOfView = 90.f;
	constexpr double GrazeDistance = 2500.0;
	constexpr double GrazeHeight = 250.0;
	constexpr double GrazePitch = 8.0;
	constexpr float GrazeFieldOfView = 50.f;
	constexpr double DetailDistance = 1200.0;
	constexpr double DetailHeight = 600.0;
	constexpr float DetailFieldOfView = 50.f;
	constexpr double MouthDistance = 2000.0;
	constexpr double MouthHeight = 500.0;
	constexpr float MouthFieldOfView = 60.f;
	constexpr double GateHeight = 1500.0;
	constexpr float GateFieldOfView = 50.f;
	constexpr double CloseDistance = 800.0;
	constexpr double CloseHeight = 150.0;
	constexpr double ClosePitch = 5.0;
	constexpr float CloseFieldOfView = 40.f;
	constexpr double ScreenAspect = 16.0 / 9.0;
	constexpr double MapViewMargin = 1.04;
	constexpr double FairnessReach = 30000.0;
	constexpr double ReferenceSpawnHeight = 60000.0;
	constexpr double AnimalMinStartDistance = 9000.0;
	constexpr float AnimalMaxSlope = 25.f;
	constexpr int32 PerfPairs = 5;
	constexpr int32 PerfFramesPerPhase = 300;
	constexpr double GrazeDiagonalFactor = 1.5;
	constexpr double GrazePushStep = 500.0;
	constexpr int32 GrazePushAttempts = 8;
	constexpr int32 StripFrames = 10;
	constexpr double StripStepSeconds = 1.0;
	constexpr double StripSettleSeconds = 0.15;
	constexpr double FleeStartDistance = 6000.0;
	constexpr double FleeArriveDistance = 900.0;
	constexpr double FleeTimeoutSeconds = 75.0;
	const double FleeFrameSeconds[] = {0.0, 3.0, 8.0};
	const TCHAR* const FleeTankClass =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/Ger_Vehicle_TigerH1.Ger_Vehicle_TigerH1_C");
}

namespace
{
	TArray<TSharedPtr<FJsonValue>> SurvivalPOIPreview_Vector(const FVector& Value)
	{
		return {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.X * 10.0) / 10.0),
		        MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.Y * 10.0) / 10.0),
		        MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.Z * 10.0) / 10.0)};
	}

	double SurvivalPOIPreview_Percentile(TArray<float> Values, const double Fraction)
	{
		if (Values.IsEmpty())
		{
			return 0.0;
		}
		Values.Sort();
		const int32 Index = FMath::Clamp(FMath::FloorToInt(Fraction * (Values.Num() - 1)), 0, Values.Num() - 1);
		return Values[Index];
	}

	/** @return Expected amounts per resource name of a scavengeable actor, read from its live ScavengeResources map. */
	TMap<FString, double> SurvivalPOIPreview_GetExpectedReward(const AActor& Actor)
	{
		TMap<FString, double> Result;
		const FMapProperty* MapProperty = FindFProperty<FMapProperty>(Actor.GetClass(), TEXT("ScavengeResources"));
		if (MapProperty == nullptr)
		{
			return Result;
		}
		FScriptMapHelper Helper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(&Actor));
		for (FScriptMapHelper::FIterator It(Helper); It; ++It)
		{
			FString Key;
			Helper.GetKeyProperty()->ExportTextItem_Direct(Key, Helper.GetKeyPtr(It), nullptr, nullptr, PPF_None);
			const FScavengeAmount* Amount = reinterpret_cast<const FScavengeAmount*>(Helper.GetValuePtr(It));
			const double Base = Amount->bIsSetAmount ? Amount->SetAmount : 0.5 * (Amount->Min + Amount->Max);
			Result.FindOrAdd(Key) += Base * FMath::Clamp(Amount->Chance, 0, 100) / 100.0;
		}
		return Result;
	}

	double SurvivalPOIPreview_Sum(const TMap<FString, double>& Values)
	{
		double Total = 0.0;
		for (const TPair<FString, double>& Pair : Values)
		{
			Total += Pair.Value;
		}
		return Total;
	}

	FString SurvivalPOIPreview_ToJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		FJsonSerializer::Serialize(Object, Writer);
		return Text;
	}

	double SurvivalPOIPreview_PolarAngle(const FVector& Position)
	{
		double Angle = FMath::Atan2(Position.Y, Position.X);
		return Angle < 0.0 ? Angle + UE_TWO_PI : Angle;
	}
}

/** @brief The tool's state machine; kept alive by its own core ticker until it asks the process to exit. */
class FSurvivalPOIPreviewRunner : public TSharedFromThis<FSurvivalPOIPreviewRunner>
{
public:
	explicit FSurvivalPOIPreviewRunner(USurvivalPointsOfInterestSubsystem* Subsystem)
		: M_Subsystem(Subsystem)
	{
	}

	void Start();

private:
	enum class EStep : uint8
	{
		WaitForGeneration,
		Sweep,
		WaitForCaptureTime,
		Shots,
		Perf,
		Finish,
		Finished
	};

	struct FShot
	{
		FVector Eye = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		float FieldOfView = 90.f;
		FString FileName;
		bool bShowUI = false;
		bool bPlayerView = false;
		// Seconds after generation start the shot may not be taken before (wildlife time samples).
		double NotBeforeSeconds = -1.0;
		int32 SampleIndex = INDEX_NONE;
		// Close-ups: aimed at this actor where it stands when the shot is taken.
		TWeakObjectPtr<AActor> Subject;
		// Strips reuse the pose of the frame before: a short settle is enough.
		double SettleSeconds = -1.0;
		// Runs once when the shot comes up (spawn the flee test's tank, take a sample).
		TFunction<void()> Action;
		// The shot waits until this returns true (or the flee timeout passes).
		TFunction<bool()> WaitUntil;
	};

	struct FPerfPhase
	{
		FString Label;
		FVector Eye = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		float FieldOfView = 90.f;
		bool bPOIsVisible = true;
		TArray<float> GameMs;
		TArray<float> RenderMs;
		TArray<float> GpuMs;
		TArray<float> FrameMs;
	};

	TWeakObjectPtr<USurvivalPointsOfInterestSubsystem> M_Subsystem;
	FTSTicker::FDelegateHandle M_TickHandle;
	EStep M_Step = EStep::WaitForGeneration;
	double M_StepStart = 0.0;
	FString M_Directory;
	bool bM_TakeShots = false;
	bool bM_MeasurePerf = false;
	int32 M_SweepCount = 0;
	double M_CaptureAtSeconds = SurvivalPOIPreviewConstants::DefaultCaptureAtSeconds;
	double M_WorstGenerationFrameMs = 0.0;
	double M_CensusSeconds = 0.0;

	TArray<FShot> M_Shots;
	int32 M_NextShot = 0;
	int32 M_ShotPhase = 0;
	bool bM_GameStarted = false;
	double M_ShotWaitStart = -1.0;
	// Flee test (view 11): the tank, when it arrived, and the herd's positions at each flee frame.
	TWeakObjectPtr<AActor> M_FleeTank;
	int32 M_FleeRecord = INDEX_NONE;
	double M_FleeArrivalSeconds = -1.0;
	TArray<TPair<double, TArray<FVector>>> M_FleeSamples;
	bool bM_FogHidden = false;
	double M_ShotPhaseStart = 0.0;
	TWeakObjectPtr<ACameraActor> M_Camera;
	TWeakObjectPtr<AActor> M_OriginalViewTarget;
	TArray<TSharedPtr<FJsonValue>> M_CameraLog;

	// Wildlife positions: [sample][wildlife record][actor].
	TArray<double> M_SampleSeconds;
	TArray<TArray<TArray<FVector>>> M_AnimalSamples;

	TArray<FPerfPhase> M_PerfPhases;
	int32 M_PerfIndex = 0;
	int32 M_PerfFrame = 0;
	double M_PerfPhaseStart = 0.0;
	double M_LastFrameSeconds = 0.0;
	double M_FinishRequested = -1.0;

	// Manifest order: polar angle of the centre from +X, counter-clockwise.
	TArray<int32> M_SiteOrder;
	TArray<TSharedPtr<FJsonValue>> M_PlanNotes;

	UWorld* GetWorld() const { return M_Subsystem.IsValid() ? M_Subsystem->GetWorld() : nullptr; }
	double GetSecondsSinceGeneration() const;
	void Tick();
	void AdvanceTo(EStep Step);

	void RunSweep();
	void BuildSiteOrder();
	void TakeAnimalSample(int32 SampleIndex);
	void QueueShots();
	void QueueSiteShots(const FSurvivalPOISite& Site, int32 OrderIndex);
	void QueueWildlifeShots();
	void QueueFleeTest();
	void QueueFlockStrips();
	void TakeFleeSample();
	void TryPressStart();
	void ApplySubjectPose(FShot& Shot) const;
	FVector PushOutOfActors(const FVector& Eye, const FVector& AwayFrom) const;
	void AddLookShot(const FVector& Eye, const FVector& Target, float FieldOfView, const FString& FileName, double NotBefore = -1.0,
	                 int32 SampleIndex = INDEX_NONE);
	void AddArmShot(const FVector& Target, const FVector2D& Approach, double Arm, double Pitch, float FieldOfView,
	                const FString& FileName, double NotBefore = -1.0, int32 SampleIndex = INDEX_NONE);
	bool RunShots();
	bool EnsureCamera();
	void SetHeldView(const FVector& Eye, const FRotator& Rotation, float FieldOfView);
	void RestorePlayerView();

	void QueuePerf();
	bool RunPerf();
	void SetPOIsVisible(bool bVisible);
	// Ticks that were on before the first "off" phase: off stops all of them (POIs absent), on restores exactly these.
	TArray<TWeakObjectPtr<UActorComponent>> M_PerfTickingComponents;
	TArray<TWeakObjectPtr<AActor>> M_PerfTickingActors;
	bool bM_PerfTicksRecorded = false;
	void WritePerf() const;
	FString PerfPairSummary() const;
	FString TickAudit() const;

	void WriteEverything();
	void WriteRenderSettings() const;
	void WriteCensus() const;
	TSharedRef<FJsonObject> BuildManifest() const;
	TSharedRef<FJsonObject> BuildRunObject() const;
	TSharedRef<FJsonObject> BuildSiteObject(const FSurvivalPOISite& Site, int32 OrderIndex) const;
	TSharedPtr<FJsonObject> BuildActorObject(const FSurvivalPOISpawnRecord& Record) const;
	TArray<TSharedPtr<FJsonValue>> BuildWildlifeArray() const;
	TSharedRef<FJsonObject> BuildExclusions() const;
	TSharedRef<FJsonObject> BuildFairness() const;
	double ComputeReferenceReward(TSharedRef<FJsonObject>& OutReference) const;
	void CheckExclusions(const FVector& Position, const FString& What, TArray<TSharedPtr<FJsonValue>>& OutViolations) const;

	SurvivalPOINav::FChainedPathResult FindChainedPath(const FVector& From, const FVector& To, const float AgentRadius,
	                                                   const float AgentHeight) const
	{
		return SurvivalPOINav::FindChainedPath(GetWorld(), From, To, AgentRadius, AgentHeight);
	}
	const FSurvivalPOIStart* FindStart(int32 DirectorIndex) const;
	FVector2D GetApproach(const FSurvivalPOISite& Site) const;
	const FSurvivalPOISpawnRecord* FindHero(int32 SiteIndex) const;
	FString GetSeedLabel() const { return FString::FromInt(M_Subsystem->GetMatchSeed()); }
	void Finish();
};

// ---------------------------------------------------------------------------

void FSurvivalPOIPreviewRunner::Start()
{
	FParse::Value(FCommandLine::Get(), TEXT("SurvivalPOIDir="), M_Directory);
	if (M_Directory.IsEmpty())
	{
		M_Directory = FPaths::ProjectSavedDir() / TEXT("SurvivalPOIPreview");
	}
	IFileManager::Get().MakeDirectory(*M_Directory, true);
	bM_TakeShots = FParse::Param(FCommandLine::Get(), TEXT("SurvivalPOIPreview"));
	bM_MeasurePerf = bM_TakeShots and not FParse::Param(FCommandLine::Get(), TEXT("SurvivalPOINoPerf"));
	FParse::Value(FCommandLine::Get(), TEXT("SurvivalPOISweep="), M_SweepCount);
	FParse::Value(FCommandLine::Get(), TEXT("SurvivalPOICaptureAt="), M_CaptureAtSeconds);
	RTSFunctionLibrary::PushModalDialogSuppression();
	M_LastFrameSeconds = FPlatformTime::Seconds();
	AdvanceTo(EStep::WaitForGeneration);
	// A core ticker, because the world starts paused until the player leaves the setup screens.
	TSharedRef<FSurvivalPOIPreviewRunner> SharedThis = AsShared();
	M_TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([SharedThis](const float)
	{
		SharedThis->Tick();
		return SharedThis->M_Step != EStep::Finished;
	}), SurvivalPOIPreviewConstants::TickSeconds);
	UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI_PREVIEW started (shots %d, perf %d, sweep %d, capture at %.0f s) -> %s"),
	       bM_TakeShots, bM_MeasurePerf, M_SweepCount, M_CaptureAtSeconds, *M_Directory);
}

double FSurvivalPOIPreviewRunner::GetSecondsSinceGeneration() const
{
	return M_Subsystem.IsValid() ? FPlatformTime::Seconds() - M_Subsystem->GetStats().StartSeconds : 0.0;
}

void FSurvivalPOIPreviewRunner::AdvanceTo(const EStep Step)
{
	M_Step = Step;
	M_StepStart = FPlatformTime::Seconds();
}

void FSurvivalPOIPreviewRunner::Tick()
{
	using namespace SurvivalPOIPreviewConstants;
	const double Now = FPlatformTime::Seconds();
	const double FrameMs = (Now - M_LastFrameSeconds) * 1000.0;
	M_LastFrameSeconds = Now;
	UWorld* World = GetWorld();
	if (not M_Subsystem.IsValid() or not IsValid(World))
	{
		Finish();
		return;
	}
	TryPressStart();
	if (World->IsPaused())
	{
		UGameplayStatics::SetGamePaused(World, false);
	}
	switch (M_Step)
	{
	case EStep::WaitForGeneration:
		M_WorstGenerationFrameMs = FMath::Max(M_WorstGenerationFrameMs, FrameMs);
		if (M_SweepCount > 0 and M_Subsystem->GetState() >= ESurvivalPOIGenerationState::Loading)
		{
			AdvanceTo(EStep::Sweep);
			break;
		}
		if (M_Subsystem->GetState() == ESurvivalPOIGenerationState::Failed or Now - M_StepStart > GenerationTimeoutSeconds)
		{
			UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI_PREVIEW generation did not finish (state %d)"),
			       static_cast<int32>(M_Subsystem->GetState()));
			WriteEverything();
			AdvanceTo(EStep::Finish);
			break;
		}
		if (M_Subsystem->GetIsFinished())
		{
			BuildSiteOrder();
			AdvanceTo(EStep::WaitForCaptureTime);
		}
		break;
	case EStep::Sweep:
		RunSweep();
		AdvanceTo(EStep::Finish);
		break;
	case EStep::WaitForCaptureTime:
		if (GetSecondsSinceGeneration() >= M_CaptureAtSeconds and Now - M_StepStart >= SettleSeconds)
		{
			M_CensusSeconds = GetSecondsSinceGeneration();
			TakeAnimalSample(0);
			WriteCensus();
			if (bM_TakeShots)
			{
				QueueShots();
				AdvanceTo(EStep::Shots);
				break;
			}
			// Watch mode: still sample the animals at +5 and +10 s.
			QueueWildlifeShots();
			AdvanceTo(EStep::Shots);
		}
		break;
	case EStep::Shots:
		if (RunShots())
		{
			if (bM_MeasurePerf)
			{
				QueuePerf();
				AdvanceTo(EStep::Perf);
				break;
			}
			WriteEverything();
			AdvanceTo(EStep::Finish);
		}
		break;
	case EStep::Perf:
		if (RunPerf())
		{
			WritePerf();
			WriteEverything();
			AdvanceTo(EStep::Finish);
		}
		break;
	case EStep::Finish:
		if (Now - M_StepStart >= ExitDelaySeconds)
		{
			Finish();
		}
		break;
	default:
		break;
	}
}

void FSurvivalPOIPreviewRunner::Finish()
{
	if (M_Step == EStep::Finished)
	{
		return;
	}
	M_Step = EStep::Finished;
	UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI_PREVIEW finished"));
	FPlatformMisc::RequestExit(false, TEXT("SurvivalPOIPreview"));
}

void FSurvivalPOIPreviewRunner::BuildSiteOrder()
{
	const TArray<FSurvivalPOISite>& Sites = M_Subsystem->GetSites();
	M_SiteOrder.Reset();
	for (int32 Index = 0; Index < Sites.Num(); ++Index)
	{
		M_SiteOrder.Add(Index);
	}
	M_SiteOrder.Sort([&Sites](const int32 A, const int32 B)
	{
		return SurvivalPOIPreview_PolarAngle(Sites[A].Center) < SurvivalPOIPreview_PolarAngle(Sites[B].Center);
	});
}

const FSurvivalPOIStart* FSurvivalPOIPreviewRunner::FindStart(const int32 DirectorIndex) const
{
	const FSurvivalPOIPlanner* Planner = M_Subsystem->GetPlanner();
	if (Planner == nullptr)
	{
		return nullptr;
	}
	return Planner->GetContext().Starts.FindByPredicate([DirectorIndex](const FSurvivalPOIStart& Start)
	{
		return Start.DirectorIndex == DirectorIndex;
	});
}

FVector2D FSurvivalPOIPreviewRunner::GetApproach(const FSurvivalPOISite& Site) const
{
	// From the nearest start's deploy centre toward the POI centre.
	const FSurvivalPOIPlanner* Planner = M_Subsystem->GetPlanner();
	FVector2D Best = FVector2D::ZeroVector;
	double BestDistance = TNumericLimits<double>::Max();
	for (const FSurvivalPOIStart& Start : Planner->GetContext().Starts)
	{
		const double Distance = FVector2D::Distance(Start.Position, FVector2D(Site.Center));
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Start.Position;
		}
	}
	const FVector2D Direction = (FVector2D(Site.Center) - Best).GetSafeNormal();
	return Direction.IsNearlyZero() ? FVector2D(1.0, 0.0) : Direction;
}

const FSurvivalPOISpawnRecord* FSurvivalPOIPreviewRunner::FindHero(const int32 SiteIndex) const
{
	const FSurvivalPOISpawnRecord* Best = nullptr;
	double BestArea = -1.0;
	for (const FSurvivalPOISpawnRecord& Record : M_Subsystem->GetSpawnRecords())
	{
		if (Record.Planned.SiteIndex != SiteIndex or not Record.Actor.IsValid() or not Record.Seat.bMeasured
			or Record.Planned.Role == ESurvivalPOIRole::Decal or Record.Planned.Role == ESurvivalPOIRole::Crater)
		{
			continue;
		}
		// View 6 (round 2): no capture pits and no masts as the hero; the largest vehicle, hull or building instead.
		const FSurvivalPOIAsset* HeroAsset = M_Subsystem->GetSettings()->GetAssetByKey(Record.Planned.AssetKey);
		const bool bIsPit = HeroAsset != nullptr and HeroAsset->SeatMode == ESurvivalPOISeatMode::PivotOnGround;
		if (bIsPit or Record.Actor->GetClass()->GetName().Contains(TEXT("SteelTower")))
		{
			continue;
		}
		const FVector Size = Record.Seat.LocalBoxMax - Record.Seat.LocalBoxMin;
		const double Area = Size.X * Size.Y;
		if (Area > BestArea)
		{
			BestArea = Area;
			Best = &Record;
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------
// Animals
// ---------------------------------------------------------------------------

void FSurvivalPOIPreviewRunner::TakeAnimalSample(const int32 SampleIndex)
{
	if (M_AnimalSamples.Num() <= SampleIndex)
	{
		M_AnimalSamples.SetNum(SampleIndex + 1);
		M_SampleSeconds.SetNum(SampleIndex + 1);
	}
	M_SampleSeconds[SampleIndex] = GetSecondsSinceGeneration();
	TArray<TArray<FVector>>& Sample = M_AnimalSamples[SampleIndex];
	Sample.Reset();
	for (const FSurvivalWildlifeRecord& Record : M_Subsystem->GetWildlifeRecords())
	{
		TArray<FVector>& Positions = Sample.AddDefaulted_GetRef();
		for (const TWeakObjectPtr<AActor>& Animal : Record.Actors)
		{
			Positions.Add(Animal.IsValid() ? Animal->GetActorLocation() : FVector(UE_BIG_NUMBER));
		}
	}
}

// ---------------------------------------------------------------------------
// Shots
// ---------------------------------------------------------------------------

void FSurvivalPOIPreviewRunner::AddLookShot(const FVector& Eye, const FVector& Target, const float FieldOfView,
                                            const FString& FileName, const double NotBefore, const int32 SampleIndex)
{
	FShot& Shot = M_Shots.AddDefaulted_GetRef();
	Shot.Eye = Eye;
	Shot.Rotation = (Target - Eye).Rotation();
	Shot.FieldOfView = FieldOfView;
	Shot.FileName = FileName;
	Shot.NotBeforeSeconds = NotBefore;
	Shot.SampleIndex = SampleIndex;
}

void FSurvivalPOIPreviewRunner::AddArmShot(const FVector& Target, const FVector2D& Approach, const double Arm, const double Pitch,
                                           const float FieldOfView, const FString& FileName, const double NotBefore,
                                           const int32 SampleIndex)
{
	const double PitchRadians = FMath::DegreesToRadians(Pitch);
	const FVector Eye = Target - FVector(Approach * Arm * FMath::Cos(PitchRadians), 0.0)
		+ FVector(0.0, 0.0, Arm * FMath::Sin(PitchRadians));
	FShot& Shot = M_Shots.AddDefaulted_GetRef();
	Shot.Eye = Eye;
	Shot.Rotation = FRotator(-Pitch, FMath::RadiansToDegrees(FMath::Atan2(Approach.Y, Approach.X)), 0.0);
	Shot.FieldOfView = FieldOfView;
	Shot.FileName = FileName;
	Shot.NotBeforeSeconds = NotBefore;
	Shot.SampleIndex = SampleIndex;
}

void FSurvivalPOIPreviewRunner::QueueShots()
{
	using namespace SurvivalPOIPreviewConstants;
	const FString Seed = GetSeedLabel();
	// The real minimap with the UI, from the player's own camera.
	FShot& Minimap = M_Shots.AddDefaulted_GetRef();
	Minimap.FileName = FString::Printf(TEXT("minimap_%s.png"), *Seed);
	Minimap.bShowUI = true;
	Minimap.bPlayerView = true;
	QueueWildlifeShots();
	QueueFleeTest();
	QueueFlockStrips();
	const FBox2D Bounds = M_Subsystem->GetPlanner()->GetContext().LandscapeBounds;
	const double HalfHeightSpan = Bounds.GetExtent().GetMax() * MapViewMargin;
	const double VerticalTan = FMath::Tan(FMath::DegreesToRadians(TopFieldOfView * 0.5)) / ScreenAspect;
	const FVector MapEye(Bounds.GetCenter(), HalfHeightSpan / VerticalTan);
	FShot& Map = M_Shots.AddDefaulted_GetRef();
	Map.Eye = MapEye;
	Map.Rotation = FRotator(-90.f, 0.f, 0.f);
	Map.FieldOfView = TopFieldOfView;
	Map.FileName = FString::Printf(TEXT("map_pois_%s_clean.png"), *Seed);
	// The first site of each type in manifest order.
	TSet<ESurvivalPOIType> Done;
	const TArray<FSurvivalPOISite>& Sites = M_Subsystem->GetSites();
	for (int32 OrderIndex = 0; OrderIndex < M_SiteOrder.Num(); ++OrderIndex)
	{
		const FSurvivalPOISite& Site = Sites[M_SiteOrder[OrderIndex]];
		if (Done.Contains(Site.Type))
		{
			continue;
		}
		Done.Add(Site.Type);
		QueueSiteShots(Site, OrderIndex);
	}
}

void FSurvivalPOIPreviewRunner::QueueSiteShots(const FSurvivalPOISite& Site, const int32 OrderIndex)
{
	using namespace SurvivalPOIPreviewConstants;
	const FString Prefix = FString::Printf(TEXT("%s_%s"), *SurvivalPOINames::GetTypeName(Site.Type), *GetSeedLabel());
	const FVector2D Approach = GetApproach(Site);
	const double VerticalTan = FMath::Tan(FMath::DegreesToRadians(TopFieldOfView * 0.5)) / ScreenAspect;
	FShot& Top = M_Shots.AddDefaulted_GetRef();
	Top.Eye = Site.Center + FVector(0.0, 0.0, (Site.Radius + TopMargin) / VerticalTan);
	Top.Rotation = FRotator(-90.f, 0.f, 0.f);
	Top.FieldOfView = TopFieldOfView;
	Top.FileName = Prefix + TEXT("_top.png");
	AddArmShot(Site.Center, Approach, RtsArm, RtsPitch, RtsFieldOfView, Prefix + TEXT("_rts.png"));
	AddArmShot(Site.Center, Approach, RtsFarArm, RtsFarPitch, RtsFieldOfView, Prefix + TEXT("_rts5200.png"));
	if (const FSurvivalPOISpawnRecord* Hero = FindHero(Site.SiteIndex))
	{
		const FVector HeroLocation = Hero->Actor->GetActorLocation();
		const FVector2D Left(-Approach.Y, Approach.X);
		const FVector BoxSize = Hero->Seat.LocalBoxMax - Hero->Seat.LocalBoxMin;
		const double HalfDiagonal = 0.5 * FVector2D(BoxSize.X, BoxSize.Y).Size();
		const double Distance = HalfDiagonal + FMath::Max(GrazeDistance, GrazeDiagonalFactor * 2.0 * HalfDiagonal);
		const double TanPitch = FMath::Tan(FMath::DegreesToRadians(GrazePitch));
		const FVector LookA = HeroLocation + FVector(0.0, 0.0, GrazeHeight - Distance * TanPitch);
		const FVector EyeA = PushOutOfActors(HeroLocation + FVector(Left * Distance, GrazeHeight), HeroLocation);
		const FVector EyeB = PushOutOfActors(HeroLocation + FVector(-Left * Distance, GrazeHeight), HeroLocation);
		AddLookShot(EyeA, LookA, GrazeFieldOfView, Prefix + TEXT("_graze_a.png"));
		AddLookShot(EyeB, LookA, GrazeFieldOfView, Prefix + TEXT("_graze_b.png"));
		AddLookShot(HeroLocation + FVector(-Approach * DetailDistance, DetailHeight), HeroLocation, DetailFieldOfView,
		            Prefix + TEXT("_detail.png"));
		if (Site.Type == ESurvivalPOIType::RuinedHangar)
		{
			// The composition's +X is the door side.
			const FVector Outward = FRotator(0.f, Site.YawDegrees, 0.f).Vector();
			AddLookShot(HeroLocation + Outward * MouthDistance + FVector(0.0, 0.0, MouthHeight), HeroLocation, MouthFieldOfView,
			            FString::Printf(TEXT("hangar_%s_mouth.png"), *GetSeedLabel()));
		}
	}
	// From the owner start's first defense point (its main gate guard), as a landmark seen from home.
	const TArray<TWeakObjectPtr<ASurvivalStartLocation>>& Starts = M_Subsystem->GetDirector()->GetStartLocations();
	if (Starts.IsValidIndex(Site.OwnerStart) and Starts[Site.OwnerStart].IsValid())
	{
		const ASurvivalStartLocation* Start = Starts[Site.OwnerStart].Get();
		const FVector Gate = Start->GetDefensePoints().IsEmpty() ? Start->GetDeployCenter() : Start->GetDefensePoints()[0];
		AddLookShot(Gate + FVector(0.0, 0.0, GateHeight), Site.Center, GateFieldOfView, Prefix + TEXT("_gate.png"));
	}
}

void FSurvivalPOIPreviewRunner::QueueWildlifeShots()
{
	using namespace SurvivalPOIPreviewConstants;
	const USurvivalPointsOfInterestSettings* Settings = M_Subsystem->GetSettings();
	const TArray<FSurvivalWildlifeRecord>& Records = M_Subsystem->GetWildlifeRecords();
	for (int32 Sample = 0; Sample < SampleCount; ++Sample)
	{
		const double NotBefore = M_CensusSeconds + Sample * SampleStepSeconds;
		bool bFirst = true;
		for (const FSurvivalWildlifeRecord& Record : Records)
		{
			const FSurvivalWildlifeEntry& Entry = Settings->Wildlife[Record.Group.EntryIndex];
			if (Entry.Kind != ESurvivalWildlifeKind::GroundHerd)
			{
				continue;
			}
			const FString Name = FString::Printf(TEXT("wild_%s_%s_g%d_t%d.png"), *Entry.Name.ToString().ToLower(), *GetSeedLabel(),
			                                     static_cast<int32>(&Record - Records.GetData()), Sample * 5);
			if (bM_TakeShots)
			{
				AddArmShot(Record.Group.Center, FVector2D(1.0, 0.0), RtsArm, RtsPitch, RtsFieldOfView, Name, NotBefore,
				           bFirst and Sample > 0 ? Sample : INDEX_NONE);
			}
			else if (bFirst and Sample > 0)
			{
				// No pictures: a placeholder shot only waits and samples.
				FShot& Wait = M_Shots.AddDefaulted_GetRef();
				Wait.NotBeforeSeconds = NotBefore;
				Wait.SampleIndex = Sample;
			}
			bFirst = false;
		}
	}
	if (not bM_TakeShots)
	{
		return;
	}
	for (const FSurvivalWildlifeRecord& Record : Records)
	{
		const FSurvivalWildlifeEntry& Entry = Settings->Wildlife[Record.Group.EntryIndex];
		const int32 Index = static_cast<int32>(&Record - Records.GetData());
		if (Entry.Kind == ESurvivalWildlifeKind::GroundHerd)
		{
			if (Record.Actors.IsEmpty() or not Record.Actors[0].IsValid())
			{
				continue;
			}
			FShot& Close = M_Shots.AddDefaulted_GetRef();
			Close.Subject = Record.Actors[0];
			Close.FieldOfView = CloseFieldOfView;
			Close.FileName = FString::Printf(TEXT("wild_%s_%s_g%d_close.png"), *Entry.Name.ToString().ToLower(), *GetSeedLabel(), Index);
			continue;
		}
		AddArmShot(Record.Group.Center, FVector2D(1.0, 0.0), RtsFarArm, RtsFarPitch, RtsFieldOfView,
		           FString::Printf(TEXT("flock_%s_%s_g%d.png"), *Entry.Name.ToString().ToLower(), *GetSeedLabel(), Index));
	}
}

bool FSurvivalPOIPreviewRunner::EnsureCamera()
{
	if (M_Camera.IsValid())
	{
		return true;
	}
	UWorld* World = GetWorld();
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	if (not IsValid(PlayerController))
	{
		return false;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Parameters);
	if (not IsValid(Camera))
	{
		return false;
	}
	Camera->GetCameraComponent()->bConstrainAspectRatio = false;
	M_OriginalViewTarget = PlayerController->GetViewTarget();
	M_Camera = Camera;
	return true;
}

void FSurvivalPOIPreviewRunner::SetHeldView(const FVector& Eye, const FRotator& Rotation, const float FieldOfView)
{
	if (not EnsureCamera())
	{
		return;
	}
	M_Camera->SetActorLocationAndRotation(Eye, Rotation);
	M_Camera->GetCameraComponent()->SetFieldOfView(FieldOfView);
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (IsValid(PlayerController) and PlayerController->GetViewTarget() != M_Camera.Get())
	{
		PlayerController->SetViewTarget(M_Camera.Get());
	}
}

void FSurvivalPOIPreviewRunner::RestorePlayerView()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (IsValid(PlayerController) and M_OriginalViewTarget.IsValid())
	{
		PlayerController->SetViewTarget(M_OriginalViewTarget.Get());
	}
}

bool FSurvivalPOIPreviewRunner::RunShots()
{
	using namespace SurvivalPOIPreviewConstants;
	const double Now = FPlatformTime::Seconds();
	if (not M_Shots.IsValidIndex(M_NextShot))
	{
		return true;
	}
	const FShot& Shot = M_Shots[M_NextShot];
	switch (M_ShotPhase)
	{
	case 0:
		if (Shot.NotBeforeSeconds >= 0.0 and GetSecondsSinceGeneration() < Shot.NotBeforeSeconds)
		{
			return false;
		}
		if (Shot.WaitUntil)
		{
			if (M_ShotWaitStart < 0.0)
			{
				M_ShotWaitStart = Now;
			}
			if (not Shot.WaitUntil() and Now - M_ShotWaitStart < FleeTimeoutSeconds)
			{
				return false;
			}
			M_ShotWaitStart = -1.0;
		}
		if (Shot.Action)
		{
			Shot.Action();
		}
		if (Shot.SampleIndex != INDEX_NONE)
		{
			TakeAnimalSample(Shot.SampleIndex);
		}
		if (Shot.Subject.IsValid())
		{
			ApplySubjectPose(M_Shots[M_NextShot]);
		}
		if (Shot.FileName.IsEmpty())
		{
			++M_NextShot;
			return false;
		}
		if (Shot.bPlayerView)
		{
			RestorePlayerView();
		}
		else
		{
			if (not bM_FogHidden and GEngine != nullptr)
			{
				// The fog of war is a post-process material: the held views show the ground, not what the player has scouted.
				GEngine->Exec(GetWorld(), TEXT("ShowFlag.PostProcessMaterial 0"));
				bM_FogHidden = true;
			}
			SetHeldView(Shot.Eye, Shot.Rotation, Shot.FieldOfView);
		}
		M_ShotPhase = 1;
		M_ShotPhaseStart = Now;
		return false;
	case 1:
		if (Now - M_ShotPhaseStart < (Shot.SettleSeconds >= 0.0 ? Shot.SettleSeconds : ShotSettleSeconds))
		{
			return false;
		}
		FScreenshotRequest::RequestScreenshot(M_Directory / Shot.FileName, Shot.bShowUI, false);
		M_ShotPhase = 2;
		M_ShotPhaseStart = Now;
		return false;
	default:
		if (not IFileManager::Get().FileExists(*(M_Directory / Shot.FileName)) and Now - M_ShotPhaseStart < ShotWriteTimeoutSeconds)
		{
			return false;
		}
		{
			const TSharedRef<FJsonObject> Camera = MakeShared<FJsonObject>();
			Camera->SetStringField(TEXT("file"), Shot.FileName);
			Camera->SetArrayField(TEXT("eye"), SurvivalPOIPreview_Vector(Shot.Eye));
			Camera->SetArrayField(TEXT("rotation_pyr"), SurvivalPOIPreview_Vector(FVector(Shot.Rotation.Pitch, Shot.Rotation.Yaw, Shot.Rotation.Roll)));
			Camera->SetNumberField(TEXT("fov_horizontal"), Shot.FieldOfView);
			Camera->SetBoolField(TEXT("player_view"), Shot.bPlayerView);
			Camera->SetBoolField(TEXT("written"), IFileManager::Get().FileExists(*(M_Directory / Shot.FileName)));
			Camera->SetNumberField(TEXT("t_seconds"), GetSecondsSinceGeneration());
			M_CameraLog.Add(MakeShared<FJsonValueObject>(Camera));
		}
		M_ShotPhase = 0;
		++M_NextShot;
		return not M_Shots.IsValidIndex(M_NextShot);
	}
}

// ---------------------------------------------------------------------------
// Round 2: start button, live close-ups, flee test, flock strips
// ---------------------------------------------------------------------------

void FSurvivalPOIPreviewRunner::TryPressStart()
{
	if (bM_GameStarted)
	{
		return;
	}
	// Press "Press To Start" as the player does (MapSmokeTestSubsystem::TryStartGame), so the real HUD and minimap show.
	ACPPController* Controller = Cast<ACPPController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	UPlayerStartGameControl* StartControl = IsValid(Controller) ? Controller->FindComponentByClass<UPlayerStartGameControl>() : nullptr;
	if (not IsValid(StartControl))
	{
		return;
	}
	if (StartControl->GetIsWaitingForPlayerToStart())
	{
		StartControl->PlayerStartedGame();
		UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI_PREVIEW pressed start"));
	}
	bM_GameStarted = true;
}

void FSurvivalPOIPreviewRunner::ApplySubjectPose(FShot& Shot) const
{
	using namespace SurvivalPOIPreviewConstants;
	const FVector Animal = Shot.Subject->GetActorLocation();
	const FVector Eye = PushOutOfActors(Animal + FVector(-CloseDistance, 0.0, CloseHeight), Animal);
	const FVector Look = Animal + FVector(0.0, 0.0, CloseHeight - FVector::Dist2D(Eye, Animal) * FMath::Tan(FMath::DegreesToRadians(ClosePitch)));
	Shot.Eye = Eye;
	Shot.Rotation = (Look - Eye).Rotation();
}

FVector FSurvivalPOIPreviewRunner::PushOutOfActors(const FVector& Eye, const FVector& AwayFrom) const
{
	using namespace SurvivalPOIPreviewConstants;
	FVector Result = Eye;
	const FVector2D Away = (FVector2D(Eye) - FVector2D(AwayFrom)).GetSafeNormal();
	for (int32 Attempt = 0; Attempt < GrazePushAttempts; ++Attempt)
	{
		bool bInside = false;
		for (const FSurvivalPOISpawnRecord& Record : M_Subsystem->GetSpawnRecords())
		{
			if (not Record.Actor.IsValid() or not Record.Seat.bMeasured)
			{
				continue;
			}
			const FVector Local = Record.Actor->GetActorTransform().InverseTransformPosition(Result);
			const FBox Box(Record.Seat.LocalBoxMin, Record.Seat.LocalBoxMax);
			if (Box.ExpandBy(GrazePushStep * 0.2).IsInside(Local))
			{
				bInside = true;
				break;
			}
		}
		if (not bInside)
		{
			break;
		}
		Result += FVector(Away * GrazePushStep, 0.0);
	}
	// Never under the sand either.
	float Ground = 0.f;
	if (M_Subsystem->GetProjector() != nullptr and M_Subsystem->GetProjector()->TryGetHeight(FVector2D(Result), Ground))
	{
		Result.Z = FMath::Max(Result.Z, static_cast<double>(Ground) + CloseHeight);
	}
	return Result;
}

void FSurvivalPOIPreviewRunner::TakeFleeSample()
{
	const TArray<FSurvivalWildlifeRecord>& Records = M_Subsystem->GetWildlifeRecords();
	if (not Records.IsValidIndex(M_FleeRecord))
	{
		return;
	}
	TPair<double, TArray<FVector>>& Sample = M_FleeSamples.AddDefaulted_GetRef();
	Sample.Key = M_FleeArrivalSeconds >= 0.0 ? FPlatformTime::Seconds() - M_FleeArrivalSeconds : -1.0;
	for (const TWeakObjectPtr<AActor>& Animal : Records[M_FleeRecord].Actors)
	{
		Sample.Value.Add(Animal.IsValid() ? Animal->GetActorLocation() : FVector(UE_BIG_NUMBER));
	}
}

void FSurvivalPOIPreviewRunner::QueueFleeTest()
{
	using namespace SurvivalPOIPreviewConstants;
	const USurvivalPointsOfInterestSettings* Settings = M_Subsystem->GetSettings();
	const TArray<FSurvivalWildlifeRecord>& Records = M_Subsystem->GetWildlifeRecords();
	M_FleeRecord = Records.IndexOfByPredicate([Settings](const FSurvivalWildlifeRecord& Record)
	{
		return Settings->Wildlife[Record.Group.EntryIndex].Kind == ESurvivalWildlifeKind::GroundHerd and not Record.Actors.IsEmpty();
	});
	if (M_FleeRecord == INDEX_NONE)
	{
		return;
	}
	const FSurvivalWildlifeRecord& Herd = Records[M_FleeRecord];
	const FVector Centre = Herd.Group.Center;
	const FString Name = Settings->Wildlife[Herd.Group.EntryIndex].Name.ToString().ToLower();
	// The tank comes over land: from the side away from the water the herd grazes beside.
	FVector2D Away(1.0, 0.0);
	for (const FSurvivalPOIExclusionZone& Zone : Settings->Placement.ExclusionZones)
	{
		if (Zone.bIsWater)
		{
			Away = (FVector2D(Centre) - Zone.Center).GetSafeNormal();
		}
	}
	FShot& Spawn = M_Shots.AddDefaulted_GetRef();
	Spawn.Action = [this, Centre, Away]()
	{
		const FVector Start = Centre + FVector(Away * FleeStartDistance, 0.0);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Away.Y, -Away.X));
		AActor* Tank = FGrenadeTestHelpers::SpawnUnitForPlayer(*GetWorld(), FleeTankClass, FTransform(FRotator(0.f, Yaw, 0.f), Start), 1);
		M_FleeTank = Tank;
		if (ICommands* Commands = Cast<ICommands>(Tank))
		{
			Commands->MoveToLocation(Centre, true, FRotator(0.f, Yaw, 0.f));
		}
		UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI_PREVIEW flee test: tank %s sent from (%.0f, %.0f) to the herd at (%.0f, %.0f)"),
		       IsValid(Tank) ? *Tank->GetName() : TEXT("none"), Start.X, Start.Y, Centre.X, Centre.Y);
	};
	for (int32 Frame = 0; Frame < UE_ARRAY_COUNT(FleeFrameSeconds); ++Frame)
	{
		AddArmShot(Centre, Away * -1.0, RtsArm, RtsPitch, RtsFieldOfView,
		           FString::Printf(TEXT("wild_%s_%s_flee_t%d.png"), *Name, *GetSeedLabel(), FMath::RoundToInt(FleeFrameSeconds[Frame])));
		FShot& Shot = M_Shots.Last();
		const double Delay = FleeFrameSeconds[Frame];
		if (Frame == 0)
		{
			Shot.WaitUntil = [this, Centre]()
			{
				return M_FleeTank.IsValid()
					and FVector::Dist2D(M_FleeTank->GetActorLocation(), Centre) <= SurvivalPOIPreviewConstants::FleeArriveDistance;
			};
			Shot.Action = [this]()
			{
				M_FleeArrivalSeconds = FPlatformTime::Seconds();
				TakeFleeSample();
			};
			continue;
		}
		Shot.WaitUntil = [this, Delay]()
		{
			return M_FleeArrivalSeconds >= 0.0 and FPlatformTime::Seconds() >= M_FleeArrivalSeconds + Delay;
		};
		Shot.Action = [this]()
		{
			TakeFleeSample();
		};
	}
	FShot& Cleanup = M_Shots.AddDefaulted_GetRef();
	Cleanup.Action = [this]()
	{
		if (M_FleeTank.IsValid())
		{
			M_FleeTank->Destroy();
		}
	};
}

void FSurvivalPOIPreviewRunner::QueueFlockStrips()
{
	using namespace SurvivalPOIPreviewConstants;
	const USurvivalPointsOfInterestSettings* Settings = M_Subsystem->GetSettings();
	const TArray<FSurvivalWildlifeRecord>& Records = M_Subsystem->GetWildlifeRecords();
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const FSurvivalWildlifeEntry& Entry = Settings->Wildlife[Records[Index].Group.EntryIndex];
		if (Entry.Kind == ESurvivalWildlifeKind::GroundHerd)
		{
			continue;
		}
		const TSharedRef<double> StripStart = MakeShared<double>(-1.0);
		for (int32 Frame = 0; Frame < StripFrames; ++Frame)
		{
			AddArmShot(Records[Index].Group.Center, FVector2D(1.0, 0.0), RtsFarArm, RtsFarPitch, RtsFieldOfView,
			           FString::Printf(TEXT("strip_%s_%s_g%d_f%02d.png"), *Entry.Name.ToString().ToLower(), *GetSeedLabel(), Index, Frame));
			FShot& Shot = M_Shots.Last();
			if (Frame == 0)
			{
				Shot.Action = [StripStart]()
				{
					*StripStart = FPlatformTime::Seconds();
				};
				continue;
			}
			Shot.SettleSeconds = StripSettleSeconds;
			Shot.WaitUntil = [StripStart, Frame]()
			{
				return *StripStart >= 0.0 and FPlatformTime::Seconds() >= *StripStart + Frame * SurvivalPOIPreviewConstants::StripStepSeconds;
			};
		}
	}
}

FString FSurvivalPOIPreviewRunner::PerfPairSummary() const
{
	FString Text = TEXT("\npose\tpairs\tgame_delta_mean\tgame_delta_sd\tgpu_delta_mean\tgpu_delta_sd\t(delta = median on - median off per pair, ms)\n");
	TArray<FString> Labels;
	for (const FPerfPhase& Phase : M_PerfPhases)
	{
		Labels.AddUnique(Phase.Label);
	}
	for (const FString& Label : Labels)
	{
		TArray<double> GameDeltas;
		TArray<double> GpuDeltas;
		for (int32 Index = 0; Index + 1 < M_PerfPhases.Num(); ++Index)
		{
			const FPerfPhase& On = M_PerfPhases[Index];
			const FPerfPhase& Off = M_PerfPhases[Index + 1];
			if (On.Label != Label or Off.Label != Label or not On.bPOIsVisible or Off.bPOIsVisible)
			{
				continue;
			}
			GameDeltas.Add(SurvivalPOIPreview_Percentile(On.GameMs, 0.5) - SurvivalPOIPreview_Percentile(Off.GameMs, 0.5));
			GpuDeltas.Add(SurvivalPOIPreview_Percentile(On.GpuMs, 0.5) - SurvivalPOIPreview_Percentile(Off.GpuMs, 0.5));
		}
		auto MeanAndSd = [](const TArray<double>& Values, double& OutMean, double& OutSd)
		{
			OutMean = 0.0;
			OutSd = 0.0;
			for (const double Value : Values)
			{
				OutMean += Value / FMath::Max(1, Values.Num());
			}
			for (const double Value : Values)
			{
				OutSd += FMath::Square(Value - OutMean) / FMath::Max(1, Values.Num() - 1);
			}
			OutSd = FMath::Sqrt(OutSd);
		};
		double GameMean = 0.0;
		double GameSd = 0.0;
		double GpuMean = 0.0;
		double GpuSd = 0.0;
		MeanAndSd(GameDeltas, GameMean, GameSd);
		MeanAndSd(GpuDeltas, GpuMean, GpuSd);
		Text += FString::Printf(TEXT("%s\t%d\t%.3f\t%.3f\t%.3f\t%.3f\n"), *Label, GameDeltas.Num(), GameMean, GameSd, GpuMean, GpuSd);
	}
	return Text;
}

FString FSurvivalPOIPreviewRunner::TickAudit() const
{
	// What of the POI system still ticks, per class: actors with an enabled tick and their interval, ticking components.
	TMap<FString, FIntVector> PerClass;
	TMap<FString, float> Interval;
	TMap<FString, TSet<FString>> ComponentClasses;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (not It->Tags.Contains(USurvivalPointsOfInterestSubsystem::SpawnedTag))
		{
			continue;
		}
		FIntVector& Counts = PerClass.FindOrAdd(It->GetClass()->GetName());
		++Counts.X;
		if (It->PrimaryActorTick.IsTickFunctionEnabled())
		{
			++Counts.Y;
			Interval.Add(It->GetClass()->GetName(), It->PrimaryActorTick.TickInterval);
		}
		TInlineComponentArray<UActorComponent*> Components(*It);
		for (const UActorComponent* Component : Components)
		{
			const bool bTicking = IsValid(Component) and Component->IsComponentTickEnabled();
			Counts.Z += bTicking ? 1 : 0;
			if (bTicking)
			{
				ComponentClasses.FindOrAdd(It->GetClass()->GetName()).Add(Component->GetClass()->GetName());
			}
		}
	}
	FString Text = TEXT("\ntick audit (POI-tagged actors): class\tactors\tticking actors\tactor tick interval s\tticking components\tticking component classes\n");
	for (const TPair<FString, FIntVector>& Pair : PerClass)
	{
		Text += FString::Printf(TEXT("%s\t%d\t%d\t%.2f\t%d\t%s\n"), *Pair.Key, Pair.Value.X, Pair.Value.Y, Interval.FindRef(Pair.Key),
		                        Pair.Value.Z, *FString::Join(ComponentClasses.FindRef(Pair.Key).Array(), TEXT(",")));
	}
	const FIntPoint Sleepers = M_Subsystem->GetOffscreenSleeperCounts();
	Text += FString::Printf(TEXT("off-screen sleepers awake at the audit: %d of %d\n"), Sleepers.X, Sleepers.Y);
	return Text;
}


// ---------------------------------------------------------------------------
// Perf
// ---------------------------------------------------------------------------

void FSurvivalPOIPreviewRunner::SetPOIsVisible(const bool bVisible)
{
	if (not bM_PerfTicksRecorded)
	{
		bM_PerfTicksRecorded = true;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (not It->Tags.Contains(USurvivalPointsOfInterestSubsystem::SpawnedTag))
			{
				continue;
			}
			if (It->PrimaryActorTick.IsTickFunctionEnabled())
			{
				M_PerfTickingActors.Add(*It);
			}
			TInlineComponentArray<UActorComponent*> Components(*It);
			for (UActorComponent* Component : Components)
			{
				if (IsValid(Component) and Component->IsComponentTickEnabled())
				{
					M_PerfTickingComponents.Add(Component);
				}
			}
		}
	}
	// "Off" means the POIs are not there at all: hidden and nothing of them ticks. The off-screen sleep may still
	// toggle animal ticks and effects in the "on" phases; that is the cost being measured.
	for (const TWeakObjectPtr<AActor>& Actor : M_PerfTickingActors)
	{
		if (Actor.IsValid())
		{
			Actor->SetActorTickEnabled(bVisible);
		}
	}
	for (const TWeakObjectPtr<UActorComponent>& Component : M_PerfTickingComponents)
	{
		if (Component.IsValid())
		{
			Component->SetComponentTickEnabled(bVisible);
		}
	}
	if (bVisible and M_Subsystem.IsValid())
	{
		M_Subsystem->MarkOffscreenSleepersAwake();
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (not It->Tags.Contains(USurvivalPointsOfInterestSubsystem::SpawnedTag))
		{
			continue;
		}
		It->SetActorHiddenInGame(not bVisible);
		if (const ANiagaraActor* Niagara = Cast<ANiagaraActor>(*It))
		{
			if (UNiagaraComponent* Component = Niagara->GetNiagaraComponent())
			{
				bVisible ? Component->Activate(false) : Component->Deactivate();
			}
		}
	}
}

void FSurvivalPOIPreviewRunner::QueuePerf()
{
	using namespace SurvivalPOIPreviewConstants;
	// The densest POI: most spawned objects.
	TMap<int32, int32> CountPerSite;
	for (const FSurvivalPOISpawnRecord& Record : M_Subsystem->GetSpawnRecords())
	{
		CountPerSite.FindOrAdd(Record.Planned.SiteIndex)++;
	}
	int32 Densest = INDEX_NONE;
	int32 MostObjects = -1;
	for (const TPair<int32, int32>& Pair : CountPerSite)
	{
		if (Pair.Value > MostObjects)
		{
			MostObjects = Pair.Value;
			Densest = Pair.Key;
		}
	}
	const TArray<FSurvivalPOISite>& Sites = M_Subsystem->GetSites();
	TArray<FPerfPhase> Poses;
	if (Sites.IsValidIndex(Densest))
	{
		const FSurvivalPOISite& Site = Sites[Densest];
		const FVector2D Approach = GetApproach(Site);
		const double PitchRadians = FMath::DegreesToRadians(RtsFarPitch);
		FPerfPhase Pose;
		Pose.Label = FString::Printf(TEXT("rts5200_densest_%s_site%d"), *SurvivalPOINames::GetTypeName(Site.Type), Site.SiteIndex);
		Pose.Eye = Site.Center - FVector(Approach * RtsFarArm * FMath::Cos(PitchRadians), 0.0)
			+ FVector(0.0, 0.0, RtsFarArm * FMath::Sin(PitchRadians));
		Pose.Rotation = FRotator(-RtsFarPitch, FMath::RadiansToDegrees(FMath::Atan2(Approach.Y, Approach.X)), 0.0);
		Pose.FieldOfView = RtsFieldOfView;
		Poses.Add(Pose);
	}
	const FBox2D Bounds = M_Subsystem->GetPlanner()->GetContext().LandscapeBounds;
	const double VerticalTan = FMath::Tan(FMath::DegreesToRadians(TopFieldOfView * 0.5)) / ScreenAspect;
	FPerfPhase Map;
	Map.Label = TEXT("whole_map");
	Map.Eye = FVector(Bounds.GetCenter(), Bounds.GetExtent().GetMax() * MapViewMargin / VerticalTan);
	Map.Rotation = FRotator(-90.f, 0.f, 0.f);
	Map.FieldOfView = TopFieldOfView;
	Poses.Add(Map);
	for (const FPerfPhase& Pose : Poses)
	{
		// Alternating on/off pairs, so slow drift of the whole game cancels out of the difference.
		for (int32 Pair = 0; Pair < PerfPairs; ++Pair)
		{
			FPerfPhase On = Pose;
			On.bPOIsVisible = true;
			FPerfPhase Off = Pose;
			Off.bPOIsVisible = false;
			M_PerfPhases.Add(On);
			M_PerfPhases.Add(Off);
		}
	}
	M_PerfIndex = 0;
	M_PerfFrame = -1;
}

bool FSurvivalPOIPreviewRunner::RunPerf()
{
	using namespace SurvivalPOIPreviewConstants;
	if (not M_PerfPhases.IsValidIndex(M_PerfIndex))
	{
		SetPOIsVisible(true);
		return true;
	}
	FPerfPhase& Phase = M_PerfPhases[M_PerfIndex];
	const double Now = FPlatformTime::Seconds();
	if (M_PerfFrame < 0)
	{
		SetPOIsVisible(Phase.bPOIsVisible);
		SetHeldView(Phase.Eye, Phase.Rotation, Phase.FieldOfView);
		M_PerfPhaseStart = Now;
		M_PerfFrame = 0;
		return false;
	}
	if (Now - M_PerfPhaseStart < ShotSettleSeconds)
	{
		return false;
	}
	Phase.GameMs.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
	Phase.RenderMs.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
	Phase.GpuMs.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
	Phase.FrameMs.Add(FApp::GetDeltaTime() * 1000.0);
	if (++M_PerfFrame >= PerfFramesPerPhase)
	{
		++M_PerfIndex;
		M_PerfFrame = -1;
	}
	return false;
}

void FSurvivalPOIPreviewRunner::WritePerf() const
{
	FString Text = FString::Printf(TEXT("POI and wildlife perf, seed %s, %d frames per phase, %d alternating on/off pairs per pose.\n"),
	                               *GetSeedLabel(), SurvivalPOIPreviewConstants::PerfFramesPerPhase, SurvivalPOIPreviewConstants::PerfPairs);
	Text += TEXT("phase\tpois\tgame_med\tgame_p95\trender_med\tgpu_med\tgpu_p95\tframe_med\tframe_p95\n");
	for (const FPerfPhase& Phase : M_PerfPhases)
	{
		Text += FString::Printf(TEXT("%s\t%s\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\n"), *Phase.Label,
		                        Phase.bPOIsVisible ? TEXT("on") : TEXT("off"), SurvivalPOIPreview_Percentile(Phase.GameMs, 0.5),
		                        SurvivalPOIPreview_Percentile(Phase.GameMs, 0.95), SurvivalPOIPreview_Percentile(Phase.RenderMs, 0.5),
		                        SurvivalPOIPreview_Percentile(Phase.GpuMs, 0.5), SurvivalPOIPreview_Percentile(Phase.GpuMs, 0.95),
		                        SurvivalPOIPreview_Percentile(Phase.FrameMs, 0.5), SurvivalPOIPreview_Percentile(Phase.FrameMs, 0.95));
	}
	Text += PerfPairSummary();
	Text += TickAudit();
	const FSurvivalPOIGenerationStats& Stats = M_Subsystem->GetStats();
	Text += FString::Printf(
		TEXT("generation: %.2f s total, sites chosen after %.2f s in %d steps (worst step %.1f ms), compose %.2f ms, wildlife plan %.2f ms, %d spawn steps (worst %.1f ms), worst frame while generating %.1f ms\n"),
		Stats.FinishedSeconds - Stats.StartSeconds, Stats.SitesChosenSeconds - Stats.StartSeconds, Stats.SiteSteps,
		Stats.WorstSiteStepMs, Stats.ComposeMs, Stats.WildlifePlanMs, Stats.SpawnSteps, Stats.WorstSpawnStepMs, M_WorstGenerationFrameMs);
	FFileHelper::SaveStringToFile(Text, *(M_Directory / FString::Printf(TEXT("perf_%s.txt"), *GetSeedLabel())));
}

// ---------------------------------------------------------------------------
// Nav
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Manifest
// ---------------------------------------------------------------------------

void FSurvivalPOIPreviewRunner::WriteEverything()
{
	RestorePlayerView();
	const FString Seed = GetSeedLabel();
	const TSharedRef<FJsonObject> Manifest = BuildManifest();
	FFileHelper::SaveStringToFile(SurvivalPOIPreview_ToJson(Manifest), *(M_Directory / FString::Printf(TEXT("poi_%s.json"), *Seed)));
	const TSharedRef<FJsonObject> Cameras = MakeShared<FJsonObject>();
	Cameras->SetArrayField(TEXT("shots"), M_CameraLog);
	Cameras->SetNumberField(TEXT("screen_aspect"), SurvivalPOIPreviewConstants::ScreenAspect);
	FFileHelper::SaveStringToFile(SurvivalPOIPreview_ToJson(Cameras), *(M_Directory / FString::Printf(TEXT("cameras_poi_%s.json"), *Seed)));
	WriteRenderSettings();
	UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI_PREVIEW manifest written for seed %s"), *Seed);
}

void FSurvivalPOIPreviewRunner::WriteRenderSettings() const
{
	const TSharedRef<FJsonObject> Settings = MakeShared<FJsonObject>();
	Settings->SetStringField(TEXT("rhi"), GDynamicRHI != nullptr ? GDynamicRHI->GetName() : TEXT("none"));
	FString FeatureLevel;
	GetFeatureLevelName(GMaxRHIFeatureLevel, FeatureLevel);
	Settings->SetStringField(TEXT("feature_level"), FeatureLevel);
	const TCHAR* const Variables[] = {
		TEXT("r.Nanite"), TEXT("r.Shadow.Virtual.Enable"), TEXT("r.ViewDistanceScale"), TEXT("r.TextureStreaming"),
		TEXT("sg.ViewDistanceQuality"), TEXT("sg.AntiAliasingQuality"), TEXT("sg.ShadowQuality"), TEXT("sg.GlobalIlluminationQuality"),
		TEXT("sg.ReflectionQuality"), TEXT("sg.PostProcessQuality"), TEXT("sg.TextureQuality"), TEXT("sg.EffectsQuality"),
		TEXT("sg.FoliageQuality"), TEXT("sg.ShadingQuality"), TEXT("sg.LandscapeQuality"), TEXT("r.ScreenPercentage")};
	for (const TCHAR* Name : Variables)
	{
		const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
		Settings->SetStringField(Name, Variable != nullptr ? Variable->GetString() : TEXT("missing"));
	}
	Settings->SetStringField(TEXT("command_line"), FCommandLine::Get());
	Settings->SetStringField(TEXT("exposure"), TEXT("the level's own post-process (auto exposure as in a match); the held camera adds no override"));
	Settings->SetStringField(TEXT("held_views"), TEXT("camera actor as the player's view target, main viewport screenshots, ShowFlag.PostProcessMaterial 0 (fog of war off) for every held view; the minimap shot keeps the player's own camera and UI"));
	FFileHelper::SaveStringToFile(SurvivalPOIPreview_ToJson(Settings), *(M_Directory / TEXT("render_settings.json")));
}

void FSurvivalPOIPreviewRunner::WriteCensus() const
{
	// An independent count of what stands in the world with the POI tag, against the spawn records.
	TMap<FString, int32> WorldCount;
	FString Lines;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (not It->Tags.Contains(USurvivalPointsOfInterestSubsystem::SpawnedTag))
		{
			continue;
		}
		const FString ClassName = It->GetClass()->GetName();
		WorldCount.FindOrAdd(ClassName)++;
		const FVector Location = It->GetActorLocation();
		Lines += FString::Printf(TEXT("%s\t%s\t%.1f\t%.1f\t%.1f\n"), *ClassName, *It->GetName(), Location.X, Location.Y, Location.Z);
	}
	TMap<FString, int32> RecordCount;
	double WorstDrift = 0.0;
	int32 LostRecords = 0;
	for (const FSurvivalPOISpawnRecord& Record : M_Subsystem->GetSpawnRecords())
	{
		if (not Record.Actor.IsValid())
		{
			++LostRecords;
			continue;
		}
		RecordCount.FindOrAdd(Record.Actor->GetClass()->GetName())++;
		WorstDrift = FMath::Max(WorstDrift, FVector::Dist(Record.Actor->GetActorLocation(), Record.SpawnedTransform.GetLocation()));
	}
	for (const FSurvivalWildlifeRecord& Record : M_Subsystem->GetWildlifeRecords())
	{
		for (const TWeakObjectPtr<AActor>& Animal : Record.Actors)
		{
			if (Animal.IsValid())
			{
				RecordCount.FindOrAdd(Animal->GetClass()->GetName())++;
			}
		}
	}
	FString Header = FString::Printf(TEXT("census at t=%.1f s after generation start, seed %s\nclass\tworld\tmanifest\n"),
	                                 M_CensusSeconds, *GetSeedLabel());
	bool bMatch = true;
	TSet<FString> Classes;
	WorldCount.GetKeys(Classes);
	for (const TPair<FString, int32>& Pair : RecordCount)
	{
		Classes.Add(Pair.Key);
	}
	for (const FString& ClassName : Classes)
	{
		const int32 World = WorldCount.FindRef(ClassName);
		const int32 Manifest = RecordCount.FindRef(ClassName);
		bMatch = bMatch and World == Manifest;
		Header += FString::Printf(TEXT("%s\t%d\t%d\n"), *ClassName, World, Manifest);
	}
	Header += FString::Printf(TEXT("counts match: %s; spawn records whose actor is gone: %d; largest move since spawn: %.1f cm\n\n"),
	                          bMatch ? TEXT("yes") : TEXT("NO"), LostRecords, WorstDrift);
	FFileHelper::SaveStringToFile(Header + Lines, *(M_Directory / FString::Printf(TEXT("census_%s.txt"), *GetSeedLabel())));
	UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI_PREVIEW census: counts match %s, lost %d, drift %.1f cm"),
	       bMatch ? TEXT("yes") : TEXT("NO"), LostRecords, WorstDrift);
}

TSharedRef<FJsonObject> FSurvivalPOIPreviewRunner::BuildManifest() const
{
	const TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
	Manifest->SetObjectField(TEXT("run"), BuildRunObject());
	TArray<TSharedPtr<FJsonValue>> Sites;
	const TArray<FSurvivalPOISite>& AllSites = M_Subsystem->GetSites();
	for (int32 OrderIndex = 0; OrderIndex < M_SiteOrder.Num(); ++OrderIndex)
	{
		Sites.Add(MakeShared<FJsonValueObject>(BuildSiteObject(AllSites[M_SiteOrder[OrderIndex]], OrderIndex)));
	}
	Manifest->SetArrayField(TEXT("pois"), Sites);
	Manifest->SetArrayField(TEXT("wildlife"), BuildWildlifeArray());
	TArray<TSharedPtr<FJsonValue>> Flee;
	for (const TPair<double, TArray<FVector>>& Sample : M_FleeSamples)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("seconds_after_arrival"), Sample.Key);
		TArray<TSharedPtr<FJsonValue>> Positions;
		for (const FVector& Position : Sample.Value)
		{
			Positions.Add(MakeShared<FJsonValueArray>(SurvivalPOIPreview_Vector(Position)));
		}
		Entry->SetArrayField(TEXT("positions"), Positions);
		Flee.Add(MakeShared<FJsonValueObject>(Entry));
	}
	const TSharedRef<FJsonObject> FleeObject = MakeShared<FJsonObject>();
	FleeObject->SetNumberField(TEXT("wildlife_record"), M_FleeRecord);
	FleeObject->SetBoolField(TEXT("tank_arrived"), M_FleeArrivalSeconds >= 0.0);
	FleeObject->SetArrayField(TEXT("samples"), Flee);
	Manifest->SetObjectField(TEXT("flee_test"), FleeObject);
	Manifest->SetObjectField(TEXT("exclusions"), BuildExclusions());
	Manifest->SetObjectField(TEXT("fairness"), BuildFairness());
	TSharedRef<FJsonObject> Reference = MakeShared<FJsonObject>();
	ComputeReferenceReward(Reference);
	Manifest->SetObjectField(TEXT("reward_reference"), Reference);
	return Manifest;
}

TSharedRef<FJsonObject> FSurvivalPOIPreviewRunner::BuildRunObject() const
{
	const TSharedRef<FJsonObject> Run = MakeShared<FJsonObject>();
	UWorld* World = GetWorld();
	Run->SetStringField(TEXT("map"), World->GetMapName());
	const FString MapFile = FPaths::ProjectContentDir() / TEXT("RTS_Survival/SurvivalMode/Maps/SM_Desert_01.umap");
	Run->SetStringField(TEXT("umap_timestamp"), IFileManager::Get().GetTimeStamp(*MapFile).ToString());
	const FString LayoutFile = FPaths::ProjectDir() / TEXT("Source/RTS_Survival/SurvivalMode/Docs/LevelTools/SM_Desert_01_layout.json");
	Run->SetStringField(TEXT("layout_json_timestamp"), IFileManager::Get().GetTimeStamp(*LayoutFile).ToString());
	Run->SetNumberField(TEXT("seed"), M_Subsystem->GetMatchSeed());
	const ASurvivalModeDirector* Director = M_Subsystem->GetDirector();
	Run->SetStringField(TEXT("difficulty"), IsValid(Director) ? FString::FromInt(static_cast<int32>(Director->GetDifficulty().DifficultyLevel)) : TEXT("?"));
	const ASurvivalStartLocation* Picked = IsValid(Director) ? Director->GetChosenStartLocation() : nullptr;
	const int32 PickedIndex = IsValid(Picked)
		                          ? Director->GetStartLocations().IndexOfByPredicate([Picked](const TWeakObjectPtr<ASurvivalStartLocation>& Start)
		                          {
			                          return Start.Get() == Picked;
		                          })
		                          : INDEX_NONE;
	Run->SetNumberField(TEXT("picked_start"), PickedIndex);
	Run->SetStringField(TEXT("picked_start_name"), IsValid(Picked) ? Picked->GetDisplayName().ToString() : TEXT(""));
	Run->SetStringField(TEXT("dll_build"), FString(ANSI_TO_TCHAR(__DATE__)) + TEXT(" ") + ANSI_TO_TCHAR(__TIME__));
	Run->SetStringField(TEXT("generation"), TEXT("runtime PCG: ASurvivalPOIPCGVolume UPCGComponent::GenerateLocal on PCG_SurvivalPointsOfInterest (Sites -> Compose -> Spawn, Sites -> Wildlife -> Spawn)"));
	Run->SetStringField(TEXT("settings"), M_Subsystem->GetSettings()->GetPathName());
	Run->SetNumberField(TEXT("census_seconds"), M_CensusSeconds);
	TArray<TSharedPtr<FJsonValue>> Notes;
	for (const FString& Note : M_Subsystem->GetPlanner()->GetPlanNotes())
	{
		Notes.Add(MakeShared<FJsonValueString>(Note));
	}
	Run->SetArrayField(TEXT("plan_notes"), Notes);
	const FSurvivalPOIPlanner* Planner = M_Subsystem->GetPlanner();
	TArray<TSharedPtr<FJsonValue>> Starts;
	for (const FSurvivalPOIStart& Start : Planner->GetContext().Starts)
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("index"), Start.DirectorIndex);
		Entry->SetStringField(TEXT("name"), Start.Name);
		Entry->SetArrayField(TEXT("centre"), SurvivalPOIPreview_Vector(FVector(Start.Position, Start.Height)));
		Entry->SetArrayField(TEXT("nav_from"), SurvivalPOIPreview_Vector(Start.NavOrigin));
		Starts.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Run->SetArrayField(TEXT("starts"), Starts);
	TArray<TSharedPtr<FJsonValue>> Sectors;
	for (const FSurvivalPOISector& Sector : Planner->GetSectors())
	{
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("sector"), Sector.SectorIndex);
		Entry->SetStringField(TEXT("start_a"), Planner->GetContext().Starts[Sector.StartA].Name);
		Entry->SetStringField(TEXT("start_b"), Planner->GetContext().Starts[Sector.StartB].Name);
		Entry->SetNumberField(TEXT("angle_a_deg"), FMath::RadiansToDegrees(Sector.AngleA));
		Entry->SetNumberField(TEXT("angle_b_deg"), FMath::RadiansToDegrees(Sector.AngleB));
		Entry->SetStringField(TEXT("landmark"), SurvivalPOINames::GetTypeName(Sector.Landmark));
		Entry->SetBoolField(TEXT("landmark_built"), Sector.bLandmarkBuilt);
		Entry->SetNumberField(TEXT("basin_floor_z"), Sector.BasinFloorHeight);
		Sectors.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Run->SetArrayField(TEXT("sectors"), Sectors);
	Run->SetArrayField(TEXT("basin_centre"), SurvivalPOIPreview_Vector(FVector(Planner->GetContext().BasinCenter, 0.0)));
	const FBox2D Bounds = Planner->GetContext().LandscapeBounds;
	Run->SetArrayField(TEXT("landscape_bounds"), SurvivalPOIPreview_Vector(FVector(Bounds.Min, 0.0)));
	Run->SetArrayField(TEXT("landscape_bounds_max"), SurvivalPOIPreview_Vector(FVector(Bounds.Max, 0.0)));
	Run->SetStringField(TEXT("nav_paths_from"), TEXT("each start's first defense point (main gate guard), deploy centre when none"));
	return Run;
}

TSharedRef<FJsonObject> FSurvivalPOIPreviewRunner::BuildSiteObject(const FSurvivalPOISite& Site, const int32 OrderIndex) const
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("id"), OrderIndex);
	Object->SetNumberField(TEXT("site_index"), Site.SiteIndex);
	Object->SetStringField(TEXT("type"), SurvivalPOINames::GetTypeName(Site.Type));
	Object->SetBoolField(TEXT("landmark"), Site.bIsLandmark);
	Object->SetNumberField(TEXT("sector"), Site.Sector);
	Object->SetNumberField(TEXT("sub_layout"), Site.SubLayout);
	Object->SetArrayField(TEXT("centre"), SurvivalPOIPreview_Vector(Site.Center));
	Object->SetNumberField(TEXT("yaw"), Site.YawDegrees);
	Object->SetNumberField(TEXT("radius"), Site.Radius);
	Object->SetNumberField(TEXT("owner_start"), Site.OwnerStart);
	TArray<TSharedPtr<FJsonValue>> Distances;
	double Nearest = TNumericLimits<double>::Max();
	int32 NearestIndex = INDEX_NONE;
	for (const FSurvivalPOIStart& Start : M_Subsystem->GetPlanner()->GetContext().Starts)
	{
		const double Distance = FVector2D::Distance(Start.Position, FVector2D(Site.Center));
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("start"), Start.DirectorIndex);
		Entry->SetNumberField(TEXT("straight"), FMath::RoundToDouble(Distance));
		Distances.Add(MakeShared<FJsonValueObject>(Entry));
		if (Distance < Nearest)
		{
			Nearest = Distance;
			NearestIndex = Start.DirectorIndex;
		}
	}
	Object->SetNumberField(TEXT("nearest_start"), NearestIndex);
	Object->SetArrayField(TEXT("start_distances"), Distances);
	Object->SetStringField(TEXT("nearest_route"), TEXT("routes not on the map yet"));
	TArray<TSharedPtr<FJsonValue>> Actors;
	double SiteReward = 0.0;
	int32 Captures = 0;
	for (const FSurvivalPOISpawnRecord& Record : M_Subsystem->GetSpawnRecords())
	{
		if (Record.Planned.SiteIndex != Site.SiteIndex)
		{
			continue;
		}
		const TSharedPtr<FJsonObject> Actor = BuildActorObject(Record);
		if (not Actor.IsValid())
		{
			continue;
		}
		SiteReward += Actor->GetNumberField(TEXT("reward_expected_total"));
		Captures += Record.Planned.Role == ESurvivalPOIRole::Capture ? 1 : 0;
		Actors.Add(MakeShared<FJsonValueObject>(Actor));
	}
	Object->SetNumberField(TEXT("reward_expected_total"), SiteReward);
	Object->SetNumberField(TEXT("capture_targets"), Captures);
	Object->SetArrayField(TEXT("actors"), Actors);
	return Object;
}

TSharedPtr<FJsonObject> FSurvivalPOIPreviewRunner::BuildActorObject(const FSurvivalPOISpawnRecord& Record) const
{
	using namespace SurvivalPOIPreviewConstants;
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	const USurvivalPointsOfInterestSettings* Settings = M_Subsystem->GetSettings();
	const FSurvivalPOIAsset* Asset = Settings->GetAssetByKey(Record.Planned.AssetKey);
	Object->SetStringField(TEXT("asset"), Asset != nullptr ? Asset->GetDisplayPath() : TEXT("?"));
	Object->SetStringField(TEXT("role"), SurvivalPOINames::GetRoleName(Record.Planned.Role));
	Object->SetBoolField(TEXT("buried"), Record.Planned.SinkCm > 0.f);
	const FSurvivalPOIAsset* SeatAsset = M_Subsystem->GetSettings()->GetAssetByKey(Record.Planned.AssetKey);
	Object->SetBoolField(TEXT("dug_in_pivot"), SeatAsset != nullptr and SeatAsset->SeatMode == ESurvivalPOISeatMode::PivotOnGround);
	if (not Record.Actor.IsValid())
	{
		Object->SetBoolField(TEXT("alive"), false);
		Object->SetNumberField(TEXT("reward_expected_total"), 0.0);
		return Object;
	}
	AActor* Actor = Record.Actor.Get();
	Object->SetBoolField(TEXT("alive"), true);
	Object->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
	Object->SetArrayField(TEXT("xyz"), SurvivalPOIPreview_Vector(Actor->GetActorLocation()));
	const FRotator Rotation = Actor->GetActorRotation();
	Object->SetArrayField(TEXT("rotation_pyr"), SurvivalPOIPreview_Vector(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
	// Seat as it stands now, measured again on the visible meshes.
	FSurvivalPOISeatReport Seat;
	if (M_Subsystem->GetProjector() != nullptr and Record.Planned.Role != ESurvivalPOIRole::Decal)
	{
		SurvivalPOISeating::MeasureSeat(*Actor, *M_Subsystem->GetProjector(), Seat);
	}
	Object->SetBoolField(TEXT("seat_measured"), Seat.bMeasured);
	if (Seat.bMeasured)
	{
		Object->SetStringField(TEXT("seat_method"), Seat.bUsedBoxBottom ? TEXT("visible box bottom (no collision)") : TEXT("underside traces"));
		Object->SetNumberField(TEXT("gap"), FMath::Max(0.f, Seat.SupportGap));
		Object->SetNumberField(TEXT("max_quadrant_gap"), Seat.MaxGap);
		Object->SetStringField(TEXT("gap_definition"), TEXT("gap = second lowest of the four footprint quadrants' lowest underside point above the ground (rest on two supports); max_quadrant_gap = highest quadrant"));
		Object->SetNumberField(TEXT("sink"), Seat.MaxSink);
		Object->SetNumberField(TEXT("slope"), Seat.GroundSlopeDegrees);
		Object->SetNumberField(TEXT("normal_dev"), Seat.NormalDeviationDegrees);
		TArray<TSharedPtr<FJsonValue>> Points;
		for (int32 Quadrant = 0; Quadrant < Seat.ContactPoints.Num(); ++Quadrant)
		{
			const TSharedRef<FJsonObject> Point = MakeShared<FJsonObject>();
			Point->SetArrayField(TEXT("contact"), SurvivalPOIPreview_Vector(Seat.ContactPoints[Quadrant]));
			Point->SetNumberField(TEXT("ground_z"), Seat.GroundHeights[Quadrant]);
			Point->SetNumberField(TEXT("gap"), Seat.QuadrantGaps[Quadrant]);
			Points.Add(MakeShared<FJsonValueObject>(Point));
		}
		Object->SetArrayField(TEXT("footprint_points"), Points);
		Object->SetArrayField(TEXT("lod0_box_local_min"), SurvivalPOIPreview_Vector(Seat.LocalBoxMin));
		Object->SetArrayField(TEXT("lod0_box_local_max"), SurvivalPOIPreview_Vector(Seat.LocalBoxMax));
	}
	const TMap<FString, double> Reward = SurvivalPOIPreview_GetExpectedReward(*Actor);
	const TSharedRef<FJsonObject> RewardObject = MakeShared<FJsonObject>();
	for (const TPair<FString, double>& Pair : Reward)
	{
		RewardObject->SetNumberField(Pair.Key, Pair.Value);
	}
	Object->SetObjectField(TEXT("reward_expected"), RewardObject);
	Object->SetNumberField(TEXT("reward_expected_total"), SurvivalPOIPreview_Sum(Reward));
	const FSurvivalPOIStart* Owner = FindStart(M_Subsystem->GetSites().IsValidIndex(Record.Planned.SiteIndex)
		                                           ? M_Subsystem->GetSites()[Record.Planned.SiteIndex].OwnerStart
		                                           : INDEX_NONE);
	const FVector From = Owner != nullptr ? Owner->NavOrigin : Actor->GetActorLocation();
	TOptional<FVector> Interaction;
	if (const AScavengeableObject* Scavenge = Cast<AScavengeableObject>(Actor))
	{
		Interaction = Scavenge->GetScavLocationClosestToPosition(From);
	}
	if (ACaptureActor* Capture = Cast<ACaptureActor>(Actor))
	{
		Interaction = Capture->GetCaptureLocationClosestTo(From);
		const FClassProperty* Captured = FindFProperty<FClassProperty>(Actor->GetClass(), TEXT("Player1ClassOnCapture"));
		const UClass* CapturedClass = Captured != nullptr ? Cast<UClass>(Captured->GetPropertyValue_InContainer(Actor)) : nullptr;
		Object->SetStringField(TEXT("captured_unit"), IsValid(CapturedClass) ? CapturedClass->GetName() : TEXT("none"));
	}
	if (Interaction.IsSet())
	{
		Object->SetArrayField(TEXT("interaction_point"), SurvivalPOIPreview_Vector(Interaction.GetValue()));
		const SurvivalPOINav::FChainedPathResult Tank = FindChainedPath(From, Interaction.GetValue(), SurvivalPOINav::HeavyTankRadius, SurvivalPOINav::HeavyTankHeight);
		const SurvivalPOINav::FChainedPathResult Infantry = FindChainedPath(From, Interaction.GetValue(), SurvivalPOINav::CharacterRadius, SurvivalPOINav::CharacterHeight);
		const TSharedRef<FJsonObject> Nav = MakeShared<FJsonObject>();
		Nav->SetBoolField(TEXT("heavytank_reached"), Tank.bReached);
		Nav->SetNumberField(TEXT("heavytank_length"), FMath::RoundToDouble(Tank.Length));
		Nav->SetNumberField(TEXT("heavytank_hops"), Tank.Hops);
		Nav->SetBoolField(TEXT("character_reached"), Infantry.bReached);
		Nav->SetNumberField(TEXT("character_length"), FMath::RoundToDouble(Infantry.Length));
		Nav->SetNumberField(TEXT("from_start"), Owner != nullptr ? Owner->DirectorIndex : -1);
		Object->SetObjectField(TEXT("interaction_nav"), Nav);
	}
	return Object;
}

TArray<TSharedPtr<FJsonValue>> FSurvivalPOIPreviewRunner::BuildWildlifeArray() const
{
	using namespace SurvivalPOIPreviewConstants;
	TArray<TSharedPtr<FJsonValue>> Result;
	const USurvivalPointsOfInterestSettings* Settings = M_Subsystem->GetSettings();
	const TArray<FSurvivalWildlifeRecord>& Records = M_Subsystem->GetWildlifeRecords();
	const FSurvivalPOIPlanner* Planner = M_Subsystem->GetPlanner();
	for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
	{
		const FSurvivalWildlifeRecord& Record = Records[RecordIndex];
		const FSurvivalWildlifeEntry& Entry = Settings->Wildlife[Record.Group.EntryIndex];
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		const bool bGround = Entry.Kind == ESurvivalWildlifeKind::GroundHerd;
		Object->SetStringField(TEXT("kind"), bGround ? TEXT("ground group") : TEXT("flock"));
		Object->SetStringField(TEXT("name"), Entry.Name.ToString());
		Object->SetStringField(TEXT("class"), bGround or Entry.Kind == ESurvivalWildlifeKind::BirdFlock
			                                      ? Entry.ActorClass.GetAssetName() : Entry.NiagaraSystem.GetAssetName());
		Object->SetNumberField(TEXT("count"), Record.Group.Count);
		Object->SetNumberField(TEXT("actors_spawned"), Record.Actors.Num());
		Object->SetArrayField(TEXT("spawn_centre"), SurvivalPOIPreview_Vector(Record.Group.Center));
		Object->SetNumberField(TEXT("over_site"), Record.Group.SiteIndex);
		double NearestStart = TNumericLimits<double>::Max();
		for (const FSurvivalPOIStart& Start : Planner->GetContext().Starts)
		{
			NearestStart = FMath::Min(NearestStart, FVector2D::Distance(Start.Position, FVector2D(Record.Group.Center)));
		}
		Object->SetNumberField(TEXT("nearest_start_distance"), FMath::RoundToDouble(NearestStart));
		if (bGround)
		{
			Object->SetNumberField(TEXT("wander_radius"), Entry.WanderRadius);
			Object->SetNumberField(TEXT("flee_distance"), Entry.FleeDistance);
			// The subsystem's own alarm check (CheckWildlifeAlarms) uses the entry's radius.
			Object->SetNumberField(TEXT("alarm_radius"), Entry.AlarmRadius);
		}
		else
		{
			Object->SetNumberField(TEXT("flight_height"), Record.Group.FlightHeight);
			Object->SetArrayField(TEXT("ellipse_axes"), SurvivalPOIPreview_Vector(FVector(Record.Group.SemiAxes, 0.0)));
		}
		TArray<TSharedPtr<FJsonValue>> Samples;
		for (int32 Sample = 0; Sample < M_AnimalSamples.Num(); ++Sample)
		{
			if (not M_AnimalSamples[Sample].IsValidIndex(RecordIndex))
			{
				continue;
			}
			const TSharedRef<FJsonObject> SampleObject = MakeShared<FJsonObject>();
			SampleObject->SetNumberField(TEXT("t"), M_SampleSeconds[Sample]);
			TArray<TSharedPtr<FJsonValue>> Positions;
			for (const FVector& Position : M_AnimalSamples[Sample][RecordIndex])
			{
				Positions.Add(MakeShared<FJsonValueArray>(SurvivalPOIPreview_Vector(Position)));
			}
			SampleObject->SetArrayField(TEXT("positions"), Positions);
			Samples.Add(MakeShared<FJsonValueObject>(SampleObject));
		}
		Object->SetArrayField(TEXT("samples"), Samples);
		Result.Add(MakeShared<FJsonValueObject>(Object));
	}
	return Result;
}

void FSurvivalPOIPreviewRunner::CheckExclusions(const FVector& Position, const FString& What,
                                                TArray<TSharedPtr<FJsonValue>>& OutViolations) const
{
	const FSurvivalPOIPlanner* Planner = M_Subsystem->GetPlanner();
	const FSurvivalPOIPlacementRules& Rules = M_Subsystem->GetSettings()->Placement;
	const FSurvivalPOIWorldContext& Context = Planner->GetContext();
	const FVector2D Point(Position);
	auto Add = [&OutViolations, &What](const FString& Rule, const double Value)
	{
		OutViolations.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s: %s (%.0f)"), *What, *Rule, Value)));
	};
	for (const FSurvivalPOIStart& Start : Context.Starts)
	{
		const double Distance = FVector2D::Distance(Start.Position, Point);
		if (Distance < Rules.MinDistanceFromStart)
		{
			Add(TEXT("near start ") + Start.Name, Distance);
		}
	}
	FBox2D Inner = Context.LandscapeBounds;
	Inner.Min += FVector2D(Rules.MinDistanceFromLandscapeEdge);
	Inner.Max -= FVector2D(Rules.MinDistanceFromLandscapeEdge);
	if (not Inner.IsInside(Point))
	{
		Add(TEXT("near landscape edge"), 0.0);
	}
	for (const TPair<FVector2D, FVector2D>& Segment : Context.SpawnZoneSegments)
	{
		const double Distance = FVector2D::Distance(FMath::ClosestPointOnSegment2D(Point, Segment.Key, Segment.Value), Point);
		if (Distance < Rules.MinDistanceFromEdgeSpawnZone)
		{
			Add(TEXT("near edge spawn zone"), Distance);
		}
	}
	for (const FBox2D& Box : Context.SovietBaseBoxes)
	{
		if (Box.ExpandBy(Rules.SovietBaseClearance).IsInside(Point))
		{
			Add(TEXT("inside Soviet base volume + clearance"), 0.0);
		}
	}
	for (const FSurvivalPOIExclusionZone& Zone : Rules.ExclusionZones)
	{
		const double Distance = FVector2D::Distance(Zone.Center, Point);
		if (Distance < Zone.Radius + Zone.PropRim)
		{
			Add(TEXT("inside ") + Zone.Name.ToString() + TEXT(" + rim"), Distance);
		}
	}
	for (const FVector& Spot : Context.ReservedSpots)
	{
		const double Distance = FVector2D::Distance(FVector2D(Spot), Point);
		if (Distance < Spot.Z)
		{
			Add(TEXT("inside a start's reserved ramp/gate spot"), Distance);
		}
	}
}

TSharedRef<FJsonObject> FSurvivalPOIPreviewRunner::BuildExclusions() const
{
	using namespace SurvivalPOIPreviewConstants;
	const TSharedRef<FJsonObject> Exclusions = MakeShared<FJsonObject>();
	const FSurvivalPOIPlacementRules& Rules = M_Subsystem->GetSettings()->Placement;
	Exclusions->SetNumberField(TEXT("min_start_distance"), Rules.MinDistanceFromStart);
	Exclusions->SetNumberField(TEXT("min_landscape_edge"), Rules.MinDistanceFromLandscapeEdge);
	Exclusions->SetNumberField(TEXT("min_spawn_zone"), Rules.MinDistanceFromEdgeSpawnZone);
	Exclusions->SetNumberField(TEXT("soviet_volume_clearance"), Rules.SovietBaseClearance);
	Exclusions->SetStringField(TEXT("not_checked_here"), TEXT("ramps, gates and pad rims (terrain test at plan time; checked against the layout JSON by poi_check.py)"));
	TArray<TSharedPtr<FJsonValue>> Violations;
	int32 Checked = 0;
	for (const FSurvivalPOISpawnRecord& Record : M_Subsystem->GetSpawnRecords())
	{
		if (Record.Actor.IsValid())
		{
			CheckExclusions(Record.Actor->GetActorLocation(), Record.Actor->GetName(), Violations);
			++Checked;
		}
	}
	// Animals: never near a start, never in the water basin, never on steep ground, at every sample.
	const FSurvivalPOIPlanner* Planner = M_Subsystem->GetPlanner();
	int32 AnimalChecks = 0;
	for (int32 Sample = 0; Sample < M_AnimalSamples.Num(); ++Sample)
	{
		for (int32 RecordIndex = 0; RecordIndex < M_AnimalSamples[Sample].Num(); ++RecordIndex)
		{
			const FSurvivalWildlifeRecord& Record = M_Subsystem->GetWildlifeRecords()[RecordIndex];
			if (M_Subsystem->GetSettings()->Wildlife[Record.Group.EntryIndex].Kind != ESurvivalWildlifeKind::GroundHerd)
			{
				continue;
			}
			for (const FVector& Position : M_AnimalSamples[Sample][RecordIndex])
			{
				if (Position.X >= UE_BIG_NUMBER * 0.5)
				{
					continue;
				}
				++AnimalChecks;
				const FString What = FString::Printf(TEXT("animal g%d t%d"), RecordIndex, Sample * 5);
				for (const FSurvivalPOIStart& Start : Planner->GetContext().Starts)
				{
					const double Distance = FVector2D::Distance(Start.Position, FVector2D(Position));
					if (Distance < AnimalMinStartDistance)
					{
						Violations.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s: near start (%.0f)"), *What, Distance)));
					}
				}
				if (Planner->GetIsNearWater(FVector2D(Position), 0.f))
				{
					Violations.Add(MakeShared<FJsonValueString>(What + TEXT(": inside the water basin")));
				}
				FSurvivalGroundHit Hit;
				if (M_Subsystem->GetProjector()->TryProject(FVector2D(Position), Hit)
					and FSurvivalGroundProjector::GetSlopeDegrees(Hit.Normal) > AnimalMaxSlope)
				{
					Violations.Add(MakeShared<FJsonValueString>(What + TEXT(": slope over 25")));
				}
			}
		}
	}
	Exclusions->SetNumberField(TEXT("actors_checked"), Checked);
	Exclusions->SetNumberField(TEXT("animal_samples_checked"), AnimalChecks);
	Exclusions->SetArrayField(TEXT("violations"), Violations);
	return Exclusions;
}

TSharedRef<FJsonObject> FSurvivalPOIPreviewRunner::BuildFairness() const
{
	using namespace SurvivalPOIPreviewConstants;
	const TSharedRef<FJsonObject> Fairness = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Rows;
	const FSurvivalPOIPlanner* Planner = M_Subsystem->GetPlanner();
	const TArray<FSurvivalPOISpawnRecord>& Records = M_Subsystem->GetSpawnRecords();
	for (const FSurvivalPOIStart& Start : Planner->GetContext().Starts)
	{
		const FVector From = Start.NavOrigin;
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetArrayField(TEXT("nav_from"), SurvivalPOIPreview_Vector(From));
		Row->SetNumberField(TEXT("start"), Start.DirectorIndex);
		Row->SetStringField(TEXT("name"), Start.Name);
		int32 WithinReach = 0;
		for (const FSurvivalPOISite& Site : M_Subsystem->GetSites())
		{
			WithinReach += FVector2D::Distance(Start.Position, FVector2D(Site.Center)) <= FairnessReach ? 1 : 0;
		}
		Row->SetNumberField(TEXT("pois_within_30000_straight"), WithinReach);
		// The drive to every site (chained HeavyTank nav to its centre), so "nearest" is nearest by drive, not by straight line.
		TMap<int32, double> DriveToSite;
		int32 WithinDrive = 0;
		for (const FSurvivalPOISite& Site : M_Subsystem->GetSites())
		{
			const SurvivalPOINav::FChainedPathResult Path = FindChainedPath(From, Site.Center, SurvivalPOINav::HeavyTankRadius,
			                                                                SurvivalPOINav::HeavyTankHeight);
			if (Path.bReached)
			{
				DriveToSite.Add(Site.SiteIndex, Path.Length);
				WithinDrive += Path.Length <= FairnessReach ? 1 : 0;
			}
		}
		Row->SetNumberField(TEXT("pois_within_30000_nav"), WithinDrive);
		for (const ESurvivalPOIRole Kind : {ESurvivalPOIRole::Scavenge, ESurvivalPOIRole::Capture})
		{
			double Shortest = -1.0;
			int32 ShortestSite = INDEX_NONE;
			for (const FSurvivalPOISpawnRecord& Record : Records)
			{
				const double* Drive = DriveToSite.Find(Record.Planned.SiteIndex);
				if (Record.Planned.Role != Kind or not Record.Actor.IsValid() or Drive == nullptr)
				{
					continue;
				}
				if (Shortest < 0.0 or *Drive < Shortest)
				{
					Shortest = *Drive;
					ShortestSite = Record.Planned.SiteIndex;
				}
			}
			const FString Key = SurvivalPOINames::GetRoleName(Kind);
			Row->SetNumberField(Key + TEXT("_nav_site_heavytank"), FMath::RoundToDouble(Shortest));
			Row->SetNumberField(Key + TEXT("_nav_site_index"), ShortestSite);
		}
		for (const ESurvivalPOIRole Kind : {ESurvivalPOIRole::Scavenge, ESurvivalPOIRole::Capture})
		{
			const AActor* Nearest = nullptr;
			double NearestDistance = TNumericLimits<double>::Max();
			for (const FSurvivalPOISpawnRecord& Record : Records)
			{
				if (Record.Planned.Role != Kind or not Record.Actor.IsValid())
				{
					continue;
				}
				const double Distance = FVector::Dist2D(From, Record.Actor->GetActorLocation());
				if (Distance < NearestDistance)
				{
					NearestDistance = Distance;
					Nearest = Record.Actor.Get();
				}
			}
			const FString Key = SurvivalPOINames::GetRoleName(Kind);
			Row->SetNumberField(Key + TEXT("_straight"), Nearest != nullptr ? FMath::RoundToDouble(NearestDistance) : -1.0);
			if (Nearest != nullptr)
			{
				const SurvivalPOINav::FChainedPathResult Path = FindChainedPath(From, Nearest->GetActorLocation(), SurvivalPOINav::HeavyTankRadius, SurvivalPOINav::HeavyTankHeight);
				Row->SetNumberField(Key + TEXT("_nav_heavytank"), Path.bReached ? FMath::RoundToDouble(Path.Length) : -1.0);
			}
		}
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	Fairness->SetArrayField(TEXT("per_start"), Rows);
	return Fairness;
}

double FSurvivalPOIPreviewRunner::ComputeReferenceReward(TSharedRef<FJsonObject>& OutReference) const
{
	using namespace SurvivalPOIPreviewConstants;
	// The start ring's tank hulks, spawned once each high above the map, read, and removed again.
	const ASurvivalModeDirector* Director = M_Subsystem->GetDirector();
	const USurvivalStartContentSettings* StartContent = IsValid(Director) and IsValid(Director->GetSettings())
		                                                    ? Director->GetSettings()->StartContent.Get()
		                                                    : nullptr;
	if (not IsValid(StartContent))
	{
		StartContent = GetDefault<USurvivalStartContentSettings>();
	}
	const FSurvivalWreckGroup* Hulks = StartContent->WreckGroups.FindByPredicate([](const FSurvivalWreckGroup& Group)
	{
		return Group.GroupName == TEXT("TankHulks");
	});
	if (Hulks == nullptr)
	{
		OutReference->SetStringField(TEXT("error"), TEXT("no TankHulks wreck group"));
		return 0.0;
	}
	double WeightedSum = 0.0;
	double TotalWeight = 0.0;
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const FSurvivalWeightedActorClass& Wreck : Hulks->Wrecks)
	{
		UClass* Class = Wreck.ActorClass.LoadSynchronous();
		if (not IsValid(Class))
		{
			continue;
		}
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Probe = GetWorld()->SpawnActor<AActor>(Class, FTransform(FVector(0.0, 0.0, ReferenceSpawnHeight)), Parameters);
		if (not IsValid(Probe))
		{
			continue;
		}
		const double Value = SurvivalPOIPreview_Sum(SurvivalPOIPreview_GetExpectedReward(*Probe));
		Probe->Destroy();
		WeightedSum += Value * Wreck.Weight;
		TotalWeight += Wreck.Weight;
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("class"), Class->GetName());
		Entry->SetNumberField(TEXT("weight"), Wreck.Weight);
		Entry->SetNumberField(TEXT("expected_total"), Value);
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	const double Reference = TotalWeight > 0.0 ? WeightedSum / TotalWeight : 0.0;
	OutReference->SetArrayField(TEXT("tank_hulks"), Entries);
	OutReference->SetNumberField(TEXT("start_ring_wreck_expected_total"), Reference);
	OutReference->SetStringField(TEXT("how"), TEXT("each TankHulks class of the match's start content spawned once, live ScavengeResources read (Chance% x set amount or (Min+Max)/2, summed over resource types), weighted mean by the group weights"));
	return Reference;
}

// ---------------------------------------------------------------------------
// Sweep
// ---------------------------------------------------------------------------

void FSurvivalPOIPreviewRunner::RunSweep()
{
	FSurvivalPOIPlanner* Planner = M_Subsystem->GetMutablePlanner();
	if (Planner == nullptr)
	{
		return;
	}
	const double Start = FPlatformTime::Seconds();
	FString Csv = TEXT("seed,sites,battlefield,hangar,crashsite,convoy,graveyard,pumpstation,relaystation,objects,wildlife_groups,violations,anchors_missing");
	const FSurvivalPOIWorldContext& Context = Planner->GetContext();
	for (const FSurvivalPOIStart& StartEntry : Context.Starts)
	{
		Csv += FString::Printf(TEXT(",s%d_scavenge,s%d_capture,s%d_within30k"), StartEntry.DirectorIndex, StartEntry.DirectorIndex,
		                       StartEntry.DirectorIndex);
	}
	Csv += TEXT(",signatures\n");
	for (int32 Seed = 1; Seed <= M_SweepCount; ++Seed)
	{
		Planner->ReplanSites(Seed);
		const TArray<FSurvivalPOISite> Sites = Planner->GetSites();
		TArray<FSurvivalPOIPlacedObject> Objects;
		for (const FSurvivalPOISite& Site : Sites)
		{
			Planner->ComposeSite(Site, Objects);
		}
		TArray<FSurvivalWildlifeGroup> Groups;
		Planner->PlanWildlife(Sites, Groups);
		TMap<ESurvivalPOIType, int32> TypeCount;
		for (const FSurvivalPOISite& Site : Sites)
		{
			TypeCount.FindOrAdd(Site.Type)++;
		}
		TArray<TSharedPtr<FJsonValue>> Violations;
		for (const FSurvivalPOIPlacedObject& Object : Objects)
		{
			CheckExclusions(Object.Location, TEXT("object"), Violations);
		}
		int32 AnchorsMissing = 0;
		for (const FSurvivalPOISite& Site : Sites)
		{
			TArray<FSurvivalPOIPlacedObject> SiteObjects = Objects.FilterByPredicate([&Site](const FSurvivalPOIPlacedObject& Object)
			{
				return Object.SiteIndex == Site.SiteIndex;
			});
			AnchorsMissing += FSurvivalPOIPlanner::GetHoldsAnchor(Site, SiteObjects) ? 0 : 1;
		}
		Csv += FString::Printf(TEXT("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"), Seed, Sites.Num(),
		                       TypeCount.FindRef(ESurvivalPOIType::SmallBattlefield), TypeCount.FindRef(ESurvivalPOIType::RuinedHangar),
		                       TypeCount.FindRef(ESurvivalPOIType::CrashSite), TypeCount.FindRef(ESurvivalPOIType::DeadConvoy),
		                       TypeCount.FindRef(ESurvivalPOIType::TankGraveyard), TypeCount.FindRef(ESurvivalPOIType::PumpStation),
		                       TypeCount.FindRef(ESurvivalPOIType::RelayStation), Objects.Num(), Groups.Num(), Violations.Num(), AnchorsMissing);
		for (const FSurvivalPOIStart& StartEntry : Context.Starts)
		{
			double Nearest[2] = {-1.0, -1.0};
			for (const FSurvivalPOIPlacedObject& Object : Objects)
			{
				const int32 Slot = Object.Role == ESurvivalPOIRole::Scavenge ? 0 : (Object.Role == ESurvivalPOIRole::Capture ? 1 : -1);
				if (Slot < 0)
				{
					continue;
				}
				const double Distance = FVector2D::Distance(StartEntry.Position, FVector2D(Object.Location));
				Nearest[Slot] = Nearest[Slot] < 0.0 ? Distance : FMath::Min(Nearest[Slot], Distance);
			}
			int32 Within = 0;
			for (const FSurvivalPOISite& Site : Sites)
			{
				Within += FVector2D::Distance(StartEntry.Position, FVector2D(Site.Center)) <= SurvivalPOIPreviewConstants::FairnessReach ? 1 : 0;
			}
			Csv += FString::Printf(TEXT(",%.0f,%.0f,%d"), Nearest[0], Nearest[1], Within);
		}
		// Layout signature per site: the objects' rounded positions and yaws relative to the site, hashed.
		FString Signatures;
		for (const FSurvivalPOISite& Site : Sites)
		{
			uint32 Hash = GetTypeHash(static_cast<int32>(Site.Type));
			const FTransform SiteFrame(FRotator(0.f, Site.YawDegrees, 0.f), Site.Center);
			for (const FSurvivalPOIPlacedObject& Object : Objects)
			{
				if (Object.SiteIndex != Site.SiteIndex)
				{
					continue;
				}
				const FVector Local = SiteFrame.InverseTransformPosition(Object.Location);
				Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt(Local.X / 100.0), FMath::RoundToInt(Local.Y / 100.0),
				                                                 FMath::RoundToInt(FRotator::NormalizeAxis(Object.YawDegrees - Site.YawDegrees) / 10.0))));
				Hash = HashCombine(Hash, GetTypeHash(Object.AssetKey));
			}
			Signatures += FString::Printf(TEXT("%s%s:%08x"), Signatures.IsEmpty() ? TEXT("") : TEXT(" "),
			                              *SurvivalPOINames::GetTypeName(Site.Type), Hash);
		}
		Csv += TEXT(",") + Signatures + TEXT("\n");
	}
	FFileHelper::SaveStringToFile(Csv, *(M_Directory / TEXT("sweep.csv")));
	UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI_PREVIEW sweep of %d seeds written in %.1f s"), M_SweepCount,
	       FPlatformTime::Seconds() - Start);
}

// ---------------------------------------------------------------------------

void FSurvivalPOIPreview::StartIfRequested(USurvivalPointsOfInterestSubsystem* Subsystem)
{
	const TCHAR* const CommandLine = FCommandLine::Get();
	int32 SweepCount = 0;
	const bool bRequested = FParse::Param(CommandLine, TEXT("SurvivalPOIPreview")) or FParse::Param(CommandLine, TEXT("SurvivalPOIWatch"))
		or FParse::Value(CommandLine, TEXT("SurvivalPOISweep="), SweepCount);
	if (not bRequested or not IsValid(Subsystem))
	{
		return;
	}
	const TSharedRef<FSurvivalPOIPreviewRunner> Runner = MakeShared<FSurvivalPOIPreviewRunner>(Subsystem);
	Runner->Start();
}

#endif
