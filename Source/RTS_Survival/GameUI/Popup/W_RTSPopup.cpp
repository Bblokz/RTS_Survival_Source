// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_RTSPopup.h"

#include "RTSPopupTypes.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "PopupCaller/PopupCaller.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"



void UW_RTSPopup::InitPopupWidget(const TWeakInterfacePtr<IPopupCaller>& PopupCaller)
{
    M_PopupCaller = PopupCaller;
    GetIsValidPopupCaller();
}

void UW_RTSPopup::SetPopupType(const ERTSPopup NewPopup, const FText& NewTitle, const FText& NewMessage)
{
	if(GetIsValidRichText())
	{
		RichPopupText->SetText(NewMessage);
	}
    if(TitleText)
    {
        TitleText->SetText(NewTitle);
    }
    
    
    M_CurrentPopup = NewPopup;
    switch (NewPopup)
    {
    case ERTSPopup::Invalid:
        RTSFunctionLibrary::ReportError("Invalid popup requested!");
        break;

    case ERTSPopup::OKBack:
        SetupOkBackPopup();
        break;

    case ERTSPopup::OKCancel:
        SetupOkCanclePopup();
        break;
    }
}


void UW_RTSPopup::OnClickLeftBtn()
{
    if(not GetIsValidPopupCaller())
    {
        return;
    }
    switch (M_CurrentPopup) {
    case ERTSPopup::Invalid:
        break;
    case ERTSPopup::OKBack:
        M_PopupCaller->OnPopupBack(); 
        break;
    case ERTSPopup::OKCancel:
        M_PopupCaller->OnPopupCancel();
        break;
    }
}

void UW_RTSPopup::OnClickMiddleBtn()
{
}

void UW_RTSPopup::OnclickRightBtn()
{
    if(not GetIsValidPopupCaller())
    {
        return;
    }
    switch (M_CurrentPopup) {
    case ERTSPopup::Invalid:
        break;
    case ERTSPopup::OKBack:
        M_PopupCaller->OnPopupOK();
        break;
    case ERTSPopup::OKCancel:
        M_PopupCaller->OnPopupOK();
        break;
    }
}

bool UW_RTSPopup::GetIsValidRichText()
{
	if(IsValid(RichPopupText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No Rich Text set on Popup widget:" +GetName());
	return false;
}

void UW_RTSPopup::SetupOkBackPopup()
{
    // Set button text for OK and Back
    if (RightText)
    {
        RightText->SetText(FText::FromString("OK"));
    }
    if (LeftText)
    {
        LeftText->SetText(FText::FromString("Back"));
    }
    if(LeftBtn)
    {
        LeftBtn->SetVisibility(ESlateVisibility::Visible);
    }
    if(RightBtn)
    {
        RightBtn->SetVisibility(ESlateVisibility::Visible);
    }

    // Hide the middle button and text
    if (MiddleBtn)
    {
        MiddleBtn->SetVisibility(ESlateVisibility::Hidden);
    }
    if (MiddleText)
    {
        MiddleText->SetVisibility(ESlateVisibility::Hidden);
    }
}


void UW_RTSPopup::SetupOkCanclePopup()
{
    // Set button text for OK and Cancel
    if (RightText)
    {
        RightText->SetVisibility(ESlateVisibility::Visible);
        RightText->SetText(FText::FromString("OK"));
    }
    if (LeftText)
    {
        LeftText->SetVisibility(ESlateVisibility::Visible);
        LeftText->SetText(FText::FromString("Cancel"));
    }
    if(LeftBtn)
    {
        LeftBtn->SetVisibility(ESlateVisibility::Visible);
    }
    if(RightBtn)
    {
        RightBtn->SetVisibility(ESlateVisibility::Visible);
    }

    // Hide the middle button and text
    if (MiddleBtn)
    {
        MiddleBtn->SetVisibility(ESlateVisibility::Hidden);
    }
    if (MiddleText)
    {
        MiddleText->SetVisibility(ESlateVisibility::Hidden);
    }
}

bool UW_RTSPopup::GetIsValidPopupCaller()
{
    if(M_PopupCaller.IsValid())
    {
        return true;
    }
    RTSFunctionLibrary::ReportError("No valid Popup Caller set on Popup widget:" + GetName());
    return false;
}
