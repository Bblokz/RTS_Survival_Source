// Copyright (C) Bas Blokzijl - All rights reserved.


#include "ChassisAnimInstance.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

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
}

void UChassisAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();
	SetChassisAnimToIdle();
	if (const UWorld* World = GetWorld())
	{
		M_VfxDel.BindUObject(this, &UChassisAnimInstance::UpdateTrackVFX);
		World->GetTimerManager().SetTimer(M_VfxHandle, M_VfxDel,
		                                  DeveloperSettings::Optimization::UpdateChassisVFXInterval, true);
	}
	
}

void UChassisAnimInstance::BeginDestroy()
{
	if(const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_VfxHandle);
	}
	Super::BeginDestroy();
}

void UChassisAnimInstance::SetupParticleVFX(UNiagaraComponent* ChassisSmokeComponent,
                                            UNiagaraComponent* ExhaustParticleComponent, float MaxExhaustPlayRate, float MinExhaustPlayRate)
{
	if(not IsValid(ChassisSmokeComponent) || not IsValid(ExhaustParticleComponent))
	{
		FString OwnerName  = IsValid(GetOwningActor()) ? GetOwningActor()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("Invalid ChassisSMokeComponent or ExhaustParticleComponent provided for Chassis"
								  "anim instance: " + GetName() + " on actor: " + OwnerName);
	}
	if(MaxExhaustPlayRate <=0 )
	{
		MaxExhaustPlayRate = 2;
	}
	if(MinExhaustPlayRate <=0 )
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
			// stop the niagara system from rendering (speed too low)
			M_TrackParticleComponent->Deactivate();
		}
		else
		{
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
