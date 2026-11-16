// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "MegaphoneBroadcastComponent.generated.h"

class UAudioComponent;
class UMegaphoneLineSet;
class USoundAttenuation;
struct FMegaphoneLine;

/** Which line played last, so the next pick can skip it. */
USTRUCT()
struct FMegaphoneLinePick
{
	GENERATED_BODY()

	int32 SetIndex = INDEX_NONE;
	int32 LineIndex = INDEX_NONE;

	bool operator==(const FMegaphoneLinePick& Other) const
	{
		return SetIndex == Other.SetIndex && LineIndex == Other.LineIndex;
	}
};

/**
 * @brief Add to a loudspeaker actor (the Rus megaphone BXPs) to have it broadcast random lines from its LineSets,
 * with a pause between them, from BeginPlay until the owner dies or StopBroadcast is called.
 * The sound plays from this component's location, so place the component at the horn. Give a placed megaphone its
 * own LineSets in the Details panel to mix generic and level-specific lines; an empty LineSets stays silent.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UMegaphoneBroadcastComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UMegaphoneBroadcastComponent();

	/** Starts (or restarts) the broadcast loop; the first line plays after FirstLineDelayRange. */
	UFUNCTION(BlueprintCallable, Category = "Megaphone")
	void StartBroadcast();

	/** Stops the loop and cuts the line that is playing, for example when the megaphone switches to its siren. */
	UFUNCTION(BlueprintCallable, Category = "Megaphone")
	void StopBroadcast();

	/** Replaces the line pool at runtime; a running broadcast picks its next line from the new sets. */
	UFUNCTION(BlueprintCallable, Category = "Megaphone")
	void SetLineSets(const TArray<UMegaphoneLineSet*>& NewLineSets);

	UFUNCTION(BlueprintPure, Category = "Megaphone")
	bool GetIsBroadcasting() const { return bM_IsBroadcasting; }

	/** The lines this megaphone picks from. Set per placed megaphone to combine generic and level-specific sets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone")
	TArray<TObjectPtr<UMegaphoneLineSet>> LineSets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone")
	bool bStartOnBeginPlay = true;

	/** Seconds before the first line, picked between X and Y, so megaphones placed together do not start in unison. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone")
	FVector2D FirstLineDelayRange = FVector2D(2.0, 8.0);

	/** Silence between the end of one line and the start of the next, in seconds, picked between X and Y. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone")
	FVector2D PauseBetweenLinesRange = FVector2D(8.0, 16.0);

	/** How far the broadcast carries; the lines already have the horn and the yard echo baked in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone")
	TObjectPtr<USoundAttenuation> Attenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float VolumeMultiplier = 1.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BeginPlay_BindToOwnerDeath();
	void UnbindFromOwnerDeath();
	void OnOwnerDies();

	void ScheduleNextLine(const float DelaySeconds);
	void PlayNextLine();
	void StopCurrentLine();

	/** @return False while the owning building expansion is being placed, built, packed or unpacked. */
	bool GetCanBroadcastNow() const;

	/**
	 * @brief Weighted random pick over every line of every set; skips the line that played last when there is
	 * another one, so the same broadcast never repeats back to back.
	 * @param OutPick The set and line index of the picked line.
	 * @return False when no set holds a playable line.
	 */
	bool PickNextLine(FMegaphoneLinePick& OutPick) const;

	const FMegaphoneLine* GetLine(const FMegaphoneLinePick& Pick) const;

	static float RandomInRange(const FVector2D& Range);

	FTimerHandle M_NextLineTimer;

	UPROPERTY()
	TWeakObjectPtr<UAudioComponent> M_CurrentLineAudio;

	FMegaphoneLinePick M_LastPick;

	bool bM_IsBroadcasting = false;

	// Set once the owner dies; StartBroadcast is ignored afterwards so a dead megaphone stays silent.
	bool bM_OwnerDied = false;
};
