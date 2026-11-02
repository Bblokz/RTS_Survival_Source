// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBasePreview.h"

#if RTS_WITH_MAP_TESTS

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "DynamicRHI.h"
#include "RenderTimer.h"
#include "RHIStats.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseDressing.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIHelpers.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/AsyncUnitDetailedState/AsyncUnitDetailedState.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseComponent.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseSettings.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOINav.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovietBasePreview, Log, All);

namespace SovietBasePreviewConstants
{
	constexpr double GateShotGateFromLaunch = 2200.0;
	constexpr double GateShotBack = 3200.0;
	constexpr double GateShotSide = 1800.0;
	constexpr double GateShotHeight = 2300.0;
	constexpr double GateShotLookPast = 1200.0;
	constexpr float GateShotFieldOfView = 55.f;
	// The critic's fixed views (base_visual_review/critic_rubric.md): RTS oblique and close-ups.
	constexpr double RtsDistance = 5200.0;
	constexpr double RtsPitchDegrees = 55.0;
	constexpr float RtsFieldOfView = 70.f;
	constexpr double GateFrontBack = 2800.0;
	constexpr double GateFrontHeight = 1600.0;
	constexpr double CloseLookHeight = 250.0;
	constexpr float CloseFieldOfView = 50.f;
	constexpr double WallLongBack = 3500.0;
	constexpr double WallLongHeight = 1500.0;
	constexpr float WallLongFieldOfView = 60.f;
	constexpr double WallCornerBack = 2400.0;
	constexpr double WallCornerHeight = 1800.0;
	constexpr double PadShotDistance = 2000.0;
	constexpr double PadShotHeight = 1400.0;
	// Footprint close views (critic round 4): eye 1 200 cm out on the door side, 500 up, FOV 45, a little off-axis.
	// Round 6: higher and looking back at the door edge so the building, its ground and the apron share the frame (round 5's
	// eye at 500 up saw a wall or only sand), lit from the camera's side with the capture's exposure bias.
	constexpr double FootprintCloseDistance = 1400.0;
	constexpr double FootprintCloseSideOffset = 500.0;
	constexpr double FootprintCloseHeight = 1100.0;
	constexpr double FootprintCloseLookAhead = -150.0;
	constexpr float FootprintCloseFieldOfView = 50.f;
	constexpr double JunctionTopHeight = 3000.0;
	constexpr float JunctionTopFieldOfView = 60.f;
	constexpr double JunctionTolerance = 50.0;
	constexpr double BunkerShotBack = 2500.0;
	constexpr double BunkerShotSide = 1200.0;
	constexpr double BunkerShotHeight = 1500.0;
	constexpr double BunkerTieWeight = 0.001;
	constexpr double YardShotBack = 2600.0;
	// Eyes move back with the size of their target, so a large building or yard stays whole in frame.
	constexpr double PadShotFootprintFactor = 1.5;
	constexpr double YardShotExtentFactor = 1.2;
	constexpr double YardTopMargin = 500.0;
	constexpr int32 MaxOpenSlotShots = 16;
	constexpr double OpenSlotShotBack = 2000.0;
	constexpr double OpenSlotShotHeight = 1400.0;
	constexpr double FlagShotBack = 1800.0;
	constexpr double FlagShotHeight = 600.0;
	constexpr double FlagLookHeight = 350.0;
	constexpr double YardShotHeight = 1800.0;
	constexpr float YardFieldOfView = 55.f;
	constexpr double YardTopHeight = 3500.0;
	constexpr float YardTopFieldOfView = 60.f;
	constexpr double YardPairMargin = 2500.0;
	constexpr double TowerShotBack = 2200.0;
	constexpr double TowerShotHeight = 1600.0;
	// Extra low close-ups of the worker: joints, faces and flicker at eye height.
	constexpr double LowGateBack = 2400.0;
	constexpr double LowGateSide = 700.0;
	constexpr double LowHeight = 450.0;
	constexpr double LowRunOut = 700.0;
	constexpr double LowRunBack = 300.0;
	constexpr float RunFieldOfView = 45.f;
	constexpr double LowInsideBack = 2200.0;
	constexpr double LowInsideHeight = 900.0;
	constexpr float TickSeconds = 0.25f;
	constexpr int32 MatchSeedsPerStart = 6;
	constexpr double LongFrameSeconds = 0.1;
	// The fixed force: five Panzer IV H, 105 supply.
	constexpr int32 FightTanks = 5;
	constexpr double FightStandOff = 1200.0;
	constexpr double FightTankSpacing = 650.0;
	constexpr double FightSpawnHeight = 150.0;
	constexpr float FightOrderSeconds = 12.f;
	constexpr float FightTimeLimitSeconds = 300.f;
	constexpr int32 MatchSeedStride = 7919;
	constexpr float WarmUpSeconds = 4.f;
	constexpr float SettleSeconds = 4.f;
	constexpr float CaptureDelaySeconds = 2.5f;
	constexpr float BaseTimeoutSeconds = 90.f;
	constexpr int32 TopPictureSize = 1400;
	constexpr int32 ObliquePictureWidth = 1400;
	constexpr int32 ObliquePictureHeight = 900;
	constexpr int32 SurveyPictureSize = 512;
	constexpr double TopViewMargin = 7000.0;
	// A narrow perspective instead of orthographic: the landscape loses whole sections in an orthographic capture.
	constexpr float TopFieldOfView = 16.f;
	constexpr float ShotSeconds = 1.25f;
	constexpr double SurveyMaxComponentDistance = 12000.0;
	constexpr double ObliqueDistanceFactor = 2.2;
	constexpr double ObliquePitchDegrees = 52.0;
	constexpr float ObliqueFieldOfView = 40.f;
	constexpr double SurveyMinWidth = 1200.0;
	constexpr double SurveyWidthFactor = 2.8;
	constexpr double SurveyObliqueDistanceFactor = 4.0;
	constexpr double NavProjectExtent = 900.0;
	constexpr double NavProjectExtentZ = 1200.0;
	constexpr double PathStartMargin = 7000.0;
	// Path tests start this far outside the main gate, on its approach road.
	constexpr double PathStartBeyondGate = 2500.0;
	constexpr int32 NavAgentCount = 4;
	constexpr float NavAgentRadii[NavAgentCount] = {35.f, 120.f, 160.f, 200.f};
	constexpr float NavAgentHeights[NavAgentCount] = {144.f, 180.f, 250.f, 300.f};
	const TCHAR* const NavAgentNames[NavAgentCount] = {
		TEXT("Character"), TEXT("LightTank"), TEXT("MediumTank"), TEXT("HeavyTank")
	};
}

/** @return Every planned structure's class, role and position plus the base's radius and fallbacks (footprint measurements, round 3). */
static FString SovietPreview_BuildExtentJson(const USurvivalSovietBaseComponent& SovietBases, const FSurvivalSovietBaseState& Base,
                                             const FString& Label)
{
	TArray<FString> Rows;
	for (const FSurvivalSovietPlannedStructure& Structure : Base.Plan.Structures)
	{
		Rows.Add(FString::Printf(TEXT("[\"%s\",%d,%.0f,%.0f,%.3f,%.3f]"), *SovietBases.GetClassNameForPreview(Structure.ClassIndex),
		                         static_cast<int32>(Structure.Role), Structure.Position.X, Structure.Position.Y, Structure.StretchXY.X,
		                         Structure.StretchXY.Y));
	}
	const float Radius = Base.StartLocation.IsValid() ? SovietBases.GetBaseBuildRadius(*Base.StartLocation) : 0.f;
	const float StartRadius = Base.StartLocation.IsValid() ? Base.StartLocation->GetBuildAreaRadius() : 0.f;
	return FString::Printf(
		TEXT("{\"label\":\"%s\",\"center\":[%.0f,%.0f],\"build_radius\":%.0f,\"start_location_radius\":%.0f,\"dropped_slots\":%d,")
		TEXT("\"open_wall_slots\":%d,\"rule_violations\":%d,\"structures\":[%s]}"),
		*Label, Base.Center.X, Base.Center.Y, Radius, StartRadius, Base.Plan.DroppedSlots.Num(), Base.Plan.OpenWallSlots.Num(),
		Base.Plan.RuleViolations.Num(), *FString::Join(Rows, TEXT(",")));
}

/** @brief The tool's state machine; kept alive by its own timer lambda until it asks the process to exit. */
class FSovietBasePreviewRunner : public TSharedFromThis<FSovietBasePreviewRunner>
{
public:
	FSovietBasePreviewRunner(ASurvivalModeDirector* InDirector, USurvivalSovietBaseComponent* InSovietBases);

	void Start();

private:
	enum class EStep : uint8
	{
		WarmUp,
		StartBatch,
		WaitForBases,
		SettleBases,
		QueueBaseShots,
		ReportBases,
		RunFights,
		StartSurveyClass,
		SettleSurveyClass,
		SurveyShots,
		FinishSurveyClass,
		Finished
	};

	/** @brief One picture: the capture runs a moment in that pose so exposure and anti aliasing settle. */
	struct FCaptureShot
	{
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		float FieldOfView = 90.f;
		UTextureRenderTarget2D* Target = nullptr;
		FString FileName;
	};

	/** @brief A fixed attacking force against one base. */
	struct FBaseFight
	{
		int32 BaseArrayIndex = INDEX_NONE;
		TArray<TWeakObjectPtr<AActor>> Attackers;
		TSharedPtr<int32> AttackersDead = MakeShared<int32>(0);
		float Seconds = 0.f;
		float NextOrderSeconds = 0.f;
		FString Outcome;
	};

	struct FPreviewEntry
	{
		int32 DistanceRank = 0;
		FString Label;
		FName PresetName = NAME_None;
		int32 Seed = 0;
		int32 Strength = 0;
		ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
		// Test only: the build radius this base is generated with (0 keeps the start location's own). Measures how small a
		// build area each archetype tolerates without touching the map or the settings.
		float BuildRadius = 0.f;
	};
	// The start locations' own build radii, restored after every batch that overrode them.
	TMap<TWeakObjectPtr<ASurvivalStartLocation>, float> M_OriginalBuildRadii;

	TWeakObjectPtr<ASurvivalModeDirector> M_Director;
	TWeakObjectPtr<USurvivalSovietBaseComponent> M_SovietBases;
	TStrongObjectPtr<UTextureRenderTarget2D> M_TopTarget;
	TStrongObjectPtr<UTextureRenderTarget2D> M_ObliqueTarget;
	TStrongObjectPtr<UTextureRenderTarget2D> M_SurveyTarget;
	TWeakObjectPtr<ASceneCapture2D> M_Camera;
	FTSTicker::FDelegateHandle M_TickHandle;
	FString M_Directory;
	EStep M_Step = EStep::WarmUp;
	float M_StepSeconds = 0.f;

	TArray<FPreviewEntry> M_Entries;
	int32 M_NextEntry = 0;
	TArray<FPreviewEntry> M_ActiveEntries;

	// Worst and summed frame times while the current batch generates.
	double M_WorstFrameSeconds = 0.0;
	double M_SummedFrameSeconds = 0.0;
	int32 M_FrameCount = 0;
	int32 M_LongFrames = 0;
	bool bM_MeasuringFrames = false;
	FTSTicker::FDelegateHandle M_FrameHandle;

	bool bM_FightMode = false;
	TArray<FBaseFight> M_Fights;
	FString M_FightLines;

	TArray<FCaptureShot> M_Shots;
	int32 M_NextShot = 0;
	bool bM_ShotRunning = false;

	TArray<FString> M_SurveyPaths;
	int32 M_NextSurveyPath = 0;
	TWeakObjectPtr<AActor> M_SurveyActor;
	FString M_SurveyLabel;
	FString M_MatchTablePath;
	int32 M_PlayerLocationIndex = 0;
	int32 M_TotalProblems = 0;

	UWorld* GetWorld() const;
	void Tick();
	void AdvanceTo(EStep Step);
	void WriteMatchTable();
	void Finish();

	bool LoadPreviewEntries(const FString& SpecPath);
	bool LoadSurveyPaths(const FString& ListPath);
	bool CreateCaptureRig();

