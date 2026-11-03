// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSMenuBattleDirector.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PostProcessComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "RTSMenuBattleArena.h"
#include "RTSMenuBattleCamera.h"
#include "UnrealClient.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerAudioController/PlayerAudioController.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSMenuBattle, Log, All);

namespace RTSMenuBattleDirectorPrivate
{
	constexpr uint8 GermanPlayer = 1;
	constexpr uint8 SovietPlayer = 2;
	constexpr float GroundTraceHalfHeightCm = 10000.f;
	constexpr float SpawnNavExtentCm = 800.f;
	constexpr float SpawnLiftCm = 100.f;
	constexpr float AircraftSpawnLiftCm = 150.f;
	// A squad counts as alive while its soldiers are still spawning.
	constexpr float SquadSpawnGraceSeconds = 4.f;
	const TCHAR* const WreckClassPrefix = TEXT("Env_Dest");

	bool GetHasGroundBelow(const UWorld& World, const FVector& Location)
	{
		FHitResult GroundHit;
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MenuBattleGroundBelow), false);
		const FVector TraceOffset = FVector::UpVector * GroundTraceHalfHeightCm;
		return World.LineTraceSingleByChannel(GroundHit, Location + TraceOffset, Location - TraceOffset, ECC_Visibility,
		                                      QueryParams);
	}

	bool GetIsAlive(const TWeakObjectPtr<AActor>& Unit)
	{
		return Unit.IsValid() && Unit->IsUnitAlive();
	}

	bool GetHasLivingSoldier(const TWeakObjectPtr<AActor>& SquadActor)
	{
		ASquadController* Squad = Cast<ASquadController>(SquadActor.Get());
		if (not IsValid(Squad))
		{
			return false;
		}
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			if (IsValid(Soldier) && Soldier->IsUnitAlive())
			{
				return true;
			}
		}
		return false;
	}

	int32 CountSquadsStillSpawning(const TArray<TWeakObjectPtr<AActor>>& Squads)
	{
		int32 Spawning = 0;
		for (const TWeakObjectPtr<AActor>& SquadActor : Squads)
		{
			ASquadController* Squad = Cast<ASquadController>(SquadActor.Get());
			Spawning += IsValid(Squad) && Squad->GetSquadUnitsChecked().Num() == 0 ? 1 : 0;
		}
		return Spawning;
	}

	AActor* FindNearestAlive(const AActor& From, const TArray<TWeakObjectPtr<AActor>>& Candidates)
	{
		AActor* Nearest = nullptr;
		float NearestDistanceSquared = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<AActor>& Candidate : Candidates)
		{
			if (not GetIsAlive(Candidate))
			{
				continue;
			}
			const float DistanceSquared = FVector::DistSquared(From.GetActorLocation(), Candidate->GetActorLocation());
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				Nearest = Candidate.Get();
			}
		}
		return Nearest;
	}

	/** Reads bM_StartedAsLanded from the class defaults' movement settings: landed aircraft take off vertically. */
	bool GetAircraftStartsLanded(const UClass& AircraftClass)
	{
		const AAircraftMaster* Defaults = Cast<AAircraftMaster>(AircraftClass.GetDefaultObject());
		const FStructProperty* MovementProperty = FindFProperty<FStructProperty>(
			AAircraftMaster::StaticClass(), TEXT("M_AircraftMovementSettings"));
		if (Defaults == nullptr || MovementProperty == nullptr)
		{
			return true;
		}
		const FBoolProperty* LandedProperty = FindFProperty<FBoolProperty>(MovementProperty->Struct, TEXT("bM_StartedAsLanded"));
		if (LandedProperty == nullptr)
		{
			return true;
		}
		return LandedProperty->GetPropertyValue_InContainer(MovementProperty->ContainerPtrToValuePtr<void>(Defaults));
	}

	const TCHAR* GetSideName(const ERTSMenuBattleSide Side)
	{
		return Side == ERTSMenuBattleSide::German ? TEXT("German") : TEXT("Soviet");
	}

	void DestroyAll(const TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		for (const TWeakObjectPtr<AActor>& Actor : Actors)
		{
			if (Actor.IsValid())
			{
				Actor->Destroy();
			}
		}
	}
}

ARTSMenuBattleDirector::ARTSMenuBattleDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ARTSMenuBattleDirector::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_CollectArenas();
	BeginPlay_InitRandomStream();
	BeginPlay_SpawnCamera();
#if RTS_WITH_MAP_TESTS
	BeginPlay_ReadTestSwitches();
#endif
	// The menu opens on the start camera before the first battle: without battle units no fog has been revealed.
	SetFogOfWarDarkeningEnabled(not bM_KeepBattleUnitsOutOfFog);
	if (M_Arenas.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSMenuBattleDirector: no ARTSMenuBattleArena in the level, no menu battles."));
		SetActorTickEnabled(false);
		return;
	}
	PrepareNextRoster();
}

void ARTSMenuBattleDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (IsValid(World) && M_Battle.ActorSpawnedHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(M_Battle.ActorSpawnedHandle);
		M_Battle.ActorSpawnedHandle.Reset();
	}
	if (M_NextRosterLoadHandle.IsValid())
	{
		M_NextRosterLoadHandle->CancelHandle();
		M_NextRosterLoadHandle.Reset();
	}
	M_CurrentBattleLoadHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void ARTSMenuBattleDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EnsurePlayerControllerSetup();
	switch (M_Phase)
	{
	case ERTSMenuBattlePhase::Starting:
		if (bM_IsNextRosterLoaded)
		{
			StartNextBattle();
		}
		break;
	case ERTSMenuBattlePhase::Fighting:
		TickFighting();
		break;
	case ERTSMenuBattlePhase::FadingOut:
		TickFadingOut();
		break;
	}
	KeepCameraAsViewTarget();
#if RTS_WITH_MAP_TESTS
	TickTestSwitches();
#endif
}

bool ARTSMenuBattleDirector::GetIsValidCamera() const
{
	if (IsValid(M_Camera))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Camera", "GetIsValidCamera", this);
	return false;
}

bool ARTSMenuBattleDirector::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerController", "GetIsValidPlayerController", this);
	return false;
}

float ARTSMenuBattleDirector::GetTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void ARTSMenuBattleDirector::BeginPlay_CollectArenas()
{
	M_Arenas.RemoveAll([](const TObjectPtr<ARTSMenuBattleArena>& Arena) { return not IsValid(Arena); });
	if (not M_Arenas.IsEmpty())
	{
		return;
	}
	for (TActorIterator<ARTSMenuBattleArena> It(GetWorld()); It; ++It)
	{
		M_Arenas.Add(*It);
	}
}

