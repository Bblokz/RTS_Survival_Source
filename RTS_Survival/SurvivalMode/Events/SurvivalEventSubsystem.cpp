// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalEventSubsystem.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/SurvivalMode/Core/SurvivalModeTypes.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/Events/SurvivalEventSettings.h"
#include "RTS_Survival/SurvivalMode/Events/W_SurvivalEventBanner.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/SurvivalMode/UI/SurvivalArmyUiTypes.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalEdgeSpawnZone.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveDirectorComponent.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveTypes.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

#if RTS_WITH_MAP_TESTS
#include "RTS_Survival/SurvivalMode/Events/Tests/SurvivalConvoyTest.h"
#endif

namespace SurvivalEventConstants
{
	const TCHAR* const SettingsAssetPath = TEXT(
		"/Game/RTS_Survival/SurvivalMode/Data/Events/DA_SurvivalEventSettings.DA_SurvivalEventSettings");
	constexpr float PollIntervalSeconds = 1.f;
	constexpr float BannerAfterResolveSeconds = 8.f;
	constexpr int32 BannerZOrder = 5;
	constexpr uint8 PlayerIndex = 1;
	constexpr float KmhToCms = 100000.f / 3600.f;
	constexpr float CmPerKilometre = 100000.f;
	// The convoy spawns in Clearing; the route length budget counts that lead plus the intermission and extension.
	constexpr float ClearingLeadSecondsEstimate = 25.f;
	constexpr float IntermissionSecondsEstimate = 50.f;
	constexpr int32 RouteSeedSalt = 7919;
	constexpr int32 RollSeedSalt = 104729;
}

void USurvivalEventSubsystem::StartForMatch(ASurvivalModeDirector* Director)
{
	UWorld* World = IsValid(Director) ? Director->GetWorld() : nullptr;
	USurvivalEventSubsystem* Subsystem = World != nullptr ? World->GetSubsystem<USurvivalEventSubsystem>() : nullptr;
	if (not IsValid(Subsystem))
	{
		return;
	}
	Subsystem->BindToDirector(Director);
}

void USurvivalEventSubsystem::BindToDirector(ASurvivalModeDirector* Director)
{
	if (bM_IsStarted)
	{
		return;
	}
	bM_IsStarted = true;
	M_Director = Director;
	LoadSettings();
	M_MatchSeed = ResolveMatchSeed();
	M_PhaseChangedHandle = Director->OnPhaseChanged.AddUObject(this, &USurvivalEventSubsystem::OnPhaseChanged);
	if (USurvivalWaveDirectorComponent* Waves = Director->GetWaveDirector())
	{
		M_RoundSurvivedHandle = Waves->OnRoundSurvived.AddUObject(this, &USurvivalEventSubsystem::OnRoundSurvived);
	}
	GetWorld()->GetTimerManager().SetTimer(M_PollTimer, this, &USurvivalEventSubsystem::Poll,
	                                       SurvivalEventConstants::PollIntervalSeconds, true);
	LogEvent(TEXT("SchedulerStarted"), FString::Printf(TEXT("seed=%d definitions=%d settings=%s"), M_MatchSeed,
	                                                   M_Definitions.Num(), *GetNameSafe(M_Settings)));
#if RTS_WITH_MAP_TESTS
	FSurvivalConvoyTest::StartIfRequested(this);
#endif
}

void USurvivalEventSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_PollTimer);
		World->GetTimerManager().ClearTimer(M_BannerHideTimer);
	}
	if (M_Director.IsValid())
	{
		M_Director->OnPhaseChanged.Remove(M_PhaseChangedHandle);
		if (USurvivalWaveDirectorComponent* Waves = M_Director->GetWaveDirector())
		{
			Waves->OnRoundSurvived.Remove(M_RoundSurvivedHandle);
		}
	}
	if (M_ActiveConvoy.IsValid())
	{
		M_ActiveConvoy->OnConvoyEventNative.Remove(M_ConvoyEventHandle);
	}
	OnEventLog.Clear();
	Super::Deinitialize();
}