	void StartBatch();
	void SetPreviewBuildRadius(ASurvivalStartLocation& Location, float Radius);
	void WriteExtentJson(const FSurvivalSovietBaseState& Base, const FPreviewEntry& Entry) const;
	bool GetAreActiveBasesReady() const;
	bool GetIsNavigationIdle() const;
	void QueueBaseShots();
	void ReportBases();
	void ReportClusters();
	void BeginFrameMeasure();
	void EndFrameMeasure();
	void StartFights();
	void StartFight(FBaseFight& Fight, const FSurvivalSovietBaseState& Base);
	void OrderAttackers(FBaseFight& Fight, const FSurvivalSovietBaseState& Base) const;
	bool TickFights();
	void FinishFight(FBaseFight& Fight, const FSurvivalSovietBaseState& Base, const FString& Outcome);
	/** @brief Runs the queued pictures one per step. @return True when every picture is written. */
	bool RunShots();
	void QueueTopShot(const FSurvivalSovietBaseState& Base, const FString& Label);
	void QueueGateShot(const FSurvivalSovietBaseState& Base, const FString& Label);
	void QueueObliqueShot(const FSurvivalSovietBaseState& Base, const FString& Label, const FVector& ThreatLocation);
	/** @brief The critic's fixed views plus low close-ups; picked from plan data, so a seed gives the same frames every round. */
	void QueueDetailShots(const FSurvivalSovietBaseState& Base, const FString& Label, const FVector& ThreatLocation);
	void QueueWallLongShot(const FSurvivalSovietBaseState& Base, const FString& Label);
	void QueueWallCornerShot(const FSurvivalSovietBaseState& Base, const FString& Label);
	void QueuePadShot(const FSurvivalSovietBaseState& Base, const FString& Label, const FVector& ThreatLocation);
	/** @brief One close view per footprint class in the base, from the door side (eye 1 200 cm out, 500 up, FOV 45). */
	void QueueFootprintCloseShots(const FSurvivalSovietBaseState& Base, const FString& Label);
	void QueueJunctionShots(const FSurvivalSovietBaseState& Base, const FString& Label, const FVector2D& Threat);
	void QueueBunkerShot(const FSurvivalSovietBaseState& Base, const FString& Label);
	void QueueTowerShot(const FSurvivalSovietBaseState& Base, const FString& Label);
	void QueueLowCloseUps(const FSurvivalSovietBaseState& Base, const FString& Label);
	/** @brief Pictures 12 and 13: the fullest ammunition yard (else the largest fuel yard) close and from above. */
	void QueueYardShots(const FSurvivalSovietBaseState& Base, const FString& Label);
	/** @brief Each open wall slot from outside, and the first flag piece with its star towards the camera. */
	void QueueWallDetailShots(const FSurvivalSovietBaseState& Base, const FString& Label);
	void AddTopShot(const FVector& Target, double Height, float FieldOfView, const FString& FileName);
	void AddLookShot(const FVector& Eye, const FVector& LookAt, float FieldOfView, const FString& FileName);
	/** @brief The RTS camera's view: fixed distance and pitch, looking at Target from FromDirection. */
	void AddRtsShot(const FVector& Target, const FVector2D& FromDirection, const FString& FileName);
	static bool GetIsOnRoad(const FVector2D& Point, const FSurvivalSovietPlannedRoad& Road);
	static bool FindJunction(const TArray<FSurvivalSovietPlannedRoad>& Roads, FVector2D& OutJunction);
	static bool GetIsInsideLine(const FVector2D& Point, const TArray<FVector2D>& Line);
	static FVector2D GetEdgeOutward(const FVector2D& A, const FVector2D& B, const FVector2D& Center);
	void WritePlanJson(const FSurvivalSovietBaseState& Base, const FPreviewEntry& Entry) const;
	void ReportCounts(const FSurvivalSovietBaseState& Base, const FPreviewEntry& Entry);
	void ReportPaths(const FSurvivalSovietBaseState& Base, const FPreviewEntry& Entry, const FVector& ThreatLocation);
	bool TestPath(int32 AgentIndex, const FVector& From, const FVector& To) const;

	void StartSurveyClass();
	void QueueSurveyShots();
	void FinishSurveyClass();
};

FSovietBasePreviewRunner::FSovietBasePreviewRunner(
	ASurvivalModeDirector* InDirector,
	USurvivalSovietBaseComponent* InSovietBases)
	: M_Director(InDirector)
	  , M_SovietBases(InSovietBases)
{
}

UWorld* FSovietBasePreviewRunner::GetWorld() const
{
	return M_Director.IsValid() ? M_Director->GetWorld() : nullptr;
}

void FSovietBasePreviewRunner::Start()
{
	FString SpecPath;
	FString ListPath;
	FParse::Value(FCommandLine::Get(), TEXT("SovietBasePreviewDir="), M_Directory);
	FParse::Value(FCommandLine::Get(), TEXT("SovietBasePreview="), SpecPath);
	if (FParse::Value(FCommandLine::Get(), TEXT("SovietBaseFight="), SpecPath))
	{
		bM_FightMode = true;
	}
	FParse::Value(FCommandLine::Get(), TEXT("SovietBaseSurvey="), ListPath);
	if (M_Directory.IsEmpty())
	{
		M_Directory = FPaths::ProjectSavedDir() / TEXT("SovietBasePreview");
	}
	RTSFunctionLibrary::PushModalDialogSuppression();
	FParse::Value(FCommandLine::Get(), TEXT("SovietBaseMatchTable="), M_MatchTablePath);
	// Which start plays the player, so every start can host a base across runs.
	FParse::Value(FCommandLine::Get(), TEXT("SovietBasePreviewPlayerStart="), M_PlayerLocationIndex);
	const bool bHasWork = (not SpecPath.IsEmpty() and LoadPreviewEntries(SpecPath))
		or (not ListPath.IsEmpty() and LoadSurveyPaths(ListPath)) or not M_MatchTablePath.IsEmpty();
	if (not bHasWork or not CreateCaptureRig())
	{
		UE_LOG(LogSovietBasePreview, Error, TEXT("SOVIET_PREVIEW nothing to do (spec %s, list %s)"), *SpecPath, *ListPath);
		Finish();
		return;
	}
	// A core ticker, because the world starts paused until the player leaves the setup screens.
	TSharedRef<FSovietBasePreviewRunner> SharedThis = AsShared();
	M_TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([SharedThis](const float)
		{
			SharedThis->Tick();
			return SharedThis->M_Step != EStep::Finished;
		}), SovietBasePreviewConstants::TickSeconds);
}

bool FSovietBasePreviewRunner::LoadPreviewEntries(const FString& SpecPath)
{
	TArray<FString> Lines;
	if (not FFileHelper::LoadFileToStringArray(Lines, *SpecPath))
	{
		return false;
	}
	for (const FString& RawLine : Lines)
	{
		// Line: <label> <preset name> <seed> <strength> <difficulty number>
		TArray<FString> Words;
		RawLine.TrimStartAndEnd().ParseIntoArrayWS(Words);
		constexpr int32 RequiredWords = 5;
		if (Words.Num() < RequiredWords or Words[0].StartsWith(TEXT("#")))
		{
			continue;
		}
		FPreviewEntry& Entry = M_Entries.AddDefaulted_GetRef();
		Entry.Label = Words[0];
		Entry.PresetName = FName(*Words[1]);
		Entry.Seed = FCString::Atoi(*Words[2]);
		Entry.Strength = FCString::Atoi(*Words[3]);
		Entry.Difficulty = static_cast<ERTSGameDifficulty>(FCString::Atoi(*Words[4]));
		Entry.DistanceRank = Words.Num() > RequiredWords ? FCString::Atoi(*Words[RequiredWords]) : 0;
		constexpr int32 RadiusWord = RequiredWords + 1;
		Entry.BuildRadius = Words.Num() > RadiusWord ? FCString::Atof(*Words[RadiusWord]) : 0.f;
	}
	return not M_Entries.IsEmpty();
}

bool FSovietBasePreviewRunner::LoadSurveyPaths(const FString& ListPath)
{
	TArray<FString> Lines;
	if (not FFileHelper::LoadFileToStringArray(Lines, *ListPath))
	{
		return false;
	}
	for (const FString& RawLine : Lines)
	{
		const FString Line = RawLine.TrimStartAndEnd();
		if (not Line.IsEmpty() and not Line.StartsWith(TEXT("#")))
		{
			M_SurveyPaths.Add(Line);
		}
	}
	return not M_SurveyPaths.IsEmpty();
}

bool FSovietBasePreviewRunner::CreateCaptureRig()
{
	using namespace SovietBasePreviewConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* Camera = World->SpawnActor<ASceneCapture2D>(
		ASceneCapture2D::StaticClass(), FTransform::Identity, SpawnParameters);
	UTextureRenderTarget2D* TopTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, TopPictureSize, TopPictureSize, RTF_RGBA8_SRGB);
	UTextureRenderTarget2D* ObliqueTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, ObliquePictureWidth, ObliquePictureHeight, RTF_RGBA8_SRGB);
	UTextureRenderTarget2D* SurveyTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, SurveyPictureSize, SurveyPictureSize, RTF_RGBA8_SRGB);
	if (not IsValid(Camera) or not IsValid(TopTarget) or not IsValid(ObliqueTarget) or not IsValid(SurveyTarget))
	{
		return false;
	}
	USceneCaptureComponent2D* CaptureComponent = Camera->GetCaptureComponent2D();
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	// The fog of war is a post-process material; the pictures show the base, not what the player may see.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetVolumetricFog(false);
	CaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
	float ExposureBias = 0.f;
	FParse::Value(FCommandLine::Get(), TEXT("CaptureExposureBias="), ExposureBias);
	FPostProcessSettings& PostProcess = CaptureComponent->PostProcessSettings;
	PostProcess.bOverride_AutoExposureMethod = true;
	PostProcess.AutoExposureMethod = AEM_Manual;
	PostProcess.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PostProcess.AutoExposureApplyPhysicalCameraExposure = 0;
	PostProcess.bOverride_AutoExposureBias = true;
	PostProcess.AutoExposureBias = ExposureBias;
	PostProcess.bOverride_DynamicGlobalIlluminationMethod = true;
	PostProcess.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PostProcess.bOverride_ReflectionMethod = true;
	PostProcess.ReflectionMethod = EReflectionMethod::None;
	CaptureComponent->PostProcessBlendWeight = 1.f;
	M_Camera = Camera;
	M_TopTarget.Reset(TopTarget);
	M_ObliqueTarget.Reset(ObliqueTarget);
	M_SurveyTarget.Reset(SurveyTarget);
	return true;
}

void FSovietBasePreviewRunner::WriteMatchTable()
{
	if (M_MatchTablePath.IsEmpty())
	{
		return;
	}
	TArray<int32> MatchSeeds;
	for (int32 Seed = 1; Seed <= SovietBasePreviewConstants::MatchSeedsPerStart; ++Seed)
	{
		MatchSeeds.Add(Seed * SovietBasePreviewConstants::MatchSeedStride);
	}
	const double StartSeconds = FPlatformTime::Seconds();
	const FString Table = M_SovietBases->BuildMatchTableJson(MatchSeeds);
	FFileHelper::SaveStringToFile(Table, *M_MatchTablePath);
	UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PREVIEW match table written in %.1f s, tuning source %s"),
	       FPlatformTime::Seconds() - StartSeconds, *M_SovietBases->GetTuningSourceForPreview());
}

void FSovietBasePreviewRunner::AdvanceTo(const EStep Step)
{
	M_Step = Step;
	M_StepSeconds = 0.f;
}

void FSovietBasePreviewRunner::Tick()
{
	using namespace SovietBasePreviewConstants;
	M_StepSeconds += TickSeconds;
	if (not M_Director.IsValid() or not M_SovietBases.IsValid())
	{
		Finish();
		return;
	}
	if (GetWorld()->IsPaused())
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
	}
	switch (M_Step)
	{
	case EStep::WarmUp:
		if (M_StepSeconds >= WarmUpSeconds)
		{
			WriteMatchTable();
			AdvanceTo(M_SurveyPaths.IsEmpty() ? EStep::StartBatch : EStep::StartSurveyClass);
		}
		break;
	case EStep::StartBatch:
		StartBatch();
		break;
	case EStep::WaitForBases:
		if (GetAreActiveBasesReady() or M_StepSeconds >= BaseTimeoutSeconds)
		{
			AdvanceTo(EStep::SettleBases);
		}
		break;
	case EStep::SettleBases:
		EndFrameMeasure();
		if (M_StepSeconds >= SettleSeconds and (GetIsNavigationIdle() or M_StepSeconds >= BaseTimeoutSeconds))
		{
			AdvanceTo(EStep::QueueBaseShots);
		}
		break;
	case EStep::QueueBaseShots:
		if (M_StepSeconds >= CaptureDelaySeconds)
		{
			QueueBaseShots();
		}
		break;
	case EStep::ReportBases:
		if (RunShots())
		{
			ReportBases();
		}
		break;
	case EStep::RunFights:
		if (TickFights())
		{
			AdvanceTo(EStep::StartBatch);
		}
		break;
	case EStep::StartSurveyClass:
		StartSurveyClass();
		break;
	case EStep::SettleSurveyClass:
		if (M_StepSeconds >= TickSeconds)
		{
			QueueSurveyShots();
		}
		break;
	case EStep::SurveyShots:
		if (RunShots())
		{
			FinishSurveyClass();
		}
		break;
	case EStep::Finished:
	default:
		break;
	}
}

void FSovietBasePreviewRunner::Finish()
{
	AdvanceTo(EStep::Finished);
	UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PREVIEW finished, %d problems"), M_TotalProblems);
	FPlatformMisc::RequestExit(false, TEXT("SovietBasePreview"));
}

// ---------------------------------------------------------------------------
// Bases
// ---------------------------------------------------------------------------

void FSovietBasePreviewRunner::SetPreviewBuildRadius(ASurvivalStartLocation& Location, const float Radius)
{
	// BuildAreaRadius is a protected map property; the test sets it through reflection and restores it per batch.
	FFloatProperty* const Property = FindFProperty<FFloatProperty>(ASurvivalStartLocation::StaticClass(), TEXT("BuildAreaRadius"));
	if (Property == nullptr)
	{
		return;
	}
	float* const Value = Property->ContainerPtrToValuePtr<float>(&Location);
	if (not M_OriginalBuildRadii.Contains(&Location))
	{
		M_OriginalBuildRadii.Add(&Location, *Value);
	}
	*Value = Radius > 0.f ? Radius : M_OriginalBuildRadii.FindRef(&Location);
	UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PREVIEW build radius %.0f at %s"), *Value, *Location.GetName());
}

