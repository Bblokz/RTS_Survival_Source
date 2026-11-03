// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalPointsOfInterestSubsystem.h"

#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/DecalActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "PCGGraph.h"
#include "TimerManager.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOIPCGNodes.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOIPlanner.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOIPreview.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOIProp.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOISettings.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"
#include "RTS_Survival/Environment/Wanderer/Wanderer.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

DEFINE_LOG_CATEGORY(LogSurvivalPOI);

const FName USurvivalPointsOfInterestSubsystem::SpawnedTag = TEXT("SurvivalPOI");

namespace SurvivalPOISubsystemConstants
{
	const TCHAR* const SettingsPath =
		TEXT("/Game/RTS_Survival/SurvivalMode/PointsOfInterest/DA_SurvivalPointsOfInterest.DA_SurvivalPointsOfInterest");
	constexpr float GroundTraceUp = 20000.f;
	constexpr float GroundTraceDown = 50000.f;
	constexpr double VolumeHalfHeight = 50000.0;
	constexpr float MinAnimalSpacing = 170.f;
	constexpr int32 AnimalSpotAttempts = 12;
	// A herd that bolted ignores units for this long, so it settles before it can bolt again.
	constexpr double AlarmCooldownSeconds = 6.0;
	// The herd's spread on top of the alarm radius (measured from the herd's centre).
	constexpr float HerdReachMargin = 300.f;
	constexpr double FrameWatchSeconds = 20.0;
	// How often sleepers check whether they are seen, and how long after their last on-screen frame they fall asleep.
	constexpr float SleepCheckSeconds = 0.25f;
	constexpr double SleepAfterUnseenSeconds = 0.5;
}

namespace
{
	/** @brief Writes reward entries into a scavengeable object's protected ScavengeResources map through reflection. */
	void SurvivalPOI_ApplyRewardOverride(AActor& Actor, const TMap<FName, int32>& Rewards)
	{
		const FMapProperty* MapProperty = FindFProperty<FMapProperty>(Actor.GetClass(), TEXT("ScavengeResources"));
		const UEnum* Enum = StaticEnum<ERTSResourceType>();
		if (MapProperty == nullptr or not IsValid(Enum))
		{
			UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI %s has no ScavengeResources to set"), *Actor.GetName());
			return;
		}
		FScriptMapHelper Helper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(&Actor));
		Helper.EmptyValues();
		for (const TPair<FName, int32>& Reward : Rewards)
		{
			const int64 Value = Enum->GetValueByNameString(TEXT("ERTSResourceType::") + Reward.Key.ToString());
			if (Value == INDEX_NONE)
			{
				UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI unknown resource %s"), *Reward.Key.ToString());
				continue;
			}
			const ERTSResourceType Key = static_cast<ERTSResourceType>(Value);
			FScavengeAmount Amount;
			Amount.bIsSetAmount = true;
			Amount.SetAmount = Reward.Value;
			Amount.Chance = 100;
			Helper.AddPair(&Key, &Amount);
		}
	}

	/** @brief Calls AScavengeableObject::SetupAsDestroyedVehicle (protected, Blueprint callable) through reflection. */
	void SurvivalPOI_ApplyTankReward(AActor& Actor, const FName TankSubtype)
	{
		UFunction* Function = Actor.FindFunction(TEXT("SetupAsDestroyedVehicle"));
		const UEnum* Enum = StaticEnum<ETankSubtype>();
		const int64 Value = IsValid(Enum) ? Enum->GetValueByNameString(TEXT("ETankSubtype::") + TankSubtype.ToString()) : INDEX_NONE;
		if (Function == nullptr or Value == INDEX_NONE)
		{
			UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI cannot set up %s as destroyed %s"), *Actor.GetName(), *TankSubtype.ToString());
			return;
		}
		TArray<uint8> Parameters;
		Parameters.SetNumZeroed(Function->ParmsSize);
		TFieldIterator<FProperty> Iterator(Function);
		FProperty* FirstParameter = Iterator ? *Iterator : nullptr;
		if (FirstParameter != nullptr and FirstParameter->HasAnyPropertyFlags(CPF_Parm))
		{
			void* ValuePointer = FirstParameter->ContainerPtrToValuePtr<void>(Parameters.GetData());
			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(FirstParameter))
			{
				EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePointer, Value);
			}
			else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(FirstParameter))
			{
				ByteProperty->SetIntPropertyValue(ValuePointer, Value);
			}
		}
		Actor.ProcessEvent(Function, Parameters.GetData());
	}

	/** @brief Sets a numeric member of a reflected struct property (protected designer structs of Blueprint parents). */
	bool SurvivalPOI_SetStructNumber(UObject& Object, const FName StructName, const FName MemberName, const double Value)
	{
		const FStructProperty* StructProperty = FindFProperty<FStructProperty>(Object.GetClass(), StructName);
		if (StructProperty == nullptr)
		{
			return false;
		}
		void* StructData = StructProperty->ContainerPtrToValuePtr<void>(&Object);
		const FProperty* Member = StructProperty->Struct->FindPropertyByName(MemberName);
		if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Member))
		{
			FloatProperty->SetPropertyValue_InContainer(StructData, static_cast<float>(Value));
			return true;
		}
		if (const FIntProperty* IntProperty = CastField<FIntProperty>(Member))
		{
			IntProperty->SetPropertyValue_InContainer(StructData, FMath::RoundToInt(Value));
			return true;
		}
		return false;
	}
}

bool USurvivalPointsOfInterestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (not Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return IsValid(World) and (World->WorldType == EWorldType::Game or World->WorldType == EWorldType::PIE);
}

void USurvivalPointsOfInterestSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SpawnTimer);
		World->GetTimerManager().ClearTimer(M_AlarmTimer);
		World->GetTimerManager().ClearTimer(M_SleepTimer);
	}
	if (M_LoadHandle.IsValid())
	{
		M_LoadHandle->CancelHandle();
		M_LoadHandle.Reset();
	}
	M_OnSpawnFinished = nullptr;
	Super::Deinitialize();
}

USurvivalPointsOfInterestSettings* USurvivalPointsOfInterestSubsystem::LoadSettings(UObject* Outer)
{
	USurvivalPointsOfInterestSettings* Settings = LoadObject<USurvivalPointsOfInterestSettings>(
		nullptr, SurvivalPOISubsystemConstants::SettingsPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (IsValid(Settings))
	{
		return Settings;
	}
	UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI settings asset missing, using the code defaults"));
	Settings = NewObject<USurvivalPointsOfInterestSettings>(Outer);
	Settings->FillDefaults();
	return Settings;
}

int32 USurvivalPointsOfInterestSubsystem::ResolveMatchSeed() const
{
#if RTS_WITH_MAP_TESTS
	int32 CommandLineSeed = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("POISeed="), CommandLineSeed))
	{
		return CommandLineSeed;
	}
#endif
	if (M_Settings->SeedMode == ESurvivalPOISeedMode::Fixed)
	{
		return M_Settings->FixedSeed;
	}
	return static_cast<int32>(HashCombine(GetTypeHash(FPlatformTime::Cycles64()), GetTypeHash(FMath::Rand())) & 0x7fffffff);
}

void USurvivalPointsOfInterestSubsystem::StartForMatch(ASurvivalModeDirector* Director)
{
	UWorld* World = IsValid(Director) ? Director->GetWorld() : nullptr;
	USurvivalPointsOfInterestSubsystem* Subsystem = IsValid(World) ? World->GetSubsystem<USurvivalPointsOfInterestSubsystem>() : nullptr;
	if (not IsValid(Subsystem))
	{
		return;
	}
	Subsystem->GenerateForMatch(Director);
}

void USurvivalPointsOfInterestSubsystem::GenerateForMatch(ASurvivalModeDirector* Director)
{
	UWorld* World = GetWorld();
	if (M_State != ESurvivalPOIGenerationState::Idle or not IsValid(Director) or not IsValid(World))
	{
		return;
	}
#if RTS_WITH_MAP_TESTS
	StartFrameWatch();
	if (FParse::Param(FCommandLine::Get(), TEXT("SurvivalPOIDisable")))
	{
		// Attribution runs: the same match without points of interest, to see whose the generation hitch is.
		UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI disabled by -SurvivalPOIDisable"));
		return;
	}
#endif
	M_Director = Director;
	M_Settings = LoadSettings(this);
	M_MatchSeed = ResolveMatchSeed();
	TArray<ASurvivalStartLocation*> Starts;
	for (const TWeakObjectPtr<ASurvivalStartLocation>& Start : Director->GetStartLocations())
	{
		Starts.Add(Start.Get());
	}
	M_Planner = MakeShared<FSurvivalPOIPlanner>(*World, *M_Settings, M_MatchSeed, Starts);
	M_Projector = MakeShared<FSurvivalGroundProjector>(*World, SurvivalPOISubsystemConstants::GroundTraceUp,
	                                                   SurvivalPOISubsystemConstants::GroundTraceDown);
	M_Stats = FSurvivalPOIGenerationStats();
	M_Stats.StartSeconds = FPlatformTime::Seconds();
	M_State = ESurvivalPOIGenerationState::SearchingSites;
	const FBox2D Bounds = M_Planner->GetContext().LandscapeBounds;
	UE_LOG(LogSurvivalPOI, Log, TEXT("SURVIVAL_POI generating, seed %d, %d starts, %d sectors, landscape x %.0f..%.0f y %.0f..%.0f, settings %s"),
	       M_MatchSeed, Starts.Num(), M_Planner->GetSectors().Num(), Bounds.Min.X, Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Y,
	       *M_Settings->GetPathName());
	WarnIfLayoutStale();
	StartGraph();
#if RTS_WITH_MAP_TESTS
	FSurvivalPOIPreview::StartIfRequested(this);
#endif
}

#if RTS_WITH_MAP_TESTS
void USurvivalPointsOfInterestSubsystem::StartFrameWatch()
{
	// The worst frame in the 20 s after the director's call, with or without points of interest (-SurvivalPOIDisable).
	const double WatchStart = FPlatformTime::Seconds();
	TSharedRef<double> LastFrame = MakeShared<double>(WatchStart);
	TSharedRef<TPair<double, double>> Worst = MakeShared<TPair<double, double>>(0.0, 0.0);
	const bool bDisabled = FParse::Param(FCommandLine::Get(), TEXT("SurvivalPOIDisable"));
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WatchStart, LastFrame, Worst, bDisabled](const float)
	{
		const double Now = FPlatformTime::Seconds();
		const double FrameMs = (Now - *LastFrame) * 1000.0;
		*LastFrame = Now;
		if (FrameMs > Worst->Key)
		{
			*Worst = TPair<double, double>(FrameMs, Now - WatchStart);
		}
		if (Now - WatchStart < SurvivalPOISubsystemConstants::FrameWatchSeconds)
		{
			return true;
		}
		UE_LOG(LogSurvivalPOI, Display, TEXT("SURVIVAL_POI frame watch (POIs %s): worst frame %.1f ms at +%.1f s in the %.0f s after the director's call"),
		       bDisabled ? TEXT("off") : TEXT("on"), Worst->Key, Worst->Value, SurvivalPOISubsystemConstants::FrameWatchSeconds);
		return false;
	}));
}
#endif

