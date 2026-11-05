// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "GA_CompositeDrop.generated.h"

class UGA_CompositeDrop;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCompositeDropChildStarted, UGA_CompositeDrop*, int32, UGlobalAbility*);

/** @brief How a child ability is aimed from the parent's aim. */
UENUM(BlueprintType)
enum class ECompositeDropChildAim : uint8
{
	// The child gets a point: the parent's target plus the offset (a strip-aimed parent gives the middle of its strip).
	Point,
	// The child gets a strip. A strip-aimed parent passes its own strip, shifted by the offset. A point-aimed parent
	// gives a strip of the child's MaxStripLength that ends at the target, flown in from the friendly map edge.
	StripEndingAtTarget,
	// As StripEndingAtTarget, but a point-aimed parent's synthesised strip is centred on the target.
	StripCentredOnTarget
};

/** @brief One aircraft group of a composite ability: its own ability template, when it starts and where it aims. */
USTRUCT(BlueprintType)
struct FCompositeDropChild
{
	GENERATED_BODY()

	// Any global ability (strafing, aircraft drop, paratroopers, ...). Only its execution is used: the parent owns
	// tier, cost, cooldown, requirements, UI and aim. Leave the child's marker and sounds empty, the parent has them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TObjectPtr<UGlobalAbility> Ability = nullptr;

	// Seconds after the parent executes before this child executes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float StartDelaySeconds = 0.f;

	// World-space offset from the parent's target in cm; a moved target is put back on the landscape and navmesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D TargetOffset = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECompositeDropChildAim AimMode = ECompositeDropChildAim::Point;
};

USTRUCT(BlueprintType)
struct FCompositeDropSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FCompositeDropChild> Children;
};

/** @brief One execution of the composite; executions can overlap (enemy AI, missions), each has its own children. */
USTRUCT()
struct FCompositeDropRun
{
	GENERATED_BODY()

	int32 RunId = INDEX_NONE;
	FVector Target = FVector::ZeroVector;
	bool bM_IsStrip = false;
	FVector LineStart = FVector::ZeroVector;
	FVector LineEnd = FVector::ZeroVector;
	// Flight direction used for synthesised strips of a point-aimed parent.
	FVector ApproachDirection = FVector::ForwardVector;

	// Copies of the child templates, index-aligned with the settings; null for an empty entry.
	UPROPERTY()
	TArray<TObjectPtr<UGlobalAbility>> Children;

	TArray<bool> ChildHasEnded;
	int32 PendingChildCount = 0;
	// True while the run is being started: a child that ends at once must not finish a half-started run.
	bool bM_IsStarting = false;
};

/**
 * @brief Designer ability that fires several global abilities, each with its own aircraft and delivery type, at
 * one target: e.g. dive bombers strike and a transport paradrops squads a few seconds later.
 * The parent is the only one in the panel: it is aimed, paid and put on cooldown once. Each execution copies the
 * child templates, initialises them with the parent's owner, manager and controller, and executes them after their
 * delays straight through ExecuteAbilityAtLocation / ExecuteAbilityAlongLine, so children never pay or cool down.
 * OnGlobalAbilityEnded of the parent is broadcast one frame after the last child of the last running execution ended.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_CompositeDrop : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd) override;
	virtual void GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
	                                TArray<FVector>& OutDropPoints) const override;

	const TArray<FCompositeDropChild>& GetChildEntries() const { return M_CompositeDropSettings.Children; }

	/** @return The executing child copies of every running execution. */
	TArray<UGlobalAbility*> GetRunningChildren() const;

	/** Broadcast right before a child executes, so listeners can bind to its OnGlobalAbilityEnded in time. */
	FOnCompositeDropChildStarted OnChildStarted;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FCompositeDropSettings M_CompositeDropSettings;

	UPROPERTY(Transient)
	TArray<FCompositeDropRun> M_Runs;

	UPROPERTY(Transient)
	int32 M_NextRunId = 0;

	FTimerHandle M_EndedBroadcastHandle;

	/**
	 * @brief Copies and initialises the children, then starts each one now or after its delay.
	 * @param Target Ground point of the parent's aim (the middle of the strip for a strip aim).
	 * @param bIsStrip Whether the parent was aimed as a strip.
	 * @param LineStart Strip start; unused for a point aim.
	 * @param LineEnd Strip end; unused for a point aim.
	 */
	void StartRun(const FVector& Target, bool bIsStrip, const FVector& LineStart, const FVector& LineEnd);
	FCompositeDropRun* FindRun(int32 RunId);
	UGlobalAbility* CreateChildCopy(const UGlobalAbility* ChildTemplate);
	FVector GetApproachDirection(const FVector& Target) const;

	void StartChild(int32 RunId, int32 ChildIndex);
	void ExecuteChild(UGlobalAbility& Child, const FCompositeDropChild& Entry, const FCompositeDropRun& Run) const;
	/** @return The target moved by the offset, back on the landscape and navmesh. */
	FVector GetOffsetTarget(const FVector& Target, const FVector2D& Offset) const;
	void GetChildStrip(const FCompositeDropChild& Entry, const UGlobalAbility& Child, const FCompositeDropRun& Run,
	                   FVector& OutLineStart, FVector& OutLineEnd) const;

	void OnChildEnded(UGlobalAbility* Child);
	void FinishRunIfDone(int32 RunId);
	void QueueEndedBroadcast();
};
