// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "GMRDirector.h"

#include "ActiveSound.h"
#include "AudioDevice.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSubmix.h"
#include "AudioThread.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/Button.h"
#include "Components/PostProcessComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MoviePipeline.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundSubmix.h"
#include "UObject/UObjectIterator.h"

#include "GMRAudioCapture.h"
#include "GMRRunner.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogGMR, Log, All);

namespace GMRDirectorPrivate
{
	// Game seconds the HUD and the start widget need before the start can be confirmed.
	constexpr double MinStartSeconds = 2.0;
	constexpr double MaxStartSeconds = 30.0;
	constexpr float GroundTraceHalfHeight = 50000.f;
	constexpr int32 VoiceSampleEveryFrames = 5;
	constexpr float SensorLongSide = 23.76f;
	constexpr float SensorShortSide = 13.365f;
	constexpr float MinLensFocal = 4.f;
	constexpr float MaxLensFocal = 1000.f;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const FName RecordedUnitTag(TEXT("GMR_Recorded"));

	const TCHAR* const MusicSoundClass = TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_Music.RTS_SClass_Music");
	const TCHAR* const UISoundClass = TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_UI.RTS_SClass_UI");
	const TCHAR* const AnnouncerSoundClass = TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_Announcer.RTS_SClass_Announcer");
	const TCHAR* const VoiceLineSoundClass = TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_VoiceLine.RTS_SClass_VoiceLine");
	const TCHAR* const CinematicSoundClass = TEXT("/Game/RTS_Survival/Sounds/Settings/SoundClasses/RTS_SClass_CinematicAndTransmissions.RTS_SClass_CinematicAndTransmissions");
	const TCHAR* const VoiceLineClassName = TEXT("RTS_SClass_VoiceLine");

	bool GetIsLiveActor(const AActor* Actor)
	{
		return IsValid(Actor) && not Actor->IsActorBeingDestroyed() && const_cast<AActor*>(Actor)->IsUnitAlive();
	}

	/** @brief Squads stand where their living soldiers are; the controller actor can lag behind them. */
	bool GetUnitLocationAndHeading(const AActor* Actor, FVector& OutLocation, FVector& OutHeading)
	{
		const ASquadController* Squad = Cast<ASquadController>(Actor);
		if (Squad == nullptr)
		{
			OutLocation = Actor->GetActorLocation();
			const FVector Velocity = Actor->GetVelocity();
			OutHeading = Velocity.Size2D() > 50.f ? Velocity.GetSafeNormal2D() : Actor->GetActorForwardVector();
			return true;
		}
		FVector Sum = FVector::ZeroVector;
		FVector HeadingSum = FVector::ZeroVector;
		int32 Living = 0;
		for (ASquadUnit* Soldier : const_cast<ASquadController*>(Squad)->GetSquadUnitsChecked())
		{
			if (not GetIsLiveActor(Soldier))
			{
				continue;
			}
			Sum += Soldier->GetActorLocation();
			HeadingSum += Soldier->GetActorForwardVector();
			++Living;
		}
		if (Living == 0)
		{
			return false;
		}
		OutLocation = Sum / Living;
		OutHeading = HeadingSum.GetSafeNormal2D();
		return true;
	}
}

AGMRDirector::AGMRDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	// After gameplay queued its sounds, so the non-spatialized override reaches them before their first update.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	M_VoiceLinesHeard = MakeShared<TMap<FString, float>, ESPMode::ThreadSafe>();
	M_VoiceLinesLock = MakeShared<FCriticalSection, ESPMode::ThreadSafe>();
}

void AGMRDirector::InitDirector(const FGMRJob& Job)
{
	M_Job = Job;
	M_SortedEvents = Job.Events;
	M_SortedEvents.StableSort([](const FGMREvent& A, const FGMREvent& B) { return A.Time < B.Time; });
	M_Manifest.JobName = Job.JobName;
	M_Manifest.Aspect = Job.Aspect;
	M_Manifest.Fps = Job.Fps;
}

void AGMRDirector::BeginPlay()
{
	Super::BeginPlay();
	M_PhaseStartWorldSeconds = GetWorld()->GetTimeSeconds();
	if (ACPPGameState* const GameState = GetWorld()->GetGameState<ACPPGameState>())
	{
		GameState->SetVehicleBalance(ERTSVehicleBalance::Realistic);
	}
	M_Manifest.bRealisticBalance = ACPPGameState::GetIsRealisticVehicleBalance(this);
	UE_LOG(LogGMR, Display, TEXT("GMR_DIRECTOR_BEGIN job=%s realistic=%s"), *M_Job.JobName,
	       M_Manifest.bRealisticBalance ? TEXT("true") : TEXT("false"));
	StartSubmixCapture();
}

void AGMRDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSubmixCapture();
	if (bM_HasSuppressedDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_HasSuppressedDialogs = false;
	}
	Super::EndPlay(EndPlayReason);
}

void AGMRDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	KeepAudioUnmuted();
	ForceSoundsNonSpatialized();
	if (M_Phase != EGMRDirectorPhase::WaitingForController && M_Phase != EGMRDirectorPhase::Finished)
	{
		KeepGameRunning();
	}
	if (bM_SetupEventsDone && M_Phase != EGMRDirectorPhase::Finished)
	{
		KeepUnitsReady();
		RecordUnitLocations();
		SuppressUnwantedActors();
		TickInstantDeploys();
		TickPendingExpansions();
		TickPendingClaims();
		LogTrenchProgress();
	}
	switch (M_Phase)
	{
	case EGMRDirectorPhase::WaitingForController: TickWaitingForController();
		break;
	case EGMRDirectorPhase::WaitingForStart: TickWaitingForStart();
		break;
	case EGMRDirectorPhase::WaitingForTimeline: TickWaitingForTimeline();
		break;
	case EGMRDirectorPhase::Running: TickRunning();
		break;
	case EGMRDirectorPhase::Finished:
		break;
	}
}