void USurvivalEventSubsystem::LoadSettings()
{
	M_Settings = LoadObject<USurvivalEventSettings>(nullptr, SurvivalEventConstants::SettingsAssetPath);
	if (not IsValid(M_Settings))
	{
		// The C++ defaults are the shipped values; the asset only tunes them.
		M_Settings = NewObject<USurvivalEventSettings>(this);
		LogEvent(TEXT("SettingsFallback"), TEXT("DA_SurvivalEventSettings missing, using C++ defaults"));
	}
	M_Definitions.Reset();
	for (const FSurvivalConvoyEventEntry& Entry : M_Settings->Convoys)
	{
		UConvoyDefinition* Definition = Entry.Definition.LoadSynchronous();
		if (not IsValid(Definition))
		{
			UE_LOG(LogSurvivalEvents, Warning, TEXT("SURVIVAL_EVENTS convoy definition %s missing; using a default fuel convoy"),
			       *Entry.Definition.ToString());
			Definition = NewObject<UConvoyDefinition>(this);
		}
		M_Definitions.Add(Definition);
	}
}

int32 USurvivalEventSubsystem::ResolveMatchSeed() const
{
#if RTS_WITH_MAP_TESTS
	int32 CommandLineSeed = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("SurvivalEventSeed="), CommandLineSeed)
		or FParse::Value(FCommandLine::Get(), TEXT("POISeed="), CommandLineSeed))
	{
		return CommandLineSeed;
	}
#endif
	return static_cast<int32>(HashCombine(GetTypeHash(FPlatformTime::Cycles64()), GetTypeHash(FMath::Rand())) & 0x7fffffff);
}

bool USurvivalEventSubsystem::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Director", "GetIsValidDirector", this);
	return false;
}

bool USurvivalEventSubsystem::GetIsValidSettings() const
{
	if (IsValid(M_Settings))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Settings", "GetIsValidSettings", this);
	return false;
}

ERTSGameDifficulty USurvivalEventSubsystem::GetDifficultyLevel() const
{
	return M_Director.IsValid() ? M_Director->GetDifficulty().DifficultyLevel : ERTSGameDifficulty::Normal;
}

void USurvivalEventSubsystem::LogEvent(const FString& Name, const FString& Detail)
{
	const UWorld* World = GetWorld();
	UE_LOG(LogSurvivalEvents, Log, TEXT("SURVIVAL_EVENTS t=%.2f %s %s"), World ? World->GetTimeSeconds() : 0.0, *Name,
	       *Detail);
	OnEventLog.Broadcast(Name, Detail);
}

// ---------------------------------------------------------------------------
// Scheduling
// ---------------------------------------------------------------------------

