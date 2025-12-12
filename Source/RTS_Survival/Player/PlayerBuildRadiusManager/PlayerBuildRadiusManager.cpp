// PlayerBuildRadiusManager.cpp

#include "PlayerBuildRadiusManager.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/BuildRadiusComp/BuildRadiusComp.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UPlayerBuildRadiusManager::UPlayerBuildRadiusManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerBuildRadiusManager::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayerBuildRadiusManager::EnsureRadiiAreValid()
{
	TArray<UBuildRadiusComp*> ValidRadii;
	for (UBuildRadiusComp* RadiusComp : M_BuildRadii)
	{
		if (IsValid(RadiusComp) && IsValid(RadiusComp->GetOwner()))
		{
			ValidRadii.Add(RadiusComp);
			continue;
		}
		FString OwnerName = RadiusComp ? RadiusComp->GetOwner()->GetName() : TEXT("INVALID");
		RTSFunctionLibrary::ReportError(
			"A BuildRadiusComponent is invalid in UPlayerBuildRadiusManager::EnsureRadiiAreValid,"
			"\n failed to deregister component?"
			"\n see UPlayerBuildRadiusManager::UnregisterBuildRadiusComponent"
			"\n Owner: " + OwnerName);
	}
	M_BuildRadii = ValidRadii;
}



void UPlayerBuildRadiusManager::ShowBuildRadius(bool bShow)
{
	EnsureRadiiAreValid();

	if (bShow)
	{
		// Get the radii that are not completely covered
		TArray<UBuildRadiusComp*> RadiiToShow = GetRadiiToShow();

		// The max amount of other circles the material can handle.
		const int32 MaxOtherCircles = 3;

		for (UBuildRadiusComp* RadiusCompI : RadiiToShow)
		{
			if(not RadiusCompI->GetIsEnabled())
			{
				continue;
			}
			// Show the radius component
			RadiusCompI->ShowRadius();

			// Set up the material parameters
			SetupRadiusComponentMaterial(RadiusCompI, RadiiToShow, MaxOtherCircles);
		}
	}
	else
	{
		// Hide all radii
		for (UBuildRadiusComp* RadiusComp : M_BuildRadii)
		{
			if (IsValid(RadiusComp))
			{
				RadiusComp->HideRadius();
			}
		}
	}
}

TArray<UBuildRadiusComp*> UPlayerBuildRadiusManager::GetBuildRadiiForConstruction()
{
	EnsureRadiiAreValid();
	return GetEnabledRadii(); 
}

void UPlayerBuildRadiusManager::RegisterBuildRadiusComponent(UBuildRadiusComp* RadiusComp)
{
	if (IsValid(RadiusComp))
	{
		if (!M_BuildRadii.Contains(RadiusComp))
		{
			M_BuildRadii.Add(RadiusComp);
		}
	}
}

void UPlayerBuildRadiusManager::UnregisterBuildRadiusComponent(UBuildRadiusComp* RadiusComp)
{
	if (RadiusComp)
	{
		if (M_BuildRadii.Contains(RadiusComp))
		{
			M_BuildRadii.Remove(RadiusComp);
			return;
		}
		RTSFunctionLibrary::ReportError(
			"Attempted to remove RadiusComp from M_BuildRadii in UPlayerBuildRadiusManager::UnregisterBuildRadiusComponent,"
			"\n But this component was not found in the array!!");
	}
}

TArray<UBuildRadiusComp*> UPlayerBuildRadiusManager::GetRadiiToShow()
{
	TArray<UBuildRadiusComp*> RadiiToShow;

	for (UBuildRadiusComp* RadiusCompI : M_BuildRadii)
	{
		bool bIsCompletelyCovered = IsRadiusCompletelyCovered(RadiusCompI);

		if (!bIsCompletelyCovered)
		{
			RadiiToShow.Add(RadiusCompI);
		}
		else
		{
			RadiusCompI->HideRadius();
		}
	}

	return RadiiToShow;
}

bool UPlayerBuildRadiusManager::IsRadiusCompletelyCovered(UBuildRadiusComp* ValidRadiusCompI)
{
	FVector CenterI = ValidRadiusCompI->GetOwner()->GetActorLocation();
	float RadiusI = ValidRadiusCompI->GetRadius();

	for (UBuildRadiusComp* RadiusCompJ : M_BuildRadii)
	{
		if (ValidRadiusCompI == RadiusCompJ)
			continue;

		FVector CenterJ = RadiusCompJ->GetOwner()->GetActorLocation();
		float RadiusJ = RadiusCompJ->GetRadius();

		float DistanceCenters = FVector::Dist(CenterI, CenterJ);

		if (DistanceCenters + RadiusI <= RadiusJ)
		{
			// RadiusI is completely covered by RadiusJ
			return true;
		}
	}

	return false;
}

