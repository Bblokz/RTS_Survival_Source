#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftSocketState/AircraftSocketState.h"

#include "UAirBaseAnimInstance.generated.h"


// Keeps no internal state of any of the aircraft sockets; only plays specific animations depending on the state provided.
// Expects the user to set as many animations in CurrentAnimSequencesForSockets as there are supported sockets in the airbase.
UCLASS()
class RTS_SURVIVAL_API UAirBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// May later be expanded with supplying an AircraftOwner if needed.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitAirbaseAnimInstance(const int32 ExpectedAmountOfSocketsSupported);
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetAnimationSequenceForState(const EAircraftSocketState NewState, const int32 SocketIndex);
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void PlayMontageForState(const EAircraftSocketState NewState, const int32 SocketIndex, const float PlayTime);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UAnimSequence*> OpenAnimSequenceForSockets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UAnimSequence*> ClosedAnimSequenceForSockets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UAnimMontage*> ClosedToOpenMontagesForSockets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UAnimMontage*> OpenToClosedMontagesForSockets;

	// The sequence set for the index that identifies the socket.
	// Set these at the start to closed animations (no aircraft)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UAnimSequence*> CurrentAnimSequencesForSockets;

private:
	bool EnsureValidIndexForSocket(const EAircraftSocketState NewState, const int32 SocketIndex) const;

	void ReportErrorInvalidSocketIndex(const int32 SocketIndex, const FString& ContextText) const;

	// Validates animations; reports error if any are null.
	void ValidateData(const int32 AmountSupportedSockets);

};
