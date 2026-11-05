// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShowcaseCaptureSubsystem.h"

#include "CanvasTypes.h"
#include "ContentStreaming.h"
#include "CineCameraComponent.h"
#include "Components/PostProcessComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Light.h"
#include "Engine/LevelStreaming.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Info.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Showcase/RTSShowcaseCamera.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShowcase, Log, All);

namespace RTSShowcaseConstants
{
	const TCHAR* const ShotsSwitch = TEXT("ShowcaseShots=");
	const TCHAR* const ScaleSwitch = TEXT("ShowcaseScale=");
	const TCHAR* const FramesSwitch = TEXT("ShowcaseFrames=");
	const TCHAR* const SettleSwitch = TEXT("ShowcaseSettle=");
	const TCHAR* const WarmupSwitch = TEXT("ShowcaseWarmup=");
	const TCHAR* const DirectorySwitch = TEXT("ShowcaseDir=");
	const TCHAR* const NoFreezeSwitch = TEXT("ShowcaseNoFreeze");
	// Opt-in: loads every mip of every texture; heavy with all units of the game resident.
	const TCHAR* const NoStreamingSwitch = TEXT("ShowcaseNoStreaming");
	const TCHAR* const AllFilter = TEXT("all");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	// TargetPoints tagged ShowcaseSpawn=<class path> stand in for units the level cannot save (child turret timelines).
	const TCHAR* const SpawnTagPrefix = TEXT("ShowcaseSpawn=");
	// Stills carry no particle VFX: exhaust and smoke freeze into blobs, and enemy-owned units fire during the
	// warm-up. Only steady glows and lights (radixite cores, lamps) stay.
	const TCHAR* const KeptEffectNameParts[] = {TEXT("Glow"), TEXT("Light"), TEXT("Lamp")};
	const TCHAR* const HiddenEffectNameParts[] = {TEXT("Smoke"), TEXT("Fire"), TEXT("Steam"), TEXT("Dust")};
	const TCHAR* const ProjectileClassNameParts[] = {TEXT("Projectile"), TEXT("Shell"), TEXT("Rocket"), TEXT("Bullet")};
	// A unit that drove, turned or flew off during the warm-up (aircraft head for a deployed airbase) is put back.
	constexpr float RestoreDistanceCm = 50.f;
	constexpr float RestoreYawDegrees = 5.f;

	constexpr double MaxGameStartSeconds = 60.0;
	constexpr double HudBuildSeconds = 3.0;
	constexpr double PendingWriteSleepSeconds = 0.01;
	constexpr double MaxWriteFlushSeconds = 60.0;
	constexpr float MinResolutionScale = 0.05f;
	constexpr int32 MinRenderFrames = 2;
	constexpr float StreamAllResourcesTimeoutSeconds = 10.f;
	// Stage sublevels are named <persistent map>_<StageName>, e.g. RTS_Showcase_HeroAlpine.
	const TCHAR* const StageLevelPrefix = TEXT("RTS_Showcase_");
	constexpr double MaxStageLoadSeconds = 180.0;
	// Real seconds after a stage sublevel became visible: squads spawn their soldiers, buildings deploy.
	constexpr double StageLoadWarmupSeconds = 6.0;

	// Quality for stills: highest scalability, far LODs and foliage, sharper virtual shadow maps, no motion blur.
	const TCHAR* const CaptureConsoleVariables[][2] = {
		{TEXT("sg.ViewDistanceQuality"), TEXT("4")},
		{TEXT("sg.ShadowQuality"), TEXT("4")},
		{TEXT("sg.GlobalIlluminationQuality"), TEXT("4")},
		{TEXT("sg.ReflectionQuality"), TEXT("4")},
		{TEXT("sg.PostProcessQuality"), TEXT("4")},
		{TEXT("sg.TextureQuality"), TEXT("4")},
		{TEXT("sg.EffectsQuality"), TEXT("4")},
		{TEXT("sg.FoliageQuality"), TEXT("4")},
		{TEXT("sg.ShadingQuality"), TEXT("4")},
		{TEXT("r.ViewDistanceScale"), TEXT("10")},
		{TEXT("r.StaticMeshLODDistanceScale"), TEXT("0.25")},
		{TEXT("r.SkeletalMeshLODBias"), TEXT("-4")},
		{TEXT("foliage.LODDistanceScale"), TEXT("4")},
		{TEXT("grass.CullDistanceScale"), TEXT("3")},
		{TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"), TEXT("-1.5")},
		{TEXT("r.MotionBlurQuality"), TEXT("0")},
		{TEXT("r.ScreenPercentage"), TEXT("100")},
		{TEXT("r.Streaming.PoolSize"), TEXT("4000")},
		{TEXT("r.Streaming.FullyLoadUsedTextures"), TEXT("1")},
		// Clouds traced at full resolution without temporal reconstruction: the default posterised the skies.
		{TEXT("r.VolumetricRenderTarget.Mode"), TEXT("2")},
		{TEXT("r.VolumetricCloud.ViewRaySampleMaxCount"), TEXT("1024")},
		{TEXT("r.Tonemapper.GrainQuantization"), TEXT("1")},
	};
}

namespace
{
	bool GetDoesCameraMatchFilter(const ARTSShowcaseCamera& Camera, const FString& Filter)
	{
		if (Filter.IsEmpty() || Filter.Equals(RTSShowcaseConstants::AllFilter, ESearchCase::IgnoreCase))
		{
			return true;
		}
		const FString Pattern = Filter.Contains(TEXT("*")) || Filter.Contains(TEXT("?")) ? Filter : Filter + TEXT("*");
		const FString StageName = Camera.StageName.ToString();
		return StageName.Equals(Filter, ESearchCase::IgnoreCase)
			|| Camera.GetShotId().MatchesWildcard(Pattern)
			|| Camera.ShotName.MatchesWildcard(Pattern)
#if WITH_EDITOR
			|| Camera.GetActorLabel().MatchesWildcard(Pattern)
#endif
			;
	}