void ARTSMenuBattleDirector::BeginPlay_InitRandomStream()
{
	const int32 Seed = M_Seed != 0 ? M_Seed : FMath::Rand();
	M_RandomStream.Initialize(Seed);
	UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_BEGIN seed=%d arenas=%d"), Seed, M_Arenas.Num());
}

void ARTSMenuBattleDirector::BeginPlay_SpawnCamera()
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_Camera = GetWorld()->SpawnActor<ARTSMenuBattleCamera>(ARTSMenuBattleCamera::StaticClass(), GetActorTransform(),
	                                                        SpawnParameters);
	(void)GetIsValidCamera();
}

void ARTSMenuBattleDirector::EnsurePlayerControllerSetup()
{
	if (M_PlayerController.IsValid())
	{
		return;
	}
	ACPPController* PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not IsValid(PlayerController))
	{
		return;
	}
	M_PlayerController = PlayerController;
	// The German line belongs to player 1: without this every shot and death would request a unit voice line.
	if (UPlayerAudioController* AudioController = PlayerController->GetPlayerAudioController())
	{
		AudioController->SetSuppressRegularVoiceLines(true);
	}
}

void ARTSMenuBattleDirector::KeepCameraAsViewTarget()
{
	// Until the first battle stands, the menu keeps its own start camera.
	if (M_BattleCount == 0 || not M_PlayerController.IsValid() || not IsValid(M_Camera))
	{
		return;
	}
	if (M_PlayerController->GetViewTarget() != M_Camera)
	{
		M_PlayerController->SetViewTarget(M_Camera);
	}
}

void ARTSMenuBattleDirector::FadeCamera(const float FromAlpha, const float ToAlpha) const
{
	if (not GetIsValidPlayerController())
	{
		return;
	}
	APlayerCameraManager* CameraManager = M_PlayerController->PlayerCameraManager;
	if (not IsValid(CameraManager))
	{
		return;
	}
	CameraManager->StartCameraFade(FromAlpha, ToAlpha, M_FadeSeconds, FLinearColor::Black, false, true);
}

void ARTSMenuBattleDirector::PrepareNextRoster()
{
	M_NextRoster = FRTSMenuBattleRoster();
	bM_IsNextRosterLoaded = false;
	ARTSMenuBattleArena* Arena = PickArena();
	if (not IsValid(Arena))
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSMenuBattleDirector: no arena could be picked for the next battle."));
		return;
	}
	const FRTSMenuBattleArenaSettings& Settings = Arena->GetSettings();
	M_NextRoster.Arena = Arena;
	M_NextRoster.CameraSeed = M_RandomStream.RandHelper(TNumericLimits<int32>::Max());
	M_NextRoster.Flavour = PickFlavour();
	M_NextRoster.WaveCount = 1 + M_RandomStream.RandRange(Settings.ReinforcementWavesMin,
	                                                      FMath::Max(Settings.ReinforcementWavesMin,
	                                                                 Settings.ReinforcementWavesMax));
	for (const ERTSMenuBattleSide Side : {ERTSMenuBattleSide::German, ERTSMenuBattleSide::Soviet})
	{
		int32 GroundUnitsLeft = Settings.MaxGroundUnitsPerSide;
		for (int32 Wave = 0; Wave < M_NextRoster.WaveCount; ++Wave)
		{
			const FRTSMenuBattleWaveCounts Counts = GetWaveCounts(Settings, M_NextRoster.Flavour, Wave);
			const int32 Vehicles = FMath::Min(Counts.Vehicles, GroundUnitsLeft);
			const int32 Squads = FMath::Min(Counts.Squads, GroundUnitsLeft - Vehicles);
			GroundUnitsLeft -= Vehicles + Squads;
			AddRosterEntries(Side, ERTSMenuBattleUnitKind::Vehicle, Wave, Vehicles, M_NextRoster);
			AddRosterEntries(Side, ERTSMenuBattleUnitKind::Squad, Wave, Squads, M_NextRoster);
			AddRosterEntries(Side, ERTSMenuBattleUnitKind::Aircraft, Wave, Counts.Aircraft, M_NextRoster);
			AddRosterEntries(Side, ERTSMenuBattleUnitKind::Pit, Wave, Counts.Pits, M_NextRoster);
		}
	}
	if (not M_NextRoster.GetIsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("ARTSMenuBattleDirector: the unit pools gave no units for the next battle."));
		return;
	}
	TArray<FSoftObjectPath> ClassPaths;
	for (const FRTSMenuBattleRosterEntry& Entry : M_NextRoster.Entries)
	{
		ClassPaths.AddUnique(Entry.UnitClass.ToSoftObjectPath());
		UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_ROSTER arena=%s wave=%d side=%s kind=%d slot=%d class=%s"),
		       *Arena->GetName(), Entry.Wave, RTSMenuBattleDirectorPrivate::GetSideName(Entry.Side),
		       static_cast<int32>(Entry.Kind), Entry.SlotIndex, *Entry.UnitClass.ToString());
	}
	M_NextRosterLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ClassPaths, FStreamableDelegate::CreateUObject(this, &ARTSMenuBattleDirector::OnNextRosterLoaded));
	if (not M_NextRosterLoadHandle.IsValid())
	{
		// Nothing left to load: every class of the roster is in memory already.
		OnNextRosterLoaded();
	}
}

ARTSMenuBattleArena* ARTSMenuBattleDirector::PickArena() const
{
	if (M_BattleCount == 0 && not M_Battle.Arena.IsValid() && IsValid(M_OpeningArena))
	{
		return M_OpeningArena;
	}
	// Every cut goes to a landscape (region) picked at random among the others, each equally likely however many arenas
	// it has; within that landscape the arena is a weighted pick, never the arena that is fighting now.
	const ARTSMenuBattleArena* CurrentArena = M_Battle.Arena.Get();
	const FName CurrentRegion = IsValid(CurrentArena) ? CurrentArena->GetSettings().Region : NAME_None;
	TArray<FName> Regions;
	for (const TObjectPtr<ARTSMenuBattleArena>& Arena : M_Arenas)
	{
		if (IsValid(Arena) && Arena->GetSettings().PickWeight > 0.f)
		{
			Regions.AddUnique(Arena->GetSettings().Region);
		}
	}
	if (Regions.Num() > 1)
	{
		Regions.Remove(CurrentRegion);
	}
	if (Regions.IsEmpty())
	{
		return nullptr;
	}
	const FName Region = Regions[M_RandomStream.RandRange(0, Regions.Num() - 1)];
	TArray<ARTSMenuBattleArena*> Candidates;
	float TotalWeight = 0.f;
	for (const TObjectPtr<ARTSMenuBattleArena>& Arena : M_Arenas)
	{
		const bool bIsCurrent = Arena == CurrentArena && M_Arenas.Num() > 1;
		if (not IsValid(Arena) || bIsCurrent || Arena->GetSettings().Region != Region
			|| Arena->GetSettings().PickWeight <= 0.f)
		{
			continue;
		}
		Candidates.Add(Arena);
		TotalWeight += Arena->GetSettings().PickWeight;
	}
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}
	float Pick = M_RandomStream.FRandRange(0.f, TotalWeight);
	for (ARTSMenuBattleArena* Candidate : Candidates)
	{
		Pick -= Candidate->GetSettings().PickWeight;
		if (Pick <= 0.f)
		{
			return Candidate;
		}
	}
	return Candidates.Last();
}

