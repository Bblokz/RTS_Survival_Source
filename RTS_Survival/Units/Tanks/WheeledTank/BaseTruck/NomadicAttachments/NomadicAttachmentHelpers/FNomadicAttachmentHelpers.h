#pragma once
#include "CoreMinimal.h"
struct FTrainingOption;

struct FNomadicAttachmentHelpers
{
	static void StartAttachmentsMontages(
		TArray<AActor*>* Attachments);

	static void StopAttachmentsMontages(
		TArray<AActor*>* Attachments);
};