FSurvivalEventRoll USurvivalEventSubsystem::RollForRound(const int32 Round, const int32 LastEventRound,
                                                         const int32 QuietRounds, const ERTSGameDifficulty Difficulty,
                                                         const int32 Seed) const
{
	FSurvivalEventRoll Roll;
	if (not GetIsValidSettings() or M_Definitions.IsEmpty())
	{
		Roll.Reason = TEXT("no settings or definitions");
		return Roll;
	}
	if (Round < M_Settings->FirstAllowedRound)
	{
		Roll.Reason = TEXT("before the first allowed round");
		return Roll;
	}
	if (Round - LastEventRound <= M_Settings->CooldownRounds)
	{
		Roll.Reason = TEXT("cooldown");
		return Roll;
	}
	FRandomStream Random(HashCombine(GetTypeHash(Seed), GetTypeHash(Round + SurvivalEventConstants::RollSeedSalt)));
	Roll.bForced = QuietRounds >= M_Settings->ForceAfterQuietRounds;
	const float Draw = Random.FRand();
	Roll.bFires = Roll.bForced or Draw < M_Settings->EventChance;
	if (not Roll.bFires)
	{
		Roll.Reason = FString::Printf(TEXT("chance %.2f drew %.2f"), M_Settings->EventChance, Draw);
		return Roll;
	}
	float TotalWeight = 0.f;
	for (const FSurvivalConvoyEventEntry& Entry : M_Settings->Convoys)
	{
		TotalWeight += FMath::Max(0.f, Entry.Weight);
	}
	float Pick = Random.FRandRange(0.f, TotalWeight);
	Roll.DefinitionIndex = 0;
	for (int32 Index = 0; Index < M_Settings->Convoys.Num() and Index < M_Definitions.Num(); ++Index)
	{
		Pick -= FMath::Max(0.f, M_Settings->Convoys[Index].Weight);
		if (Pick <= 0.f)
		{
			Roll.DefinitionIndex = Index;
			break;
		}
	}
	const UConvoyDefinition* Definition = M_Definitions[Roll.DefinitionIndex];
	const int32 MaxTier = IsValid(Definition) ? FMath::Max(0, Definition->GuardTiers.Num() - 1) : 0;
	Roll.GuardTier = FMath::Clamp(M_Settings->GetGuardTierForRound(Round, Difficulty), 0, MaxTier);
	Roll.CargoMultiplier = M_Settings->GetCargoMultiplier(Difficulty);
	Roll.Reason = Roll.bForced ? TEXT("forced after quiet rounds") : FString::Printf(TEXT("chance drew %.2f"), Draw);
	return Roll;
}

void USurvivalEventSubsystem::OnPhaseChanged(const ESurvivalPhase OldPhase, const ESurvivalPhase NewPhase)
{
	LogEvent(TEXT("PhaseChanged"), FString::Printf(TEXT("%s->%s"), *Global_GetSurvivalPhaseAsString(OldPhase),
	                                               *Global_GetSurvivalPhaseAsString(NewPhase)));
	if (NewPhase == ESurvivalPhase::Defeated)
	{
		if (M_ActiveConvoy.IsValid())
		{
			M_ActiveConvoy->EndConvoy(true);
		}
		bM_HasPendingEvent = false;
		GetWorld()->GetTimerManager().ClearTimer(M_PollTimer);
		HideBanner();
		return;
	}
	if (NewPhase == ESurvivalPhase::GracePeriod and GetIsValidSettings() and M_Settings->bAllowInFirstGrace)
	{
		M_PendingRoll = RollForRound(M_Settings->FirstAllowedRound, -1000, M_Settings->ForceAfterQuietRounds,
		                             GetDifficultyLevel(), M_MatchSeed);
		bM_HasPendingEvent = M_PendingRoll.bFires;
		M_PendingRound = 0;
		TrySpawnPendingEvent(TEXT("first grace"));
		AnnounceActiveEvent();
	}
}

void USurvivalEventSubsystem::Poll()
{
	if (not M_Director.IsValid())
	{
		return;
	}
	const USurvivalWaveDirectorComponent* Waves = M_Director->GetWaveDirector();
	if (not IsValid(Waves))
	{
		return;
	}
	const ESurvivalRoundState State = Waves->GetRoundState();
	if (State != M_LastRoundState)
	{
		LogEvent(TEXT("RoundStateChanged"), FString::Printf(TEXT("round=%d state=%d->%d enemies_remaining=%d"),
		                                                     Waves->GetWavesLaunched(), static_cast<int32>(M_LastRoundState),
		                                                     static_cast<int32>(State), Waves->GetEnemiesRemaining()));
		if (State == ESurvivalRoundState::Clearing)
		{
			OnClearingStarted(Waves->GetWavesLaunched());
		}
		if (State == ESurvivalRoundState::Active and M_ActiveConvoy.IsValid() and not bM_ActiveResolved
			and IsValid(M_Banner))
		{
			M_Banner->SetStatusText(NSLOCTEXT("SurvivalEvents", "ConvoyDuringRound",
			                                  "The next wave is here. The convoy drives on to its exit."));
			LogEvent(TEXT("ConvoyIntoNextRound"), TEXT("rule=continues to its exit"));
		}
		M_LastRoundState = State;
	}
	const bool bCanSpawnInClearing = State == ESurvivalRoundState::Clearing and bM_HasPendingEvent
		and Waves->GetEnemiesRemaining() <= M_Settings->ClearingSpawnEnemiesThreshold;
	if (bCanSpawnInClearing)
	{
		TrySpawnPendingEvent(TEXT("clearing"));
	}
	UpdateBanner();
	CheckActiveEventResolved();
}