ERTSMenuBattleFlavour ARTSMenuBattleDirector::PickFlavour() const
{
	// Combined arms most often; the others give the sequence of battles its variety.
	constexpr float CombinedArmsWeight = 0.33f;
	constexpr float ArmouredWeight = 0.24f;
	constexpr float InfantryWeight = 0.12f;
	constexpr float EntrenchedWeight = 0.16f;
	const float Pick = M_RandomStream.FRand();
	if (Pick < CombinedArmsWeight)
	{
		return ERTSMenuBattleFlavour::CombinedArms;
	}
	if (Pick < CombinedArmsWeight + ArmouredWeight)
	{
		return ERTSMenuBattleFlavour::Armoured;
	}
	if (Pick < CombinedArmsWeight + ArmouredWeight + InfantryWeight)
	{
		return ERTSMenuBattleFlavour::Infantry;
	}
	if (Pick < CombinedArmsWeight + ArmouredWeight + InfantryWeight + EntrenchedWeight)
	{
		return ERTSMenuBattleFlavour::Entrenched;
	}
	return ERTSMenuBattleFlavour::AirRaid;
}

FRTSMenuBattleWaveCounts ARTSMenuBattleDirector::GetWaveCounts(const FRTSMenuBattleArenaSettings& Settings,
                                                               const ERTSMenuBattleFlavour Flavour,
                                                               const int32 Wave) const
{
	constexpr float AirRaidAircraftChance = 0.75f;
	FRTSMenuBattleWaveCounts Counts;
	const bool bOpening = Wave == 0;
	const int32 VehiclesMax = bOpening ? Settings.OpeningVehiclesMax : Settings.WaveVehiclesMax;
	const int32 SquadsMax = bOpening ? Settings.OpeningSquadsMax : Settings.WaveSquadsMax;
	Counts.Vehicles = M_RandomStream.RandRange(1, FMath::Max(1, VehiclesMax));
	Counts.Squads = M_RandomStream.RandRange(0, SquadsMax);
	// No aircraft over the opening skirmish: the sky fills as the battle grows.
	const float AircraftChance = Flavour == ERTSMenuBattleFlavour::AirRaid
		                             ? AirRaidAircraftChance
		                             : Settings.AircraftChancePerWave;
	Counts.Aircraft = not bOpening && M_RandomStream.FRand() < AircraftChance ? 1 : 0;
	Counts.Pits = bOpening && M_RandomStream.FRand() < Settings.PitChancePerSide ? 1 : 0;
	switch (Flavour)
	{
	case ERTSMenuBattleFlavour::Armoured:
		Counts.Squads = 0;
		Counts.Vehicles += bOpening ? 0 : 1;
		break;
	case ERTSMenuBattleFlavour::Infantry:
		Counts.Vehicles = bOpening ? 0 : M_RandomStream.RandRange(0, 1);
		Counts.Squads = FMath::Max(1, Counts.Squads + 1);
		break;
	case ERTSMenuBattleFlavour::Entrenched:
		// The opening is a duel of pits; the waves bring the assault on them.
		Counts.Pits = bOpening ? M_RandomStream.RandRange(2, FMath::Max(2, Settings.MaxPitsPerSide)) : 0;
		Counts.Vehicles = bOpening ? M_RandomStream.RandRange(0, 1) : Counts.Vehicles;
		break;
	case ERTSMenuBattleFlavour::CombinedArms:
	case ERTSMenuBattleFlavour::AirRaid:
		break;
	}
	return Counts;
}

void ARTSMenuBattleDirector::AddRosterEntries(const ERTSMenuBattleSide Side, const ERTSMenuBattleUnitKind Kind,
                                              const int32 Wave, const int32 Count, FRTSMenuBattleRoster& OutRoster) const
{
	constexpr float LateralJitterCm = 350.f;
	constexpr float DepthJitterCm = 500.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const TSoftClassPtr<AActor> UnitClass = PickUnitClass(Side, Kind);
		if (UnitClass.IsNull())
		{
			return;
		}
		FRTSMenuBattleRosterEntry Entry;
		Entry.UnitClass = UnitClass;
		Entry.Side = Side;
		Entry.Kind = Kind;
		Entry.Wave = Wave;
		Entry.bDrivesIn = Wave == 0 && (Kind == ERTSMenuBattleUnitKind::Vehicle || Kind == ERTSMenuBattleUnitKind::Squad);
		Entry.SlotIndex = Index;
		Entry.SlotJitter = FVector2D(M_RandomStream.FRandRange(-LateralJitterCm, LateralJitterCm),
		                             M_RandomStream.FRandRange(0.f, DepthJitterCm));
		OutRoster.Entries.Add(Entry);
	}
}

TSoftClassPtr<AActor> ARTSMenuBattleDirector::PickUnitClass(const ERTSMenuBattleSide Side,
                                                           const ERTSMenuBattleUnitKind Kind) const
{
	const FRTSMenuBattleFactionPool& Pool = Side == ERTSMenuBattleSide::German ? M_GermanPool : M_SovietPool;
	const TArray<TSoftClassPtr<AActor>>* BasePool = Kind == ERTSMenuBattleUnitKind::Pit ? &Pool.Pits : &Pool.Aircraft;
	const TArray<TSoftClassPtr<AActor>>* VariantPool = nullptr;
	if (Kind == ERTSMenuBattleUnitKind::Vehicle)
	{
		BasePool = &Pool.Vehicles;
		VariantPool = &Pool.VariantVehicles;
	}
	else if (Kind == ERTSMenuBattleUnitKind::Squad)
	{
		BasePool = &Pool.Squads;
		VariantPool = &Pool.VariantSquads;
	}
	const bool bUseVariant = VariantPool != nullptr && VariantPool->Num() > 0
		&& M_RandomStream.FRand() < M_VariantChance;
	const TArray<TSoftClassPtr<AActor>>& Chosen = bUseVariant ? *VariantPool : *BasePool;
	if (Chosen.IsEmpty())
	{
		return TSoftClassPtr<AActor>();
	}
	return Chosen[M_RandomStream.RandRange(0, Chosen.Num() - 1)];
}