void FSovietBasePreviewRunner::StartBatch()
{
	const TArray<TWeakObjectPtr<ASurvivalStartLocation>>& Locations = M_Director->GetStartLocations();
	if (Locations.Num() < 2 or M_Entries.IsEmpty())
	{
		Finish();
		return;
	}
	M_SovietBases->ClearBasesForPreview();
	M_ActiveEntries.Reset();

	// One location plays the player's start: every base faces it, like in a match.
	const FVector2D ThreatLocation(Locations[M_PlayerLocationIndex]->GetDeployCenter());
	for (int32 LocationIndex = 0; LocationIndex < Locations.Num() and M_NextEntry < M_Entries.Num(); ++LocationIndex)
	{
		ASurvivalStartLocation* Location = Locations[LocationIndex].Get();
		if (not IsValid(Location) or LocationIndex == M_PlayerLocationIndex)
		{
			continue;
		}
		const FPreviewEntry& Entry = M_Entries[M_NextEntry++];
		FSurvivalSovietBasePreviewRequest Request;
		Request.PresetName = Entry.PresetName;
		Request.Strength = Entry.Strength;
		Request.Seed = Entry.Seed;
		Request.Difficulty = Entry.Difficulty;
		Request.ThreatLocation = ThreatLocation;
		Request.DistanceRank = Entry.DistanceRank;
		SetPreviewBuildRadius(*Location, Entry.BuildRadius);
		M_SovietBases->GenerateBaseForPreview(Location, M_ActiveEntries.Num(), Request);
		M_ActiveEntries.Add(Entry);
	}
	BeginFrameMeasure();
	AdvanceTo(M_ActiveEntries.IsEmpty() ? EStep::Finished : EStep::WaitForBases);
	if (M_ActiveEntries.IsEmpty())
	{
		Finish();
	}
}

bool FSovietBasePreviewRunner::GetAreActiveBasesReady() const
{
	for (const FSurvivalSovietBaseState& Base : M_SovietBases->GetBaseStatesForPreview())
	{
		if (not Base.bStructuresSpawned or Base.PendingGarrisonGroups > 0)
		{
			return false;
		}
	}
	return true;
}

bool FSovietBasePreviewRunner::GetIsNavigationIdle() const
{
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	return NavigationSystem == nullptr or not NavigationSystem->IsNavigationBuildInProgress();
}

void FSovietBasePreviewRunner::QueueBaseShots()
{
	const FVector ThreatLocation = M_Director->GetStartLocations()[M_PlayerLocationIndex]->GetDeployCenter();
	const TArray<FSurvivalSovietBaseState>& Bases = M_SovietBases->GetBaseStatesForPreview();
	for (int32 BaseIndex = 0; BaseIndex < Bases.Num() and BaseIndex < M_ActiveEntries.Num(); ++BaseIndex)
	{
		QueueTopShot(Bases[BaseIndex], M_ActiveEntries[BaseIndex].Label);
		if (not FParse::Param(FCommandLine::Get(), TEXT("SovietBasePreviewTopOnly")))
		{
			QueueObliqueShot(Bases[BaseIndex], M_ActiveEntries[BaseIndex].Label, ThreatLocation);
			QueueGateShot(Bases[BaseIndex], M_ActiveEntries[BaseIndex].Label);
			QueueDetailShots(Bases[BaseIndex], M_ActiveEntries[BaseIndex].Label, ThreatLocation);
		}
	}
	AdvanceTo(EStep::ReportBases);
}

void FSovietBasePreviewRunner::ReportBases()
{
	const FVector ThreatLocation = M_Director->GetStartLocations()[M_PlayerLocationIndex]->GetDeployCenter();
	const TArray<FSurvivalSovietBaseState>& Bases = M_SovietBases->GetBaseStatesForPreview();
	for (int32 BaseIndex = 0; BaseIndex < Bases.Num() and BaseIndex < M_ActiveEntries.Num(); ++BaseIndex)
	{
		WritePlanJson(Bases[BaseIndex], M_ActiveEntries[BaseIndex]);
		ReportCounts(Bases[BaseIndex], M_ActiveEntries[BaseIndex]);
		ReportPaths(Bases[BaseIndex], M_ActiveEntries[BaseIndex], ThreatLocation);
	}
	ReportClusters();
	if (bM_FightMode)
	{
		StartFights();
		return;
	}
	if (M_NextEntry >= M_Entries.Num())
	{
		Finish();
		return;
	}
	AdvanceTo(EStep::StartBatch);
}

void FSovietBasePreviewRunner::BeginFrameMeasure()
{
	M_WorstFrameSeconds = 0.0;
	M_SummedFrameSeconds = 0.0;
	M_FrameCount = 0;
	M_LongFrames = 0;
	bM_MeasuringFrames = true;
	TSharedRef<FSovietBasePreviewRunner> SharedThis = AsShared();
	M_FrameHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([SharedThis](const float DeltaSeconds)
	{
		SharedThis->M_WorstFrameSeconds = FMath::Max<double>(SharedThis->M_WorstFrameSeconds, DeltaSeconds);
		SharedThis->M_SummedFrameSeconds += DeltaSeconds;
		SharedThis->M_FrameCount++;
		SharedThis->M_LongFrames += DeltaSeconds > SovietBasePreviewConstants::LongFrameSeconds ? 1 : 0;
		return SharedThis->bM_MeasuringFrames;
	}));
}

void FSovietBasePreviewRunner::EndFrameMeasure()
{
	if (not bM_MeasuringFrames)
	{
		return;
	}
	bM_MeasuringFrames = false;
	UE_LOG(LogSovietBasePreview, Display,
	       TEXT("SOVIET_FRAMES batch of %d bases: %d frames, mean %.1f ms, worst %.1f ms, over 100 ms %d"),
	       M_ActiveEntries.Num(), M_FrameCount, M_FrameCount > 0 ? 1000.0 * M_SummedFrameSeconds / M_FrameCount : 0.0,
	       1000.0 * M_WorstFrameSeconds, M_LongFrames);
}

void FSovietBasePreviewRunner::ReportClusters()
{
	const FFindEnemyBaseClusters* Request = M_SovietBases->GetClusterRequestForPreview();
	if (Request == nullptr)
	{
		UE_LOG(LogSovietBasePreview, Warning, TEXT("SOVIET_CLUSTER no enemy controller, no request"));
		return;
	}
	const TArray<FSurvivalSovietBaseState>& Bases = M_SovietBases->GetBaseStatesForPreview();
	const FResultEnemyBaseClusters Result = FStrategicAIHelpers::BuildEnemyBaseClustersResult(
		*Request, M_SovietBases->BuildBuildingSnapshotForPreview());
	for (int32 BaseIndex = 0; BaseIndex < Bases.Num() and BaseIndex < M_ActiveEntries.Num(); ++BaseIndex)
	{
		int32 Core = 0;
		for (const FEnemyBasePointCoreBuildings& Point : Result.BasePoints)
		{
			const bool bThisBase = FVector::Dist2D(Point.BaseLocation, Bases[BaseIndex].Center)
				<= M_SovietBases->GetBaseBuildRadius(*Bases[BaseIndex].StartLocation);
			Core += bThisBase ? Point.CoreBuildingLocations.Num() : 0;
		}
		UE_LOG(LogSovietBasePreview, Display,
		       TEXT("SOVIET_CLUSTER %s: production %d, core buildings in cluster %d, %s (request distance %.0f, chain %.0f, clusters %d)"),
		       *M_ActiveEntries[BaseIndex].Label, Bases[BaseIndex].Plan.ProductionCount, Core,
		       Core > 0 ? TEXT("FOUND") : TEXT("NOT FOUND"), Request->CoreClusterDistanceXY,
		       Bases[BaseIndex].Plan.RequiredCoreClusterDistance, Result.BasePoints.Num());
	}
}

void FSovietBasePreviewRunner::StartFight(FBaseFight& Fight, const FSurvivalSovietBaseState& Base)
{
	using namespace SovietBasePreviewConstants;
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(M_SovietBases.Get());
	if (not IsValid(Spawner))
	{
		return;
	}
	TSharedRef<FSovietBasePreviewRunner> SharedThis = AsShared();
	const int32 BaseArrayIndex = Fight.BaseArrayIndex;
	const FVector Outward = (Base.LaunchPoint - Base.Center).GetSafeNormal2D();
	const FVector Side(-Outward.Y, Outward.X, 0.0);
	for (int32 Tank = 0; Tank < FightTanks; ++Tank)
	{
		const FVector Location = Base.LaunchPoint + Outward * FightStandOff + Side * ((Tank - FightTanks / 2) * FightTankSpacing)
			+ FVector::UpVector * FightSpawnHeight;
		const FTrainingOption Option(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzIV_H));
		Spawner->AsyncSpawnOptionAtLocation(
			Option, Location, M_SovietBases.Get(), Tank,
			[SharedThis, BaseArrayIndex](const FTrainingOption&, AActor* Spawned, const int32)
			{
				for (FBaseFight& Candidate : SharedThis->M_Fights)
				{
					if (Candidate.BaseArrayIndex != BaseArrayIndex or not IsValid(Spawned))
					{
						continue;
					}
					Candidate.Attackers.Add(Spawned);
					TSharedPtr<int32> Dead = Candidate.AttackersDead;
					if (IHealthBarOwner* Health = Cast<IHealthBarOwner>(Spawned))
					{
						if (FOnUnitDies* OnDies = Health->GetOnUnitDiesDelegate())
						{
							OnDies->AddLambda([Dead]() { ++(*Dead); });
						}
					}
				}
			},
			(-Outward).Rotation());
	}
}

void FSovietBasePreviewRunner::StartFights()
{
	M_Fights.Reset();
	const TArray<FSurvivalSovietBaseState>& Bases = M_SovietBases->GetBaseStatesForPreview();
	for (int32 BaseIndex = 0; BaseIndex < Bases.Num(); ++BaseIndex)
	{
		M_Fights.AddDefaulted_GetRef().BaseArrayIndex = BaseIndex;
	}
	for (FBaseFight& Fight : M_Fights)
	{
		StartFight(Fight, Bases[Fight.BaseArrayIndex]);
	}
	AdvanceTo(EStep::RunFights);
}

void FSovietBasePreviewRunner::OrderAttackers(FBaseFight& Fight, const FSurvivalSovietBaseState& Base) const
{
	// Attack-move to the nearest defence that still stands, then production: a plain assault, no micro.
	const FVector From = Fight.Attackers.IsEmpty() or not Fight.Attackers[0].IsValid()
		                     ? Base.LaunchPoint
		                     : Fight.Attackers[0]->GetActorLocation();
	const FVector Target = M_SovietBases->GetNearestStandingStructureForPreview(Base, From);
	for (const TWeakObjectPtr<AActor>& Attacker : Fight.Attackers)
	{
		if (ICommands* Commands = Cast<ICommands>(Attacker.Get()))
		{
			Commands->AttackMoveToLocation(Target, true, FRotator::ZeroRotator);
		}
	}
}

void FSovietBasePreviewRunner::FinishFight(FBaseFight& Fight, const FSurvivalSovietBaseState& Base, const FString& Outcome)
{
	Fight.Outcome = Outcome;
	const FPreviewEntry& Entry = M_ActiveEntries[Fight.BaseArrayIndex];
	const FString Line = FString::Printf(
		TEXT("{\"label\":\"%s\",\"outcome\":\"%s\",\"seconds\":%.0f,\"attackers\":%d,\"attackers_lost\":%d,")
		TEXT("\"budget\":%d,\"threat\":%d,\"defense_points\":%d,\"defense_destroyed\":%d,\"production_left\":%d,")
		TEXT("\"garrison_left\":%d,\"garrison_total\":%d}\n"),
		*Entry.Label, *Outcome, Fight.Seconds, Fight.Attackers.Num(), *Fight.AttackersDead, Base.Plan.ThreatBudget,
		Base.Plan.ThreatPoints, Base.DefensePointsTotal, Base.DefensePointsDestroyed, Base.TrackedStructuresAlive,
		M_SovietBases->CountLiveGarrisonForPreview(Base), Base.GarrisonUnits.Num());
	M_FightLines += Line;
	UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_FIGHT %s"), *Line.TrimEnd());
}

bool FSovietBasePreviewRunner::TickFights()
{
	using namespace SovietBasePreviewConstants;
	const TArray<FSurvivalSovietBaseState>& Bases = M_SovietBases->GetBaseStatesForPreview();
	bool bAllDone = true;
	for (FBaseFight& Fight : M_Fights)
	{
		if (not Fight.Outcome.IsEmpty())
		{
			continue;
		}
		const FSurvivalSovietBaseState& Base = Bases[Fight.BaseArrayIndex];
		Fight.Seconds += TickSeconds;
		bAllDone = false;
		const bool bAllSpawned = Fight.Attackers.Num() == FightTanks;
		if (bAllSpawned and *Fight.AttackersDead >= FightTanks)
		{
			FinishFight(Fight, Base, TEXT("force destroyed"));
			continue;
		}
		const bool bDefenceBroken = Base.DefensePointsDestroyed * 2 >= Base.DefensePointsTotal;
		if (Base.bDestroyed and bDefenceBroken)
		{
			FinishFight(Fight, Base, TEXT("base taken"));
			continue;
		}
		if (Fight.Seconds >= FightTimeLimitSeconds)
		{
			FinishFight(Fight, Base, TEXT("time limit"));
			continue;
		}
		if (bAllSpawned and Fight.Seconds >= Fight.NextOrderSeconds)
		{
			Fight.NextOrderSeconds = Fight.Seconds + FightOrderSeconds;
			OrderAttackers(Fight, Base);
		}
	}
	if (not bAllDone)
	{
		return false;
	}
	FFileHelper::SaveStringToFile(M_FightLines, *(M_Directory / TEXT("fights.jsonl")));
	if (M_NextEntry >= M_Entries.Num())
	{
		Finish();
	}
	return true;
}

