// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "FowManager.h"

#include "NiagaraComponent.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/FOWSystem/FowVisibilityInterface/FowVisibility.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values
AFowManager::AFowManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	// Start ticking after we have components registered.
	PrimaryActorTick.bStartWithTickEnabled = false;
	bIsInitialized = false;
	bIsStarted = false;
	bM_IsPendingReadBackAnswer = false;
	// Settings
	PendingStartLimit = 30;
	NiagaraDrawBufferName = "";
	MapExtent = 0.0f;
	RenderTargetSize = 0;
	FowTickRate = 0.1f;
	FowSystem = nullptr;
	FoWReadBackSystem = nullptr;
	NiagaraDraw = nullptr;
	NiagaraReadBack = nullptr;

	M_ActiveFowComponents = {};
	M_DrawBuffer = {};
	bM_IsPendingEnemyVisionUpdate = false;
	M_StartupAttempts = 0;
}

void AFowManager::AddFowParticipant(UFowComp* FowComp)
{
	if (!IsValid(FowComp))
	{
		RTSFunctionLibrary::ReportError("Invalid Fow component supplied to fow manager, will not add this fow"
			"comp to participate!"
			"\n See function: AFowManager::AddFowParticipant()");
		return;
	}
	switch (FowComp->GetFowBehaviour())
	{
	case EFowBehaviour::Fow_Active:
		AddActiveFowParticipant(FowComp);
		return;
	case EFowBehaviour::Fow_Passive:
		AddPassiveFowParticipant(FowComp);
		return;
	case EFowBehaviour::Fow_PassiveEnemyVision:
		AddPassiveFowParticipant(FowComp);
		return;
	}
	AddPassiveFowParticipant(FowComp);
}

void AFowManager::RemoveFowParticipant(UFowComp* FowComp)
{
	if (not IsValid(FowComp))
	{
		return;
	}
	const bool bIsActive = FowComp->GetFowBehaviour() == EFowBehaviour::Fow_Active;
	if (bIsActive && not M_ActiveFowComponents.Contains(FowComp))
	{
		return;
	}
	if (not bIsActive && not M_PassiveFowComponents.Contains(FowComp))
	{
		return;
	}
	bIsActive ? M_ActiveFowComponents.Remove(FowComp) : M_PassiveFowComponents.Remove(FowComp);
	const int32 NewLength = M_ActiveFowComponents.Num();
	M_DrawBuffer.SetNum(NewLength);
	if (NewLength <= 0 && IsActorTickEnabled())
	{
		StopFow();
	}
}

void AFowManager::ReceiveParticleData_Implementation(const TArray<FBasicParticleData>& Data,
                                                     UNiagaraSystem* NiagaraSystem,
                                                     const FVector& SimulationPositionOffset)
{
	if (NiagaraSystem == FoWReadBackSystem)
	{
		OnReceivedReadBackUpdate(Data);
		return;
	}
	if (NiagaraSystem == FowEnemyVisionSystem)
	{
		OnReceivedEnemyVisionUpdate(Data);
		return;
	}
	RTSFunctionLibrary::ReportError("unknown Niagara system is providing data to the FowManager!"
		"\n See function: AFowManager::ReceiveParticleData_Implementation()");
}

void AFowManager::RequestComponentFogVisibility(UFowComp* RequestingComponent)
{
	if (not NiagaraReadBack)
	{
		return;
	}
	if (not IsValid(RequestingComponent) || not RequestingComponent->Implements<UFowVisibility>())
	{
		const FName ActorName = IsValid(RequestingComponent) ? RequestingComponent->GetFName() : FName("InvalidActor");
		RTSFunctionLibrary::PrintString(
			"Will not check actor visibility, actor is not valid or does not implement IFowVisibility!"
			"\n Actor requested visiblity update: " + ActorName.ToString()
			+ "\n See function: AFowManager::RequestActorFogVisibility()");
		return;
	}
	if (M_ComponentsRequestingFogVisibility.Contains(RequestingComponent))
	{
		RTSFunctionLibrary::PrintString("Actor already requested fog visibility update, will not request again!"
			"\n Actor requested visiblity update: " + RequestingComponent->GetFName().ToString()
			+ "\n See function: AFowManager::RequestActorFogVisibility()");
		return;
	}
	M_ComponentsRequestingFogVisibility.Add(RequestingComponent);
}

void AFowManager::RemoveComponentFromVisibilityRequest(UFowComp* Component)
{
	if (M_ComponentsRequestingFogVisibility.Contains(Component))
	{
		M_ComponentsRequestingFogVisibility.Remove(Component);
	}
}

UTextureRenderTarget2D* AFowManager::GetIsValidActiveRT() const
{
	if(not IsValid(ActiveRT))
	{
		RTSFunctionLibrary::ReportError("Invalid Active RT On Fow Manager! Cannot provide requested RT!");
		return nullptr;
	}
	return ActiveRT;
}

