// Copyright (C) Bas Blokzijl - All rights reserved.

#include "HFunctionLibary.h"
#include "NavigationSystem.h"
#include "Components/BoxComponent.h"
#include "EnvironmentQuery/EnvQueryDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/MessageDialog.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDevice.h"
#include "NavAreas/NavArea_Default.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"

class ARecastNavMesh;
DEFINE_LOG_CATEGORY(LogRTS);


RTSFunctionLibrary::RTSFunctionLibrary()
{
}

RTSFunctionLibrary::~RTSFunctionLibrary()
{
}

void RTSFunctionLibrary::PrintString(
	const FString& StringToPrint,
	const FColor Color, const float Time)
{
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, Time, Color, StringToPrint);
	}
	UE_LOG(LogTemp, Log, TEXT("%s"), *StringToPrint);
}

void RTSFunctionLibrary::PrintString(const FVector& Location, const UObject* WorldContextObject,
                                     const FString& StringToPrint,
                                     const FColor Color, const float Time)
{
	if (not WorldContextObject)
	{
		return;
	}
	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		DrawDebugString(World, Location, StringToPrint, nullptr, Color, Time);
	}
}

void RTSFunctionLibrary::Print(UObject* ObjectWeprint)
{
	if (GEngine != nullptr)
	{
		if (ObjectWeprint != nullptr)
		{
			FString StringToPrint = ObjectWeprint->GetName();
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Blue, StringToPrint);
		}
	}
}

void RTSFunctionLibrary::NullWarning(FString VariableName)
{
	UE_LOG(LogTemp, Error, TEXT( "%s  = NULL"), *VariableName);
}

int32 RTSFunctionLibrary::RoundToNearestMultipleOf(const int32 Value, const int32 Multiple)
{
	return (Value / Multiple) * Multiple;
}

void RTSFunctionLibrary::ReportNullErrorInitialisation(
	const UObject* Object,
	const FString& VariableName,
	const FString& FunctionName)
{
	FString Name;
	if (IsValid(Object))
	{
		Name = Object->GetName();
	}
	else
	{
		Name = "Object NULL";
	}
	const FString Report = "Failed to initialize variable: " + VariableName
		+ " in actor: " + Name
		+ "\n at function: " + FunctionName
		+ "This data is not accessible at Init?";

	ReportError(Report, false);
}

void RTSFunctionLibrary::ReportErrorVariableNotInitialised(
	const UObject* Object,
	const FString& VariableName,
	const FString& FunctionName,
	const AActor* DerivedBpContext)
{
	FString ObjectName;
	if (IsValid(Object))
	{
		ObjectName = Object->GetName();
	}
	else
	{
		ObjectName = "Object NULL";
	}
	FString Context = "";
	if (IsValid(DerivedBpContext))
	{
		Context = "Make sure to set variable in derived bp of actor: " + DerivedBpContext->GetName();
	}
	FString Report = "Variable found unitialised in Object:" + ObjectName
		+ "\n Variable Name: " + VariableName
		+ "\n at function: " + FunctionName
		+ "\n" + Context;

	ReportError(Report, false);
}



void RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(const UObject* Object, const FString& VariableName,
                                                                  const FString& FunctionName,
                                                                  const UObject* DerivedBpContext)
{
	FString ObjectName;
	if (IsValid(Object))
	{
		ObjectName = Object->GetName();
	}
	else
	{
		ObjectName = "Object NULL";
	}
	FString Context = "";
	if (IsValid(DerivedBpContext))
	{
		Context = "Make sure to set variable in derived bp of actor: " + DerivedBpContext->GetName();
	}
	FString Report = "Variable found unitialised in Object:" + ObjectName
		+ "\n Variable Name: " + VariableName
		+ "\n at function: " + FunctionName
		+ "\n" + Context;

	ReportError(Report, false);
}