	bool GetShouldFreezeActor(const AActor& Actor)
	{
		return not Actor.IsA<AController>()
			&& not Actor.IsA<AInfo>()
			&& not Actor.IsA<APlayerCameraManager>()
			&& not Actor.IsA<AHUD>()
			&& not Actor.IsA<ACameraActor>()
			&& not Actor.IsA<ALight>();
	}

	bool GetIsHiddenEffect(const UNiagaraComponent& NiagaraComponent)
	{
		const UNiagaraSystem* const System = NiagaraComponent.GetAsset();
		if (not IsValid(System))
		{
			return false;
		}
		const FString SystemName = System->GetName();
		for (const TCHAR* const NamePart : RTSShowcaseConstants::HiddenEffectNameParts)
		{
			if (SystemName.Contains(NamePart))
			{
				return true;
			}
		}
		for (const TCHAR* const NamePart : RTSShowcaseConstants::KeptEffectNameParts)
		{
			if (SystemName.Contains(NamePart))
			{
				return false;
			}
		}
		return true;
	}

	bool GetIsProjectile(const AActor& Actor)
	{
		const FString ClassName = Actor.GetClass()->GetName();
		for (const TCHAR* const NamePart : RTSShowcaseConstants::ProjectileClassNameParts)
		{
			if (ClassName.Contains(NamePart))
			{
				return true;
			}
		}
		return false;
	}

	void HideUnitEffects(AActor& Actor)
	{
		if (GetIsProjectile(Actor) && not Actor.IsA<APawn>())
		{
			Actor.SetActorHiddenInGame(true);
			return;
		}
		// Cascade systems on level props (chimney smoke columns) are all hidden: stills carry no particle VFX.
		TInlineComponentArray<UParticleSystemComponent*> CascadeComponents(&Actor);
		for (UParticleSystemComponent* const CascadeComponent : CascadeComponents)
		{
			CascadeComponent->DeactivateImmediate();
			CascadeComponent->SetVisibility(false);
		}
		TInlineComponentArray<UNiagaraComponent*> NiagaraComponents(&Actor);
		for (UNiagaraComponent* const NiagaraComponent : NiagaraComponents)
		{
			if (not GetIsHiddenEffect(*NiagaraComponent))
			{
				continue;
			}
			NiagaraComponent->DeactivateImmediate();
			NiagaraComponent->SetVisibility(false);
		}
	}

	// Health bars float over every unit, the fog of war would hide anything outside the player's vision and a
	// level-placed nomadic truck is still playing its deploy animation after the warm-up.
	void PrepareUnitForStill(AActor& Actor)
	{
		if (const UHealthComponent* const HealthComponent = Actor.FindComponentByClass<UHealthComponent>())
		{
			HealthComponent->MakeHealthBarInvisible();
		}
		if (UFowComp* const FowComponent = Actor.FindComponentByClass<UFowComp>())
		{
			FowComponent->StopFowParticipation();
			// Enemy-owned units (all Soviet units) were already hidden by the fog; leaving the fog does not show them.
			Actor.SetActorHiddenInGame(false);
			TArray<AActor*> AttachedActors;
			Actor.GetAttachedActors(AttachedActors, true, true);
			for (AActor* const AttachedActor : AttachedActors)
			{
				AttachedActor->SetActorHiddenInGame(false);
			}
		}
#if RTS_WITH_MAP_TESTS
		if (ANomadicVehicle* const Nomadic = Cast<ANomadicVehicle>(&Actor))
		{
			if (not Nomadic->ShippingTest_ForceFinishConversionToBuilding())
			{
				UE_LOG(LogRTSShowcase, Warning, TEXT("RTS_SHOWCASE %s is not a building (status %d)"), *Actor.GetName(),
				       static_cast<int32>(Nomadic->GetNomadicStatus()));
			}
		}
#endif
		// After the deploy: finishing a building starts its smoke systems (refinery radixite smoke).
		HideUnitEffects(Actor);
	}

