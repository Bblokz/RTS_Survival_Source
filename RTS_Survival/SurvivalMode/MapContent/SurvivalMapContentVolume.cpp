// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalMapContentVolume.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "PCGComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

ASurvivalMapContentVolume::ASurvivalMapContentVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	M_Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	M_Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_Bounds->SetCanEverAffectNavigation(false);
	M_Bounds->SetHiddenInGame(true);
	RootComponent = M_Bounds;
	M_PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
	M_PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
}

bool ASurvivalMapContentVolume::StartForMatch(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		return false;
	}
	bool bAnyStarted = false;
	for (TActorIterator<ASurvivalMapContentVolume> It(World); It; ++It)
	{
		bAnyStarted = It->GenerateForMatch() or bAnyStarted;
	}
	return bAnyStarted;
}

int32 ASurvivalMapContentVolume::ResolveMatchSeed() const
{
#if RTS_WITH_MAP_TESTS
	int32 CommandLineSeed = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("SurvivalMapContentSeed="), CommandLineSeed) and CommandLineSeed != 0)
	{
		return CommandLineSeed;
	}
#endif
	if (FixedSeed != 0)
	{
		return FixedSeed;
	}
	// Never 0: that value means "no seed chosen".
	return FMath::Max(1, FMath::Rand());
}

bool ASurvivalMapContentVolume::GenerateForMatch()
{
	if (not GetIsValidPCGComponent())
	{
		return false;
	}
	M_PCGComponent->Seed = ResolveMatchSeed();
	constexpr bool bForce = true;
	M_PCGComponent->GenerateLocal(bForce);
	UE_LOG(LogTemp, Log, TEXT("Survival map content: generating %s with seed %d."), *GetName(), M_PCGComponent->Seed);
	return true;
}

bool ASurvivalMapContentVolume::GetIsValidPCGComponent() const
{
	if (IsValid(M_PCGComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PCGComponent", "GetIsValidPCGComponent", this);
	return false;
}
