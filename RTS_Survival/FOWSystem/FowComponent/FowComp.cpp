// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "FowComp.h"

#include "FowType.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


UFowComp::UFowComp(): VisionRadius(0), M_FowManager(nullptr), M_FowParticpationHandle(),
                      M_FowBehaviour(EFowBehaviour::Fow_Passive)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFowComp::SetVisionRadius(const float NewVisionRadius)
{
	if (NewVisionRadius <= 0)
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "NoOwner";
		RTSFunctionLibrary::ReportError("Vision radius set to 0 or less! for component: " + GetName() +
			"With owner: " + OwnerName);
		return;
	}
	VisionRadius = NewVisionRadius;
}


void UFowComp::StartFow(const EFowBehaviour FowComponentBehaviour, const bool bStartImmediately)
{
	M_FowBehaviour = FowComponentBehaviour;

	if(bStartImmediately)
	{
		OnFowStartParticipation();
		return;
	}
	
	// As there is a delay between when the unit gets loaded and when it is actually selectable for the player we wait
	// a bit before we start participating in the FOW system.
	constexpr float TimeTillActivateFow = DeveloperSettings::GamePlay::Training::TimeRemainingStartAsyncLoading + 2.0f;
	StartFowParticipationInSeconds(TimeTillActivateFow);
}

void UFowComp::OnFowVisibilityUpdated(const float Visibility)
{
	using DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	using DeveloperSettings::GamePlay::FoW::VisibleEnoughToRevealEnemy;
	using DeveloperSettings::GamePlay::FoW::VisibleEnoughToRevealPlayer;
	if (AActor* FowOwner = GetOwner(); IsValid(GetOwner()))
	{
		if (M_FowBehaviour == EFowBehaviour::Fow_Active)
		{
			const FName VisionTag = DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
			if (Visibility >= VisibleEnoughToRevealPlayer)
			{
				if (not FowOwner->Tags.Contains(TagUnitInEnemyVision))
				{
					FowOwner->Tags.Add(TagUnitInEnemyVision);
				}
			}
			else
			{
				FowOwner->Tags.Remove(TagUnitInEnemyVision);
			}
			if constexpr (DeveloperSettings::Debugging::GFowComponents_Compile_DebugSymbols)
			{
				FVector DrawTextLocation = FowOwner->GetActorLocation() + FVector(0, 0, 300);
				DrawDebugString(GetWorld(), DrawTextLocation, FString::Printf(TEXT("PlayerVis:: %f"), Visibility),
				                nullptr,
				                FColor::White, 2.0f, false);
			}
			return;
		}

		if (Visibility >= VisibleEnoughToRevealEnemy)
		{
			FowOwner->SetActorHiddenInGame(false);
			if constexpr (DeveloperSettings::Debugging::GFowComponents_Compile_DebugSymbols)
			{
				FVector DrawTextLocation = FowOwner->GetActorLocation() + FVector(0, 0, 300);
				DrawDebugString(GetWorld(), DrawTextLocation, FString::Printf(TEXT("Visibility > .15! %f"), Visibility),
				                nullptr, FColor::Red, 1.0f, false);
			}
			return;
		}
		FowOwner->SetActorHiddenInGame(true);
		if constexpr (DeveloperSettings::Debugging::GFowComponents_Compile_DebugSymbols)
		{
			FVector DrawTextLocation = FowOwner->GetActorLocation() + FVector(0, 0, 300);
			DrawDebugString(GetWorld(), DrawTextLocation, FString::Printf(TEXT("Visibility: %f"), Visibility), nullptr,
			                FColor::White, 2.0f, false);
		}
	}
}

void UFowComp::StartFowParticipationInSeconds(const float Seconds)
{
	if (not GetWorld())
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(M_FowParticpationHandle, this, &UFowComp::OnFowStartParticipation, Seconds,
	                                       false);
}

void UFowComp::StopFowParticipation()
{
	if (GetIsValidFowManager())
	{
		M_FowManager->RemoveFowParticipant(this);
	}
}


void UFowComp::BeginPlay()
{
	Super::BeginPlay();

	M_FowManager = FRTS_Statics::GetFowManager(this);
}

void UFowComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	StopFowParticipation();
	if (not GetWorld() || !M_FowParticpationHandle.IsValid())
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(M_FowParticpationHandle);
}


bool UFowComp::GetIsValidFowManager()
{
	if (M_FowManager.IsValid())
	{
		return true;
	}
	M_FowManager = FRTS_Statics::GetFowManager(this);
	return M_FowManager.IsValid();
}

void UFowComp::OnFowStartParticipation()
{
	if (not IsActive())
	{
		Activate();
	}
	if (GetIsValidFowManager())
	{
		M_FowManager->AddFowParticipant(this);
	}
}