bool FSovietBasePreviewRunner::RunShots()
{
	using namespace SovietBasePreviewConstants;
	USceneCaptureComponent2D* CaptureComponent = M_Camera->GetCaptureComponent2D();
	if (bM_ShotRunning)
	{
		if (M_StepSeconds < (M_SurveyPaths.IsEmpty() ? ShotSeconds : ShotSeconds * 0.5f))
		{
			return false;
		}
		const FCaptureShot& Finished = M_Shots[M_NextShot++];
		CaptureComponent->bCaptureEveryFrame = false;
		FlushRenderingCommands();
		UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), Finished.Target, M_Directory, Finished.FileName);
		bM_ShotRunning = false;
	}
	if (not M_Shots.IsValidIndex(M_NextShot))
	{
		M_Shots.Reset();
		M_NextShot = 0;
		return true;
	}
	const FCaptureShot& Shot = M_Shots[M_NextShot];
	M_Camera->SetActorLocationAndRotation(Shot.Location, Shot.Rotation);
	CaptureComponent->FOVAngle = Shot.FieldOfView;
	CaptureComponent->TextureTarget = Shot.Target;
	CaptureComponent->bCaptureEveryFrame = true;
	bM_ShotRunning = true;
	M_StepSeconds = 0.f;
	return false;
}

void FSovietBasePreviewRunner::QueueTopShot(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	const double ViewWidth = 2.0 * (M_SovietBases->GetBaseBuildRadius(*Base.StartLocation) + TopViewMargin);
	const double Height = ViewWidth * 0.5 / FMath::Tan(FMath::DegreesToRadians(TopFieldOfView * 0.5));
	FCaptureShot& Shot = M_Shots.AddDefaulted_GetRef();
	Shot.Location = Base.Center + FVector(0.0, 0.0, Height);
	Shot.Rotation = FRotator(-90.f, 0.f, 0.f);
	Shot.FieldOfView = TopFieldOfView;
	Shot.Target = M_TopTarget.Get();
	Shot.FileName = Label + TEXT("_top.png");
}

void FSovietBasePreviewRunner::QueueObliqueShot(
	const FSurvivalSovietBaseState& Base,
	const FString& Label,
	const FVector& ThreatLocation)
{
	using namespace SovietBasePreviewConstants;
	const double Distance = M_SovietBases->GetBaseBuildRadius(*Base.StartLocation) * ObliqueDistanceFactor;
	const double PitchRadians = FMath::DegreesToRadians(ObliquePitchDegrees);
	// Seen from the player's side, like an attacker looks at it.
	const FVector Toward = (ThreatLocation - Base.Center).GetSafeNormal2D();
	FCaptureShot& Shot = M_Shots.AddDefaulted_GetRef();
	Shot.Location = Base.Center + Toward * (Distance * FMath::Cos(PitchRadians))
		+ FVector::UpVector * (Distance * FMath::Sin(PitchRadians));
	Shot.Rotation = (Base.Center - Shot.Location).Rotation();
	Shot.FieldOfView = ObliqueFieldOfView;
	Shot.Target = M_ObliqueTarget.Get();
	Shot.FileName = Label + TEXT("_oblique.png");
}

void FSovietBasePreviewRunner::QueueGateShot(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	// Close on the main gate from outside: walls, joins and pads at the size a player sees them.
	const FVector2D Center(Base.Center);
	const FVector2D Launch(Base.Plan.LaunchPoint);
	const FVector2D Outward = (Launch - Center).GetSafeNormal();
	const FVector2D Side(-Outward.Y, Outward.X);
	const FVector2D Gate = Launch - Outward * GateShotGateFromLaunch;
	const FVector2D Eye = Gate + Outward * GateShotBack + Side * GateShotSide;
	FCaptureShot& Shot = M_Shots.AddDefaulted_GetRef();
	Shot.Location = FVector(Eye, Base.Center.Z + GateShotHeight);
	Shot.Rotation = (FVector(Gate - Outward * GateShotLookPast, Base.Center.Z) - Shot.Location).Rotation();
	Shot.FieldOfView = GateShotFieldOfView;
	Shot.Target = M_ObliqueTarget.Get();
	Shot.FileName = Label + TEXT("_gate.png");
}

void FSovietBasePreviewRunner::AddLookShot(const FVector& Eye, const FVector& LookAt, const float FieldOfView,
                                           const FString& FileName)
{
	FCaptureShot& Shot = M_Shots.AddDefaulted_GetRef();
	Shot.Location = Eye;
	Shot.Rotation = (LookAt - Eye).Rotation();
	Shot.FieldOfView = FieldOfView;
	Shot.Target = M_ObliqueTarget.Get();
	Shot.FileName = FileName;
}

void FSovietBasePreviewRunner::AddRtsShot(const FVector& Target, const FVector2D& FromDirection, const FString& FileName)
{
	using namespace SovietBasePreviewConstants;
	const double Pitch = FMath::DegreesToRadians(RtsPitchDegrees);
	const FVector2D From = FromDirection.IsNearlyZero() ? FVector2D(1.0, 0.0) : FromDirection.GetSafeNormal();
	const FVector Eye = Target + FVector(From * (RtsDistance * FMath::Cos(Pitch)), RtsDistance * FMath::Sin(Pitch));
	AddLookShot(Eye, Target, RtsFieldOfView, FileName);
}

bool FSovietBasePreviewRunner::GetIsInsideLine(const FVector2D& Point, const TArray<FVector2D>& Line)
{
	bool bInside = false;
	for (int32 Index = 0, Previous = Line.Num() - 1; Index < Line.Num(); Previous = Index++)
	{
		const FVector2D& A = Line[Index];
		const FVector2D& B = Line[Previous];
		const bool bCrosses = (A.Y > Point.Y) != (B.Y > Point.Y) and Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X;
		bInside = bCrosses ? not bInside : bInside;
	}
	return bInside;
}

FVector2D FSovietBasePreviewRunner::GetEdgeOutward(const FVector2D& A, const FVector2D& B, const FVector2D& Center)
{
	const FVector2D Along = (B - A).GetSafeNormal();
	const FVector2D Out(-Along.Y, Along.X);
	return FVector2D::DotProduct(Out, (A + B) * 0.5 - Center) < 0.0 ? -Out : Out;
}

void FSovietBasePreviewRunner::QueueDetailShots(const FSurvivalSovietBaseState& Base, const FString& Label,
                                                const FVector& ThreatLocation)
{
	using namespace SovietBasePreviewConstants;
	const FVector2D Threat = FVector2D(ThreatLocation - Base.Center).GetSafeNormal();
	// The critic's fixed views (critic_rubric.md, pictures 2 to 11), then low close-ups for joints and flicker.
	AddRtsShot(Base.Center, Threat, Label + TEXT("_rts_center.png"));
	if (not Base.Plan.Gates.IsEmpty())
	{
		const FSurvivalSovietPlannedGate& Gate = Base.Plan.Gates[0];
		const FVector GateCenter(Gate.Center, Base.Center.Z);
		AddRtsShot(GateCenter, Gate.Outward, Label + TEXT("_rts_gate.png"));
		AddLookShot(GateCenter + FVector(Gate.Outward * GateFrontBack, GateFrontHeight), GateCenter + FVector(0.0, 0.0, CloseLookHeight),
		            CloseFieldOfView, Label + TEXT("_gate_front.png"));
	}
	QueueWallLongShot(Base, Label);
	QueueWallCornerShot(Base, Label);
	QueuePadShot(Base, Label, ThreatLocation);
	QueueFootprintCloseShots(Base, Label);
	QueueJunctionShots(Base, Label, Threat);
	QueueBunkerShot(Base, Label);
	QueueTowerShot(Base, Label);
	QueueYardShots(Base, Label);
	QueueWallDetailShots(Base, Label);
	QueueLowCloseUps(Base, Label);
}

void FSovietBasePreviewRunner::QueueWallLongShot(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	const TArray<FVector2D>& Line = Base.Plan.PerimeterLine;
	const FVector2D Center(Base.Center);
	int32 Longest = INDEX_NONE;
	for (int32 Index = 0; Index < Line.Num(); ++Index)
	{
		const double Length = FVector2D::Distance(Line[Index], Line[(Index + 1) % Line.Num()]);
		if (Longest == INDEX_NONE or Length > FVector2D::Distance(Line[Longest], Line[(Longest + 1) % Line.Num()]))
		{
			Longest = Index;
		}
	}
	if (Line.Num() < 3 or Longest == INDEX_NONE)
	{
		return;
	}
	const FVector2D A = Line[Longest];
	const FVector2D B = Line[(Longest + 1) % Line.Num()];
	const FVector2D Middle = (A + B) * 0.5;
	const FVector2D Out = GetEdgeOutward(A, B, Center);
	AddLookShot(FVector(Middle + Out * WallLongBack, Base.Center.Z + WallLongHeight), FVector(Middle, Base.Center.Z),
	            WallLongFieldOfView, Label + TEXT("_wall_long.png"));
}

void FSovietBasePreviewRunner::QueueWallCornerShot(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	const TArray<FVector2D>& Line = Base.Plan.PerimeterLine;
	if (Line.Num() < 3)
	{
		return;
	}
	const FVector2D Center(Base.Center);
	const FVector2D GateDirection = Base.Plan.Gates.IsEmpty()
		                                ? FVector2D(1.0, 0.0)
		                                : (Base.Plan.Gates[0].Center - Center).GetSafeNormal();
	const double GateYaw = FMath::Atan2(GateDirection.Y, GateDirection.X);
	// Clockwise in the top picture (+X up, +Y right) is a growing yaw: the first vertex past the main gate.
	int32 Pick = 0;
	double BestTurn = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Line.Num(); ++Index)
	{
		const FVector2D Offset = Line[Index] - Center;
		double Turn = FMath::Atan2(Offset.Y, Offset.X) - GateYaw;
		Turn = FMath::Fmod(Turn + 4.0 * PI, 2.0 * PI);
		if (Turn > KINDA_SMALL_NUMBER and Turn < BestTurn)
		{
			BestTurn = Turn;
			Pick = Index;
		}
	}
	const FVector2D Corner = Line[Pick];
	const FVector2D Before = Line[(Pick + Line.Num() - 1) % Line.Num()];
	const FVector2D After = Line[(Pick + 1) % Line.Num()];
	FVector2D Bisector = GetEdgeOutward(Before, Corner, Center) + GetEdgeOutward(Corner, After, Center);
	Bisector = Bisector.IsNearlyZero() ? (Corner - Center).GetSafeNormal() : Bisector.GetSafeNormal();
	AddLookShot(FVector(Corner + Bisector * WallCornerBack, Base.Center.Z + WallCornerHeight), FVector(Corner, Base.Center.Z),
	            CloseFieldOfView, Label + TEXT("_wall_corner.png"));
}

void FSovietBasePreviewRunner::QueueFootprintCloseShots(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	TSet<FString> Done;
	for (const FSurvivalSovietFootprintRecord& Record : Base.FootprintRecords)
	{
		if (Done.Contains(Record.Style) or Record.DoorNormal.IsNearlyZero())
		{
			continue;
		}
		Done.Add(Record.Style);
		// The door side: texture, edge transition, apron start and seating in one view; the eye stands 1 200 cm out from the
		// door wall (not the centre, which put it inside large buildings).
		const double Yaw = FMath::DegreesToRadians(Record.YawDegrees);
		const FVector2D Axis(FMath::Cos(Yaw), FMath::Sin(Yaw));
		const FVector2D AxisSide(-Axis.Y, Axis.X);
		const double Reach = FMath::Abs(FVector2D::DotProduct(Record.DoorNormal, Axis)) * Record.FootprintHalf.X
			+ FMath::Abs(FVector2D::DotProduct(Record.DoorNormal, AxisSide)) * Record.FootprintHalf.Y;
		const FVector2D Side(-Record.DoorNormal.Y, Record.DoorNormal.X);
		const FVector2D DoorEdge = Record.Center + Record.DoorNormal * Reach;
		const FVector2D EyeXY = DoorEdge + Record.DoorNormal * FootprintCloseDistance + Side * FootprintCloseSideOffset;
		AddLookShot(FVector(EyeXY, Base.Center.Z + FootprintCloseHeight), FVector(DoorEdge + Record.DoorNormal * FootprintCloseLookAhead, Base.Center.Z),
		            FootprintCloseFieldOfView, FString::Printf(TEXT("%s_close_%s.png"), *Label, *Record.Style));
	}
}