void AGMRDirector::TickWaitingForController()
{
	if (not bM_HasSuppressedDialogs)
	{
		RTSFunctionLibrary::PushModalDialogSuppression();
		bM_HasSuppressedDialogs = true;
	}
	M_PlayerController = Cast<ACPPController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (not M_PlayerController.IsValid())
	{
		return;
	}
	M_Phase = EGMRDirectorPhase::WaitingForStart;
	M_PhaseStartWorldSeconds = GetWorld()->GetTimeSeconds();
}

void AGMRDirector::TickWaitingForStart()
{
	using namespace GMRDirectorPrivate;
	if (not GetIsValidPlayerController())
	{
		return;
	}
	const double Waited = GetWorld()->GetTimeSeconds() - M_PhaseStartWorldSeconds;
	UPlayerStartGameControl* StartControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	const bool bControlReady = IsValid(StartControl) && StartControl->HasBegunPlay();
	if (Waited >= MinStartSeconds && bControlReady && StartControl->GetIsWaitingForPlayerToStart())
	{
		StartControl->PlayerStartedGame();
		UE_LOG(LogGMR, Display, TEXT("GMR_START_CONFIRMED after=%.1fs"), Waited);
	}
	const bool bStarted = bControlReady && not StartControl->GetIsWaitingForPlayerToStart();
	if ((not bStarted || Waited < MinStartSeconds) && Waited < MaxStartSeconds)
	{
		return;
	}
	if (not bStarted)
	{
		AddManifestWarning(TEXT("start dialog not confirmed, continuing"));
	}
	PrepareScene();
	M_Phase = EGMRDirectorPhase::WaitingForTimeline;
}

void AGMRDirector::PrepareScene()
{
	PrepareScene_ResolveAnchor();
	PrepareScene_NeutraliseMap();
	HideGameUI();
	PushSoundMix();
	DisableFogOfWar();
	DisableUnitOptimisation();
	if (GetIsValidPlayerController())
	{
		M_PlayerController->SetSuppressRegularVoiceLines(false);
		// Screen-space weather (ash, dust) rides on the RTS camera pawn, which follows the cine camera here:
		// sprites a metre from the lens read as squares, so the pawn's effects are switched off.
		if (APawn* const Pawn = M_PlayerController->GetPawn())
		{
			TInlineComponentArray<UFXSystemComponent*> Effects(Pawn);
			for (UFXSystemComponent* const Effect : Effects)
			{
				UE_LOG(LogGMR, Display, TEXT("GMR_PAWN_FX_OFF %s"), *Effect->GetName());
				Effect->Deactivate();
				Effect->SetVisibility(false);
			}
		}
	}
	PrepareScene_SpawnSky();
	PrepareScene_SpawnCamera();
	// Setup events (negative times) run now, so their units settle during the rest of the warmup.
	RunDueEvents(0.f - KINDA_SMALL_NUMBER, GetTimelineFrame());
	bM_SetupEventsDone = true;
	HideGameUI();
	M_Manifest.Anchor = M_AnchorLocation;
	UE_LOG(LogGMR, Display, TEXT("GMR_SCENE_PREPARED anchor=%s yaw=%.1f frame=%d"), *M_AnchorLocation.ToCompactString(),
	       M_AnchorYaw, GetTimelineFrame());
}

void AGMRDirector::PrepareScene_ResolveAnchor()
{
	using namespace GMRDirectorPrivate;
	M_AnchorLocation = M_Job.Anchor;
	M_AnchorYaw = M_Job.AnchorYaw;
	if (M_Job.AnchorActor.Equals(TEXT("PlayerStart"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			M_AnchorLocation = It->GetActorLocation() + M_Job.Anchor;
			break;
		}
	}
	else if (not M_Job.AnchorActor.IsEmpty())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			const bool bMatch = It->GetName().StartsWith(M_Job.AnchorActor) || It->GetActorNameOrLabel().StartsWith(M_Job.AnchorActor);
			if (bMatch)
			{
				M_AnchorLocation = It->GetActorLocation() + M_Job.Anchor;
				break;
			}
		}
	}
	// Anchors are authored on the ground; drop to it so heights in shots are relative to the terrain.
	FHitResult Hit;
	FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	const FVector Up(0.f, 0.f, GroundTraceHalfHeight);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, M_AnchorLocation + Up, M_AnchorLocation - Up, ObjectParams))
	{
		M_AnchorLocation = Hit.ImpactPoint;
	}
}

void AGMRDirector::PrepareScene_NeutraliseMap()
{
	int32 Destroyed = 0;
	int32 Hidden = 0;
	int32 Cleared = 0;
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this || not IsValid(Actor))
		{
			continue;
		}
		const FString ClassName = Actor->GetClass()->GetName();
		const FString Label = Actor->GetActorNameOrLabel();
		if (M_Job.DestroyClassPrefixes.ContainsByPredicate([&ClassName](const FString& Prefix) { return ClassName.StartsWith(Prefix); }))
		{
			ToDestroy.Add(Actor);
			continue;
		}
		if (M_Job.HideActorPrefixes.ContainsByPredicate([&Label](const FString& Prefix) { return Label.StartsWith(Prefix); }))
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			++Hidden;
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		Actor->Destroy();
		++Destroyed;
	}
	Cleared = ClearPreplacedUnits();
	UE_LOG(LogGMR, Display, TEXT("GMR_MAP_NEUTRALISED destroyed=%d (units=%d) hidden=%d"), Destroyed, Cleared, Hidden);
}

void AGMRDirector::PrepareScene_SpawnCamera()
{
	using namespace GMRDirectorPrivate;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_Camera = GetWorld()->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), M_AnchorLocation, FRotator::ZeroRotator,
	                                                    SpawnParameters);
	if (not GetIsValidCamera())
	{
		return;
	}
	M_SpawnedActors.Add(M_Camera);
	UCineCameraComponent* const Cine = M_Camera->GetCineCameraComponent();
	const bool bVertical = M_Job.ResolutionY > M_Job.ResolutionX;
	Cine->Filmback.SensorWidth = bVertical ? SensorShortSide : SensorLongSide;
	Cine->Filmback.SensorHeight = bVertical ? SensorLongSide : SensorShortSide;
	Cine->LensSettings.MinFocalLength = MinLensFocal;
	Cine->LensSettings.MaxFocalLength = MaxLensFocal;
	Cine->LensSettings.MinFStop = 1.2f;
	Cine->LensSettings.MaxFStop = 22.f;
	Cine->bConstrainAspectRatio = true;
	Cine->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
	if (M_Job.MotionBlurAmount >= 0.f)
	{
		Cine->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Cine->PostProcessSettings.MotionBlurAmount = M_Job.MotionBlurAmount;
	}
	if (GetIsValidPlayerController())
	{
		M_PlayerController->SetViewTargetWithBlend(M_Camera, 0.f);
	}
}

