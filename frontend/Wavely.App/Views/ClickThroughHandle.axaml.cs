using System;
using Avalonia.Controls;
using Avalonia.Input;

namespace Wavely.App.Views;

/// <summary>
/// Always-clickable handle shown only while the overlay is click-through: WS_EX_TRANSPARENT
/// routes every click straight through the main window's entire native area, so this "safe
/// zone" to disable click-through must be its own top-level window (own HWND) to stay clickable.
/// </summary>
public partial class ClickThroughHandle : Window
{
    public event EventHandler? HandleClicked;

    public ClickThroughHandle()
    {
        InitializeComponent();
        PointerPressed += (_, e) =>
        {
            HandleClicked?.Invoke(this, EventArgs.Empty);
            e.Handled = true;
        };
    }
}