void FSovietBasePreviewRunner::QueuePadShot(const FSurvivalSovietBaseState& Base, const FString& Label,
                                            const FVector& ThreatLocation)
{
	using namespace SovietBasePreviewConstants;
	const FVector2D Center(Base.Center);
	const FSurvivalSovietPlannedStructure* Building = nullptr;
	for (const FSurvivalSovietPlannedStructure& Structure : Base.Plan.Structures)
	{
		const bool bCloser = Building == nullptr
			or FVector2D::Distance(Structure.Position, Center) < FVector2D::Distance(Building->Position, Center);
		if (Structure.Role == ESurvivalSovietStructureRole::Production and bCloser)
		{
			Building = &Structure;
		}
	}
	if (Building == nullptr)
	{
		return;
	}
	// The pad's centre is the footprint centre; a Blueprint's pivot may sit off it.
	FVector2D Target = Building->Position;
	double BestDistance = TNumericLimits<double>::Max();
	double HalfDiagonal = 0.0;
	for (const FSurvivalSovietPlannedPad& Pad : Base.Plan.Pads)
	{
		const double Distance = FVector2D::Distance(Pad.Center, Building->Position);
		if (Distance < BestDistance and Distance < FMath::Max(Pad.FootprintHalfExtents.X, Pad.FootprintHalfExtents.Y))
		{
			BestDistance = Distance;
			Target = Pad.Center;
			HalfDiagonal = Pad.FootprintHalfExtents.Size();
		}
	}
	const FVector2D Threat = FVector2D(ThreatLocation - Base.Center).GetSafeNormal();
	const double Back = PadShotDistance + PadShotFootprintFactor * HalfDiagonal;
	const double Up = PadShotHeight * Back / PadShotDistance;
	AddLookShot(FVector(Target + Threat * Back, Base.Center.Z + Up), FVector(Target, Base.Center.Z), CloseFieldOfView,
	            Label + TEXT("_pad.png"));
}

bool FSovietBasePreviewRunner::GetIsOnRoad(const FVector2D& Point, const FSurvivalSovietPlannedRoad& Road)
{
	using namespace SovietBasePreviewConstants;
	for (int32 Index = 0; Index + 1 < Road.Points.Num(); ++Index)
	{
		const FVector2D Along = (Road.Points[Index + 1] - Road.Points[Index]).GetSafeNormal();
		const double Reach = FMath::Clamp(FVector2D::DotProduct(Point - Road.Points[Index], Along), 0.0,
		                                  FVector2D::Distance(Road.Points[Index], Road.Points[Index + 1]));
		if (FVector2D::Distance(Point, Road.Points[Index] + Along * Reach) < Road.Width * 0.5 + JunctionTolerance)
		{
			return true;
		}
	}
	return false;
}

bool FSovietBasePreviewRunner::FindJunction(const TArray<FSurvivalSovietPlannedRoad>& Roads, FVector2D& OutJunction)
{
	// First point where a lane meets the main street, in plan order.
	for (const FSurvivalSovietPlannedRoad& Lane : Roads)
	{
		for (const FVector2D& Point : Lane.Points)
		{
			for (const FSurvivalSovietPlannedRoad& Main : Roads)
			{
				if (Lane.Kind == ESurvivalSovietRoadKind::Lane and Main.Kind == ESurvivalSovietRoadKind::Main and GetIsOnRoad(Point, Main))
				{
					OutJunction = Point;
					return true;
				}
			}
		}
	}
	// Else the first street point another street runs into.
	for (int32 RoadIndex = 0; RoadIndex < Roads.Num(); ++RoadIndex)
	{
		for (const FVector2D& Point : Roads[RoadIndex].Points)
		{
			for (int32 OtherIndex = 0; OtherIndex < Roads.Num(); ++OtherIndex)
			{
				if (OtherIndex != RoadIndex and GetIsOnRoad(Point, Roads[OtherIndex]))
				{
					OutJunction = Point;
					return true;
				}
			}
		}
	}
	return false;
}

void FSovietBasePreviewRunner::QueueJunctionShots(const FSurvivalSovietBaseState& Base, const FString& Label, const FVector2D& Threat)
{
	using namespace SovietBasePreviewConstants;
	FVector2D Junction;
	if (not FindJunction(Base.Plan.Roads, Junction))
	{
		return;
	}
	const FVector Target(Junction, Base.Center.Z);
	FCaptureShot& Shot = M_Shots.AddDefaulted_GetRef();
	Shot.Location = Target + FVector(0.0, 0.0, JunctionTopHeight);
	Shot.Rotation = FRotator(-90.f, 0.f, 0.f);
	Shot.FieldOfView = JunctionTopFieldOfView;
	Shot.Target = M_ObliqueTarget.Get();
	Shot.FileName = Label + TEXT("_junction.png");
	AddRtsShot(Target, Threat, Label + TEXT("_junction_rts.png"));
}

void FSovietBasePreviewRunner::QueueBunkerShot(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	const TArray<FVector2D>& Line = Base.Plan.PerimeterLine;
	if (Line.Num() < 3)
	{
		return;
	}
	const FVector2D Center(Base.Center);
	const FVector2D Gate = Base.Plan.Gates.IsEmpty() ? Center : Base.Plan.Gates[0].Center;
	const FSurvivalSovietPlannedStructure* Pick = nullptr;
	double PickDistance = TNumericLimits<double>::Max();
	FVector2D PickOutward(1.0, 0.0);
	for (const FSurvivalSovietPlannedStructure& Structure : Base.Plan.Structures)
	{
		if (Structure.Role != ESurvivalSovietStructureRole::PerimeterBunker)
		{
			continue;
		}
		for (int32 Index = 0; Index < Line.Num(); ++Index)
		{
			const FVector2D A = Line[Index];
			const FVector2D B = Line[(Index + 1) % Line.Num()];
			const FVector2D Along = (B - A).GetSafeNormal();
			const double Reach = FMath::Clamp(FVector2D::DotProduct(Structure.Position - A, Along), 0.0, FVector2D::Distance(A, B));
			// Ties go to the bunker nearest the main gate.
			const double Distance = FVector2D::Distance(Structure.Position, A + Along * Reach)
				+ FVector2D::Distance(Structure.Position, Gate) * BunkerTieWeight;
			if (Distance < PickDistance)
			{
				PickDistance = Distance;
				Pick = &Structure;
				PickOutward = GetEdgeOutward(A, B, Center);
			}
		}
	}
	if (Pick == nullptr)
	{
		return;
	}
	const FVector2D Side(-PickOutward.Y, PickOutward.X);
	AddLookShot(FVector(Pick->Position + PickOutward * BunkerShotBack + Side * BunkerShotSide, Base.Center.Z + BunkerShotHeight),
	            FVector(Pick->Position, Base.Center.Z), CloseFieldOfView, Label + TEXT("_bunker.png"));
}

void FSovietBasePreviewRunner::QueueTowerShot(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	const FVector2D Center(Base.Center);
	for (const TWeakObjectPtr<AActor>& Structure : Base.SpawnedStructures)
	{
		if (not Structure.IsValid() or not Structure->GetClass()->GetName().Contains(TEXT("GuardTowerMG")))
		{
			continue;
		}
		const FVector2D Position(Structure->GetActorLocation());
		if (Base.Plan.PerimeterLine.Num() >= 3 and not GetIsInsideLine(Position, Base.Plan.PerimeterLine))
		{
			continue;
		}
		FVector2D Toward = (Center - Position).GetSafeNormal();
		Toward = Toward.IsNearlyZero() ? FVector2D(1.0, 0.0) : Toward;
		AddLookShot(FVector(Position + Toward * TowerShotBack, Base.Center.Z + TowerShotHeight), FVector(Position, Base.Center.Z),
		            CloseFieldOfView, Label + TEXT("_tower.png"));
		return;
	}
}

void FSovietBasePreviewRunner::AddTopShot(const FVector& Target, const double Height, const float FieldOfView, const FString& FileName)
{
	FCaptureShot& Shot = M_Shots.AddDefaulted_GetRef();
	Shot.Location = Target + FVector(0.0, 0.0, Height);
	Shot.Rotation = FRotator(-90.f, 0.f, 0.f);
	Shot.FieldOfView = FieldOfView;
	Shot.Target = M_ObliqueTarget.Get();
	Shot.FileName = FileName;
}

void FSovietBasePreviewRunner::QueueYardShots(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	const FSurvivalSovietPlannedYard* Ammo = nullptr;
	const FSurvivalSovietPlannedYard* Fuel = nullptr;
	for (const FSurvivalSovietPlannedYard& Yard : Base.Plan.Yards)
	{
		const FSurvivalSovietPlannedYard*& Best = Yard.Kind == ESurvivalSovietYardKind::Ammo ? Ammo : Fuel;
		const bool bBetter = Best == nullptr or (Yard.Kind == ESurvivalSovietYardKind::Ammo
			                                         ? Yard.ItemPositions.Num() > Best->ItemPositions.Num()
			                                         : Yard.HalfExtents.X * Yard.HalfExtents.Y > Best->HalfExtents.X * Best->HalfExtents.Y);
		Best = bBetter ? &Yard : Best;
	}
	const FSurvivalSovietPlannedYard* Pick = Ammo != nullptr ? Ammo : Fuel;
	if (Pick == nullptr)
	{
		return;
	}
	const FVector Center(Pick->Center, Base.Center.Z);
	const FVector2D Out = (Pick->Entrance - Pick->Center).GetSafeNormal();
	const double Extent = Pick->HalfExtents.GetMax();
	const double Back = YardShotBack + YardShotExtentFactor * Extent;
	AddLookShot(Center + FVector(Out * Back, YardShotHeight * Back / YardShotBack), Center, YardFieldOfView, Label + TEXT("_yard.png"));
	const double TopHeight = FMath::Max(YardTopHeight, (Extent + YardTopMargin) / FMath::Tan(FMath::DegreesToRadians(YardTopFieldOfView * 0.5)));
	AddTopShot(Center, TopHeight, YardTopFieldOfView, Label + TEXT("_yard_top.png"));
	if (Ammo == nullptr or Fuel == nullptr)
	{
		return;
	}
	// Both yards and the ground between them in one frame.
	const FVector2D Middle = (Ammo->Center + Fuel->Center) * 0.5;
	const double HalfSpan = FVector2D::Distance(Ammo->Center, Fuel->Center) * 0.5 + YardPairMargin;
	const double Height = HalfSpan / FMath::Tan(FMath::DegreesToRadians(YardTopFieldOfView * 0.5));
	AddTopShot(FVector(Middle, Base.Center.Z), Height, YardTopFieldOfView, Label + TEXT("_yard_fuel_top.png"));
}

void FSovietBasePreviewRunner::QueueWallDetailShots(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	for (int32 SlotIndex = 0; SlotIndex < Base.Plan.OpenWallSlots.Num() and SlotIndex < MaxOpenSlotShots; ++SlotIndex)
	{
		const FSurvivalSovietPlannedGate& Slot = Base.Plan.OpenWallSlots[SlotIndex];
		const FVector Target(Slot.Center, Base.Center.Z);
		AddLookShot(Target + FVector(Slot.Outward * OpenSlotShotBack, OpenSlotShotHeight), Target, CloseFieldOfView,
		            FString::Printf(TEXT("%s_wall_open_%d.png"), *Label, SlotIndex + 1));
	}
	const FSurvivalSovietPlannedWallPiece* Flag = Base.Plan.WallPieces.FindByPredicate([](const FSurvivalSovietPlannedWallPiece& Piece)
	{
		return Piece.bFlag;
	});
	if (Flag == nullptr)
	{
		return;
	}
	const FVector2D Middle = (Flag->JointStart + Flag->JointEnd) * 0.5;
	const FVector2D Out = GetEdgeOutward(Flag->JointStart, Flag->JointEnd, FVector2D(Base.Center));
	AddLookShot(FVector(Middle + Out * FlagShotBack, Base.Center.Z + FlagShotHeight), FVector(Middle, Base.Center.Z + FlagLookHeight),
	            CloseFieldOfView, Label + TEXT("_wall_flag.png"));
}

void FSovietBasePreviewRunner::QueueLowCloseUps(const FSurvivalSovietBaseState& Base, const FString& Label)
{
	using namespace SovietBasePreviewConstants;
	const FVector2D Center(Base.Center);
	if (not Base.Plan.Gates.IsEmpty())
	{
		const FSurvivalSovietPlannedGate& Gate = Base.Plan.Gates[0];
		const FVector2D Side(-Gate.Outward.Y, Gate.Outward.X);
		AddLookShot(FVector(Gate.Center + Gate.Outward * LowGateBack + Side * LowGateSide, Base.Center.Z + LowHeight),
		            FVector(Gate.Center, Base.Center.Z + CloseLookHeight), CloseFieldOfView, Label + TEXT("_x_gate_low.png"));
	}
	// Along the longest straight span at eye height: joints, faces and flicker show here first.
	TMap<int32, TPair<FVector2D, FVector2D>> Spans;
	for (const FSurvivalSovietPlannedWallPiece& Piece : Base.Plan.WallPieces)
	{
		if (Piece.Kind == ESurvivalSovietWallPieceKind::Gate or Piece.Kind == ESurvivalSovietWallPieceKind::Corner)
		{
			continue;
		}
		TPair<FVector2D, FVector2D>* Span = Spans.Find(Piece.Span);
		if (Span == nullptr)
		{
			Spans.Add(Piece.Span, TPair<FVector2D, FVector2D>(Piece.JointStart, Piece.JointEnd));
			continue;
		}
		Span->Value = Piece.JointEnd;
	}
	double BestLength = 0.0;
	TPair<FVector2D, FVector2D> Best;
	for (const TPair<int32, TPair<FVector2D, FVector2D>>& Span : Spans)
	{
		const double Length = FVector2D::Distance(Span.Value.Key, Span.Value.Value);
		if (Length > BestLength)
		{
			BestLength = Length;
			Best = Span.Value;
		}
	}
	if (BestLength <= 0.0)
	{
		return;
	}
	const FVector2D Walk = (Best.Value - Best.Key).GetSafeNormal();
	const FVector2D Out = GetEdgeOutward(Best.Key, Best.Value, Center);
	AddLookShot(FVector(Best.Key + Out * LowRunOut - Walk * LowRunBack, Base.Center.Z + LowHeight),
	            FVector(Best.Value, Base.Center.Z + CloseLookHeight), RunFieldOfView, Label + TEXT("_x_run_low.png"));
	const FVector2D Middle = (Best.Key + Best.Value) * 0.5;
	AddLookShot(FVector(Middle - Out * LowInsideBack, Base.Center.Z + LowInsideHeight), FVector(Middle, Base.Center.Z + CloseLookHeight),
	            CloseFieldOfView, Label + TEXT("_x_run_inside.png"));
}