void AGMRDirector::PrepareScene_SpawnSky()
{
	if (M_Job.SkyMaterial.IsEmpty())
	{
		return;
	}
	UStaticMesh* const SkyMesh = LoadObject<UStaticMesh>(nullptr, *M_Job.SkyMesh);
	if (not IsValid(SkyMesh))
	{
		AddManifestWarning(TEXT("sky mesh missing ") + M_Job.SkyMesh);
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location = M_AnchorLocation - FVector(0.f, 0.f, M_Job.SkyDrop);
	AStaticMeshActor* const Sky = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location,
	                                                                        FRotator(0.f, M_Job.SkyYaw, 0.f), SpawnParameters);
	if (not IsValid(Sky))
	{
		return;
	}
	UStaticMeshComponent* const Mesh = Sky->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(SkyMesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bAffectDynamicIndirectLighting = false;
	Mesh->bAffectDistanceFieldLighting = false;
	Mesh->bVisibleInRayTracing = false;
	const float BoundsRadius = FMath::Max(1.f, SkyMesh->GetBounds().SphereRadius);
	Sky->SetActorScale3D(FVector(M_Job.SkyRadius / BoundsRadius));
	M_Sky = Sky;
	M_SpawnedActors.Add(Sky);
	FGMREvent SkyEvent;
	SkyEvent.Text = M_Job.SkyMaterial;
	UE_LOG(LogGMR, Display, TEXT("GMR_SKY %s scale=%.1f"), *Event_Sky(SkyEvent), M_Job.SkyRadius / BoundsRadius);
}

FString AGMRDirector::Event_Sky(const FGMREvent& Event)
{
	AStaticMeshActor* const Sky = Cast<AStaticMeshActor>(M_Sky);
	UMaterialInterface* const Material = LoadObject<UMaterialInterface>(nullptr, *Event.Text);
	if (not IsValid(Sky) || not IsValid(Material))
	{
		return TEXT("no sky or material ") + Event.Text;
	}
	UStaticMeshComponent* const Mesh = Sky->GetStaticMeshComponent();
	for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
	{
		Mesh->SetMaterial(Slot, Material);
	}
	if (Event.Value != 0.f)
	{
		Sky->SetActorRotation(FRotator(0.f, Event.Value, 0.f));
	}
	return TEXT("sky ") + Material->GetName();
}

void AGMRDirector::HideGameUI() const
{
	// Runs the game's own IRTSUIElement loop (that interface is not exported to other modules).
	if (not GetIsValidPlayerController())
	{
		return;
	}
	M_PlayerController->SetGameUIVisibility(false);
	UE_LOG(LogGMR, Display, TEXT("GMR_UI_HIDDEN"));
}

void AGMRDirector::PushSoundMix()
{
	using namespace GMRDirectorPrivate;
	TArray<const TCHAR*> MutedPaths = {UISoundClass, CinematicSoundClass};
	if (not M_Job.bKeepMusic)
	{
		MutedPaths.Add(MusicSoundClass);
	}
	if (not M_Job.bKeepAnnouncer)
	{
		MutedPaths.Add(AnnouncerSoundClass);
	}
	if (not M_Job.bKeepVoiceLines)
	{
		MutedPaths.Add(VoiceLineSoundClass);
	}
	M_SoundMix = NewObject<USoundMix>(this, TEXT("GMR_SoundMix"));
	for (const TCHAR* Path : MutedPaths)
	{
		USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, Path);
		if (not IsValid(SoundClass))
		{
			AddManifestWarning(FString::Printf(TEXT("sound class missing %s"), Path));
			continue;
		}
		FSoundClassAdjuster& Adjuster = M_SoundMix->SoundClassEffects.AddDefaulted_GetRef();
		Adjuster.SoundClassObject = SoundClass;
		Adjuster.VolumeAdjuster = 0.f;
		Adjuster.bApplyToChildren = true;
	}
	UGameplayStatics::PushSoundMixModifier(GetWorld(), M_SoundMix);
}