	// Units out of the fog reveal nothing, so the fog post process would darken the stage backgrounds.
	void DisableFogOfWarPostProcess(UWorld& World)
	{
		for (TActorIterator<AFowManager> It(&World); It; ++It)
		{
			TInlineComponentArray<UPostProcessComponent*> PostProcesses(*It);
			for (UPostProcessComponent* const PostProcess : PostProcesses)
			{
				PostProcess->bEnabled = false;
			}
		}
	}

	FRTSShowcaseSessionSettings ReadCommandLineSettings()
	{
		using namespace RTSShowcaseConstants;
		FRTSShowcaseSessionSettings Settings;
		const TCHAR* const CommandLine = FCommandLine::Get();
		FParse::Value(CommandLine, ScaleSwitch, Settings.M_ResolutionScale);
		FParse::Value(CommandLine, FramesSwitch, Settings.M_RenderFrames);
		FParse::Value(CommandLine, SettleSwitch, Settings.M_SettleSeconds);
		FParse::Value(CommandLine, WarmupSwitch, Settings.M_WarmupSeconds);
		FParse::Value(CommandLine, DirectorySwitch, Settings.M_OutputDirectory);
		Settings.bM_FreezeActors = not FParse::Param(CommandLine, NoFreezeSwitch);
		Settings.bM_DisableTextureStreaming = FParse::Param(CommandLine, NoStreamingSwitch);
		return Settings;
	}

	// RTS.Showcase.Shot <all|stage|shot id|wildcard> [resolution scale]
	void RunShowcaseShotCommand(const TArray<FString>& Arguments, UWorld* World)
	{
		URTSShowcaseCaptureSubsystem* const Subsystem = IsValid(World)
			                                                ? World->GetSubsystem<URTSShowcaseCaptureSubsystem>()
			                                                : nullptr;
		if (not IsValid(Subsystem))
		{
			UE_LOG(LogRTSShowcase, Error, TEXT("RTS_SHOWCASE only works in a running game world (PIE or -game)."));
			return;
		}
		FRTSShowcaseSessionSettings Settings = ReadCommandLineSettings();
		if (Arguments.Num() > 1)
		{
			Settings.M_ResolutionScale = FCString::Atof(*Arguments[1]);
		}
		// Units are already up when someone types the command.
		Settings.M_WarmupSeconds = 0.0;
		Subsystem->RequestShots(Arguments.Num() > 0 ? Arguments[0] : FString(RTSShowcaseConstants::AllFilter), Settings);
	}

	void RunShowcaseListCommand(const TArray<FString>& Arguments, UWorld* World)
	{
		const URTSShowcaseCaptureSubsystem* const Subsystem = IsValid(World)
			                                                      ? World->GetSubsystem<URTSShowcaseCaptureSubsystem>()
			                                                      : nullptr;
		if (IsValid(Subsystem))
		{
			Subsystem->LogCameras();
		}
	}

#if RTS_WITH_MAP_TESTS
	FAutoConsoleCommandWithWorldAndArgs GShowcaseShotCommand(
		TEXT("RTS.Showcase.Shot"),
		TEXT("Renders showcase camera presets to Saved/Showcase. Args: <all|stage|shot|wildcard> [resolution scale]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunShowcaseShotCommand));

	FAutoConsoleCommandWithWorldAndArgs GShowcaseListCommand(
		TEXT("RTS.Showcase.List"),
		TEXT("Logs every showcase camera preset of the current map."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunShowcaseListCommand));
#endif
}

bool URTSShowcaseCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
#else
	return false;
#endif
}

void URTSShowcaseCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	SpawnMarkedUnits(InWorld);
	RecordPlacedPawns(InWorld);
	FString Filter;
	if (not FParse::Value(FCommandLine::Get(), RTSShowcaseConstants::ShotsSwitch, Filter))
	{
		return;
	}
	FRTSShowcaseSessionSettings Settings = ReadCommandLineSettings();
	Settings.bM_QuitWhenDone = true;
	// Error dialogs are answered automatically in unattended runs, but each one stalls a frame; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	if (RequestShots(Filter, Settings) == 0)
	{
		FPlatformMisc::RequestExit(false, TEXT("ShowcaseCapture"));
	}
}

void URTSShowcaseCaptureSubsystem::SpawnMarkedUnits(UWorld& World)
{
	TArray<TPair<FString, FTransform>> UnitsToSpawn;
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			FString ClassPath;
			if (Tag.ToString().Split(RTSShowcaseConstants::SpawnTagPrefix, nullptr, &ClassPath))
			{
				UnitsToSpawn.Emplace(ClassPath, It->GetActorTransform());
			}
		}
		// Each stage load spawns only its own markers once.
		It->Tags.RemoveAll([](const FName& Tag)
		{
			return Tag.ToString().StartsWith(RTSShowcaseConstants::SpawnTagPrefix);
		});
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const TPair<FString, FTransform>& Unit : UnitsToSpawn)
	{
		UClass* const UnitClass = StaticLoadClass(AActor::StaticClass(), nullptr, *Unit.Key);
		if (UnitClass == nullptr || World.SpawnActor<AActor>(UnitClass, Unit.Value, SpawnParameters) == nullptr)
		{
			UE_LOG(LogRTSShowcase, Error, TEXT("RTS_SHOWCASE could not spawn marked unit %s"), *Unit.Key);
		}
	}
	if (not UnitsToSpawn.IsEmpty())
	{
		UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE spawned %d marked units"), UnitsToSpawn.Num());
	}
}