void USurvivalEventSubsystem::OnClearingStarted(const int32 RoundNumber)
{
	if (bM_HasActiveEvent or bM_HasPendingEvent)
	{
		LogEvent(TEXT("EventRolled"), FString::Printf(TEXT("round=%d fires=0 reason=an event is still live"), RoundNumber));
		return;
	}
	const FSurvivalEventRoll Roll = RollForRound(RoundNumber, M_LastEventRound, M_QuietRounds, GetDifficultyLevel(),
	                                             M_MatchSeed);
	LogEvent(TEXT("EventRolled"), FString::Printf(TEXT("round=%d fires=%d forced=%d definition=%d tier=%d cargo_mult=%.2f reason=%s"),
	                                              RoundNumber, Roll.bFires ? 1 : 0, Roll.bForced ? 1 : 0,
	                                              Roll.DefinitionIndex, Roll.GuardTier, Roll.CargoMultiplier,
	                                              *Roll.Reason));
	if (not Roll.bFires)
	{
		M_QuietRounds += RoundNumber >= M_Settings->FirstAllowedRound ? 1 : 0;
		return;
	}
	bM_HasPendingEvent = true;
	M_PendingRoll = Roll;
	M_PendingRound = RoundNumber;
}

void USurvivalEventSubsystem::OnRoundSurvived(const int32 RoundNumber)
{
	LogEvent(TEXT("RoundSurvived"), FString::Printf(TEXT("round=%d"), RoundNumber));
	if (bM_HasPendingEvent)
	{
		// The round ended before few enough enemies were left: spawn now, still at a far edge.
		TrySpawnPendingEvent(TEXT("intermission start"));
	}
	if (not M_ActiveConvoy.IsValid() or bM_ActiveResolved or M_ActiveRound != RoundNumber)
	{
		return;
	}
	AnnounceActiveEvent();
	if (GetIsValidDirector() and M_Settings->MaxIntermissionExtensionSeconds > 0.f)
	{
		bM_ActiveExtended = M_Director->ExtendIntermission(M_Settings->MaxIntermissionExtensionSeconds);
		LogEvent(TEXT("IntermissionExtended"), FString::Printf(TEXT("seconds=%.0f ok=%d remaining=%.0f"),
		                                                        M_Settings->MaxIntermissionExtensionSeconds,
		                                                        bM_ActiveExtended ? 1 : 0,
		                                                        M_Director->GetIntermissionSecondsRemaining()));
	}
}

void USurvivalEventSubsystem::TrySpawnPendingEvent(const TCHAR* Moment)
{
	if (not bM_HasPendingEvent)
	{
		return;
	}
	bM_HasPendingEvent = false;
	if (not M_Definitions.IsValidIndex(M_PendingRoll.DefinitionIndex))
	{
		return;
	}
	UConvoyDefinition* Definition = M_Definitions[M_PendingRoll.DefinitionIndex];
	const int32 RouteSeed = HashCombine(GetTypeHash(M_MatchSeed), GetTypeHash(M_PendingRound * SurvivalEventConstants::RouteSeedSalt));
	const FSurvivalConvoyRoutePlan Plan = PlanRoute(RouteSeed, Definition, FString(), FString());
	LogEvent(TEXT("RoutePlanned"), FString::Printf(TEXT("round=%d moment=%s valid=%d id=%s length=%.0f closest=%.0f soviet=%.0f candidates=%d nav_tries=%d notes=%s"),
	                                               M_PendingRound, Moment, Plan.bIsValid ? 1 : 0,
	                                               *Plan.Route.RouteId.ToString(), Plan.Metrics.Length,
	                                               Plan.Metrics.ClosestApproach, Plan.Metrics.MinSovietBaseDistance,
	                                               Plan.CandidatesConsidered, Plan.NavCandidatesTried, *Plan.Notes));
	if (not Plan.bIsValid)
	{
		M_QuietRounds++;
		return;
	}
	StartConvoyEvent(Definition, M_PendingRoll.GuardTier, M_PendingRoll.CargoMultiplier, Plan, M_PendingRound, false);
}

