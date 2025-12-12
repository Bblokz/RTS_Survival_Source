// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UT_ErrorReportingActor.h"

#include "RTS_Survival/UnitTests/UT_ErrorReporting/UT_ErrorReportManager/UT_ErrorReportManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


AErrorReportingActor::AErrorReportingActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AErrorReportingActor::BeginPlay()
{
	Super::BeginPlay();
}

void AErrorReportingActor::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AErrorReportingActor::SetErrorReportManager(AUT_ErrorReportManager* InManager)
{
	M_ErrorReportManager = InManager;
}

bool AErrorReportingActor::GetIsValidErrorReportManager() const
{
	if (M_ErrorReportManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_ErrorReportManager"),
	                                                      TEXT("GetIsValidErrorReportManager"), this);
	return false;
}

void AErrorReportingActor::ReportError(const FString& Message, const FVector& WorldLocation)
{
	if (not GetIsValidErrorReportManager())
	{
		return;
	}
	M_ErrorReportManager->ReceiveErrorReport(Message, WorldLocation);
}