void ARTSMenuBattleDirector::OnNextRosterLoaded()
{
	bM_IsNextRosterLoaded = true;
	UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_LOADED arena=%s units=%d"),
	       M_NextRoster.Arena.IsValid() ? *M_NextRoster.Arena->GetName() : TEXT("none"), M_NextRoster.Entries.Num());
}

void ARTSMenuBattleDirector::StartNextBattle()
{
	if (not M_NextRoster.GetIsValid())
	{
		PrepareNextRoster();
		return;
	}
	EndCurrentBattle();
	ARTSMenuBattleArena* Arena = M_NextRoster.Arena.Get();
	M_Battle = FRTSMenuBattleState();
	M_Battle.Arena = Arena;
	M_Battle.BattleNumber = ++M_BattleCount;
	M_Battle.BeginTimeSeconds = GetTimeSeconds();
	M_Battle.ApproachEndSeconds = M_Battle.BeginTimeSeconds + M_ApproachSeconds;
	M_Battle.WaveCount = M_NextRoster.WaveCount;
	M_Battle.bKeepUnitsOutOfFog = bM_KeepBattleUnitsOutOfFog || Arena->GetSettings().bKeepUnitsOutOfFog;
	SetFogOfWarDarkeningEnabled(not M_Battle.bKeepUnitsOutOfFog);
	M_Battle.PendingEntries = M_NextRoster.Entries;
	M_Battle.PendingEntries.StableSort([](const FRTSMenuBattleRosterEntry& A, const FRTSMenuBattleRosterEntry& B)
	{
		return A.Wave < B.Wave;
	});
	// Registered before the units spawn: their team weapons and soldiers leave with the battle too.
	M_Battle.ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &ARTSMenuBattleDirector::OnBattleActorSpawned));
	const int32 CameraSeed = M_NextRoster.CameraSeed;
	const ERTSMenuBattleFlavour Flavour = M_NextRoster.Flavour;
	M_CurrentBattleLoadHandle = M_NextRosterLoadHandle;
	M_NextRosterLoadHandle.Reset();
	M_NextRoster = FRTSMenuBattleRoster();
	bM_IsNextRosterLoaded = false;
	SpawnWave(0);
	if (GetIsValidCamera())
	{
		M_Camera->BeginCoverage(*Arena, CameraSeed, GetViewCentreForArena(*Arena));
	}
	UpdateCameraSubjects();
	M_LastOrderSeconds = -1.f;
	M_Phase = ERTSMenuBattlePhase::Fighting;
	KeepCameraAsViewTarget();
	FadeCamera(1.f, 0.f);
	UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_START battle=%d arena=%s flavour=%s waves=%d outOfFog=%d"),
	       M_Battle.BattleNumber, *Arena->GetName(), *UEnum::GetValueAsString(Flavour), M_Battle.WaveCount,
	       M_Battle.bKeepUnitsOutOfFog ? 1 : 0);
	// The next battle loads while this one is fought.
	PrepareNextRoster();
}

void ARTSMenuBattleDirector::SetFogOfWarDarkeningEnabled(const bool bEnabled) const
{
	// Units out of the fog reveal nothing, so the fog post process would darken all but the ground they spawned on.
	for (TActorIterator<AFowManager> It(GetWorld()); It; ++It)
	{
		TArray<UPostProcessComponent*> PostProcesses;
		It->GetComponents<UPostProcessComponent>(PostProcesses);
		for (UPostProcessComponent* PostProcess : PostProcesses)
		{
			if (IsValid(PostProcess))
			{
				PostProcess->bEnabled = bEnabled;
			}
		}
	}
}

FVector ARTSMenuBattleDirector::GetViewCentreForArena(const ARTSMenuBattleArena& Arena) const
{
	// The centre of the landscape under the arena: the camera looks across that landscape, not off its edge.
	constexpr float MinLandscapeExtentCm = 10000.f;
	FHitResult GroundHit;
	const FVector Location = Arena.GetActorLocation();
	const FVector TraceOffset = FVector::UpVector * RTSMenuBattleDirectorPrivate::GroundTraceHalfHeightCm;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MenuBattleViewCentre), false);
	if (not GetWorld()->LineTraceSingleByChannel(GroundHit, Location + TraceOffset, Location - TraceOffset, ECC_WorldStatic,
	                                             QueryParams) || not IsValid(GroundHit.GetActor()))
	{
		return M_MapViewCentre;
	}
	FVector Origin;
	FVector Extent;
	GroundHit.GetActor()->GetActorBounds(false, Origin, Extent);
	return Extent.Size2D() >= MinLandscapeExtentCm ? Origin : M_MapViewCentre;
}

void ARTSMenuBattleDirector::SpawnWave(const int32 Wave)
{
	const ARTSMenuBattleArena* Arena = M_Battle.Arena.Get();
	if (not IsValid(Arena))
	{
		return;
	}
	int32 Spawned = 0;
	while (M_Battle.PendingEntries.Num() > 0 && M_Battle.PendingEntries[0].Wave <= Wave)
	{
		const FRTSMenuBattleRosterEntry Entry = M_Battle.PendingEntries[0];
		M_Battle.PendingEntries.RemoveAt(0);
		SpawnRosterUnit(Entry, *Arena);
		++Spawned;
	}
	const float Now = GetTimeSeconds();
	M_Battle.NextWave = Wave + 1;
	M_Battle.LastWaveTimeSeconds = Now;
	// Each wave comes a little earlier or later than the arena's interval, so arrivals do not tick like a clock.
	constexpr float IntervalJitterFraction = 0.25f;
	M_Battle.NextWaveTimeSeconds = Now + Arena->GetSettings().WaveIntervalSeconds
		* M_RandomStream.FRandRange(1.f - IntervalJitterFraction, 1.f + IntervalJitterFraction);
	UpdateCameraSubjects();
	UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_WAVE battle=%d wave=%d spawned=%d pending=%d"),
	       M_Battle.BattleNumber, Wave, Spawned, M_Battle.PendingEntries.Num());
}

