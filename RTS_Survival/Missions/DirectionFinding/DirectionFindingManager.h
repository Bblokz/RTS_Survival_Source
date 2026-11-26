// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DirectionFindingSolver.h"
#include "DirectionFindingTypes.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementActor.h"
#include "DirectionFindingManager.generated.h"

class ASupplyPoint;
class UAudioComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDirectionFindingStationChanged, int32, StationIndex, bool,
                                             bHeldByPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDirectionFindingBurstHeard, EDirectionFindingBurst, BurstType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDirectionFindingFirstOverlap);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDirectionFindingFix, int32, CommandPostWerkIndex);

/**
 * @brief Place one per map (Ger_OderWarthe): runs the hunt for the hidden command post. It picks the command post
 * among Werke, gives every station the player holds a bearing wedge on the minimap and the tactical map, narrows the
 * held wedges on every burst the command post transmits, and raises OnFix once the overlap holds one bunker group.
 * The command post's three targets spawn hidden at its Werk and appear at the fix.
 * Missions feed it: NotifyAttackOrder when a wave launches, NotifyPlayerAttackedSite when the player hits an outpost
 * or a Werk, SetWerkEliminated when a wrong group is destroyed. Routine bursts run on their own timer.
 * Rules: Source/Docs/Campaign/OderWartheMissionFlow.md sections 4 and 10.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ADirectionFindingManager : public AEnemyPlacementActor
{
	GENERATED_BODY()

public:
	ADirectionFindingManager();

	virtual FColor GetPlacementColor() const override;
	virtual FString GetConfigurationProblem() const override;

	/** A wave launched: every held station hears the attack order. */
	UFUNCTION(BlueprintCallable, Category = "Direction Finding")
	void NotifyAttackOrder();

	/**
	 * @brief The player attacked a site (an outpost, a Werk). The command post answers after ReplyDelaySeconds unless
	 * this site caused a reply less than ReplyCooldownPerSiteSeconds ago.
	 * @param SiteId Any name that stays the same for one site.
	 * @return True when a reply is on its way.
	 */
	UFUNCTION(BlueprintCallable, Category = "Direction Finding")
	bool NotifyPlayerAttackedSite(const FName SiteId);

	/** A wrong bunker group was destroyed and drops out of the search; ignored for the command post. */
	UFUNCTION(BlueprintCallable, Category = "Direction Finding")
	void SetWerkEliminated(const int32 WerkIndex);

	/** Transmits a burst at once (tests, scripted moments). */
	UFUNCTION(BlueprintCallable, Category = "Direction Finding")
	void ForceBurst(const EDirectionFindingBurst BurstType);

	/** Fixes the command post at once, as if the bearings had found it. */
	UFUNCTION(BlueprintCallable, Category = "Direction Finding")
	void ForceFix();

	/**
	 * @brief A decoy transmitter (the Hard+ radio truck, once parked) starts answering every burst from DecoyLocation:
	 * each held station gets a second wedge towards it, as wide as its real one and drawn the same. The decoy never
	 * makes a fix; the fix removes it.
	 * @param DecoyLocation Where the decoy transmits from (world).
	 */
	UFUNCTION(BlueprintCallable, Category = "Direction Finding")
	void ActivateDecoy(const FVector DecoyLocation);

	/** The decoy went silent (its truck was destroyed): its wedges disappear. */
	UFUNCTION(BlueprintCallable, Category = "Direction Finding")
	void DeactivateDecoy();

	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	bool GetIsDecoyActive() const { return bM_IsDecoyActive; }

	/** Stops the jammer noise bed (the jam lifts at victory). */
	UFUNCTION(BlueprintCallable, Category = "Direction Finding")
	void StopJammer();

	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	int32 GetCommandPostWerkIndex() const { return M_Solver.GetCommandPostIndex(); }

	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	FName GetCommandPostWerkId() const;

	/** @return The jammer, command porch and rail gate of the command post (hidden until the fix). */
	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	TArray<AActor*> GetCommandPostTargets() const;

	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	int32 GetHeldStationCount() const { return M_Solver.GetHeldWedgeCount(); }

	/** @return Standing bunker groups inside every held wedge (0 while no station is held). */
	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	int32 GetCandidateWerkCount() const { return M_Solver.GetCandidateWerke().Num(); }

	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	bool GetIsFixed() const { return bM_IsFixed; }

	/** @return True once the player held two stations: the command post has noticed them. */
	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	bool GetHasNoticedPlayer() const { return bM_HasRaisedFirstOverlap; }

	/** @return Half-angle in degrees of the station's wedge; 0 while it has none. */
	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	float GetStationHalfAngle(const int32 StationIndex) const;

	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	int32 GetStationIndex(const ASupplyPoint* Station) const;

	/** @return True while the player holds the station (StationIndex 0 = DF 1). */
	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	bool GetIsStationHeldByPlayer(const int32 StationIndex) const;

	/** @return True once the bunker group was destroyed and dropped out of the search (SetWerkEliminated). */
	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	bool GetIsWerkEliminated(const int32 WerkIndex) const { return M_Solver.GetIsWerkEliminated(WerkIndex); }

	UFUNCTION(BlueprintPure, Category = "Direction Finding")
	int32 GetWerkCount() const { return Werke.Num(); }

	/** @return The map's direction-finding manager, or null (no error) on maps without one. */
	UFUNCTION(BlueprintPure, Category = "Direction Finding", meta = (WorldContext = "WorldContextObject"))
	static ADirectionFindingManager* FindDirectionFindingManager(const UObject* WorldContextObject);

	const TArray<TObjectPtr<ASupplyPoint>>& GetStations() const { return Stations; }
	const TArray<FDirectionFindingWerk>& GetWerke() const { return Werke; }
	const FDirectionFindingSolver& GetSolver() const { return M_Solver; }
	const FDirectionFindingLook& GetLook() const { return Look; }
	bool GetHasStarted() const { return bM_HasStarted; }

	/**
	 * @brief The wedges and their overlap as the map shows them now.
	 * @param MapBounds The minimap's world square (XY, cm).
	 * @param OutDrawData Filled with polygons clipped to MapBounds.
	 */
	void GetDrawData(const FBox2D& MapBounds, FDirectionFindingDrawData& OutDrawData) const;

	// The player took (bHeldByPlayer) or lost a station.
	UPROPERTY(BlueprintAssignable, Category = "Direction Finding")
	FOnDirectionFindingStationChanged OnStationChanged;

	// A burst was heard by at least one held station.
	UPROPERTY(BlueprintAssignable, Category = "Direction Finding")
	FOnDirectionFindingBurstHeard OnBurstHeard;

	// The player holds two stations for the first time: the command post notices them (M06).
	UPROPERTY(BlueprintAssignable, Category = "Direction Finding")
	FOnDirectionFindingFirstOverlap OnFirstOverlap;

	// The bearings found the command post; its targets are now visible and can be attacked.
	UPROPERTY(BlueprintAssignable, Category = "Direction Finding")
	FOnDirectionFindingFix OnFix;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FString GetPlacementKindName() const override;
	virtual FName GetPlacementId() const override { return TEXT("DirectionFinding"); }
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const override;
	virtual void OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty) override;
	virtual bool GetNeedsEnemyController() const override { return false; }

	// The capture-only stations in DF order (DF 1 first). Set by the map build.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Direction Finding|Map")
	TArray<TObjectPtr<ASupplyPoint>> Stations;

	// The bunker groups the command post can sit in. Set by the map build.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Direction Finding|Map")
	TArray<FDirectionFindingWerk> Werke;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding")
	FDirectionFindingSettings Settings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding")
	FDirectionFindingLook Look;

	// Index into Werke of a fixed command post; -1 picks one at random each play.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Command Post", meta = (ClampMin = "-1"))
	int32 ForcedCommandPostIndex = INDEX_NONE;

	// Seed for the command post pick and the wedge errors; 0 seeds from the clock.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Command Post")
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Command Post")
	TSoftClassPtr<AActor> JammerClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Command Post")
	TSoftClassPtr<AActor> CommandPorchClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Command Post")
	TSoftClassPtr<AActor> RailGateClass;

	// Morse of each burst kind, played with the matching static (2D, the player's radio).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Sound")
	TMap<EDirectionFindingBurst, TSoftObjectPtr<USoundBase>> BurstSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Sound")
	TMap<EDirectionFindingBurst, TSoftObjectPtr<USoundBase>> StaticSounds;

	// Played when a station's wedge first appears.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Sound")
	TSoftObjectPtr<USoundBase> StationLockOnSound;

	// Intercepted voice per burst kind (Russian, English subtitles), played InterceptDelaySeconds after the Morse.
	// Routine lines are skipped while another portrait line plays; replies and attack orders queue behind it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Sound")
	TMap<EDirectionFindingBurst, FDirectionFindingIntercepts> InterceptLines;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Sound")
	ERTSPortraitTypes InterceptPortrait = ERTSPortraitTypes::SovietInterceptStarik;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Sound", meta = (ClampMin = "0.0"))
	float InterceptDelaySeconds = 1.2f;

	// The jam: a quiet noise bed for the whole mission until StopJammer. Empty plays nothing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Direction Finding|Sound")
	TSoftObjectPtr<USoundBase> JammerBedSound;