void USurvivalPointsOfInterestSubsystem::WarnIfLayoutStale() const
{
#if not UE_BUILD_SHIPPING
	// The layout exclusions are a copy of the level tools' JSON; a newer JSON means the terrain moved since the copy.
	const FSurvivalPOILayoutExclusions& Layout = M_Settings->Placement.Layout;
	if (Layout.LayoutFile.IsEmpty())
	{
		UE_LOG(LogSurvivalPOI, Warning, TEXT("SURVIVAL_POI layout exclusions are empty: run Tools/poi_sync_layout.py"));
		return;
	}
	const FString Path = FPaths::ProjectDir() / Layout.LayoutFile;
	if (not FPaths::FileExists(Path))
	{
		return;
	}
	const FString CurrentMD5 = LexToString(FMD5Hash::HashFile(*Path));
	if (not CurrentMD5.Equals(Layout.LayoutMD5, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogSurvivalPOI, Warning,
		       TEXT("SURVIVAL_POI layout exclusions are stale (%s changed since the copy): run Tools/poi_sync_layout.py"),
		       *Layout.LayoutFile);
		return;
	}
	UE_LOG(LogSurvivalPOI, Log, TEXT("SURVIVAL_POI layout exclusions current: %d ramps, %d areas, %d obstacles, %d routes"),
	       Layout.Ramps.Num(), Layout.Areas.Num(), Layout.Obstacles.Num(), Layout.Routes.Num());
#endif
}

void USurvivalPointsOfInterestSubsystem::ThrottlePropTicks(AActor& Actor) const
{
	const float Interval = M_Settings->PropTickInterval;
	if (Interval <= 0.f)
	{
		return;
	}
	if (Actor.PrimaryActorTick.IsTickFunctionEnabled())
	{
		Actor.SetActorTickInterval(Interval);
	}
	TInlineComponentArray<UActorComponent*> Components(&Actor);
	for (UActorComponent* Component : Components)
	{
		// Effects keep their own tick (a throttled Niagara system would stutter).
		if (IsValid(Component) and Component->IsComponentTickEnabled() and not Component->IsA<UNiagaraComponent>())
		{
			Component->SetComponentTickInterval(Interval);
		}
	}
}

void USurvivalPointsOfInterestSubsystem::StartOffscreenSleep()
{
	M_Sleepers.Reset();
	for (const FSurvivalWildlifeRecord& Record : M_WildlifeRecords)
	{
		for (const TWeakObjectPtr<AActor>& Animal : Record.Actors)
		{
			// The kites are one Niagara system: a paused system would not render, so it would never wake.
			if (Animal.IsValid() and not Animal->IsA<ANiagaraActor>())
			{
				AddOffscreenSleeper(*Animal, true);
			}
		}
	}
	for (const FSurvivalPOISpawnRecord& Record : M_SpawnRecords)
	{
		if (Record.Actor.IsValid())
		{
			AddOffscreenSleeper(*Record.Actor, false);
		}
	}
	UWorld* World = GetWorld();
	if (not IsValid(World) or M_Sleepers.IsEmpty())
	{
		return;
	}
	TWeakObjectPtr<USurvivalPointsOfInterestSubsystem> WeakThis(this);
	World->GetTimerManager().SetTimer(M_SleepTimer, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->UpdateOffscreenSleepers();
		}
	}), SurvivalPOISubsystemConstants::SleepCheckSeconds, true);
	UE_LOG(LogSurvivalPOI, Log, TEXT("SURVIVAL_POI off-screen sleep: %d actors (animals and props with effects)"), M_Sleepers.Num());
}

void USurvivalPointsOfInterestSubsystem::AddOffscreenSleeper(AActor& Actor, const bool bSleepsActorTick)
{
	FSurvivalPOIOffscreenSleeper Sleeper;
	Sleeper.Actor = &Actor;
	Sleeper.bSleepsActorTick = bSleepsActorTick and Actor.PrimaryActorTick.IsTickFunctionEnabled();
	TInlineComponentArray<UPrimitiveComponent*> Primitives(&Actor);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (UNiagaraComponent* Effect = Cast<UNiagaraComponent>(Primitive))
		{
			Sleeper.Effects.Add(Effect);
			Sleeper.EffectTicked.Add(Effect->IsComponentTickEnabled());
			continue;
		}
		if (IsValid(Primitive) and Primitive->IsVisible())
		{
			Sleeper.VisibleParts.Add(Primitive);
		}
	}
	// Nothing to put to sleep, or nothing that could ever wake it.
	if ((not Sleeper.bSleepsActorTick and Sleeper.Effects.IsEmpty()) or Sleeper.VisibleParts.IsEmpty())
	{
		return;
	}
	M_Sleepers.Add(MoveTemp(Sleeper));
}

void USurvivalPointsOfInterestSubsystem::UpdateOffscreenSleepers()
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	for (FSurvivalPOIOffscreenSleeper& Sleeper : M_Sleepers)
	{
		if (not Sleeper.Actor.IsValid())
		{
			continue;
		}
		const bool bOnScreen = GetIsSleeperOnScreen(Sleeper, *World);
		if (bOnScreen != Sleeper.bAwake)
		{
			SetSleeperAwake(Sleeper, bOnScreen);
		}
	}
}

