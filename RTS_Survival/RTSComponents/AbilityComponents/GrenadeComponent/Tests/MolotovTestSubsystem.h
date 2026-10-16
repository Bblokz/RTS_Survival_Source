// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MolotovTestSubsystem.generated.h"

class ACPPController;
class AGrenadeActor;
class ASceneCapture2D;
class ASquadController;
class ASquadUnit;
class UNiagaraComponent;
class UNiagaraSystem;
class UTextureRenderTarget2D;

UENUM()
enum class EMolotovTestPhase : uint8
{
	Inactive,
	WaitingForGameStart,
	WaitingForSquads,
	WaitingForImpact,
	Burning,
	Done
};

/** A scene capture the test aims every frame and saves pictures from. */
USTRUCT()
struct FMolotovTestCamera
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ASceneCapture2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;
};

/**
 * @brief Plays one molotov throw of an enemy-owned Soviet Mosin squad onto a player squad and checks the ability:
 * the burning rag in hand and in flight, the impact effect, burn damage for the burn duration and the effect ending.
 * Started with -MolotovValidate (pictures with -MolotovDir=<folder>); logs RTS_MOLOTOV_TEST. Not in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UMolotovTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// The game boots paused behind the start widget; the test has to tick to start it.
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EMolotovTestPhase M_Phase = EMolotovTestPhase::Inactive;
	float M_PhaseStartGameSeconds = 0.f;
	double M_PhaseStartRealSeconds = 0.0;
	int32 M_FailedCheckCount = 0;
	bool bM_IsSuppressingErrorDialogs = false;
	FString M_OutputDirectory;
	FVector M_TestLocation = FVector::ZeroVector;
	FVector M_ImpactLocation = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_MolotovSquad;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_TargetSquad;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_RagFlameSystem = nullptr;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> M_ImpactSystem = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UNiagaraComponent> M_ImpactComponent;

	// Every molotov impact effect seen, logged once each.
	UPROPERTY()
	TArray<TWeakObjectPtr<UNiagaraComponent>> M_LoggedImpacts;

	// Grenades seen in this test's thrower hands; AI squads elsewhere on the map may throw molotovs as well.
	UPROPERTY()
	TArray<TWeakObjectPtr<AGrenadeActor>> M_OwnGrenades;

	UPROPERTY()
	FMolotovTestCamera M_CloseCamera;

	UPROPERTY()
	FMolotovTestCamera M_GameCamera;

	// What the throw showed so far; checked when the burn is over.
	bool bM_HasOrderedThrow = false;
	bool bM_SawRagFlameInHand = false;
	bool bM_SawRagFlameInFlight = false;
	int32 M_HoldFramesSeen = 0;
	// Game time both squads first stood with soldiers; -1 until then.
	float M_SquadsReadyGameSeconds = -1.f;
	bool bM_HasSavedFlightPicture = false;
	// Target squad health once the impact damage is through, and at the end of the burn.
	float M_TargetHealthAfterImpact = -1.f;
	int32 M_NextBurnPictureIndex = 0;

	void EnterPhase(EMolotovTestPhase NewPhase);
	float GetGameSecondsInPhase() const;
	double GetRealSecondsInPhase() const;
	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void Finish();


	/**
	 * @brief Spawns a squad with its owner set before BeginPlay, so it registers and fights as that player.
	 * @param ClassPath Squad Blueprint class path.
	 * @param Location Where the squad spawns, projected onto the navmesh.
	 * @param OwningPlayer Player the squad belongs to.
	 * @return The squad, or null when it could not be spawned.
	 */
	ASquadController* SpawnSquadForPlayer(const TCHAR* ClassPath, const FVector& Location, uint8 OwningPlayer) const;

	bool CreateCameras();
	bool CreateCamera(FMolotovTestCamera& OutCamera, float FieldOfView) const;
	void AimCloseCameraAt(const FVector& LookAt) const;
	void AimGameCameraAt(const FVector& LookAt) const;
	void SaveCameraPicture(const FMolotovTestCamera& Camera, const FString& Label) const;

	void TickWaitingForGameStart();
	void TickWaitingForSquads();
	void TickWaitingForImpact();
	void TickBurning();

	bool GetIsMolotovOnSquadCard() const;
	void OrderThrow();
	void WatchThrownMolotovs();
	UNiagaraComponent* FindActiveImpactComponent() const;
	bool GetIsSoldierOfMolotovSquad(ASquadUnit* Soldier) const;
	void LogNewImpacts();
	FVector GetTargetSquadCentre() const;
	float GetTargetSquadHealth() const;
	void LogTargetSoldiers(const TCHAR* Moment) const;

	// The thrower's squad keeps shooting after the throw; with its guns off only the fire hurts the target.
	void SilenceMolotovSquad() const;

	static UNiagaraComponent* FindAttachedRagFlame(const AGrenadeActor& Grenade, const UNiagaraSystem* RagFlameSystem);
};