void FSovietBasePreviewRunner::WritePlanJson(const FSurvivalSovietBaseState& Base, const FPreviewEntry& Entry) const
{
	const FString Json = M_SovietBases->BuildSpawnManifestJson(Base, Entry.Label);
	FFileHelper::SaveStringToFile(Json, *(M_Directory / (Entry.Label + TEXT("_manifest.json"))));
	WriteExtentJson(Base, Entry);
}

void FSovietBasePreviewRunner::WriteExtentJson(const FSurvivalSovietBaseState& Base, const FPreviewEntry& Entry) const
{
	FFileHelper::SaveStringToFile(SovietPreview_BuildExtentJson(*M_SovietBases, Base, Entry.Label),
	                              *(M_Directory / (Entry.Label + TEXT("_extent.json"))));
}

void FSovietBasePreviewRunner::ReportCounts(const FSurvivalSovietBaseState& Base, const FPreviewEntry& Entry)
{
	int32 PlannedUnits = 0;
	for (const FSurvivalSovietPlannedGarrison& Group : Base.Plan.Garrison)
	{
		PlannedUnits += Group.Units.Num();
	}
	int32 AliveStructures = 0;
	for (const TWeakObjectPtr<AActor>& Structure : Base.SpawnedStructures)
	{
		AliveStructures += Structure.IsValid() ? 1 : 0;
	}
	int32 AliveUnits = 0;
	for (const TWeakObjectPtr<AActor>& Unit : Base.GarrisonUnits)
	{
		AliveUnits += Unit.IsValid() ? 1 : 0;
	}
	const bool bMatches = AliveStructures == Base.Plan.Structures.Num() and AliveUnits == PlannedUnits
		and Base.TrackedStructuresTotal > 0;
	M_TotalProblems += bMatches ? 0 : 1;
	UE_LOG(LogSovietBasePreview, Display,
	       TEXT("SOVIET_PREVIEW counts %s: structures %d/%d, units %d/%d, production %d, tracked %d, dressed pads %d, dressing instances %d in %d groups, %s"),
	       *Entry.Label, AliveStructures, Base.Plan.Structures.Num(), AliveUnits, PlannedUnits,
	       Base.Plan.ProductionCount, Base.TrackedStructuresTotal, Base.DressedPads, Base.DressingInstances, Base.DressingComponents,
	       bMatches ? TEXT("OK") : TEXT("MISMATCH"));
}

bool FSovietBasePreviewRunner::TestPath(const int32 AgentIndex, const FVector& From, const FVector& To) const
{
	using namespace SovietBasePreviewConstants;
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavigationSystem == nullptr)
	{
		return false;
	}
	FNavAgentProperties AgentProperties;
	AgentProperties.AgentRadius = NavAgentRadii[AgentIndex];
	AgentProperties.AgentHeight = NavAgentHeights[AgentIndex];
	const ANavigationData* NavData = NavigationSystem->GetNavDataForProps(AgentProperties);
	if (NavData == nullptr)
	{
		return false;
	}
	const FVector Extent(NavProjectExtent, NavProjectExtent, NavProjectExtentZ);
	FNavLocation ProjectedFrom;
	FNavLocation ProjectedTo;
	if (not NavigationSystem->ProjectPointToNavigation(From, ProjectedFrom, Extent, NavData)
		or not NavigationSystem->ProjectPointToNavigation(To, ProjectedTo, Extent, NavData))
	{
		return false;
	}
	const FPathFindingQuery Query(nullptr, *NavData, ProjectedFrom.Location, ProjectedTo.Location,
	                              UNavigationQueryFilter::GetQueryFilter(*NavData, nullptr, nullptr));
	const FPathFindingResult Result = NavigationSystem->FindPathSync(AgentProperties, Query);
	if (Result.IsSuccessful() and Result.Path.IsValid() and not Result.IsPartial())
	{
		return true;
	}
	// A partial result can be the search node limit (fine character navmesh, long way round a rim): chain from its end.
	const SurvivalPOINav::FChainedPathResult Chained = SurvivalPOINav::FindChainedPath(
		GetWorld(), ProjectedFrom.Location, ProjectedTo.Location, AgentProperties.AgentRadius, AgentProperties.AgentHeight);
	UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PREVIEW path %s to %s: single query %s, chained %s after %d hops"),
	       *ProjectedFrom.Location.ToCompactString(), *ProjectedTo.Location.ToCompactString(),
	       Result.IsSuccessful() ? (Result.IsPartial() ? TEXT("partial") : TEXT("no path object")) : TEXT("failed"),
	       Chained.bReached ? TEXT("reached") : TEXT("not reached"), Chained.Hops);
	return Chained.bReached;
}

void FSovietBasePreviewRunner::ReportPaths(
	const FSurvivalSovietBaseState& Base,
	const FPreviewEntry& Entry,
	const FVector& ThreatLocation)
{
	using namespace SovietBasePreviewConstants;
	TArray<FVector> Targets = M_SovietBases->GetPathTestPointsForPreview(Base);
	// From outside the main gate, on its own approach: a fixed distance toward the threat landed in the oasis on the shrunk
	// map (Red Mesa, round 3), where nothing projects to the navmesh.
	FVector Outside = Base.Center + (ThreatLocation - Base.Center).GetSafeNormal2D()
		* (M_SovietBases->GetBaseBuildRadius(*Base.StartLocation) + PathStartMargin);
	if (not Base.Plan.Gates.IsEmpty())
	{
		const FSurvivalSovietPlannedGate& Gate = Base.Plan.Gates[0];
		Outside = FVector(Gate.Center + Gate.Outward * PathStartBeyondGate, Base.Center.Z);
	}
	UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PREVIEW path start for %s: %s"), *Entry.Label, *Outside.ToCompactString());
	for (int32 AgentIndex = 0; AgentIndex < NavAgentCount; ++AgentIndex)
	{
		int32 Reached = 0;
		FString Missed;
		for (const FVector& Target : Targets)
		{
			const bool bReached = TestPath(AgentIndex, Outside, Target);
			Reached += bReached ? 1 : 0;
			Missed += bReached ? FString() : FString::Printf(TEXT(" (%.0f, %.0f)"), Target.X, Target.Y);
		}
		if (not Missed.IsEmpty())
		{
			UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PREVIEW unreached %s %s:%s"), *Entry.Label, NavAgentNames[AgentIndex], *Missed);
		}
		const bool bAllReached = Reached == Targets.Num();
		M_TotalProblems += bAllReached ? 0 : 1;
		UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PREVIEW paths %s %s: %d/%d %s"), *Entry.Label,
		       NavAgentNames[AgentIndex], Reached, Targets.Num(), bAllReached ? TEXT("OK") : TEXT("BLOCKED"));
	}
}

// ---------------------------------------------------------------------------
// Survey
// ---------------------------------------------------------------------------

void FSovietBasePreviewRunner::StartSurveyClass()
{
	if (M_NextSurveyPath >= M_SurveyPaths.Num())
	{
		Finish();
		return;
	}
	UWorld* World = GetWorld();
	const FString Path = M_SurveyPaths[M_NextSurveyPath++];
	M_SurveyLabel = FPaths::GetBaseFilename(Path);
	UClass* ActorClass = LoadClass<AActor>(nullptr, *Path);
	if (ActorClass == nullptr)
	{
		UE_LOG(LogSovietBasePreview, Warning, TEXT("SOVIET_SURVEY cannot load %s"), *Path);
		return;
	}
	const TArray<TWeakObjectPtr<ASurvivalStartLocation>>& Locations = M_Director->GetStartLocations();
	const FVector Location = Locations[0]->GetDeployCenter();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.bDeferConstruction = true;
	const FTransform Transform(Location);
	AActor* Actor = World->SpawnActor<AActor>(ActorClass, Transform, SpawnParameters);
	if (not IsValid(Actor))
	{
		UE_LOG(LogSovietBasePreview, Warning, TEXT("SOVIET_SURVEY cannot spawn %s"), *Path);
		return;
	}
	if (URTSComponent* RTSComponent = Actor->FindComponentByClass<URTSComponent>())
	{
		RTSComponent->SetAddToGameState(false);
	}
	Actor->FinishSpawning(Transform);
	M_SurveyActor = Actor;
	AdvanceTo(EStep::SettleSurveyClass);
}

void FSovietBasePreviewRunner::QueueSurveyShots()
{
	using namespace SovietBasePreviewConstants;
	AActor* Actor = M_SurveyActor.Get();
	if (not IsValid(Actor))
	{
		AdvanceTo(EStep::StartSurveyClass);
		return;
	}
	const FVector Location = Actor->GetActorLocation();
	// Some Blueprints leave a hidden component at the world origin; it must not count towards the footprint.
	FBox Bounds(ForceInit);
	FBox ColliderBounds(ForceInit);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (FVector::Dist2D(Primitive->Bounds.Origin, Location) > SurveyMaxComponentDistance)
		{
			continue;
		}
		Bounds += Primitive->Bounds.GetBox();
		if (Primitive->IsCollisionEnabled())
		{
			ColliderBounds += Primitive->Bounds.GetBox();
		}
	}
	const FVector Center = Bounds.GetCenter() - Location;
	const FVector Extent = Bounds.GetExtent();
	const FVector ColliderCenter = ColliderBounds.GetCenter() - Location;
	const FVector ColliderExtent = ColliderBounds.GetExtent();
	const URTSComponent* RTSComponent = Actor->FindComponentByClass<URTSComponent>();
	UE_LOG(LogSovietBasePreview, Display,
	       TEXT("SOVIET_SURVEY %s extent %.0f %.0f %.0f center %.0f %.0f %.0f colliders extent %.0f %.0f center %.0f %.0f owner %d parent %s"),
	       *M_SurveyLabel, Extent.X, Extent.Y, Extent.Z, Center.X, Center.Y, Center.Z, ColliderExtent.X, ColliderExtent.Y,
	       ColliderCenter.X, ColliderCenter.Y, IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1,
	       *Actor->GetClass()->GetSuperClass()->GetName());

	const double ViewWidth = FMath::Max(SurveyMinWidth, FMath::Max(Extent.X, Extent.Y) * SurveyWidthFactor);
	const double Height = ViewWidth * 0.5 / FMath::Tan(FMath::DegreesToRadians(TopFieldOfView * 0.5));
	FCaptureShot& TopShot = M_Shots.AddDefaulted_GetRef();
	TopShot.Location = Location + FVector(0.0, 0.0, Height);
	TopShot.Rotation = FRotator(-90.f, 0.f, 0.f);
	TopShot.FieldOfView = TopFieldOfView;
	TopShot.Target = M_SurveyTarget.Get();
	TopShot.FileName = M_SurveyLabel + TEXT("_top.png");

	// From +X: an actor whose front is local +X shows its front in this picture.
	const double Distance = ViewWidth * SurveyObliqueDistanceFactor * 0.5;
	FCaptureShot& ObliqueShot = M_Shots.AddDefaulted_GetRef();
	ObliqueShot.Location = Location + FVector(Distance * 0.8, Distance * 0.25, Distance * 0.55);
	ObliqueShot.Rotation = (Location - ObliqueShot.Location).Rotation();
	ObliqueShot.FieldOfView = ObliqueFieldOfView;
	ObliqueShot.Target = M_SurveyTarget.Get();
	ObliqueShot.FileName = M_SurveyLabel + TEXT("_oblique.png");
	AdvanceTo(EStep::SurveyShots);
}

void FSovietBasePreviewRunner::FinishSurveyClass()
{
	if (M_SurveyActor.IsValid())
	{
		M_SurveyActor->Destroy();
	}
	AdvanceTo(EStep::StartSurveyClass);
}


/**
 * @brief Frame cost of the footprint dressing in a real match (-SovietBasePerf with the watch): the main viewport is posed
 * over the densest base at the RTS camera and over the whole map, and the dressing is hidden and shown in alternating
 * blocks (5 pairs x 600 frames per pose, after a settle). Medians of game, render and GPU time and draw calls per block,
 * the per-pair deltas and their mean and spread go to perf_dressing.txt. Hiding in-process keeps the scene, units and
 * light the same for both halves of a pair.
 */
class FSovietDressingPerf : public TSharedFromThis<FSovietDressingPerf>
{
public:
	FSovietDressingPerf(UWorld* InWorld, USurvivalSovietBaseComponent* InSovietBases, const FString& InDirectory, const FVector& InPlayerStart)
		: M_World(InWorld)
		  , M_SovietBases(InSovietBases)
		  , M_Directory(InDirectory)
		  , M_PlayerStart(InPlayerStart)
	{
	}

	void Start()
	{
		TSharedRef<FSovietDressingPerf> SharedThis = AsShared();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([SharedThis](const float Delta)
		{
			return SharedThis->Step();
		}));
	}

	bool GetIsDone() const { return bM_Done; }

