// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DirectionFindingManager.h"

#include "Components/AudioComponent.h"
#include "DirectionFindingSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/SupplyPoints/SupplyPoint.h"
#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace DirectionFindingManagerConstants
{
	const FColor PlacementColor(255, 150, 40);
	constexpr float WerkLinkBrightness = 0.55f;
	constexpr float EditorDashSize = 400.f;
	constexpr float WaitForEnemyAISeconds = 0.5f;
	constexpr float JammerFadeOutSeconds = 1.5f;
	const TCHAR* const TargetFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Targets/Blueprints/");
	const TCHAR* const SoundFolder = TEXT("/Game/RTS_Survival/Sounds/OderWarthe/");
	const TCHAR* const InterceptLinePath = TEXT(
		"/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/OderWarthe/");

	TSoftClassPtr<AActor> MakeTargetClass(const FString& BlueprintName)
	{
		return TSoftClassPtr<AActor>(FSoftObjectPath(
			FString(TargetFolder) + BlueprintName + TEXT(".") + BlueprintName + TEXT("_C")));
	}

	TSoftObjectPtr<USoundBase> MakeSound(const FString& RelativePath, const FString& AssetName)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(
			FString(SoundFolder) + RelativePath + AssetName + TEXT(".") + AssetName));
	}
}

ADirectionFindingManager::ADirectionFindingManager()
{
	using namespace DirectionFindingManagerConstants;
	PrimaryActorTick.bCanEverTick = false;
	JammerClass = MakeTargetClass(TEXT("BP_OW_Target_Jammer"));
	CommandPorchClass = MakeTargetClass(TEXT("BP_OW_Target_CommandPorch"));
	RailGateClass = MakeTargetClass(TEXT("BP_OW_Target_RailGate"));
	BurstSounds.Add(EDirectionFindingBurst::Routine, MakeSound(TEXT(""), TEXT("Cue_OW_Burst_Routine")));
	BurstSounds.Add(EDirectionFindingBurst::Reply, MakeSound(TEXT(""), TEXT("Cue_OW_Burst_Reply")));
	BurstSounds.Add(EDirectionFindingBurst::AttackOrder, MakeSound(TEXT(""), TEXT("Cue_OW_Burst_AttackOrder")));
	StaticSounds.Add(EDirectionFindingBurst::Routine, MakeSound(TEXT(""), TEXT("Cue_OW_Static_Routine")));
	StaticSounds.Add(EDirectionFindingBurst::Reply, MakeSound(TEXT(""), TEXT("Cue_OW_Static_Reply")));
	StaticSounds.Add(EDirectionFindingBurst::AttackOrder, MakeSound(TEXT(""), TEXT("Cue_OW_Static_AttackOrder")));
	StationLockOnSound = MakeSound(TEXT(""), TEXT("Cue_OW_StationLockOn"));
	const auto MakeIntercepts = [](const TCHAR* const FirstLine, const TCHAR* const SecondLine)
	{
		FDirectionFindingIntercepts Intercepts;
		for (const TCHAR* const LineName : {FirstLine, SecondLine})
		{
			Intercepts.Lines.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"),
				DirectionFindingManagerConstants::InterceptLinePath, LineName, LineName))));
		}
		return Intercepts;
	};
	InterceptLines.Add(EDirectionFindingBurst::Routine,
	                   MakeIntercepts(TEXT("OderWarthe_I_Routine_A"), TEXT("OderWarthe_I_Routine_B")));
	InterceptLines.Add(EDirectionFindingBurst::Reply,
	                   MakeIntercepts(TEXT("OderWarthe_I_Reply_A"), TEXT("OderWarthe_I_Reply_B")));
	InterceptLines.Add(EDirectionFindingBurst::AttackOrder,
	                   MakeIntercepts(TEXT("OderWarthe_I_Attack_A"), TEXT("OderWarthe_I_Attack_B")));
	JammerBedSound = MakeSound(TEXT("Jammer/"), TEXT("OW_JammerBed_Loop"));
}

FColor ADirectionFindingManager::GetPlacementColor() const
{
	return DirectionFindingManagerConstants::PlacementColor;
}