UTextureRenderTarget2D* AFowManager::GetIsValidPassiveRT() const
{
	if(not IsValid(PassiveRT))
	{
		RTSFunctionLibrary::ReportError("Invalid Passive RT On Fow Manager! Cannot provide requested RT!");
		return nullptr;
	}
	return PassiveRT;
}

// Called when the game starts or when spawned
void AFowManager::BeginPlay()
{
	Super::BeginPlay();
}

void AFowManager::BeginDestroy()
{
	Super::BeginDestroy();
}

void AFowManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Ensure passive and active components are valid.
	EnsureComponentsAreValid();
	UpdateDrawBuffer();
	AskUpdateEnemyVision();
	AskReadBack();
}

bool AFowManager::GetIsActiveParticipantUnique(UFowComp* FowComp) const
{
	if (M_ActiveFowComponents.Contains(FowComp))
	{
		return false;
	}
	return true;
}

bool AFowManager::GetIsPassiveParticipantUnique(UFowComp* FowComp) const
{
	if (M_PassiveFowComponents.Contains(FowComp))
	{
		return false;
	}
	return true;
}


void AFowManager::StartFow()
{
	if (bIsInitialized)
	{
		SetActorTickInterval(FowTickRate);
		SetActorTickEnabled(true);
		bIsStarted = true;
		M_StartupAttempts = 0;
	}
	else
	{
		if (M_StartupAttempts >= PendingStartLimit)
		{
			RTSFunctionLibrary::ReportError("Fow Manager failed to start the Fog of War subsystem, "
				"pending start limit reached!"
				"\n See function: AFowManager::StartFow()");
			return;
		}
		if (const UWorld* World = GetWorld())
		{
			M_StartupAttempts++;
			const FTimerDelegate TimerDel = FTimerDelegate::CreateUObject(this, &AFowManager::StartFow);
			World->GetTimerManager().SetTimerForNextTick(TimerDel);
		}
	}
}

void AFowManager::StopFow()
{
	if (not bIsStarted)
	{
		return;
	}
	bIsStarted = false;
	SetActorTickEnabled(false);
	const TArray<FVector> EmptyArray = {};
	if (IsValid(NiagaraDraw))
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraDraw, FName(*NiagaraDrawBufferName), EmptyArray);
	}
	const TArray<FVector2D> EmptyArray2d = {};
	if (IsValid(NiagaraReadBack))
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
			NiagaraReadBack, FName(*NiagaraReadBackBufferName), EmptyArray2d);
		const FName EnabledName = FName("bEnabled");
		NiagaraReadBack->SetVariableBool(EnabledName, false);
	}
}

void AFowManager::AddActiveFowParticipant(UFowComp* FowComp)
{
	if (!GetIsActiveParticipantUnique(FowComp))
	{
		RTSFunctionLibrary::ReportError("Fow component already participating in fow manager, will not add this fow"
			"comp to partcipate again!"
			"\n See function: AFowManager::AddFowParticipant()");
		return;
	}
	const int32 PreviousLenght = M_ActiveFowComponents.Num();
	M_ActiveFowComponents.Add(FowComp);
	const int32 NewLenght = M_ActiveFowComponents.Num();
	M_DrawBuffer.SetNum(NewLenght);
	if (PreviousLenght <= 0 && NewLenght > 0 && not bIsStarted)
	{
		StartFow();
	}
}

void AFowManager::AddPassiveFowParticipant(UFowComp* FowComp)
{
	if (not GetIsPassiveParticipantUnique(FowComp))
	{
		RTSFunctionLibrary::ReportError("Fow component already participating in fow manager, will not add this fow"
			"comp to partcipate again!"
			"\n See function: AFowManager::AddFowParticipant()");
		return;
	}
	M_PassiveFowComponents.Add(FowComp);
}


void AFowManager::UpdateDrawBuffer()
{
	if (not IsValid(NiagaraDraw))
	{
		OnInvalidNiagaraDraw();
		return;
	}
	int32 Index = 0;
	for (const auto EachComp : M_ActiveFowComponents)
	{
		FVector Location = EachComp->GetOwner()->GetActorLocation();
		const float VisionRadius = EachComp->GetVisionRadius();
		// Save the vision radius in the z component.
		Location.Z = VisionRadius;
		M_DrawBuffer[Index] = Location;
		Index++;
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraDraw, FName(*NiagaraDrawBufferName), M_DrawBuffer);
}

void AFowManager::AskUpdateEnemyVision()
{
	if (IsValid(NiagaraEnemyVision) && not bM_IsPendingEnemyVisionUpdate)
	{
		UpdateEnemyVision();
		bM_IsPendingEnemyVisionUpdate = true;
	}
}