void RTSFunctionLibrary::ReportError(FString Text, const bool ResetErrorSuppression)
{
	static bool bSuppressError;
	if (ResetErrorSuppression)
	{
		bSuppressError = false;
	}
	if (bSuppressError)
	{
		return;
	};
	FString StackTrace;
	StackTrace += Text;
	StackTrace += "\n\nStack Trace:\n";

#if WITH_EDITOR
	// Capture the stack trace
	const int32 MaxStackTraceDepth = 8;
	uint64 BackTrace[MaxStackTraceDepth];
	int32 NumStackFrames = FPlatformStackWalk::CaptureStackBackTrace(BackTrace, MaxStackTraceDepth);

	// Array to store symbol information
	TArray<FProgramCounterSymbolInfo> SymbolInfoArray;
	SymbolInfoArray.SetNumUninitialized(NumStackFrames);

	for (int32 Index = 0; Index < NumStackFrames; ++Index)
	{
		FPlatformStackWalk::ProgramCounterToSymbolInfo(BackTrace[Index], SymbolInfoArray[Index]);
	}

	// Convert stack trace to string
	for (int32 Index = 0; Index < NumStackFrames; ++Index)
	{
		const FProgramCounterSymbolInfo& SymbolInfo = SymbolInfoArray[Index];
		StackTrace += FString::Printf(TEXT("Symbol %d:\n"), Index + 1);
		StackTrace += FString::Printf(TEXT("  Function: %s\n"), ANSI_TO_TCHAR(SymbolInfo.FunctionName));
		StackTrace += FString::Printf(TEXT("  File: %s\n"), ANSI_TO_TCHAR(SymbolInfo.Filename));
		StackTrace += FString::Printf(TEXT("  Line: %d\n"), SymbolInfo.LineNumber);
	}

	// Show the error message with the stack trace and Yes/No buttons
	FText ErrorMessage = FText::FromString(StackTrace);
	FText DialogTitle = FText::FromString(TEXT("Error, Suppress errors?"));

	EAppReturnType::Type ReturnType = FMessageDialog::Open(EAppMsgType::YesNo, ErrorMessage, DialogTitle);

	// Handle the button press
	if (ReturnType == EAppReturnType::Yes)
	{
		// Suppress the errors so we can exit the game.
		bSuppressError = true;
	}


#else
    // Log the error message
    UE_LOG(LogRTS, Error, TEXT("%s"), *Text);

    // Print the call stack to the RTSlog
    
	FDebug::DumpStackTraceToLog(ELogVerbosity::Error);

#endif
}

void RTSFunctionLibrary::ReportWarning(FString Text)
{
	PrintString("WARNING: " + Text, FColor::Red);
}

void RTSFunctionLibrary::DisplayNotification(const FText& Text)
{
#if WITH_EDITOR

	EAppReturnType::Type ReturnType = FMessageDialog::Open(EAppMsgType::Type::Ok, Text);
#endif
}

void RTSFunctionLibrary::DisplayNotification(const FString& Text)
{
#if WITH_EDITOR

	EAppReturnType::Type ReturnType = FMessageDialog::Open(EAppMsgType::Type::Ok, FText::FromString(Text));
#endif
}


void RTSFunctionLibrary::DrawDebugAtLocation(
	const UObject* WorldContext,
	const FVector& Location,
	const FString& Text,
	const FColor Color,
	const float Duration)
{
	if (IsValid(WorldContext))
	{
		if (const UWorld* World = WorldContext->GetWorld())
		{
			DrawDebugString(World, Location, Text, nullptr, Color, Duration);
		}
	}
}

void RTSFunctionLibrary::ReportNullErrorComponent(
	const UObject* Object,
	const FString& ComponentName,
	const FString& FunctionName)
{
	FString ActorName = "Actor NULL";
	if (IsValid(Object))
	{
		ActorName = Object->GetName();
	}
	ReportError("Component: " + ComponentName + " is either null or invalid in actor: " + ActorName
		+ "\n at function: " + FunctionName
		+ "\n Forgot to add this component to the derived blueprint?");
}

void RTSFunctionLibrary::ReportFailedCastError(
	const FString& CastedObjectName,
	const FString& ExpectedType,
	const FString& FunctionName)
{
	ReportError("Failed to cast object: " + CastedObjectName + " to type: " + ExpectedType
		+ "\n at function: " + FunctionName);
}

void RTSFunctionLibrary::PrintToLog(FString Text, bool bIsWarning)
{
	if (bIsWarning)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Text);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("%s"), *Text);
	}
}


void RTSFunctionLibrary::SetRotatorToZeroes(FRotator& Rotator)
{
	Rotator.Yaw = 0.0;
	Rotator.Roll = 0.0;
	Rotator.Pitch = 0.0;
} //SetRotatorToZeroes

