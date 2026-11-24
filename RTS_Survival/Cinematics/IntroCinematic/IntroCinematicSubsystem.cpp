// Copyright (C) Bas Blokzijl - All rights reserved.

#include "IntroCinematicSubsystem.h"

#include "ShaderCompiler.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "RTS_Survival/GameUI/CinematicOverlay/W_CinematicOverlay.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "W_IntroCinematicHud.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogIntroCinematic, Log, All);

namespace IntroCinematicConstants
{
	const TCHAR* MapName = TEXT("RTS_IntroCinematic_1976");
	const TCHAR* MasterSequencePath = TEXT(
		"/Game/RTS_Survival/Cinematics/Intro1976/LS_Intro1976_Master.LS_Intro1976_Master");
	const TCHAR* CueTablePath = TEXT("/Game/RTS_Survival/Cinematics/Intro1976/Data/DT_IntroCues.DT_IntroCues");
	// Part of the object path of the fog of war post process material the mission maps add.
	const TCHAR* FogOfWarPathPart = TEXT("FogOfWar");
	constexpr float WorldSweepIntervalSeconds = 0.5f;
	// Below the subtitle overlay (5), above nothing else: the map has no other UI.
	constexpr int32 HudZOrder = 3;
	// -IntroCinematicAutoQuit: a plain -game run exits when the film ends (headless end-to-end test).
	const TCHAR* AutoQuitParam = TEXT("IntroCinematicAutoQuit");
	// -IntroCinematicShotEvery=<seconds>: screenshots with UI during a plain -game run, as proof of what played.
	const TCHAR* ShotEveryParam = TEXT("IntroCinematicShotEvery=");
	constexpr float ProgressLogIntervalSeconds = 10.0f;
	const TCHAR* const LevelStreamingCommands[] = {
		TEXT("s.LevelStreamingActorsUpdateTimeLimit 100000"),
		TEXT("s.PriorityLevelStreamingActorsUpdateExtraTime 100000"),
		TEXT("s.UnregisterComponentsTimeLimit 100000"),
		TEXT("s.LevelStreamingComponentsRegistrationGranularity 1000000"),
		TEXT("s.LevelStreamingComponentsUnregistrationGranularity 1000000"),
		TEXT("s.LevelStreamingAddPrimitiveGranularity 1000000"),
	};
	// Tag the engine puts on every actor a level sequence spawns.
	const FName SequencerSpawnedTag = TEXT("SequencerActor");
}

bool UIntroCinematicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* const World = Cast<UWorld>(Outer);
	if (World == nullptr || not World->IsGameWorld())
	{
		return false;
	}
	return Super::ShouldCreateSubsystem(Outer);
}

void UIntroCinematicSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetName() != IntroCinematicConstants::MapName)
	{
		return;
	}
	bM_IsActive = true;
	// The streamed mission maps run without the RTS player and report that through modal error dialogs.
	RTSFunctionLibrary::PushModalDialogSuppression();
	LoadCues();
	if (GEngine != nullptr)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
		// Also the engine's own red notices ("lighting needs to be rebuilt", shader counts).
		GEngine->Exec(&InWorld, TEXT("DisableAllScreenMessages"));
		// By default a level is added to the world a few milliseconds per frame, so a mission map with thousands of
		// actors pops in over many frames after a cut. Here it has to be complete at once.
		for (const TCHAR* const Command : IntroCinematicConstants::LevelStreamingCommands)
		{
			GEngine->Exec(&InWorld, Command);
		}
	}
	M_LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UIntroCinematicSubsystem::HandleLevelAdded);
	M_LevelRemovedHandle = FWorldDelegates::PreLevelRemovedFromWorld.AddUObject(
		this, &UIntroCinematicSubsystem::HandleLevelRemoved);
	bM_AutoQuit = FParse::Param(FCommandLine::Get(), IntroCinematicConstants::AutoQuitParam);
	FParse::Value(FCommandLine::Get(), IntroCinematicConstants::ShotEveryParam, M_ScreenshotIntervalSeconds);
	UE_LOG(LogIntroCinematic, Display, TEXT("INTRO_CINEMATIC active, %d cues"), M_Cues.Num());
}

