// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSOptimizer.h"

#include "RTSOptimizationDistance.h"
#include "RTS_Survival/Player/CPPController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


URTSOptimizer::URTSOptimizer()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URTSOptimizer::SetOptimizationEnabled(const bool bEnable)
{
	if (not GetWorld())
	{
		return;
	}
	if (bEnable && GetWorld()->GetTimerManager().IsTimerActive(M_OptimizationTimer))
	{
		RTSFunctionLibrary::PrintString("Optimization already active! Will not enable again.", FColor::Red);
		return;
	}
	if (bEnable)
	{
		const float InitialDelay = FMath::RandRange(0.01f, 0.1f);
		M_OptimizeInterval = FMath::RandRange(0.25f, 0.5f);
		GetWorld()->GetTimerManager().SetTimer(M_OptimizationTimer,
		                                       this,
		                                       &URTSOptimizer::OptimizeTick,
		                                       M_OptimizeInterval,
		                                       true,
		                                       InitialDelay);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(M_OptimizationTimer);
		// Set all components to be as if in FOV.
		InFOVUpdateComponents();
		// reset.
		M_PreviousOptimizationDistance = ERTSOptimizationDistance::None;
	}
}


void URTSOptimizer::BeginPlay()
{
	Super::BeginPlay();
	if (not GetWorld())
	{
		return;
	}
	BeginPlay_SetPlayerController();
	BeginPlay_SetupComponentReferences();
	SetOptimizationEnabled(true);
}

void URTSOptimizer::DetermineOwnerOptimization(AActor* ValidOwner)
{
}

void URTSOptimizer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	SetOptimizationEnabled(false);
}

void URTSOptimizer::BeginDestroy()
{
	Super::BeginDestroy();
	SetOptimizationEnabled(false);
}

void URTSOptimizer::OptimizeTick()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RTSOptimizer_Tick);
	if (not GetOwner())
	{
		return;
	}
	const auto NewOptimizationDistance = GetOptimizationDistanceToCamera();
	if (NewOptimizationDistance == M_PreviousOptimizationDistance)
	{
		return;
	}
	M_PreviousOptimizationDistance = NewOptimizationDistance;
	switch (NewOptimizationDistance)
	{
	case ERTSOptimizationDistance::None:
		break;
	case ERTSOptimizationDistance::InFOV:
		if (DeveloperSettings::Debugging::GOptimComponent_Compile_DebugSymbols)
		{
			DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 300),
			                "In FOV!", nullptr, FColor::Red, M_OptimizeInterval);
		}
		InFOVUpdateComponents();
		break;
	case ERTSOptimizationDistance::OutFOVClose:

		if (DeveloperSettings::Debugging::GOptimComponent_Compile_DebugSymbols)
		{
			DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 300),
			                "Out of FOV close!", nullptr, FColor::Orange, M_OptimizeInterval);
		}
		OutFovCloseUpdateComponents();
		break;
	case ERTSOptimizationDistance::OutFOVFar:
		if (DeveloperSettings::Debugging::GOptimComponent_Compile_DebugSymbols)
		{
			DrawDebugString(GetWorld(), GetOwner()->GetActorLocation() + FVector(0, 0, 300),
			                "Out of FOV far!", nullptr, FColor::Yellow, M_OptimizeInterval);
		}
		OutFovFarUpdateComponents();
		break;
	}
}

void URTSOptimizer::InFOVUpdateComponents()
{
	for (auto EachTicking : M_TickingComponentsNotSkeletal)
	{
		if (not EachTicking.TickingComponent)
		{
			continue;
		}
		EachTicking.TickingComponent->SetComponentTickInterval(EachTicking.BaseTickInterval);
	}
	for (auto EachSkeleton : M_SkeletalMeshComponents)
	{
		if (not EachSkeleton.Skeleton)
		{
			continue;
		}
		EachSkeleton.Skeleton->VisibilityBasedAnimTickOption = EachSkeleton.BasedAnimTickOption;
		EachSkeleton.Skeleton->SetComponentTickEnabled(true);
		EachSkeleton.Skeleton->SetCastContactShadow(EachSkeleton.bCastContactShadow);
		EachSkeleton.Skeleton->SetCastShadow(EachSkeleton.bCastDynamicShadow);
	}
	for (auto EachPrimitive : M_PrimitiveComponents)
	{
		if (not EachPrimitive.PrimitiveComponent)
		{
			continue;
		}
		EachPrimitive.PrimitiveComponent->SetCastContactShadow(EachPrimitive.bCastContactShadow);
		EachPrimitive.PrimitiveComponent->SetCastShadow(EachPrimitive.bCastDynamicShadow);
	}
}