AConvoy* USurvivalEventSubsystem::StartConvoyEvent(UConvoyDefinition* Definition, const int32 GuardTier,
                                                   const float CargoMultiplier, const FSurvivalConvoyRoutePlan& Plan,
                                                   const int32 Round, const bool bAnnounceNow)
{
	if (not IsValid(Definition) or not Plan.bIsValid)
	{
		return nullptr;
	}
	FConvoyStartParams Params;
	Params.OwningPlayer = 2;
	Params.GuardTierIndex = GuardTier;
	Params.CargoMultiplier = CargoMultiplier;
	Params.bShowOnMinimap = true;
	Params.DebugLabel = FString::Printf(TEXT("survival_round_%d"), Round);
	AConvoy* Convoy = AConvoy::StartConvoy(this, Definition, Plan.Route, Params);
	if (not IsValid(Convoy))
	{
		LogEvent(TEXT("EventStartFailed"), Definition->GetName());
		return nullptr;
	}
	M_ActiveConvoy = Convoy;
	bM_HasActiveEvent = true;
	M_ActiveRound = Round;
	M_ActivePlan = Plan;
	bM_ActiveAnnounced = false;
	bM_ActiveResolved = false;
	bM_ActiveExtended = false;
	M_LastEventRound = Round;
	M_QuietRounds = 0;
	M_ConvoyEventHandle = Convoy->OnConvoyEventNative.AddUObject(this, &USurvivalEventSubsystem::OnConvoyEvent);
	LogEvent(TEXT("EventStarted"), FString::Printf(TEXT("round=%d convoy=%d definition=%s tier=%d cargo_mult=%.2f route=%s entry=%s exit=%s"),
	                                               Round, Convoy->GetConvoyId(), *Definition->GetName(), GuardTier,
	                                               CargoMultiplier, *Plan.Route.RouteId.ToString(), *Plan.EntryName,
	                                               *Plan.ExitName));
	if (bAnnounceNow)
	{
		AnnounceActiveEvent();
	}
	return Convoy;
}

// ---------------------------------------------------------------------------
// Routes
// ---------------------------------------------------------------------------

float USurvivalEventSubsystem::EstimatePaceSpeed(const UConvoyDefinition* Definition) const
{
	if (not IsValid(Definition))
	{
		return 0.f;
	}
	float Pace = TNumericLimits<float>::Max();
	for (const FConvoyTruckSpec& Truck : Definition->Trucks)
	{
		bool bIsValidData = false;
		const FTankData Data = FRTS_Statics::BP_GetTankDataOfPlayer(
			2, static_cast<ETankSubtype>(Truck.TrainingOption.SubtypeValue), this, bIsValidData);
		const float Cruise = Truck.CalibratedCruiseSpeedCms > 0.f
			                     ? Truck.CalibratedCruiseSpeedCms
			                     : Data.VehicleMaxSpeedKmh * SurvivalEventConstants::KmhToCms;
		if (Cruise > 0.f)
		{
			Pace = FMath::Min(Pace, Cruise * Definition->Speed.TruckSpeedFraction);
		}
	}
	return Pace == TNumericLimits<float>::Max() ? 0.f : Pace;
}