void UPlayerBuildRadiusManager::SetupRadiusComponentMaterial(
	UBuildRadiusComp* ValidRadiusComp,
	const TArray<UBuildRadiusComp*>& RadiiToShow,
	int32 MaxOtherCircles)
{
	TArray<FVector> OtherCenters;
	TArray<float> OtherRadii;

	// Collect overlapping circles for material parameters
	CollectOverlappingCircles(ValidRadiusComp, RadiiToShow, MaxOtherCircles, OtherCenters, OtherRadii);
	if (DeveloperSettings::Debugging::GBuildRadius_Compile_DebugSymbols)
	{
		DebugOtherCircles(ValidRadiusComp, OtherCenters, OtherRadii);
	}

	// Set material parameters
	UMaterialInstanceDynamic* DynMaterial = ValidRadiusComp->GetOrCreateDynamicMaterialInstance();

	if (DynMaterial)
	{
		DynMaterial->SetScalarParameterValue(TEXT("NumOtherCircles"), OtherCenters.Num());

		if (OtherCenters.Num() > 3)
		{
			RTSFunctionLibrary::ReportError(
				"WARNING; More than 3 overlapping circles detected in UPlayerBuildRadiusManager::SetupRadiusComponentMaterial"
				"\n the material is set up to only support 3!");
		}

		// Set parameters for other circles
		for (int32 k = 0; k < OtherCenters.Num(); ++k)
		{
			FString CenterParamName = FString::Printf(TEXT("CircleCenter%d"), k);
			FString RadiusParamName = FString::Printf(TEXT("CircleRadius%d"), k);

			// Use only the X and Y components for the circle centers
			FVector2D CircleCenter2D(OtherCenters[k].X, OtherCenters[k].Y);
			DynMaterial->SetVectorParameterValue(FName(*CenterParamName),
			                                     FLinearColor(CircleCenter2D.X, CircleCenter2D.Y, 0.0f));
			DynMaterial->SetScalarParameterValue(FName(*RadiusParamName), OtherRadii[k]);
		}
		if (DeveloperSettings::Debugging::GBuildRadius_Compile_DebugSymbols)
		{
			DebugDynamicMaterial(ValidRadiusComp, DynMaterial, OtherCenters, OtherRadii);
		}
		return;
	}
	RTSFunctionLibrary::ReportError(
		"failed to get or create dynamic material instance in UPlayerBuildRadiusManager::SetupRadiusComponentMaterial"
		"\n for radius component: " + ValidRadiusComp->GetOwner()->GetName());
}

void UPlayerBuildRadiusManager::DebugOtherCircles(
	UBuildRadiusComp* CurrentComp,
	const TArray<FVector>& OtherCenters,
	const TArray<float>& OtherRadii)
{
	if (!IsValid(CurrentComp) || !IsValid(CurrentComp->GetOwner()))
	{
		return;
	}

	// Get the world context
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector CurrentLocation = CurrentComp->GetOwner()->GetActorLocation();
	const FString OwnerName = CurrentComp->GetOwner()->GetName();

	for (int32 i = 0; i < OtherCenters.Num(); ++i)
	{
		const FVector& OtherLocation = OtherCenters[i];
		const float OtherRadius = OtherRadii[i];

		// Draw a red debug line from the current component to the other circle
		DrawDebugLine(
			World,
			CurrentLocation,
			OtherLocation,
			FColor::Red,
			false, // bPersistentLines
			10.0f, // LifeTime
			0, // DepthPriority
			2.0f // Thickness
		);

		// Draw the index above the other circle's location (Z + 50 units)
		DrawDebugString(
			World,
			OtherLocation + FVector(0.0f, 0.0f, 50.0f),
			FString::Printf(TEXT("%d"), i),
			nullptr, // TestBaseActor
			FColor::White,
			10.0f, // Duration
			false // bDrawShadow
		);

		// Draw a green sphere at the other circle's location with its radius
		DrawDebugSphere(
			World,
			OtherLocation,
			OtherRadius,
			16, // Segments
			FColor::Green,
			false, // bPersistentLines
			10.0f, // LifeTime
			0, // DepthPriority
			2.0f // Thickness
		);
	}
}