void ARTSMenuBattleDirector::TickReinforcements()
{
	using namespace RTSMenuBattleDirectorPrivate;
	if (not M_Battle.GetHasPendingWaves())
	{
		return;
	}
	const float Now = GetTimeSeconds();
	// A side that has been wiped out would leave the camera on an empty field: bring the next wave in early.
	constexpr float WipedOutWaveDelaySeconds = 2.f;
	const bool bASideIsWipedOut = CountAliveMobileUnits(M_Battle.German) == 0
		|| CountAliveMobileUnits(M_Battle.Soviet) == 0;
	const bool bSquadsMaySpawn = Now - M_Battle.LastWaveTimeSeconds < SquadSpawnGraceSeconds;
	if (bASideIsWipedOut && not bSquadsMaySpawn)
	{
		M_Battle.NextWaveTimeSeconds = FMath::Min(M_Battle.NextWaveTimeSeconds, Now + WipedOutWaveDelaySeconds);
	}
	if (Now >= M_Battle.NextWaveTimeSeconds)
	{
		SpawnWave(M_Battle.NextWave);
	}
}

void ARTSMenuBattleDirector::SpawnRosterUnit(const FRTSMenuBattleRosterEntry& Entry, const ARTSMenuBattleArena& Arena)
{
	UClass* UnitClass = Entry.UnitClass.Get();
	if (not IsValid(UnitClass))
	{
		UE_LOG(LogRTSMenuBattle, Warning, TEXT("RTS_MENU_BATTLE_CLASS_MISSING class=%s"), *Entry.UnitClass.ToString());
		return;
	}
	const FVector SlotLocation = GetSlotOverGround(Arena, Arena.GetSlotLocation(Entry));
	FVector SpawnLocation = GetGroundedSpawnLocation(SlotLocation, Entry.Kind);
	if (Entry.Kind == ERTSMenuBattleUnitKind::Aircraft && not RTSMenuBattleDirectorPrivate::GetAircraftStartsLanded(*UnitClass))
	{
		// An aircraft that starts airborne cruises at the height it spawns at.
		SpawnLocation.Z += M_AircraftFlightHeight - RTSMenuBattleDirectorPrivate::AircraftSpawnLiftCm;
	}
	const FTransform SpawnTransform(FRotator(0.f, Arena.GetFacingYaw(Entry.Side), 0.f), SpawnLocation);
	AActor* Unit = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*GetWorld(), UnitClass, SpawnTransform,
	                                                         GetOwningPlayer(Entry.Side));
	if (not IsValid(Unit))
	{
		UE_LOG(LogRTSMenuBattle, Warning, TEXT("RTS_MENU_BATTLE_SPAWN_FAILED class=%s"), *UnitClass->GetName());
		return;
	}
	if (Entry.Kind == ERTSMenuBattleUnitKind::Pit)
	{
		// The pit's own shell is not ground: put it back where the trace found the ground, like a placed pit.
		Unit->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	FRTSMenuBattleSideUnits& Side = Entry.Side == ERTSMenuBattleSide::German ? M_Battle.German : M_Battle.Soviet;
	if (Entry.Kind == ERTSMenuBattleUnitKind::Aircraft)
	{
		Side.Aircraft.Add(Unit);
		// Set before the first order makes it take off: the aircraft climbs to and flies its runs at this height.
		if (AAircraftMaster* Aircraft = Cast<AAircraftMaster>(Unit))
		{
			Aircraft->SetPreviewTakeOffHeight(M_AircraftFlightHeight);
		}
	}
	else if (Cast<ASquadController>(Unit) != nullptr)
	{
		Side.Squads.Add(Unit);
	}
	else
	{
		Side.GroundUnits.Add(Unit);
	}
	PrepareBattleUnit(Unit);
	if (Entry.bDrivesIn)
	{
		OrderDriveToFormUp(*Unit, Entry, Arena);
	}
	UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_SPAWNED wave=%d side=%s unit=%s at=%s"), Entry.Wave,
	       RTSMenuBattleDirectorPrivate::GetSideName(Entry.Side), *Unit->GetName(), *SpawnLocation.ToCompactString());
}

void ARTSMenuBattleDirector::OrderDriveToFormUp(AActor& Unit, const FRTSMenuBattleRosterEntry& Entry,
                                                const ARTSMenuBattleArena& Arena) const
{
	ICommands* Commands = Cast<ICommands>(&Unit);
	if (Commands == nullptr)
	{
		return;
	}
	const FVector FormUp = GetGroundedSpawnLocation(Arena.GetFormUpLocation(Entry), Entry.Kind);
	Commands->MoveToLocation(FormUp, true, FRotator(0.f, Arena.GetFacingYaw(Entry.Side), 0.f));
}

FVector ARTSMenuBattleDirector::GetGroundedSpawnLocation(const FVector& Location, const ERTSMenuBattleUnitKind Kind) const
{
	using namespace RTSMenuBattleDirectorPrivate;
	FVector Grounded = Location;
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MenuBattleGround), false);
	const FVector TraceOffset = FVector::UpVector * GroundTraceHalfHeightCm;
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Location + TraceOffset, Location - TraceOffset, ECC_Visibility,
	                                         QueryParams))
	{
		Grounded = GroundHit.ImpactPoint;
	}
	if (Kind == ERTSMenuBattleUnitKind::Aircraft)
	{
		return Grounded + FVector::UpVector * AircraftSpawnLiftCm;
	}
	if (Kind == ERTSMenuBattleUnitKind::Pit)
	{
		// A pit sits in the ground where it is placed; the navmesh has a hole there once it stands.
		return Grounded;
	}
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation NavLocation;
	if (IsValid(NavigationSystem)
		&& NavigationSystem->ProjectPointToNavigation(Grounded, NavLocation, FVector(SpawnNavExtentCm)))
	{
		Grounded = NavLocation.Location;
	}
	return Grounded + FVector::UpVector * SpawnLiftCm;
}

FVector ARTSMenuBattleDirector::GetSlotOverGround(const ARTSMenuBattleArena& Arena, const FVector& SlotLocation) const
{
	constexpr float PullStepCm = 500.f;
	constexpr int32 MaxPullSteps = 60;
	const FVector BattleCentre = (Arena.GetLineCenter(ERTSMenuBattleSide::German)
		+ Arena.GetLineCenter(ERTSMenuBattleSide::Soviet)) * 0.5f;
	FVector Candidate = SlotLocation;
	for (int32 Step = 0; Step < MaxPullSteps; ++Step)
	{
		if (RTSMenuBattleDirectorPrivate::GetHasGroundBelow(*GetWorld(), Candidate))
		{
			return Candidate;
		}
		const FVector ToCentre(BattleCentre.X - Candidate.X, BattleCentre.Y - Candidate.Y, 0.f);
		if (ToCentre.Size() <= PullStepCm)
		{
			break;
		}
		Candidate += ToCentre.GetSafeNormal() * PullStepCm;
	}
	return SlotLocation;
}

