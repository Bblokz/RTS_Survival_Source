#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#include "PlayerRotationArrowSettings.generated.h"


class UFormationController;
class ACPPController;

USTRUCT(BlueprintType)
struct FPlayerRotationArrowSettings
{
	GENERATED_BODY()

	void InitRotationArrowAction(
		const FVector2D& InitialMouseScreenLocation,
		const FVector& InitialMouseProjectedLocation);
	
	void TickArrowRotation(
		const FVector2D& MouseScreenLocation,
		const FVector& MouseProjectedLocation);

	void StopRotationArrow();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> RotationArrowClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector ArrowOffset = FVector(0.f, 0.f, 100.f);

	// Set by player controller; spawned at begin play to provide visual feedback of where
	// the rotation arrow is pointing.
	UPROPERTY()
	AActor* RotationArrowActor = nullptr;

	// Set by player controller; to set the final rotation of the arrow on.
	TWeakObjectPtr<UFormationController> PlayerFormationController = nullptr;

private:
	bool EnsureRotationActorIsValid() const;
	bool EnsureFormationControllerIsValid() const;

	void ResetForNextRotation();

	void RotateArrowToProjection(const FVector& MouseProjectedLocation) const;

	// Set to true upon the start of the secondary click; will then tick every frame
	// till the StopRotationArrow is called.
	bool bM_RotationArrowInitialized = false;

	// Whether the mouse was moved enough in screen space to justify a rotation arrow action.
	bool bM_MouseMovedEnough = false;
	
	UPROPERTY()
	FVector2D M_OriginalMouseScreenLocation = FVector2D::ZeroVector;

	void DebugArrow(const FString& Message, const FColor Color = FColor::Blue) const;
};