void URTSOptimizer::OutFovCloseUpdateComponents()
{
	for (auto EachTicking : M_TickingComponentsNotSkeletal)
	{
		if (not EachTicking.TickingComponent)
		{
			continue;
		}
		EachTicking.TickingComponent->SetComponentTickInterval(EachTicking.CloseOutFovInterval);
	}

	for (auto EachSkeleton : M_SkeletalMeshComponents)
	{
		if (not EachSkeleton.Skeleton)
		{
			continue;
		}
		EachSkeleton.Skeleton->VisibilityBasedAnimTickOption =
			EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		EachSkeleton.Skeleton->SetComponentTickEnabled(true);
		EachSkeleton.Skeleton->SetCastContactShadow(false);
		EachSkeleton.Skeleton->SetCastShadow(false);
	}
	for (auto EachPrimitive : M_PrimitiveComponents)
	{
		if (not EachPrimitive.PrimitiveComponent)
		{
			continue;
		}
		EachPrimitive.PrimitiveComponent->SetCastContactShadow(false);
		EachPrimitive.PrimitiveComponent->SetCastShadow(false);
	}
}

void URTSOptimizer::OutFovFarUpdateComponents()
{
	for (auto EachTicking : M_TickingComponentsNotSkeletal)
	{
		if (not EachTicking.TickingComponent)
		{
			continue;
		}
		EachTicking.TickingComponent->SetComponentTickInterval(EachTicking.FarOutFovInterval);
	}

	for (auto EachSkeleton : M_SkeletalMeshComponents)
	{
		if (not EachSkeleton.Skeleton)
		{
			continue;
		}
		EachSkeleton.Skeleton->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		EachSkeleton.Skeleton->SetComponentTickEnabled(false);
	}
}

void URTSOptimizer::DetermineCharMovtOptimization(UCharacterMovementComponent* CharacterMovementComponent)
{
	// Used in squad unit optimization.
}

void URTSOptimizer::DetermineChildActorCompOptimization(UChildActorComponent* ChildActorComponent)
{
	if (not ChildActorComponent)
	{
		return;
	}
	AActor* ChildActor = ChildActorComponent->GetChildActor();
	if (not ChildActor)
	{
		return;
	}
	auto Components = ChildActor->GetComponents();
	for (auto EachComponent : Components)
	{
		if (USkeletalMeshComponent* Skeleton = Cast<USkeletalMeshComponent>(EachComponent))
		{
			AddSkeletonToOptimize(Skeleton);
			continue;
		}
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(EachComponent))
		{
			AddPrimitiveComponentToOptimize(Primitive);
		}
		if (EachComponent->IsA(UChildActorComponent::StaticClass()))
		{
			DetermineChildActorCompOptimization(Cast<UChildActorComponent>(EachComponent));
			continue;
		}
		if (EachComponent->PrimaryComponentTick.bCanEverTick)
		{
			TickingComponentDetermineOptimizedTicks(EachComponent);
		}
	}
}

void URTSOptimizer::AddSkeletonToOptimize(USkeletalMeshComponent* Skeleton)
{
	if (not Skeleton)
	{
		return;
	}
	FSkeletonOptimizationSettings Entry;
	Entry.Skeleton = Skeleton;
	Entry.BasedAnimTickOption = Skeleton->VisibilityBasedAnimTickOption;
	Entry.bCastDynamicShadow = Skeleton->bCastDynamicShadow;
	Entry.bCastContactShadow = Skeleton->bCastContactShadow;

	Skeleton->bDisableClothSimulation = true;
	Skeleton->bDisableMorphTarget = true;

	M_SkeletalMeshComponents.Add(Entry);
}

void URTSOptimizer::AddPrimitiveComponentToOptimize(UPrimitiveComponent* PrimitiveComponent)
{
	if (not PrimitiveComponent)
	{
		return;
	}
	FRTSOptimizePrimitive Entry;
	Entry.PrimitiveComponent = PrimitiveComponent;
	Entry.bCastContactShadow = PrimitiveComponent->bCastContactShadow;
	Entry.bCastDynamicShadow = PrimitiveComponent->bCastDynamicShadow;
	M_PrimitiveComponents.Add(Entry);
}


