// Copyright (C) Bas Blokzijl - All rights reserved.


#include "SmallArmsProjectileManager.h"

#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


ASmallArmsProjectileManager::ASmallArmsProjectileManager()
{
	PrimaryActorTick.TickInterval = 0.25;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	M_LastActivatedProjectileIndex = -1;
	M_NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
}

void ASmallArmsProjectileManager::FireProjectile(
	const FVector& Position,
	const FVector& EndPosition,
	const int32 Type,
	const float StartTime)
{
	if (not GetIsValidNiagaraComponent())
	{
		return;
	}
	bool bIsPoolSaturated;
	const int32 Index = FindDormantProjectileIndex(bIsPoolSaturated);
	if (bIsPoolSaturated)
	{
		OnPoolSaturated();
		return;
	}
	const double LifeTime = FVector::Dist(Position, EndPosition) /
		DeveloperSettings::GamePlay::Projectile::SmallArmsProjectileSpeed;
	// Index is valid.
	M_LastActivatedProjectileIndex = Index;
	SetProjectileValues(
		Position,
		EndPosition,
		Type,
		StartTime,
		LifeTime,
		Index);
}

AProjectile* ASmallArmsProjectileManager::GetDormantTankProjectile()
{
	return M_TankProjectilesPool.GetDormantProjectile();
}


// Called when the game starts or when spawned
void ASmallArmsProjectileManager::BeginPlay()
{
	Super::BeginPlay();
	M_World = GetWorld();
}

void ASmallArmsProjectileManager::InitProjectileManager(UNiagaraSystem* ProjectileSystem,
                                                        const TSubclassOf<AProjectile> TankProjectileClass)
{
	if (not EnsureSystemIsValid(ProjectileSystem) || not GetIsValidNiagaraComponent())
	{
		return;
	}
	M_NiagaraComponent->SetAsset(ProjectileSystem);
	M_NiagaraComponent->Activate();
	PrimaryActorTick.SetTickFunctionEnable(true);
	M_NiagaraComponent->SetFloatParameter(FName(*NiagaraProjectileSpeedVarName),
	                                      DeveloperSettings::GamePlay::Projectile::SmallArmsProjectileSpeed);
	M_TankProjectileClass = TankProjectileClass;
	M_TankProjectilesPool.CreateProjectilesForPool(
		this, GetWorld(), TankProjectileClass, GetActorLocation());
}


void ASmallArmsProjectileManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateProjectiles();
	Debug_AmountProjectilesActive();
}

bool ASmallArmsProjectileManager::EnsureSystemIsValid(TObjectPtr<UNiagaraSystem> System) const
{
	if (not IsValid(System))
	{
		RTSFunctionLibrary::ReportError("Attempted to init projectile manager with invalid system.");
		return false;
	}
	return true;
}

bool ASmallArmsProjectileManager::EnsureWorldIsValid()
{
	if (not M_World.IsValid())
	{
		RTSFunctionLibrary::ReportError("World is invalid on projectile manager: " + GetName() +
			"\n Attempting repair...");
		M_World = GetWorld();
		return M_World.IsValid();
	}
	return true;
}