void AGMRDirector::KeepGameRunning() const
{
	using namespace GMRDirectorPrivate;
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		if (UButton* const NormalButton = Cast<UButton>(Picker->WidgetTree->FindWidget(DifficultyButtonName)))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	if (M_PlayerController.IsValid() && M_PlayerController->GetIsGameOnPause())
	{
		M_PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void AGMRDirector::DisableFogOfWar() const
{
	if (not M_Job.bDisableFogOfWar)
	{
		return;
	}
	// Same recipe as the intro cinematic's MRQ renders: stop the manager, drop its post process and the
	// fog blendables of the map's post process volumes. Units are kept visible per frame in KeepUnitsReady.
	int32 Disabled = 0;
	for (TActorIterator<AFowManager> It(GetWorld()); It; ++It)
	{
		It->SetActorTickEnabled(false);
		It->SetActorHiddenInGame(true);
		TInlineComponentArray<UPostProcessComponent*> PostProcesses(*It);
		for (UPostProcessComponent* const PostProcess : PostProcesses)
		{
			PostProcess->bEnabled = false;
			++Disabled;
		}
	}
	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		TArray<FWeightedBlendable>& Blendables = It->Settings.WeightedBlendables.Array;
		for (int32 Index = Blendables.Num() - 1; Index >= 0; --Index)
		{
			const UObject* const Blendable = Blendables[Index].Object;
			if (Blendable != nullptr && Blendable->GetPathName().Contains(TEXT("FogOfWar")))
			{
				Blendables.RemoveAt(Index);
				++Disabled;
			}
		}
	}
	UE_LOG(LogGMR, Display, TEXT("GMR_FOG_DISABLED removed=%d"), Disabled);
}

void AGMRDirector::TickWaitingForTimeline()
{
	const int32 Frame = GetTimelineFrame();
	const int32 WarmupFrames = FMath::RoundToInt(M_Job.WarmupSeconds * M_Job.Fps);
	// The first shot already runs (frozen at its start) during the warmup, so the first recorded frame is framed.
	UpdateCamera(0.f, 1.f / M_Job.Fps, Frame);
	if (Frame < WarmupFrames)
	{
		// The player profile spawns its start units asynchronously, after the first clear: keep clearing.
		constexpr int32 ClearEveryFrames = 10;
		if (Frame % ClearEveryFrames == 0)
		{
			const int32 Cleared = ClearPreplacedUnits();
			if (Cleared > 0)
			{
				UE_LOG(LogGMR, Display, TEXT("GMR_WARMUP_CLEARED units=%d frame=%d"), Cleared, Frame);
			}
		}
		return;
	}
	if (Frame > WarmupFrames)
	{
		AddManifestWarning(FString::Printf(TEXT("setup overran the warmup: timeline starts at frame %d, wanted %d"), Frame,
		                                   WarmupFrames));
	}
	M_TimelineStartFrame = Frame;
	M_TimelineEndFrame = Frame + FMath::RoundToInt(M_Job.DurationSeconds * M_Job.Fps);
	M_Manifest.StartFrame = M_TimelineStartFrame;
	M_Manifest.EndFrame = M_TimelineEndFrame;
	M_Manifest.TotalFrames = M_TimelineEndFrame - M_TimelineStartFrame;
	// Restart the first shot on the first recorded frame so its move starts with the recording; the warmup
	// already framed it, so this re-snap lands on the same pose.
	M_CurrentShot = -1;
	M_Manifest.Shots.Reset();
	M_Phase = EGMRDirectorPhase::Running;
	UE_LOG(LogGMR, Display, TEXT("GMR_TIMELINE_START frame=%d end=%d"), M_TimelineStartFrame, M_TimelineEndFrame);
	TickRunning();
}

void AGMRDirector::TickRunning()
{
	const int32 Frame = GetTimelineFrame();
	if (Frame == M_LastFrame)
	{
		return;
	}
	const float DeltaSeconds = M_LastFrame < 0 ? 0.f : (Frame - M_LastFrame) / static_cast<float>(M_Job.Fps);
	M_LastFrame = Frame;
	const float TimelineSeconds = (Frame - M_TimelineStartFrame) / static_cast<float>(M_Job.Fps);
	UpdateSubmixCaptureWindow(Frame);
	RunDueEvents(TimelineSeconds, Frame);
	UpdateCamera(TimelineSeconds, DeltaSeconds, Frame);
	SampleVoiceLines(Frame);
	if (Frame >= M_TimelineEndFrame)
	{
		M_Manifest.bComplete = true;
		WriteManifest();
	}
}

void AGMRDirector::RunDueEvents(const float TimelineSeconds, const int32 Frame)
{
	while (M_NextEvent < M_SortedEvents.Num() && M_SortedEvents[M_NextEvent].Time <= TimelineSeconds)
	{
		ExecuteEvent(M_SortedEvents[M_NextEvent], Frame);
		++M_NextEvent;
	}
}

void AGMRDirector::ExecuteEvent(const FGMREvent& Event, const int32 Frame)
{
	const FString Result = ExecuteEventByType(Event);
	FGMRManifestEvent& Logged = M_Manifest.Events.AddDefaulted_GetRef();
	Logged.Frame = Frame;
	Logged.Type = Event.Type;
	Logged.Tag = Event.Tag;
	Logged.Result = Result;
	UE_LOG(LogGMR, Display, TEXT("GMR_EVENT t=%.2f frame=%d type=%s tag=%s result=%s"), Event.Time, Frame, *Event.Type,
	       *Event.Tag, *Result);
}

FString AGMRDirector::ExecuteEventByType(const FGMREvent& Event)
{
	const FString& Type = Event.Type;
	if (Type.Equals(TEXT("Spawn"), ESearchCase::IgnoreCase))
	{
		return Event_Spawn(Event);
	}
	const bool bIsOrder = Type.Equals(TEXT("Move"), ESearchCase::IgnoreCase) || Type.Equals(TEXT("AttackMove"), ESearchCase::IgnoreCase)
		|| Type.Equals(TEXT("Attack"), ESearchCase::IgnoreCase) || Type.Equals(TEXT("AttackGround"), ESearchCase::IgnoreCase)
		|| Type.Equals(TEXT("Stop"), ESearchCase::IgnoreCase) || Type.Equals(TEXT("Rotate"), ESearchCase::IgnoreCase);
	if (bIsOrder)
	{
		return Event_Order(Event);
	}
	if (Type.Equals(TEXT("Garrison"), ESearchCase::IgnoreCase))
	{
		return Event_Garrison(Event);
	}
	if (Type.Equals(TEXT("Clear"), ESearchCase::IgnoreCase))
	{
		return Event_Clear(Event);
	}
	if (Type.Equals(TEXT("Stance"), ESearchCase::IgnoreCase))
	{
		return Event_Stance(Event);
	}
	if (Type.Equals(TEXT("Destroy"), ESearchCase::IgnoreCase))
	{
		return Event_Destroy(Event);
	}
	if (Type.Equals(TEXT("Hide"), ESearchCase::IgnoreCase))
	{
		return Event_Hide(Event);
	}
	if (Type.Equals(TEXT("Resources"), ESearchCase::IgnoreCase))
	{
		return Event_Resources(Event);
	}
	if (Type.Equals(TEXT("Deploy"), ESearchCase::IgnoreCase))
	{
		return Event_Deploy(Event);
	}
	if (Type.Equals(TEXT("Expand"), ESearchCase::IgnoreCase))
	{
		return Event_Expand(Event);
	}
	if (Type.Equals(TEXT("Ability"), ESearchCase::IgnoreCase))
	{
		return Event_Ability(Event);
	}
	if (Type.Equals(TEXT("Claim"), ESearchCase::IgnoreCase))
	{
		return Event_Claim(Event);
	}
	if (Type.Equals(TEXT("Voice"), ESearchCase::IgnoreCase))
	{
		return Event_Voice(Event);
	}
	if (Type.Equals(TEXT("Trench"), ESearchCase::IgnoreCase))
	{
		return Event_Trench(Event);
	}
	if (Type.Equals(TEXT("Capture"), ESearchCase::IgnoreCase))
	{
		return Event_Capture(Event);
	}
	if (Type.Equals(TEXT("Marker"), ESearchCase::IgnoreCase))
	{
		return Event_Marker(Event);
	}
	if (Type.Equals(TEXT("Sky"), ESearchCase::IgnoreCase))
	{
		return Event_Sky(Event);
	}
	if (Type.Equals(TEXT("HideUI"), ESearchCase::IgnoreCase))
	{
		HideGameUI();
		return TEXT("ok");
	}
	return TEXT("unknown event type");
}

void AGMRDirector::UpdateCamera(const float TimelineSeconds, const float DeltaSeconds, const int32 Frame)
{
	if (not GetIsValidCamera())
	{
		return;
	}
	int32 WantedShot = M_CurrentShot;
	for (int32 Index = 0; Index < M_Job.Shots.Num(); ++Index)
	{
		const FGMRShot& Shot = M_Job.Shots[Index];
		if (TimelineSeconds >= Shot.Start && TimelineSeconds < Shot.End)
		{
			WantedShot = Index;
			break;
		}
	}
	if (WantedShot < 0)
	{
		WantedShot = 0;
	}
	if (WantedShot != M_CurrentShot)
	{
		BeginShot(WantedShot, Frame);
	}
	const FGMRShot& Shot = M_Job.Shots[M_CurrentShot];
	auto Resolver = [this](const FString& Pivot, FVector& OutLocation, FVector& OutHeading)
	{
		return ResolvePivot(Pivot, OutLocation, OutHeading);
	};
	const FGMRCameraPose Pose = M_Rig.Evaluate(TimelineSeconds - Shot.Start, DeltaSeconds, *GetWorld(), Resolver,
	                                           GetRigIgnoredActors());
	M_Camera->SetActorLocationAndRotation(Pose.Location, Pose.Rotation);
	UCineCameraComponent* const Cine = M_Camera->GetCineCameraComponent();
	Cine->SetCurrentFocalLength(Pose.FocalLength);
	// Movie Render Queue only resets temporal history at its own shot boundaries, not at a game camera cut: the
	// cut frame's camera jump would smear as motion blur, so blur stays off for the first frames of every shot.
	constexpr float NoMotionBlur = 0.f;
	Cine->PostProcessSettings.bOverride_MotionBlurAmount = M_CutBlurHoldFrames > 0 || M_Job.MotionBlurAmount >= 0.f;
	Cine->PostProcessSettings.MotionBlurAmount = M_CutBlurHoldFrames > 0 ? NoMotionBlur : M_Job.MotionBlurAmount;
	M_CutBlurHoldFrames = FMath::Max(0, M_CutBlurHoldFrames - 1);
	if (Shot.Aperture > 0.f)
	{
		Cine->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		Cine->FocusSettings.ManualFocusDistance = Pose.FocusDistance;
		Cine->SetCurrentAperture(Shot.Aperture);
	}
	else
	{
		Cine->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
	}
	FollowCameraWithPawn(Pose.LookAt);
	// The camera manager cached its view before this tick group; refresh it so this frame renders this pose
	// (and a cut's camera-cut flag, set in BeginShot, applies to the right frame).
	if (GetIsValidPlayerController() && IsValid(M_PlayerController->PlayerCameraManager))
	{
		M_PlayerController->PlayerCameraManager->UpdateCamera(0.f);
	}
}

void AGMRDirector::BeginShot(const int32 ShotIndex, const int32 Frame)
{
	if (M_CurrentShot >= 0 && M_Manifest.Shots.IsValidIndex(M_CurrentShot))
	{
		M_Manifest.Shots[M_CurrentShot].EndFrame = Frame;
	}
	M_CurrentShot = ShotIndex;
	M_ShotLiveChoices.Reset();
	const FGMRShot& Shot = M_Job.Shots[ShotIndex];
	auto Resolver = [this](const FString& Pivot, FVector& OutLocation, FVector& OutHeading)
	{
		return ResolvePivot(Pivot, OutLocation, OutHeading);
	};
	const float Boost = M_Rig.BeginShot(Shot, M_Job, *GetWorld(), Resolver, GetRigIgnoredActors());
	constexpr int32 CutBlurHoldFrames = 2;
	M_CutBlurHoldFrames = CutBlurHoldFrames;
	// A hard cut: the game's camera cut flag stops temporal effects (TAA, motion blur) smearing across it.
	if (GetIsValidPlayerController() && M_PlayerController->PlayerCameraManager)
	{
		M_PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
	}
	FGMRManifestShot& Logged = M_Manifest.Shots.AddDefaulted_GetRef();
	Logged.Name = Shot.Name;
	Logged.Rig = Shot.Rig;
	Logged.StartFrame = Frame;
	Logged.EndFrame = M_TimelineEndFrame;
	Logged.OcclusionPitchBoost = Boost;
	const FGMRCameraPose Pose = M_Rig.Evaluate(0.f, 0.f, *GetWorld(), Resolver, GetRigIgnoredActors());
	Logged.FirstCameraLocation = Pose.Location;
	Logged.FirstLookAt = Pose.LookAt;
	Logged.FirstFocal = Pose.FocalLength;
	// Evaluate(0) above consumed the snap; begin again so the first real frame snaps too.
	M_Rig.BeginShot(Shot, M_Job, *GetWorld(), Resolver, GetRigIgnoredActors());
	UE_LOG(LogGMR, Display, TEXT("GMR_SHOT_BEGIN index=%d name=%s rig=%s frame=%d boost=%.0f cam=%s look=%s focal=%.1f"),
	       ShotIndex, *Shot.Name, *Shot.Rig, Frame, Boost, *Pose.Location.ToCompactString(),
	       *Pose.LookAt.ToCompactString(), Pose.FocalLength);
	if (Boost > 0.f)
	{
		AddManifestWarning(FString::Printf(TEXT("shot %s tilted %.0f deg up to clear occlusion"), *Shot.Name, Boost));
	}
}

bool AGMRDirector::ResolvePivot(const FString& Pivot, FVector& OutLocation, FVector& OutHeading)
{
	OutHeading = FRotator(0.f, M_AnchorYaw, 0.f).Vector();
	if (Pivot.IsEmpty())
	{
		OutLocation = M_AnchorLocation;
		return true;
	}
	bool bFound = false;
	if (Pivot.StartsWith(TEXT("live:")))
	{
		bFound = ResolveLivePivot(Pivot.RightChop(5), OutLocation, OutHeading);
	}
	else if (Pivot.StartsWith(TEXT("tag:")))
	{
		FString Tag = Pivot.RightChop(4);
		int32 Index = -1;
		FString IndexText;
		if (Tag.Split(TEXT("["), &Tag, &IndexText))
		{
			Index = FCString::Atoi(*IndexText.LeftChop(1));
		}
		bFound = GetTaggedCentroid(Tag, Index, OutLocation, OutHeading);
		const TArray<FVector>* Known = M_LastUnitLocations.Find(Tag);
		if (not bFound && Index >= 0 && Known != nullptr && Known->IsValidIndex(Index) && not (*Known)[Index].IsZero())
		{
			OutLocation = (*Known)[Index];
			bFound = true;
		}
	}
	else if (Pivot.StartsWith(TEXT("actor:")))
	{
		const FString Prefix = Pivot.RightChop(6);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->GetName().StartsWith(Prefix) || It->GetActorNameOrLabel().StartsWith(Prefix))
			{
				OutLocation = It->GetActorLocation();
				OutHeading = It->GetActorForwardVector();
				bFound = true;
				break;
			}
		}
	}
	if (bFound)
	{
		M_LastPivotLocations.Add(Pivot, OutLocation);
		return true;
	}
	if (const FVector* Last = M_LastPivotLocations.Find(Pivot))
	{
		OutLocation = *Last;
		return true;
	}
	// Never seen alive (its unit died before any shot looked at it): frame the group, then the anchor.
	const FString Tag = Pivot.Contains(TEXT(":")) ? Pivot.RightChop(Pivot.Find(TEXT(":")) + 1).Replace(TEXT("[0]"), TEXT("")) : Pivot;
	FString GroupTag = Tag;
	FString IndexText;
	Tag.Split(TEXT("["), &GroupTag, &IndexText);
	if (GetTaggedCentroid(GroupTag, -1, OutLocation, OutHeading) || GetLastKnownLocation(GroupTag, OutLocation))
	{
		return true;
	}
	OutLocation = M_AnchorLocation;
	return true;
}

