// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ReinforcementPoint.h"

#include "Components/MeshComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/Reinforcement/SquadReinforcementComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

UReinforcementPoint::UReinforcementPoint(): M_OwningPlayer(-1), M_ReinforcementActivationRadius(0.0f)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UReinforcementPoint::InitReinforcementPoint(UMeshComponent* InMeshComponent, const FName& InSocketName,
                                                 const int32 OwningPlayer, const float ActivationRadius,
                                                 const bool bStartEnabled)
{
	M_ReinforcementMeshComponent = InMeshComponent;
	M_ReinforcementSocketName = InSocketName;
	M_OwningPlayer = OwningPlayer;
	M_ReinforcementActivationRadius = ActivationRadius;
	SetReinforcementEnabled(bStartEnabled);
}

void UReinforcementPoint::SetReinforcementEnabled(const bool bEnable)
{
	bM_ReinforcementEnabled = bEnable;

	if (bM_ReinforcementEnabled)
	{
		if (not CreateReinforcementTriggerSphere(M_ReinforcementActivationRadius))
		{
			bM_ReinforcementEnabled = false;
			return;
		}
		SetTriggerOverlapEnabled(true);
		return;
	}

	SetTriggerOverlapEnabled(false);
}

bool UReinforcementPoint::GetIsReinforcementEnabled() const
{
	return bM_ReinforcementEnabled;
}

FVector UReinforcementPoint::GetReinforcementLocation(bool& bOutHasValidLocation) const
{
	bOutHasValidLocation = false;

	if (not GetIsValidReinforcementMesh())
	{
		return FVector::ZeroVector;
	}

	UMeshComponent* MeshComponent = M_ReinforcementMeshComponent.Get();
	if (not MeshComponent->DoesSocketExist(M_ReinforcementSocketName))
	{
		RTSFunctionLibrary::ReportError("Requested reinforcement socket does not exist on mesh."
			"\n Socket: " + M_ReinforcementSocketName.ToString());
		return FVector::ZeroVector;
	}

	bOutHasValidLocation = true;
	return MeshComponent->GetSocketLocation(M_ReinforcementSocketName);
}

bool UReinforcementPoint::GetIsValidReinforcementMesh() const
{
	if (M_ReinforcementMeshComponent.IsValid())
	{
		return true;
	}
	return false;
}

bool UReinforcementPoint::GetIsValidReinforcementTriggerSphere(const bool bReportIfMissing) const
{
	if (IsValid(M_ReinforcementTriggerSphere))
	{
		return true;
	}

	if (bReportIfMissing && bM_ReinforcementEnabled)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_ReinforcementTriggerSphere",
		                                                      "GetIsValidReinforcementTriggerSphere", GetOwner());
	}

	return false;
}

bool UReinforcementPoint::CreateReinforcementTriggerSphere(const float ActivationRadius)
{
	if (GetIsValidReinforcementTriggerSphere(false))
	{
		return true;
	}
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		RTSFunctionLibrary::ReportError("Reinforcement point has no valid owner for trigger creation."
			"\nUReinforcementPoint::CreateReinforcementTriggerSphere");
		return false;
	}

	M_ReinforcementTriggerSphere = NewObject<USphereComponent>(OwnerActor, TEXT("ReinforcementTriggerSphere"));
	if (not IsValid(M_ReinforcementTriggerSphere))
	{
		RTSFunctionLibrary::ReportError("Failed to create reinforcement trigger sphere component."
			"\nUReinforcementPoint::CreateReinforcementTriggerSphere");
		return false;
	}

	const float ClampedRadius = FMath::Max(ActivationRadius, 0.0f);
	M_ReinforcementTriggerSphere->SetSphereRadius(ClampedRadius);

	if (M_ReinforcementMeshComponent.IsValid())
	{
		M_ReinforcementTriggerSphere->SetupAttachment(M_ReinforcementMeshComponent.Get());
	}
	else
	{
		M_ReinforcementTriggerSphere->SetupAttachment(OwnerActor->GetRootComponent());
	}
	M_ReinforcementTriggerSphere->RegisterComponent();

	const ETriggerOverlapLogic TriggerLogic = M_OwningPlayer == 1
		                                          ? ETriggerOverlapLogic::OverlapPlayer
		                                          : ETriggerOverlapLogic::OverlapEnemy;
	FRTS_CollisionSetup::SetupTriggerOverlapCollision(M_ReinforcementTriggerSphere, TriggerLogic);

	M_ReinforcementTriggerSphere->OnComponentBeginOverlap.AddDynamic(this,
	                                                                 &UReinforcementPoint::OnReinforcementOverlapBegin);
	M_ReinforcementTriggerSphere->OnComponentEndOverlap.AddDynamic(this,
	                                                               &UReinforcementPoint::OnReinforcementOverlapEnd);

	return true;
}