void ARTSMenuBattleDirector::EndCurrentBattle()
{
	using namespace RTSMenuBattleDirectorPrivate;
	if (not M_Battle.Arena.IsValid())
	{
		return;
	}
	if (M_Battle.ActorSpawnedHandle.IsValid())
	{
		GetWorld()->RemoveOnActorSpawnedHandler(M_Battle.ActorSpawnedHandle);
	}
	UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_END battle=%d germanAlive=%d sovietAlive=%d seconds=%.1f leftovers=%d hiddenShownAgain=%d"),
	       M_Battle.BattleNumber, CountAliveGroundUnits(M_Battle.German), CountAliveGroundUnits(M_Battle.Soviet),
	       GetTimeSeconds() - M_Battle.BeginTimeSeconds, M_Battle.SpawnedByBattle.Num(), M_Battle.HiddenUnitsShownAgain);
	if (not M_Battle.GetIsDecided())
	{
		LogUndecidedSurvivors();
	}
	if (IsValid(M_Camera))
	{
		M_Camera->StopCoverage();
	}
	// Squads first: a squad controller may take its soldiers along, the remaining soldiers follow.
	for (FRTSMenuBattleSideUnits* Side : {&M_Battle.German, &M_Battle.Soviet})
	{
		DestroyAll(Side->Squads);
		DestroyAll(Side->GroundUnits);
		DestroyAll(Side->Aircraft);
	}
	DestroyAll(M_Battle.SpawnedByBattle);
	M_Battle = FRTSMenuBattleState();
	M_CurrentBattleLoadHandle.Reset();
}

void ARTSMenuBattleDirector::LogUndecidedSurvivors() const
{
	using namespace RTSMenuBattleDirectorPrivate;
	// A battle that ran out of time: name the survivors of the losing side, they are what nobody could reach.
	const FRTSMenuBattleSideUnits& Smaller = CountAliveGroundUnits(M_Battle.German) < CountAliveGroundUnits(M_Battle.Soviet)
		                                         ? M_Battle.German
		                                         : M_Battle.Soviet;
	for (const TWeakObjectPtr<AActor>& Unit : Smaller.GroundUnits)
	{
		if (GetIsAlive(Unit))
		{
			UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_UNDECIDED_SURVIVOR unit=%s at=%s"), *Unit->GetName(),
			       *Unit->GetActorLocation().ToCompactString());
		}
	}
}

void ARTSMenuBattleDirector::OnBattleActorSpawned(AActor* SpawnedActor)
{
	if (not IsValid(SpawnedActor))
	{
		return;
	}
	const bool bIsWreck = SpawnedActor->GetClass()->GetName().StartsWith(RTSMenuBattleDirectorPrivate::WreckClassPrefix);
	const bool bIsBattleUnitPart = SpawnedActor->IsA<ATeamWeapon>() || SpawnedActor->IsA<ASquadUnit>();
	if (bIsWreck || bIsBattleUnitPart)
	{
		M_Battle.SpawnedByBattle.Add(SpawnedActor);
	}
}

void ARTSMenuBattleDirector::TickFighting()
{
	if (not M_Battle.GetIsActive())
	{
		if (bM_IsNextRosterLoaded)
		{
			StartNextBattle();
		}
		return;
	}
	AbsorbSquadSoldiers(M_Battle.German);
	AbsorbSquadSoldiers(M_Battle.Soviet);
	KeepBattleUnitsPresentable(M_Battle.German);
	KeepBattleUnitsPresentable(M_Battle.Soviet);
	const float Now = GetTimeSeconds();
	const bool bOpeningUnitsFormingUp = Now < M_Battle.ApproachEndSeconds;
	if (bOpeningUnitsFormingUp)
	{
		UpdateCameraSubjects();
	}
	else if (M_LastOrderSeconds < 0.f || Now - M_LastOrderSeconds >= M_OrderIntervalSeconds)
	{
		M_LastOrderSeconds = Now;
		IssueAttackOrders(M_Battle.German, M_Battle.Soviet, GetOwningPlayer(ERTSMenuBattleSide::German));
		IssueAttackOrders(M_Battle.Soviet, M_Battle.German, GetOwningPlayer(ERTSMenuBattleSide::Soviet));
		UpdateCameraSubjects();
	}
	TickReinforcements();
	UpdateBattleDecision();
	if (not GetIsReadyToLeaveBattle())
	{
		return;
	}
	if (not bM_IsNextRosterLoaded && not M_NextRoster.GetIsValid() && not M_NextRosterLoadHandle.IsValid())
	{
		// The roster prepared at the start of this battle failed; try again rather than film this field forever.
		PrepareNextRoster();
	}
	if (bM_IsNextRosterLoaded)
	{
		FadeCamera(0.f, 1.f);
		M_FadeOutStartSeconds = Now;
		M_Phase = ERTSMenuBattlePhase::FadingOut;
	}
}

void ARTSMenuBattleDirector::TickFadingOut()
{
	KeepBattleUnitsPresentable(M_Battle.German);
	KeepBattleUnitsPresentable(M_Battle.Soviet);
	if (GetTimeSeconds() - M_FadeOutStartSeconds < M_FadeSeconds)
	{
		return;
	}
	StartNextBattle();
}

void ARTSMenuBattleDirector::AbsorbSquadSoldiers(FRTSMenuBattleSideUnits& Side) const
{
	for (const TWeakObjectPtr<AActor>& SquadActor : Side.Squads)
	{
		ASquadController* Squad = Cast<ASquadController>(SquadActor.Get());
		const URTSComponent* SquadRTS = IsValid(Squad) ? Squad->FindComponentByClass<URTSComponent>() : nullptr;
		if (SquadRTS == nullptr)
		{
			continue;
		}
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			const TWeakObjectPtr<AActor> SoldierActor(Soldier);
			if (not IsValid(Soldier) || Side.GroundUnits.Contains(SoldierActor))
			{
				continue;
			}
			Side.GroundUnits.Add(SoldierActor);
			FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, SquadRTS->GetOwningPlayer());
			PrepareBattleUnit(Soldier);
		}
	}
}

