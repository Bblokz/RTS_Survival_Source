// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/SupplyPoints/SupplyPointTypes.h"
#include "SupplyPoint.generated.h"

class ATankMaster;
enum class EMinimapIconType : uint8;
class USupplyTruckComponent;
class UMaterialInterface;
class USceneComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSupplyPointOwnerChanged, class ASupplyPoint* /*SupplyPoint*/,
                                     uint8 /*PreviousOwner*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSupplyPointSuppliesPreparing, float /*Duration*/);
DECLARE_MULTICAST_DELEGATE(FOnSupplyPointSuppliesReady);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSupplyPointTruckLoadingStarted, ATankMaster* /*SupplyTruck*/,
                                       float /*Duration*/, const FSupplyTruckCargo& /*Cargo*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSupplyPointTruckLoadingFinished, bool /*bCompleted*/);

/** Capture bookkeeping of one supply point: whose progress it is and how far the flag is up the pole. */
USTRUCT()
struct FSupplyPointCaptureState
{
	GENERATED_BODY()

	// Player holding the point; SupplyPointConstants::NeutralOwner when nobody does.
	uint8 OwningPlayer = 0;

	// Player the current progress (the flag on the pole) belongs to.
	uint8 ProgressPlayer = 0;

	// 0 = flag down, 1 = flag up and the point is held by ProgressPlayer.
	float Progress = 0.f;

	// Flag height the visuals have reached; follows Progress every frame while they differ.
	float ShownProgress = 0.f;

	// The player's running capture started under somebody else's flag, so its bar covers lowering and raising.
	bool bPlayerCaptureIncludesLowering = false;
};