void UIntroCinematicSubsystem::Deinitialize()
{
	if (bM_IsActive)
	{
		FWorldDelegates::LevelAddedToWorld.Remove(M_LevelAddedHandle);
		FWorldDelegates::PreLevelRemovedFromWorld.Remove(M_LevelRemovedHandle);
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsActive = false;
	}
	Super::Deinitialize();
}

TStatId UIntroCinematicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UIntroCinematicSubsystem, STATGROUP_Tickables);
}

void UIntroCinematicSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (not bM_IsActive)
	{
		return;
	}
	// A frame drawn while a material still compiles shows the grey default material; the film waits instead.
	if (GShaderCompilingManager != nullptr && GShaderCompilingManager->IsCompiling())
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	M_SecondsUntilWorldSweep -= DeltaTime;
	if (M_SecondsUntilWorldSweep <= 0.0f)
	{
		M_SecondsUntilWorldSweep = IntroCinematicConstants::WorldSweepIntervalSeconds;
		SweepWorld();
	}
	EnsureHud();
	EnsureSequencePlayer();
	if (not GetIsValidHud() || not GetIsValidSequencePlayer())
	{
		return;
	}
	const float SequenceSeconds = static_cast<float>(M_SequencePlayer->GetCurrentTime().AsSeconds());
	M_Hud->ShowCuesAtTime(M_Cues, SequenceSeconds);
	ApplyMaterialRamps(SequenceSeconds);
	ReportProgress(SequenceSeconds);
}

void UIntroCinematicSubsystem::ApplyMaterialRamps(const float SequenceSeconds) const
{
	for (const FIntroCinematicCueRow& Cue : M_Cues)
	{
		const bool bIsActiveRamp = Cue.Type == EIntroCinematicCueType::MaterialRamp
			&& SequenceSeconds >= Cue.StartSeconds && SequenceSeconds <= Cue.EndSeconds;
		if (bIsActiveRamp)
		{
			ApplyMaterialRamp(Cue, SequenceSeconds);
		}
	}
}

void UIntroCinematicSubsystem::ApplyMaterialRamp(const FIntroCinematicCueRow& Cue, const float SequenceSeconds) const
{
	const float Duration = FMath::Max(Cue.EndSeconds - Cue.StartSeconds, UE_KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp((SequenceSeconds - Cue.StartSeconds) / Duration, 0.0f, 1.0f);
	const FName ParameterName(*Cue.Line2);
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		UStaticMeshComponent* const MeshComponent = It->GetStaticMeshComponent();
		const UStaticMesh* const Mesh = MeshComponent != nullptr ? MeshComponent->GetStaticMesh() : nullptr;
		if (Mesh != nullptr && Mesh->GetName().Contains(Cue.Line1))
		{
			MeshComponent->SetScalarParameterValueOnMaterials(ParameterName, Alpha);
			UE_LOG(LogIntroCinematic, Verbose, TEXT("INTRO_CINEMATIC ramp %s on %s = %.2f"), *Cue.Line2, *It->GetName(), Alpha);
		}
	}
}

void UIntroCinematicSubsystem::ReportProgress(const float SequenceSeconds)
{
	if (SequenceSeconds >= M_NextProgressLogSeconds)
	{
		M_NextProgressLogSeconds = SequenceSeconds + IntroCinematicConstants::ProgressLogIntervalSeconds;
		UE_LOG(LogIntroCinematic, Display, TEXT("INTRO_CINEMATIC playing %.1f s"), SequenceSeconds);
	}
	if (M_ScreenshotIntervalSeconds > 0.0f && SequenceSeconds >= M_NextScreenshotSeconds)
	{
		M_NextScreenshotSeconds = SequenceSeconds + M_ScreenshotIntervalSeconds;
		const FString FileName = FPaths::ProjectSavedDir() / TEXT("IntroCinematic/GameRun") /
			FString::Printf(TEXT("game_%06.1f.png"), SequenceSeconds);
		FScreenshotRequest::RequestScreenshot(FileName, true, false);
	}
}