FString ADirectionFindingManager::GetConfigurationProblem() const
{
	if (Stations.Num() < Settings.StationsNeededForFix)
	{
		return FString::Printf(TEXT("needs %d stations, has %d"), Settings.StationsNeededForFix, Stations.Num());
	}
	for (int32 StationIndex = 0; StationIndex < Stations.Num(); ++StationIndex)
	{
		if (not IsValid(Stations[StationIndex]))
		{
			return FString::Printf(TEXT("station %d is not set"), StationIndex + 1);
		}
	}
	if (Werke.Num() < 2)
	{
		return TEXT("needs at least two Werke");
	}
	if (ForcedCommandPostIndex >= Werke.Num())
	{
		return TEXT("forced command post is not a Werk");
	}
	return FString();
}

void ADirectionFindingManager::NotifyAttackOrder()
{
	TransmitBurst(EDirectionFindingBurst::AttackOrder);
}

bool ADirectionFindingManager::NotifyPlayerAttackedSite(const FName SiteId)
{
	if (not bM_HasStarted || bM_IsFixed)
	{
		return false;
	}
	const double NowSeconds = GetWorldSeconds();
	if (const double* const LastReply = M_LastReplyBySite.Find(SiteId))
	{
		if (NowSeconds - *LastReply < Settings.ReplyCooldownPerSiteSeconds)
		{
			return false;
		}
	}
	M_LastReplyBySite.Add(SiteId, NowSeconds);
	const float Delay = M_Random.FRandRange(Settings.ReplyDelaySeconds.X, Settings.ReplyDelaySeconds.Y);
	FTimerHandle ReplyTimerHandle;
	GetWorldTimerManager().SetTimer(ReplyTimerHandle,
	                                FTimerDelegate::CreateUObject(this, &ADirectionFindingManager::OnReplyBurst),
	                                FMath::Max(Delay, KINDA_SMALL_NUMBER), false);
	return true;
}

void ADirectionFindingManager::SetWerkEliminated(const int32 WerkIndex)
{
	if (not M_Solver.SetWerkEliminated(WerkIndex))
	{
		return;
	}
	CheckFix();
}

void ADirectionFindingManager::ForceBurst(const EDirectionFindingBurst BurstType)
{
	TransmitBurst(BurstType);
}

void ADirectionFindingManager::ForceFix()
{
	if (not bM_HasStarted || bM_IsFixed)
	{
		return;
	}
	Fix();
}

void ADirectionFindingManager::StopJammer()
{
	if (not GetIsValidJammerBedComponent())
	{
		return;
	}
	M_JammerBedComponent->FadeOut(DirectionFindingManagerConstants::JammerFadeOutSeconds, 0.f);
}

bool ADirectionFindingManager::GetIsValidJammerBedComponent() const
{
	if (IsValid(M_JammerBedComponent))
	{
		return true;
	}
	// A manager without a jammer sound has no bed to stop; that is a setup choice, not an error.
	if (not JammerBedSound.IsNull())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_JammerBedComponent",
		                                                     "GetIsValidJammerBedComponent", this);
	}
	return false;
}

FName ADirectionFindingManager::GetCommandPostWerkId() const
{
	const int32 CommandPostIndex = M_Solver.GetCommandPostIndex();
	return Werke.IsValidIndex(CommandPostIndex) ? Werke[CommandPostIndex].WerkId : NAME_None;
}

TArray<AActor*> ADirectionFindingManager::GetCommandPostTargets() const
{
	TArray<AActor*> Targets;
	for (AActor* const Target : M_CommandPostTargets)
	{
		if (IsValid(Target))
		{
			Targets.Add(Target);
		}
	}
	return Targets;
}

float ADirectionFindingManager::GetStationHalfAngle(const int32 StationIndex) const
{
	const FDirectionFindingWedge* const Wedge = M_Solver.GetWedge(StationIndex);
	return Wedge != nullptr && Wedge->bOpen ? Wedge->HalfAngleDegrees : 0.f;
}

bool ADirectionFindingManager::GetIsStationHeldByPlayer(const int32 StationIndex) const
{
	if (not Stations.IsValidIndex(StationIndex) || not IsValid(Stations[StationIndex]))
	{
		return false;
	}
	return Stations[StationIndex]->GetOwningPlayer() == RTSPlayers::HumanPlayer;
}

