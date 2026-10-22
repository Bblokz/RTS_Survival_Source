// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "UnitVariantSelectionCaptureSubsystem.generated.h"

class AFactionPlayerController;
class UW_UnitVariantSelectionMenu;

UENUM()
enum class EUnitVariantSelectionCapturePhase : uint8
{
	Inactive,
	WaitingForFactionMenu,
	WaitingForVariantMenu,
	SettlingOnBase,
	// The screenshot is written at the end of a later frame; clicking first would photograph the next state.
	WaitingForBaseCapture,
	SettlingOnVariant,
	WaitingForLastCapture
};

/**
 * @brief Offscreen capture of the unit variant selection on RTS_FactionSelection, started with
 * -UnitVariantSelectionCapture (optional -UnitVariantSelectionFaction=<ERTSFaction value>). It allows the selection
 * in the game instance, launches the faction, screenshots the screen on Base and after choosing Variant A, and checks
 * that the hangar spawned the variant class. Never confirms, so the player profile is not written. Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UUnitVariantSelectionCaptureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EUnitVariantSelectionCapturePhase M_Phase = EUnitVariantSelectionCapturePhase::Inactive;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;

	UPROPERTY()
	TWeakObjectPtr<UW_UnitVariantSelectionMenu> M_Menu;

	void EnterPhase(const EUnitVariantSelectionCapturePhase NewPhase);
	float GetPhaseSeconds() const;
	void Check(const bool bCondition, const FString& Description, const FString& Details = FString());
	void Capture(const FString& CaptureName) const;
	void Finish();

	void TickWaitingForFactionMenu();
	void TickWaitingForVariantMenu();
	void TickSettlingOnBase();
	void TickWaitingForBaseCapture();
	void TickSettlingOnVariant();

	// Checks that an actor of the class the data asset lists for the variant stands in the hangar.
	void CheckPreviewClass(const bool bExpectVariant);

	AFactionPlayerController* GetController() const;
};