bool AGMRDirector::ResolveLivePivot(const FString& Tag, FVector& OutLocation, FVector& OutHeading)
{
	// The first unit of the tag that is alive when the shot starts; the shot then stays on that unit.
	TWeakObjectPtr<AActor>& Chosen = M_ShotLiveChoices.FindOrAdd(Tag);
	if (not Chosen.IsValid())
	{
		const TArray<AActor*> Live = GetLiveTaggedUnits(Tag);
		if (Live.Num() > 0)
		{
			Chosen = Live[0];
		}
	}
	const AActor* Unit = Chosen.Get();
	if (not GMRDirectorPrivate::GetIsLiveActor(Unit))
	{
		return false;
	}
	return GMRDirectorPrivate::GetUnitLocationAndHeading(Unit, OutLocation, OutHeading);
}

bool AGMRDirector::GetLastKnownLocation(const FString& Tag, FVector& OutLocation) const
{
	const TArray<FVector>* Known = M_LastUnitLocations.Find(Tag);
	if (Known == nullptr || Known->Num() == 0)
	{
		return false;
	}
	FVector Sum = FVector::ZeroVector;
	for (const FVector& Location : *Known)
	{
		Sum += Location;
	}
	OutLocation = Sum / Known->Num();
	return true;
}

void AGMRDirector::RecordUnitLocations()
{
	for (const TPair<FString, FGMRTaggedUnits>& Pair : M_TaggedUnits)
	{
		TArray<FVector>& Known = M_LastUnitLocations.FindOrAdd(Pair.Key);
		Known.SetNum(Pair.Value.Units.Num());
		for (int32 Index = 0; Index < Pair.Value.Units.Num(); ++Index)
		{
			const AActor* Unit = Pair.Value.Units[Index].Get();
			FVector Location;
			FVector Heading;
			if (GMRDirectorPrivate::GetIsLiveActor(Unit) && GMRDirectorPrivate::GetUnitLocationAndHeading(Unit, Location, Heading))
			{
				Known[Index] = Location;
			}
		}
	}
}