bool USurvivalPointsOfInterestSubsystem::GetIsSleeperOnScreen(const FSurvivalPOIOffscreenSleeper& Sleeper, const UWorld& World)
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Part : Sleeper.VisibleParts)
	{
		if (Part.IsValid() and World.TimeSince(Part->GetLastRenderTimeOnScreen()) <= SurvivalPOISubsystemConstants::SleepAfterUnseenSeconds)
		{
			return true;
		}
	}
	return false;
}

void USurvivalPointsOfInterestSubsystem::SetSleeperAwake(FSurvivalPOIOffscreenSleeper& Sleeper, const bool bAwake)
{
	Sleeper.bAwake = bAwake;
	if (Sleeper.bSleepsActorTick)
	{
		Sleeper.Actor->SetActorTickEnabled(bAwake);
	}
	for (int32 Index = 0; Index < Sleeper.Effects.Num(); ++Index)
	{
		UNiagaraComponent* Effect = Sleeper.Effects[Index].Get();
		if (not IsValid(Effect))
		{
			continue;
		}
		Effect->SetPaused(not bAwake);
		if (Sleeper.EffectTicked[Index])
		{
			Effect->SetComponentTickEnabled(bAwake);
		}
	}
}

void USurvivalPointsOfInterestSubsystem::MarkOffscreenSleepersAwake()
{
	for (FSurvivalPOIOffscreenSleeper& Sleeper : M_Sleepers)
	{
		Sleeper.bAwake = true;
	}
}

FIntPoint USurvivalPointsOfInterestSubsystem::GetOffscreenSleeperCounts() const
{
	int32 Awake = 0;
	for (const FSurvivalPOIOffscreenSleeper& Sleeper : M_Sleepers)
	{
		Awake += Sleeper.Actor.IsValid() and Sleeper.bAwake ? 1 : 0;
	}
	return FIntPoint(Awake, M_Sleepers.Num());
}

void USurvivalPointsOfInterestSubsystem::CheckWildlifeAlarms()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) or not GetIsValidSettings())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	for (FSurvivalWildlifeRecord& Record : M_WildlifeRecords)
	{
		const FSurvivalWildlifeEntry* Entry = M_Settings->Wildlife.IsValidIndex(Record.Group.EntryIndex)
			                                      ? &M_Settings->Wildlife[Record.Group.EntryIndex]
			                                      : nullptr;
		if (Entry == nullptr or Entry->Kind != ESurvivalWildlifeKind::GroundHerd or Entry->AlarmRadius <= 0.f
			or Now < Record.AlarmCooldownUntil)
		{
			continue;
		}
		const TOptional<FVector> Threat = FindThreatNearHerd(Record, Entry->AlarmRadius);
		if (not Threat.IsSet())
		{
			continue;
		}
		int32 Fled = 0;
		int32 Animals = 0;
		for (const TWeakObjectPtr<AActor>& Animal : Record.Actors)
		{
			AWanderer* Wanderer = Cast<AWanderer>(Animal.Get());
			if (not IsValid(Wanderer))
			{
				continue;
			}
			++Animals;
			// A sleeping (off-screen) animal must tick to run.
			Wanderer->SetActorTickEnabled(true);
			const bool bAlreadyFleeing = Wanderer->GetWandererState() == EWandererState::Fleeing;
			Fled += bAlreadyFleeing or Wanderer->StartFleeing(Threat.GetValue(), true) ? 1 : 0;
		}
		Record.AlarmCooldownUntil = Now + SurvivalPOISubsystemConstants::AlarmCooldownSeconds;
		UE_LOG(LogSurvivalPOI, Log, TEXT("SURVIVAL_POI herd at (%.0f, %.0f) bolts from a unit at (%.0f, %.0f): %d of %d animals running"),
		       Record.Group.Center.X, Record.Group.Center.Y, Threat->X, Threat->Y, Fled, Animals);
	}
}

TOptional<FVector> USurvivalPointsOfInterestSubsystem::FindThreatNearHerd(const FSurvivalWildlifeRecord& Record,
                                                                          const float Radius) const
{
	FVector Centre = FVector::ZeroVector;
	int32 Alive = 0;
	for (const TWeakObjectPtr<AActor>& Animal : Record.Actors)
	{
		if (Animal.IsValid())
		{
			Centre += Animal->GetActorLocation();
			++Alive;
		}
	}
	if (Alive == 0)
	{
		return {};
	}
	Centre /= Alive;
	const float Reach = Radius + SurvivalPOISubsystemConstants::HerdReachMargin;
	// Units are pawns; the player's camera pawn is the one controlled pawn that is not a unit.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn* Pawn = *It;
		if (not IsValid(Pawn) or Pawn->IsPlayerControlled())
		{
			continue;
		}
		if (FVector::DistSquared2D(Pawn->GetActorLocation(), Centre) < FMath::Square(Reach))
		{
			return Pawn->GetActorLocation();
		}
	}
	return {};
}


