#include "FImpulseOnCrushed.h"

#include "Field/FieldSystemComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSDebugBreak/RTSDebugBreak.h"

#include "Field/FieldSystemObjects.h"
#include "Field/FieldSystemTypes.h"
#include "RTS_Survival/DeveloperSettings.h"


void FImpulseOnCrushed::Init(UPrimitiveComponent* TargetPrimitive, const float CrushImpulseScale,
                             const float TimeTillImpulse)
{
	M_TargetPrimitive = TargetPrimitive;
	M_CrushImpulseScale = CrushImpulseScale;
	M_TimeTillImpulse = TimeTillImpulse;
	bM_IsImpulseSet = false;
	bM_IsInitialized = true;
}


void FImpulseOnCrushed::QueueImpulseFromOverlap(UPrimitiveComponent* const OverlappedComponent,
                                                UPrimitiveComponent* const OtherComp,
                                                const bool bFromSweep,
                                                const FHitResult& SweepResult,
                                                UWorld* const World)
{
	if (bM_IsImpulseSet)
	{
		return;
	}
	if (World == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("FImpulseOnCrushed::QueueImpulseFromOverlap: World is null."));
		return;
	}

	const TWeakObjectPtr<UPrimitiveComponent> WeakTarget(M_TargetPrimitive);
	const TWeakObjectPtr<UPrimitiveComponent> WeakOther(OtherComp);
	const bool bWasSweep = bFromSweep;
	const FHitResult SweepCopy = SweepResult; // copy by value
	const float ScaleCopy = M_CrushImpulseScale;

	FTimerDelegate Delegate;
	Delegate.BindLambda([WeakTarget, WeakOther, bWasSweep, SweepCopy, ScaleCopy]()
	{
		UPrimitiveComponent* const Target = WeakTarget.Get();
		if (not IsValid(Target))
		{
			RTSFunctionLibrary::ReportError(TEXT("Crush impulse: target primitive is no longer valid."));
			return;
		}
		if (not Target->IsSimulatingPhysics())
		{
			RTSFunctionLibrary::ReportError(TEXT("Crush impulse: target primitive is not simulating physics."));
			return;
		}

		const FVector Direction = ComputeImpulseDirection(Target, WeakOther.Get(), bWasSweep, SweepCopy);
		if (Direction.IsNearlyZero())
		{
			return;
		}
		const float Magnitude = ComputeImpulseMagnitude(Target, WeakOther.Get(), ScaleCopy);
		if (Magnitude <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const FVector AtLocation = ComputeImpactPoint(Target, bWasSweep, SweepCopy);
		if constexpr (DeveloperSettings::Debugging::GCrushableActors_Compile_DebugSymbols)
		{
			// Debug arrow 100 units red from start in direction:
			DrawDebugDirectionalArrow(Target->GetWorld(), AtLocation, AtLocation + Direction * 100.f, 10.f, FColor::Red,
			                          false, 5.f, 0, 2.f);
			const FString Msg = FString::Printf(
				TEXT("Crush Impulse: Dir %s, Mag %.1f"), *Direction.ToString(), Magnitude);
			DrawDebugString(Target->GetWorld(), AtLocation + Direction * 100.f, Msg, nullptr, FColor::White, 5.f);
		}


		// use Chaos Field System instead of AddImpulseAtLocation
		FImpulseOnCrushed::ApplyImpulseField(Target, Direction, Magnitude, AtLocation);
	});

	World->GetTimerManager().SetTimer(M_ImpulseHandle, Delegate, FMath::Max(0.f, M_TimeTillImpulse), false);
	bM_IsImpulseSet = true;
}

FVector FImpulseOnCrushed::ComputeImpactPoint(UPrimitiveComponent* const Target,
                                              const bool bFromSweep,
                                              const FHitResult& SweepResult)
{
	if (bFromSweep && SweepResult.bBlockingHit)
	{
		return SweepResult.ImpactPoint;
	}
	return IsValid(Target) ? Target->GetComponentLocation() : FVector::ZeroVector;
}

