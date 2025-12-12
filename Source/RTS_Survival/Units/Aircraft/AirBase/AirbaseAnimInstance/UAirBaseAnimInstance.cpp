#include "UAirBaseAnimInstance.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

void UAirBaseAnimInstance::InitAirbaseAnimInstance(const int32 ExpectedAmountOfSocketsSupported)
{
	ValidateData(ExpectedAmountOfSocketsSupported);
}

void UAirBaseAnimInstance::SetAnimationSequenceForState(const EAircraftSocketState NewState, const int32 SocketIndex)
{
	if (NewState == EAircraftSocketState::None)
	{
		RTSFunctionLibrary::ReportError("Attempted to set sequence for an aircraft socket state of None!"
			"\n ignoring.");
		return;
	}
	if (NewState == EAircraftSocketState::Closing || NewState == EAircraftSocketState::Opening)
	{
		RTSFunctionLibrary::ReportError("Attempted to set sequence for an aircraft socket state that uses montages!"
			"\n ignoring.");
		return;
	}
	if (not EnsureValidIndexForSocket(NewState, SocketIndex))
	{
		return;
	}
	CurrentAnimSequencesForSockets[SocketIndex] =
		NewState == EAircraftSocketState::Open
			? OpenAnimSequenceForSockets[SocketIndex]
			: ClosedAnimSequenceForSockets[SocketIndex];
}

void UAirBaseAnimInstance::PlayMontageForState(const EAircraftSocketState NewState, const int32 SocketIndex,
                                               const float PlayTime)
{
	if (NewState == EAircraftSocketState::None)
	{
		RTSFunctionLibrary::ReportError("Attempted to play montage for an aircraft socket state of None!"
			"\n ignoring.");
		return;
	}
	if (NewState == EAircraftSocketState::Open || NewState == EAircraftSocketState::Closed)
	{
		RTSFunctionLibrary::ReportError("Attempted to play montage for an aircraft socket state that uses sequences!"
			"\n ignoring.");
		return;
	}
	if (not EnsureValidIndexForSocket(NewState, SocketIndex))
	{
		return;
	}
	UAnimMontage* SelectedMontage = NewState == EAircraftSocketState::Opening
		                                ? ClosedToOpenMontagesForSockets[SocketIndex]
		                                : OpenToClosedMontagesForSockets[SocketIndex];

	float PlayRate = 1.0f;
	if (PlayTime > 0.0f)
	{
		const float MontageLength = SelectedMontage->GetPlayLength();
		PlayRate = MontageLength / PlayTime;
	}
	Montage_Play(SelectedMontage, PlayRate);
}

void UAirBaseAnimInstance::ValidateData(const int32 AmountSupportedSockets)
{
	if (OpenAnimSequenceForSockets.Num() != AmountSupportedSockets)
	{
		RTSFunctionLibrary::ReportError(
			"UAirBaseAnimInstance::OpenAnimSequenceForSockets does not have the correct amount of entries!");
	}
	for (int32 i = 0; i < OpenAnimSequenceForSockets.Num(); i++)
	{
		if (not IsValid(OpenAnimSequenceForSockets[i]))
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("UAirBaseAnimInstance::OpenAnimSequenceForSockets[%d] is null!"), i));
		}
	}

	if (ClosedAnimSequenceForSockets.Num() != AmountSupportedSockets)
	{
		RTSFunctionLibrary::ReportError(
			"UAirBaseAnimInstance::ClosedAnimSequenceForSockets does not have the correct amount of entries!");
	}
	for (int32 i = 0; i < ClosedAnimSequenceForSockets.Num(); i++)
	{
		if (not IsValid(ClosedAnimSequenceForSockets[i]))
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("UAirBaseAnimInstance::ClosedAnimSequenceForSockets[%d] is null!"), i));
		}
	}

	if (ClosedToOpenMontagesForSockets.Num() != AmountSupportedSockets)
	{
		RTSFunctionLibrary::ReportError(
			"UAirBaseAnimInstance::ClosedToOpenMontagesForSockets does not have the correct amount of entries!");
	}
	for (int32 i = 0; i < ClosedToOpenMontagesForSockets.Num(); i++)
	{
		if (not IsValid(ClosedToOpenMontagesForSockets[i]))
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("UAirBaseAnimInstance::ClosedToOpenMontagesForSockets[%d] is null!"), i));
		}
	}

	if (OpenToClosedMontagesForSockets.Num() != AmountSupportedSockets)
	{
		RTSFunctionLibrary::ReportError(
			"UAirBaseAnimInstance::OpenToClosedMontagesForSockets does not have the correct amount of entries!");
	}
	for (int32 i = 0; i < OpenToClosedMontagesForSockets.Num(); i++)
	{
		if (not IsValid(OpenToClosedMontagesForSockets[i]))
		{
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("UAirBaseAnimInstance::OpenToClosedMontagesForSockets[%d] is null!"), i));
		}
	}
}