bool AGMRDirector::GetTaggedCentroid(const FString& Tag, const int32 Index, FVector& OutLocation, FVector& OutHeading) const
{
	using namespace GMRDirectorPrivate;
	const FGMRTaggedUnits* Units = M_TaggedUnits.Find(Tag);
	if (Units == nullptr)
	{
		return false;
	}
	FVector Sum = FVector::ZeroVector;
	FVector HeadingSum = FVector::ZeroVector;
	int32 Count = 0;
	for (int32 UnitIndex = 0; UnitIndex < Units->Units.Num(); ++UnitIndex)
	{
		if (Index >= 0 && UnitIndex != Index)
		{
			continue;
		}
		const AActor* Unit = Units->Units[UnitIndex].Get();
		if (not GetIsLiveActor(Unit))
		{
			continue;
		}
		FVector Location;
		FVector Heading;
		if (not GetUnitLocationAndHeading(Unit, Location, Heading))
		{
			continue;
		}
		Sum += Location;
		HeadingSum += Heading;
		++Count;
	}
	if (Count == 0)
	{
		return false;
	}
	OutLocation = Sum / Count;
	OutHeading = HeadingSum.IsNearlyZero() ? OutHeading : HeadingSum.GetSafeNormal2D();
	return true;
}

TArray<const AActor*> AGMRDirector::GetRigIgnoredActors() const
{
	TArray<const AActor*> Ignored;
	for (const TPair<FString, FGMRTaggedUnits>& Pair : M_TaggedUnits)
	{
		for (const TWeakObjectPtr<AActor>& Unit : Pair.Value.Units)
		{
			if (Unit.IsValid())
			{
				Ignored.Add(Unit.Get());
			}
		}
	}
	return Ignored;
}