void URTSShowcaseCaptureSubsystem::RecordPlacedPawns(UWorld& World)
{
	M_PlacedPawnTransforms.Reset();
	for (TActorIterator<APawn> It(&World); It; ++It)
	{
		M_PlacedPawnTransforms.Add(*It, It->GetActorTransform());
	}
	// Turrets start facing along the hull; idle scanning swings them away during the warm-up.
	M_PlacedTurretRotations.Reset();
	for (TActorIterator<ACPPTurretsMaster> It(&World); It; ++It)
	{
		const USceneComponent* const TurretRoot = It->GetRootComponent();
		if (IsValid(TurretRoot) && IsValid(TurretRoot->GetAttachParent()))
		{
			M_PlacedTurretRotations.Add(*It, TurretRoot->GetRelativeRotation());
		}
	}
}

int32 URTSShowcaseCaptureSubsystem::RestorePlacedPawns() const
{
	int32 RestoredCount = 0;
	for (const TPair<TWeakObjectPtr<APawn>, FTransform>& Placed : M_PlacedPawnTransforms)
	{
		APawn* const Pawn = Placed.Key.Get();
		if (not IsValid(Pawn))
		{
			continue;
		}
		const float Distance = FVector::Dist(Pawn->GetActorLocation(), Placed.Value.GetLocation());
		const float YawDelta = FMath::Abs(FRotator::NormalizeAxis(
			Pawn->GetActorRotation().Yaw - Placed.Value.Rotator().Yaw));
		if (Distance < RTSShowcaseConstants::RestoreDistanceCm && YawDelta < RTSShowcaseConstants::RestoreYawDegrees)
		{
			continue;
		}
		Pawn->SetActorLocationAndRotation(Placed.Value.GetLocation(), Placed.Value.Rotator(), false, nullptr,
		                                  ETeleportType::TeleportPhysics);
		++RestoredCount;
	}
	for (const TPair<TWeakObjectPtr<ACPPTurretsMaster>, FRotator>& Placed : M_PlacedTurretRotations)
	{
		USceneComponent* const TurretRoot = Placed.Key.IsValid() ? Placed.Key->GetRootComponent() : nullptr;
		if (IsValid(TurretRoot))
		{
			TurretRoot->SetRelativeRotation(Placed.Value);
		}
	}
	return RestoredCount;
}

void URTSShowcaseCaptureSubsystem::Deinitialize()
{
	if (M_EndFrameDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(M_EndFrameDelegateHandle);
		M_EndFrameDelegateHandle.Reset();
	}
	DestroyCaptureViewport();
	RestoreConsoleVariables();
	Super::Deinitialize();
}

TStatId URTSShowcaseCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URTSShowcaseCaptureSubsystem, STATGROUP_Tickables);
}

int32 URTSShowcaseCaptureSubsystem::RequestShots(const FString& Filter, const FRTSShowcaseSessionSettings& Settings)
{
	if (M_Phase != ERTSShowcasePhase::Idle)
	{
		UE_LOG(LogRTSShowcase, Warning, TEXT("RTS_SHOWCASE a batch is still running; request ignored."));
		return 0;
	}
	TArray<ARTSShowcaseCamera*> Cameras;
	for (TActorIterator<ARTSShowcaseCamera> It(GetWorld()); It; ++It)
	{
		if (GetDoesCameraMatchFilter(**It, Filter))
		{
			Cameras.Add(*It);
		}
	}
	Cameras.Sort([](const ARTSShowcaseCamera& Left, const ARTSShowcaseCamera& Right)
	{
		return Left.GetShotId() < Right.GetShotId();
	});
	if (Cameras.IsEmpty())
	{
		UE_LOG(LogRTSShowcase, Error, TEXT("RTS_SHOWCASE no camera matches '%s'"), *Filter);
		return 0;
	}
	M_Settings = Settings;
	M_Settings.M_ResolutionScale = FMath::Max(RTSShowcaseConstants::MinResolutionScale, Settings.M_ResolutionScale);
	M_Settings.M_RenderFrames = FMath::Max(RTSShowcaseConstants::MinRenderFrames, Settings.M_RenderFrames);
	if (M_Settings.M_OutputDirectory.IsEmpty())
	{
		M_Settings.M_OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Showcase"),
		                                               FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	}
	M_PendingCameras.Reset();
	for (ARTSShowcaseCamera* const Camera : Cameras)
	{
		M_PendingCameras.Add(Camera);
	}
	M_WrittenShotCount = 0;
	UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE queued %d shots (filter '%s', scale %.2f) -> %s"),
	       Cameras.Num(), *Filter, M_Settings.M_ResolutionScale, *M_Settings.M_OutputDirectory);
	SetPhase(ERTSShowcasePhase::WaitingForGame);
	return Cameras.Num();
}

