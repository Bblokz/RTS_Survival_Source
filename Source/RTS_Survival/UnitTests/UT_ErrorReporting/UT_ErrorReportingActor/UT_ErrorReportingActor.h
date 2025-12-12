// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UT_ErrorReportingActor.generated.h"

class AUT_ErrorReportManager;

/**
 * @brief Lightweight world actor that can report unit-test errors to the central manager.
 * The manager injects itself at BeginPlay so ReportError can propagate.
 * @note ReportError: call from Blueprint to post an error with message and world location.
 */
UCLASS()
class RTS_SURVIVAL_API AErrorReportingActor : public AActor
{
	GENERATED_BODY()

public:
	AErrorReportingActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/** Manager setter used by AUT_ErrorReportManager during BeginPlay discovery. */
	void SetErrorReportManager(AUT_ErrorReportManager* InManager);

	/** Report an error to the central manager (BP-callable entry point). */
	UFUNCTION(BlueprintCallable, Category = "ErrorReporting")
	void ReportError(const FString& Message, const FVector& WorldLocation);

	/** Returns whether the manager reference is valid; logs standardized error when not. */
	bool GetIsValidErrorReportManager() const;

private:
	// Not owned; injected by AUT_ErrorReportManager at BeginPlay.
	UPROPERTY()
	TWeakObjectPtr<AUT_ErrorReportManager> M_ErrorReportManager;
};
