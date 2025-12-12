#include "AUT_ResourceSystem_Manager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

AUT_ResourceSystem_Manager::AUT_ResourceSystem_Manager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AUT_ResourceSystem_Manager::InitializeDropOffClasses(const TArray<TSubclassOf<AActor>>& InClasses)
{
	DropOffActorClasses = InClasses;
}

void AUT_ResourceSystem_Manager::InitializeSpawnLocations(const TArray<FVector>& InLocations)
{
	SpawnLocations = InLocations;
}

void AUT_ResourceSystem_Manager::BeginPlay()
{
	Super::BeginPlay();
	SpawnDropOffActors();
	TestStepIndex = 0;
	ScheduleNextStep();
}

void AUT_ResourceSystem_Manager::SpawnDropOffActors()
{
	if (DropOffActorClasses.Num() == 0)
	{
		RTSFunctionLibrary::ReportError("No drop-off classes provided.");
		return;
	}

	for (int32 i = 0; i < DropOffActorClasses.Num(); i++)
	{
		if (!DropOffActorClasses[i])
		{
			RTSFunctionLibrary::ReportError("Invalid drop-off class at index " + FString::FromInt(i));
			continue;
		}
		FVector SpawnLoc(0.f, 0.f, 0.f);
		if (SpawnLocations.IsValidIndex(i))
		{
			SpawnLoc = SpawnLocations[i];
		}
		else if (SpawnLocations.Num() > 0)
		{
			SpawnLoc = SpawnLocations.Last();
		}

		FActorSpawnParameters Params;
		AActor* Spawned = GetWorld()->SpawnActor<AActor>(DropOffActorClasses[i], SpawnLoc, FRotator::ZeroRotator,
		                                                 Params);
		if (Spawned)
		{
			SpawnedActors.Add(Spawned);
			FindDropOffComp(Spawned);
		}
	}
}

void AUT_ResourceSystem_Manager::FindDropOffComp(AActor* Actor)
{
	if (!Actor) return;
	UActorComponent* Component = Actor->GetComponentByClass(UResourceDropOff::StaticClass());
	if (UResourceDropOff* DropComp = Cast<UResourceDropOff>(Component))
	{
		DropOffComponents.Add(DropComp);
	}
}

void AUT_ResourceSystem_Manager::ScheduleNextStep()
{
	GetWorldTimerManager().SetTimer(TestStepTimerHandle, this, &AUT_ResourceSystem_Manager::PerformTestStep,
	                                DelayBetweenSteps, false);
}

void AUT_ResourceSystem_Manager::PerformTestStep()
{
	switch (TestStepIndex)
	{
	case 0: PerformTestStep0();
		break;
	case 1: PerformTestStep1();
		break;
	case 2: PerformTestStep2();
		break;
	case 3: PerformTestStep3();
		break;
	case 4: PerformTestStep4();
		break;
	default:
		RTSFunctionLibrary::PrintString("All ResourceSystem tests completed!", FColor::Green);
		return;
	}
	TestStepIndex++;
	ScheduleNextStep();
}

void AUT_ResourceSystem_Manager::PerformTestStep0()
{
	RTSFunctionLibrary::PrintString("TestStep0: Check initial states of drop-offs", FColor::Cyan);
	LogDropOffState("Before any additions");
}

void AUT_ResourceSystem_Manager::PerformTestStep1()
{
	RTSFunctionLibrary::PrintString("TestStep1: Add resources", FColor::Cyan);

	for (UResourceDropOff* DropComp : DropOffComponents)
	{
		if (!DropComp) continue;
		DropComp->AddResourcesNotHarvested(ERTSResourceType::Resource_Radixite, 50);
		DropComp->AddResourcesNotHarvested(ERTSResourceType::Resource_Metal, 25);
	}
	LogDropOffState("After adding resources");
}

void AUT_ResourceSystem_Manager::PerformTestStep2()
{
	RTSFunctionLibrary::PrintString("TestStep2: Remove some resources", FColor::Cyan);

	for (UResourceDropOff* DropComp : DropOffComponents)
	{
		if (!DropComp) continue;
		int32 RadixiteHeld = DropComp->GetDropOffAmountStored(ERTSResourceType::Resource_Radixite);
		if (RadixiteHeld >= 10)
		{
			DropComp->RemoveResources(ERTSResourceType::Resource_Radixite, 10);
		}
		int32 MetalHeld = DropComp->GetDropOffAmountStored(ERTSResourceType::Resource_Metal);
		if (MetalHeld >= 5)
		{
			DropComp->RemoveResources(ERTSResourceType::Resource_Metal, 5);
		}
	}
	LogDropOffState("After removing resources");
}

void AUT_ResourceSystem_Manager::PerformTestStep3()
{
	RTSFunctionLibrary::PrintString("TestStep3: Destroy one drop-off actor", FColor::Cyan);

	if (SpawnedActors.Num() > 0)
	{
		AActor* Victim = SpawnedActors[0];
		if (Victim && Victim->IsValidLowLevel())
		{
			RTSFunctionLibrary::PrintString("Destroying drop-off actor: " + Victim->GetName());
			Victim->Destroy();
		}
		else
		{
			RTSFunctionLibrary::ReportError("Couldn't destroy drop-off actor at index 0!");
		}
		SpawnedActors.RemoveAt(0, 1, EAllowShrinking::No);

		if (DropOffComponents.Num() > 0)
		{
			DropOffComponents.RemoveAt(0, 1, EAllowShrinking::No);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("No actors to destroy in TestStep3!");
	}
	LogDropOffState("After destroying one drop-off actor");
}

void AUT_ResourceSystem_Manager::PerformTestStep4()
{
	RTSFunctionLibrary::PrintString("TestStep4: Final checks", FColor::Cyan);
	LogDropOffState("Final check");
	// Additional final validations if desired
}

void AUT_ResourceSystem_Manager::LogDropOffState(const FString& Context)
{
	for (int32 i = 0; i < DropOffComponents.Num(); i++)
	{
		UResourceDropOff* Comp = DropOffComponents[i];
		if (!Comp)
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("DropOffComponents[%d] is invalid in %s"), i, *Context));
			continue;
		}
		int32 RadixiteHeld = Comp->GetDropOffAmountStored(ERTSResourceType::Resource_Radixite);
		int32 MetalHeld = Comp->GetDropOffAmountStored(ERTSResourceType::Resource_Metal);
		FString Msg = FString::Printf(
			TEXT("[%s:%d] => Radixite=%d, Metal=%d"),
			*Context, i, RadixiteHeld, MetalHeld
		);
		RTSFunctionLibrary::PrintString(Msg, FColor::White);
	}
}
