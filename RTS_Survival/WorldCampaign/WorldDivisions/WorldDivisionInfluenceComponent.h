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

	void ShowHoverRadius();
	void HideHoverRadius();
	void ShowSelectedRadius();
	void HideSelectedRadius();
	void HideAllRadii();

	static void ShowHoverRadiiOnActor(AActor* Actor);
	static void HideHoverRadiiOnActor(AActor* Actor);
	static void ShowSelectedRadiiOnActor(AActor* Actor);
	static void HideSelectedRadiiOnActor(AActor* Actor);

private:
	static constexpr int32 InvalidRadiusId = -1;

	UPROPERTY()
	TWeakObjectPtr<URTSRadiusPoolSubsystem> M_RadiusPoolSubsystem;

	int32 M_HoverRadiusId = InvalidRadiusId;
	int32 M_SelectedRadiusId = InvalidRadiusId;

	bool GetIsValidRadiusPoolSubsystem();
	URTSRadiusPoolSubsystem* GetRadiusPoolSubsystem();
	int32 CreateRadius();
	bool GetIsRadiusActive(int32 RadiusId);
	void HideRadiusById(int32& RadiusId);
};
