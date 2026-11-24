// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "EnemyWorkCircuitRunner.generated.h"

class ASquadController;
class ASquadUnit;
class UAnimMontage;

UENUM()
enum class EEnemyWorkCircuitPhase : uint8
{
	Stopped,
	Walking,
	Working
};

/**
 * @brief Created by AEnemyStrongpoint for a squad defender whose post has a work circuit: walks the squad from point
 * to point and loops a work montage on every soldier at each point. The strongpoint stops it on the first fight, on
 * release and when it is cleared; the circuit also stops by itself when its squad fights or dies.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyWorkCircuitRunner : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @brief Starts walking to the first point.
	 * @param Squad The defender that works the circuit.
	 * @param WorldPoints Points on the navmesh, visited in order and then again from the first.
	 * @param WorkSeconds Seconds of work at each point.
	 * @param MaxWalkSeconds A walk that takes longer than this ends where the squad stands.
	 * @param WorkMontage Looped on every soldier while working; may be null (the squad then only stands there).
	 */
	void StartCircuit(ASquadController* Squad, const TArray<FVector>& WorldPoints, const float WorkSeconds,
	                  const float MaxWalkSeconds, UAnimMontage* WorkMontage);

	// Ends the circuit; the squad stays where it is and its montages blend out.
	void StopCircuit();

	bool GetIsRunning() const { return M_Phase != EEnemyWorkCircuitPhase::Stopped; }

	EEnemyWorkCircuitPhase GetPhase() const { return M_Phase; }

	int32 GetPointIndex() const { return M_PointIndex; }

	virtual void BeginDestroy() override;

private:
	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_Squad = nullptr;

	UPROPERTY()
	TObjectPtr<UAnimMontage> M_WorkMontage = nullptr;

	TArray<FVector> M_Points;
	FTimerHandle M_TickTimerHandle;
	EEnemyWorkCircuitPhase M_Phase = EEnemyWorkCircuitPhase::Stopped;
	int32 M_PointIndex = 0;
	float M_WorkSeconds = 25.f;
	float M_MaxWalkSeconds = 60.f;
	// Seconds spent in the current phase.
	float M_PhaseSeconds = 0.f;

	bool GetIsValidSquad() const;
	void TickCircuit();
	void StartWalkingToCurrentPoint();
	void StartWorking();
	void FinishWorkingAtPoint();
	bool GetHasSquadArrived() const;
	FVector GetSquadCentre() const;
	TArray<ASquadUnit*> GetSquadUnits() const;
	void LoopWorkMontage() const;
	void StopWorkMontage() const;
	void ClearTickTimer();
};
