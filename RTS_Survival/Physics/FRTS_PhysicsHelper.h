#pragma once

enum class ERTSSurfaceType : uint8;

class FRTS_PhysicsHelper
{
public:
	static ERTSSurfaceType GetRTSSurfaceType(TWeakObjectPtr<UPhysicalMaterial> PhysicalMaterial);
	static void PrintSurfaceType(ERTSSurfaceType SurfaceType);
	static void DrawTextSurfaceType(ERTSSurfaceType SurfaceType, const FVector& Location, UWorld* World, const float Duration);

	static FString GetSurfaceTypeString(ERTSSurfaceType SurfaceType);
};
