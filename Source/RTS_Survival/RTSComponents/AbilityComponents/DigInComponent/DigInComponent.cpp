// Copyright (C) Bas Blokzijl - All rights reserved.


#include "DigInComponent.h"

#include "DigInUnit/DigInUnit.h"
#include "DigInWall/DigInWall.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


UDigInComponent::UDigInComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDigInComponent::OnWallCompletedBuilding(const ADigInWall* WallCompleted)
{
	if (not GetIsValidOwningDigInUnit() || not EnsureCallingWallIsOwnedByComponent(WallCompleted))
	{
		return;
	}
	M_DigInStatus = EDigInStatus::CompletedBuildingWall;
	M_OwningDigInUnit->OnDigInCompleted();
}

void UDigInComponent::OnWallDestroyed(const ADigInWall* WallDestroyed)
{
	if(M_DigInStatus == EDigInStatus::Movable)
	{
		// We broke cover that destroyed the wall; nothing to do.
		return;
	}
	if (not GetIsValidOwningDigInUnit() || not EnsureCallingWallIsOwnedByComponent(WallDestroyed))
	{
		return;
	}
	// Force a break cover ability on the owning unit as the wall was destroyed.	
	M_OwningDigInUnit->WallGotDestroyedForceBreakCover();
}

void UDigInComponent::SetupOwner(const TScriptInterface<IDigInUnit>& NewOwner)
{
	M_OwningDigInUnit = NewOwner;
	// Error check.
	(void)GetIsValidOwningDigInUnit();
}

void UDigInComponent::ExecuteDigInCommand()
{
	if (not EnsureIsValidWallActorClass() || not GetIsValidOwningDigInUnit())
	{
		return;
	}
	const FVector WallLocation = GetWallLocation();
	SpawnDigInWallActor(WallLocation);
	if (not GetIsValidDigInWallActor())
	{
		return;
	}
	int32 OwningPlayer = 0;
	if (not GetOwningPlayerFromOwner(OwningPlayer))
	{
		return;
	}
	const float Health = M_DigInData.DigInWallHealth;
	const float DigInTime = M_DigInData.DigInTime;
	// Important set these statuses and change the owning unit abilites before wall placement. if the placement fails the
	// unit can cancel correctly.
	M_DigInStatus = EDigInStatus::BuildingWall;
	M_OwningDigInUnit->OnStartDigIn();
	const FVector Scaling = EnsureScalingVectorIsValid(DigInWallScaling) ? DigInWallScaling : FVector::OneVector;
	M_DigInWallActor->StartBuildingWall(Health, DigInTime, OwningPlayer, this, Scaling);
}

void UDigInComponent::TerminateDigInCommand()
{
	// Set imm so that when the wall is destroyed and calls back we know it was because we wanted to break cover!
	M_DigInStatus = EDigInStatus::Movable;
	if (not GetIsValidOwningDigInUnit())
	{
		return;
	}
	if (M_DigInWallActor.IsValid())
	{
		M_DigInWallActor->DestroyWall();
	}
	M_DigInWallActor.Reset();
	M_OwningDigInUnit->OnBreakCoverCompleted();
}


void UDigInComponent::BeginPlay()
{
	Super::BeginPlay();
	M_DigInStatus = EDigInStatus::Movable;
	bool bIsValidData = false;
	int32 OwningPlayer;
	if (not GetOwningPlayerFromOwner(OwningPlayer))
	{
		return;
	}

	M_DigInData = FRTS_Statics::BP_GetDigInDataOfPlayer(OwningPlayer, DigInType, this, bIsValidData);
}


bool UDigInComponent::EnsureIsValidWallActorClass() const
{
	if (IsValid(WallActorClass))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid wall actor class set for DigInComponent: " + GetName() +
		"UDigInComponent::EnsureIsValidWallActorClass");
	return false;
}

FVector UDigInComponent::GetWallLocation() const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		RTSFunctionLibrary::ReportError("Cannot determine wall placement as owner is not valid");
		return FVector::ZeroVector;
	}
	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector OwnerForward = Owner->GetActorForwardVector();
	const FVector WallLocation = OwnerLocation + (OwnerForward * DigInOffset);
	return WallLocation;
}

bool UDigInComponent::SpawnDigInWallActor(const FVector& SpawnLocation)
{
	UWorld* World = GetWorld();
	if (not World || not EnsureIsValidWallActorClass() || not GetOwner())
	{
		return false;
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	const FRotator SpawnRotation = GetOwner()->GetActorRotation();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* AsClass = WallActorClass.Get();
	if (not AsClass)
	{
		RTSFunctionLibrary::ReportError("failed to get class from WallActorClass for DIgInComponent");
		return false;
	}
	M_DigInWallActor = World->SpawnActor<ADigInWall>(
		WallActorClass.Get(),
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);
	if(M_DigInWallActor.IsValid() && EnsureScalingVectorIsValid(DigInWallScaling))
	{
		M_DigInWallActor->SetActorScale3D(DigInWallScaling);
	}
	return GetIsValidDigInWallActor();
}


bool UDigInComponent::GetIsValidDigInWallActor() const
{
	if (not M_DigInWallActor.IsValid())
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("DigInComponent expected valid wall actor but got Null"
			"\n DigInComponent of owner: " + OwnerName);
		return false;
	}
	return true;
}

bool UDigInComponent::GetIsValidOwningDigInUnit() const
{
	if (not M_OwningDigInUnit)
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("Digin component has no valid owner Interface Pointer set!"
			"DigInComponent Of owner: " + OwnerName);
		return false;
	}
	return true;
}


bool UDigInComponent::GetOwningPlayerFromOwner(int32& OutOwningPlayer) const
{
	const AActor* Owner = GetOwner();
	if (not Owner)
	{
		RTSFunctionLibrary::ReportError("DigInComponent has no valid owner to get OwningPlayer from.");
		return false;
	}
	const URTSComponent* RTSComponent = Owner->FindComponentByClass<URTSComponent>();
	if (not RTSComponent)
	{
		RTSFunctionLibrary::ReportError(
			"DigInComponent owner does not have a valid RTSComponent to get OwningPlayer from.");
		return false;
	}
	OutOwningPlayer = RTSComponent->GetOwningPlayer();
	return true;
}

bool UDigInComponent::EnsureCallingWallIsOwnedByComponent(const ADigInWall* CallingWall) const
{
	if (not CallingWall || CallingWall!= M_DigInWallActor.Get())
	{
		const FString COmpletedWallName = CallingWall ? CallingWall->GetName() : "NULL";
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("Wall completed but UDigInComponent does not recognize it!"
			"\n Wall: " + COmpletedWallName + "\nOwner: " + OwnerName);
		return false;
	}
	return true;
	
}

bool UDigInComponent::EnsureScalingVectorIsValid(const FVector& InVector) const
{
	const bool bIsDefaultScale = InVector.Equals(FVector::OneVector, 0.01f);
	return (not bIsDefaultScale) && (not InVector.IsNearlyZero());
}
