// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
/**
 * 
 */

class UBoxComponent;
DECLARE_LOG_CATEGORY_EXTERN(LogRTS, Log, All);

class RTS_SURVIVAL_API RTSFunctionLibrary
{
public:
	RTSFunctionLibrary();
	~RTSFunctionLibrary();

	static void PrintString(const FString& StringToPrint = "",
	                        FColor Color = FColor::White, const float Time = 8);
	static void PrintString(const FVector& Location, const UObject* WorldContextObject,
	                        const FString& StringToPrint = "", FColor Color = FColor::White, const float Time = 8);
	static void Print(UObject* ObjectWeprint);
	static void NullWarning(FString VariableName);

	static int32 RoundToNearestMultipleOf(const int32 Value, const int32 Multiple);


	/** @brief To report an error when we when fail to initialize a variable in an init or post init component function.
	 * @param Object The boject that has the invalid or null variable.
	 * @param VariableName Name of the invalid variable.
	 * @param FunctionName The name of the function that encountered the error.
	 */
	static void ReportNullErrorInitialisation(
		const UObject* Object,
		const FString& VariableName,
		const FString& FunctionName);

	/**
	 * @brief To report an error when a variable was not initialized before being used.
	 * @param Object The object that has the invalid or null variable.
	 * @param VariableName Name of the invalid variable.
	 * @param FunctionName The name of the function that encountered the error.
	 * @param DerivedBpContext The BP context in which the variable shoudl have been intialised.
	 */
	static void ReportErrorVariableNotInitialised(
		const UObject* Object,
		const FString& VariableName,
		const FString& FunctionName,
		const AActor* DerivedBpContext = nullptr);
	

	/**
	 * @brief To report an error when a variable was not initialized before being used.
	 * @param Object The object that has the invalid or null variable.
	 * @param VariableName Name of the invalid variable.
	 * @param FunctionName The name of the function that encountered the error.
	 * @param DerivedBpContext The BP context in which the variable shoudl have been intialised.
	 */
	static void ReportErrorVariableNotInitialised_Object(
		const UObject* Object,
		const FString& VariableName,
		const FString& FunctionName,
		const UObject* DerivedBpContext);


	/**
	 * To report a null / invalid error on an actor's component.
	 * @param Object The Object that has the invalid component.
	 * @param ComponentName Name of the invalid component.
	 * @param FunctionName The name of the function that encountered the error.
	 */
	static void ReportNullErrorComponent(
		const UObject* Object,
		const FString& ComponentName,
		const FString& FunctionName);

	/**
	 * To report that a cast failed where we expected it to succeed.
	 * @param CastedObjectName The name of the object that we attempted to cast.
	 * @param ExpectedType The expected type resulting from the cast.
	 * @param FunctionName The name of the function where the cast failed.
	 */
	static void ReportFailedCastError(const FString& CastedObjectName, const FString& ExpectedType,
	                                  const FString& FunctionName);

	/**
	 * To any type of error the game encounters.
	 * @param Text Description of the error.
	 * @param ResetErrorSuppression Whether the error is critical and should stop the game.
	 */
	static void ReportError(FString Text, const bool ResetErrorSuppression = false);

	static void ReportWarning(FString Text);

	static void DisplayNotification(const FText& Text);
	static void DisplayNotification(const FString& Text);

	static void DrawDebugAtLocation(
		const UObject* WorldContext,
		const FVector& Location,
		const FString& Text,
		FColor Color = FColor::White,
		float Duration = 5.0f);


	static bool RandomBool();

	static void PrintToLog(FString Text, bool bIsWarning = false);

	static void SetRotatorToZeroes(FRotator& Rotator);

	/**
	 * @brief Returns the smallest singed angle between the Source and Dest Vector.
	 * The Abs value of the angle is calculated with FindLookAtRotation.Yaw, the cross product between
	 * the two vectors gives a vector perpendicular to both of them. Using the dot product we check whether
	 * This cross product is pointing in the same direction as the normal vector of the plane on which the two vectors
	 * live.
	 * @param Source The position from which we start measuring.
	 * @param Dest The destination vector to find the angle to.
	 * @param NormalToPlane The Normal vector of the plane on which the vector live.
	 * @return Signed shortest angle between the two vectors.
	 */
	static float SingedAngleTo(
		const FVector Source,
		const FVector Dest,
		const FVector NormalToPlane = FVector(0, 0, 1));

	/**
	 * @brief To check if an actor is valid and not killed.
	 * @param ActorToCheck The actor to check for validity.
	 * @return True if the actor is valid and not pending kill.
	 */
	static bool RTSIsValid(AActor* ActorToCheck);

	/**
	 * @brief Check if the actor is a valid target for the unit, is alive and
	 * is visible in the FOW.
	 * @param ActorToCheck The actor to check for target validity.
	 * @param EnemyPlayerOfActor The enemy of th actor targeted.
	 * @return True if the actor is valid, alive and visible in fow.
	 * @note If player 1 is the enemy then this actor must not be hidden in fow (visible to player 1).
	 * @note if Player 1 is the ally then this actor must be in the enemy vision (tagged with enemy vision).
	 */
	static bool RTSIsVisibleTarget(AActor* ActorToCheck, const int32 EnemyPlayerOfActor);


	static void DebugWeapons(const FString& Message, FColor Color, const bool bIsTrace = false);


	static USoundAttenuation* CreateSoundAttenuation(const float Range);

	static FVector GetLocationProjected(const UObject* WorldContextObject, const FVector& OriginalLocation,
	                                    const bool bExtentInZ, bool& bOutWasSuccessful,
	                                    const float ProjectionScale = 1.f);

	
};
