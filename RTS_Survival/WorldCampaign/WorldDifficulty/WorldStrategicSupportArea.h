// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "WorldStrategicSupportArea.generated.h"

class AActor;
class URTSRadiusPoolSubsystem;

/**
 * @brief Blueprint-friendly settings for one world-map strategic support radius.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldStrategicSupportAreaSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Strategic Support")
	EWorldStrategicSupport StrategicSupport = EWorldStrategicSupport::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Strategic Support",
		meta = (ClampMin = "0"))
	float XYRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Strategic Support")
	ERTSRadiusType RadiusType = ERTSRadiusType::FullCircle_DifficultyRadius;
};

/**
 * @brief Added to world map objects to apply enum-driven strategic support inside a radius.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UWorldStrategicSupportArea : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldStrategicSupportArea();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * @brief Gets the enum type used to resolve this area's strength reason from WorldData.
	 * @return Strategic support enum selected by the designer.
	 */
	EWorldStrategicSupport GetStrategicSupport() const { return M_Settings.StrategicSupport; }

	/**
	 * @brief Gets the XY radius used when applying this support area to enemy and mission targets.
	 * @return Radius in world units.
	 */
	float GetXYRadius() const { return M_Settings.XYRadius; }

	/**
	 * @brief Gets the visual radius style used by the radius pool subsystem.
	 * @return Radius visualization type.
	 */
	ERTSRadiusType GetRadiusType() const { return M_Settings.RadiusType; }

	/**
	 * @brief Gets the last difficulty-adjusted percentage resolved for this strategic support area.
	 * @return Cached strategic support percentage, or zero when unresolved or missing from WorldData.
	 */
	int32 GetCachedStrategicSupportPercentage() const { return M_CachedStrategicSupportPercentage; }

	/**
	 * @brief Stores the last difficulty-adjusted strategic support percentage resolved from WorldData.
	 * @param StrengthPercentage Percentage to cache for debugging/inspection.
	 */
	void SetCachedStrategicSupportPercentage(int32 StrengthPercentage);

	/**
	 * @brief Shows this component's hover radius if no radius is already visible.
	 */
	void ShowHoverRadius();

	/**
	 * @brief Hides this component's hover radius.
	 */
	void HideHoverRadius();

	/**
	 * @brief Shows this component's selected radius and clears its hover radius first.
	 */
	void ShowSelectedRadius();

	/**
	 * @brief Hides this component's selected radius.
	 */
	void HideSelectedRadius();

	/**
	 * @brief Hides all radius visuals owned by this component.
	 */
	void HideAllRadii();

	/**
	 * @brief Shows hover radii for every UWorldStrategicSupportArea on an actor.
	 * @param Actor Actor whose support area components should be shown.
	 */
	static void ShowHoverRadiiOnActor(AActor* Actor);

	/**
	 * @brief Hides hover radii for every UWorldStrategicSupportArea on an actor.
	 * @param Actor Actor whose support area components should be hidden.
	 */
	static void HideHoverRadiiOnActor(AActor* Actor);

	/**
	 * @brief Shows selected radii for every UWorldStrategicSupportArea on an actor.
	 * @param Actor Actor whose support area components should be shown.
	 */
	static void ShowSelectedRadiiOnActor(AActor* Actor);

	/**
	 * @brief Hides selected radii for every UWorldStrategicSupportArea on an actor.
	 * @param Actor Actor whose support area components should be hidden.
	 */
	static void HideSelectedRadiiOnActor(AActor* Actor);

private:
	static constexpr int32 InvalidRadiusId = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Strategic Support",
		meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FWorldStrategicSupportAreaSettings M_Settings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Strategic Support",
		meta = (AllowPrivateAccess = "true"))
	int32 M_CachedStrategicSupportPercentage = 0;

	UPROPERTY()
	TWeakObjectPtr<class URTSRadiusPoolSubsystem> M_RadiusPoolSubsystem;

	int32 M_HoverRadiusId = InvalidRadiusId;
	int32 M_SelectedRadiusId = InvalidRadiusId;

	bool GetIsValidRadiusPoolSubsystem();
	URTSRadiusPoolSubsystem* GetRadiusPoolSubsystem();
	int32 CreateRadius();
	bool GetIsRadiusActive(int32 RadiusId);
	void HideRadiusById(int32& RadiusId);
};
