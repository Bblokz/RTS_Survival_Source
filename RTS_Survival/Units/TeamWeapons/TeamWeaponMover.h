// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TeamWeaponMover.generated.h"

class ASquadUnit;
class ATeamWeapon;

UENUM(BlueprintType)
enum class ETeamWeaponMoverState : uint8
{
	Idle,
	AwaitingCrew,
	Pathing,
	Moving
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTeamWeaponMoverStateChanged, ETeamWeaponMoverState /*NewState*/);
DECLARE_MULTICAST_DELEGATE(FOnTeamWeaponMoverArrived);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTeamWeaponMoverFailed, const FString& /*FailureReason*/);

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
 * @brief Executes physical movement for packed team weapons and reports movement status to the controller.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTeamWeaponMover : public UActorComponent
{
	GENERATED_BODY()

public:
	UTeamWeaponMover();

	void SetCrewMembersToFollow(const TArray<FTeamWeaponCrewMemberOffset>& CrewMembers);
	void MoveWeaponToLocation(const FVector& Destination);
	void BeginFollowingCrew();
	void AbortMove(const FString& Reason);
	void NotifyCrewReady(const bool bIsReady);

	bool GetIsMoving() const;
	ETeamWeaponMoverState GetMoverState() const { return M_MoverState; }
	int32 GetCrewMembersRequiredToMove() const { return M_CrewMembersRequiredToMove; }
	const FVector& GetCurrentDestination() const { return M_MoveDestination; }

	FOnTeamWeaponMoverStateChanged OnMoverStateChanged;
	FOnTeamWeaponMoverArrived OnMoverArrived;
	FOnTeamWeaponMoverFailed OnMoverFailed;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetMoverState(const ETeamWeaponMoverState NewState);
	bool GetIsOwnerValid() const;
	bool GetIsValidOwnerTeamWeapon() const;
	bool GetIsCrewDataValid() const;
	bool TryGetCrewCenterLocation(FVector& OutCenter) const;
	bool HaveCrewReachedDestination() const;
	void UpdateOwnerLocationFromCrew();
	void UpdateMovementAnimationState();

	UPROPERTY(EditDefaultsOnly, Category="TeamWeapon|Mover")
	int32 M_CrewMembersRequiredToMove = 2;

	UPROPERTY()
	TArray<FTeamWeaponCrewMemberOffset> M_CrewMembers;

	UPROPERTY()
	FVector M_MoveDestination = FVector::ZeroVector;

	UPROPERTY()
	bool bM_IsCrewReady = false;

	UPROPERTY()
	bool bM_HasMoveRequest = false;

	UPROPERTY()
	ETeamWeaponMoverState M_MoverState = ETeamWeaponMoverState::Idle;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeapon> M_OwnerTeamWeapon;

	UPROPERTY()
	FVector M_LastOwnerLocation = FVector::ZeroVector;
};
