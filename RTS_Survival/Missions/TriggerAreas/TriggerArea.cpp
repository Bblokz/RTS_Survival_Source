#include "TriggerArea.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ATriggerArea::ATriggerArea()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATriggerArea::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (not BeginPlay_InitTriggerCollisionComponent())
	{
		return;
	}

	if (USphereComponent* SphereComponent = Cast<USphereComponent>(M_TriggerCollisionComponent))
	{
		M_BaseSphereRadius = SphereComponent->GetUnscaledSphereRadius();
	}

	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(M_TriggerCollisionComponent))
	{
		M_BaseBoxExtent = BoxComponent->GetUnscaledBoxExtent();
	}

	M_TriggerCollisionComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ATriggerArea::OnCollisionBeginOverlap);
	M_TriggerCollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ATriggerArea::OnCollisionBeginOverlap);
}

void ATriggerArea::ApplyScaleToCollision(const FVector& Scale)
{
	if (not GetIsValidTriggerCollisionComponent())
	{
		return;
	}

	const FVector AbsoluteScale = Scale.GetAbs();
	if (USphereComponent* SphereComponent = Cast<USphereComponent>(M_TriggerCollisionComponent))
	{
		const float MaxScaleAxis = AbsoluteScale.GetMax();
		SphereComponent->SetSphereRadius(M_BaseSphereRadius * MaxScaleAxis, true);
		return;
	}

	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(M_TriggerCollisionComponent))
	{
		const FVector NewExtent = M_BaseBoxExtent * AbsoluteScale;
		BoxComponent->SetBoxExtent(NewExtent, true);
	}
}

void ATriggerArea::SetupOverlapLogic(const ETriggerOverlapLogic TriggerOverlapLogic)
{
	if (not GetIsValidTriggerCollisionComponent())
	{
		return;
	}

	FRTS_CollisionSetup::SetupTriggerOverlapCollision(M_TriggerCollisionComponent, TriggerOverlapLogic);
}

bool ATriggerArea::BeginPlay_InitTriggerCollisionComponent()
{
	M_TriggerCollisionComponent = FindComponentByClass<USphereComponent>();
	if (not IsValid(M_TriggerCollisionComponent))
	{
		M_TriggerCollisionComponent = FindComponentByClass<UBoxComponent>();
	}

	if (not IsValid(M_TriggerCollisionComponent))
	{
		RTSFunctionLibrary::ReportError(
			"ATriggerArea could not find a sphere or box collision component in PostInitializeComponents.");
		return false;
	}

	return true;
}

bool ATriggerArea::GetIsValidTriggerCollisionComponent() const
{
	if (IsValid(M_TriggerCollisionComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_TriggerCollisionComponent",
		"GetIsValidTriggerCollisionComponent",
		this);
	return false;
}

void ATriggerArea::OnCollisionBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                           AActor* OtherActor,
                                           UPrimitiveComponent* OtherComp,
                                           int32 OtherBodyIndex,
                                           bool bFromSweep,
                                           const FHitResult& SweepResult)
{
	if (not IsValid(OtherActor) || OtherActor == this)
	{
		return;
	}

	OnTriggerAreaOverlap.Broadcast(OtherActor, this);
}
