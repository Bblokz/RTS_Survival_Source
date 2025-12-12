// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ReinforcementPoint.h"

#include "Components/MeshComponent.h"
#include "RTS_Survival/Utils/RTSFunctionLibrary.h"

UReinforcementPoint::UReinforcementPoint()
{
        PrimaryComponentTick.bCanEverTick = false;
        PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UReinforcementPoint::InitReinforcementPoint(UMeshComponent* InMeshComponent, const FName& InSocketName)
{
        M_ReinforcementMeshComponent = InMeshComponent;
        M_ReinforcementSocketName = InSocketName;
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