void AGMRDirector::FollowCameraWithPawn(const FVector& LookAt) const
{
	// The game's distance optimisations (tick rates, effects) follow the player's pawn, not the view target, so the
	// pawn stands where the camera looks. Its own meshes (a checker sphere on the spring arm) must never show.
	if (not GetIsValidPlayerController())
	{
		return;
	}
	APawn* const Pawn = M_PlayerController->GetPawn();
	if (not IsValid(Pawn))
	{
		return;
	}
	if (not Pawn->IsHidden())
	{
		Pawn->SetActorHiddenInGame(true);
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Pawn);
		for (UPrimitiveComponent* const Primitive : Primitives)
		{
			Primitive->SetHiddenInGame(true);
		}
	}
	Pawn->SetActorLocation(LookAt, false, nullptr, ETeleportType::TeleportPhysics);
}

TArray<AActor*> AGMRDirector::GetLiveTaggedUnits(const FString& Tag) const
{
	using namespace GMRDirectorPrivate;
	TArray<AActor*> Live;
	const FGMRTaggedUnits* Units = M_TaggedUnits.Find(Tag);
	if (Units == nullptr)
	{
		return Live;
	}
	for (const TWeakObjectPtr<AActor>& Unit : Units->Units)
	{
		if (GetIsLiveActor(Unit.Get()))
		{
			Live.Add(Unit.Get());
		}
	}
	return Live;
}

FVector AGMRDirector::AnchorToWorld(const FVector& AnchorFrameOffset) const
{
	return M_AnchorLocation + FRotator(0.f, M_AnchorYaw, 0.f).RotateVector(AnchorFrameOffset);
}

float AGMRDirector::AnchorYawToWorld(const float AnchorFrameYaw) const
{
	return M_AnchorYaw + AnchorFrameYaw;
}

void AGMRDirector::RegisterUnit(const FString& Tag, AActor* Unit)
{
	if (not IsValid(Unit))
	{
		return;
	}
	Unit->Tags.AddUnique(GMRDirectorPrivate::RecordedUnitTag);
	M_TaggedUnits.FindOrAdd(Tag).Units.Add(Unit);
}

ACPPController* AGMRDirector::GetPlayerController() const
{
	return M_PlayerController.Get();
}

void AGMRDirector::AddManifestWarning(const FString& Warning)
{
	M_Manifest.Warnings.Add(Warning);
	UE_LOG(LogGMR, Warning, TEXT("GMR_WARNING %s"), *Warning);
}

int32 AGMRDirector::GetTimelineFrame() const
{
	const UMoviePipeline* Pipeline = GetActivePipeline();
	if (not IsValid(Pipeline))
	{
		return FMath::FloorToInt(GetWorld()->GetTimeSeconds() * M_Job.Fps);
	}
	return Pipeline->GetOutputState().OutputFrameNumber;
}

UMoviePipeline* AGMRDirector::GetActivePipeline() const
{
	const UGMRRunner* Runner = UGMRRunner::Get();
	return IsValid(Runner) ? Runner->GetActivePipeline() : nullptr;
}

void AGMRDirector::NotifyRenderFinished()
{
	if (M_CurrentShot >= 0 && M_Manifest.Shots.IsValidIndex(M_CurrentShot))
	{
		M_Manifest.Shots[M_CurrentShot].EndFrame = M_TimelineEndFrame;
	}
	{
		FScopeLock Lock(M_VoiceLinesLock.Get());
		M_Manifest.VoiceLinesHeard = M_VoiceLinesHeard->Num();
		for (const TPair<FString, float>& Heard : *M_VoiceLinesHeard)
		{
			UE_LOG(LogGMR, Display, TEXT("GMR_VOICE_HEARD %s volume=%.2f"), *Heard.Key, Heard.Value);
		}
	}
	M_Phase = EGMRDirectorPhase::Finished;
	WriteSubmixCaptureWav();
	WriteManifest();
	DestroySpawnedActors();
}

void AGMRDirector::DestroySpawnedActors()
{
	int32 Destroyed = 0;
	for (TPair<FString, FGMRTaggedUnits>& Pair : M_TaggedUnits)
	{
		for (const TWeakObjectPtr<AActor>& Unit : Pair.Value.Units)
		{
			M_SpawnedActors.AddUnique(Unit);
		}
	}
	for (const TWeakObjectPtr<AActor>& Actor : M_SpawnedActors)
	{
		// Level actors the job only claimed (a supply point, a landmark) stay; only spawned ones go.
		if (Actor.IsValid() && not Actor->Tags.Contains(FName(TEXT("GMR_Claimed"))))
		{
			Actor->Destroy();
			++Destroyed;
		}
	}
	M_SpawnedActors.Reset();
	M_TaggedUnits.Reset();
	UE_LOG(LogGMR, Display, TEXT("GMR_CLEANUP destroyed=%d"), Destroyed);
}