ADirectionFindingManager* ADirectionFindingManager::FindDirectionFindingManager(const UObject* WorldContextObject)
{
	const UWorld* const World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	const UDirectionFindingSubsystem* const Subsystem = World != nullptr
		                                                   ? World->GetSubsystem<UDirectionFindingSubsystem>()
		                                                   : nullptr;
	return IsValid(Subsystem) ? Subsystem->GetDirectionFindingManager() : nullptr;
}

int32 ADirectionFindingManager::GetStationIndex(const ASupplyPoint* Station) const
{
	for (int32 StationIndex = 0; StationIndex < Stations.Num(); ++StationIndex)
	{
		if (Stations[StationIndex] == Station)
		{
			return StationIndex;
		}
	}
	return INDEX_NONE;
}

void ADirectionFindingManager::GetDrawData(const FBox2D& MapBounds, FDirectionFindingDrawData& OutDrawData) const
{
	OutDrawData.Wedges.Reset();
	OutDrawData.Overlap.Reset();
	if (not bM_HasStarted)
	{
		return;
	}
	const double NowSeconds = GetWorldSeconds();
	int32 HeldDrawn = 0;
	for (int32 StationIndex = 0; StationIndex < M_Solver.GetStationCount(); ++StationIndex)
	{
		const FDirectionFindingWedge* const Wedge = M_Solver.GetWedge(StationIndex);
		if (Wedge == nullptr || not Wedge->bOpen)
		{
			continue;
		}
		const float ShownHalfAngle = FDirectionFindingSolver::GetShownHalfAngle(
			*Wedge, NowSeconds, Settings.NarrowAnimationSeconds);
		FDirectionFindingWedgeDraw& Draw = OutDrawData.Wedges.AddDefaulted_GetRef();
		Draw.bHeld = Wedge->bHeld;
		Draw.Pulse = Wedge->NarrowedAtSeconds < 0.0
			             ? 0.f
			             : 1.f - FMath::Clamp(static_cast<float>(NowSeconds - Wedge->NarrowedAtSeconds) /
			                                  Look.PulseSeconds, 0.f, 1.f);
		FDirectionFindingSolver::BuildWedgePolygon(M_Solver.GetStationLocation(StationIndex),
		                                           Wedge->GetCentreBearingDegrees(ShownHalfAngle), ShownHalfAngle,
		                                           MapBounds, Draw.Polygon);
		if (not Wedge->bHeld || Draw.Polygon.IsEmpty())
		{
			continue;
		}
		if (HeldDrawn == 0)
		{
			OutDrawData.Overlap = Draw.Polygon;
		}
		else
		{
			FDirectionFindingSolver::ClipConvexPolygon(OutDrawData.Overlap, Draw.Polygon);
		}
		HeldDrawn++;
	}
	if (HeldDrawn < 2)
	{
		OutDrawData.Overlap.Reset();
	}
}

void ADirectionFindingManager::BeginPlay()
{
	Super::BeginPlay();
	if (UDirectionFindingSubsystem* const Subsystem = GetWorld()->GetSubsystem<UDirectionFindingSubsystem>())
	{
		Subsystem->RegisterManager(this);
	}
	const FString Problem = GetConfigurationProblem();
	if (not Problem.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("Direction-finding manager ") + GetName() + TEXT(": ") + Problem);
		return;
	}
	BeginPlay_BindStations();
	WaitForEnemyAI(DirectionFindingManagerConstants::WaitForEnemyAISeconds);
}

void ADirectionFindingManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (ASupplyPoint* const Station : Stations)
	{
		if (IsValid(Station))
		{
			Station->OnSupplyPointOwnerChanged.RemoveAll(this);
		}
	}
	// The bed may never have started (no sound set, or the hunt never began); that is not an error here.
	if (IsValid(M_JammerBedComponent))
	{
		M_JammerBedComponent->Stop();
	}
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
		if (UDirectionFindingSubsystem* const Subsystem = World->GetSubsystem<UDirectionFindingSubsystem>())
		{
			Subsystem->UnregisterManager(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FString ADirectionFindingManager::GetPlacementKindName() const
{
	return TEXT("DIRECTION FINDING");
}

void ADirectionFindingManager::BuildEditorShapes(FEnemyPlacementShapes& OutSummary,
                                                 FEnemyPlacementShapes& OutDetail) const
{
	using namespace DirectionFindingManagerConstants;
	const FColor Color = GetPlacementColor();
	const FColor WerkColor = EnemyPlacementColors::GetDimmed(Color, WerkLinkBrightness);
	for (const ASupplyPoint* const Station : Stations)
	{
		if (IsValid(Station))
		{
			OutDetail.DashedLines.Emplace(GetActorLocation(), Station->GetActorLocation(), Color, EditorDashSize);
		}
	}
	for (const FDirectionFindingWerk& Werk : Werke)
	{
		OutDetail.DashedLines.Emplace(GetActorLocation(), Werk.Centre, WerkColor, EditorDashSize);
	}
}

void ADirectionFindingManager::OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
	StartHunt(Difficulty);
}

void ADirectionFindingManager::BeginPlay_BindStations()
{
	for (ASupplyPoint* const Station : Stations)
	{
		Station->OnSupplyPointOwnerChanged.AddUObject(this, &ADirectionFindingManager::OnStationOwnerChanged);
	}
}

void ADirectionFindingManager::StartHunt(const ERTSGameDifficulty Difficulty)
{
	if (bM_HasStarted)
	{
		return;
	}
	M_Difficulty = Difficulty;
	M_Random.Initialize(RandomSeed != 0 ? RandomSeed : static_cast<int32>(FDateTime::Now().GetTicks() % MAX_int32));
	InitSolver(PickCommandPost());
	bM_HasStarted = true;
	SpawnCommandPostTargets();
	StartJammerBed();
	ScheduleRoutineBurst();
	for (int32 StationIndex = 0; StationIndex < Stations.Num(); ++StationIndex)
	{
		ApplyStationOwner(StationIndex);
	}
}

int32 ADirectionFindingManager::PickCommandPost()
{
#if RTS_WITH_MAP_TESTS
	// Map tests and clip captures (-DirectionFindingCommandPost=N) need a known command post.
	int32 TestCommandPost = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("DirectionFindingCommandPost="), TestCommandPost)
		&& Werke.IsValidIndex(TestCommandPost))
	{
		return TestCommandPost;
	}
#endif
	if (Werke.IsValidIndex(ForcedCommandPostIndex))
	{
		return ForcedCommandPostIndex;
	}
	return M_Random.RandRange(0, Werke.Num() - 1);
}

void ADirectionFindingManager::InitSolver(const int32 CommandPostIndex)
{
	TArray<FVector2D> StationLocations;
	for (const ASupplyPoint* const Station : Stations)
	{
		StationLocations.Add(FVector2D(Station->GetActorLocation()));
	}
	TArray<FVector2D> WerkCentres;
	TArray<float> WerkRadii;
	for (const FDirectionFindingWerk& Werk : Werke)
	{
		WerkCentres.Add(FVector2D(Werk.Centre));
		WerkRadii.Add(Werk.RadiusCm);
	}
	M_Solver.Init(StationLocations, WerkCentres, WerkRadii, CommandPostIndex);
}

void ADirectionFindingManager::SpawnCommandPostTargets()
{
	const int32 CommandPostIndex = M_Solver.GetCommandPostIndex();
	if (not Werke.IsValidIndex(CommandPostIndex))
	{
		return;
	}
	const FDirectionFindingWerk& Werk = Werke[CommandPostIndex];
	for (AActor* const Target : {
		     SpawnHiddenTarget(JammerClass, Werk.JammerTransform),
		     SpawnHiddenTarget(CommandPorchClass, Werk.CommandPorchTransform),
		     SpawnHiddenTarget(RailGateClass, Werk.RailGateTransform)
	     })
	{
		if (IsValid(Target))
		{
			M_CommandPostTargets.Add(Target);
		}
	}
	SetTargetsRevealed(false);
}