private:
	static constexpr int32 SettleFrames = 60;
	static constexpr int32 SampleFrames = 600;
	static constexpr int32 Pairs = 5;
	static constexpr int32 Poses = 2;
	static constexpr double MapViewMargin = 8000.0;
	static constexpr float MapFieldOfView = 60.f;
	static constexpr double ViewTargetTolerance = 50.0;

	struct FSample
	{
		double Game = 0.0;
		double Render = 0.0;
		double Gpu = 0.0;
		double Draws = 0.0;
		double Primitives = 0.0;
	};

	TWeakObjectPtr<UWorld> M_World;
	TWeakObjectPtr<USurvivalSovietBaseComponent> M_SovietBases;
	FString M_Directory;
	FVector M_PlayerStart;
	TWeakObjectPtr<ACameraActor> M_Camera;
	int32 M_Pose = 0;
	int32 M_Block = 0;
	int32 M_Frame = 0;
	int32 M_ViewMisses = 0;
	bool bM_Done = false;
	TArray<FSample> M_Samples;
	FString M_Report;
	// Medians per pose, block: shown (true) or hidden.
	TArray<TPair<bool, FSample>> M_BlockMedians[Poses];

	static double Median(TArray<double> Values)
	{
		if (Values.IsEmpty())
		{
			return 0.0;
		}
		Values.Sort();
		return Values[Values.Num() / 2];
	}

	/** Block order inside a pair alternates (shown first, then hidden first) so drift does not favour one side. */
	static bool GetIsShownBlock(const int32 Block)
	{
		const int32 Pair = Block / 2;
		const bool bFirst = Block % 2 == 0;
		return Pair % 2 == 0 ? bFirst : not bFirst;
	}

	void PoseCamera()
	{
		UWorld* World = M_World.Get();
		const TArray<FSurvivalSovietBaseState>& Bases = M_SovietBases->GetBaseStatesForPreview();
		if (not M_Camera.IsValid())
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			M_Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SpawnParameters);
		}
		FVector Eye = FVector::ZeroVector;
		FVector LookAt = FVector::ZeroVector;
		float FieldOfView = SovietBasePreviewConstants::RtsFieldOfView;
		if (M_Pose == 0)
		{
			// The RTS camera over the base with the most dressing, from the player's side.
			const FSurvivalSovietBaseState* Densest = nullptr;
			for (const FSurvivalSovietBaseState& Base : Bases)
			{
				Densest = Densest == nullptr or Base.DressingInstances > Densest->DressingInstances ? &Base : Densest;
			}
			LookAt = Densest != nullptr ? Densest->Center : FVector::ZeroVector;
			const FVector2D Toward = FVector2D(M_PlayerStart - LookAt).GetSafeNormal();
			const double Pitch = FMath::DegreesToRadians(SovietBasePreviewConstants::RtsPitchDegrees);
			Eye = LookAt + FVector(Toward * (SovietBasePreviewConstants::RtsDistance * FMath::Cos(Pitch)),
			                       SovietBasePreviewConstants::RtsDistance * FMath::Sin(Pitch));
			M_Report += FString::Printf(TEXT("pose rts: base %d at %s, eye %s\n"), Densest != nullptr ? Densest->BaseIndex : -1,
			                            *LookAt.ToCompactString(), *Eye.ToCompactString());
		}
		else
		{
			// Straight down over the middle of all bases, high enough to hold every one.
			FVector Sum = FVector::ZeroVector;
			for (const FSurvivalSovietBaseState& Base : Bases)
			{
				Sum += Base.Center;
			}
			LookAt = Bases.IsEmpty() ? FVector::ZeroVector : Sum / Bases.Num();
			double Reach = 0.0;
			for (const FSurvivalSovietBaseState& Base : Bases)
			{
				Reach = FMath::Max(Reach, FVector::Dist2D(Base.Center, LookAt));
			}
			FieldOfView = MapFieldOfView;
			const double Height = (Reach + MapViewMargin) / FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5));
			Eye = LookAt + FVector(0.0, -1.0, Height);
			M_Report += FString::Printf(TEXT("pose map: centre %s, height %.0f\n"), *LookAt.ToCompactString(), Height);
		}
		M_Camera->SetActorLocationAndRotation(Eye, (LookAt - Eye).Rotation());
		M_Camera->GetCameraComponent()->SetFieldOfView(FieldOfView);
		M_Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		if (APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0))
		{
			Controller->SetViewTargetWithBlend(M_Camera.Get(), 0.f);
		}
	}

	bool Step()
	{
		UWorld* World = M_World.Get();
		if (not IsValid(World) or not M_SovietBases.IsValid())
		{
			bM_Done = true;
			return false;
		}
		if (M_Frame == 0)
		{
			if (M_Block == 0)
			{
				PoseCamera();
			}
			ASurvivalSovietBaseDressing::SetAllHidden(World, not GetIsShownBlock(M_Block));
			M_Samples.Reset();
		}
		++M_Frame;
		// Still posed? The RTS camera pawn could take the view back.
		if (APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (Controller->PlayerCameraManager != nullptr and M_Camera.IsValid()
				and FVector::Dist(Controller->PlayerCameraManager->GetCameraLocation(), M_Camera->GetActorLocation()) > ViewTargetTolerance)
			{
				++M_ViewMisses;
				Controller->SetViewTargetWithBlend(M_Camera.Get(), 0.f);
			}
		}
		if (M_Frame <= SettleFrames)
		{
			return true;
		}
		FSample Sample;
		Sample.Game = FPlatformTime::ToMilliseconds(GGameThreadTime);
		Sample.Render = FPlatformTime::ToMilliseconds(GRenderThreadTime);
		Sample.Gpu = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
		Sample.Draws = GNumDrawCallsRHI[0];
		Sample.Primitives = GNumPrimitivesDrawnRHI[0];
		M_Samples.Add(Sample);
		if (M_Frame < SettleFrames + SampleFrames)
		{
			return true;
		}
		// Block done: its medians.
		TArray<double> Game, Render, Gpu, Draws, Primitives;
		for (const FSample& Each : M_Samples)
		{
			Game.Add(Each.Game);
			Render.Add(Each.Render);
			Gpu.Add(Each.Gpu);
			Draws.Add(Each.Draws);
			Primitives.Add(Each.Primitives);
		}
		FSample BlockMedian;
		BlockMedian.Game = Median(Game);
		BlockMedian.Render = Median(Render);
		BlockMedian.Gpu = Median(Gpu);
		BlockMedian.Draws = Median(Draws);
		BlockMedian.Primitives = Median(Primitives);
		const bool bShown = GetIsShownBlock(M_Block);
		M_BlockMedians[M_Pose].Emplace(bShown, BlockMedian);
		M_Report += FString::Printf(TEXT("pose %d block %d dressing %s: game %.3f ms, render %.3f ms, gpu %.3f ms, draws %.0f, primitives %.0f\n"),
		                            M_Pose, M_Block, bShown ? TEXT("on ") : TEXT("off"), BlockMedian.Game, BlockMedian.Render, BlockMedian.Gpu,
		                            BlockMedian.Draws, BlockMedian.Primitives);
		M_Frame = 0;
		++M_Block;
		if (M_Block < 2 * Pairs)
		{
			return true;
		}
		WritePoseSummary(M_Pose);
		M_Block = 0;
		++M_Pose;
		if (M_Pose < Poses)
		{
			return true;
		}
		ASurvivalSovietBaseDressing::SetAllHidden(World, false);
		FinishReport();
		bM_Done = true;
		return false;
	}

	void WritePoseSummary(const int32 Pose)
	{
		// Per pair: shown minus hidden.
		TArray<double> Deltas[4];
		const TArray<TPair<bool, FSample>>& Blocks = M_BlockMedians[Pose];
		for (int32 Pair = 0; 2 * Pair + 1 < Blocks.Num(); ++Pair)
		{
			const TPair<bool, FSample>& A = Blocks[2 * Pair];
			const TPair<bool, FSample>& B = Blocks[2 * Pair + 1];
			const FSample& On = A.Key ? A.Value : B.Value;
			const FSample& Off = A.Key ? B.Value : A.Value;
			Deltas[0].Add(On.Game - Off.Game);
			Deltas[1].Add(On.Render - Off.Render);
			Deltas[2].Add(On.Gpu - Off.Gpu);
			Deltas[3].Add(On.Draws - Off.Draws);
		}
		const TCHAR* Names[4] = {TEXT("game ms"), TEXT("render ms"), TEXT("gpu ms"), TEXT("draws")};
		M_Report += FString::Printf(TEXT("pose %d deltas (dressing on minus off, %d pairs):\n"), Pose, Deltas[0].Num());
		for (int32 Metric = 0; Metric < 4; ++Metric)
		{
			double Sum = 0.0;
			for (const double Value : Deltas[Metric])
			{
				Sum += Value;
			}
			const double Mean = Deltas[Metric].IsEmpty() ? 0.0 : Sum / Deltas[Metric].Num();
			double Variance = 0.0;
			for (const double Value : Deltas[Metric])
			{
				Variance += FMath::Square(Value - Mean);
			}
			const double Spread = Deltas[Metric].Num() > 1 ? FMath::Sqrt(Variance / (Deltas[Metric].Num() - 1)) : 0.0;
			M_Report += FString::Printf(TEXT("  %s: mean %+.3f, sd %.3f, median %+.3f\n"), Names[Metric], Mean, Spread, Median(Deltas[Metric]));
		}
	}

	void FinishReport()
	{
		M_Report += FString::Printf(TEXT("view target taken back %d times (re-posed each time)\n"), M_ViewMisses);
		for (const FSurvivalSovietBaseState& Base : M_SovietBases->GetBaseStatesForPreview())
		{
			M_Report += FString::Printf(TEXT("base %d: %d dressed pads, %d instances in %d instanced groups\n"), Base.BaseIndex, Base.DressedPads,
			                            Base.DressingInstances, Base.DressingComponents);
		}
		FFileHelper::SaveStringToFile(M_Report, *(M_Directory / TEXT("perf_dressing.txt")));
		UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PERF written: %s"), *(M_Directory / TEXT("perf_dressing.txt")));
	}
};

/**
 * @brief Watches a real match for the review: every enemy unit that appears outside the garrison spawn step, with its
 * distance to the nearest base, proves where the strategic AI trains. Logs storage too; ends the process when done.
 */
class FSovietBaseWatch : public TSharedFromThis<FSovietBaseWatch>
{
public:
	FSovietBaseWatch(ASurvivalModeDirector* InDirector, USurvivalSovietBaseComponent* InSovietBases, const float InSeconds)
		: M_Director(InDirector)
		  , M_SovietBases(InSovietBases)
		  , M_SecondsLeft(InSeconds)
	{
	}

	void Start()
	{
		TSharedRef<FSovietBaseWatch> SharedThis = AsShared();
		M_Director->GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([SharedThis](AActor* Actor)
		{
			SharedThis->OnActorSpawned(Actor);
		}));
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([SharedThis](const float Delta)
		{
			return SharedThis->Report(Delta);
		}), ReportSeconds);
	}