void URTSShowcaseCaptureSubsystem::LogCameras() const
{
	for (TActorIterator<ARTSShowcaseCamera> It(GetWorld()); It; ++It)
	{
		const FIntPoint Resolution = It->GetOutputResolution(1.f);
		UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE camera %s %dx%d  %s"), *It->GetShotId(), Resolution.X,
		       Resolution.Y, *It->Notes);
	}
}

void URTSShowcaseCaptureSubsystem::Tick(const float DeltaTime)
{
	switch (M_Phase)
	{
	case ERTSShowcasePhase::WaitingForGame:
		TickWaitingForGame();
		break;
	case ERTSShowcasePhase::LoadingStage:
		TickLoadingStage();
		break;
	case ERTSShowcasePhase::WarmingUp:
		TickWarmingUp();
		break;
	case ERTSShowcasePhase::Settling:
		TickSettling();
		break;
	case ERTSShowcasePhase::Rendering:
		KeepViewOnActiveCamera();
		break;
	case ERTSShowcasePhase::Finishing:
		TickFinishing();
		break;
	default:
		break;
	}
}

void URTSShowcaseCaptureSubsystem::SetPhase(const ERTSShowcasePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double URTSShowcaseCaptureSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void URTSShowcaseCaptureSubsystem::TickWaitingForGame()
{
	if (GetSecondsInPhase() < RTSShowcaseConstants::HudBuildSeconds)
	{
		return;
	}
	if (TryStartGame())
	{
		ApplyCaptureConsoleVariables();
		if (not BeginNextShot())
		{
			SetPhase(ERTSShowcasePhase::Finishing);
		}
		return;
	}
	if (GetSecondsInPhase() > RTSShowcaseConstants::MaxGameStartSeconds)
	{
		UE_LOG(LogRTSShowcase, Error, TEXT("RTS_SHOWCASE no player controller after %.0f s"), GetSecondsInPhase());
		M_PendingCameras.Reset();
		SetPhase(ERTSShowcasePhase::Finishing);
	}
}

bool URTSShowcaseCaptureSubsystem::TryStartGame()
{
	UWorld* const World = GetWorld();
	APlayerController* const PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	if (not IsValid(PlayerController))
	{
		return false;
	}
	M_PlayerController = PlayerController;
	if (bM_HasStartedGame)
	{
		return true;
	}
	bM_HasStartedGame = true;
	UPlayerStartGameControl* const StartGameControl = PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	DisableFogOfWarPostProcess(*World);
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(RTSShowcaseConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	return true;
}

void URTSShowcaseCaptureSubsystem::TickWarmingUp()
{
	if (GetSecondsInPhase() < M_WarmupSecondsForStage)
	{
		return;
	}
	if (not bM_HasPreparedActors)
	{
		PrepareActorsForStills();
	}
	StartActiveShot();
}

bool URTSShowcaseCaptureSubsystem::RequestStageLevel(const FName StageName)
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FString WantedName = FString(RTSShowcaseConstants::StageLevelPrefix) + StageName.ToString();
	bool bFoundStageLevel = false;
	for (ULevelStreaming* const StreamingLevel : World->GetStreamingLevels())
	{
		const FString LevelName = IsValid(StreamingLevel)
			                          ? FPackageName::GetShortName(StreamingLevel->GetWorldAssetPackageName())
			                          : FString();
		if (not LevelName.Contains(RTSShowcaseConstants::StageLevelPrefix))
		{
			continue;
		}
		// PIE prefixes package names (UEDPIE_0_...); the stage name ends the short name.
		const bool bIsWanted = LevelName.EndsWith(WantedName);
		bFoundStageLevel |= bIsWanted;
		StreamingLevel->SetShouldBeLoaded(bIsWanted);
		StreamingLevel->SetShouldBeVisible(bIsWanted);
	}
	return bFoundStageLevel;
}

bool URTSShowcaseCaptureSubsystem::GetIsRequestedStageLevelReady() const
{
	const UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	for (const ULevelStreaming* const StreamingLevel : World->GetStreamingLevels())
	{
		const FString LevelName = IsValid(StreamingLevel)
			                          ? FPackageName::GetShortName(StreamingLevel->GetWorldAssetPackageName())
			                          : FString();
		if (not LevelName.Contains(RTSShowcaseConstants::StageLevelPrefix))
		{
			continue;
		}
		const bool bWanted = StreamingLevel->ShouldBeLoaded();
		const bool bReady = bWanted ? StreamingLevel->IsLevelVisible() : not StreamingLevel->IsLevelLoaded();
		if (not bReady)
		{
			return false;
		}
	}
	return true;
}

void URTSShowcaseCaptureSubsystem::TickLoadingStage()
{
	if (not GetIsRequestedStageLevelReady())
	{
		if (GetSecondsInPhase() > RTSShowcaseConstants::MaxStageLoadSeconds)
		{
			UE_LOG(LogRTSShowcase, Error, TEXT("RTS_SHOWCASE stage %s did not stream in"), *M_LoadedStage.ToString());
			SetPhase(ERTSShowcasePhase::Finishing);
		}
		return;
	}
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// The previous stage's units and textures go before the new stage renders at full size.
	GEngine->ForceGarbageCollection(true);
	SpawnMarkedUnits(*World);
	RecordPlacedPawns(*World);
	DisableFogOfWarPostProcess(*World);
	bM_HasPreparedActors = false;
	M_WarmupSecondsForStage = FMath::Max(M_Settings.M_WarmupSeconds, RTSShowcaseConstants::StageLoadWarmupSeconds);
	UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE stage %s streamed in after %.1f s"), *M_LoadedStage.ToString(),
	       GetSecondsInPhase());
	SetPhase(ERTSShowcasePhase::WarmingUp);
}

