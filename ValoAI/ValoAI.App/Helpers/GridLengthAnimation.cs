using System.Windows;
using System.Windows.Media.Animation;

namespace ValoAI.App.Helpers;

public class GridLengthAnimation : AnimationTimeline
{
    public GridLength From
    {
        get => (GridLength)GetValue(FromProperty);
        set => SetValue(FromProperty, value);
    }
    public static readonly DependencyProperty FromProperty =
        DependencyProperty.Register(nameof(From), typeof(GridLength),
            typeof(GridLengthAnimation));

    public GridLength To
    {
        get => (GridLength)GetValue(ToProperty);
        set => SetValue(ToProperty, value);
    }
    public static readonly DependencyProperty ToProperty =
        DependencyProperty.Register(nameof(To), typeof(GridLength),
            typeof(GridLengthAnimation));

    public IEasingFunction? EasingFunction { get; set; }

    public override Type TargetPropertyType => typeof(GridLength);

    protected override Freezable CreateInstanceCore() => new GridLengthAnimation();

    public override object GetCurrentValue(object defaultOriginValue,
        object defaultDestinationValue, AnimationClock animationClock)
    {
        double progress = animationClock.CurrentProgress ?? 0;
        if (EasingFunction != null)
            progress = EasingFunction.Ease(progress);

        double from = From.Value;
        double to = To.Value;
        return new GridLength(from + (to - from) * progress);
    }
}