bool UAirBaseAnimInstance::EnsureValidIndexForSocket(const EAircraftSocketState NewState, const int32 SocketIndex) const
{
	// Check if we are looking to change a sequence; in that case check if we support this index for CurrentAnimSequencesForSockets.
	if (NewState == EAircraftSocketState::Open || NewState == EAircraftSocketState::Closed)
	{
		if (not CurrentAnimSequencesForSockets.IsValidIndex(SocketIndex))
		{
			ReportErrorInvalidSocketIndex(SocketIndex,
			                              "UAirBaseAnimInstance::SetAnimationSequenceForState - CurrentAnimSequencesForSockets");
			return false;
		}
	}
	switch (NewState)
	{
	case EAircraftSocketState::None:
		RTSFunctionLibrary::ReportError("Attempted to set sequence for an aircraft socket state of None!"
			"\n ignoring.");
		return false;
	case EAircraftSocketState::Open:
		if (not OpenAnimSequenceForSockets.IsValidIndex(SocketIndex))
		{
			ReportErrorInvalidSocketIndex(SocketIndex,
			                              "UAirBaseAnimInstance::SetAnimationSequenceForState - Open state");
			return false;
		}
	case EAircraftSocketState::Opening:
		if (not ClosedToOpenMontagesForSockets.IsValidIndex(SocketIndex))
		{
			ReportErrorInvalidSocketIndex(SocketIndex,
			                              "UAirBaseAnimInstance::SetAnimationSequenceForState - Opening state");
			return false;
		}
	case EAircraftSocketState::Closed:
		if (not ClosedAnimSequenceForSockets.IsValidIndex(SocketIndex))
		{
			ReportErrorInvalidSocketIndex(SocketIndex,
			                              "UAirBaseAnimInstance::SetAnimationSequenceForState - Closed state");
			return false;
		}
	case EAircraftSocketState::Closing:
		if (not OpenToClosedMontagesForSockets.IsValidIndex(SocketIndex))
		{
			ReportErrorInvalidSocketIndex(SocketIndex,
			                              "UAirBaseAnimInstance::SetAnimationSequenceForState - Closing state");
			return false;
		}
	}
	return true;
}

void UAirBaseAnimInstance::ReportErrorInvalidSocketIndex(const int32 SocketIndex, const FString& ContextText) const
{
	const FString IndexOutOfBoundsError = FString::Printf(
		TEXT("%s: SocketIndex %d is out of bounds."), *ContextText, SocketIndex);
	const FString OwnerName = IsValid(GetOwningActor()) ? GetOwningActor()->GetName() : TEXT("InvalidOwner");
	const FString ObjectContext = "For UAirbaseAnimInstance owned by " + OwnerName + ".";
	RTSFunctionLibrary::ReportError(IndexOutOfBoundsError + "\n" + ContextText + "\n" + ObjectContext);
}
