// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "FowComp.h"

#include "FowType.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


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

	if (bStartImmediately)
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
	AActor* FowOwner = GetOwner();
	if (not IsValid(FowOwner))
	{
		return;
	}
	if (M_FowBehaviour == EFowBehaviour::Fow_Active)
	{
		const FName VisionTag = DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
		SetShouldDrawMiniMapIcon(not FowOwner->IsHidden());
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

	SetShouldDrawMiniMapIcon(Visibility >= VisibleEnoughToRevealEnemy);
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
	BeginPlay_InitMiniMapIconSettings();
}

void UFowComp::CacheMiniMapWorldLocation(const FVector& WorldLocation)
{
	M_MiniMapRuntimeState.M_WorldLocation = WorldLocation;
	M_MiniMapRuntimeState.bM_HasCachedWorldLocation = true;
}

void UFowComp::SetShouldDrawMiniMapIcon(const bool bShouldDrawMiniMapIcon)
{
	M_MiniMapRuntimeState.bM_ShouldDrawMiniMapIcon = bShouldDrawMiniMapIcon;
}

void UFowComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	StopFowParticipation();
	if (not GetWorld() || not M_FowParticpationHandle.IsValid())
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

void UFowComp::UpdateMiniMapIconSizeFromFowManager()
{
	if (not GetIsValidFowManager())
	{
		return;
	}

	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	if (bM_ShouldUseTinyMiniMapIcon)
	{
		M_MinimapIconSettings.M_IconSizePixels = M_FowManager->GetMiniMapTinyIconSizePixels();
		return;
	}

	if (M_UnitType == EAllUnitType::UNType_None)
	{
		return;
	}

	M_MinimapIconSettings.M_IconSizePixels = FRTSMinimapIconHelpers::GetMiniMapIconSizePixels(
		*M_FowManager.Get(),
		OwnerActor,
		M_UnitType,
		M_UnitSubtype);
}

void UFowComp::BeginPlay_InitMiniMapIconSettings()
{
	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	bM_ShouldUseTinyMiniMapIcon = OwnerActor->IsA(ASquadUnit::StaticClass());
	URTSComponent* const RTSComponent = OwnerActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		UpdateMiniMapIconSizeFromFowManager();
		return;
	}

	UpdateMiniMapIconSettingsFromOwnerRTSComponent();
	RTSComponent->OnSubTypeInitialized.AddUObject(this, &UFowComp::UpdateMiniMapIconSettingsFromOwnerRTSComponent);
}

void UFowComp::UpdateMiniMapIconSettingsFromOwnerRTSComponent()
{
	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	URTSComponent* const RTSComponent = OwnerActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return;
	}

	M_UnitType = RTSComponent->GetUnitType();
	M_UnitSubtype = RTSComponent->GetUnitSubType();
	M_MinimapIconSettings.M_IconColor = FRTSMinimapIconHelpers::GetMiniMapIconColor(
		*RTSComponent,
		M_UnitType,
		M_UnitSubtype);
	UpdateMiniMapIconSizeFromFowManager();
}

bool UFowComp::GetShouldDrawMiniMapIcon() const
{
	if (M_MinimapIconSettings.M_IconSizePixels <= 0.0f)
	{
		return false;
	}

	if (not M_MiniMapRuntimeState.bM_HasCachedWorldLocation)
	{
		return false;
	}

	return M_MiniMapRuntimeState.bM_ShouldDrawMiniMapIcon;
}

void UFowComp::OnFowStartParticipation()
{
	if (not IsActive())
	{
		Activate();
	}
	if (GetIsValidFowManager())
	{
		UpdateMiniMapIconSizeFromFowManager();
		AActor* const OwnerActor = GetOwner();
		if (IsValid(OwnerActor))
		{
			CacheMiniMapWorldLocation(OwnerActor->GetActorLocation());
			SetShouldDrawMiniMapIcon(M_FowBehaviour == EFowBehaviour::Fow_Active && not OwnerActor->IsHidden());
		}
		M_FowManager->AddFowParticipant(this);
	}
}
