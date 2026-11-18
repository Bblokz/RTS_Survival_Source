// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionChainAuditSubsystem.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/Tests/MissionMapTestLibrary.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTSMapTests/DamageHooks/DamageMapTestHooks.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponOwner/WeaponOwner.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSMissionChainAudit, Log, All);

namespace MissionChainAuditPrivate
{
	const TCHAR* const AuditSwitch = TEXT("RTSMissionChainAudit");
	const TCHAR* const DirectorySwitch = TEXT("RTSMissionChainAuditDir=");
	const TCHAR* const StepSwitch = TEXT("RTSMissionChainAuditStep=");
	const FName StackDumpCategory(TEXT("LogOutputDevice"));
	constexpr float DefaultStepSeconds = 30.f;
	constexpr float MinStepSeconds = 5.f;
	constexpr float UnitAuditIntervalSeconds = 5.f;
	constexpr float SummaryIntervalSeconds = 20.f;
	// Turrets and infantry weapons set up their weapon states a moment after the unit begins play.
	constexpr float WeaponSettleSeconds = 6.f;
	constexpr int32 MaxErrorMessageChars = 220;
	constexpr int32 MaxListedErrors = 30;
	constexpr int32 MaxListedProblems = 15;
	// Slot for owners outside 0..MaxPlayers (an unset 222).
	constexpr int32 OtherOwnerSlot = RTSPlayers::PlayerArraySize;

	void AddCarrierIfWeaponOwner(TArray<UObject*>& InOutCarriers, UObject* Candidate)
	{
		if (IsValid(Candidate) && Cast<IWeaponOwner>(Candidate) != nullptr)
		{
			InOutCarriers.AddUnique(Candidate);
		}
	}

	/** The unit, its components (hull weapons) and attached actors (turrets, infantry weapons) that hold weapons. */
	TArray<UObject*> GetWeaponCarriers(AActor& Unit)
	{
		TArray<UObject*> Carriers;
		AddCarrierIfWeaponOwner(Carriers, &Unit);
		TArray<UActorComponent*> Components;
		Unit.GetComponents(Components);
		for (UActorComponent* const Component : Components)
		{
			AddCarrierIfWeaponOwner(Carriers, Component);
		}
		TArray<AActor*> AttachedActors;
		Unit.GetAttachedActors(AttachedActors, true, true);
		for (AActor* const AttachedActor : AttachedActors)
		{
			AddCarrierIfWeaponOwner(Carriers, AttachedActor);
		}
		return Carriers;
	}

	int32 GetOwnerSlot(const int32 Owner)
	{
		return Owner >= RTSPlayers::NeutralOwner && Owner <= RTSPlayers::MaxPlayers ? Owner : OtherOwnerSlot;
	}
}

/** Keeps every distinct error and ensure line with its count while the map runs; registered with GLog. */
class FMissionChainAuditLogCapture final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* Message, const ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		const ELogVerbosity::Type BaseVerbosity = static_cast<ELogVerbosity::Type>(
			Verbosity & ELogVerbosity::VerbosityMask);
		if (BaseVerbosity > ELogVerbosity::Error || Message == nullptr
			|| Category == MissionChainAuditPrivate::StackDumpCategory)
		{
			return;
		}
		const FString Key = Category.ToString() + TEXT(": ") + FString(Message).Left(
			MissionChainAuditPrivate::MaxErrorMessageChars).Replace(TEXT("\n"), TEXT(" "));
		FScopeLock Lock(&M_Lock);
		++M_CountByMessage.FindOrAdd(Key);
		++M_TotalErrors;
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }

	void GetErrors(TArray<TPair<FString, int32>>& OutErrors, int32& OutTotal)
	{
		FScopeLock Lock(&M_Lock);
		OutTotal = M_TotalErrors;
		for (const TPair<FString, int32>& Entry : M_CountByMessage)
		{
			OutErrors.Add(Entry);
		}
		OutErrors.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
		{
			return A.Value > B.Value;
		});
	}

private:
	FCriticalSection M_Lock;
	TMap<FString, int32> M_CountByMessage;
	int32 M_TotalErrors = 0;
};

bool UMissionChainAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), MissionChainAuditPrivate::AuditSwitch);
}

void UMissionChainAuditSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace MissionChainAuditPrivate;
	Super::OnWorldBeginPlay(InWorld);
	if (not FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory))
	{
		M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("MissionChainAudit") /
			(FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT("_") + InWorld.GetMapName());
	}
	float StepSeconds = DefaultStepSeconds;
	if (FParse::Value(FCommandLine::Get(), StepSwitch, StepSeconds))
	{
		M_StepSeconds = FMath::Max(MinStepSeconds, StepSeconds);
	}
	M_LogCapture = MakeShared<FMissionChainAuditLogCapture>();
	GLog->AddOutputDevice(M_LogCapture.Get());
	M_DamageHandle = FDamageMapTestHooks::OnDamageApplied().AddUObject(
		this, &UMissionChainAuditSubsystem::OnDamageApplied);
	bM_IsActive = true;
	UE_LOG(LogRTSMissionChainAudit, Display, TEXT("RTS_MCA started map=%s step_s=%.0f dir=%s"),
	       *InWorld.GetMapName(), M_StepSeconds, *M_OutputDirectory);
}

void UMissionChainAuditSubsystem::Deinitialize()
{
	if (bM_IsActive)
	{
		bM_IsActive = false;
		const TArray<FString> Reasons = EvaluateVerdict();
		WriteSummary(true);
		UE_LOG(LogRTSMissionChainAudit, Display,
		       TEXT("RTS_MCA RESULT %s reasons=%s missions_seen=%d triggers_fired=%d completed=%d units=%d "
			       "enemy_formation_units=%d summary=%s/summary.md"),
		       Reasons.Num() == 0 ? TEXT("PASS") : TEXT("FAIL"),
		       Reasons.Num() == 0 ? TEXT("none") : *FString::Join(Reasons, TEXT(";")), M_MissionsSeen.Num(),
		       M_TriggersFired, M_MissionsCompleted, M_UnitsSeen.Num(), M_EnemyFormationUnitsSeen.Num(),
		       *M_OutputDirectory);
	}
	FDamageMapTestHooks::OnDamageApplied().Remove(M_DamageHandle);
	if (M_LogCapture.IsValid() && GLog != nullptr)
	{
		GLog->RemoveOutputDevice(M_LogCapture.Get());
	}
	Super::Deinitialize();
}

TStatId UMissionChainAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMissionChainAuditSubsystem, STATGROUP_Tickables);
}