bool ASmallArmsProjectileManager::GetIsValidNiagaraComponent()
{
	if (IsValid(M_NiagaraComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("NiagaraComponent is invalid on projectile manager: " + GetName() +
		"\n Attempting repair...");
	M_NiagaraComponent = NewObject<UNiagaraComponent>(this);
	return IsValid(M_NiagaraComponent);
}

void ASmallArmsProjectileManager::OnPoolSaturated()
{
	RTSFunctionLibrary::ReportError("Projectile pool is saturated! No projectiles left.");
}

int32 ASmallArmsProjectileManager::FindDormantProjectileIndex(bool& bOutIsPoolSaturated) const
{
	// Start looking from the last activated projectile.
	const int32 StartIndex = (M_LastActivatedProjectileIndex + 1) % FSoA_SmallArmsProjectilesPool::PoolSize;
	constexpr int32 MaxSteps = FSoA_SmallArmsProjectilesPool::PoolSize;
	for (int32 i = 0; i < MaxSteps; ++i)
	{
		const int32 Index = (StartIndex + i) % FSoA_SmallArmsProjectilesPool::PoolSize;
		if (not M_SmallArmsProjectilePool.bIsActive[Index])
		{
			bOutIsPoolSaturated = false;
			return Index;
		}
	}
	bOutIsPoolSaturated = true;
	return -1;
}

void ASmallArmsProjectileManager::SetProjectileValues(const FVector& Position, const FVector& EndPosition,
                                                      const int32 Type, const float StartTime, const double LifeTime,
                                                      const int32 ProjectileIndex)
{
	M_SmallArmsProjectilePool.bIsActive[ProjectileIndex] = true;
	M_SmallArmsProjectilePool.Positions[ProjectileIndex] = Position;
	M_SmallArmsProjectilePool.EndPositions[ProjectileIndex] = EndPosition;
	const FVector TypeAndTime = FVector(Type, StartTime, LifeTime);
	M_SmallArmsProjectilePool.ProjectileTypesStartTimeLifeTime[ProjectileIndex] = TypeAndTime;
}

void ASmallArmsProjectileManager::UpdateProjectiles()
{
	if (not EnsureWorldIsValid())
	{
		return;
	}
	const double CurrentTime = M_World->GetTimeSeconds();
	for (int32 i = 0; i < FSoA_SmallArmsProjectilesPool::PoolSize; ++i)
	{
		if (not M_SmallArmsProjectilePool.bIsActive[i])
		{
			continue;
		}
		if (CurrentTime - M_SmallArmsProjectilePool.ProjectileTypesStartTimeLifeTime[i].Y > M_SmallArmsProjectilePool.
			ProjectileTypesStartTimeLifeTime[i].Z)
		{
			Debug_ProjectilePooling("Projectile at index: " + FString::FromInt(i) + " has expired.");
			M_SmallArmsProjectilePool.bIsActive[i] = false;
		}
	}
	UpdateNiagaraWithData();
}

void ASmallArmsProjectileManager::UpdateNiagaraWithData()
{
	if (not GetIsValidNiagaraComponent())
	{
		return;
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		M_NiagaraComponent, FName(*NiagaraPositionArrayName), M_SmallArmsProjectilePool.Positions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		M_NiagaraComponent, FName(*NiagaraEndPositionArrayName), M_SmallArmsProjectilePool.EndPositions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		M_NiagaraComponent, FName(*NiagaraTypeArrayName), M_SmallArmsProjectilePool.ProjectileTypesStartTimeLifeTime);
}

void ASmallArmsProjectileManager::Debug_ProjectilePooling(const FString& Message) const
{
	if (DeveloperSettings::Debugging::GProjectilePooling_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, FColor::Purple);
	}
}

void ASmallArmsProjectileManager::Debug_AmountProjectilesActive() const
{
	if (DeveloperSettings::Debugging::GProjectilePooling_Compile_DebugSymbols)
	{
		int32 ActiveProjectiles = 0;
		for (int32 i = 0; i < FSoA_SmallArmsProjectilesPool::PoolSize; ++i)
		{
			if (M_SmallArmsProjectilePool.bIsActive[i])
			{
				++ActiveProjectiles;
			}
		}
		RTSFunctionLibrary::PrintString("Amount of active projectiles: " + FString::FromInt(ActiveProjectiles),
		                                FColor::Red);
	}
}

void ASmallArmsProjectileManager::OnTankProjectileDormant(const int32 Index)
{
	if (not M_TankProjectilesPool.bIsActive.IsValidIndex(Index))
	{
		RTSFunctionLibrary::ReportError("Tank projectile index is invalid: " + FString::FromInt(Index) +
			"\n At function: OnTankProjectileDormant");
		return;
	}
	M_TankProjectilesPool.bIsActive[Index] = false;
}