AActor* ADirectionFindingManager::SpawnHiddenTarget(const TSoftClassPtr<AActor>& TargetClass,
                                                    const FTransform& Transform)
{
	UClass* const LoadedClass = TargetClass.LoadSynchronous();
	if (not IsValid(LoadedClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("Direction-finding manager: command post target class ") +
			TargetClass.ToString() + TEXT(" did not load."));
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.Owner = this;
	return GetWorld()->SpawnActor<AActor>(LoadedClass, Transform, SpawnParameters);
}

void ADirectionFindingManager::SetTargetsRevealed(const bool bRevealed) const
{
	for (AActor* const Target : M_CommandPostTargets)
	{
		if (not IsValid(Target))
		{
			continue;
		}
		Target->SetActorHiddenInGame(not bRevealed);
		Target->SetActorEnableCollision(bRevealed);
	}
}

void ADirectionFindingManager::StartJammerBed()
{
	USoundBase* const JammerBed = JammerBedSound.LoadSynchronous();
	if (not IsValid(JammerBed))
	{
		return;
	}
	// Kept (no auto destroy) so StopJammer can fade it; it dies with the level and EndPlay stops it.
	M_JammerBedComponent = UGameplayStatics::CreateSound2D(this, JammerBed, 1.f, 1.f, 0.f, nullptr, false, false);
	if (not GetIsValidJammerBedComponent())
	{
		return;
	}
	M_JammerBedComponent->Play();
}

void ADirectionFindingManager::OnStationOwnerChanged(ASupplyPoint* Station, uint8 PreviousOwner)
{
	if (not bM_HasStarted)
	{
		// StartHunt reads every station's owner once it runs.
		return;
	}
	ApplyStationOwner(GetStationIndex(Station));
}

void ADirectionFindingManager::ApplyStationOwner(const int32 StationIndex)
{
	if (not Stations.IsValidIndex(StationIndex) || not IsValid(Stations[StationIndex]))
	{
		return;
	}
	const bool bHeld = Stations[StationIndex]->GetOwningPlayer() == RTSPlayers::HumanPlayer;
	const FDirectionFindingWedge* const Wedge = M_Solver.GetWedge(StationIndex);
	if (Wedge == nullptr || (Wedge->bHeld == bHeld && (Wedge->bOpen || not bHeld)))
	{
		return;
	}
	const float ErrorFraction = M_Random.FRandRange(-Settings.MaxErrorFraction, Settings.MaxErrorFraction);
	if (M_Solver.SetStationHeld(StationIndex, bHeld, GetStartHalfAngle(), ErrorFraction))
	{
		PlaySoftSound2D(StationLockOnSound);
	}
	OnStationChanged.Broadcast(StationIndex, bHeld);
	if (bM_IsFixed)
	{
		return;
	}
	CheckFirstOverlap();
	CheckFix();
}

void ADirectionFindingManager::CheckFirstOverlap()
{
	if (bM_HasRaisedFirstOverlap || M_Solver.GetHeldWedgeCount() < 2)
	{
		return;
	}
	bM_HasRaisedFirstOverlap = true;
	// The routine traffic drops from the next interval on.
	ScheduleRoutineBurst();
	OnFirstOverlap.Broadcast();
}

void ADirectionFindingManager::TransmitBurst(const EDirectionFindingBurst BurstType)
{
	if (not bM_HasStarted || bM_IsFixed)
	{
		return;
	}
	const int32 HeardCount = M_Solver.ApplyBurst(GetBurstFactor(BurstType), Settings.MinHalfAngle, GetWorldSeconds());
	if (HeardCount == 0)
	{
		return;
	}
	if (const TSoftObjectPtr<USoundBase>* const BurstSound = BurstSounds.Find(BurstType))
	{
		PlaySoftSound2D(*BurstSound);
	}
	if (const TSoftObjectPtr<USoundBase>* const StaticSound = StaticSounds.Find(BurstType))
	{
		PlaySoftSound2D(*StaticSound);
	}
	ScheduleInterceptLine(BurstType);
	OnBurstHeard.Broadcast(BurstType);
	CheckFix();
}

void ADirectionFindingManager::ScheduleInterceptLine(const EDirectionFindingBurst BurstType)
{
	UWorld* const World = GetWorld();
	const FDirectionFindingIntercepts* const Intercepts = InterceptLines.Find(BurstType);
	if (World == nullptr || Intercepts == nullptr || Intercepts->Lines.IsEmpty())
	{
		return;
	}
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, BurstType]()
	{
		PlayInterceptLine(BurstType);
	}), FMath::Max(InterceptDelaySeconds, UE_KINDA_SMALL_NUMBER), false);
}