void URTSShowcaseCaptureSubsystem::PrepareActorsForStills()
{
	bM_HasPreparedActors = true;
	const int32 RestoredCount = RestorePlacedPawns();
	int32 FrozenCount = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		PrepareUnitForStill(**It);
		if (not M_Settings.bM_FreezeActors || not GetShouldFreezeActor(**It))
		{
			continue;
		}
		// Ticks, movement, animation and child turrets stop; the world clock keeps running for exposure and Lumen.
		It->CustomTimeDilation = 0.f;
		++FrozenCount;
	}
	UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE froze %d actors, put %d moved units back"), FrozenCount,
	       RestoredCount);
}

void URTSShowcaseCaptureSubsystem::ApplyCaptureConsoleVariables()
{
	for (const auto& Variable : RTSShowcaseConstants::CaptureConsoleVariables)
	{
		SetConsoleVariableForBatch(Variable[0], Variable[1]);
	}
	if (M_Settings.bM_DisableTextureStreaming)
	{
		// Streaming sizes mips for the small game window, which leaves an 8K still blurry.
		SetConsoleVariableForBatch(TEXT("r.TextureStreaming"), TEXT("0"));
	}
}

void URTSShowcaseCaptureSubsystem::SetConsoleVariableForBatch(const TCHAR* Name, const TCHAR* Value)
{
	IConsoleVariable* const Variable = IConsoleManager::Get().FindConsoleVariable(Name);
	if (Variable == nullptr)
	{
		return;
	}
	if (not M_SavedConsoleVariables.Contains(Name))
	{
		M_SavedConsoleVariables.Add(Name, Variable->GetString());
	}
	Variable->Set(Value, ECVF_SetByConsole);
}

void URTSShowcaseCaptureSubsystem::RestoreConsoleVariables()
{
	for (const TPair<FString, FString>& Saved : M_SavedConsoleVariables)
	{
		if (IConsoleVariable* const Variable = IConsoleManager::Get().FindConsoleVariable(*Saved.Key))
		{
			Variable->Set(*Saved.Value, ECVF_SetByConsole);
		}
	}
	M_SavedConsoleVariables.Reset();
}

bool URTSShowcaseCaptureSubsystem::BeginNextShot()
{
	while (not M_PendingCameras.IsEmpty())
	{
		const TWeakObjectPtr<ARTSShowcaseCamera> Camera = M_PendingCameras[0];
		M_PendingCameras.RemoveAt(0);
		if (not Camera.IsValid())
		{
			continue;
		}
		M_ActiveCamera = Camera;
		const bool bStageChanged = Camera->StageName != M_LoadedStage;
		if (bStageChanged && RequestStageLevel(Camera->StageName))
		{
			M_LoadedStage = Camera->StageName;
			UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE streaming stage %s"), *M_LoadedStage.ToString());
			SetPhase(ERTSShowcasePhase::LoadingStage);
			return true;
		}
		if (not bM_HasPreparedActors)
		{
			// A map without stage sublevels: one warm-up for everything.
			M_LoadedStage = Camera->StageName;
			M_WarmupSecondsForStage = M_Settings.M_WarmupSeconds;
			SetPhase(ERTSShowcasePhase::WarmingUp);
			return true;
		}
		StartActiveShot();
		return true;
	}
	return false;
}

