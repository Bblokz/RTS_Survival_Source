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
	WaitingForLastCapture,
	// -UnitVariantSelectionEachUnit only: selects every unit in turn and photographs its cards.
	SettlingOnEachUnit,
	WaitingForEachUnitCapture,
	// After the last unit: arrow key + Enter equip a variant, Escape opens the discard dialog (photographed), a
	// second Escape closes it again.
	SettlingOnDiscardDialog,
	WaitingForDiscardDialogCapture
};

/**
 * @brief Offscreen capture of the unit variant selection on RTS_FactionSelection, started with
 * -UnitVariantSelectionCapture (optional -UnitVariantSelectionFaction=<ERTSFaction value>). It allows the selection
 * in the game instance, launches the faction, screenshots the screen on Base and after choosing Variant A, and checks
 * that the hangar spawned the variant class; -UnitVariantSelectionEachUnit also photographs the cards of every unit.
 * Never confirms, so the player profile is not written. Not in shipping.
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
	int32 M_EachUnitIndex = 0;
	bool bM_EachUnitScrolledToEnd = false;

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
	void TickWaitingForLastCapture();
	void TickSettlingOnEachUnit();
	void TickWaitingForEachUnitCapture();
	void SelectEachUnitOrFinish();
	void StartKeyboardCheck();
	void TickSettlingOnDiscardDialog();
	void TickWaitingForDiscardDialogCapture();

	// The command card of every unit in the hangar, to compare with the abilities the variant screen resolved.
	// @return How many commandable units stand in the hangar.
	int32 LogPreviewAbilities() const;

	// Checks that exactly one unit stands in the hangar: Variant A, or else the variant the profile saved for it.
	void CheckPreviewClass(const bool bExpectVariant);

	AFactionPlayerController* GetController() const;
};
