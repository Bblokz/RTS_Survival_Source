// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Mission_BerlinForgeStation.h"

#include "Engine/World.h"

#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Enemy/Strongpoint/EnemyStrongpoint.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Missions/Berlin/BerlinMissionForcesData.h"
#include "RTS_Survival/Missions/Berlin/ForgeStation/BerlinForgeStation.h"
#include "RTS_Survival/Missions/Berlin/ForgeStation/ForgeStationSightedTrigger.h"
#include "RTS_Survival/Missions/PlayerAirDrop/PlayerAirDropSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace BerlinForgeStationMissionConstants
{
	constexpr float TrackingValidityIntervalSeconds = 1.f;
	constexpr int32 ArchiveSortingPriority = 0;
	constexpr float ArchiveNotificationSeconds = 20.f;
	// The landing point is traced down from this high.
	constexpr float LandingTraceHalfHeight = 50000.f;
}

UMission_BerlinForgeStation::UMission_BerlinForgeStation()
{
	MissionWidgetState.MissionTitle = NSLOCTEXT("BerlinForgeStation", "Title", "The Forge survey station");
	MissionWidgetState.MissionDescription = NSLOCTEXT("BerlinForgeStation", "Description",
	                                                  "A Forge survey team works the crater by the Brandenburg Gate. "
	                                                  "Destroy the drill rig, the cooling plant and the command bunker. "
	                                                  "Do not let a guard run to the Reichstag.");
	MissionWidgetState.ObjectiveType = EMissionObjectiveType::Bonus;
	// The Blueprint's reward sequence ends the stage with its own OnMissionComplete.
	bCompleteMissionWhenTrackingDone = false;
	MissionTrigger = CreateDefaultSubobject<UForgeStationSightedTrigger>(TEXT("SightedTrigger"));

	CraterIntel.Key = TEXT("Berlin_ForgeCraterSurvey");
	CraterIntel.Title = NSLOCTEXT("BerlinForgeStation", "IntelTitle", "Forge survey: the Berlin crater");
	CraterIntel.Description = NSLOCTEXT("BerlinForgeStation", "IntelDescription",
	                                    "Recovered from the Forge survey station at the Brandenburg Gate. Core samples "
	                                    "from the 1945 crater, logged every seventy-five seconds, each one marked "
	                                    "'sedated'. The Forge freezes the crystals before it touches them. The readings "
	                                    "do not match any Radixite field we know; the survey calls Berlin a control "
	                                    "sample. Of what, it does not say.");
	CraterIntel.SortType = ERTSArchiveType::Lore;
	CraterIntel.bHasImage = false;
}

// ------------------------------------------------------------------
// Start and setup
// ------------------------------------------------------------------

void UMission_BerlinForgeStation::OnMissionStart()
{
	Super::OnMissionStart();
	// Ticks from the start (before the trigger): the garrison is set up and dead targets are noticed early.
	MissionState.bTickOnMission = true;
	M_Station = ABerlinForgeStation::FindForgeStation(this);
	if (GetIsValidStation())
	{
		M_Station->OnTargetDestroyed.AddUniqueDynamic(this, &UMission_BerlinForgeStation::OnStationTargetDestroyed);
	}
	TrySetUpGarrison();
}

bool UMission_BerlinForgeStation::GetIsValidStation() const
{
	if (M_Station.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Station",
	                                                             "UMission_BerlinForgeStation::GetIsValidStation",
	                                                             this);
	return false;
}