void USurvivalPointsOfInterestSubsystem::StartGraph()
{
	UWorld* World = GetWorld();
	UPCGGraphInterface* Graph = M_Settings->Graph.LoadSynchronous();
	if (not IsValid(Graph) or not IsValid(World) or not GetIsValidPlanner())
	{
		M_State = ESurvivalPOIGenerationState::Failed;
		RTSFunctionLibrary::ReportError(TEXT("Survival points of interest: the PCG graph did not load: ")
			+ M_Settings->Graph.ToString());
		return;
	}
	const FBox2D Bounds = M_Planner->GetContext().LandscapeBounds;
	const FVector2D Center = Bounds.bIsValid ? Bounds.GetCenter() : FVector2D::ZeroVector;
	const FVector2D HalfSize = Bounds.bIsValid ? Bounds.GetExtent() : FVector2D(50000.0);
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_Volume = World->SpawnActor<ASurvivalPOIPCGVolume>(ASurvivalPOIPCGVolume::StaticClass(),
	                                                    FTransform(FVector(Center, 0.0)), Parameters);
	if (not IsValid(M_Volume))
	{
		M_State = ESurvivalPOIGenerationState::Failed;
		RTSFunctionLibrary::ReportError(TEXT("Survival points of interest: could not spawn the PCG volume."));
		return;
	}
	M_Volume->StartGeneration(this, Graph, M_MatchSeed, FVector(HalfSize, SurvivalPOISubsystemConstants::VolumeHalfHeight));
}

bool USurvivalPointsOfInterestSubsystem::GetIsValidSettings() const
{
	if (IsValid(M_Settings))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Settings", "GetIsValidSettings", this);
	return false;
}

bool USurvivalPointsOfInterestSubsystem::GetIsValidPlanner() const
{
	if (M_Planner.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Planner", "GetIsValidPlanner", this);
	return false;
}

// ---------------------------------------------------------------------------
// Graph nodes
// ---------------------------------------------------------------------------

bool USurvivalPointsOfInterestSubsystem::StepSiteSearch()
{
	if (not GetIsValidPlanner() or not GetIsValidSettings())
	{
		return true;
	}
	const double StepStart = FPlatformTime::Seconds();
	constexpr double SiteStepBudgetSeconds = 0.004;
	const bool bDone = M_Planner->StepSiteSearch(M_Settings->Placement.HeightSamplesPerStep, SiteStepBudgetSeconds);
	const double StepMs = (FPlatformTime::Seconds() - StepStart) * 1000.0;
	M_Stats.WorstSiteStepMs = FMath::Max(M_Stats.WorstSiteStepMs, StepMs);
	++M_Stats.SiteSteps;
	if (not bDone)
	{
		return false;
	}
	M_Stats.SitesChosenSeconds = FPlatformTime::Seconds();
	UE_LOG(LogSurvivalPOI, Log, TEXT("SURVIVAL_POI sites chosen: %d (%d candidates not drivable from their start, %d outside the nav band, %d without room for their main object, %d losing a scavengeable or capture target)"),
	       M_Planner->GetSites().Num(), M_Planner->GetUnreachableCandidates(), M_Planner->GetNavBandRejects(), M_Planner->GetAnchorRejects(),
	       M_Planner->GetKeyObjectRejects());
	for (const FString& Note : M_Planner->GetPlanNotes())
	{
		UE_LOG(LogSurvivalPOI, Log, TEXT("SURVIVAL_POI plan note: %s"), *Note);
	}
	for (const FSurvivalPOISite& Site : M_Planner->GetSites())
	{
		UE_LOG(LogSurvivalPOI, Log, TEXT("SURVIVAL_POI site %d %s%s sector %d owner %d at (%.0f, %.0f, %.0f) yaw %.0f layout %d"),
		       Site.SiteIndex, *SurvivalPOINames::GetTypeName(Site.Type), Site.bIsLandmark ? TEXT(" (landmark)") : TEXT(""),
		       Site.Sector, Site.OwnerStart, Site.Center.X, Site.Center.Y, Site.Center.Z, Site.YawDegrees, Site.SubLayout);
	}
	return true;
}

const TArray<FSurvivalPOISite>& USurvivalPointsOfInterestSubsystem::GetPlannedSites() const
{
	static const TArray<FSurvivalPOISite> NoSites;
	return M_Planner.IsValid() ? M_Planner->GetSites() : NoSites;
}

void USurvivalPointsOfInterestSubsystem::ComposeSites(const TArray<FSurvivalPOISite>& Sites, TArray<FSurvivalPOIPlacedObject>& OutObjects)
{
	if (not GetIsValidPlanner())
	{
		return;
	}
	const double Start = FPlatformTime::Seconds();
	M_Sites = Sites;
	for (const FSurvivalPOISite& Site : Sites)
	{
		M_Planner->ComposeSite(Site, OutObjects);
	}
	M_Stats.ComposeMs = (FPlatformTime::Seconds() - Start) * 1000.0;
}

void USurvivalPointsOfInterestSubsystem::PlanWildlife(const TArray<FSurvivalPOISite>& Sites, TArray<FSurvivalWildlifeGroup>& OutGroups)
{
	if (not GetIsValidPlanner())
	{
		return;
	}
	const double Start = FPlatformTime::Seconds();
	M_Planner->PlanWildlife(Sites, OutGroups);
	M_Stats.WildlifePlanMs = (FPlatformTime::Seconds() - Start) * 1000.0;
}

void USurvivalPointsOfInterestSubsystem::SpawnFromGraph(TArray<FSurvivalPOIPlacedObject>&& Objects,
                                                        TArray<FSurvivalWildlifeGroup>&& Groups, TFunction<void()> OnFinished)
{
	if (bM_SpawnRequested or not GetIsValidSettings())
	{
		return;
	}
	bM_SpawnRequested = true;
	M_PendingObjects = MoveTemp(Objects);
	M_PendingGroups = MoveTemp(Groups);
	M_OnSpawnFinished = MoveTemp(OnFinished);
	M_NextObject = 0;
	M_NextGroup = 0;
	TSet<FSoftObjectPath> Paths;
	for (const FSurvivalPOIPlacedObject& Object : M_PendingObjects)
	{
		if (const FSurvivalPOIAsset* Asset = M_Settings->GetAssetByKey(Object.AssetKey))
		{
			Paths.Add(Asset->GetLoadPath());
		}
	}
	for (const FSurvivalWildlifeGroup& Group : M_PendingGroups)
	{
		if (not M_Settings->Wildlife.IsValidIndex(Group.EntryIndex))
		{
			continue;
		}
		const FSurvivalWildlifeEntry& Entry = M_Settings->Wildlife[Group.EntryIndex];
		Paths.Add(Entry.Kind == ESurvivalWildlifeKind::NiagaraFlock ? Entry.NiagaraSystem.ToSoftObjectPath()
			          : Entry.ActorClass.ToSoftObjectPath());
	}
	Paths.Remove(FSoftObjectPath());
	M_State = ESurvivalPOIGenerationState::Loading;
	UE_LOG(LogSurvivalPOI, Log, TEXT("SURVIVAL_POI loading %d assets for %d objects and %d wildlife groups"), Paths.Num(),
	       M_PendingObjects.Num(), M_PendingGroups.Num());
	if (Paths.IsEmpty())
	{
		OnAssetsLoaded();
		return;
	}
	TWeakObjectPtr<USurvivalPointsOfInterestSubsystem> WeakThis(this);
	M_LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths.Array(), FStreamableDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAssetsLoaded();
			}
		}), FStreamableManager::AsyncLoadHighPriority);
}