void URTSShowcaseCaptureSubsystem::StartActiveShot()
{
	if (not M_ActiveCamera.IsValid())
	{
		if (not BeginNextShot())
		{
			SetPhase(ERTSShowcasePhase::Finishing);
		}
		return;
	}
	const int32 LightingActors = FRTSShowcaseStageLighting::ActivateStage(GetWorld(), M_ActiveCamera->StageName, false);
	M_OutputResolution = M_ActiveCamera->GetOutputResolution(M_Settings.M_ResolutionScale);
	KeepViewOnActiveCamera();
	UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE shot %s %dx%d (stage lights %d)"), *M_ActiveCamera->GetShotId(),
	       M_OutputResolution.X, M_OutputResolution.Y, LightingActors);
	SetPhase(ERTSShowcasePhase::Settling);
}

void URTSShowcaseCaptureSubsystem::KeepViewOnActiveCamera() const
{
	if (not M_ActiveCamera.IsValid() || not GetIsValidPlayerController())
	{
		return;
	}
	if (M_PlayerController->GetViewTarget() != M_ActiveCamera.Get())
	{
		M_PlayerController->SetViewTarget(M_ActiveCamera.Get());
	}
}

void URTSShowcaseCaptureSubsystem::TickSettling()
{
	KeepViewOnActiveCamera();
	AddStillViewToTextureStreaming();
	if (GetSecondsInPhase() < M_Settings.M_SettleSeconds)
	{
		return;
	}
	// Mips requested for the still's size must be resident before the frames that converge temporal AA.
	IStreamingManager::Get().StreamAllResources(RTSShowcaseConstants::StreamAllResourcesTimeoutSeconds);
	if (not CreateCaptureViewport())
	{
		UE_LOG(LogRTSShowcase, Error, TEXT("RTS_SHOWCASE could not create a %dx%d capture viewport"),
		       M_OutputResolution.X, M_OutputResolution.Y);
		if (not BeginNextShot())
		{
			SetPhase(ERTSShowcasePhase::Finishing);
		}
		return;
	}
	M_RenderedFrameCount = 0;
	M_EndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &URTSShowcaseCaptureSubsystem::OnEndFrame);
	SetPhase(ERTSShowcasePhase::Rendering);
}

void URTSShowcaseCaptureSubsystem::AddStillViewToTextureStreaming() const
{
	if (not M_ActiveCamera.IsValid())
	{
		return;
	}
	const UCineCameraComponent* const CameraComponent = M_ActiveCamera->GetCineCameraComponent();
	if (not IsValid(CameraComponent))
	{
		return;
	}
	// The game window is small; streaming must size mips for the still, not for the window.
	const float ScreenSize = static_cast<float>(FMath::Max(M_OutputResolution.X, M_OutputResolution.Y));
	const float HalfFovRadians = FMath::DegreesToRadians(FMath::Max(1.f, CameraComponent->FieldOfView) * 0.5f);
	IStreamingManager::Get().AddViewInformation(CameraComponent->GetComponentLocation(), ScreenSize,
	                                            ScreenSize / FMath::Tan(HalfFovRadians));
}

bool URTSShowcaseCaptureSubsystem::CreateCaptureViewport()
{
	DestroyCaptureViewport();
	const UWorld* const World = GetWorld();
	UGameViewportClient* const GameViewportClient = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (not IsValid(GameViewportClient) || GameViewportClient->Viewport == nullptr)
	{
		return false;
	}
	M_CaptureViewport = MakeUnique<FRTSSteamCaptureViewport>(GameViewportClient);
	if (not M_CaptureViewport->Initialize(M_OutputResolution, *GameViewportClient->Viewport))
	{
		M_CaptureViewport.Reset();
		return false;
	}
	// A fresh view state per shot: no temporal history from the previous camera.
	M_CaptureViewExtension = FSceneViewExtensions::NewExtension<FRTSSteamCaptureViewExtension>(
		M_CaptureViewport.Get(),
		World->GetFeatureLevel());
	return M_CaptureViewExtension.IsValid();
}

void URTSShowcaseCaptureSubsystem::DestroyCaptureViewport()
{
	if (M_CaptureViewport.IsValid())
	{
		M_CaptureViewport->Release();
	}
	M_CaptureViewExtension.Reset();
	M_CaptureViewport.Reset();
}