void UMissionChainAuditSubsystem::Tick(float DeltaTime)
{
	using namespace MissionChainAuditPrivate;
	const UWorld* const World = GetWorld();
	if (not bM_IsActive || not IsValid(World))
	{
		return;
	}
	const float GameSeconds = World->GetTimeSeconds();
	TickMissionChain(GameSeconds);
	if (M_LastUnitAuditGameSeconds < 0.f || GameSeconds - M_LastUnitAuditGameSeconds >= UnitAuditIntervalSeconds)
	{
		M_LastUnitAuditGameSeconds = GameSeconds;
		AuditUnits(GameSeconds);
		AuditEnemyFormations(GameSeconds);
	}
	if (M_LastSummaryGameSeconds < 0.f || GameSeconds - M_LastSummaryGameSeconds >= SummaryIntervalSeconds)
	{
		M_LastSummaryGameSeconds = GameSeconds;
		WriteSummary(false);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Mission chain
// ---------------------------------------------------------------------------------------------------------------------

void UMissionChainAuditSubsystem::TickMissionChain(const float GameSeconds)
{
	RecordActiveMissions(GameSeconds);
	if (M_LastStepGameSeconds < 0.f)
	{
		// The first step waits a full interval: the opening mission runs on its own first.
		M_LastStepGameSeconds = GameSeconds;
		return;
	}
	if (GameSeconds - M_LastStepGameSeconds < M_StepSeconds)
	{
		return;
	}
	M_LastStepGameSeconds = GameSeconds;
	if (not TryAdvanceMissionChain(GameSeconds))
	{
		AddTimelineLine(GameSeconds, TEXT("no mission to advance"));
	}
}

bool UMissionChainAuditSubsystem::TryAdvanceMissionChain(const float GameSeconds)
{
	const TArray<UMissionBase*> ActiveMissions = UMissionMapTestLibrary::MapTest_GetActiveMissions(this);
	// A mission waiting for the player (an area, a timer) gets its trigger first: its Blueprint start logic runs then.
	for (UMissionBase* const Mission : ActiveMissions)
	{
		if (UMissionMapTestLibrary::MapTest_FireMissionTrigger(Mission))
		{
			++M_TriggersFired;
			AddTimelineLine(GameSeconds, TEXT("fired trigger of ") + Mission->GetClass()->GetName());
			return true;
		}
	}
	// Otherwise the running mission that ran longest completes, which starts its next mission(s).
	UMissionBase* OldestRunning = nullptr;
	float OldestSince = TNumericLimits<float>::Max();
	for (UMissionBase* const Mission : ActiveMissions)
	{
		const float* const Since = M_RunningSinceGameSeconds.Find(Mission);
		if (UMissionMapTestLibrary::MapTest_GetIsMissionRunning(Mission) && Since != nullptr && *Since < OldestSince)
		{
			OldestSince = *Since;
			OldestRunning = Mission;
		}
	}
	if (not IsValid(OldestRunning))
	{
		return false;
	}
	++M_MissionsCompleted;
	AddTimelineLine(GameSeconds, TEXT("completed ") + OldestRunning->GetClass()->GetName());
	OldestRunning->OnMissionComplete();
	return true;
}

void UMissionChainAuditSubsystem::RecordActiveMissions(const float GameSeconds)
{
	for (UMissionBase* const Mission : UMissionMapTestLibrary::MapTest_GetActiveMissions(this))
	{
		const FString MissionName = Mission->GetClass()->GetName();
		if (not M_MissionsSeen.Contains(MissionName))
		{
			M_MissionsSeen.Add(MissionName);
			AddTimelineLine(GameSeconds, TEXT("active: ") + MissionName);
		}
		if (UMissionMapTestLibrary::MapTest_GetIsMissionRunning(Mission) && not M_RunningSinceGameSeconds.Contains(Mission))
		{
			M_RunningSinceGameSeconds.Add(Mission, GameSeconds);
			AddTimelineLine(GameSeconds, TEXT("running: ") + MissionName);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Units, weapons and the enemy controller
// ---------------------------------------------------------------------------------------------------------------------

void UMissionChainAuditSubsystem::AuditUnits(const float GameSeconds)
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* const Actor = *It;
		const URTSComponent* const RTSComponent = IsValid(Actor) ? Actor->FindComponentByClass<URTSComponent>() : nullptr;
		if (IsValid(RTSComponent))
		{
			AuditUnit(*Actor, RTSComponent->GetOwningPlayer(), GameSeconds);
		}
	}
}

void UMissionChainAuditSubsystem::AuditUnit(AActor& Unit, const int32 Owner, const float GameSeconds)
{
	using namespace MissionChainAuditPrivate;
	const TWeakObjectPtr<AActor> UnitKey(&Unit);
	if (not M_UnitsSeen.Contains(UnitKey))
	{
		M_UnitsSeen.Add(UnitKey);
		M_FirstSeenGameSeconds.Add(UnitKey, GameSeconds);
		++M_UnitsByOwner[GetOwnerSlot(Owner)];
		// These missions know the human and one AI player; players 3 and 4 must never appear on them.
		if (Owner > RTSPlayers::FirstAIPlayer && Owner <= RTSPlayers::MaxPlayers
			&& M_OwnerProblems.Num() < MaxListedProblems)
		{
			M_OwnerProblems.Add(FString::Printf(TEXT("%s owned by P%d"), *Unit.GetName(), Owner));
		}
	}
	const float* const FirstSeen = M_FirstSeenGameSeconds.Find(UnitKey);
	const bool bIsSettled = FirstSeen != nullptr && GameSeconds - *FirstSeen >= WeaponSettleSeconds;
	if (not bIsSettled || M_UnitsWithWeaponsChecked.Contains(UnitKey) || FRTSPlayers::IsNeutralOwner(Owner))
	{
		return;
	}
	M_UnitsWithWeaponsChecked.Add(UnitKey);
	const int32 ExpectedWeaponOwner = FRTSPlayers::NormalizePlayer(Owner);
	const TArray<UObject*> Carriers = GetWeaponCarriers(Unit);
	for (UObject* const Carrier : Carriers)
	{
		const TArray<UWeaponState*> Weapons = Cast<IWeaponOwner>(Carrier)->GetWeapons();
		for (const UWeaponState* const Weapon : Weapons)
		{
			if (not IsValid(Weapon) || FRTSPlayers::NormalizePlayer(Weapon->GetOwningPlayer()) == ExpectedWeaponOwner)
			{
				continue;
			}
			++M_WeaponOwnerMismatches;
			if (M_OwnerProblems.Num() < MaxListedProblems)
			{
				M_OwnerProblems.Add(FString::Printf(TEXT("%s (P%d): weapon on %s reports P%d"), *Unit.GetName(), Owner,
				                                    *GetNameSafe(Carrier), Weapon->GetOwningPlayer()));
			}
		}
	}
}

void UMissionChainAuditSubsystem::AuditEnemyFormations(const float GameSeconds)
{
	const TArray<AEnemyController*> EnemyControllers = FRTS_Statics::GetAllEnemyControllers(this);
	M_EnemyControllerCount = FMath::Max(M_EnemyControllerCount, EnemyControllers.Num());
	int32 FormationUnitsNow = 0;
	for (const AEnemyController* const Controller : EnemyControllers)
	{
		if (not IsValid(Controller))
		{
			continue;
		}
		TArray<AActor*> FormationUnits = Controller->GetAllTankActorsInFormations();
		FormationUnits.Append(Controller->GetAllSquadControllerActorsInFormations());
		for (AActor* const Unit : FormationUnits)
		{
			if (not IsValid(Unit))
			{
				continue;
			}
			++FormationUnitsNow;
			const int32 UnitOwner = FRTSPlayers::GetOwningPlayerOfActor(Unit);
			const bool bIsNew = not M_EnemyFormationUnitsSeen.Contains(Unit);
			M_EnemyFormationUnitsSeen.Add(Unit);
			if (bIsNew && UnitOwner != Controller->GetControlledPlayer())
			{
				++M_EnemyFormationOwnerMismatches;
				M_OwnerProblems.Add(FString::Printf(TEXT("%s in formation of P%d's controller is owned by P%d"),
				                                    *Unit->GetName(), Controller->GetControlledPlayer(), UnitOwner));
			}
		}
	}
	if (FormationUnitsNow > M_PeakEnemyFormationUnits)
	{
		M_PeakEnemyFormationUnits = FormationUnitsNow;
		AddTimelineLine(GameSeconds, FString::Printf(TEXT("enemy formation units now %d"), FormationUnitsNow));
	}
}

void UMissionChainAuditSubsystem::OnDamageApplied(const FMapTestDamageReport& Report)
{
	if (Report.AppliedDamage <= 0.f)
	{
		return;
	}
	const int32 Attacker = FRTSPlayers::IsValidPlayer(Report.AttackingPlayer)
		                       ? Report.AttackingPlayer
		                       : RTSPlayers::NeutralOwner;
	// Unowned victims (walls, scenery) and neutral ones share the neutral column; never normalized into player 2.
	const int32 Victim = FRTSPlayers::IsValidPlayer(Report.VictimOwner) ? Report.VictimOwner : RTSPlayers::NeutralOwner;
	M_DamageMatrix[Attacker][Victim] += Report.AppliedDamage;
	++M_HitMatrix[Attacker][Victim];
}

// ---------------------------------------------------------------------------------------------------------------------
// Verdict and report
// ---------------------------------------------------------------------------------------------------------------------

TArray<FString> UMissionChainAuditSubsystem::EvaluateVerdict() const
{
	TArray<FString> Reasons;
	if (M_EnemyControllerCount == 0)
	{
		Reasons.Add(TEXT("no_enemy_controller"));
	}
	const int32 LaterAIPlayerUnits = M_UnitsByOwner[3] + M_UnitsByOwner[4];
	if (LaterAIPlayerUnits > 0)
	{
		Reasons.Add(FString::Printf(TEXT("units_of_players_3_4=%d"), LaterAIPlayerUnits));
	}
	if (M_WeaponOwnerMismatches > 0)
	{
		Reasons.Add(FString::Printf(TEXT("weapon_owner_mismatches=%d"), M_WeaponOwnerMismatches));
	}
	if (M_EnemyFormationOwnerMismatches > 0)
	{
		Reasons.Add(FString::Printf(TEXT("enemy_formation_owner_mismatches=%d"), M_EnemyFormationOwnerMismatches));
	}
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		if (M_DamageMatrix[Player][Player] > 0.f)
		{
			Reasons.Add(FString::Printf(TEXT("P%d_damaged_own_units=%.0f"), Player, M_DamageMatrix[Player][Player]));
		}
	}
	return Reasons;
}

void UMissionChainAuditSubsystem::WriteSummary(const bool bIsFinal) const
{
	using namespace MissionChainAuditPrivate;
	const UWorld* const World = GetWorld();
	TArray<FString> Lines;
	const TArray<FString> Reasons = EvaluateVerdict();
	Lines.Add(FString::Printf(TEXT("# Mission chain audit: %s"), IsValid(World) ? *World->GetMapName() : TEXT("?")));
	Lines.Add(FString::Printf(TEXT("\n**%s**%s (%s, written %s)"), Reasons.Num() == 0 ? TEXT("PASS") : TEXT("FAIL"),
	                          Reasons.Num() > 0 ? *(TEXT(": ") + FString::Join(Reasons, TEXT(", "))) : TEXT(""),
	                          bIsFinal ? TEXT("final") : TEXT("interim"), *FDateTime::Now().ToString()));
	Lines.Add(FString::Printf(TEXT("\nMissions seen %d, triggers fired %d, missions completed %d (one step every %.0f s)."),
	                          M_MissionsSeen.Num(), M_TriggersFired, M_MissionsCompleted, M_StepSeconds));
	Lines.Add(FString::Printf(TEXT("Enemy controllers %d; enemy formation units seen %d, peak at once %d; formation units "
		                          "not owned by their controller's player: %d."), M_EnemyControllerCount,
	                          M_EnemyFormationUnitsSeen.Num(), M_PeakEnemyFormationUnits,
	                          M_EnemyFormationOwnerMismatches));
	Lines.Add(FString::Printf(TEXT("Units seen %d: neutral %d, P1 %d, P2 %d, P3 %d, P4 %d, other owner %d. Weapon owner "
		                          "mismatches %d."), M_UnitsSeen.Num(), M_UnitsByOwner[0], M_UnitsByOwner[1],
	                          M_UnitsByOwner[2], M_UnitsByOwner[3], M_UnitsByOwner[4], M_UnitsByOwner[OtherOwnerSlot],
	                          M_WeaponOwnerMismatches));
	Lines.Add(TEXT("\n## Damage (health lost / hits), attacker row -> victim owner\n"));
	Lines.Add(TEXT("| Attacker | Neutral | P1 | P2 | P3 | P4 |"));
	Lines.Add(TEXT("|---|---|---|---|---|---|"));
	for (int32 Attacker = RTSPlayers::NeutralOwner; Attacker <= RTSPlayers::MaxPlayers; ++Attacker)
	{
		FString Row = Attacker == RTSPlayers::NeutralOwner ? FString(TEXT("| unclaimed |"))
			              : FString::Printf(TEXT("| P%d |"), Attacker);
		for (int32 Victim = RTSPlayers::NeutralOwner; Victim <= RTSPlayers::MaxPlayers; ++Victim)
		{
			Row += FString::Printf(TEXT(" %.0f / %d |"), M_DamageMatrix[Attacker][Victim], M_HitMatrix[Attacker][Victim]);
		}
		Lines.Add(Row);
	}
	Lines.Add(TEXT("\n## Owner problems\n"));
	Lines.Add(M_OwnerProblems.Num() == 0 ? FString(TEXT("None.")) : TEXT("- ") + FString::Join(M_OwnerProblems,
		TEXT("\n- ")));
	Lines.Add(TEXT("\n## Timeline (game seconds)\n"));
	Lines.Append(M_Timeline);
	Lines.Add(TEXT("\n## Error lines (most frequent first)\n"));
	TArray<TPair<FString, int32>> Errors;
	int32 TotalErrors = 0;
	if (M_LogCapture.IsValid())
	{
		M_LogCapture->GetErrors(Errors, TotalErrors);
	}
	Lines.Add(FString::Printf(TEXT("%d error lines, %d distinct.\n"), TotalErrors, Errors.Num()));
	for (int32 Index = 0; Index < Errors.Num() && Index < MaxListedErrors; ++Index)
	{
		Lines.Add(FString::Printf(TEXT("- %dx %s"), Errors[Index].Value, *Errors[Index].Key));
	}
	FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")) + TEXT("\n"), *(M_OutputDirectory / TEXT("summary.md")));
}

void UMissionChainAuditSubsystem::AddTimelineLine(const float GameSeconds, const FString& Line)
{
	M_Timeline.Add(FString::Printf(TEXT("- %.1f %s"), GameSeconds, *Line));
	UE_LOG(LogRTSMissionChainAudit, Display, TEXT("RTS_MCA t=%.1f %s"), GameSeconds, *Line);
}

#else

bool UMissionChainAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UMissionChainAuditSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UMissionChainAuditSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UMissionChainAuditSubsystem::Tick(float DeltaTime)
{
}

TStatId UMissionChainAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMissionChainAuditSubsystem, STATGROUP_Tickables);
}

#endif
