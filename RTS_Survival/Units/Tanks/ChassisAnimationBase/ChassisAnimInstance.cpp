// Copyright (C) Bas Blokzijl - All rights reserved.


#include "ChassisAnimInstance.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Environment/GroundSurface/GroundSurfaceSubsystem.h"
#include "RTS_Survival/Environment/GroundSurface/RTSGroundSurfaceSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ChassisTrackSmoke
{
	// User parameter of NS_Track_Smokes: RGB tint and opacity scale in A. Each particle copies it into its dynamic
	// material parameter at spawn, so particles already in the air keep the look of the ground they rose from.
	const FName SmokeSurfaceLookParameter = TEXT("SmokeSurfaceLook");
	// Below this difference the eased look snaps to the ground's look and stops updating the Niagara component.
	constexpr float LookSnapTolerance = 0.01f;
}

void UChassisAnimInstance::SetMovementParameters(const float NewSpeed, const float NewRotationYaw,
                                                 const bool bIsMovingForward)
{
	RotationYaw = NewRotationYaw;
	Speed = NewSpeed;
	bM_IsMovingForward = bIsMovingForward;

	PlayRate = Speed / AnimationSpeedDivider;
}

void UChassisAnimInstance::SetChassisAnimToStationaryRotation(const bool bRotateToRight)
{
	// Derived Wheeled Or Track anim instances set the correct animation enum.
}

void UChassisAnimInstance::SetChassisAnimToIdle()
{
	// Derived Wheeled Or Track anim instances set the correct animation enum.
	DisableTrackSmoke();
}

void UChassisAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();
	SetChassisAnimToIdle();
	if (UWorld* World = GetWorld())
	{
		M_GroundSurfaceSubsystem = World->GetSubsystem<UGroundSurfaceSubsystem>();
		M_VfxDel.BindUObject(this, &UChassisAnimInstance::UpdateTrackVFX);
		World->GetTimerManager().SetTimer(M_VfxHandle, M_VfxDel,
		                                  DeveloperSettings::Optimization::UpdateChassisVFXInterval, true);
	}
}

void UChassisAnimInstance::BeginDestroy()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_VfxHandle);
	}
	Super::BeginDestroy();
}

void UChassisAnimInstance::SetupParticleVFX(UNiagaraComponent* ChassisSmokeComponent,
                                            UNiagaraComponent* ExhaustParticleComponent, float MaxExhaustPlayRate,
                                            float MinExhaustPlayRate)
{
	if (not IsValid(ChassisSmokeComponent) || not IsValid(ExhaustParticleComponent))
	{
		FString OwnerName = IsValid(GetOwningActor()) ? GetOwningActor()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("Invalid ChassisSMokeComponent or ExhaustParticleComponent provided for Chassis"
			"anim instance: " + GetName() + " on actor: " + OwnerName);
	}
	if (MaxExhaustPlayRate <= 0)
	{
		MaxExhaustPlayRate = 2;
	}
	if (MinExhaustPlayRate <= 0)
	{
		MinExhaustPlayRate = 1;
	}
	M_TrackParticleComponent = ChassisSmokeComponent;
	M_ExhaustParticleComponent = ExhaustParticleComponent;
	M_MaxExhaustPlayRate = MaxExhaustPlayRate;
	M_MinExhaustPlayRate = MinExhaustPlayRate;
}


void UChassisAnimInstance::UpdateTrackVFX()
{
	if (IsValid(M_TrackParticleComponent))
	{
		if (Speed <= AnimationSpeedDivider)
		{
			// stop the niagara system from rendering (speed too low); idle vehicles skip this once it is off.
			if (M_TrackParticleComponent->IsActive())
			{
				DisableTrackSmoke();
			}
		}
		else
		{
			// Before activating, so the first particles already have the ground's look.
			UpdateTrackSmokeSurfaceLook();
			// enable the niagara system to render (speed high enough)
			if (!M_TrackParticleComponent->IsActive())
			{
				M_TrackParticleComponent->SetVisibility(true);
				M_TrackParticleComponent->Activate(true);
			}
			M_TrackParticleComponent->
				SetVariableFloat(FName("XSmokeSpeedMultiplier"), FMath::Clamp(PlayRate, 1, 3));
		}
	}
	if (IsValid(M_ExhaustParticleComponent))
	{
		const float LocalPlayrate = FMath::Clamp(PlayRate * M_MinExhaustPlayRate, M_MinExhaustPlayRate,
		                                         M_MaxExhaustPlayRate);
		M_ExhaustParticleComponent->
			SetVariableFloat(FName("PlayRate"),
			                 LocalPlayrate);
	}
}

void UChassisAnimInstance::DisableTrackSmoke()
{
	if (not IsValid(M_TrackParticleComponent))
	{
		return;
	}
	M_TrackParticleComponent->Deactivate();
	M_TrackParticleComponent->SetVisibility(false);
	M_TrackParticleComponent->
		SetVariableFloat(FName("XSmokeSpeedMultiplier"), 0);
	M_SmokeSurfaceState.bM_HasAppliedLook = false;
}

void UChassisAnimInstance::UpdateTrackSmokeSurfaceLook()
{
	const AActor* OwningActor = GetOwningActor();
	if (not IsValid(OwningActor) || not IsValid(M_TrackParticleComponent) || not GetIsValidGroundSurfaceSubsystem())
	{
		return;
	}
	const FGroundSurfaceSmokeLook& TargetLook = M_GroundSurfaceSubsystem->GetSmokeLookAt(OwningActor->GetActorLocation());
	FChassisSmokeSurfaceState& State = M_SmokeSurfaceState;
	const URTSGroundSurfaceSettings* Settings = URTSGroundSurfaceSettings::Get();
	const float BlendAlpha = State.bM_HasAppliedLook && Settings != nullptr ? Settings->SmokeLookBlendPerUpdate : 1.f;

	FLinearColor NewTint = FMath::Lerp(State.M_CurrentTint, TargetLook.SmokeTint, BlendAlpha);
	float NewOpacity = FMath::Lerp(State.M_CurrentOpacity, TargetLook.SmokeOpacity, BlendAlpha);
	if (NewTint.Equals(TargetLook.SmokeTint, ChassisTrackSmoke::LookSnapTolerance)
		&& FMath::IsNearlyEqual(NewOpacity, TargetLook.SmokeOpacity, ChassisTrackSmoke::LookSnapTolerance))
	{
		NewTint = TargetLook.SmokeTint;
		NewOpacity = TargetLook.SmokeOpacity;
	}
	// Most updates land here: the vehicle stays on the same ground and the component keeps its values.
	if (State.bM_HasAppliedLook && NewTint == State.M_CurrentTint && NewOpacity == State.M_CurrentOpacity)
	{
		return;
	}
	M_TrackParticleComponent->SetVariableVec4(ChassisTrackSmoke::SmokeSurfaceLookParameter,
	                                          FVector4(NewTint.R, NewTint.G, NewTint.B, NewOpacity));
	State.M_CurrentTint = NewTint;
	State.M_CurrentOpacity = NewOpacity;
	State.bM_HasAppliedLook = true;
}

bool UChassisAnimInstance::GetIsValidGroundSurfaceSubsystem() const
{
	if (M_GroundSurfaceSubsystem.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_GroundSurfaceSubsystem",
		"GetIsValidGroundSurfaceSubsystem",
		GetOwningActor());
	return false;
}