bool UMission_BerlinForgeStation::TrySetUpGarrison()
{
	if (bM_HasSetUpGarrison)
	{
		return true;
	}
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	if (not EnemyPlacement::TryGetGameDifficulty(this, Difficulty))
	{
		// The mission manager sets the difficulty up during the first frames; try again next tick.
		return false;
	}
	bM_HasSetUpGarrison = true;
	M_Forces = LoadForcesForDifficulty(Difficulty);
	AEnemyStrongpoint* Strongpoint = AEnemyStrongpoint::FindEnemyStrongpoint(this, StrongpointId);
	if (not IsValid(Strongpoint))
	{
		return true;
	}
	Strongpoint->OnGarrisonEngaged.AddUniqueDynamic(this, &UMission_BerlinForgeStation::OnGarrisonEngaged);
	if (IsValid(M_Forces))
	{
		Strongpoint->SetPostUnitOverrides(M_Forces->ForgeStation.Garrison);
	}
	else
	{
		RTSFunctionLibrary::ReportError("Forge station mission: no Berlin forces data asset for difficulty " +
			UEnum::GetValueAsString(Difficulty) + "; the strongpoint uses its own posts.");
	}
	Strongpoint->SpawnGarrison();
	return true;
}

UBerlinMissionForcesData* UMission_BerlinForgeStation::LoadForcesForDifficulty(
	const ERTSGameDifficulty Difficulty) const
{
	TArray<ERTSGameDifficulty> Candidates = {Difficulty};
	if (Difficulty == ERTSGameDifficulty::Ironman)
	{
		Candidates.Add(ERTSGameDifficulty::Brutal);
	}
	Candidates.Add(ERTSGameDifficulty::Normal);
	for (const ERTSGameDifficulty Candidate : Candidates)
	{
		const TSoftObjectPtr<UBerlinMissionForcesData>* Entry = ForcesPerDifficulty.Find(Candidate);
		if (Entry == nullptr)
		{
			continue;
		}
		if (UBerlinMissionForcesData* Forces = Entry->LoadSynchronous())
		{
			return Forces;
		}
	}
	return nullptr;
}

void UMission_BerlinForgeStation::OnMissionStartedNative()
{
	// The plate is up: the trigger fired. The portrait manager queues, so the crew chatter plays before the advisor.
	// Calm chatter makes no sense once the crew is already fighting.
	if (not bM_HasPlayedCrewEngagedLine)
	{
		PlayCrewLine(CrewLines.SightedLine);
	}
	PlayLine(Lines.SightedLine);
	StartTargetTracking();
}

void UMission_BerlinForgeStation::OnGarrisonEngaged(AEnemyStrongpoint* Strongpoint)
{
	if (bM_HasPlayedCrewEngagedLine || MissionState.bIsMissionComplete)
	{
		return;
	}
	bM_HasPlayedCrewEngagedLine = true;
	PlayCrewLine(CrewLines.EngagedLine);
}

void UMission_BerlinForgeStation::OnStationTargetDestroyed(ABerlinForgeStation* Station,
                                                           const EForgeStationPartRole Role)
{
	if (bM_HasPlayedCrewTargetLostLine || MissionState.bIsMissionComplete)
	{
		return;
	}
	bM_HasPlayedCrewTargetLostLine = true;
	// The first loss means a fight: the engaged line would only repeat the alarm after it.
	bM_HasPlayedCrewEngagedLine = true;
	PlayCrewLine(CrewLines.FirstTargetLostLine);
}

void UMission_BerlinForgeStation::StartTargetTracking()
{
	if (bM_HasStartedTracking || not GetIsValidStation())
	{
		return;
	}
	bM_HasStartedTracking = true;
	TArray<AActor*> AliveTargets;
	for (AActor* Target : M_Station->GetTargetActors())
	{
		if (RTSFunctionLibrary::RTSIsValid(Target))
		{
			AliveTargets.Add(Target);
		}
	}
	if (AliveTargets.IsEmpty())
	{
		// Everything died before the plate came up; TickMission fires the event.
		return;
	}
	TrackActorsWithMiniMapProgress(EMissionTrackingType::TrackOnActorsDestroyed, AliveTargets,
	                               BerlinForgeStationMissionConstants::TrackingValidityIntervalSeconds,
	                               TEXT("Station parts destroyed: "), TEXT("/"), TEXT(""),
	                               ERTSRichText::Text_Accent);
}

// ------------------------------------------------------------------
// Tick and completion
// ------------------------------------------------------------------

