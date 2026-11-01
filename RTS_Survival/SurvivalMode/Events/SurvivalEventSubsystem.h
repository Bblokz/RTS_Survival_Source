// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/SurvivalMode/Events/SurvivalConvoyRoutePlanner.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "SurvivalEventSubsystem.generated.h"

class AConvoy;
class ASurvivalModeDirector;
class UConvoyDefinition;
class USurvivalEventSettings;
class UW_SurvivalEventBanner;
enum class ESurvivalPhase : uint8;
enum class ESurvivalRoundState : uint8;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSurvivalEventLog, const FString& /*EventName*/, const FString& /*Detail*/);

/** What the scheduler decided for one survived round. */
struct FSurvivalEventRoll
{
	bool bFires = false;
	bool bForced = false;
	int32 DefinitionIndex = INDEX_NONE;
	int32 GuardTier = 0;
	float CargoMultiplier = 1.f;
	FString Reason;
};

/**
 * @brief Survival random events: rolls an event when a round starts clearing, spawns it out of sight at a far edge
 * once the round is nearly over, announces it when the intermission starts and holds the intermission open (up to
 * the data's bound) while the convoy is live. The first event type is an escorted supply convoy (AConvoy).
 * @note StartForMatch: the director calls it once, after the start content is generated.
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalEventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** @brief The director's one hook: binds the scheduler to its phases and the wave director's rounds. */
	static void StartForMatch(ASurvivalModeDirector* Director);

	virtual void Deinitialize() override;

	const USurvivalEventSettings* GetSettings() const { return M_Settings; }
	AConvoy* GetActiveConvoy() const { return M_ActiveConvoy.Get(); }
	const TArray<TObjectPtr<UConvoyDefinition>>& GetDefinitions() const { return M_Definitions; }
	ASurvivalModeDirector* GetDirector() const { return M_Director.Get(); }
	int32 GetMatchSeed() const { return M_MatchSeed; }

	/**
	 * @brief The scheduler's decision for a survived round (no side effects; the sweep and the live game share it).
	 * @param Round Survived round number (1 = first wave).
	 * @param LastEventRound Round of the previous event (very negative when none).
	 * @param QuietRounds Allowed rounds without an event since the last one.
	 * @param Difficulty Match difficulty.
	 * @param Seed Match seed.
	 * @return Whether an event fires and which convoy, tier and cargo multiplier.
	 */
	FSurvivalEventRoll RollForRound(int32 Round, int32 LastEventRound, int32 QuietRounds,
	                                ERTSGameDifficulty Difficulty, int32 Seed) const;

	/**
	 * @brief Plans a convoy route for this match with the live rules (base, Soviet bases, edges, active spawns).
	 * @param Seed Route seed.
	 * @param Definition Its trucks set the pace the length budget uses.
	 * @param ForcedEntry Test override: entry edge actor name, empty = any.
	 * @param ForcedExit Test override: exit edge actor name, empty = any.
	 * @return The plan (bIsValid false when nothing passed).
	 */
	FSurvivalConvoyRoutePlan PlanRoute(int32 Seed, const UConvoyDefinition* Definition, const FString& ForcedEntry,
	                                   const FString& ForcedExit) const;

	/**
	 * @brief Starts a convoy now through the same path the scheduler uses (tests force definition, tier and route).
	 * @param Definition The convoy.
	 * @param GuardTier Guard tier index.
	 * @param CargoMultiplier Cargo scale.
	 * @param Plan A valid route plan.
	 * @param Round Round the event belongs to (logged; 0 for the first grace).
	 * @param bAnnounceNow Announce at once instead of at the intermission start.
	 * @return The convoy, nullptr when it could not start.
	 */
	AConvoy* StartConvoyEvent(UConvoyDefinition* Definition, int32 GuardTier, float CargoMultiplier,
	                          const FSurvivalConvoyRoutePlan& Plan, int32 Round, bool bAnnounceNow);

	/** @brief Broadcast for every scheduler decision (RoundSurvived, EventRolled, EventStarted, Announced, ...). */
	FOnSurvivalEventLog OnEventLog;

	/** @return Pace (cm/s) the definition's trucks would drive at for this owner. */
	float EstimatePaceSpeed(const UConvoyDefinition* Definition) const;

private:
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	UPROPERTY()
	TObjectPtr<USurvivalEventSettings> M_Settings;

	UPROPERTY()
	TArray<TObjectPtr<UConvoyDefinition>> M_Definitions;

	UPROPERTY()
	TWeakObjectPtr<AConvoy> M_ActiveConvoy;

	UPROPERTY()
	TObjectPtr<UW_SurvivalEventBanner> M_Banner;

	FTimerHandle M_PollTimer;
	FTimerHandle M_BannerHideTimer;
	FDelegateHandle M_PhaseChangedHandle;
	FDelegateHandle M_RoundSurvivedHandle;
	FDelegateHandle M_ConvoyEventHandle;
	int32 M_MatchSeed = 0;
	bool bM_IsStarted = false;

	// Round tracking (the wave director has no round-state delegate: polled once a second).
	ESurvivalRoundState M_LastRoundState{};
	int32 M_LastEventRound = -1000;
	int32 M_QuietRounds = 0;

	// An event rolled for the round that is clearing, waiting to spawn.
	bool bM_HasPendingEvent = false;
	FSurvivalEventRoll M_PendingRoll;
	int32 M_PendingRound = 0;

	// The live event (from its start until it is resolved: escaped, or trucks and cargo gone).
	bool bM_HasActiveEvent = false;
	int32 M_ActiveRound = 0;
	bool bM_ActiveAnnounced = false;
	bool bM_ActiveResolved = false;
	bool bM_ActiveExtended = false;
	FSurvivalConvoyRoutePlan M_ActivePlan;
	FText M_ActiveTitle;

	void BindToDirector(ASurvivalModeDirector* Director);
	void LoadSettings();
	int32 ResolveMatchSeed() const;
	void OnPhaseChanged(ESurvivalPhase OldPhase, ESurvivalPhase NewPhase);
	void OnRoundSurvived(int32 RoundNumber);
	void Poll();
	void OnClearingStarted(int32 RoundNumber);
	void TrySpawnPendingEvent(const TCHAR* Moment);
	FSurvivalConvoyRouteRequest BuildRouteRequest(int32 Seed) const;
	void AnnounceActiveEvent();
	TArray<FText> BuildAnnouncementLines(const AConvoy& Convoy) const;
	void UpdateBanner();
	void OnConvoyEvent(const FConvoyEvent& Event);
	void CheckActiveEventResolved();
	void ResolveActiveEvent(const FText& Status);
	void HideBanner();
	void LogEvent(const FString& Name, const FString& Detail);
	bool GetIsValidDirector() const;
	bool GetIsValidSettings() const;
	ERTSGameDifficulty GetDifficultyLevel() const;
};