void UIntroCinematicSubsystem::HandleSequenceFinished()
{
	UE_LOG(LogIntroCinematic, Display, TEXT("INTRO_CINEMATIC finished"));
	if (bM_AutoQuit)
	{
		FPlatformMisc::RequestExit(false);
	}
}

bool UIntroCinematicSubsystem::GetIsValidHud() const
{
	// No error report: the HUD cannot exist before the first player controller does, which takes a few frames.
	return IsValid(M_Hud);
}

bool UIntroCinematicSubsystem::GetIsValidSequencePlayer() const
{
	// No error report: Movie Render Queue creates the sequence player some frames after the world begins play.
	return M_SequencePlayer.IsValid();
}

void UIntroCinematicSubsystem::LoadCues()
{
	M_Cues.Reset();
	M_RemovalRules.Reset();
	const UDataTable* const Table = LoadObject<UDataTable>(nullptr, IntroCinematicConstants::CueTablePath);
	if (Table == nullptr)
	{
		UE_LOG(LogIntroCinematic, Warning, TEXT("INTRO_CINEMATIC cue table missing: %s"),
		       IntroCinematicConstants::CueTablePath);
		return;
	}
	TArray<FIntroCinematicCueRow*> Rows;
	Table->GetAllRows<FIntroCinematicCueRow>(TEXT("UIntroCinematicSubsystem"), Rows);
	for (const FIntroCinematicCueRow* const Row : Rows)
	{
		if (Row == nullptr)
		{
			continue;
		}
		if (Row->Type == EIntroCinematicCueType::HideActors)
		{
			M_RemovalRules.Emplace(Row->Line1, Row->Line2);
			continue;
		}
		M_Cues.Add(*Row);
	}
}

void UIntroCinematicSubsystem::EnsureHud()
{
	if (GetIsValidHud())
	{
		return;
	}
	APlayerController* const PlayerController = GetWorld()->GetFirstPlayerController();
	if (not IsValid(PlayerController))
	{
		return;
	}
	M_Hud = CreateWidget<UW_IntroCinematicHud>(PlayerController, UW_IntroCinematicHud::StaticClass());
	if (not IsValid(M_Hud))
	{
		return;
	}
	M_Hud->AddToViewport(IntroCinematicConstants::HudZOrder);
}

void UIntroCinematicSubsystem::EnsureSequencePlayer()
{
	if (GetIsValidSequencePlayer())
	{
		return;
	}
	UMovieSceneSequencePlayer* const Player = FindMasterSequencePlayer();
	APlayerController* const PlayerController = GetWorld()->GetFirstPlayerController();
	if (Player == nullptr || not IsValid(PlayerController))
	{
		return;
	}
	M_SequencePlayer = Player;
	Player->OnFinished.AddUniqueDynamic(this, &UIntroCinematicSubsystem::HandleSequenceFinished);
	M_Subtitles = UW_CinematicOverlay::ShowForSequence(PlayerController, Player,
	                                                   UW_IntroCinematicSubtitles::StaticClass());
	UE_LOG(LogIntroCinematic, Display, TEXT("INTRO_CINEMATIC sequence bound, %d subtitle cues"),
	       M_Subtitles.IsValid() ? M_Subtitles->GetSubtitleCueCount() : -1);
}

UMovieSceneSequencePlayer* UIntroCinematicSubsystem::FindMasterSequencePlayer() const
{
	const FSoftObjectPath MasterPath(IntroCinematicConstants::MasterSequencePath);
	UMovieSceneSequencePlayer* Found = nullptr;
	for (TActorIterator<ALevelSequenceActor> It(GetWorld()); It; ++It)
	{
		const ULevelSequence* const Sequence = It->GetSequence();
		ULevelSequencePlayer* const Player = It->GetSequencePlayer();
		if (Sequence == nullptr || Player == nullptr || FSoftObjectPath(Sequence) != MasterPath)
		{
			continue;
		}
		// Movie Render Queue may spawn its own actor next to the map's: the one that plays is the one to follow.
		if (Player->IsPlaying())
		{
			return Player;
		}
		Found = Player;
	}
	return Found;
}

