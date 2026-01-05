// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Components/ActorComponent.h"
#include "FieldConstructionData/FieldConstructionData.h"
#include "FieldConstructionAbilityComponent.generated.h"

class ASquadController;
class AStaticPreviewMesh;
class AFieldConstruction;
enum class EFieldConstructionType : uint8;


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
	
};

USTRUCT(Blueprintable)
struct FFieldConstructionEquipmentData
{
	GENERATED_BODY()
	
	// The equipment that the scavenger uses.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	TObjectPtr<UStaticMesh> FieldConstructEquipment;

	// The socket to which the ScavengeEquipment can be attached.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Scavenge")
	FName FieldConstructSocketName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> FieldConstructEffect;

	// The name of the socket on the equipment mesh to attach the effect to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	FName FieldConstructEffectSocketName;
	
};

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

	UPROPERTY()
	TArray<AStaticPreviewMesh*> M_StaticPreviewsWaitingForConstruction;

	UPROPERTY()
	ASquadController* M_OwningSquadController = nullptr;
	bool GetIsValidSquadController() const;

};