void URTSShowcaseCaptureSubsystem::OnEndFrame()
{
	if (M_Phase != ERTSShowcasePhase::Rendering)
	{
		return;
	}
	++M_RenderedFrameCount;
	const bool bIsLastFrame = M_RenderedFrameCount >= M_Settings.M_RenderFrames;
	TArray<FColor> Pixels;
	if (not DrawCaptureFrame(bIsLastFrame, Pixels))
	{
		UE_LOG(LogRTSShowcase, Error, TEXT("RTS_SHOWCASE frame %d of %s failed"), M_RenderedFrameCount,
		       M_ActiveCamera.IsValid() ? *M_ActiveCamera->GetShotId() : TEXT("?"));
		Pixels.Reset();
		FinishActiveShot(MoveTemp(Pixels));
		return;
	}
	if (bIsLastFrame)
	{
		FinishActiveShot(MoveTemp(Pixels));
	}
}

bool URTSShowcaseCaptureSubsystem::DrawCaptureFrame(const bool bReadPixels, TArray<FColor>& OutPixels)
{
	UWorld* const World = GetWorld();
	UGameViewportClient* const GameViewportClient = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (not IsValid(GameViewportClient) || not M_CaptureViewport.IsValid() || not M_CaptureViewport->GetIsInitialized())
	{
		return false;
	}
	M_CaptureViewport->EnqueueBeginRenderFrame(false);
	FCanvas CaptureCanvas(
		M_CaptureViewport.Get(),
		nullptr,
		World,
		World->GetFeatureLevel(),
		FCanvas::CDM_DeferDrawing,
		GameViewportClient->ShouldDPIScaleSceneCanvas() ? GameViewportClient->GetDPIScale() : 1.0f);
	CaptureCanvas.SetRenderTargetRect(FIntRect(FIntPoint::ZeroValue, M_OutputResolution));
	GameViewportClient->Draw(M_CaptureViewport.Get(), &CaptureCanvas);
	CaptureCanvas.Flush_GameThread();
	if (FCanvas* const DebugCanvas = M_CaptureViewport->GetDebugCanvas())
	{
		DebugCanvas->SetAllowedModes(FCanvas::Allow_DeleteOnRender);
		DebugCanvas->Flush_GameThread(true);
	}
	M_CaptureViewport->EnqueueEndRenderFrame(false, false);
	if (not bReadPixels)
	{
		return true;
	}
	// The viewport already holds display-encoded colour: no second sRGB conversion (see SteamCapture/AGENTS.md).
	FReadSurfaceDataFlags ReadSurfaceFlags(RCM_UNorm, CubeFace_MAX);
	ReadSurfaceFlags.SetLinearToGamma(false);
	if (not M_CaptureViewport->ReadPixels(OutPixels, ReadSurfaceFlags)
		|| OutPixels.Num() != M_OutputResolution.X * M_OutputResolution.Y)
	{
		return false;
	}
	for (FColor& Pixel : OutPixels)
	{
		Pixel.A = MAX_uint8;
	}
	return true;
}

void URTSShowcaseCaptureSubsystem::FinishActiveShot(TArray<FColor>&& Pixels)
{
	FCoreDelegates::OnEndFrame.Remove(M_EndFrameDelegateHandle);
	M_EndFrameDelegateHandle.Reset();
	if (not Pixels.IsEmpty() && M_ActiveCamera.IsValid())
	{
		const FString FilePath = FPaths::Combine(M_Settings.M_OutputDirectory,
		                                         FPaths::MakeValidFileName(M_ActiveCamera->GetShotId()) + TEXT(".png"));
		M_FrameWriter.WritePngFrameAsync(FilePath, M_OutputResolution.X, M_OutputResolution.Y, MoveTemp(Pixels));
		++M_WrittenShotCount;
		UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE wrote %s"), *FilePath);
	}
	DestroyCaptureViewport();
	if (not BeginNextShot())
	{
		SetPhase(ERTSShowcasePhase::Finishing);
	}
}

void URTSShowcaseCaptureSubsystem::TickFinishing()
{
	const double FlushStart = FPlatformTime::Seconds();
	while (M_FrameWriter.GetPendingWriteCount() > 0
		&& FPlatformTime::Seconds() - FlushStart < RTSShowcaseConstants::MaxWriteFlushSeconds)
	{
		FPlatformProcess::Sleep(RTSShowcaseConstants::PendingWriteSleepSeconds);
	}
	RestoreConsoleVariables();
	M_ActiveCamera.Reset();
	UE_LOG(LogRTSShowcase, Display, TEXT("RTS_SHOWCASE done: %d stills (%d failed writes) in %s"), M_WrittenShotCount,
	       M_FrameWriter.GetFailedWriteCount(), *M_Settings.M_OutputDirectory);
	SetPhase(ERTSShowcasePhase::Idle);
	if (M_Settings.bM_QuitWhenDone)
	{
		FPlatformMisc::RequestExit(false, TEXT("ShowcaseCapture"));
	}
}

bool URTSShowcaseCaptureSubsystem::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerController"),
		TEXT("URTSShowcaseCaptureSubsystem::GetIsValidPlayerController"),
		this
	);
	return false;
}
