#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Audio/RTSVoiceLineHelpers/RTS_VoiceLineHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "WeaponShellType.generated.h"


UENUM(BlueprintType)
enum class EWeaponShellType : uint8
{
	Shell_None,
	Shell_AP,
	Shell_APHE,
	Shell_APHEBC,
	Shell_APCR,
	Shell_HE,
	Shell_HEAT,
	Shell_Fire,
	Shell_Radixite
};

static ERTSVoiceLine GetVoiceLineForShell(const EWeaponShellType ShellType)
{
	switch (ShellType)
	{
	case EWeaponShellType::Shell_None:
		RTSFunctionLibrary::ReportError("None shell type has no voiceline!");
		return ERTSVoiceLine::None;
	case EWeaponShellType::Shell_AP:
		return ERTSVoiceLine::LoadAP;
	case EWeaponShellType::Shell_APHE:
		return ERTSVoiceLine::LoadAP;
	case EWeaponShellType::Shell_APHEBC:
		return ERTSVoiceLine::LoadAP;
	case EWeaponShellType::Shell_APCR:
		return ERTSVoiceLine::LoadAPCR;
	case EWeaponShellType::Shell_HE:
		return ERTSVoiceLine::LoadHE;
	case EWeaponShellType::Shell_HEAT:
		return ERTSVoiceLine::LoadHEAT;
	case EWeaponShellType::Shell_Fire:
		return ERTSVoiceLine::LoadRadixite;
	case EWeaponShellType::Shell_Radixite:
		return ERTSVoiceLine::LoadRadixite;
		break;
	}
	return ERTSVoiceLine::LoadAP;
}