void FImpulseOnCrushed::ApplyImpulseField(UPrimitiveComponent* const Target,
                                          const FVector& Direction,
                                          const float Magnitude,
                                          const FVector& AtLocation)
{
	if (not IsValid(Target))
	{
		return;
	}

	AActor* const OwningActor = Target->GetOwner();
	if (not IsValid(OwningActor))
	{
		RTSFunctionLibrary::ReportError(TEXT("Crush impulse: target has no valid owning actor."));
		return;
	}

	// Reuse or create FieldSystemComponent
	UFieldSystemComponent* FieldComp = OwningActor->FindComponentByClass<UFieldSystemComponent>();
	if (not IsValid(FieldComp))
	{
		FieldComp = NewObject<UFieldSystemComponent>(OwningActor);
		if (not IsValid(FieldComp))
		{
			RTSFunctionLibrary::ReportError(TEXT("Crush impulse: failed to create UFieldSystemComponent."));
			return;
		}
		FieldComp->RegisterComponent();
		if (USceneComponent* const Root = OwningActor->GetRootComponent())
		{
			FieldComp->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	const float BoundsBasedRadius = Target->Bounds.SphereRadius * 0.75f;
	const float Radius = FMath::Clamp(BoundsBasedRadius, 50.f, 200.f);

	// Vector force
	UUniformVector* const UniformForce = NewObject<UUniformVector>(FieldComp);
	if (not IsValid(UniformForce))
	{
		return;
	}
	UniformForce->Magnitude = Magnitude;
	UniformForce->Direction = Direction.GetSafeNormal();

	// Soft/graded mask (compatible with Field_LinearForce)
	URadialFalloff* const Falloff = NewObject<URadialFalloff>(FieldComp);
	if (not IsValid(Falloff)) { return; }

	Falloff->Magnitude = 1.0f; // scalar multiplier
	Falloff->MinRange = 0.0f;
	Falloff->MaxRange = Radius; // fade to 0 at Radius
	Falloff->Default = 0.0f; // outside = 0
	Falloff->Radius = Radius;
	Falloff->Position = AtLocation;
	Falloff->Falloff = EFieldFalloffType::Field_Falloff_Linear;

	UOperatorField* const Multiply = NewObject<UOperatorField>(FieldComp);
	if (not IsValid(Multiply)) { return; }
	Multiply->LeftField = UniformForce; // FVector
	Multiply->RightField = Falloff; // float
	Multiply->Operation = EFieldOperationType::Field_Multiply;

	FieldComp->ApplyPhysicsField(
		true,
		EFieldPhysicsType::Field_LinearForce,
		nullptr,
		Multiply
	);
}


FVector FImpulseOnCrushed::ComputeImpulseDirection(UPrimitiveComponent* const Target,
                                                   UPrimitiveComponent* const OtherComp,
                                                   const bool bFromSweep,
                                                   const FHitResult& SweepResult)
{
	if (bFromSweep && SweepResult.bBlockingHit)
	{
		// Push away from the collider: opposite of the hit surface normal.
		return (-SweepResult.ImpactNormal).GetSafeNormal();
	}

	if (IsValid(OtherComp))
	{
		const FVector OtherVel = OtherComp->GetComponentVelocity();
		if (!OtherVel.IsNearlyZero())
		{
			return OtherVel.GetSafeNormal();
		}

		// Fallback: away from the other component’s location.
		if (IsValid(Target))
		{
			const FVector Away = (Target->GetComponentLocation() - OtherComp->GetComponentLocation()).GetSafeNormal();
			if (!Away.IsNearlyZero())
			{
				return Away;
			}
		}
	}

	// Last resort: no direction.
	return FVector::ZeroVector;
}

float FImpulseOnCrushed::ComputeImpulseMagnitude(UPrimitiveComponent* const Target,
                                                 UPrimitiveComponent* const OtherComp,
                                                 const float Scale)
{
	const FVector SelfVel = IsValid(Target) ? Target->GetComponentVelocity() : FVector::ZeroVector;
	const FVector OtherVel = IsValid(OtherComp) ? OtherComp->GetComponentVelocity() : FVector::ZeroVector;
	const float RelativeSpeed = (OtherVel - SelfVel).Size();
	const float Mass = IsValid(Target) ? Target->GetMass() : 0.f;

	return FMath::Max(0.f, RelativeSpeed * Mass * FMath::Max(0.f, Scale));
}