float RTSFunctionLibrary::SingedAngleTo(
	const FVector Source,
	const FVector Dest,
	const FVector NormalToPlane)
{
	float angle = UKismetMathLibrary::FindLookAtRotation(Source, Dest).Yaw;
	UE_LOG(LogTemp, Log, TEXT("angle : %f"), angle);
	float dot = Source.Cross(Dest).Dot(NormalToPlane);
	UE_LOG(LogTemp, Log, TEXT("dot : %d"), dot);
	// Are the cross and perpendicular vectors pointing in the same direction?
	return dot < 0 ? -angle : angle;
}

bool RTSFunctionLibrary::RTSIsValid(AActor* ActorToCheck)
{
	if (!IsValid(ActorToCheck))
	{
		return false;
	}
	return ActorToCheck->IsUnitAlive();
}

bool RTSFunctionLibrary::RTSIsVisibleTarget(AActor* ActorToCheck, const int32 EnemyPlayerOfActor)
{
	if (!IsValid(ActorToCheck))
	{
		return false;
	}
	using DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	// If player 1 is the enemy then this actor must not be hidden in fow (visible to player 1).
	// if Player 1 is the ally then this actor must be in the enemy vision (tagged with enemy vision).
	const bool bIsVisible = EnemyPlayerOfActor == 1
		                        ? not ActorToCheck->IsHidden()
		                        : ActorToCheck->Tags.Contains(TagUnitInEnemyVision);
	return ActorToCheck->IsUnitAlive() && not ActorToCheck->IsHidden() && bIsVisible;
}


static bool GBEnableTraceDebug = false;

void RTSFunctionLibrary::DebugWeapons(const FString& Message, FColor Color, const bool bIsTrace)
{
	if constexpr (DeveloperSettings::Debugging::GWeapon_ArmorPen_Compile_DebugSymbols)
	{
		if (bIsTrace)
		{
			if (GBEnableTraceDebug)
			{
				RTSFunctionLibrary::PrintString(Message, Color);
			}
			return;
		}
		RTSFunctionLibrary::PrintString(Message, Color);
	}
}

USoundAttenuation* RTSFunctionLibrary::CreateSoundAttenuation(float Range)
{
	FSoundAttenuationSettings AttenuationSettings;
	AttenuationSettings.AttenuationShape = EAttenuationShape::Sphere;
	AttenuationSettings.AttenuationShapeExtents = FVector(Range);
	AttenuationSettings.FalloffDistance = Range * 0.5f;

	USoundAttenuation* SoundAttenuation = NewObject<USoundAttenuation>();
	SoundAttenuation->Attenuation = AttenuationSettings;

	return SoundAttenuation;
}



static bool GetIsDefaultNavAreaAtLocation(
    const ARecastNavMesh* const RecastNavMesh,
    const FNavLocation& NavLocation)
{
    if (RecastNavMesh == nullptr)
    {
        return false;
    }

    if (!NavLocation.HasNodeRef())
    {
        return false;
    }

    FNavMeshNodeFlags PolyFlags;
    if (!RecastNavMesh->GetPolyFlags(NavLocation.NodeRef, PolyFlags))
    {
        return false;
    }

    const UClass* const AreaClass = RecastNavMesh->GetAreaClass(PolyFlags.Area);
    if (AreaClass == nullptr)
    {
        return false;
    }

    return AreaClass->IsChildOf(UNavArea_Default::StaticClass());
}

FVector RTSFunctionLibrary::GetLocationProjected(const UObject* WorldContextObject, const FVector& OriginalLocation,
                                                 const bool bExtentInZ, bool& bOutWasSuccessful,
                                                 const float ProjectionScale)
{
	bOutWasSuccessful = false;
	if (not IsValid(WorldContextObject))
	{
		return OriginalLocation;
	}
	const FVector RTSExtent = DeveloperSettings::GamePlay::Navigation::RTSToNavProjectionExtent * ProjectionScale;
	const FVector Extent = bExtentInZ ? RTSExtent : RTSExtent * FVector(1.f, 1.f, 0.f);


	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(WorldContextObject->GetWorld());
	if (not NavSys)
	{
		return OriginalLocation;
	}
	FNavLocation ProjectedLocation;
	

	if (NavSys->ProjectPointToNavigation(OriginalLocation, ProjectedLocation, Extent))
	{
		// Successfully projected onto navmesh
		bOutWasSuccessful = true;
		return ProjectedLocation.Location;
	}
	return OriginalLocation;
}
