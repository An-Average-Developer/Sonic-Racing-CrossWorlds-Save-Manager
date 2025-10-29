using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using SonicRacingSaveManager.Common.Infrastructure;
using SonicRacingSaveManager.Features.MemoryEditor.Models;
using SonicRacingSaveManager.Features.MemoryEditor.Services;
using SonicRacingSaveManager.Features.MemoryEditor.Views;

namespace SonicRacingSaveManager.Features.MemoryEditor.ViewModels
{
    public class MemoryEditorViewModel : INotifyPropertyChanged
    {
        private readonly MemoryEditorService _memoryService;
        private readonly DispatcherTimer _updateTimer;
        private readonly DispatcherTimer _autoAttachTimer;
        private MemoryValue? _selectedValue;
        private bool _isAttached;
        private string _statusMessage = "Searching for game process...";
        private bool _autoRefresh = true;
        private int _lastKnownGoodTicketValue = 0;
        private string _selectedTool = "menu";

        public MemoryEditorViewModel()
        {
            _memoryService = new MemoryEditorService();

            AttachToProcessCommand = new RelayCommand(AttachToProcess, CanAttachToProcess);
            DetachFromProcessCommand = new RelayCommand(DetachFromProcess, CanDetachFromProcess);
            RefreshValuesCommand = new RelayCommand(RefreshValues, CanRefreshValues);
            ApplyValueCommand = new RelayCommand(ApplyValue, CanApplyValue);
            SelectTicketEditorCommand = new RelayCommand(() => SelectedTool = "tickets");
            BackToMenuCommand = new RelayCommand(() => SelectedTool = "menu");

            MemoryValues = new ObservableCollection<MemoryValue>
            {
                new MemoryValue
                {
                    Name = "Tickets",
                    Description = "In-game currency for purchases",
                    BaseAddress = 0x08472CE0,
                    Offsets = new int[] { 0x10, 0x10, 0x1D8, 0x108, 0x2D8, 0xD8, 0x58 },
                    CurrentValue = 0,
                    NewValue = 0
                }
            };

            _updateTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(1)
            };
            _updateTimer.Tick += UpdateTimer_Tick;

            _autoAttachTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(2)
            };
            _autoAttachTimer.Tick += AutoAttachTimer_Tick;
            _autoAttachTimer.Start();