void ADirectionFindingManager::PlayInterceptLine(const EDirectionFindingBurst BurstType)
{
	const FDirectionFindingIntercepts* const Intercepts = InterceptLines.Find(BurstType);
	UPlayerPortraitManager* const PortraitManager = FRTS_Statics::GetPlayerPortraitManager(this);
	if (Intercepts == nullptr || Intercepts->Lines.IsEmpty() || not IsValid(PortraitManager))
	{
		return;
	}
	// Routine traffic is background chatter: it never waits behind a line that matters.
	if (BurstType == EDirectionFindingBurst::Routine && PortraitManager->GetIsPortraitPlaying())
	{
		return;
	}
	const int32 LineCount = Intercepts->Lines.Num();
	const int32* const LastLine = M_LastInterceptLine.Find(BurstType);
	int32 LineIndex = FMath::RandRange(0, LineCount - 1);
	if (LineCount > 1 && LastLine != nullptr && LineIndex == *LastLine)
	{
		LineIndex = (LineIndex + 1) % LineCount;
	}
	M_LastInterceptLine.Add(BurstType, LineIndex);
	USoundBase* const Line = Intercepts->Lines[LineIndex].LoadSynchronous();
	if (Line == nullptr)
	{
		return;
	}
	PortraitManager->PlayPortrait(InterceptPortrait, Line);
}

float ADirectionFindingManager::GetBurstFactor(const EDirectionFindingBurst BurstType) const
{
	switch (BurstType)
	{
	case EDirectionFindingBurst::Routine:
		return Settings.RoutineFactor;
	case EDirectionFindingBurst::Reply:
		return Settings.ReplyFactor;
	case EDirectionFindingBurst::AttackOrder:
		return Settings.AttackOrderFactor;
	}
	return 1.f;
}

void ADirectionFindingManager::ScheduleRoutineBurst()
{
	if (bM_IsFixed)
	{
		return;
	}
	const FVector2D Interval = bM_HasRaisedFirstOverlap
		                           ? Settings.RoutineIntervalAfterNoticeSeconds
		                           : Settings.RoutineIntervalSeconds;
	const float Delay = FMath::Max(M_Random.FRandRange(Interval.X, Interval.Y), 1.f);
	GetWorldTimerManager().SetTimer(M_RoutineBurstTimerHandle, this, &ADirectionFindingManager::OnRoutineBurst,
	                                Delay, false);
}

void ADirectionFindingManager::OnRoutineBurst()
{
	TransmitBurst(EDirectionFindingBurst::Routine);
	ScheduleRoutineBurst();
}

void ADirectionFindingManager::OnReplyBurst()
{
	TransmitBurst(EDirectionFindingBurst::Reply);
}

void ADirectionFindingManager::CheckFix()
{
	if (not bM_HasStarted || bM_IsFixed)
	{
		return;
	}
	const bool bAllStationsFix = Settings.bAllStationsGiveFix && M_Solver.GetAreAllStationsHeld();
	if (bAllStationsFix || M_Solver.GetIsFix(Settings.StationsNeededForFix))
	{
		Fix();
	}
}

void ADirectionFindingManager::Fix()
{
	bM_IsFixed = true;
	GetWorldTimerManager().ClearTimer(M_RoutineBurstTimerHandle);
	SetTargetsRevealed(true);
	OnFix.Broadcast(M_Solver.GetCommandPostIndex());
}

void ADirectionFindingManager::PlaySoftSound2D(const TSoftObjectPtr<USoundBase>& Sound) const
{
	if (Sound.IsNull())
	{
		return;
	}
	USoundBase* const LoadedSound = Sound.LoadSynchronous();
	if (not IsValid(LoadedSound))
	{
		RTSFunctionLibrary::ReportError(TEXT("Direction-finding manager: sound ") + Sound.ToString() +
			TEXT(" did not load."));
		return;
	}
	UGameplayStatics::PlaySound2D(this, LoadedSound);
}

float ADirectionFindingManager::GetStartHalfAngle() const
{
	switch (M_Difficulty)
	{
	case ERTSGameDifficulty::Hard:
		return Settings.StartHalfAngleHard;
	case ERTSGameDifficulty::Brutal:
	case ERTSGameDifficulty::Ironman:
		return Settings.StartHalfAngleBrutal;
	default:
		return Settings.StartHalfAngleNormal;
	}
}

double ADirectionFindingManager::GetWorldSeconds() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}
