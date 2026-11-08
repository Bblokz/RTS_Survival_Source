// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"

#include "AbilityPreviewDirector.generated.h"

class AActor;
class UCameraComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UGlobalAbilitiesManager;
class UGlobalAbility;
class UGlobalAbilitySetDataAsset;
class UAbilityPreviewTargetsComponent;
class AGliderTankDelivery;
class ATankMaster;
struct FGlobalAbilityAimSettings;
class USceneComponent;

/**
 * @brief Plays a global ability in the hangar map's preview desert for the commander selection screen.
 *
 * A separate path from the in-mission ability flow: it copies the template from DA_GlobalAbilitySystem like the
 * mission manager does, but never touches a global ability panel, resources, requirements, cooldowns or aim. Its
 * own UGlobalAbilitiesManager (SetupForAbilityPreview) only places the aircraft around the preview area.
 *
 * Placed in RTS_FactionSelection on the preview desert (actor location = the ground point the abilities target; the
 * camera component frames it). AFactionPlayerController spawns one at the default location when the level has none.
 * Everything the ability spawns near the preview area is destroyed when the preview stops, so every preview starts
 * on an empty desert. Attack abilities get Soviet targets under their run (UAbilityPreviewTargetsComponent).
 */
UCLASS()
class RTS_SURVIVAL_API AAbilityPreviewDirector : public AActor
{
	GENERATED_BODY()

public:
	AAbilityPreviewDirector();

	/**
	 * @brief Copies the ability's template and executes it at the preview centre (strip abilities along a strip
	 * across the view) after a short delay, so the camera cut lands first.
	 * @param Ability The kit ability to show.
	 * @return False when the data asset has no template for it; nothing started then.
	 */
	bool StartPreview(const EGlobalAbility Ability);

	/** Stops timers and destroys what the preview spawned; safe to call when no preview runs. */
	void StopPreview();

	bool GetIsPreviewRunning() const { return bM_IsPreviewRunning; }

	/** True once the ability reported it finished (units landed, bombs down, area strafed). */
	UFUNCTION(BlueprintPure, Category = "Ability Preview")
	bool GetHasAbilityFinished() const { return bM_HasAbilityFinished; }

	UFUNCTION(BlueprintPure, Category = "Ability Preview")
	bool GetHasAbilityExecuted() const { return bM_HasExecuted; }

	UFUNCTION(BlueprintPure, Category = "Ability Preview")
	int32 GetTrackedSpawnCount() const { return M_SpawnedActors.Num(); }

	/** Seconds since StartPreview; 0 while no preview runs. */
	float GetSecondsSincePreviewStart() const;

	/** Seconds until the finished preview returns by itself; negative while the ability still runs. */
	float GetAutoReturnSecondsLeft() const;

	/** The ability finished and the linger time passed, or the safety timeout hit: the owner closes the preview. */
	FSimpleDelegate OnPreviewWantsToClose;

	/** Why the last preview asked to close; for logs and the test runner. */
	const FString& GetCloseReason() const { return M_CloseReason; }

