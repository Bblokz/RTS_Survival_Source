// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldDivisionInfluenceComponent.generated.h"

class AActor;
class URTSRadiusPoolSubsystem;

/**
 * @brief Added to world division actors to show their influence radius during hover or selection.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWorldDivisionInfluenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldDivisionInfluenceComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * @brief Shows this division's hover radius unless a selected or hover radius is already active.
	 */
	void ShowHoverRadius();

	/**
	 * @brief Hides this division's hover radius while leaving selected radius state alone.
	 */
	void HideHoverRadius();

	/**
	 * @brief Shows this division's selected radius and clears transient hover display first.
	 */
	void ShowSelectedRadius();

	/**
	 * @brief Hides this division's selected radius.
	 */
	void HideSelectedRadius();

	/**
	 * @brief Hides all radius actors owned by this component.
	 */
	void HideAllRadii();

	/**
	 * @brief Shows hover radii for every division influence component on an actor.
	 * @param Actor Actor that may own one or more division influence components.
	 */
	static void ShowHoverRadiiOnActor(AActor* Actor);

	/**
	 * @brief Hides hover radii for every division influence component on an actor.
	 * @param Actor Actor that may own one or more division influence components.
	 */
	static void HideHoverRadiiOnActor(AActor* Actor);

	/**
	 * @brief Shows selected radii for every division influence component on an actor.
	 * @param Actor Actor that may own one or more division influence components.
	 */
	static void ShowSelectedRadiiOnActor(AActor* Actor);

	/**
	 * @brief Hides selected radii for every division influence component on an actor.
	 * @param Actor Actor that may own one or more division influence components.
	 */
	static void HideSelectedRadiiOnActor(AActor* Actor);

private:
	static constexpr int32 InvalidRadiusId = -1;

	UPROPERTY()
	TWeakObjectPtr<URTSRadiusPoolSubsystem> M_RadiusPoolSubsystem;

	int32 M_HoverRadiusId = InvalidRadiusId;
	int32 M_SelectedRadiusId = InvalidRadiusId;

	/**
	 * @brief Lazily finds the radius pool subsystem used for hover and selected visuals.
	 * @return true when the subsystem can be used.
	 */
	bool GetIsValidRadiusPoolSubsystem();

	/**
	 * @brief Gets the cached radius pool subsystem after validation.
	 * @return Radius pool subsystem, or nullptr when unavailable.
	 */
	URTSRadiusPoolSubsystem* GetRadiusPoolSubsystem();

	/**
	 * @brief Creates and attaches a radius actor using the owning division's radius settings.
	 * @return Active radius pool id, or InvalidRadiusId if creation failed.
	 */
	int32 CreateRadius();

	/**
	 * @brief Checks whether a cached radius pool id is still active.
	 * @param RadiusId Radius pool id to inspect.
	 * @return true when the radius subsystem still owns an active actor for the id.
	 */
	bool GetIsRadiusActive(int32 RadiusId);

	/**
	 * @brief Hides a radius pool actor and resets the cached id.
	 * @param RadiusId Cached id passed by reference so it can be invalidated.
	 */
	void HideRadiusById(int32& RadiusId);
};
