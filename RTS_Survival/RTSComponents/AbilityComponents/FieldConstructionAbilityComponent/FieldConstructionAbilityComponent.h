// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "FieldConstructionData/FieldConstructionData.h"
#include "FieldConstructionTypes/FieldConstructionTypes.h"
#include "FieldConstructionAbilityComponent.generated.h"

class ASquadController;
class ASquadUnit;
class AStaticPreviewMesh;
class AFieldConstruction;
class UNiagaraComponent;
class UStaticMeshComponent;
class URTSTimedProgressBarManager;


USTRUCT(Blueprintable)
struct FFieldConstructionAbilitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EFieldConstructionType FieldConstructionType;
	
	// Attempts to add the abilty to this index of the Unit's Ability Array.
	// Reverts to first empty index if negative or already occupied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;

	// How long the ability is on cooldown after use. (does not affect behaviour duration)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cooldown = 0;

	// How long the field construction takes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ConstructionTime = 15.f;

	// To spawn to actor to construct.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AFieldConstruction> FieldConstructionClass;

	// Mesh used for previewing the placement of the field construction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh>	PreviewMesh;

	// Defines rules about how the field construction is built.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFieldConstructionData FieldConstructionData;

	// One-shot effect that will play at the location of the construction when it completes.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> OnCompletionEffect;
	
};

USTRUCT(Blueprintable)
struct FFieldConstructionEquipmentData
{
	GENERATED_BODY()
	
	// The equipment that the scavenger uses.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UStaticMesh> FieldConstructEquipment;

	// The socket to which the ScavengeEquipment can be attached.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	FName FieldConstructSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> FieldConstructEffect;

	// The name of the socket on the equipment mesh to attach the effect to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	FName FieldConstructEffectSocketName;
	
};

UENUM()
enum class EFieldConstructionAbilityPhase : uint8
{
	None,
	MovingToLocation,
	Constructing,
	Completed
};

USTRUCT()
struct FFieldConstructionUnitEquipmentState
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_SquadUnit = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_EquipmentMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_EquipmentEffect = nullptr;
};

USTRUCT()
struct FFieldConstructionActiveState
{
	GENERATED_BODY()

	void Reset()
	{
		M_PreviewActor = nullptr;
		M_SpawnedConstruction = nullptr;
		M_TargetLocation = FVector::ZeroVector;
		M_TargetRotation = FRotator::ZeroRotator;
		M_ConstructionDuration = 0.f;
		M_ProgressBarActivationID = 0;
		M_CurrentPhase = EFieldConstructionAbilityPhase::None;
		M_ConstructionType = EFieldConstructionType::DefaultGerHedgeHog;
	}

	UPROPERTY()
	TWeakObjectPtr<AStaticPreviewMesh> M_PreviewActor = nullptr;

	UPROPERTY()
	TWeakObjectPtr<AFieldConstruction> M_SpawnedConstruction = nullptr;

	FVector M_TargetLocation = FVector::ZeroVector;
	FRotator M_TargetRotation = FRotator::ZeroRotator;
	float M_ConstructionDuration = 0.f;
	uint64 M_ProgressBarActivationID = 0;
	EFieldConstructionAbilityPhase M_CurrentPhase = EFieldConstructionAbilityPhase::None;
	EFieldConstructionType M_ConstructionType = EFieldConstructionType::DefaultGerHedgeHog;
};

/**
 * @brief Component that lets squads place and build field constructions including visuals and equipment handling.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UFieldConstructionAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UFieldConstructionAbilityComponent();

	void AddStaticPreviewWaitingForConstruction(AStaticPreviewMesh* StaticPreview);

	UStaticMesh* GetPreviewMesh() const { return FieldConstructionAbilitySettings.PreviewMesh; }
	FFieldConstructionData GetFieldConstructionData() const { return FieldConstructionAbilitySettings.FieldConstructionData; }

	EFieldConstructionType GetConstructionType() const { return FieldConstructionAbilitySettings.FieldConstructionType; }

	/**
	 * @brief Starts the field construction command for this component's construction type.
	 * @param ConstructionLocation Target build location.
	 * @param ConstructionRotation Rotation for the construction actor.
	 * @param StaticPreviewActor Preview actor placed by the player.
	 */
	void ExecuteFieldConstruction(const FVector& ConstructionLocation, const FRotator& ConstructionRotation,
	                              AStaticPreviewMesh* StaticPreviewActor);
	/**
	 * @brief Terminates the active field construction command and cleans up visuals.
	 * @param StaticPreviewActor Preview actor cached on the command queue.
	 */
	void TerminateFieldConstructionCommand(AActor* StaticPreviewActor);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings")
	FFieldConstructionAbilitySettings FieldConstructionAbilitySettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Settings|Effects")
	FFieldConstructionEquipmentData FieldConstructionEquipmentSettings;

private:
	void BeginPlay_SetFieldTypeOnConstructionData();
	void BeginPlay_SetOwningSquadController();
	void BeginPlay_ValidateSettings() const;
	void BeginPlay_AddAbility();
	void BeginPlay_CacheTimedProgressBarManager();
	void AddAbilityToCommands();
	void AddAbilityToSquad(ASquadController* Squad);
	bool GetIsValidCommandsInterface() const;

	UPROPERTY()
	TArray<AStaticPreviewMesh*> M_StaticPreviewsWaitingForConstruction;

	UPROPERTY()
	ASquadController* M_OwningSquadController = nullptr;
	bool GetIsValidSquadController() const;

	UPROPERTY()
	FTimerHandle M_FieldConstructionRangeCheckHandle;

	UPROPERTY()
	FTimerHandle M_FieldConstructionDurationHandle;

	UPROPERTY()
	FFieldConstructionActiveState M_ActiveConstructionState;

	UPROPERTY()
	TArray<FFieldConstructionUnitEquipmentState> M_FieldConstructionEquipment;

	UPROPERTY()
	TWeakObjectPtr<URTSTimedProgressBarManager> M_TimedProgressBarManager;
	bool GetIsValidTimedProgressBarManager() const;

	void StartMovementToConstruction();
	void OnUnitCloseEnoughForConstruction();
	bool GetAnySquadUnitWithinConstructionRange() const;
	void StartConstructionPhase();
	AFieldConstruction* SpawnFieldConstructionActor();
	UStaticMeshComponent* GetPreviewMeshComponent(AActor* StaticPreviewActor) const;
	void SetupSpawnedConstruction(AFieldConstruction* SpawnedConstruction, UStaticMeshComponent* PreviewMeshComponent);
	void StartConstructionTimer();
	float CalculateConstructionDurationSeconds() const;
	int32 GetAliveSquadUnitCount() const;
	int32 GetMaxSquadUnitCount() const;
	void FinishConstruction();
	void SpawnCompletionEffect() const;
	void AddEquipmentToSquad();
	void RemoveEquipmentFromSquad();
	void DisableSquadWeapons(const bool bDisable) const;
	void PlayConstructionAnimation() const;
	void StopConstructionAnimation() const;
	void DestroyPreviewActor(AActor* StaticPreviewActor) const;
	UFUNCTION()
	void OnConstructionActorDestroyed(AActor* DestroyedActor);
	void ResetConstructionState();
	void StopConstructionRangeCheckTimer();
	void StopConstructionDurationTimer();
	void ReportError_InvalidConstructionState(const FString& ErrorContext) const;
	void StartConstructionProgressBar(AFieldConstruction* SpawnedConstruction);
	void StopConstructionProgressBar();
};