void ARTSMenuBattleDirector::PrepareBattleUnit(AActor* Unit) const
{
	if (not IsValid(Unit))
	{
		return;
	}
	if (bM_HideUnitWidgets)
	{
		// Health bars are screen-space widgets that the health component shows on damage; this keeps them off.
		if (const UHealthComponent* Health = Unit->FindComponentByClass<UHealthComponent>())
		{
			Health->MakeHealthBarInvisible();
		}
	}
	if (not M_Battle.bKeepUnitsOutOfFog)
	{
		return;
	}
	// Out of the fog system: the fog would hide the Soviet line from the camera and keep the German line out of the
	// AI's vision, so neither side would fire.
	if (UFowComp* Fow = Unit->FindComponentByClass<UFowComp>())
	{
		Fow->StopFowParticipation();
	}
}

void ARTSMenuBattleDirector::KeepBattleUnitsPresentable(const FRTSMenuBattleSideUnits& Side)
{
	using DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	for (const TArray<TWeakObjectPtr<AActor>>* Units : {&Side.GroundUnits, &Side.Aircraft})
	{
		for (const TWeakObjectPtr<AActor>& Unit : *Units)
		{
			if (not Unit.IsValid())
			{
				continue;
			}
			if (bM_HideUnitWidgets)
			{
				TArray<UWidgetComponent*> Widgets;
				Unit->GetComponents<UWidgetComponent>(Widgets, true);
				for (UWidgetComponent* Widget : Widgets)
				{
					if (IsValid(Widget) && not Widget->bHiddenInGame)
					{
						Widget->SetHiddenInGame(true);
					}
				}
			}
			if (not M_Battle.bKeepUnitsOutOfFog)
			{
				continue;
			}
			// Player 1 units must be in enemy vision for the AI to target them; player 2 units must not be hidden.
			Unit->Tags.AddUnique(TagUnitInEnemyVision);
			if (Unit->IsHidden())
			{
				Unit->SetActorHiddenInGame(false);
				++M_Battle.HiddenUnitsShownAgain;
			}
			TArray<AActor*> Attached;
			Unit->GetAttachedActors(Attached, true, true);
			for (AActor* Child : Attached)
			{
				if (IsValid(Child) && Child->IsHidden())
				{
					Child->SetActorHiddenInGame(false);
				}
			}
		}
	}
}

void ARTSMenuBattleDirector::IssueAttackOrders(const FRTSMenuBattleSideUnits& Attackers,
                                               const FRTSMenuBattleSideUnits& Defenders, const uint8 AttackerPlayer)
{
	using namespace RTSMenuBattleDirectorPrivate;
	// Soldiers are commanded through their squad: vehicles, squad controllers and aircraft take the orders.
	TArray<TWeakObjectPtr<AActor>> Commanded;
	// Pits fight from where they stand: their turrets pick their own targets.
	for (const TWeakObjectPtr<AActor>& Unit : Attackers.GroundUnits)
	{
		if (Cast<ASquadUnit>(Unit.Get()) == nullptr && Cast<ABuildingExpansion>(Unit.Get()) == nullptr)
		{
			Commanded.Add(Unit);
		}
	}
	Commanded.Append(Attackers.Squads);
	Commanded.Append(Attackers.Aircraft);
	for (const TWeakObjectPtr<AActor>& Attacker : Commanded)
	{
		const bool bIsSquad = Cast<ASquadController>(Attacker.Get()) != nullptr;
		if (bIsSquad ? not GetHasLivingSoldier(Attacker) : not GetIsAlive(Attacker))
		{
			continue;
		}
		const TWeakObjectPtr<AActor>* OrderedTarget = M_Battle.OrderedTargets.Find(Attacker);
		const bool bIsAdvancing = M_Battle.AdvanceOrderSeconds.Contains(Attacker);
		const float* LastAttackOrder = M_Battle.AttackOrderSeconds.Find(Attacker);
		const bool bOrderIsFresh = LastAttackOrder != nullptr && GetTimeSeconds() - *LastAttackOrder < M_ReorderSeconds;
		if (OrderedTarget != nullptr && GetIsAlive(*OrderedTarget) && not bIsAdvancing && bOrderIsFresh)
		{
			continue;
		}
		AActor* Target = FindNearestAlive(*Attacker.Get(), Defenders.GroundUnits);
		ICommands* Commands = Cast<ICommands>(Attacker.Get());
		if (Target == nullptr || Commands == nullptr)
		{
			continue;
		}
		if (not RTSFunctionLibrary::RTSIsVisibleTarget(Target, AttackerPlayer))
		{
			OrderAdvanceOnHiddenTarget(Attacker, *Target);
			continue;
		}
		OrderAttack(Attacker, *Target);
	}
}

void ARTSMenuBattleDirector::OrderAttack(const TWeakObjectPtr<AActor>& Attacker, AActor& Target)
{
	ICommands* Commands = Cast<ICommands>(Attacker.Get());
	if (Commands == nullptr)
	{
		return;
	}
	// An attacker that stood still since its last order (its path to the target was aborted) is driven most of the way
	// to the target; it shoots on the way and gets a new attack order next time.
	constexpr float StuckMoveCm = 300.f;
	constexpr float CloseEnoughCm = 4000.f;
	constexpr float UnstickFraction = 0.7f;
	const FVector AttackerLocation = Attacker->GetActorLocation();
	const FVector TargetLocation = Target.GetActorLocation();
	const FVector* LastOrderLocation = M_Battle.AttackOrderLocations.Find(Attacker);
	const bool bStuck = LastOrderLocation != nullptr
		&& FVector::Dist2D(*LastOrderLocation, AttackerLocation) < StuckMoveCm
		&& FVector::Dist2D(AttackerLocation, TargetLocation) > CloseEnoughCm;
	if (bStuck)
	{
		const FVector Toward = FMath::Lerp(AttackerLocation, TargetLocation, UnstickFraction);
		Commands->MoveToLocation(Toward, true, FRotator(0.f, (TargetLocation - AttackerLocation).Rotation().Yaw, 0.f));
	}
	else
	{
		Commands->AttackActor(&Target, true);
	}
	M_Battle.OrderedTargets.Add(Attacker, &Target);
	M_Battle.AdvanceOrderSeconds.Remove(Attacker);
	M_Battle.AttackOrderSeconds.Add(Attacker, GetTimeSeconds());
	M_Battle.AttackOrderLocations.Add(Attacker, AttackerLocation);
}