void USurvivalPointsOfInterestSubsystem::OnAssetsLoaded()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_State = ESurvivalPOIGenerationState::Spawning;
	TWeakObjectPtr<USurvivalPointsOfInterestSubsystem> WeakThis(this);
	M_SpawnTimer = World->GetTimerManager().SetTimerForNextTick([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->SpawnStep();
		}
	});
}

void USurvivalPointsOfInterestSubsystem::SpawnStep()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) or not GetIsValidSettings())
	{
		return;
	}
	constexpr double SpawnStepBudgetSeconds = 0.006;
	const double StepStart = FPlatformTime::Seconds();
	const int32 End = FMath::Min(M_PendingObjects.Num(), M_NextObject + M_Settings->ObjectsPerSpawnStep);
	for (; M_NextObject < End; ++M_NextObject)
	{
		if (FPlatformTime::Seconds() - StepStart > SpawnStepBudgetSeconds)
		{
			break;
		}
		FSurvivalPOISpawnRecord& Record = M_SpawnRecords.AddDefaulted_GetRef();
		Record.Planned = M_PendingObjects[M_NextObject];
		Record.Actor = SpawnObject(Record.Planned, Record.Seat);
		if (not Record.Actor.IsValid())
		{
			++M_Stats.FailedSpawns;
			continue;
		}
		Record.SpawnedTransform = Record.Actor->GetActorTransform();
	}
	if (M_NextObject >= M_PendingObjects.Num() and M_PendingGroups.IsValidIndex(M_NextGroup))
	{
		SpawnWildlifeGroup(M_PendingGroups[M_NextGroup++]);
	}
	M_Stats.WorstSpawnStepMs = FMath::Max(M_Stats.WorstSpawnStepMs, (FPlatformTime::Seconds() - StepStart) * 1000.0);
	++M_Stats.SpawnSteps;
	if (M_NextObject >= M_PendingObjects.Num() and M_NextGroup >= M_PendingGroups.Num())
	{
		FinishSpawning();
		return;
	}
	TWeakObjectPtr<USurvivalPointsOfInterestSubsystem> WeakThis(this);
	M_SpawnTimer = World->GetTimerManager().SetTimerForNextTick([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->SpawnStep();
		}
	});
}

void USurvivalPointsOfInterestSubsystem::FinishSpawning()
{
	M_State = ESurvivalPOIGenerationState::Finished;
	M_Stats.FinishedSeconds = FPlatformTime::Seconds();
	int32 Animals = 0;
	for (const FSurvivalWildlifeRecord& Record : M_WildlifeRecords)
	{
		Animals += Record.Actors.Num();
	}
	UE_LOG(LogSurvivalPOI, Log,
	       TEXT("SURVIVAL_POI finished: %d sites, %d objects (%d failed), %d wildlife groups with %d actors, %.2f s, worst site step %.1f ms, worst spawn step %.1f ms"),
	       M_Sites.Num(), M_SpawnRecords.Num(), M_Stats.FailedSpawns, M_WildlifeRecords.Num(), Animals,
	       M_Stats.FinishedSeconds - M_Stats.StartSeconds, M_Stats.WorstSiteStepMs, M_Stats.WorstSpawnStepMs);
	if (M_Stats.FailedSpawns > 0)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Survival points of interest: %d objects failed to spawn (classes did not load?)."), M_Stats.FailedSpawns));
	}
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<USurvivalPointsOfInterestSubsystem> WeakThis(this);
		World->GetTimerManager().SetTimer(M_AlarmTimer, FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->CheckWildlifeAlarms();
			}
		}), M_Settings->WildlifeAlarmInterval, true);
	}
	StartOffscreenSleep();
	if (M_OnSpawnFinished)
	{
		TFunction<void()> OnFinished = MoveTemp(M_OnSpawnFinished);
		M_OnSpawnFinished = nullptr;
		OnFinished();
	}
}

