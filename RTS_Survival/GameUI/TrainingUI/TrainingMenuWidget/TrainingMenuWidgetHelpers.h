// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/PanelWidget.h"

namespace TrainingMenuWidgets
{
	/**
	 * @brief Lets the training widgets find the buttons a designer placed inside a bound panel, so that the
	 * widget blueprint decides how many there are and how they are arranged.
	 * @param Panel The panel to search; nested panels such as one box per row are searched too.
	 * @param OutWidgets The widgets of the asked type, in widget tree order.
	 */
	template <typename TWidgetType>
	void CollectWidgetsInPanel(UPanelWidget* Panel, TArray<TWidgetType*>& OutWidgets)
	{
		if (not IsValid(Panel))
		{
			return;
		}
		for (UWidget* ChildWidget : Panel->GetAllChildren())
		{
			if (TWidgetType* TypedChild = Cast<TWidgetType>(ChildWidget))
			{
				OutWidgets.Add(TypedChild);
				continue;
			}
			CollectWidgetsInPanel(Cast<UPanelWidget>(ChildWidget), OutWidgets);
		}
	}
}
