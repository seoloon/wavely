using Avalonia.Controls;
using Wavely.App.ViewModels;

namespace Wavely.App.Views;

public partial class SettingsWindow : Window
{
    public SettingsWindow(SettingsViewModel viewModel)
    {
        DataContext = viewModel;
        InitializeComponent();
    }
}