void AFowManager::UpdateEnemyVision()
{
	M_CurrentPlayerCompsForEnemyVisionReadBack.Empty();

	// Prepare the enemy positions and vision ranges
	// Using FVector3f for compatibility with Niagara	
	TArray<FVector3f> EnemyData;
	for (auto EnemyComp : M_PassiveFowComponents)
	{
		if (EnemyComp->GetFowBehaviour() == EFowBehaviour::Fow_PassiveEnemyVision && EnemyComp->GetVisionRadius() > 0)
		{
			const FVector Position = EnemyComp->GetOwner()->GetActorLocation();
			const float VisionRange = EnemyComp->GetVisionRadius();
			EnemyData.Add(FVector3f(Position.X, Position.Y, VisionRange));
		}
	}

	// Prepare the player positions
	TArray<FVector2f> PlayerPositions;
	for (auto PlayerComp : M_ActiveFowComponents)
	{
		FVector Position = PlayerComp->GetOwner()->GetActorLocation();
		PlayerPositions.Add(FVector2f(Position.X, Position.Y));
		M_CurrentPlayerCompsForEnemyVisionReadBack.Add(PlayerComp);
	}

	// Send data to the Niagara system
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraEnemyVision,
		*EnemyPositionVisionBufferName,
		EnemyData
	);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
		NiagaraEnemyVision,
		*PlayerPositionsBufferName,
		PlayerPositions
	);

	NiagaraReadBack->SetVariableBool("bEnabled", true);

	// Reinitialize the system to process the new data
	NiagaraEnemyVision->ReinitializeSystem();

	if constexpr (DeveloperSettings::Debugging::GFowSystem_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			"Amount of enemy vision fow components (Update Buffers): " + FString::FromInt(EnemyData.Num()),
			FColor::Purple);

		RTSFunctionLibrary::PrintString(
			"Amount of player fow components (Update Buffers): " + FString::FromInt(PlayerPositions.Num()),
			FColor::Purple);
		RTSFunctionLibrary::PrintString("Enemy vision data sent to Niagara system", FColor::Purple);
	}
}

void AFowManager::OnReceivedEnemyVisionUpdate(const TArray<FBasicParticleData>& Data)
{
	if (not bM_IsPendingEnemyVisionUpdate)
	{
		return;
	}
	bM_IsPendingEnemyVisionUpdate = false;
	// Loop through the particle data to get visibility information
	for (const FBasicParticleData& ParticleData : Data)
	{
		// The index of the player unit is stored in Velocity.X
		int32 PlayerIndex = static_cast<int32>(ParticleData.Velocity.X);

		if (M_CurrentPlayerCompsForEnemyVisionReadBack.IsValidIndex(PlayerIndex))
		{
			TWeakObjectPtr<UFowComp> PlayerComp = M_CurrentPlayerCompsForEnemyVisionReadBack[PlayerIndex];
			if (PlayerComp.IsValid())
			{
				// The Size represents visibility (0.0f for hidden, 1.0f for visible)
				const float Visibility = ParticleData.Size;

				// Update the player's visibility status
				PlayerComp->OnFowVisibilityUpdated(Visibility);
			}
		}
		else
		{
			// todo renable
			// RTSFunctionLibrary::PrintString("Invalid index received from Niagara system for enemy vision update!"
			// 	"\n See function: AFowManager::OnReceivedEnemyVisionUpdate()"
			// 	"\n Index: " + FString::FromInt(PlayerIndex) +
			// 	" \n but CurrentAmountOfPlayerFowComponents: " + FString::FromInt(
			// 		M_CurrentPlayerCompsForEnemyVisionReadBack.Num()));
		}
	}

	if (IsValid(NiagaraEnemyVision))
	{
		// Reset enemy vision components in niagara system.
		const TArray<FVector3f> ResetEnemyPosVisionArray;
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraEnemyVision, FName(*EnemyPositionVisionBufferName), ResetEnemyPosVisionArray);
		const TArray<FVector2D> ResetPlayerPositionsArray;
		// Reset player position buffer in enemy vision niagara system.
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
			NiagaraEnemyVision, FName(*PlayerPositionsBufferName), ResetPlayerPositionsArray);
		// Flag that stops updating this manager with data from the data interface on the enemy vision niagara system.
		NiagaraEnemyVision->SetVariableBool("bEnabled", false);
		NiagaraEnemyVision->ReinitializeSystem();
		// Clear the valid components requesting fog visibility.
		M_CurrentPlayerCompsForEnemyVisionReadBack.Empty();

		if constexpr (DeveloperSettings::Debugging::GFowSystem_Compile_DebugSymbols)
		{
			if (Data.Num() > 0)
			{
				RTSFunctionLibrary::PrintString(
					"Amount of enemy components in compute shader:" + FString::SanitizeFloat(Data[0].Velocity.Z));
			}
			for (const auto& PaticleData : Data)
			{
				RTSFunctionLibrary::PrintString("velocity vector found: " + PaticleData.Velocity.ToString());
			}
			RTSFunctionLibrary::PrintString("Reset vision arrays in niagara system");
		}
	}
}


