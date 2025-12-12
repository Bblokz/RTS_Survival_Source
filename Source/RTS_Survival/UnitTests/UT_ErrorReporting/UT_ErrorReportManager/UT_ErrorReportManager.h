// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UT_ErrorReportManager.generated.h"

class UW_UT_ErrorReport;
class AErrorReportingActor;
class USoundBase;

/**
 * @brief Central unit-test error report manager.
 * Finds all AErrorReportingActor on BeginPlay and injects itself so they can report.
 * Creates the UW_UT_ErrorReport (non-soft class) and adds it to the viewport for user interaction.
 */
UCLASS()
class RTS_SURVIVAL_API AUT_ErrorReportManager : public AActor
{
	GENERATED_BODY()

public:
	AUT_ErrorReportManager();

	/** Receive an error from a reporting actor; plays sound and forwards to the report widget. */
	/// @brief Entry point for error propagation from reporters.
	/// @param Message Error message to display.
	/// @param WorldLocation World-space location associated with the error (for camera move).
	void ReceiveErrorReport(const FString& Message, const FVector& WorldLocation);

	/** Camera move proxy called by the report widget. */
	void MoveTo(const FVector& WorldLocation) const;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	// --- Defaults / settings ---

	// Non-soft widget class for the error report panel (derived from UW_UT_ErrorReport).
	UPROPERTY(EditDefaultsOnly, Category = "Error Reporting")
	TSubclassOf<UW_UT_ErrorReport> M_ErrorReportWidgetClass;

	// Optional notification sound played whenever an error is reported.
	UPROPERTY(EditDefaultsOnly, Category = "Error Reporting")
	TObjectPtr<USoundBase> M_ReportSound = nullptr;

	// --- Runtime state ---

	// Created and owned UMG widget added to the viewport.
	UPROPERTY()
	TObjectPtr<UW_UT_ErrorReport> M_ErrorReportWidget = nullptr;

	// Helpers
	void BeginPlay_InitFindAndLinkReportingActors();
	void BeginPlay_InitCreateAndSetupWidget();

	bool GetIsValidErrorReportWidgetClass() const;
	bool GetIsValidErrorReportWidget() const;
};
