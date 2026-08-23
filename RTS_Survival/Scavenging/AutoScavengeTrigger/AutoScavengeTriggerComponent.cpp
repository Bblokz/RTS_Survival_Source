#include "AutoScavengeTriggerComponent.h"

#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace AutoScavengeTriggerConstants
{
	constexpr float DefaultTriggerRadius = 1000.0f;
	constexpr uint8 PlayerOneId = 1;
}

UAutoScavengeTriggerComponent::UAutoScavengeTriggerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InitSphereRadius(AutoScavengeTriggerConstants::DefaultTriggerRadius);
	FRTS_CollisionSetup::SetupTriggerOverlapCollision(this, ETriggerOverlapLogic::OverlapPlayer);
}

void UAutoScavengeTriggerComponent::BeginPlay()
{
	Super::BeginPlay();

	M_ScavengeableOwner = Cast<AScavengeableObject>(GetOwner());
	if (not GetIsValidScavengeableOwner())
	{
		SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	FRTS_CollisionSetup::SetupTriggerOverlapCollision(this, ETriggerOverlapLogic::OverlapPlayer);
	OnComponentBeginOverlap.RemoveDynamic(this, &UAutoScavengeTriggerComponent::OnTriggerBeginOverlap);
	OnComponentBeginOverlap.AddDynamic(this, &UAutoScavengeTriggerComponent::OnTriggerBeginOverlap);
}

void UAutoScavengeTriggerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnComponentBeginOverlap.RemoveDynamic(this, &UAutoScavengeTriggerComponent::OnTriggerBeginOverlap);
	M_ScavengeableOwner.Reset();

	Super::EndPlay(EndPlayReason);
}

bool UAutoScavengeTriggerComponent::GetIsValidScavengeableOwner() const
{
	if (M_ScavengeableOwner.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ScavengeableOwner",
		"UAutoScavengeTriggerComponent::GetIsValidScavengeableOwner",
		this);
	return false;
}

ASquadController* UAutoScavengeTriggerComponent::ResolveOverlappingSquadController(AActor* OverlappingActor) const
{
	ASquadUnit* SquadUnit = Cast<ASquadUnit>(OverlappingActor);
	if (not IsValid(SquadUnit))
	{
		return Cast<ASquadController>(OverlappingActor);
	}

	return SquadUnit->GetSquadControllerChecked();
}

void UAutoScavengeTriggerComponent::OnTriggerBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComponent*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (not IsValid(OtherActor) || OtherActor == GetOwner() || not GetIsValidScavengeableOwner())
	{
		return;
	}

	AScavengeableObject* ScavengeableOwner = M_ScavengeableOwner.Get();
	if (not ScavengeableOwner->GetCanScavenge())
	{
		return;
	}

	ASquadController* SquadController = ResolveOverlappingSquadController(OtherActor);
	if (not IsValid(SquadController))
	{
		return;
	}

	const URTSComponent* RTSComponent = SquadController->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent) || RTSComponent->GetOwningPlayer() != AutoScavengeTriggerConstants::PlayerOneId)
	{
		return;
	}

	if (not SquadController->GetIsScavenger() || not SquadController->HasAbility(EAbilityID::IdScavenge))
	{
		return;
	}

	if (SquadController->GetActiveCommandID() == EAbilityID::IdScavenge
		&& SquadController->GetTargetScavengeObject() == ScavengeableOwner)
	{
		return;
	}

	constexpr bool bSetUnitToIdle = true;
	SquadController->ScavengeObject(ScavengeableOwner, bSetUnitToIdle);
}