	/** The Soviet units under the running preview's attack; the test reads what they took. */
	UAbilityPreviewTargetsComponent* GetPreviewTargets() const { return M_PreviewTargets; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// After the ability reports its end, the preview stays this long before it returns to the hangar by itself.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Timing", meta = (ClampMin = "0"))
	float M_SecondsAfterAbilityFinished = 6.f;

	// Abilities that never report an end (instant ones such as the spies) count as finished after this long.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Timing", meta = (ClampMin = "0"))
	float M_SecondsUntilInstantAbilityFinished = 4.f;

	// Safety net: a preview never runs longer than this, whatever the ability does.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Timing", meta = (ClampMin = "5"))
	float M_MaxPreviewSeconds = 90.f;

	// The camera cuts first; the ability starts this much later.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Timing", meta = (ClampMin = "0"))
	float M_ExecuteDelaySeconds = 0.35f;

	// Horizontal direction from the centre towards where the aircraft come in (actor space; X looks into the view).
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Aircraft")
	FVector M_AircraftApproachDirection = FVector(-0.35f, 1.f, 0.f);

	// Distance from the centre to the aircraft spawn line; also the "map" half size straight flights walk back over.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Aircraft", meta = (ClampMin = "1000"))
	float M_AircraftSpawnDistance = 16000.f;

	// Aircraft spawn at most this far to either side of the approach line (missions spread them over half the map), so
	// every run crosses the view along the line the targets stand on; at 16 km out it is under 5 degrees.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Aircraft", meta = (ClampMin = "0"))
	float M_AircraftSpawnLineHalfLength = 1200.f;

	// Low enough that landing transports descend within their landing sound (Cue_AircraftLanded is 7 s), high enough
	// to clear the mesas and the ridge.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Aircraft", meta = (ClampMin = "500"))
	float M_AircraftHeightAboveGround = 2500.f;

	// Strafing abilities circle the target for minutes in a mission; the preview's copy strafes at most this long, then
	// the aircraft leave and the ability ends as usual. The templates are not touched.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Aircraft", meta = (ClampMin = "5"))
	float M_MaxPreviewStrafeSeconds = 25.f;

	// Strafing targets stand from this share of a run before the target to the share below along the first run: every
	// run fires from the target outwards, so units near it are hit by most runs.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Targets", meta = (ClampMin = "0", ClampMax = "1"))
	float M_StrafeTargetsBeforeStart = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Ability Preview|Targets", meta = (ClampMin = "0.1", ClampMax = "1"))
	float M_StrafeTargetsAlongRun = 0.55f;

	// Glider drops: once the delivered tank is this far from the landed glider (or after the timeout below) the glider
	// is hidden and the camera pans to where the tank stops; the return countdown starts after the pan.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Glider", meta = (ClampMin = "0"))
	float M_GliderHideTankDistance = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Ability Preview|Glider", meta = (ClampMin = "0"))
	float M_GliderHideTimeoutSeconds = 6.f;

	UPROPERTY(EditAnywhere, Category = "Ability Preview|Glider", meta = (ClampMin = "0.1"))
	float M_GliderCameraPanSeconds = 2.f;

	// Longest strip a strip ability gets; the view is about 70 m wide at the centre.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Aim", meta = (ClampMin = "500"))
	float M_MaxStripLength = 5200.f;

	// Spawned actors within this distance of the centre (and every aircraft) are destroyed when the preview stops.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Cleanup", meta = (ClampMin = "1000"))
	float M_CleanupRadius = 60000.f;

	// Off: the camera looks down and shows no sky, and a runtime sky atmosphere counts as unbuilt lighting next to the
	// hangar's stationary sky light ("LIGHTING NEEDS TO BE REBUILT" in development builds). On spawns one for the
	// preview's duration, for a framing that shows the sky.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Look")
	bool bM_SpawnSkyAtmosphere = false;

	// Height fog density while the preview runs (the hangar's own value comes back afterwards); < 0 leaves it.
	UPROPERTY(EditAnywhere, Category = "Ability Preview|Look")
	float M_PreviewFogDensity = 0.006f;

	UPROPERTY(EditAnywhere, Category = "Ability Preview|Look")
	FLinearColor M_PreviewFogInscatteringColor = FLinearColor(0.62f, 0.52f, 0.40f, 1.f);

private:
	UPROPERTY(VisibleAnywhere, Category = "Ability Preview")
	TObjectPtr<USceneComponent> M_PreviewCentre;

	UPROPERTY(VisibleAnywhere, Category = "Ability Preview")
	TObjectPtr<UCameraComponent> M_PreviewCamera;

	// Owns no panel or resources: only places aircraft (UGlobalAbilitiesManager::SetupForAbilityPreview).
	UPROPERTY(VisibleAnywhere, Category = "Ability Preview")
	TObjectPtr<UGlobalAbilitiesManager> M_PreviewAbilityManager;

	UPROPERTY(VisibleAnywhere, Category = "Ability Preview")
	TObjectPtr<UAbilityPreviewTargetsComponent> M_PreviewTargets;

	// Where the ability executes, worked out at StartPreview: strip abilities along the strip, carpet bombing from half
	// a carpet before the centre, so the carpet crosses the middle of the view, everything else at the centre.
	FVector M_ExecuteLocation = FVector::ZeroVector;
	FVector M_StripStart = FVector::ZeroVector;
	FVector M_StripEnd = FVector::ZeroVector;
	bool bM_ExecutesAlongStrip = false;

	UPROPERTY()
	TObjectPtr<UGlobalAbilitySetDataAsset> M_AbilityDataAsset;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_PreviewAbility;

	// Abilities of stopped previews stay referenced until this actor goes: some keep timers that capture them raw.
	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> M_RetiredAbilities;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedActors;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_SpawnedSky;

	UPROPERTY()
	TWeakObjectPtr<UDirectionalLightComponent> M_SunLight;

	UPROPERTY()
	TWeakObjectPtr<UExponentialHeightFogComponent> M_HeightFog;

	bool bM_SunWasAtmosphereLight = false;
	float M_HangarFogDensity = 0.f;
	FLinearColor M_HangarFogInscatteringColor = FLinearColor::Black;

	FDelegateHandle M_ActorSpawnedHandle;
	FTimerHandle M_ExecuteTimerHandle;
	FTimerHandle M_LingerTimerHandle;
	FTimerHandle M_MaxDurationTimerHandle;
	// Runs while a nuke or V2 warhead the ability launched is still coming down (the ability ended at launch).
	FTimerHandle M_WarheadWaitTimerHandle;

	// The glider drop's ending: wait for the tank to leave the hull, hide the glider, pan the camera to the tank.
	enum class EGliderFinalePhase : uint8
	{
		None,
		WaitingForTankOut,
		Panning
	};

	struct FGliderFinaleState
	{
		EGliderFinalePhase Phase = EGliderFinalePhase::None;
		TWeakObjectPtr<AGliderTankDelivery> Delivery;
		TWeakObjectPtr<ATankMaster> Tank;
		double PhaseStartSeconds = 0.0;
		FVector PanStartRelative = FVector::ZeroVector;
		FVector PanTargetRelative = FVector::ZeroVector;
	};

	FGliderFinaleState M_GliderFinale;
	FTimerHandle M_GliderFinaleTimerHandle;

	EGlobalAbility M_Ability = EGlobalAbility::GA_None;
	bool bM_IsPreviewRunning = false;
	// StartPreview pushed an RTS modal dialog suppression that StopPreview pops.
	bool bM_IsPreviewRunning_SuppressingDialogs = false;
	bool bM_HasExecuted = false;
	bool bM_HasAbilityFinished = false;
	double M_PreviewStartTime = 0.0;
	FString M_CloseReason;

	bool LoadAbilityDataAsset();
	void ShortenForPreview(UGlobalAbility& AbilityCopy) const;

	/**
	 * @brief Works out where the ability executes and the ground line its attack covers.
	 * @param AbilityCopy The preview's copy of the template.
	 * @param GroundCentre Ground point under the director.
	 * @param ApproachDirection Horizontal world direction the aircraft come in from.
	 * @param OutAttackStart Start of the attacked line on the ground.
	 * @param OutAttackEnd End of the attacked line.
	 * @return False when the ability attacks no line (drops, barrages, instant abilities).
	 */
	bool PlanPreviewAttack(UGlobalAbility& AbilityCopy, const FVector& GroundCentre, const FVector& ApproachDirection,
	                       FVector& OutAttackStart, FVector& OutAttackEnd);
	float GetPreviewStripLength(const FGlobalAbilityAimSettings& AimSettings) const;
	void SpawnPreviewTargets(UGlobalAbility& AbilityCopy, const FVector& GroundCentre, const FVector& ApproachDirection);
	void ExecutePreviewAbility();
	FVector GetGroundPoint(const FVector& Location) const;
	void GetStripAcrossView(const FVector& GroundCentre, float StripLength, FVector& OutStart, FVector& OutEnd) const;

	void HandleAbilityEnded(UGlobalAbility* EndedAbility);
	bool GetHasWarheadInFlight() const;
	void CheckWarheadsLanded();

	/** The landed glider of this preview whose tank drove out; null for every other ability. */
	AGliderTankDelivery* FindDeliveredGlider() const;
	void StartGliderFinale(AGliderTankDelivery& Delivery);
	void TickGliderFinale();
	void HideGliderAndStartPan(AGliderTankDelivery* Delivery);
	// Camera relative location that keeps the framing but slides sideways until Focus is in the middle of the view.
	FVector GetCameraRelativeLocationFocusing(const FVector& Focus) const;
	void ResetGliderFinale();
	void MarkAbilityFinished(const TCHAR* Reason);
	void HandleLingerFinished();
	void HandleMaxDurationReached();
	void RequestClose(const FString& Reason);

	void BeginTrackingSpawns();
	void HandleActorSpawned(AActor* SpawnedActor);
	bool GetIsCleanupCandidateClass(const AActor* Actor) const;
	bool GetShouldDestroyOnCleanup(const AActor* Actor) const;
	void DestroyPreviewSpawns();
	void RetireAbility();

	void ApplyPreviewLook();
	void RestoreHangarLook();
	void ClearPreviewTimers();
};