private:
	void BeginPlay_BindStations();
	void StartHunt(const ERTSGameDifficulty Difficulty);
	int32 PickCommandPost();
	void InitSolver(const int32 CommandPostIndex);
	void SpawnCommandPostTargets();
	AActor* SpawnHiddenTarget(const TSoftClassPtr<AActor>& TargetClass, const FTransform& Transform);
	void SetTargetsRevealed(const bool bRevealed) const;
	void StartJammerBed();
	bool GetIsValidJammerBedComponent() const;

	void OnStationOwnerChanged(ASupplyPoint* Station, uint8 PreviousOwner);
	void ApplyStationOwner(const int32 StationIndex);
	void CheckFirstOverlap();

	void TransmitBurst(const EDirectionFindingBurst BurstType);
	float GetBurstFactor(const EDirectionFindingBurst BurstType) const;
	void ScheduleRoutineBurst();
	void OnRoutineBurst();
	void OnReplyBurst();

	void CheckFix();
	void Fix();

	void PlaySoftSound2D(const TSoftObjectPtr<USoundBase>& Sound) const;

	/** Plays one of the burst kind's intercept lines on the intercept portrait, after InterceptDelaySeconds. */
	void ScheduleInterceptLine(const EDirectionFindingBurst BurstType);
	void PlayInterceptLine(const EDirectionFindingBurst BurstType);
	// Index of the line each burst kind played last, so the same line does not come twice in a row.
	TMap<EDirectionFindingBurst, int32> M_LastInterceptLine;
	float GetStartHalfAngle() const;
	double GetWorldSeconds() const;

	FDirectionFindingSolver M_Solver;
	// The decoy's bearings: the same stations, one "Werk" at the decoy's spot. Used only while bM_IsDecoyActive.
	FDirectionFindingSolver M_DecoySolver;
	bool bM_IsDecoyActive = false;

	/**
	 * @brief Adds the open wedges of one solver to the draw data and intersects its held ones.
	 * @param Solver The real or the decoy bearings.
	 * @param MapBounds The map's world square.
	 * @param NowSeconds World time, for the narrowing animation.
	 * @param OutWedges Receives one draw entry per open wedge.
	 * @param OutOverlap Receives the polygon inside every held wedge; empty with fewer than two.
	 */
	void AppendSolverDrawData(const FDirectionFindingSolver& Solver, const FBox2D& MapBounds, const double NowSeconds,
	                          TArray<FDirectionFindingWedgeDraw>& OutWedges, TArray<FVector2D>& OutOverlap) const;
	FRandomStream M_Random;
	ERTSGameDifficulty M_Difficulty = ERTSGameDifficulty::Normal;

	bool bM_HasStarted = false;
	bool bM_HasRaisedFirstOverlap = false;
	bool bM_IsFixed = false;

	// World seconds a site last caused a reply.
	TMap<FName, double> M_LastReplyBySite;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> M_CommandPostTargets;

	UPROPERTY()
	TObjectPtr<UAudioComponent> M_JammerBedComponent;

	FTimerHandle M_RoutineBurstTimerHandle;
};
