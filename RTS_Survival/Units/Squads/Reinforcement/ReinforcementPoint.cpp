// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ReinforcementPoint.h"

#include "Components/MeshComponent.h"
#include "Components/SphereComponent.h"
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
        const int32 OwningPlayer, const float ActivationRadius)
{
        M_ReinforcementMeshComponent = InMeshComponent;
        M_ReinforcementSocketName = InSocketName;
        M_OwningPlayer = OwningPlayer;
        M_ReinforcementActivationRadius = ActivationRadius;
        CreateReinforcementTriggerSphere(ActivationRadius);
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

void UReinforcementPoint::CreateReinforcementTriggerSphere(const float ActivationRadius)
{
        if (IsValid(M_ReinforcementTriggerSphere))
        {
                return;
        }
        AActor* OwnerActor = GetOwner();
        if (not IsValid(OwnerActor))
        {
                RTSFunctionLibrary::ReportError("Reinforcement point has no valid owner for trigger creation.",
                        "\nUReinforcementPoint::CreateReinforcementTriggerSphere");
                return;
        }

        M_ReinforcementTriggerSphere = NewObject<USphereComponent>(OwnerActor, TEXT("ReinforcementTriggerSphere"));
        if (not IsValid(M_ReinforcementTriggerSphere))
        {
                RTSFunctionLibrary::ReportError("Failed to create reinforcement trigger sphere component.",
                        "\nUReinforcementPoint::CreateReinforcementTriggerSphere");
                return;
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

        const ETriggerOverlapLogic TriggerLogic = M_OwningPlayer == 1 ? ETriggerOverlapLogic::OverlapPlayer :
                ETriggerOverlapLogic::OverlapEnemy;
        FRTS_CollisionSetup::SetupTriggerOverlapCollision(M_ReinforcementTriggerSphere, TriggerLogic);

        M_ReinforcementTriggerSphere->OnComponentBeginOverlap.AddDynamic(this,
                &UReinforcementPoint::OnReinforcementOverlapBegin);
        M_ReinforcementTriggerSphere->OnComponentEndOverlap.AddDynamic(this,
                &UReinforcementPoint::OnReinforcementOverlapEnd);
}

void UReinforcementPoint::HandleSquadUnitEnteredRadius(ASquadUnit* OverlappingUnit) const
{
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
        ReinforcementComponent->ActivateReinforcements(true);
}

void UReinforcementPoint::HandleSquadUnitExitedRadius(ASquadUnit* OverlappingUnit) const
{
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
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
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
