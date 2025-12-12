#include "ScavengerComponent.h"

#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UScavengerComponent::UScavengerComponent()
	: ScavengeTimeDivider(1)
	  , ScavengeEquipment(nullptr)
	  , M_ScavengeState(EScavengeState::None)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UScavengerComponent::SetScavengeState(const EScavengeState NewState)
{
	M_ScavengeState = NewState;
}

void UScavengerComponent::TeleportAndRotateAtScavSocket(const FVector& InDesiredLocation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "World", __FUNCTION__);
		return;
	}

	World->GetTimerManager().ClearTimer(M_TeleportHandle);

	TWeakObjectPtr<UScavengerComponent> WeakThis(this);
	const FTimerDelegate TimerDel = FTimerDelegate::CreateLambda([WeakThis, InDesiredLocation]()
	{
		if (!WeakThis.IsValid()) return;
		auto* Self = WeakThis.Get();
		if (!Self->GetIsValidTargetScavengeObject() || !Self->GetIsValidOwnerSquadUnit()) return;

		ASquadUnit* Unit = Self->M_OwnerSquadUnit;
		const AScavengeableObject* Obj = Self->M_TargetScavengeObject;

		float CapsuleHalf = 44.f;
		if (UCapsuleComponent* Cap = Unit->GetCapsuleComponent())
			CapsuleHalf = Cap->GetUnscaledCapsuleHalfHeight();

		// Start from the requested socket location
		FVector Desired = InDesiredLocation;

		// Clamp to at least the scav object “ground” height
		Desired.Z = FMath::Max(Desired.Z, Obj->GetActorLocation().Z + CapsuleHalf );

		// Trace down to find actual ground
		FHitResult Hit;
		const FVector Start = Desired + FVector(0, 0, CapsuleHalf );
		const FVector End = Desired - FVector(0, 0, 10000.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(Scav_TeleportGroundSnap), false, Unit);
		if (Unit->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			Desired = Hit.ImpactPoint + FVector(0, 0, CapsuleHalf + 2.f);
		}

		// Set with sweep so we don’t tunnel into geometry
		FHitResult SweepHit;
		Unit->SetActorLocation(Desired, /*bSweep*/ true, &SweepHit);

		// Face the object (yaw only, keep pitch/roll sane)
		const FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(Unit->GetActorLocation(),
		                                                               Obj->GetActorLocation());
		Unit->SetActorRotation(FRotator(0.f, LookAt.Yaw, 0.f));
	});

	World->GetTimerManager().SetTimer(M_TeleportHandle, TimerDel, 2.f, false);
}


void UScavengerComponent::SetTargetScavengeObject(TObjectPtr<AScavengeableObject> ScavengeObject)
{
	M_TargetScavengeObject = ScavengeObject;
}

void UScavengerComponent::UpdateOwnerBlockScavengeObjects(const bool bBlock) const
{
	if (!IsValid(M_OwnerSquadUnit))
	{
		return;
	}

	UCapsuleComponent* Capsule = M_OwnerSquadUnit->GetCapsuleComponent();
	if (!IsValid(Capsule))
	{
		return;
	}

	Capsule->SetCollisionResponseToChannel(ECC_Destructible, bBlock ? ECR_Block : ECR_Ignore);
}

EScavengeState UScavengerComponent::GetScavengeState() const
{
	return M_ScavengeState;
}

void UScavengerComponent::SetSquadOwner(ASquadUnit* OwnerUnit)
{
	if (!IsValid(OwnerUnit))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "M_OwnerSquadUnit", "SetSquadOwner");
		return;
	}
	M_OwnerSquadUnit = OwnerUnit;
}

void UScavengerComponent::StartScavenging()
{
	M_ScavengeState = EScavengeState::Scavenging;
	if (!GetIsValidOwnerSquadUnit() || !GetIsValidTargetScavengeObject())
	{
		return;
	}

	const float ScavengeTime = M_TargetScavengeObject->GetScavengeTime() / ScavengeTimeDivider;
	M_OwnerSquadUnit->OnScavengeStart(ScavengeEquipment, ScavengeSocketName, ScavengeTime, M_TargetScavengeObject,
	                                  ScavengeEffect, ScavengeEffectSocketName);
}

void UScavengerComponent::TerminateScavenging()
{
	// Reset scavenge state and clear target object.
	M_ScavengeState = EScavengeState::None;
	M_TargetScavengeObject = nullptr;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TeleportHandle);
	}

	// Unblock scavengable objects.
	UpdateOwnerBlockScavengeObjects(false);
}

void UScavengerComponent::MoveToLocationComplete()
{
	switch (M_ScavengeState)
	{
	case EScavengeState::None:
		RTSFunctionLibrary::ReportError("ScavengerComponent: MoveToLocationComplete called with state None.");
		break;
	case EScavengeState::MoveToObject:
		HandleMoveToObjectComplete();
		break;
	case EScavengeState::MoveToScavengeLocation:
		StartScavenging();
		break;
	case EScavengeState::Scavenging:
		break;
	default:
		RTSFunctionLibrary::ReportError("ScavengerComponent: Unknown ScavengeState in MoveToLocationComplete.");
		break;
	}
}

void UScavengerComponent::HandleMoveToObjectComplete()
{
	if (!GetIsValidOwnerSquadUnit())
	{
		return;
	}

	if (ASquadController* SquadController = M_OwnerSquadUnit->GetSquadControllerChecked())
	{
		SquadController->OnMoveToScavengeObjectComplete();
	}
}

void UScavengerComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TeleportHandle);
	}
}

bool UScavengerComponent::GetIsValidOwnerSquadUnit()
{
	if (IsValid(M_OwnerSquadUnit))
	{
		return true;
	}

	RTSFunctionLibrary::ReportNullErrorInitialisation(this, "M_OwnerSquadUnit", "GetIsValidOwnerSquadUnit");
	if (AActor* Owner = GetOwner())
	{
		M_OwnerSquadUnit = Cast<ASquadUnit>(Owner);
	}

	return IsValid(M_OwnerSquadUnit);
}

bool UScavengerComponent::GetIsValidTargetScavengeObject() const
{
	if (IsValid(M_TargetScavengeObject))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TargetScavengeObject",
	                                                      "GetIsValidTargetScavengeObject",
	                                                      GetOwner());
	return false;
}