ERTSOptimizationDistance URTSOptimizer::GetOptimizationDistanceToCamera() const
{
	if (!M_PlayerController || !(M_PlayerController->PlayerCameraManager))
	{
		return ERTSOptimizationDistance::InFOV;
	}
	
	FVector CamLoc;
	FRotator CamRotation;
	M_PlayerController->GetPlayerViewPoint(CamLoc, CamRotation);
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const FVector ToActor = (OwnerLocation - CamLoc).GetSafeNormal();
	const FVector Forward = CamRotation.Vector();
	
	// half-angle in radians
	const float HalfFovRad = FMath::DegreesToRadians(M_PlayerController->PlayerCameraManager->GetFOVAngle() * 0.5f);
	const float CosHalfFov = FMath::Cos(HalfFovRad);
	
	// if the dot-product is above cos(halfFov), actor is inside the view cone
	const bool InFOV = FVector::DotProduct(Forward, ToActor) >= CosHalfFov;
	
	const float SquaredDistanceToCamera = FVector::DistSquared(OwnerLocation, CamLoc);
	if (not InFOV)
	{
		if (SquaredDistanceToCamera <= DeveloperSettings::Optimization::DistanceAlwaysConsiderUnitInFOVSquared)
		{
			return ERTSOptimizationDistance::InFOV;
		}
		if (SquaredDistanceToCamera >= DeveloperSettings::Optimization::OutOfFOVConsideredFarAwayUnitSquared)
		{
			return ERTSOptimizationDistance::OutFOVFar;
		}
		return ERTSOptimizationDistance::OutFOVClose;
	}
	return ERTSOptimizationDistance::InFOV;
}

void URTSOptimizer::BeginPlay_SetPlayerController()
{
	M_PlayerController = FRTS_Statics::GetRTSController(this);
	if (not M_PlayerController)
	{
		RTSFunctionLibrary::ReportError("Failed to set player controller for URTSOptimizer!");
	}
}

void URTSOptimizer::BeginPlay_SetupComponentReferences()
{
	if (not GetOwner())
	{
		return;
	}
	DetermineOwnerOptimization(GetOwner());
	auto AllComponents = GetOwner()->GetComponents();
	for (auto EachComponent : AllComponents)
	{
		if (USkeletalMeshComponent* Skeleton = Cast<USkeletalMeshComponent>(EachComponent))
		{
			AddSkeletonToOptimize(Skeleton);
			continue;
		}
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(EachComponent))
		{
			AddPrimitiveComponentToOptimize(Primitive);
			// Do not continue as this component may tick too.
		}
		if (EachComponent->IsA(UChildActorComponent::StaticClass()))
		{
			DetermineChildActorCompOptimization(Cast<UChildActorComponent>(EachComponent));
			continue;
		}
		// Check if component is ticking.
		if (EachComponent->PrimaryComponentTick.bCanEverTick)
		{
			TickingComponentDetermineOptimizedTicks(EachComponent);
		}
	}
	if (DeveloperSettings::Debugging::GOptimComponent_Compile_DebugSymbols)
	{
		const FString OwnerName = GetOwner()->GetName();
		FString Message = "\n\n" + OwnerName + "Found the following ticking components:";
		for (const auto EachComp : M_TickingComponentsNotSkeletal)
		{
			Message += "\n - " + EachComp.TickingComponent->GetName() + " with tick interval: " +
				FString::SanitizeFloat(EachComp.BaseTickInterval);
		}
		Message += "\n Found the following skeletal mesh components:";
		for (const auto EachSkeletal : M_SkeletalMeshComponents)
		{
			const FString TickOptionEnumAsString = UEnum::GetValueAsString(
				EachSkeletal.Skeleton->VisibilityBasedAnimTickOption);
			Message += "\n - " + EachSkeletal.Skeleton->GetName() + " with tick option: " +
				TickOptionEnumAsString;
		}
		Message += "\n Found the following primitive components:";
		for (const auto EachPrimitive : M_PrimitiveComponents)
		{
			Message += "\n - " + EachPrimitive.PrimitiveComponent->GetName();
		}
		RTSFunctionLibrary::PrintString(Message);
	}
}

void URTSOptimizer::TickingComponentDetermineOptimizedTicks(UActorComponent* TickingComponent)
{
	FRTSTickingComponent Entry;
	const bool bIsCharacterMovement = TickingComponent->IsA<UCharacterMovementComponent>();
	const float TickTimeBase = TickingComponent->GetComponentTickInterval();
	Entry.TickingComponent = TickingComponent;
	Entry.BaseTickInterval = TickTimeBase;
	if (bIsCharacterMovement)
	{
		DetermineCharMovtOptimization(Cast<UCharacterMovementComponent>(TickingComponent));
		return;
	}
	if (FMath::IsNearlyZero(TickTimeBase))
	{
		// A nearly zero tick time identifies the component as important.
		Entry.CloseOutFovInterval = DeveloperSettings::Optimization::ImportantTickCompOutOfFOVCloseTickRate;
		Entry.FarOutFovInterval = DeveloperSettings::Optimization::ImportantTickCompOutOfFOVFarTickRate;
		M_TickingComponentsNotSkeletal.Add(Entry);
		return;
	}
	// If the tick time is not zero, we can use the tick time to determine the intervals.
	Entry.CloseOutFovInterval = TickTimeBase * DeveloperSettings::Optimization::OutofFOVCloseTickTimeMlt;
	Entry.FarOutFovInterval = TickTimeBase * DeveloperSettings::Optimization::OutofFOVFarTickTimeMlt;
	M_TickingComponentsNotSkeletal.Add(Entry);
}