/**
 * @brief Place on a map as a Blueprint child: units of one side standing inside the capture radius raise the flag
 * and take the point, after which the owner's base sends supply trucks that the crane loads in the truck bay.
 * Tune the instance through SupplyPointSettings; the Blueprint supplies the meshes and reacts to the BP events.
 * The flag is the component named FlagComponentName (or the one handed to InitSupplyPoint); add a
 * USupplyPointCraneComponent for the crane, stockpile and truck cargo visuals.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASupplyPoint : public AActor
{
	GENERATED_BODY()

public:
	ASupplyPoint();

	virtual void Tick(float DeltaSeconds) override;

	/** @param FlagComponent The flag (not the pole); it moves up from where the Blueprint placed it. */
	UFUNCTION(BlueprintCallable, Category = "Supply Point")
	void InitSupplyPoint(USceneComponent* FlagComponent);

	UFUNCTION(BlueprintPure, Category = "Supply Point")
	uint8 GetOwningPlayer() const { return M_CaptureState.OwningPlayer; }

	UFUNCTION(BlueprintPure, Category = "Supply Point")
	ESupplyPointState GetSupplyPointState() const { return M_SupplyPointState; }

	UFUNCTION(BlueprintPure, Category = "Supply Point")
	float GetCaptureProgress() const { return M_CaptureState.Progress; }

	const FSupplyPointSettings& GetSupplyPointSettings() const { return SupplyPointSettings; }

	bool GetIsCaptureProgressBarShown() const { return M_CaptureBarActivationId != INDEX_NONE; }

	/** Replaces the instance settings; timers that already run keep their old duration. */
	UFUNCTION(BlueprintCallable, Category = "Supply Point")
	void SetSupplyPointSettings(const FSupplyPointSettings& NewSettings) { SupplyPointSettings = NewSettings; }

	// Where the truck leaves the road network and lines up with the bay.
	FVector GetTruckApproachLocation() const;
	// Where the truck stands while the crane loads it, and the heading it must have there.
	FTransform GetTruckParkTransform() const;
	// Where the park order aims: past the park point by the distance vehicles stop short.
	FVector GetTruckParkOrderLocation() const;
	// Where the loaded truck drives to first, so it leaves the bay forwards.
	FVector GetTruckExitLocation() const;

	/**
	 * @brief The truck reports it stands in the bay; loading starts now or as soon as the supplies are ready.
	 * @param SupplyTruck The parked truck.
	 * @return False when the point does not belong to the truck's owner (anymore), so the truck must leave empty.
	 */
	bool OnSupplyTruckParked(USupplyTruckComponent* SupplyTruck);

	// A truck that dies or is recalled while parked frees the bay again.
	void OnSupplyTruckLeftBay(const USupplyTruckComponent* SupplyTruck);

	/** Hands the point to a player at once; used by scripts and tests instead of standing units in the radius. */
	UFUNCTION(BlueprintCallable, Category = "Supply Point")
	void ForceCaptureByPlayer(uint8 NewOwningPlayer);

	FOnSupplyPointOwnerChanged OnSupplyPointOwnerChanged;

	// The supply cycle for code listeners (the crane component); the Blueprint gets the BP_ events below.
	FOnSupplyPointSuppliesPreparing OnSuppliesPreparingNative;
	FOnSupplyPointSuppliesReady OnSuppliesReadyNative;
	FOnSupplyPointTruckLoadingStarted OnTruckLoadingStartedNative;
	FOnSupplyPointTruckLoadingFinished OnTruckLoadingFinishedNative;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Supply Point")
	FSupplyPointSettings SupplyPointSettings;

	// Flag material while the enemy (or nobody) holds the point, per enemy director of the map.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Point|Flag")
	TMap<EEnemyDirector, TObjectPtr<UMaterialInterface>> EnemyFlagMaterials;

	// Flag material while the player holds the point, per player faction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Point|Flag")
	TMap<ERTSFaction, TObjectPtr<UMaterialInterface>> PlayerFlagMaterials;

	// Name of the flag component (not the pole) on the Blueprint; found at BeginPlay unless InitSupplyPoint was
	// called with another component.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Point|Flag")
	FName FlagComponentName = TEXT("Flag");

	// Material slot of the flag mesh that carries the banner.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Supply Point|Flag", meta = (ClampMin = "0"))
	int32 FlagMaterialSlot = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Supply Point")
	TObjectPtr<USceneComponent> SupplyPointRoot;

	// Move in the Blueprint: where the truck lines up before it drives into the bay.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Supply Point")
	TObjectPtr<USceneComponent> TruckApproachPoint;

	// Move and rotate in the Blueprint: the truck stops here facing along this component's X axis.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Supply Point")
	TObjectPtr<USceneComponent> TruckParkPoint;

	// Move in the Blueprint: first waypoint of the loaded truck.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Supply Point")
	TObjectPtr<USceneComponent> TruckExitPoint;

	/** Called whenever the flag height changes. Progress 0 = down, 1 = raised; ProgressPlayer owns the flag. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Supply Point")
	void BP_OnCaptureProgressChanged(float Progress, uint8 ProgressPlayer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Supply Point")
	void BP_OnOwnerChanged(uint8 NewOwningPlayer, uint8 PreviousOwningPlayer);

	/** Supplies started being prepared; the stockpiles can fill over Duration seconds. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Supply Point")
	void BP_OnSuppliesPreparing(float Duration);

	UFUNCTION(BlueprintImplementableEvent, Category = "Supply Point")
	void BP_OnSuppliesReady();

	/** The crane cycle: load Cargo into the bed of SupplyTruck over Duration seconds (skip kinds that are zero). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Supply Point")
	void BP_OnTruckLoadingStarted(ATankMaster* SupplyTruck, float Duration, const FSupplyTruckCargo& Cargo);

	/** Loading ended; bCompleted is false when it was cut short (truck destroyed, point lost). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Supply Point")
	void BP_OnTruckLoadingFinished(bool bCompleted);

private:
	void BeginPlay_FindFlag();
	void BeginPlay_ApplyInitialOwner();
	void BeginPlay_StartCaptureSearch();
	void BeginPlay_AddMiniMapIcon();

	// ---- Capture ----
	void UpdateCapture();
	/**
	 * @brief Counts who stands in the capture radius.
	 * @param OutSinglePlayerPresent The only player with units in the radius; neutral when nobody is there.
	 * @return True when units of more than one player are inside, which stalls the capture.
	 */
	bool FindPlayersInCaptureRadius(uint8& OutSinglePlayerPresent) const;
	bool GetCanActorCapture(AActor* Actor, uint8& OutOwningPlayer) const;
	void AdvanceCapture(uint8 CapturingPlayer, float DeltaSeconds);
	void RelaxCapture(float DeltaSeconds);
	void SetOwningPlayer(uint8 NewOwningPlayer);
	void UpdateFlagVisuals(float DeltaSeconds);
	void ApplyShownFlagProgress();
	// The minimap shows the flag of the holder: the player's faction, else the map's enemy director.
	EMinimapIconType GetMiniMapIconOfHolder() const;
	void UpdateMiniMapIcon() const;
	// Gives the flag mesh the banner of the player whose flag is on the pole.
	void ApplyFlagFaction() const;

	// ---- Supply cycle ----
	void StartPreparingSupplies();
	void OnSuppliesReady();
	void StartLoadingTruck();
	void OnTruckLoadingFinished();
	void StopSupplyCycle();
	void SetSupplyPointState(ESupplyPointState NewState);

	// Shows the player how far their capture is; the bar runs by itself and is restarted whenever the capture
	// stalls and resumes (contested, units left the radius).
	void UpdateCaptureProgressBar(bool bIsPlayerCapturing);
	float GetPlayerCaptureSecondsLeft() const;
	void StopCaptureProgressBar();
	void StartProgressBar(float Duration, const FText& Description);
	void StopProgressBar();

	FSupplyPointCaptureState M_CaptureState;

	ESupplyPointState M_SupplyPointState = ESupplyPointState::Inactive;

	// The flag mesh handed over by the Blueprint; optional, capture works without it.
	UPROPERTY()
	TWeakObjectPtr<USceneComponent> M_FlagComponent;

	FVector M_FlagLoweredRelativeLocation = FVector::ZeroVector;

	// Truck standing in the bay (waiting or being loaded).
	UPROPERTY()
	TWeakObjectPtr<USupplyTruckComponent> M_ParkedTruck;

	FTimerHandle M_CaptureSearchTimerHandle;
	FTimerHandle M_SupplyTimerHandle;

	// Activation id of the pooled progress bar above the point; INDEX_NONE when none is shown.
	int32 M_ProgressBarActivationId = INDEX_NONE;

	// Activation id of the capture bar; INDEX_NONE while the player is not capturing.
	int32 M_CaptureBarActivationId = INDEX_NONE;

	FName M_MiniMapIconId = NAME_None;
};
