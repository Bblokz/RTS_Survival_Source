// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BxpHolesQATestSubsystem.generated.h"

class ABuildingExpansion;
class UMaterialInterface;
class UWallHoleDamageComponent;
class UStaticMesh;

UENUM()
enum class EBxpHolesQATestStage : uint8
{
	WaitingForStart,
	SpawningNext,
	WaitingForSettle,
	WaitingAfterDamage,
	WaitingAfterDeath,
	Finished
};

/**
 * @brief -BxpHolesQAValidate: the German expansions that moved from _At50/_At33 damage meshes to procedural wall holes.
 * Each class is spawned on its own near the anchor, gets debug shell hits at its aim points (holes must be cut), is then
 * damaged through AActor::TakeDamage past every health notify level (the legacy Blueprint swaps must not change the
 * building mesh, and the hole materials must stay), and is finally killed (its death must run without errors).
 * Optional -BxpQAAnchorX= / -BxpQAAnchorY= and -BxpQAOnly=<substring>. Log lines RTS_BXPQA_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UBxpHolesQATestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EBxpHolesQATestStage M_Stage = EBxpHolesQATestStage::WaitingForStart;
	float M_StageSeconds = 0.f;
	float M_BootSeconds = -1.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	int32 M_ClassIndex = -1;
	FVector M_Anchor = FVector::ZeroVector;
	TArray<FString> M_ClassPaths;

	UPROPERTY()
	TWeakObjectPtr<ABuildingExpansion> M_Bxp;

	UPROPERTY()
	TWeakObjectPtr<UStaticMesh> M_MeshAfterSpawn;

	UPROPERTY()
	TArray<TWeakObjectPtr<UMaterialInterface>> M_MaterialsAfterHoles;

	void TickWaitingForStart();
	void TickSpawningNext();
	void TickWaitingForSettle();
	void TickWaitingAfterDamage();
	void TickWaitingAfterDeath();

	void CutTestHoles(ABuildingExpansion& Bxp);
	void CheckDebrisMaterials(const UWallHoleDamageComponent& Holes);
	void DamagePastNotifyLevels(ABuildingExpansion& Bxp);
	void CheckMeshAndMaterialsKept(ABuildingExpansion& Bxp, const FString& Moment);
	FString GetCurrentName() const;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	void EnterStage(EBxpHolesQATestStage NewStage);
	float GetNow() const;
	float GetStageElapsed() const;
	bool StartGameAndGetIsReady() const;
	bool FindAnchor(FVector& OutAnchor) const;
	void Finish();
};
