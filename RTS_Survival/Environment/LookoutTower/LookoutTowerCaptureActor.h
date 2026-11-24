// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CaptureMechanic/CaptureActor/CaptureActor.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"

#include "LookoutTowerCaptureActor.generated.h"

class AConvoy;
class AEnemyConvoyRoute;
class AGlobalAbilityVisionBubble;
class ALookoutTowerCaptureActor;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLookoutTowerCaptured, ALookoutTowerCaptureActor*, Tower,
                                             int32, Player);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnLookoutTowerConvoyWarning, ALookoutTowerCaptureActor*, Tower,
                                              int32, ConvoyIndex, EConvoyCargoType, CargoType,
                                              AEnemyConvoyRoute*, Route, AConvoy*, Convoy);

/** What a held lookout tower does for its holder. */
USTRUCT(BlueprintType)
struct FLookoutTowerSettings
{
	GENERATED_BODY()

	// Fog of war revealed around the tower for the capturing (human) player, for good, in cm.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout", meta = (ClampMin = "0.0"))
	float RevealRadius = 9000.f;

	// Socket on the tower mesh the reveal is centred on; the actor location when no mesh has it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout")
	FName RevealSocketName = TEXT("Reveal");

	// Capturing units walk to the tower mesh sockets whose name contains this (the hoist house door).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout")
	FName CaptureLocationSocketFilter = TEXT("entrance");

	// Routes whose convoys the held tower warns about; empty = every AEnemyConvoyRoute placed in the level.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout|Convoy Warning")
	TArray<FName> WatchedRouteIds;

	// On: with several towers held, only the first one taken broadcasts OnConvoyWarning (one line per convoy).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout|Convoy Warning")
	bool bOnlyFirstHeldTowerWarns = true;

	// Components with this tag stay hidden until the tower is held (the orange panel on the deck rail).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout|Look")
	FName HeldPanelTag = TEXT("LookoutHeldPanel");

	// Components with this tag turn about their local X while the tower is held (the sheave axle).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout|Look")
	FName SpinTag = TEXT("LookoutSpin");

	// 0 = the sheaves stay still.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout|Look", meta = (ClampMin = "0.0"))
	float SpinDegreesPerSecond = 12.f;
};

/**
 * @brief Place on a high point as a lookout tower (BP_OreLookoutTower, no graph needed). Infantry capture it as a
 * capture point; once a capture starts it cannot be captured again and the capturer keeps it. For a human holder it
 * reveals the fog around it and broadcasts OnConvoyWarning with the cargo whenever a watched route starts a convoy.
 * Missions find the towers with GetAllLookoutTowers and bind OnConvoyWarning / OnLookoutTowerCaptured.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ALookoutTowerCaptureActor : public ACaptureActor
{
	GENERATED_BODY()

public:
	ALookoutTowerCaptureActor(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	// ICaptureInterface
	// Once a capture started, more units than any squad has: capture orders are refused instead of costing units.
	virtual int32 GetCaptureUnitAmountNeeded() const override;
	virtual void OnCaptureByPlayer(const int32 Player) override;
	// ~ICaptureInterface

	/**
	 * @brief Makes the tower Player's: shows the panel, spins the sheaves, reveals the fog and starts the warnings.
	 * Called when the capture completes; a mission may call it to hand the tower over.
	 * @param Player The new holder; only a human holder gets the reveal and the warnings.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Lookout")
	void OnLookoutCaptured(const int32 Player);

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Lookout")
	bool GetIsHeld() const { return bM_IsHeld; }

	/** @return The player holding the tower, or INDEX_NONE while nobody has taken it. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Lookout")
	int32 GetHoldingPlayer() const { return M_HoldingPlayer; }

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Lookout", meta = (WorldContext = "WorldContextObject"))
	static TArray<ALookoutTowerCaptureActor*> GetAllLookoutTowers(const UObject* WorldContextObject);

	/** @return True when Player holds at least one lookout tower (the mission marks a convoy's cargo with it). */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Lookout", meta = (WorldContext = "WorldContextObject"))
	static bool GetIsAnyLookoutTowerHeld(const UObject* WorldContextObject, const int32 Player);

	// The tower was taken (fires once).
	UPROPERTY(BlueprintAssignable, Category = "Lookout")
	FOnLookoutTowerCaptured OnLookoutTowerCaptured;

	// A watched route started a convoy while a human holds the tower. ConvoyIndex counts the convoys every watched
	// route started since the level began, this one included (1 = the first convoy of the level).
	UPROPERTY(BlueprintAssignable, Category = "Lookout")
	FOnLookoutTowerConvoyWarning OnConvoyWarning;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lookout")
	FLookoutTowerSettings LookoutSettings;

private:
	void BeginPlay_CacheTaggedComponents();
	void BeginPlay_SetupTowerMesh();
	void BeginPlay_BindWatchedConvoyRoutes();

	UStaticMeshComponent* FindMeshWithSocketContaining(const FString& SocketNameFragment) const;
	void SetHeldPanelsVisible(const bool bVisible) const;
	void StartSpin();
	void StartFogReveal(const int32 Player);
	void UnbindConvoyRoutes();
	TArray<AEnemyConvoyRoute*> FindWatchedConvoyRoutes() const;
	[[nodiscard]] bool GetIsValidRevealBubble() const;

	UFUNCTION()
	void HandleConvoyStarted(AEnemyConvoyRoute* Route, AConvoy* Convoy);

	/** @return False when another tower of the same holder, watching Route too, was taken earlier and warns instead. */
	bool GetIsWarningTower(const AEnemyConvoyRoute* Route) const;
	bool GetIsWatchingRoute(const AEnemyConvoyRoute* Route) const;

	bool bM_IsHeld = false;
	// Set when a capture starts: ACaptureActor has no cancel path, so a started capture always completes.
	bool bM_IsCaptureLocked = false;
	int32 M_HoldingPlayer = INDEX_NONE;
	// World seconds when the tower was taken; with bOnlyFirstHeldTowerWarns the earliest held tower warns.
	double M_HeldSinceSeconds = 0.0;
	// Convoys started on the watched routes since BeginPlay (held or not), so the index matches the mission's count.
	int32 M_WatchedConvoyStartCount = 0;

	// Mirrors ACaptureActor's capture timer: the base only reports completion to Blueprint (BP_OnCapturedByPlayer).
	FTimerHandle M_LookoutCaptureTimerHandle;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> M_HeldPanelComponents;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> M_SpinComponents;

	// Permanent vision bubble spawned on capture; destroyed with the tower.
	UPROPERTY()
	TObjectPtr<AGlobalAbilityVisionBubble> M_RevealBubble = nullptr;

	UPROPERTY()
	TArray<TWeakObjectPtr<AEnemyConvoyRoute>> M_BoundRoutes;
};