void UIntroCinematicSubsystem::SweepWorld()
{
	RemoveFogOfWar();
	RemoveGameplayActors();
	RemoveForeignWidgets();
}

void UIntroCinematicSubsystem::RemoveForeignWidgets() const
{
	UWorld* const World = GetWorld();
	TArray<UUserWidget*> TopLevelWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, TopLevelWidgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* const Widget : TopLevelWidgets)
	{
		if (IsValid(Widget) && Widget != M_Hud && not Widget->IsA<UW_CinematicOverlay>())
		{
			Widget->RemoveFromParent();
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UWidgetComponent*> WidgetComponents(*It);
		for (UWidgetComponent* const WidgetComponent : WidgetComponents)
		{
			if (WidgetComponent->IsVisible())
			{
				WidgetComponent->SetVisibility(false);
			}
		}
	}
}

void UIntroCinematicSubsystem::HandleLevelAdded(ULevel* Level, UWorld* World)
{
	if (not bM_IsActive || World != GetWorld() || Level == nullptr)
	{
		return;
	}
	StopForeignSequences(Level);
	SweepWorld();
	int32 ValidActors = 0;
	for (const AActor* const Actor : Level->Actors)
	{
		ValidActors += IsValid(Actor) ? 1 : 0;
	}
	UE_LOG(LogIntroCinematic, Display, TEXT("INTRO_CINEMATIC level shown %s, %d actors, visible %d"),
	       *Level->GetOutermost()->GetName(), ValidActors, Level->bIsVisible ? 1 : 0);
}

void UIntroCinematicSubsystem::HandleLevelRemoved(ULevel* Level, UWorld* World)
{
	if (not bM_IsActive || World != GetWorld() || Level == nullptr || Level == World->PersistentLevel)
	{
		return;
	}
	FTimerManager& TimerManager = World->GetTimerManager();
	for (AActor* const Actor : Level->Actors)
	{
		if (not IsValid(Actor))
		{
			continue;
		}
		TimerManager.ClearAllTimersForObject(Actor);
		for (UActorComponent* const Component : Actor->GetComponents())
		{
			if (IsValid(Component))
			{
				TimerManager.ClearAllTimersForObject(Component);
			}
		}
	}
}

void UIntroCinematicSubsystem::StopForeignSequences(const ULevel* Level) const
{
	// The old intro map brings its own sequence actors; only the master may play.
	const FSoftObjectPath MasterPath(IntroCinematicConstants::MasterSequencePath);
	for (AActor* const Actor : Level->Actors)
	{
		ALevelSequenceActor* const SequenceActor = Cast<ALevelSequenceActor>(Actor);
		if (SequenceActor == nullptr || FSoftObjectPath(SequenceActor->GetSequence()) == MasterPath)
		{
			continue;
		}
		if (ULevelSequencePlayer* const Player = SequenceActor->GetSequencePlayer())
		{
			Player->Stop();
		}
		SequenceActor->SetActorTickEnabled(false);
	}
}

void UIntroCinematicSubsystem::RemoveFogOfWar() const
{
	UWorld* const World = GetWorld();
	for (TActorIterator<AFowManager> It(World); It; ++It)
	{
		It->SetActorTickEnabled(false);
		It->SetActorHiddenInGame(true);
		// The fog darkens the frame through a post process component on the manager.
		TInlineComponentArray<UPostProcessComponent*> PostProcessComponents(*It);
		for (UPostProcessComponent* const PostProcess : PostProcessComponents)
		{
			if (PostProcess->bEnabled)
			{
				PostProcess->bEnabled = false;
				UE_LOG(LogIntroCinematic, Display, TEXT("INTRO_CINEMATIC fog of war off on %s"), *It->GetName());
			}
		}
	}
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		TArray<FWeightedBlendable>& Blendables = It->Settings.WeightedBlendables.Array;
		for (int32 Index = Blendables.Num() - 1; Index >= 0; --Index)
		{
			const UObject* const Blendable = Blendables[Index].Object;
			if (Blendable != nullptr && Blendable->GetPathName().Contains(IntroCinematicConstants::FogOfWarPathPart))
			{
				UE_LOG(LogIntroCinematic, Display, TEXT("INTRO_CINEMATIC removed blendable %s from %s"),
				       *Blendable->GetPathName(), *It->GetName());
				Blendables.RemoveAt(Index);
			}
		}
	}
}

