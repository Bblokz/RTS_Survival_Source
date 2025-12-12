#include "FNomadicAttachmentHelpers.h"

#include "GameFramework/Actor.h"

#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicAttachments/NomadicSkeletalAttachment/NomadicSkeletalAttachment.h"

void FNomadicAttachmentHelpers::StartAttachmentsMontages(
	TArray<AActor*>* Attachments)
{
	if(not Attachments)
	{
		return;
	}
	for(const auto EachAttachment : *Attachments)
	{
		if(not IsValid(EachAttachment))
		{
			continue;
		}
		ANomadicAttachmentSkeletal* SkeletalAttachment =
			Cast<ANomadicAttachmentSkeletal>(EachAttachment);
		if(not IsValid(SkeletalAttachment))
		{
			continue;
		}
		// Allow for montages as we are building something.
		SkeletalAttachment->SetAttachmentAnimationState(EAttachmentAnimationState::MontageAndAO);
	}
}

void FNomadicAttachmentHelpers::StopAttachmentsMontages(
	TArray<AActor*>* Attachments)
{
	
	if(not Attachments)
	{
		return;
	}
	for(const auto EachAttachment : *Attachments)
	{
		if(not IsValid(EachAttachment))
		{
			continue;
		}
		ANomadicAttachmentSkeletal* SkeletalAttachment =
			Cast<ANomadicAttachmentSkeletal>(EachAttachment);
		if(not IsValid(SkeletalAttachment))
		{
			continue;
		}
		// Cancel: only AO, no montages.
		SkeletalAttachment->SetAttachmentAnimationState(EAttachmentAnimationState::AOOnly);
	}
}
