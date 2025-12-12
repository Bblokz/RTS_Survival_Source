#include "ArmorOffsetDecorator.h"
#include "Framework/Text/TextDecorators.h"
#include "Components/RichTextBlock.h"
#include "Framework/Text/SlateTextRun.h"

// TODO THIS DOES NOT WORK; parsing problem with arguments...
class FArmorTextRun : public FSlateTextRun
{
public:
    FArmorTextRun(const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle, const FVector2D& InOffset, const FTextRange& InRange)
        : FSlateTextRun(InRunInfo, InText, InStyle, InRange), Offset(InOffset)
    {}

    virtual FVector2D Measure(int32 BeginIndex, int32 EndIndex, float Scale, const FRunTextContext& TextContext) const override
    {
        FVector2D MeasuredSize = FSlateTextRun::Measure(BeginIndex, EndIndex, Scale, TextContext);
        MeasuredSize += Offset;
        return MeasuredSize;
    }

    virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        // Apply the offset before painting
        FGeometry AdjustedGeometry = AllottedGeometry;
        AdjustedGeometry.AppendTransform(FSlateLayoutTransform(Offset));

        return FSlateTextRun::OnPaint(PaintArgs, TextArgs, AdjustedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    }

private:
    FVector2D Offset;
};

class FArmorTextDecorator : public FRichTextDecorator
{
public:
    FArmorTextDecorator(URichTextBlock* InOwner)
        : FRichTextDecorator(InOwner) {}

    virtual bool Supports(const FTextRunParseResults& RunInfo, const FString& Text) const override
    {
        return RunInfo.Name.Equals(TEXT("armor"));
    }

protected:
virtual void CreateDecoratorText(const FTextRunInfo& RunInfo, FTextBlockStyle& InOutTextStyle, FString& InOutString) const override
{
    FVector2D Offset(0.0f, 0.0f);

    // Extracting parameters from the MetaData map
    if (const FString* XValue = RunInfo.MetaData.Find(TEXT("x")))
    {
        // Assuming the value is enclosed in single quotes, extract the numeric part
        FString CleanXValue = XValue->Replace(TEXT("'"), TEXT(""));
        Offset.X = FCString::Atof(*CleanXValue);
    }

    if (const FString* YValue = RunInfo.MetaData.Find(TEXT("y")))
    {
        // Assuming the value is enclosed in single quotes, extract the numeric part
        FString CleanYValue = YValue->Replace(TEXT("'"), TEXT(""));
        Offset.Y = FCString::Atof(*CleanYValue);
    }

    // Example: Modifying the string to include the offset (for debugging purposes)
    InOutString = FString::Printf(TEXT("%s (Offset: X=%.0f, Y=%.0f)"), *InOutString, Offset.X, Offset.Y);
}
};

// TODO THIS DOES NOT WORK; parsing problem with arguments...
TSharedPtr<ITextDecorator> UArmorOffsetDecorator::CreateDecorator(URichTextBlock* InOwner)
{
    return MakeShareable(new FArmorTextDecorator(InOwner));
}