            TryAutoAttach();
        }

        public ObservableCollection<MemoryValue> MemoryValues { get; }

        public MemoryValue? SelectedValue
        {
            get => _selectedValue;
            set
            {
                _selectedValue = value;
                OnPropertyChanged();
                ((RelayCommand)ApplyValueCommand).RaiseCanExecuteChanged();
            }
        }

        public bool IsAttached
        {
            get => _isAttached;
            set
            {
                _isAttached = value;
                OnPropertyChanged();
                ((RelayCommand)AttachToProcessCommand).RaiseCanExecuteChanged();
                ((RelayCommand)DetachFromProcessCommand).RaiseCanExecuteChanged();
                ((RelayCommand)RefreshValuesCommand).RaiseCanExecuteChanged();
                ((RelayCommand)ApplyValueCommand).RaiseCanExecuteChanged();
            }
        }

        public string StatusMessage
        {
            get => _statusMessage;
            set
            {
                _statusMessage = value;
                OnPropertyChanged();
            }
        }

        public bool AutoRefresh
        {
            get => _autoRefresh;
            set
            {
                _autoRefresh = value;
                OnPropertyChanged();

                if (_autoRefresh && IsAttached)
                {
                    _updateTimer.Start();
                }
                else
                {
                    _updateTimer.Stop();
                }
            }
        }

        public string SelectedTool
        {
            get => _selectedTool;
            set
            {
                _selectedTool = value;
                OnPropertyChanged();
            }
        }

        public ICommand AttachToProcessCommand { get; }
        public ICommand DetachFromProcessCommand { get; }
        public ICommand RefreshValuesCommand { get; }
        public ICommand ApplyValueCommand { get; }
        public ICommand SelectTicketEditorCommand { get; }
        public ICommand BackToMenuCommand { get; }

        private bool CanAttachToProcess(object? parameter)
        {
            return !IsAttached;
        }

        private void AttachToProcess(object? parameter)
        {
            try
            {
                bool success = _memoryService.AttachToProcess();

                if (success)
                {
                    IsAttached = true;
                    StatusMessage = "Successfully attached to game process";
                    _autoAttachTimer.Stop();

                    RefreshValues(null);

                    if (AutoRefresh)
                    {
                        _updateTimer.Start();
                    }
                }
                else
                {
                    StatusMessage = "Failed to attach. Make sure the game is running.";
                }
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error attaching to process: {ex.Message}";
            }
        }

        private bool CanDetachFromProcess(object? parameter)
        {
            return IsAttached;
        }

        private void DetachFromProcess(object? parameter)
        {
            try
            {
                _updateTimer.Stop();
                _memoryService.DetachFromProcess();
                IsAttached = false;
                _lastKnownGoodTicketValue = 0;
                StatusMessage = "Detached from game process. Searching for game...";

                _autoAttachTimer.Start();
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error detaching from process: {ex.Message}";
            }
        }

        private bool CanRefreshValues(object? parameter)
        {
            return IsAttached;
        }

        private void RefreshValues(object? parameter)
        {
            if (!IsAttached)
                return;

            try
            {
                foreach (var memValue in MemoryValues)
                {
                    int value = _memoryService.ReadValue(memValue.BaseAddress, memValue.Offsets);

                    if (memValue.Name == "Tickets")
                    {
                        if (value == 0 && _lastKnownGoodTicketValue > 0)
                        {
                            value = _lastKnownGoodTicketValue;
                            StatusMessage = $"Using cached value (tickets menu may be active) - {DateTime.Now:HH:mm:ss}";
                        }
                        else if (value > 0)
                        {
                            _lastKnownGoodTicketValue = value;
                            StatusMessage = $"Values refreshed at {DateTime.Now:HH:mm:ss}";
                        }
                    }

                    memValue.CurrentValue = value;

                    if (memValue.NewValue == 0 || memValue.NewValue == memValue.CurrentValue)
                    {
                        memValue.NewValue = value;
                    }
                }

                if (!StatusMessage.Contains("cached value"))
                {
                    StatusMessage = $"Values refreshed at {DateTime.Now:HH:mm:ss}";
                }
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error reading values: {ex.Message}";
            }
        }

        private bool CanApplyValue(object? parameter)
        {
            return IsAttached && SelectedValue != null;
        }

        private void ApplyValue(object? parameter)
        {
            if (SelectedValue == null || !IsAttached)
                return;

            try
            {
                bool success = _memoryService.WriteValue(
                    SelectedValue.BaseAddress,
                    SelectedValue.Offsets,
                    SelectedValue.NewValue
                );

                if (success)
                {
                    SelectedValue.CurrentValue = SelectedValue.NewValue;
                    StatusMessage = $"Successfully updated {SelectedValue.Name} to {SelectedValue.NewValue}";

                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        var dialog = new ValueAppliedDialog
                        {
                            Owner = Application.Current.MainWindow
                        };
                        dialog.ShowDialog();
                    });
                }
                else
                {
                    StatusMessage = $"Failed to update {SelectedValue.Name}";

                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        MessageBox.Show(
                            $"Failed to update {SelectedValue.Name}.\n\n" +
                            "Make sure the game is running and you are attached to the process.",
                            "Update Failed",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error
                        );
                    });
                }
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error writing value: {ex.Message}";

                Application.Current.Dispatcher.Invoke(() =>
                {
                    MessageBox.Show(
                        $"Error occurred while updating value:\n\n{ex.Message}",
                        "Error",
                        MessageBoxButton.OK,
                        MessageBoxImage.Error
                    );
                });
            }
        }

        private void UpdateTimer_Tick(object? sender, EventArgs e)
        {
            if (IsAttached && AutoRefresh)
            {
                if (!_memoryService.IsProcessRunning())
                {
                    _updateTimer.Stop();
                    _memoryService.DetachFromProcess();
                    IsAttached = false;
                    _lastKnownGoodTicketValue = 0;
                    StatusMessage = "Game closed. Waiting for game to start...";
                    _autoAttachTimer.Start();
                    return;
                }

                RefreshValues(null);
            }
        }

        private void AutoAttachTimer_Tick(object? sender, EventArgs e)
        {
            TryAutoAttach();
        }

        private void TryAutoAttach()
        {
            if (IsAttached)
                return;

            try
            {
                bool success = _memoryService.AttachToProcess();

                if (success)
                {
                    IsAttached = true;
                    StatusMessage = "Successfully attached to game process";
                    _autoAttachTimer.Stop();

                    RefreshValues(null);

                    if (AutoRefresh)
                    {
                        _updateTimer.Start();
                    }
                }
                else
                {
                    StatusMessage = "Waiting for game to start...";
                }
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error while searching for game: {ex.Message}";
            }
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