void UIntroCinematicSubsystem::RemoveGameplayActors() const
{
	if (M_RemovalRules.IsEmpty())
	{
		return;
	}
	const UWorld* const World = GetWorld();
	for (const ULevel* const Level : World->GetLevels())
	{
		if (Level == nullptr || Level == World->PersistentLevel)
		{
			continue;
		}
		// Destroyed, not hidden: gameplay units keep timers that crash once their map is switched off again.
		// Only this play session's copy goes; the map asset is untouched. Destroy() edits Level->Actors, so collect.
		const FString LevelName = Level->GetOutermost()->GetName();
		TArray<AActor*> ActorsToRemove;
		for (AActor* const Actor : Level->Actors)
		{
			if (not IsValid(Actor))
			{
				continue;
			}
			if (GetShouldRemoveActor(Actor, LevelName))
			{
				ActorsToRemove.Add(Actor);
				continue;
			}
			HideRuleMatchedMeshComponents(Actor, LevelName);
		}
		for (AActor* const Actor : ActorsToRemove)
		{
			Actor->Destroy();
		}
	}
}

void UIntroCinematicSubsystem::HideRuleMatchedMeshComponents(const AActor* Actor, const FString& LevelName) const
{
	// Instanced scatter (trees, reeds) lives as components of one actor: a rule can take out one mesh of it.
	if (Actor->ActorHasTag(IntroCinematicConstants::SequencerSpawnedTag) || Actor->IsA<AStaticMeshActor>())
	{
		return;
	}
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Actor);
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		const UStaticMesh* const Mesh = MeshComponent->GetStaticMesh();
		if (Mesh != nullptr && GetIsNameMatchedByRule(Mesh->GetName(), LevelName))
		{
			MeshComponent->SetVisibility(false);
		}
	}
}

bool UIntroCinematicSubsystem::GetIsNameMatchedByRule(const FString& Name, const FString& LevelName) const
{
	for (const TPair<FString, FString>& Rule : M_RemovalRules)
	{
		const bool bIsRuleForLevel = Rule.Value.IsEmpty() || LevelName.Contains(Rule.Value);
		if (bIsRuleForLevel && Name.Contains(Rule.Key))
		{
			return true;
		}
	}
	return false;
}

bool UIntroCinematicSubsystem::GetShouldRemoveActor(const AActor* Actor, const FString& LevelName) const
{
	// The film's own spawnables (tank columns, the sentry) carry Sequencer's tag wherever they were spawned.
	if (Actor->ActorHasTag(IntroCinematicConstants::SequencerSpawnedTag))
	{
		return false;
	}
	const FString ClassName = Actor->GetClass()->GetName();
	FString MeshName;
	if (const AStaticMeshActor* const MeshActor = Cast<AStaticMeshActor>(Actor))
	{
		const UStaticMeshComponent* const MeshComponent = MeshActor->GetStaticMeshComponent();
		const UStaticMesh* const Mesh = MeshComponent != nullptr ? MeshComponent->GetStaticMesh() : nullptr;
		MeshName = Mesh != nullptr ? Mesh->GetName() : FString();
	}
	for (const TPair<FString, FString>& Rule : M_RemovalRules)
	{
		if (not Rule.Value.IsEmpty() && not LevelName.Contains(Rule.Value))
		{
			continue;
		}
		if (ClassName.Contains(Rule.Key) || (not MeshName.IsEmpty() && MeshName.Contains(Rule.Key)))
		{
			return true;
		}
	}
	return false;
}