void AGMRDirector::WriteManifest() const
{
	FString Json;
	if (not FJsonObjectConverter::UStructToJsonObjectString(M_Manifest, Json))
	{
		UE_LOG(LogGMR, Error, TEXT("GMR_MANIFEST_SERIALIZE_FAILED"));
		return;
	}
	FFileHelper::SaveStringToFile(Json, *FPaths::Combine(M_Job.OutputDir, TEXT("Manifest.json")),
	                              FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool AGMRDirector::GetIsValidCamera() const
{
	if (IsValid(M_Camera))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Camera", "GetIsValidCamera", this);
	return false;
}

bool AGMRDirector::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerController", "GetIsValidPlayerController", this);
	return false;
}

// ---------------------------------------------------------------------------------------------------------------
// Audio: the headless render needs its own submix capture and 2D sounds (see headless-render-audio-mute).

void AGMRDirector::StartSubmixCapture()
{
	FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	if (AudioDevice == nullptr || M_AudioCapture.IsValid())
	{
		AddManifestWarning(TEXT("no audio device: the take is silent"));
		return;
	}
	M_AudioCapture = MakeShared<FGMRAudioCapture, ESPMode::ThreadSafe>();
	AudioDevice->RegisterSubmixBufferListener(M_AudioCapture.ToSharedRef(), AudioDevice->GetMainSubmixObject());
}

void AGMRDirector::StopSubmixCapture()
{
	WriteSubmixCaptureWav();
	if (not M_AudioCapture.IsValid())
	{
		return;
	}
	if (FAudioDevice* AudioDevice = GetWorld() ? GetWorld()->GetAudioDeviceRaw() : nullptr)
	{
		AudioDevice->UnregisterSubmixBufferListener(M_AudioCapture.ToSharedRef(), AudioDevice->GetMainSubmixObject());
	}
	M_AudioCapture.Reset();
}

void AGMRDirector::UpdateSubmixCaptureWindow(const int32 Frame)
{
	// Audio for frame F is rendered after the game tick that sees F, so this tick's clock holds frames up to F-1.
	if (not M_AudioCapture.IsValid())
	{
		return;
	}
	const FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	if (AudioDevice == nullptr)
	{
		return;
	}
	const double Clock = AudioDevice->GetAudioClock();
	const double FrameSeconds = 1.0 / M_Job.Fps;
	if (not bM_CaptureStartClockSet)
	{
		M_CaptureStartClock = Clock;
		bM_CaptureStartClockSet = true;
		UE_LOG(LogGMR, Display, TEXT("GMR_CAPTURE_ANCHOR frame=%d clock=%.4f firstBuffer=%.4f"), Frame, Clock,
		       M_AudioCapture->GetFirstBufferClock());
	}
	M_CaptureEndClock = M_CaptureStartClock + (M_TimelineEndFrame - M_TimelineStartFrame) * FrameSeconds;
}

void AGMRDirector::WriteSubmixCaptureWav()
{
	if (bM_CaptureWritten || not M_AudioCapture.IsValid() || not bM_CaptureStartClockSet)
	{
		return;
	}
	bM_CaptureWritten = true;
	const FString Path = M_Job.OutputDir / (M_Job.JobName + TEXT("_capture.wav"));
	FString Error;
	const double Written = M_AudioCapture->WriteWav(Path, M_CaptureStartClock, M_CaptureEndClock, Error);
	if (Written < 0.0)
	{
		UE_LOG(LogGMR, Error, TEXT("GMR_CAPTURE_FAILED %s"), *Error);
		return;
	}
	UE_LOG(LogGMR, Display, TEXT("GMR_CAPTURE_WRITTEN file=%s seconds=%.3f expected=%.3f"), *Path, Written,
	       M_Job.DurationSeconds);
}

void AGMRDirector::KeepAudioUnmuted() const
{
	FApp::SetVolumeMultiplier(1.f);
	FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	Audio::FMixerDevice* Mixer = AudioDevice != nullptr ? static_cast<Audio::FMixerDevice*>(AudioDevice) : nullptr;
	if (Mixer == nullptr)
	{
		return;
	}
	if (Audio::FMixerSubmixPtr Master = Mixer->GetMasterSubmix().Pin())
	{
		Master->SetBackgroundMuted(false);
	}
	for (TObjectIterator<USoundSubmix> It; It; ++It)
	{
		It->bMuteWhenBackgrounded = false;
		if (Audio::FMixerSubmixPtr Instance = Mixer->GetSubmixInstance(*It).Pin())
		{
			Instance->SetBackgroundMuted(false);
		}
	}
}

void AGMRDirector::ForceSoundsNonSpatialized() const
{
	FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	if (AudioDevice == nullptr)
	{
		return;
	}
	const bool bNoDistanceAttenuation = M_Job.bNoDistanceAttenuation;
	for (TObjectIterator<USoundAttenuation> It; It; ++It)
	{
		It->Attenuation.bSpatialize = false;
		if (bNoDistanceAttenuation)
		{
			It->Attenuation.bAttenuate = false;
		}
	}
	FAudioThread::RunCommandOnAudioThread([AudioDevice, bNoDistanceAttenuation]()
	{
		for (FActiveSound* ActiveSound : AudioDevice->GetActiveSounds())
		{
			if (ActiveSound == nullptr)
			{
				continue;
			}
			ActiveSound->bAllowSpatialization = false;
			if (bNoDistanceAttenuation)
			{
				ActiveSound->bHasAttenuationSettings = false;
			}
		}
	});
}

void AGMRDirector::SampleVoiceLines(const int32 Frame)
{
	using namespace GMRDirectorPrivate;
	if (Frame % VoiceSampleEveryFrames != 0)
	{
		return;
	}
	FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw();
	if (AudioDevice == nullptr)
	{
		return;
	}
	TSharedPtr<TMap<FString, float>, ESPMode::ThreadSafe> Heard = M_VoiceLinesHeard;
	TSharedPtr<FCriticalSection, ESPMode::ThreadSafe> Lock = M_VoiceLinesLock;
	FAudioThread::RunCommandOnAudioThread([AudioDevice, Heard, Lock]()
	{
		for (const FActiveSound* ActiveSound : AudioDevice->GetActiveSounds())
		{
			if (ActiveSound == nullptr || ActiveSound->GetSound() == nullptr)
			{
				continue;
			}
			const USoundClass* SoundClass = ActiveSound->GetSoundClass();
			if (SoundClass == nullptr || not SoundClass->GetName().Contains(VoiceLineClassName))
			{
				continue;
			}
			// Audible, not just active: the loudest wave instance after mix, attenuation and class volume.
			float LoudestVolume = 0.f;
			for (const TPair<UPTRINT, FWaveInstance*>& Pair : ActiveSound->GetWaveInstances())
			{
				if (Pair.Value != nullptr)
				{
					LoudestVolume = FMath::Max(LoudestVolume, Pair.Value->GetActualVolume());
				}
			}
			constexpr float AudibleVolume = 0.01f;
			if (LoudestVolume < AudibleVolume)
			{
				continue;
			}
			FScopeLock ScopeLock(Lock.Get());
			float& Loudest = Heard->FindOrAdd(ActiveSound->GetSound()->GetName());
			Loudest = FMath::Max(Loudest, LoudestVolume);
		}
	});
}
