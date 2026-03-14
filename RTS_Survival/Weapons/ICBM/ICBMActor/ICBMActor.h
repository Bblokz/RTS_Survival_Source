#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RTS_Survival/Weapons/ICBM/ICBMTypes.h"
#include "ICBMActor.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UICBMLaunchComponent;

UENUM()
enum class EICBMFlightStage : uint8
{
	None,
	Prep,
	Launch,
	Arc,
	Terminal
};

/**
 * @brief Runtime missile actor used by UICBMLaunchComponent from spawn, ready-up, and staged flight until impact.
 */
UCLASS()
class RTS_SURVIVAL_API AICBMActor : public AActor
{
	GENERATED_BODY()

public:
	AICBMActor();

	void InitICBMActor(UICBMLaunchComponent* OwningLaunchComponent, int32 SocketIndex, int32 OwningPlayer);
	void StartVerticalMoveToLaunchReady(const FVector& TargetLocation, float MoveDuration);
	void FireToLocation(const FVector& TargetLocation, int32 OwningPlayer);

	bool GetIsLaunchReady() const { return bM_IsLaunchReady; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void EnterPrepStage();
	void EnterLaunchStage();
	void EnterArcStage();
	void EnterTerminalStage();

	void UpdateVerticalMove(const float DeltaSeconds);
	void UpdateLaunchStage(const float DeltaSeconds);
	void UpdateArcStage(const float DeltaSeconds);
	void UpdateTerminalStage();

	void StartTerminalTraceTimer();
	void PerformAsyncLineTrace();
	void OnAsyncTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);
	void HandleImpact(const FHitResult& HitResult);
	void SpawnImpactEffects(const FVector& ImpactLocation, const ERTSSurfaceType SurfaceType) const;
	void ApplyDirectDamage(const FHitResult& HitResult, AActor* HitActor);
	void ApplyAOEDamage(const FVector& ImpactLocation, AActor* PrimaryHitActor);

	bool GetIsValidProjectileMovementComp() const;
	bool GetIsValidOwningLaunchComponent() const;
	bool GetIsValidMissileMeshComponent() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> M_MissileMeshComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> M_ProjectileMovementComp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> M_AudioComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UNiagaraComponent> M_NiagaraComponent;

	UPROPERTY()
	TWeakObjectPtr<UICBMLaunchComponent> M_OwningLaunchComponent = nullptr;

	UPROPERTY()
	FICBMLaunchSettings M_LaunchSettings;

	UPROPERTY()
	bool bM_IsLaunchReady = false;

	UPROPERTY()
	FVector M_VerticalMoveStart = FVector::ZeroVector;

	UPROPERTY()
	FVector M_VerticalMoveTarget = FVector::ZeroVector;

	UPROPERTY()
	float M_VerticalMoveDuration = 0.0f;

	UPROPERTY()
	float M_VerticalMoveElapsed = 0.0f;

	UPROPERTY()
	int32 M_OwningPlayer = INDEX_NONE;

	UPROPERTY()
	int32 M_SocketIndex = INDEX_NONE;

	UPROPERTY()
	FVector M_TargetLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector M_ArcStartLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector M_ArcControlLocation = FVector::ZeroVector;

	UPROPERTY()
	float M_ArcDuration = 0.6f;

	UPROPERTY()
	float M_ArcElapsed = 0.0f;

	UPROPERTY()
	float M_CurrentLaunchSpeed = 0.0f;

	UPROPERTY()
	float M_LaunchStartZ = 0.0f;

	UPROPERTY()
	EICBMFlightStage M_FlightStage = EICBMFlightStage::None;

	FTimerHandle M_PrepStageTimer;
	FTimerHandle M_TerminalTraceTimer;
};