// ---------------------------------------------------------------------------
// Spawning objects
// ---------------------------------------------------------------------------

void USurvivalPointsOfInterestSubsystem::MarkSpawned(AActor& Actor) const
{
	Actor.Tags.AddUnique(SpawnedTag);
}

AActor* USurvivalPointsOfInterestSubsystem::SpawnObject(const FSurvivalPOIPlacedObject& Object, FSurvivalPOISeatReport& OutSeat)
{
	const FSurvivalPOIAsset* Asset = M_Settings->GetAssetByKey(Object.AssetKey);
	if (Asset == nullptr)
	{
		return nullptr;
	}
	if (Asset->GetIsDecal())
	{
		AActor* Decal = SpawnDecal(*Asset, Object);
		if (IsValid(Decal))
		{
			MarkSpawned(*Decal);
		}
		return Decal;
	}
	AActor* Actor = not Asset->ActorClass.IsNull() ? SpawnActorClass(*Asset, Object) : SpawnMeshProp(*Asset, Object);
	if (not IsValid(Actor))
	{
		return nullptr;
	}
	MarkSpawned(*Actor);
	if (not Asset->RewardTankSubtype.IsNone())
	{
		SurvivalPOI_ApplyTankReward(*Actor, Asset->RewardTankSubtype);
	}
	if (not Asset->RewardOverride.IsEmpty())
	{
		SurvivalPOI_ApplyRewardOverride(*Actor, Asset->RewardOverride);
	}
	ThrottlePropTicks(*Actor);
	// Ground decals of the composition lie under and against the objects; they must not paint onto the hulls.
	if (Object.Role != ESurvivalPOIRole::Crater)
	{
		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents<UPrimitiveComponent>(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			Component->SetReceivesDecals(false);
		}
	}
	if (M_Projector.IsValid())
	{
		SurvivalPOISeating::SeatActor(*Actor, Asset->SeatMode, Object.SinkCm, Asset->MaxSlopeDegrees, *M_Projector, OutSeat);
	}
	return Actor;
}

AActor* USurvivalPointsOfInterestSubsystem::SpawnActorClass(const FSurvivalPOIAsset& Asset, const FSurvivalPOIPlacedObject& Object)
{
	UClass* Class = Asset.ActorClass.Get();
	if (not IsValid(Class))
	{
		++M_Stats.FailedLoads;
		UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI class not loaded: %s"), *Asset.ActorClass.ToString());
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Transform(FRotator(0.f, Object.YawDegrees, 0.f), Object.Location, FVector(Object.Scale));
	return GetWorld()->SpawnActor<AActor>(Class, Transform, Parameters);
}

AActor* USurvivalPointsOfInterestSubsystem::SpawnMeshProp(const FSurvivalPOIAsset& Asset, const FSurvivalPOIPlacedObject& Object)
{
	UStaticMesh* Mesh = Asset.Mesh.Get();
	if (not IsValid(Mesh))
	{
		++M_Stats.FailedLoads;
		UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI mesh not loaded: %s"), *Asset.Mesh.ToString());
		return nullptr;
	}
	const FTransform Transform(FRotator(0.f, Object.YawDegrees, 0.f), Object.Location, FVector(Object.Scale));
	ASurvivalPOIProp* Prop = GetWorld()->SpawnActorDeferred<ASurvivalPOIProp>(
		ASurvivalPOIProp::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Prop))
	{
		return nullptr;
	}
	Prop->InitProp(Mesh, Asset.bBlocksNavigation);
	Prop->FinishSpawning(Transform);
	return Prop;
}

