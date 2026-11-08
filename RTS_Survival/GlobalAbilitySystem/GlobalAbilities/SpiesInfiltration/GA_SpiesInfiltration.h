// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "GA_SpiesInfiltration.generated.h"

USTRUCT(BlueprintType)
struct FSpiesInfiltrationSettings
{
	GENERATED_BODY()

	// How many hidden enemy units the spies reveal per use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 UnitsToReveal = 6;

	// Radius of the vision bubble that follows each revealed unit; picked units are at least this far apart.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100.0"))
	float RevealRadius = 600.0f;

	// How long the bubbles follow their units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float RevealHoldSeconds = 25.0f;
};

/**
 * @brief Instant intel: no aim. Picks random units of the opponent that its owner cannot see and attaches a vision
 * bubble to each, so they stay revealed while they move. Units revealed by the previous use go last in the draw, so
 * two uses in a row show different units when there are enough to choose from.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_SpiesInfiltration : public UGlobalAbility
{
	GENERATED_BODY()

public:
	// Executes at once through the click path, which pays and starts the cooldown.
	virtual void ActivateAbility() override;

	// The location is ignored: the AI and mission scripts only give a point.
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FSpiesInfiltrationSettings M_SpiesSettings;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> M_PreviouslyRevealed;

	// One actor per unit (one soldier per squad) of the opponent that the owner cannot see right now.
	TArray<AActor*> GetHiddenOpponentUnits() const;
	TArray<AActor*> PickUnitsToReveal(TArray<AActor*> Candidates) const;
	void AttachVisionBubble(AActor* Unit) const;
};
