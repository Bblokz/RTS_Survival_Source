#include "FRTS_PhysicsHelper.h"

#include "RTSSurfaceSubtypes.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ERTSSurfaceType FRTS_PhysicsHelper::GetRTSSurfaceType(TWeakObjectPtr<UPhysicalMaterial> PhysicalMaterial)
{
	if (not PhysicalMaterial.IsValid())
	{
		if (DeveloperSettings::Debugging::GPhysicalMaterialSurfaces_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("No physical material found, defaulting to Sand", FColor::Red);
		}
		return ERTSSurfaceType::Sand;
	}
	switch (PhysicalMaterial->SurfaceType)
	{
	case SurfaceType1:
		return ERTSSurfaceType::Metal;
	case SurfaceType2:
		return ERTSSurfaceType::Sand;
	case SurfaceType3:
		return ERTSSurfaceType::Flesh;
	case SurfaceType4:
		return ERTSSurfaceType::Air;
	case SurfaceType5:
		return ERTSSurfaceType::Stone;
	default:
		// Handle unknown surface types
		return ERTSSurfaceType::Metal;
	}
}

void FRTS_PhysicsHelper::PrintSurfaceType(ERTSSurfaceType SurfaceType)
{
	FString SurfaceTypeString = GetSurfaceTypeString(SurfaceType);
	RTSFunctionLibrary::PrintString("Surface: " + SurfaceTypeString, FColor::Green);
}

void FRTS_PhysicsHelper::DrawTextSurfaceType(ERTSSurfaceType SurfaceType, const FVector& Location, UWorld* World,
                                             const float Duration)
{
	if (not IsValid(World))
	{
		return;
	}
	// Draw surface type string at location
	FString SurfaceTypeString = GetSurfaceTypeString(SurfaceType);

	if (Duration <= 0)
	{
		DrawDebugString(World, Location, SurfaceTypeString, nullptr, FColor::Green, 5);
		return;
	}
	DrawDebugString(World, Location, SurfaceTypeString, nullptr, FColor::Green, Duration);
}

FString FRTS_PhysicsHelper::GetSurfaceTypeString(ERTSSurfaceType SurfaceType)
{
	FString SurfaceTypeString = "Not Set";
	switch (SurfaceType)
	{
	case ERTSSurfaceType::Metal:
		SurfaceTypeString = "Metal";
		break;
	case ERTSSurfaceType::Sand:
		SurfaceTypeString = "Sand";
		break;
	case ERTSSurfaceType::Flesh:
		SurfaceTypeString = "Flesh";
		break;
		case ERTSSurfaceType::Air:
			return "Default";
	case ERTSSurfaceType::Stone:
		return "Stone";
	default:
		SurfaceTypeString = "Unknown";
		break;
	}
	return SurfaceTypeString;
}
