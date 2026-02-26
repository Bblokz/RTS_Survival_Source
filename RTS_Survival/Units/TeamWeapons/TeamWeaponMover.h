// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "TeamWeaponMover.generated.h"

class ASquadUnit;
class ATeamWeapon;
class ATeamWeaponController;
struct FNavigationPath;
enum class ETeamWeaponMovementType : uint8;

DECLARE_MULTICAST_DELEGATE(FOnPushedMoveArrived);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPushedMoveFailed, const FString& /*FailureReason*/);

USTRUCT()
struct FTeamWeaponCrewMemberOffset
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_CrewMember;

	UPROPERTY()
	FVector M_OffsetFromCenter = FVector::ZeroVector;
};

/**
 * @brief Handles both legacy crew-follow and pushed path-follow movement for team weapons.
 *
 * Uses controller-generated nav paths and keeps movement-state notifications consistent for animation.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTeamWeaponMover : public UPathFollowingComponent
{
	GENERATED_BODY()

public:
	UTeamWeaponMover();

	void InitMover(ATeamWeapon* InTeamWeapon, ATeamWeaponController* InController);
	void SetCrewMembersToFollow(const TArray<FTeamWeaponCrewMemberOffset>& CrewMembers);

	void StartLegacyFollowCrew();
	void StopLegacyFollowCrew();
	void MoveWeaponToLocation(const FVector& Destination);
	void NotifyCrewReady(const bool bIsReady);

	void StartPushedFollowPath(const FNavPathSharedPtr& InPath);
	void AbortPushedFollowPath(const FString& Reason);
	bool GetIsPushedFollowingPath() const;

	FOnPushedMoveArrived OnPushedMoveArrived;
	FOnPushedMoveFailed OnPushedMoveFailed;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdateLegacyMovement();
	void UpdatePushedMovement(const float DeltaSeconds);
	void FinalizePushedArrived();
	void StopPushedMovement();
	bool GetIsCrewDataValid() const;
	bool TryGetCrewCenterLocation(FVector& OutCenter) const;
	bool HaveCrewReachedDestination() const;
	void UpdateOwnerLocationFromCrew();
	void UpdateMovementAnimationState(const float DeltaSeconds);
	bool TryAdvancePathIndexIfReachedTarget(const FVector& WeaponLocation, const float AcceptanceRadiusCm);
	bool TryGetGroundAdjustedLocation(const FVector& InProposedLocation, FVector& OutGroundAdjustedLocation) const;
	void ApplyYawStepTowards(const FVector& MovementDirection, const float DeltaSeconds) const;
	bool TryMoveWeaponWithSweep(const FVector& ProposedLocation, FVector& OutAppliedLocation) const;
	void SyncControllerTransformToWeapon() const;
	bool GetUsesLegacyCrewMode() const;
	bool GetIsValidTeamWeapon() const;
	bool GetIsValidTeamWeaponController() const;
	bool GetIsValidPushedPath() const;
	void UpdateTickEnabledState();

	UPROPERTY()
	TArray<FTeamWeaponCrewMemberOffset> M_CrewMembers;

	UPROPERTY()
	FVector M_MoveDestination = FVector::ZeroVector;

	UPROPERTY()
	bool bM_IsCrewReady = false;

	UPROPERTY()
	bool bM_HasMoveRequest = false;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeapon> M_TeamWeapon;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeaponController> M_TeamWeaponController;

	FNavPathSharedPtr M_PushedPath;

	UPROPERTY()
	int32 M_CurrentPathPointIndex = 0;

	UPROPERTY()
	FVector M_LastWeaponLocation = FVector::ZeroVector;

	UPROPERTY()
	bool bM_IsLegacyFollowActive = false;

	UPROPERTY()
	bool bM_IsPushedActive = false;
};