void ARTSMenuBattleDirector::OrderAdvanceOnHiddenTarget(const TWeakObjectPtr<AActor>& Attacker, AActor& Target)
{
	// An attack order on a target the fog hides does nothing: drive toward it instead, the turrets engage whatever
	// comes into view, and the next round of orders turns this into an attack once the target is seen. The move is
	// renewed now and then, as the target moves and the attacker may have arrived and stopped.
	constexpr float AdvanceRenewSeconds = 6.f;
	const float Now = GetTimeSeconds();
	const TWeakObjectPtr<AActor>* OrderedTarget = M_Battle.OrderedTargets.Find(Attacker);
	const float* LastAdvanceSeconds = M_Battle.AdvanceOrderSeconds.Find(Attacker);
	const bool bSameTarget = OrderedTarget != nullptr && OrderedTarget->Get() == &Target;
	if (bSameTarget && LastAdvanceSeconds != nullptr && Now - *LastAdvanceSeconds < AdvanceRenewSeconds)
	{
		return;
	}
	ICommands* Commands = Cast<ICommands>(Attacker.Get());
	if (Commands == nullptr)
	{
		return;
	}
	const FVector TargetLocation = Target.GetActorLocation();
	const FRotator FacingTarget(0.f, (TargetLocation - Attacker->GetActorLocation()).Rotation().Yaw, 0.f);
	Commands->MoveToLocation(TargetLocation, true, FacingTarget);
	M_Battle.OrderedTargets.Add(Attacker, &Target);
	M_Battle.AdvanceOrderSeconds.Add(Attacker, Now);
}

void ARTSMenuBattleDirector::UpdateCameraSubjects() const
{
	if (not IsValid(M_Camera))
	{
		return;
	}
	// Ground units only: aircraft circle far out after their runs and would pull the framing apart.
	M_Camera->SetSubjects(M_Battle.German.GroundUnits, M_Battle.Soviet.GroundUnits);
}

void ARTSMenuBattleDirector::UpdateBattleDecision()
{
	using namespace RTSMenuBattleDirectorPrivate;
	if (M_Battle.GetIsDecided())
	{
		return;
	}
	// While reinforcements are still coming, a wiped-out side is only a lull: the next wave brings it back.
	if (M_Battle.GetHasPendingWaves())
	{
		return;
	}
	const float Now = GetTimeSeconds();
	const bool bSquadsMaySpawn = Now - M_Battle.LastWaveTimeSeconds < SquadSpawnGraceSeconds;
	// Pits alone do not hold the field: a side whose vehicles and soldiers are gone has lost.
	const int32 GermanAlive = CountAliveMobileUnits(M_Battle.German)
		+ (bSquadsMaySpawn ? CountSquadsStillSpawning(M_Battle.German.Squads) : 0);
	const int32 SovietAlive = CountAliveMobileUnits(M_Battle.Soviet)
		+ (bSquadsMaySpawn ? CountSquadsStillSpawning(M_Battle.Soviet.Squads) : 0);
	if (GermanAlive > 0 && SovietAlive > 0)
	{
		return;
	}
	M_Battle.DecidedTimeSeconds = Now;
	UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_DECIDED battle=%d winner=%s survivors=%d seconds=%.1f"),
	       M_Battle.BattleNumber, GermanAlive > 0 ? TEXT("German") : (SovietAlive > 0 ? TEXT("Soviet") : TEXT("none")),
	       FMath::Max(GermanAlive, SovietAlive), Now - M_Battle.BeginTimeSeconds);
}

bool ARTSMenuBattleDirector::GetIsReadyToLeaveBattle() const
{
	const float Now = GetTimeSeconds();
	if (M_Battle.GetIsDecided())
	{
		return Now - M_Battle.DecidedTimeSeconds >= M_DecidedHoldSeconds;
	}
	return not M_Battle.GetHasPendingWaves() && Now - M_Battle.LastWaveTimeSeconds >= M_MaxBattleSeconds;
}

int32 ARTSMenuBattleDirector::CountAliveMobileUnits(const FRTSMenuBattleSideUnits& Side)
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<AActor>& Unit : Side.GroundUnits)
	{
		const bool bIsPit = Cast<ABuildingExpansion>(Unit.Get()) != nullptr;
		Alive += not bIsPit && RTSMenuBattleDirectorPrivate::GetIsAlive(Unit) ? 1 : 0;
	}
	return Alive;
}

int32 ARTSMenuBattleDirector::CountAliveGroundUnits(const FRTSMenuBattleSideUnits& Side)
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<AActor>& Unit : Side.GroundUnits)
	{
		Alive += RTSMenuBattleDirectorPrivate::GetIsAlive(Unit) ? 1 : 0;
	}
	return Alive;
}

uint8 ARTSMenuBattleDirector::GetOwningPlayer(const ERTSMenuBattleSide Side)
{
	// Player 0 would fire as player 1 (weapon data normalises it), so the Germans are player 1 and the Soviets 2.
	return Side == ERTSMenuBattleSide::German
		       ? RTSMenuBattleDirectorPrivate::GermanPlayer
		       : RTSMenuBattleDirectorPrivate::SovietPlayer;
}

#if RTS_WITH_MAP_TESTS
void ARTSMenuBattleDirector::BeginPlay_ReadTestSwitches()
{
	FParse::Value(FCommandLine::Get(), TEXT("MenuBattleShots="), M_ScreenshotIntervalSeconds);
	FParse::Value(FCommandLine::Get(), TEXT("MenuBattleQuitAfter="), M_QuitAfterSeconds);
	if (FParse::Param(FCommandLine::Get(), TEXT("MenuBattleGameplayFog")))
	{
		bM_KeepBattleUnitsOutOfFog = false;
	}
}

void ARTSMenuBattleDirector::TickTestSwitches()
{
	const float RealSeconds = GetWorld()->GetRealTimeSeconds();
	if (M_ScreenshotIntervalSeconds > 0.f && RealSeconds - M_LastScreenshotSeconds >= M_ScreenshotIntervalSeconds)
	{
		M_LastScreenshotSeconds = RealSeconds;
		const FString Filename = FPaths::ProjectSavedDir() / TEXT("MenuBattleShots")
			/ FString::Printf(TEXT("MenuBattle_%03d.png"), M_ScreenshotIndex++);
		FScreenshotRequest::RequestScreenshot(Filename, true, false);
		UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_SCREENSHOT file=%s battle=%d"), *Filename,
		       M_Battle.BattleNumber);
	}
	if (M_QuitAfterSeconds > 0.f && RealSeconds >= M_QuitAfterSeconds)
	{
		M_QuitAfterSeconds = 0.f;
		UE_LOG(LogRTSMenuBattle, Display, TEXT("RTS_MENU_BATTLE_QUIT battles=%d"), M_BattleCount);
		FPlatformMisc::RequestExit(false, TEXT("MenuBattleQuitAfter"));
	}
}
#endif