FSurvivalConvoyRouteRequest USurvivalEventSubsystem::BuildRouteRequest(const int32 Seed) const
{
	FSurvivalConvoyRouteRequest Request;
	Request.Seed = Seed;
	Request.Settings = IsValid(M_Settings) ? &M_Settings->Route : nullptr;
	if (not M_Director.IsValid())
	{
		return Request;
	}
	const ASurvivalStartLocation* Chosen = M_Director->GetChosenStartLocation();
	Request.BaseCentre = IsValid(Chosen) ? Chosen->GetDeployCenter() : M_Director->GetCurrentPlayerBaseLocation();
	for (const TWeakObjectPtr<ASurvivalStartLocation>& Start : M_Director->GetStartLocations())
	{
		if (Start.IsValid() and Start.Get() != Chosen)
		{
			Request.SovietBaseCentres.Add(Start->GetDeployCenter());
		}
	}
	for (const TWeakObjectPtr<ASurvivalEdgeSpawnZone>& Zone : M_Director->GetEdgeSpawnZones())
	{
		if (Zone.IsValid() and Zone->GetIsEnabled())
		{
			FSurvivalConvoyEndpoint& Endpoint = Request.Endpoints.AddDefaulted_GetRef();
			Endpoint.Name = Zone->GetName();
			Endpoint.Location = Zone->GetSpawnCenter();
		}
	}
	if (const USurvivalWaveDirectorComponent* Waves = M_Director->GetWaveDirector())
	{
		Request.ActiveSpawnLocations = Waves->GetLastAnnouncement().EdgeLocations;
	}
	if (const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(this))
	{
		for (const ATankMaster* Tank : UnitManager->GetPlayerTanks(SurvivalEventConstants::PlayerIndex))
		{
			if (IsValid(Tank))
			{
				Request.PlayerUnitLocations.Add(Tank->GetActorLocation());
			}
		}
		for (const ASquadUnit* Soldier : UnitManager->GetSquadUnitsOfPlayer(SurvivalEventConstants::PlayerIndex))
		{
			if (IsValid(Soldier))
			{
				Request.PlayerUnitLocations.Add(Soldier->GetActorLocation());
			}
		}
	}
	return Request;
}

FSurvivalConvoyRoutePlan USurvivalEventSubsystem::PlanRoute(const int32 Seed, const UConvoyDefinition* Definition,
                                                           const FString& ForcedEntry, const FString& ForcedExit) const
{
	using namespace SurvivalEventConstants;
	FSurvivalConvoyRouteRequest Request = BuildRouteRequest(Seed);
	Request.ForcedEntry = ForcedEntry;
	Request.ForcedExit = ForcedExit;
	const float WindowSeconds = ClearingLeadSecondsEstimate + IntermissionSecondsEstimate
		+ (IsValid(M_Settings) ? M_Settings->MaxIntermissionExtensionSeconds : 0.f);
	Request.LengthBudget = EstimatePaceSpeed(Definition) * WindowSeconds;
	if (Request.Settings == nullptr)
	{
		return FSurvivalConvoyRoutePlan();
	}
	return FSurvivalConvoyRoutePlanner::Plan(GetWorld(), Request);
}

// ---------------------------------------------------------------------------
// Announcement and banner
// ---------------------------------------------------------------------------

