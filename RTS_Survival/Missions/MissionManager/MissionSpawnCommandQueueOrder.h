#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachWeaponAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionTypes/FieldConstructionTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapAbilityTypes.h"
#include "RTS_Survival/RTSComponents/TowMechanic/TowAbilityTypes/TowAbilityTypes.h"
#include "MissionSpawnCommandQueueOrder.generated.h"

/**
 * @brief One mission-authored command entry executed after an async spawned unit finished startup initialization.
 * Designers can define a queue of these entries in blueprint and the first command clears existing queue state.
 */
USTRUCT(BlueprintType)
struct FMissionSpawnCommandQueueOrder
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue")
	EAbilityID AbilityID = EAbilityID::IdNoAbility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue")
	FRotator TargetRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue|Subtype")
	EBehaviourAbilityType BehaviourAbilityType = EBehaviourAbilityType::DefaultSprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue|Subtype")
	EModeAbilityType ModeAbilityType = EModeAbilityType::DefaultSniperOverwatch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue|Subtype")
	EFieldConstructionType FieldConstructionType = EFieldConstructionType::DefaultGerHedgeHog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue|Subtype")
	EGrenadeAbilityType GrenadeAbilityType = EGrenadeAbilityType::DefaultGerBundleGrenade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue|Subtype")
	EAimAbilityType AimAbilityType = EAimAbilityType::DefaultBrummbarFire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue|Subtype")
	EAttachWeaponAbilitySubType AttachedWeaponAbilityType = EAttachWeaponAbilitySubType::Pz38AttachedMortarDefault;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue|Subtype")
	ETurretSwapAbility TurretSwapAbilityType = ETurretSwapAbility::DefaultSwitchAT;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Spawn Queue|Subtype")
	ETowedActorTarget TowedActorTargetType = ETowedActorTarget::None;
};