void UPlayerBuildRadiusManager::DebugDynamicMaterial(UBuildRadiusComp* CurrentComp, UMaterialInstanceDynamic* DynMaterial,
                                                     TArray<FVector> OtherCenters, TArray<float> OtherRadii)
{
	if (DynMaterial)
	{
		FString OwnerName = CurrentComp->GetOwner() ? CurrentComp->GetOwner()->GetName() : TEXT("INVALID");
		float NumOtherCirclesValue = 0;
		if (DynMaterial->GetScalarParameterValue(FMaterialParameterInfo(TEXT("NumOtherCircles")), NumOtherCirclesValue))
		{
			RTSFunctionLibrary::PrintString(
				FString::Printf(TEXT("Owner: %s, NumOtherCircles: %f"), *OwnerName, NumOtherCirclesValue));
		}
		else
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("Owner: %s, Failed to get NumOtherCircles parameter"), *OwnerName));
		}
		for (int32 k = 0; k < OtherCenters.Num(); ++k)
		{
			FString CenterParamName = FString::Printf(TEXT("CircleCenter%d"), k);
			FString RadiusParamName = FString::Printf(TEXT("CircleRadius%d"), k);

			// Debug: Get and RTSlog the CircleCenter and CircleRadius parameters
			FLinearColor CircleCenterValue;
			if (DynMaterial->GetVectorParameterValue(FMaterialParameterInfo(*CenterParamName), CircleCenterValue))
			{
				RTSFunctionLibrary::PrintString(
					FString::Printf(TEXT("Owner: %s, %s: (%f, %f)"),
					                *OwnerName,
					                *CenterParamName,
					                CircleCenterValue.R,
					                CircleCenterValue.G));
			}
			else
			{
				RTSFunctionLibrary::ReportError(
					FString::Printf(TEXT("Owner: %s, Failed to get %s parameter"), *OwnerName, *CenterParamName));
			}
			float CircleRadiusValue = 0.0f;
			if (DynMaterial->GetScalarParameterValue(FMaterialParameterInfo(*RadiusParamName), CircleRadiusValue))
			{
				RTSFunctionLibrary::PrintString(
					FString::Printf(TEXT("Owner: %s, %s: %f"), *OwnerName, *RadiusParamName, CircleRadiusValue));
			}
			else
			{
				RTSFunctionLibrary::ReportError(
					FString::Printf(TEXT("Owner: %s, Failed to get %s parameter"), *OwnerName, *RadiusParamName));
			}
		}
	}
}

TArray<UBuildRadiusComp*> UPlayerBuildRadiusManager::GetEnabledRadii()
{
	TArray<UBuildRadiusComp*> EnabledRadii;
	for (auto RadiusComp : M_BuildRadii)
	{
		if (IsValid(RadiusComp))
		{
			if (RadiusComp->GetIsEnabled())
			{
				EnabledRadii.Add(RadiusComp);
			}
		}
	}
	return EnabledRadii;
}


void UPlayerBuildRadiusManager::CollectOverlappingCircles(UBuildRadiusComp* ValidRadiusComp,
                                                          const TArray<UBuildRadiusComp*>& RadiiToShow,
                                                          int32 MaxOtherCircles, TArray<FVector>& OutOtherCenters,
                                                          TArray<float>& OutOtherRadii)
{
	FVector CenterI = ValidRadiusComp->GetOwner()->GetActorLocation();
	float RadiusI = ValidRadiusComp->GetRadius();

	for (UBuildRadiusComp* RadiusCompJ : RadiiToShow)
	{
		if (ValidRadiusComp == RadiusCompJ || not RadiusCompJ->GetIsEnabled())
			continue;

		if (!IsValid(RadiusCompJ) || !IsValid(RadiusCompJ->GetOwner()))
			continue;

		FVector CenterJ = RadiusCompJ->GetOwner()->GetActorLocation();
		float RadiusJ = RadiusCompJ->GetRadius();

		// Optionally, check if the circles actually overlap
		float DistanceCenters = FVector::Dist(CenterI, CenterJ);

		if (DistanceCenters <= RadiusI + RadiusJ)
		{
			OutOtherCenters.Add(CenterJ);
			OutOtherRadii.Add(RadiusJ);

			if (OutOtherCenters.Num() >= MaxOtherCircles)
			{
				break;
			}
		}
	}
}
