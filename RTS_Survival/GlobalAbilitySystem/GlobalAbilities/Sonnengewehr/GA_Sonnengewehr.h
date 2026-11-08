// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SonnengewehrBeam.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "GA_Sonnengewehr.generated.h"

struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSonnengewehrBeamSpawned, UGlobalAbility*, ASonnengewehrBeam*);

/**
 * @brief The Light Bringer's "sun gun": an orbital mirror burns the strip the player draws. After a warning everyone
 * can see, a blinding beam comes down at the start of the strip and sweeps to its end, burning every unit and
 * building it touches with laser damage. The aim area's AreaRadius is half the beam width and MaxStripLength the
 * longest sweep. Enemy AI and missions give a point: the strip is then centred on it at the longest length.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_Sonnengewehr : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd) override;
	virtual void BeginDestroy() override;

	const FSonnengewehrSettings& GetSonnengewehrSettings() const { return M_SonnengewehrSettings; }

	// Lets tests and missions follow the strike the beam actor runs.
	FOnSonnengewehrBeamSpawned OnBeamSpawned;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FSonnengewehrSettings M_SonnengewehrSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FSonnengewehrVisuals M_SonnengewehrVisuals;

	// Keeps the async asset loads alive until their callbacks can spawn the beam.
	TArray<TSharedPtr<FStreamableHandle>> M_LoadHandles;

	// Missions can fire the ability again before an earlier beam is done: it ends when the last sweep is over.
	UPROPERTY(Transient)
	int32 M_StrikesInFlight = 0;

	void ClampStripToAim(FVector& InOutLineStart, FVector& InOutLineEnd) const;
	FVector GetDirectionForPointExecution(const FVector& TargetLocation) const;
	void OnAssetsLoaded(const FVector& StripStart, const FVector& StripEnd);
	void OnBeamSweepFinished(ASonnengewehrBeam* Beam);
	void OnStrikeOver();
	void GiveOwnerVisionOfStrip(const FVector& StripStart, const FVector& StripEnd) const;
	TArray<FSoftObjectPath> GetAssetsToLoad() const;
};