TArray<FText> USurvivalEventSubsystem::BuildAnnouncementLines(const AConvoy& Convoy) const
{
	TArray<FText> Lines;
	const UConvoyDefinition* Definition = Convoy.GetDefinition();
	if (not IsValid(Definition))
	{
		return Lines;
	}
	const FConvoyGuardTier* Tier = Definition->GetGuardTier(Convoy.GetStartParams().GuardTierIndex);
	int32 Vehicles = 0;
	int32 Squads = 0;
	if (Tier != nullptr)
	{
		for (const FConvoyEscortSpec& Escort : Tier->Escorts)
		{
			(Escort.Unit.UnitType == EAllUnitType::UNType_Squad ? Squads : Vehicles) += Escort.Count;
		}
	}
	const int32 Trucks = Definition->GetTruckColumn(Convoy.GetStartParams().GuardTierIndex).Num();
	Lines.Add(FText::Format(NSLOCTEXT("SurvivalEvents", "GuardLine", "{0} trucks. Guard: {1} ({2} vehicles, {3} squads)"),
	                        FText::AsNumber(Trucks), FText::FromName(Tier != nullptr ? Tier->TierName : NAME_None),
	                        FText::AsNumber(Vehicles), FText::AsNumber(Squads)));
	TArray<FString> CargoParts;
	for (const TPair<ERTSResourceType, int32>& Pair : Convoy.GetTotalCargo().Resources)
	{
		CargoParts.Add(FString::Printf(TEXT("%d %s"), Pair.Value, *Global_GetResourceTypeDisplayString(Pair.Key)));
	}
	Lines.Add(FText::Format(NSLOCTEXT("SurvivalEvents", "CargoLine", "Cargo: {0}"),
	                        FText::FromString(FString::Join(CargoParts, TEXT(", ")))));
	const FVector Base = M_Director.IsValid() ? M_Director->GetCurrentPlayerBaseLocation() : FVector::ZeroVector;
	const TArray<FVector>& Points = M_ActivePlan.Route.Points;
	if (Points.Num() >= 2)
	{
		Lines.Add(FText::Format(NSLOCTEXT("SurvivalEvents", "RouteLine", "From the {0} to the {1}, {2} km from your base at the closest"),
		                        SurvivalArmyUi::GetCompassDirectionText(Points[0], Base),
		                        SurvivalArmyUi::GetCompassDirectionText(Points.Last(), Base),
		                        FText::AsNumber(FMath::RoundToInt(M_ActivePlan.Metrics.ClosestApproach / SurvivalEventConstants::CmPerKilometre * 10.f) / 10.f)));
	}
	Lines.Add(NSLOCTEXT("SurvivalEvents", "InstructionLine",
	                    "Destroy the trucks, then scavenge their cargo before it is gone."));
	return Lines;
}

