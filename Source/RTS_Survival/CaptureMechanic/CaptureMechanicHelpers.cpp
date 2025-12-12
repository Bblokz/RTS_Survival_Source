#include "CaptureMechanicHelpers.h"

#include "CaptureInterface/CaptureInterface.h"


ICaptureInterface* FCaptureMechanicHelpers::GetValidCaptureInterface(AActor* TargetActor)
{
	if (!TargetActor) return nullptr;

	if (TargetActor->GetClass()->ImplementsInterface(UCaptureInterface::StaticClass()))
	{
		return Cast<ICaptureInterface>(TargetActor);
	}
	return nullptr;
}