void UMission_BerlinForgeStation::TickMission(const float DeltaTime)
{
	Super::TickMission(DeltaTime);
	if (MissionState.bIsMissionComplete)
	{
		return;
	}
	TrySetUpGarrison();
	// The trigger fires as soon as a target dies; the reward starts once the plate is up. Tracking normally gets
	// there first (OnTrackingReachedMaxCountNative); this covers a station that was gone before the plate appeared.
	if (MissionState.bIsTriggered && GetAreAllStationTargetsDestroyed())
	{
		FireAllStationTargetsDestroyed();
	}
}

bool UMission_BerlinForgeStation::GetAreAllStationTargetsDestroyed() const
{
	return M_Station.IsValid() && M_Station->GetTargetCount() > 0 && M_Station->GetAliveTargetCount() == 0;
}

void UMission_BerlinForgeStation::FireAllStationTargetsDestroyed()
{
	if (bM_HasFiredAllTargetsDestroyed)
	{
		return;
	}
	bM_HasFiredAllTargetsDestroyed = true;
	if (M_Station.IsValid())
	{
		M_Station->StopStation();
	}
	BP_OnAllStationTargetsDestroyed();
}

void UMission_BerlinForgeStation::OnTrackingReachedMaxCountNative()
{
	FireAllStationTargetsDestroyed();
}

void UMission_BerlinForgeStation::GiveIntel()
{
	PlayLine(Lines.IntelLine);
	UMainGameUI* MainGameUI = FRTS_Statics::GetMainGameUI(this);
	if (not IsValid(MainGameUI))
	{
		RTSFunctionLibrary::ReportError("Forge station mission: no main game UI for the crater intel archive entry.");
		return;
	}
	MainGameUI->AddNewCustomArchiveItem(CraterIntel, BerlinForgeStationMissionConstants::ArchiveSortingPriority,
	                                    BerlinForgeStationMissionConstants::ArchiveNotificationSeconds);
	bM_HasAddedArchiveEntry = true;
}

void UMission_BerlinForgeStation::LaunchReinforcements()
{
	if (bM_HasLaunchedReinforcements)
	{
		return;
	}
	bM_HasLaunchedReinforcements = true;
	PlayLine(Lines.ReinforcementsLine);
	if (not IsValid(M_Forces) || M_Forces->ForgeStation.Reinforcements.GetIsEmpty())
	{
		RTSFunctionLibrary::ReportError("Forge station mission: no reinforcements set in the Berlin forces data "
			"asset for this difficulty.");
		return;
	}
	UPlayerAirDropSubsystem* AirDrop = UPlayerAirDropSubsystem::Get(this);
	if (not IsValid(AirDrop))
	{
		return;
	}
	const FVector LandingPoint = GetGroundedLandingPoint();
	AirDrop->DeliverPlayerUnitsByAir(LandingPoint, M_Forces->ForgeStation.Reinforcements);
	ShowReinforcementsOnMiniMap(LandingPoint);
}

FVector UMission_BerlinForgeStation::GetGroundedLandingPoint() const
{
	using namespace BerlinForgeStationMissionConstants;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return ReinforcementLandingPoint;
	}
	FHitResult Hit;
	const FVector Start = ReinforcementLandingPoint + FVector(0.f, 0.f, LandingTraceHalfHeight);
	const FVector End = ReinforcementLandingPoint - FVector(0.f, 0.f, LandingTraceHalfHeight);
	FCollisionQueryParams QueryParams(TEXT("ForgeStationLanding"), false);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
	{
		return Hit.ImpactPoint;
	}
	return ReinforcementLandingPoint;
}

void UMission_BerlinForgeStation::PlayLine(USoundBase* Line) const
{
	if (not IsValid(Line))
	{
		return;
	}
	PlayPortrait(Lines.Portrait, Line);
}

void UMission_BerlinForgeStation::PlayCrewLine(USoundBase* Line) const
{
	if (not IsValid(Line))
	{
		return;
	}
	PlayPortrait(CrewLines.Portrait, Line);
}