AActor* USurvivalPointsOfInterestSubsystem::SpawnDecal(const FSurvivalPOIAsset& Asset, const FSurvivalPOIPlacedObject& Object)
{
	UMaterialInterface* Material = Asset.DecalMaterial.Get();
	if (not IsValid(Material))
	{
		++M_Stats.FailedLoads;
		UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI decal material not loaded: %s"), *Asset.DecalMaterial.ToString());
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Pitch -90 projects down; the decal's Y axis (its length) then runs along yaw + 90, so turn it back by 90.
	const FRotator Rotation(-90.f, Object.YawDegrees - 90.f, 0.f);
	ADecalActor* Decal = GetWorld()->SpawnActor<ADecalActor>(ADecalActor::StaticClass(), Object.Location, Rotation, Parameters);
	if (not IsValid(Decal) or not IsValid(Decal->GetDecal()))
	{
		return Decal;
	}
	Decal->GetDecal()->SetDecalMaterial(Material);
	Decal->GetDecal()->DecalSize = Asset.DecalSize * Object.Scale;
	Decal->GetDecal()->MarkRenderStateDirty();
	return Decal;
}

// ---------------------------------------------------------------------------
// Spawning wildlife
// ---------------------------------------------------------------------------

void USurvivalPointsOfInterestSubsystem::SpawnWildlifeGroup(const FSurvivalWildlifeGroup& Group)
{
	if (not M_Settings->Wildlife.IsValidIndex(Group.EntryIndex))
	{
		return;
	}
	const FSurvivalWildlifeEntry& Entry = M_Settings->Wildlife[Group.EntryIndex];
	FSurvivalWildlifeRecord& Record = M_WildlifeRecords.AddDefaulted_GetRef();
	Record.Group = Group;
	switch (Entry.Kind)
	{
	case ESurvivalWildlifeKind::GroundHerd:
		SpawnHerd(Entry, Group, Record);
		break;
	case ESurvivalWildlifeKind::BirdFlock:
		SpawnBirdFlock(Entry, Group, Record);
		break;
	case ESurvivalWildlifeKind::NiagaraFlock:
		SpawnNiagaraFlock(Entry, Group, Record);
		break;
	default:
		break;
	}
}

void USurvivalPointsOfInterestSubsystem::SpawnHerd(const FSurvivalWildlifeEntry& Entry, const FSurvivalWildlifeGroup& Group,
                                                   FSurvivalWildlifeRecord& OutRecord)
{
	using namespace SurvivalPOISubsystemConstants;
	UClass* Class = Entry.ActorClass.Get();
	if (not IsValid(Class) or not M_Projector.IsValid())
	{
		UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI wildlife class not loaded: %s"), *Entry.ActorClass.ToString());
		return;
	}
	FRandomStream Random(Group.Seed);
	TArray<FVector2D> Spots;
	for (int32 Animal = 0; Animal < Group.Count; ++Animal)
	{
		for (int32 Attempt = 0; Attempt < AnimalSpotAttempts; ++Attempt)
		{
			// Irregular spacing: some close together, a straggler or two farther out.
			const float Distance = Entry.HerdSpread * FMath::Sqrt(Random.FRand());
			const float Angle = Random.FRandRange(0.f, UE_TWO_PI);
			const FVector2D Spot = FVector2D(Group.Center) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
			const bool bFree = not Spots.ContainsByPredicate([&Spot](const FVector2D& Other)
			{
				return FVector2D::Distance(Other, Spot) < MinAnimalSpacing;
			});
			if (bFree)
			{
				Spots.Add(Spot);
				break;
			}
		}
	}
	for (const FVector2D& Spot : Spots)
	{
		FSurvivalGroundHit Hit;
		if (not M_Projector->TryProject(Spot, Hit))
		{
			continue;
		}
		const FTransform Transform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Hit.Location);
		AActor* Animal = GetWorld()->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr,
		                                                        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (not IsValid(Animal))
		{
			continue;
		}
		SurvivalPOI_SetStructNumber(*Animal, TEXT("MovementSettings"), TEXT("WanderRadius"), Entry.WanderRadius);
		SurvivalPOI_SetStructNumber(*Animal, TEXT("FleeSettings"), TEXT("FleeDistance"), Entry.FleeDistance);
		Animal->FinishSpawning(Transform);
		MarkSpawned(*Animal);
		OutRecord.Actors.Add(Animal);
	}
}

void USurvivalPointsOfInterestSubsystem::SpawnBirdFlock(const FSurvivalWildlifeEntry& Entry, const FSurvivalWildlifeGroup& Group,
                                                        FSurvivalWildlifeRecord& OutRecord)
{
	UClass* Class = Entry.ActorClass.Get();
	if (not IsValid(Class))
	{
		UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI flock class not loaded: %s"), *Entry.ActorClass.ToString());
		return;
	}
	const FTransform Transform(FRotator(0.f, Group.YawDegrees, 0.f), Group.Center);
	AActor* Flock = GetWorld()->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr,
	                                                       ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Flock))
	{
		return;
	}
	SurvivalPOI_SetStructNumber(*Flock, TEXT("Ellipse"), TEXT("SemiAxisX"), Group.SemiAxes.X);
	SurvivalPOI_SetStructNumber(*Flock, TEXT("Ellipse"), TEXT("SemiAxisY"), Group.SemiAxes.Y);
	SurvivalPOI_SetStructNumber(*Flock, TEXT("FlightSettings"), TEXT("BirdCount"), Group.Count);
	SurvivalPOI_SetStructNumber(*Flock, TEXT("FlightSettings"), TEXT("FlightHeight"), Group.FlightHeight);
	SurvivalPOI_SetStructNumber(*Flock, TEXT("FlightSettings"), TEXT("Speed"), Entry.FlightSpeed);
	SurvivalPOI_SetStructNumber(*Flock, TEXT("FlightSettings"), TEXT("Seed"), Group.Seed % 100000);
	Flock->FinishSpawning(Transform);
	MarkSpawned(*Flock);
	OutRecord.Actors.Add(Flock);
}

void USurvivalPointsOfInterestSubsystem::SpawnNiagaraFlock(const FSurvivalWildlifeEntry& Entry, const FSurvivalWildlifeGroup& Group,
                                                           FSurvivalWildlifeRecord& OutRecord)
{
	UNiagaraSystem* System = Entry.NiagaraSystem.Get();
	if (not IsValid(System))
	{
		UE_LOG(LogSurvivalPOI, Error, TEXT("SURVIVAL_POI Niagara flock not loaded: %s"), *Entry.NiagaraSystem.ToString());
		return;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location = Group.Center + FVector(0.0, 0.0, Group.FlightHeight);
	ANiagaraActor* Holder = GetWorld()->SpawnActor<ANiagaraActor>(ANiagaraActor::StaticClass(), Location,
	                                                             FRotator(0.f, Group.YawDegrees, 0.f), Parameters);
	if (not IsValid(Holder) or not IsValid(Holder->GetNiagaraComponent()))
	{
		return;
	}
	Holder->GetNiagaraComponent()->SetAsset(System);
	Holder->GetNiagaraComponent()->Activate(true);
	MarkSpawned(*Holder);
	OutRecord.Actors.Add(Holder);
}