void AFowManager::OnInvalidNiagaraDraw()
{
	RTSFunctionLibrary::ReportError("FOW manager does not have a valid niagara draw component, "
		"cannot update draw buffer!"
		"\n See function: AFowManager::UpdateDrawBuffer()"
		"\n Active fow components will be cleared!");
	M_ActiveFowComponents.Empty();
	StopFow();
}

void AFowManager::EnsureComponentsAreValid()
{
	TArray<TWeakObjectPtr<UFowComp>> ValidComponents = {};
	for (auto EachComp : M_ActiveFowComponents)
	{
		if (EachComp.IsValid())
		{
			if (IsValid(EachComp->GetOwner()))
			{
				ValidComponents.Add(EachComp);
			}
		}
	}
	M_ActiveFowComponents = ValidComponents;
	ValidComponents.Empty();
	for (const auto EachComp : M_PassiveFowComponents)
	{
		if (EachComp.IsValid())
		{
			if (IsValid(EachComp->GetOwner()))
			{
				ValidComponents.Add(EachComp);
			}
		}
	}
	M_PassiveFowComponents = ValidComponents;
	// Ensure that the draw buffer has sufficient space.
	if (M_DrawBuffer.Num() != M_ActiveFowComponents.Num())
	{
		M_DrawBuffer.SetNum(M_ActiveFowComponents.Num());
	}
}

void AFowManager::AskReadBack()
{
	if (not bM_IsPendingReadBackAnswer && IsValid(NiagaraReadBack) && M_PassiveFowComponents.Num() > 0)
	{
		UpdateReadBackBuffer();
		bM_IsPendingReadBackAnswer = true;
	}
}

void AFowManager::UpdateReadBackBuffer()
{
	TArray<FVector2D> NiagaraData = {};
	for (auto PassiveFowComp : M_PassiveFowComponents)
	{
		M_CurrentReadBackPassiveComponents.Add(PassiveFowComp);
		const FVector Location = PassiveFowComp->GetOwner()->GetActorLocation();
		NiagaraData.Add(FVector2D(Location.X, Location.Y));
	}
	if (M_CurrentReadBackPassiveComponents.Num() > 0)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
			NiagaraReadBack, FName(*NiagaraReadBackBufferName), NiagaraData);
		NiagaraReadBack->SetVariableBool("bEnabled", true);
		NiagaraReadBack->ReinitializeSystem();
	}
}

void AFowManager::OnReceivedReadBackUpdate(const TArray<FBasicParticleData>& Data)
{
	if (not bM_IsPendingReadBackAnswer)
	{
		return;
	}
	bM_IsPendingReadBackAnswer = false;
	int32 Index = 0;
	for (const auto EachElm : Data)
	{
		// Obtain the index from the velocity vector's x component, it turns out that for arrays bigger than 64
		// The GPU does not respect the order of the particles, so we need to use the velocity vector to store the index.
		if (GetValidParticleIndex(EachElm.Velocity, Index))
		{
			TWeakObjectPtr<UFowComp> PassiveComp = M_CurrentReadBackPassiveComponents[Index];
			if (PassiveComp.IsValid())
			{
				PassiveComp->OnFowVisibilityUpdated(EachElm.Size);
			}
		}
	}
	if (IsValid(NiagaraReadBack))
	{
		const TArray<FVector2D> EmptyArray;
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector2D(
			NiagaraReadBack, FName(*NiagaraReadBackBufferName), EmptyArray);
		NiagaraReadBack->SetVariableBool("bEnabled", false);
		NiagaraReadBack->ReinitializeSystem();
	}
	// Clear the valid components requesting fog visibility.
	M_CurrentReadBackPassiveComponents.Empty();
}

bool AFowManager::GetValidParticleIndex(const FVector& ParticleVector, int32& OutIndex) const
{
	OutIndex = FMath::TruncToInt32(ParticleVector.X);
	if (OutIndex < 0 || OutIndex >= M_CurrentReadBackPassiveComponents.Num())
	{
		// todo off by one error here, one particle too many is used on GPU.
		RTSFunctionLibrary::PrintString("Dangerous particle index requested, index out of bounds!"
			"\n Particle index requested: " + FString::FromInt(OutIndex) +
			"\n Particle vector: " + ParticleVector.ToString() +
			"\n Amount of valid actors requesting fog visibility: " + FString::FromInt(
				M_CurrentReadBackPassiveComponents.Num()) +
			"\n See function: AFowManager::GetValidParticleIndex())");
		return false;
	}
	return true;
}