void USurvivalEventSubsystem::AnnounceActiveEvent()
{
	AConvoy* Convoy = M_ActiveConvoy.Get();
	if (not IsValid(Convoy) or bM_ActiveAnnounced or not GetIsValidSettings())
	{
		return;
	}
	bM_ActiveAnnounced = true;
	const UConvoyDefinition* Definition = Convoy->GetDefinition();
	M_ActiveTitle = FText::FromString(FString::Printf(TEXT("SOVIET %s SPOTTED"),
	                                                  *(IsValid(Definition) ? Definition->DisplayName.ToString() : TEXT("CONVOY")).ToUpper()));
	const TArray<FText> Lines = BuildAnnouncementLines(*Convoy);
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (not IsValid(M_Banner) and IsValid(PlayerController))
	{
		M_Banner = CreateWidget<UW_SurvivalEventBanner>(PlayerController, UW_SurvivalEventBanner::StaticClass());
		if (IsValid(M_Banner))
		{
			M_Banner->AddToViewport(SurvivalEventConstants::BannerZOrder);
		}
	}
	if (IsValid(M_Banner))
	{
		M_Banner->ShowEvent(M_ActiveTitle, Lines);
		M_Banner->SetStatusText(FText::GetEmpty());
	}
	if (USoundBase* Sound = M_Settings->AnnouncementSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
	FString Joined;
	for (const FText& Line : Lines)
	{
		Joined += Line.ToString() + TEXT(" | ");
	}
	LogEvent(TEXT("Announced"), FString::Printf(TEXT("round=%d convoy=%d title=\"%s\" text=\"%s\""), M_ActiveRound,
	                                            Convoy->GetConvoyId(), *M_ActiveTitle.ToString(), *Joined));
	UpdateBanner();
}

void USurvivalEventSubsystem::UpdateBanner()
{
	const AConvoy* Convoy = M_ActiveConvoy.Get();
	if (not IsValid(M_Banner) or not IsValid(Convoy) or bM_ActiveResolved or not M_Director.IsValid())
	{
		return;
	}
	const float RouteLeft = FMath::Max(0.f, Convoy->GetRoutePolyline().GetLength() - Convoy->GetColumnReferenceDistance());
	const float Pace = FMath::Max(1.f, Convoy->GetPaceSpeed());
	const FText ExitClock = SurvivalArmyUi::FormatClock(RouteLeft / Pace);
	if (M_Director->GetIsInIntermission())
	{
		M_Banner->SetCountdownText(FText::Format(
			NSLOCTEXT("SurvivalEvents", "CountdownIntermission", "Next wave in {0}  |  convoy leaves in ~{1}"),
			SurvivalArmyUi::FormatClock(M_Director->GetIntermissionSecondsRemaining()), ExitClock));
		return;
	}
	M_Banner->SetCountdownText(FText::Format(NSLOCTEXT("SurvivalEvents", "CountdownExit", "Convoy leaves in ~{0}"),
	                                         ExitClock));
}

void USurvivalEventSubsystem::OnConvoyEvent(const FConvoyEvent& Event)
{
	switch (Event.Type)
	{
	case EConvoyEventType::Finished:
	case EConvoyEventType::TruckDestroyed:
	case EConvoyEventType::CargoScavenged:
	case EConvoyEventType::CargoExpired:
		CheckActiveEventResolved();
		break;
	default:
		break;
	}
}

void USurvivalEventSubsystem::CheckActiveEventResolved()
{
	if (not bM_HasActiveEvent or bM_ActiveResolved)
	{
		return;
	}
	const AConvoy* Convoy = M_ActiveConvoy.Get();
	if (not IsValid(Convoy))
	{
		ResolveActiveEvent(NSLOCTEXT("SurvivalEvents", "ConvoyGone", "The convoy is gone."));
		return;
	}
	if (Convoy->GetOutcome() == EConvoyOutcome::Escaped)
	{
		ResolveActiveEvent(NSLOCTEXT("SurvivalEvents", "ConvoyEscaped", "The convoy escaped with its cargo."));
		return;
	}
	const bool bTrucksGone = Convoy->GetAliveTruckCount() == 0 and Convoy->GetConvoyState() != EConvoyState::Spawning;
	if (bTrucksGone and Convoy->GetOpenCacheCount() == 0)
	{
		ResolveActiveEvent(NSLOCTEXT("SurvivalEvents", "ConvoyLooted", "All trucks destroyed and their cargo is gone."));
	}
}

void USurvivalEventSubsystem::ResolveActiveEvent(const FText& Status)
{
	bM_ActiveResolved = true;
	bM_HasActiveEvent = false;
	LogEvent(TEXT("EventResolved"), FString::Printf(TEXT("round=%d status=\"%s\" extended=%d"), M_ActiveRound,
	                                                *Status.ToString(), bM_ActiveExtended ? 1 : 0));
	if (bM_ActiveExtended and M_Director.IsValid())
	{
		M_Director->EndIntermissionExtension();
		LogEvent(TEXT("ExtensionEnded"), FString::Printf(TEXT("remaining=%.0f"), M_Director->GetIntermissionSecondsRemaining()));
	}
	if (IsValid(M_Banner))
	{
		M_Banner->SetStatusText(Status);
		M_Banner->SetCountdownText(FText::GetEmpty());
	}
	if (M_ActiveConvoy.IsValid())
	{
		M_ActiveConvoy->OnConvoyEventNative.Remove(M_ConvoyEventHandle);
	}
	M_ActiveConvoy = nullptr;
	TWeakObjectPtr<USurvivalEventSubsystem> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(M_BannerHideTimer, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->HideBanner();
		}
	}, SurvivalEventConstants::BannerAfterResolveSeconds, false);
}

void USurvivalEventSubsystem::HideBanner()
{
	if (IsValid(M_Banner))
	{
		M_Banner->RemoveFromParent();
	}
	M_Banner = nullptr;
}