void UReinforcementPoint::HandleSquadUnitEnteredRadius(ASquadUnit* OverlappingUnit) const
{
	if (not bM_ReinforcementEnabled)
	{
		return;
	}
	if (not IsValid(OverlappingUnit))
	{
		return;
	}
	if (OverlappingUnit->GetOwningPlayer() != M_OwningPlayer)
	{
		return;
	}

	TObjectPtr<ASquadController> SquadController = OverlappingUnit->GetSquadControllerChecked();
	if (not IsValid(SquadController))
	{
		return;
	}

	TObjectPtr<USquadReinforcementComponent> ReinforcementComponent = SquadController->GetSquadReinforcementComponent();
	if (not IsValid(ReinforcementComponent))
	{
		return;
	}

	if constexpr (DeveloperSettings::Debugging::GReinforcementAbility_Compile_DebugSymbols)
	{
		const FVector DrawLocation = OverlappingUnit->GetActorLocation() + FVector::UpVector * 150.0f;
		const FString DebugString = FString("Reinforcement overlap begin - Unit: ") + OverlappingUnit->GetName();
		DrawDebugStatusString(DebugString, DrawLocation);
	}

	ReinforcementComponent->ActivateReinforcements(true);
}

void UReinforcementPoint::HandleSquadUnitExitedRadius(ASquadUnit* OverlappingUnit) const
{
	if (not bM_ReinforcementEnabled)
	{
		return;
	}
	if (not IsValid(OverlappingUnit))
	{
		return;
	}
	if (OverlappingUnit->GetOwningPlayer() != M_OwningPlayer)
	{
		return;
	}

	TObjectPtr<ASquadController> SquadController = OverlappingUnit->GetSquadControllerChecked();
	if (not IsValid(SquadController))
	{
		return;
	}

	TObjectPtr<USquadReinforcementComponent> ReinforcementComponent = SquadController->GetSquadReinforcementComponent();
	if (not IsValid(ReinforcementComponent))
	{
		return;
	}
	ReinforcementComponent->ActivateReinforcements(false);
}

void UReinforcementPoint::OnReinforcementOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                                      bool bFromSweep, const FHitResult& SweepResult)
{
	ASquadUnit* OverlappingUnit = Cast<ASquadUnit>(OtherActor);
	HandleSquadUnitEnteredRadius(OverlappingUnit);
}

void UReinforcementPoint::OnReinforcementOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ASquadUnit* OverlappingUnit = Cast<ASquadUnit>(OtherActor);
	HandleSquadUnitExitedRadius(OverlappingUnit);
}

void UReinforcementPoint::OverlapOnReinforcementEnabled(float Radius, int32 OwningPlayer, bool bEnable) const
{
		// create a sphere overlap at owner location with given radius and player filter
	ECollisionChannel CollisionChannel = OwningPlayer == 1 ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY;
	
}

bool UReinforcementPoint::GetIsDebugEnabled() const
{
	return DeveloperSettings::Debugging::GReinforcementAbility_Compile_DebugSymbols;
}

void UReinforcementPoint::SetTriggerOverlapEnabled(const bool bEnable) const
{
	if (not GetIsValidReinforcementTriggerSphere())
	{
		return;
	}

	M_ReinforcementTriggerSphere->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	M_ReinforcementTriggerSphere->SetGenerateOverlapEvents(bEnable);

	if constexpr (DeveloperSettings::Debugging::GReinforcementAbility_Compile_DebugSymbols)
	{
		const FVector DebugLocation = M_ReinforcementTriggerSphere->GetComponentLocation() + FVector::UpVector * 150.0f;
		const FString DebugText = bEnable ? TEXT("Reinforcement Trigger Enabled") : TEXT("Reinforcement Trigger Disabled");
		DrawDebugStatusString(DebugText, DebugLocation);
	}
}

void UReinforcementPoint::DrawDebugStatusString(const FString& DebugText, const FVector& DrawLocation) const
{
	if (not GetIsDebugEnabled())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		DrawDebugString(World, DrawLocation, DebugText, nullptr, FColor::Green, 2.5f, true);
	}
}