private:
	static constexpr float ReportSeconds = 2.f;
	static constexpr float RealMatchExposureBias = 1.5f;
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;
	bool bM_PressedStart = false;
	double M_PressSeconds = 0.0;
	// Seconds from the start press until each base's last structure stood (by base array index); -1 while building.
	TArray<double> M_StoodAfterPress;
	bool bM_ManifestsWritten = false;
	// Real-match views: one RTS oblique per base, taken over two reports each (pose, then export).
	FString M_ShotDirectory;
	int32 M_NextShotBase = INDEX_NONE;
	bool bM_ShotPosed = false;
	TWeakObjectPtr<ASceneCapture2D> M_ShotCamera;
	TStrongObjectPtr<UTextureRenderTarget2D> M_ShotTarget;
	TWeakObjectPtr<USurvivalSovietBaseComponent> M_SovietBases;
	float M_SecondsLeft = 0.f;
	// -SovietBasePerf: the dressing frame cost, started once the base views are taken; the watch waits for it.
	TSharedPtr<FSovietDressingPerf> M_Perf;

	void OnActorSpawned(AActor* Actor) const
	{
		if (not M_SovietBases.IsValid() or not IsValid(Actor) or M_SovietBases->GetBaseStatesForPreview().IsEmpty())
		{
			return;
		}
		if (not Actor->IsA<ASquadController>() and not Actor->IsA<ATankMaster>())
		{
			return;
		}
		double NearestDistance = TNumericLimits<double>::Max();
		int32 NearestBase = INDEX_NONE;
		for (const FSurvivalSovietBaseState& Base : M_SovietBases->GetBaseStatesForPreview())
		{
			const double Distance = FVector::Dist2D(Actor->GetActorLocation(), Base.Center);
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestBase = Base.BaseIndex;
			}
		}
		// The AI spawns trained units at a cluster's centre; aircraft drops land elsewhere. Same search as the AI's.
		double NearestCluster = TNumericLimits<double>::Max();
		if (const FFindEnemyBaseClusters* Request = M_SovietBases->GetClusterRequestForPreview())
		{
			const FResultEnemyBaseClusters Result = FStrategicAIHelpers::BuildEnemyBaseClustersResult(
				*Request, M_SovietBases->BuildBuildingSnapshotForPreview());
			for (const FEnemyBasePointCoreBuildings& Point : Result.BasePoints)
			{
				NearestCluster = FMath::Min(NearestCluster, FVector::Dist2D(Actor->GetActorLocation(), Point.BaseLocation));
			}
		}
		UE_LOG(LogSovietBasePreview, Display,
		       TEXT("SOVIET_WATCH unit spawned %s at %s, %.0f cm from base %d, %.0f cm from a cluster centre, garrison pending %d"),
		       *Actor->GetClass()->GetName(), *Actor->GetActorLocation().ToCompactString(), NearestDistance, NearestBase,
		       NearestCluster, M_SovietBases->GetPendingGarrisonGroupsForPreview());
	}

	static bool GetIsWaitingForStart(UWorld* World)
	{
		ACPPController* const Controller = Cast<ACPPController>(UGameplayStatics::GetPlayerController(World, 0));
		const UPlayerStartGameControl* const StartControl = IsValid(Controller)
			                                                    ? Controller->FindComponentByClass<UPlayerStartGameControl>()
			                                                    : nullptr;
		return not IsValid(StartControl) or StartControl->GetIsWaitingForPlayerToStart();
	}

	/** Presses "Press To Start" as the player does (MapSmokeTestSubsystem::TryStartGame), so the match is a real one. */
	void PressStartAsPlayer()
	{
		UWorld* const World = M_Director->GetWorld();
		if (not bM_PressedStart and not GetIsWaitingForStart(World))
		{
			// Someone else (the POI preview) pressed start: the match runs, time it from now.
			bM_PressedStart = true;
			M_PressSeconds = FPlatformTime::Seconds();
		}
		if (not bM_PressedStart)
		{
			ACPPController* const Controller = Cast<ACPPController>(UGameplayStatics::GetPlayerController(World, 0));
			UPlayerStartGameControl* const StartControl = IsValid(Controller)
				                                              ? Controller->FindComponentByClass<UPlayerStartGameControl>()
				                                              : nullptr;
			if (IsValid(StartControl) and StartControl->GetIsWaitingForPlayerToStart())
			{
				StartControl->PlayerStartedGame();
				bM_PressedStart = true;
				M_PressSeconds = FPlatformTime::Seconds();
				UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_WATCH pressed start"));
			}
		}
		// Fallback only: a world still paused after the press (no start control) would build nothing.
		if (World->IsPaused())
		{
			UGameplayStatics::SetGamePaused(World, false);
		}
	}

	bool Report(const float Delta)
	{
		// With -SurvivalAutoStart nobody presses the start button of UPlayerStartGameControl, so the game stays paused.
		const bool bKeepPause = FParse::Param(FCommandLine::Get(), TEXT("SovietBaseWatchKeepPause"));
		if (M_Director.IsValid() and not bKeepPause)
		{
			PressStartAsPlayer();
		}
		M_SecondsLeft -= ReportSeconds;
		if (M_SovietBases.IsValid())
		{
			const UPlayerResourceManager* Resources = FRTS_Statics::GetPlayerResourceManager(M_SovietBases.Get());
			int32 Standing = 0;
			for (const FSurvivalSovietBaseState& Base : M_SovietBases->GetBaseStatesForPreview())
			{
				for (const TWeakObjectPtr<AActor>& Structure : Base.SpawnedStructures)
				{
					Standing += Structure.IsValid() ? 1 : 0;
				}
			}
			UE_LOG(LogSovietBasePreview, Display,
			       TEXT("SOVIET_WATCH paused %d, phase %d, structures standing %d, bases alive %d/%d, storage rad %d metal %d parts %d fuel %d"),
			       M_Director->GetWorld()->IsPaused() ? 1 : 0, static_cast<int32>(M_Director->GetPhase()), Standing,
			       M_SovietBases->GetAliveSovietBaseCount(), M_SovietBases->GetSovietBaseCount(),
			       IsValid(Resources) ? Resources->GetResourceStorage(ERTSResourceType::Resource_Radixite) : -1,
			       IsValid(Resources) ? Resources->GetResourceStorage(ERTSResourceType::Resource_Metal) : -1,
			       IsValid(Resources) ? Resources->GetResourceStorage(ERTSResourceType::Resource_VehicleParts) : -1,
			       IsValid(Resources) ? Resources->GetResourceStorage(ERTSResourceType::Resource_Fuel) : -1);
		}
		if (M_SovietBases.IsValid())
		{
			NoteBasesStanding();
			StepBaseShots();
			StartPerfWhenReady();
		}
		if (M_SecondsLeft > 0.f or (M_Perf.IsValid() and not M_Perf->GetIsDone()))
		{
			return true;
		}
		FPlatformMisc::RequestExit(false, TEXT("SovietBaseWatch"));
		return false;
	}

	/** @brief Once every base stands: one summary line per base and its manifest from the real match's actors. */
	void NoteBasesStanding()
	{
		const TArray<FSurvivalSovietBaseState>& Bases = M_SovietBases->GetBaseStatesForPreview();
		M_StoodAfterPress.SetNum(Bases.Num());
		bool bAllStand = not Bases.IsEmpty();
		for (int32 Index = 0; Index < Bases.Num(); ++Index)
		{
			const FSurvivalSovietBaseState& Base = Bases[Index];
			int32 Standing = 0;
			for (const TWeakObjectPtr<AActor>& Structure : Base.SpawnedStructures)
			{
				Standing += Structure.IsValid() ? 1 : 0;
			}
			const bool bStands = Base.bStructuresSpawned and Standing > 0;
			if (bStands and M_StoodAfterPress[Index] <= 0.0 and bM_PressedStart)
			{
				M_StoodAfterPress[Index] = FPlatformTime::Seconds() - M_PressSeconds;
			}
			bAllStand = bAllStand and bStands;
		}
		if (not bAllStand or bM_ManifestsWritten or not bM_PressedStart)
		{
			return;
		}
		bM_ManifestsWritten = true;
		FString Directory;
		if (not FParse::Value(FCommandLine::Get(), TEXT("SovietBaseWatchDir="), Directory))
		{
			FParse::Value(FCommandLine::Get(), TEXT("SurvivalPOIDir="), Directory);
		}
		const bool bWriteManifests = not Directory.IsEmpty();
		for (int32 Index = 0; Index < Bases.Num(); ++Index)
		{
			const FSurvivalSovietBaseState& Base = Bases[Index];
			int32 Standing = 0;
			for (const TWeakObjectPtr<AActor>& Structure : Base.SpawnedStructures)
			{
				Standing += Structure.IsValid() ? 1 : 0;
			}
			const FString Label = FString::Printf(TEXT("real_base%d"), Base.BaseIndex);
			UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_WATCH base %d build radius %.0f (start location actor says %.0f), foundation dressing on %d pads, %d mesh instances"),
			       Base.BaseIndex, Base.StartLocation.IsValid() ? M_SovietBases->GetBaseBuildRadius(*Base.StartLocation) : 0.f,
			       Base.StartLocation.IsValid() ? Base.StartLocation->GetBuildAreaRadius() : 0.f, Base.DressedPads, Base.DressingInstances);
			UE_LOG(LogSovietBasePreview, Display,
			       TEXT("SOVIET_WATCH base %d at %s: preset index %d, seed %d, strength %d, rank %d, structures %d planned / %d standing, rule violations %d, stood %.1f s after the start press, manifest %s"),
			       Base.BaseIndex, *GetNameSafe(Base.StartLocation.Get()), Base.PresetIndex, Base.Seed, Base.Strength,
			       Base.DistanceRank, Base.Plan.Structures.Num(), Standing, Base.Plan.RuleViolations.Num(), M_StoodAfterPress[Index],
			       bWriteManifests ? *(Label + TEXT("_manifest.json")) : TEXT("not written"));
			if (bWriteManifests)
			{
				FFileHelper::SaveStringToFile(M_SovietBases->BuildSpawnManifestJson(Base, Label),
				                              *(Directory / (Label + TEXT("_manifest.json"))));
				FFileHelper::SaveStringToFile(SovietPreview_BuildExtentJson(*M_SovietBases, Base, Label),
				                              *(Directory / (Label + TEXT("_extent.json"))));
			}
		}
		if (bWriteManifests)
		{
			M_ShotDirectory = Directory;
			M_NextShotBase = 0;
		}
	}

	void StartPerfWhenReady()
	{
		if (M_Perf.IsValid() or not bM_ManifestsWritten or M_NextShotBase != INDEX_NONE or M_ShotDirectory.IsEmpty()
			or not FParse::Param(FCommandLine::Get(), TEXT("SovietBasePerf")))
		{
			return;
		}
		const ASurvivalStartLocation* const Picked = M_Director->GetChosenStartLocation();
		M_Perf = MakeShared<FSovietDressingPerf>(M_Director->GetWorld(), M_SovietBases.Get(), M_ShotDirectory,
		                                         IsValid(Picked) ? Picked->GetActorLocation() : FVector::ZeroVector);
		M_Perf->Start();
		UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_PERF started"));
	}

	bool EnsureShotCamera()
	{
		if (M_ShotCamera.IsValid() and M_ShotTarget.IsValid())
		{
			return true;
		}
		UWorld* const World = M_Director->GetWorld();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASceneCapture2D* const Camera = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity,
		                                                                   SpawnParameters);
		UTextureRenderTarget2D* const Target = UKismetRenderingLibrary::CreateRenderTarget2D(
			World, SovietBasePreviewConstants::ObliquePictureWidth, SovietBasePreviewConstants::ObliquePictureHeight, RTF_RGBA8_SRGB);
		if (not IsValid(Camera) or not IsValid(Target))
		{
			return false;
		}
		USceneCaptureComponent2D* const Capture = Camera->GetCaptureComponent2D();
		Capture->CaptureSource = SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->ShowFlags.SetPostProcessMaterial(false);
		Capture->ShowFlags.SetFog(false);
		Capture->ShowFlags.SetVolumetricFog(false);
		Capture->TextureTarget = Target;
		Capture->FOVAngle = SovietBasePreviewConstants::RtsFieldOfView;
		// Same exposure as the base previews (-CaptureExposureBias=1.5), so the real-match views compare with them.
		FPostProcessSettings& PostProcess = Capture->PostProcessSettings;
		PostProcess.bOverride_AutoExposureMethod = true;
		PostProcess.AutoExposureMethod = AEM_Manual;
		PostProcess.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		PostProcess.AutoExposureApplyPhysicalCameraExposure = 0;
		PostProcess.bOverride_AutoExposureBias = true;
		PostProcess.AutoExposureBias = RealMatchExposureBias;
		Capture->PostProcessBlendWeight = 1.f;
		M_ShotCamera = Camera;
		M_ShotTarget.Reset(Target);
		return true;
	}

	/** @brief One RTS oblique per base from the real match, posed from the side facing the player's start. */
	void StepBaseShots()
	{
		const TArray<FSurvivalSovietBaseState>& Bases = M_SovietBases->GetBaseStatesForPreview();
		if (M_NextShotBase == INDEX_NONE or not EnsureShotCamera())
		{
			return;
		}
		USceneCaptureComponent2D* const Capture = M_ShotCamera->GetCaptureComponent2D();
		if (bM_ShotPosed)
		{
			const FSurvivalSovietBaseState& Shot = Bases[M_NextShotBase];
			Capture->bCaptureEveryFrame = false;
			FlushRenderingCommands();
			int32 Seed = 0;
			FParse::Value(FCommandLine::Get(), TEXT("POISeed="), Seed);
			const FString StartName = Shot.StartLocation.IsValid() ? Shot.StartLocation->GetDisplayName().ToString().Replace(TEXT(" "), TEXT("")) : TEXT("unknown");
			const FString FileName = FString::Printf(TEXT("base_%s_%d_rts.png"), *StartName, Seed);
			UKismetRenderingLibrary::ExportRenderTarget(M_Director->GetWorld(), M_ShotTarget.Get(), M_ShotDirectory, FileName);
			UE_LOG(LogSovietBasePreview, Display, TEXT("SOVIET_WATCH view %s of base %d"), *FileName, Shot.BaseIndex);
			bM_ShotPosed = false;
			++M_NextShotBase;
		}
		if (not Bases.IsValidIndex(M_NextShotBase))
		{
			M_NextShotBase = INDEX_NONE;
			return;
		}
		const FSurvivalSovietBaseState& Base = Bases[M_NextShotBase];
		const ASurvivalStartLocation* const Picked = M_Director->GetChosenStartLocation();
		const FVector2D Toward = IsValid(Picked) ? FVector2D(Picked->GetActorLocation() - Base.Center).GetSafeNormal() : FVector2D(1.0, 0.0);
		const double Pitch = FMath::DegreesToRadians(SovietBasePreviewConstants::RtsPitchDegrees);
		const FVector Eye = Base.Center + FVector(Toward * (SovietBasePreviewConstants::RtsDistance * FMath::Cos(Pitch)),
		                                          SovietBasePreviewConstants::RtsDistance * FMath::Sin(Pitch));
		M_ShotCamera->SetActorLocationAndRotation(Eye, (Base.Center - Eye).Rotation());
		Capture->bCaptureEveryFrame = true;
		bM_ShotPosed = true;
	}
};

void FSurvivalSovietBasePreview::StartIfRequested(ASurvivalModeDirector* Director,
                                                  USurvivalSovietBaseComponent* SovietBases)
{
	float WatchSeconds = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SovietBaseWatch="), WatchSeconds) and IsValid(Director) and IsValid(SovietBases))
	{
		MakeShared<FSovietBaseWatch>(Director, SovietBases, WatchSeconds)->Start();
		return;
	}
	const bool bRequested = FCString::Stristr(FCommandLine::Get(), TEXT("-SovietBasePreview=")) != nullptr
		or FCString::Stristr(FCommandLine::Get(), TEXT("-SovietBaseSurvey=")) != nullptr
		or FCString::Stristr(FCommandLine::Get(), TEXT("-SovietBaseFight=")) != nullptr;
	if (not bRequested or not IsValid(Director) or not IsValid(SovietBases))
	{
		return;
	}
	MakeShared<FSovietBasePreviewRunner>(Director, SovietBases)->Start();
}

#endif
