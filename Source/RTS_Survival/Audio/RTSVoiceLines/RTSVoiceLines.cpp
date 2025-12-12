#include "RTSVoicelines.h"
#include "Algo/RandomShuffle.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

USoundBase* FVoiceLineData::GetVoiceLine()
{
	if (VoiceLines.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Empty VoiceLine array! In FVoiceLineData::GetVoiceLine()");
		return nullptr;
	}
	const bool bAreVoiceLinesAlmostOutOfBounds = (M_VoiceLineIndex >= (VoiceLines.Num() - 1));
	if (M_VoiceLineIndex == INDEX_NONE || bAreVoiceLinesAlmostOutOfBounds)
	{
		// VoiceLines have not been shuffled yet or we are at the end and need to reshuffle.
		ReshuffleVoiceLines(M_LastPlayedVoiceLine);
		M_VoiceLineIndex = 0;
		if (DeveloperSettings::Debugging::GAudioController_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString(
				"\n Reshuffled voicelines amount: " + FString::SanitizeFloat(VoiceLines.Num()));
			RTSFunctionLibrary::PrintString("VoiceLineIndex: " + FString::FromInt(M_VoiceLineIndex));
		}
	}
	else
	{
		M_VoiceLineIndex++;
		if (DeveloperSettings::Debugging::GAudioController_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("VoiceLines incremented to: " + FString::FromInt(M_VoiceLineIndex));
		}
	}
	if (not EnsureVoiceLineIndexIsValid(M_VoiceLineIndex))
	{
		return nullptr;
	}
	M_LastPlayedVoiceLine = VoiceLines[M_VoiceLineIndex];
	return M_LastPlayedVoiceLine;
}

void FVoiceLineData::ReshuffleVoiceLines(USoundBase* LastPlayedVoiceLine)
{
	ValidateVoiceLines();
	if (LastPlayedVoiceLine != nullptr)
	{
		// Remove the last played voiceline from the array.
		VoiceLines.Remove(LastPlayedVoiceLine);
	}
	// Shuffle the array.
	Algo::RandomShuffle(VoiceLines);
	// Add the last played voiceline back to the array.
	if (LastPlayedVoiceLine != nullptr)
	{
		VoiceLines.Add(LastPlayedVoiceLine);
	}
}

void FVoiceLineData::ValidateVoiceLines()
{
	TArray<USoundBase*> ValidVoiceLines;
	for (auto EachVoiceLine : VoiceLines)
	{
		if (!IsValid(EachVoiceLine))
		{
			RTSFunctionLibrary::ReportError("Invalid VoiceLine in FVoiceLineData ");
			continue;
		}
		ValidVoiceLines.Add(EachVoiceLine);
	}
	VoiceLines = ValidVoiceLines;
}

bool FVoiceLineData::EnsureVoiceLineIndexIsValid(int32& Index) const
{
	if (VoiceLines.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("attempted to find valid VoiceLine Index but the array is empty."
			"Possibly an invalid voiceline was removed."
			"\n see FVoiceLineData::EnsureVoiceLineIndexIsValid() in RTSVoicelines.h");
		return false;
	}
	if (Index < 0)
	{
		Index = 0;
	}
	else if (Index >= VoiceLines.Num())
	{
		Index = VoiceLines.Num() - 1;
	}
	return true;
}

bool FUnitVoiceLinesData::GetVoiceLinesForType(const ERTSVoiceLine VoiceLineType, FVoiceLineData*& OutVoiceLines)
{
	if (FVoiceLineData* Found = VoiceLines.Find(VoiceLineType))
	{
		OutVoiceLines = Found;
		return true;
	}

	const FString ForUnitVlData = UEnum::GetValueAsString(UnitVoiceLineType);
	RTSFunctionLibrary::ReportError(
		TEXT("Cannot find voice-line type '") +
		UEnum::GetValueAsString(VoiceLineType) +
		TEXT("' in FUnitVoiceLinesData") +
		"\n for unit voice line type: " + ForUnitVlData);
	return false;
}

bool FAnnouncerVoiceLineData::GetVoiceLinesForType(const EAnnouncerVoiceLineType VoiceLineType,
                                                   FVoiceLineData*& OutVoiceLines)
{
	if( FVoiceLineData* Found = VoiceLines.Find(VoiceLineType))
	{
		OutVoiceLines = Found;
		return true;
	}
	RTSFunctionLibrary::ReportError(
		TEXT("Cannot find announcer voice-line type '") +
		UEnum::GetValueAsString(VoiceLineType) +
		TEXT("' in FAnnouncerVoiceLineData"));
	return false;
}
