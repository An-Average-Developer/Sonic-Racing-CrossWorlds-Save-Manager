using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
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
        private bool _isTicketsFrozen = false;
        private bool _isTicketsMultiplied = false;
        private bool _isTicketsMultiply500 = false;

        // Freeze configuration for tickets
        private const long TICKET_FREEZE_ADDRESS = 0x4D83DBD;
        private static readonly byte[] FREEZE_BYTES = new byte[] { 0x90, 0x90, 0x90 }; // NOP instructions
        private static readonly byte[] ORIGINAL_BYTES = new byte[] { 0x89, 0x5E, 0x58 }; // mov [rsi+58],ebx

        // Multiply configuration for tickets (flips sub→add so spending gains tickets)
        private const long TICKET_MULTIPLY_ADDRESS = 0x4D83DAB; //6 time up of the freeze bytes
        private static readonly byte[] MULTIPLY_BYTES = new byte[] { 0x41, 0x03, 0xC6 }; // add eax,r14d
        private static readonly byte[] MULTIPLY_ORIGINAL_BYTES = new byte[] { 0x41, 0x2B, 0xC6 }; // sub eax,r14d

        // Multiply x500 configuration - code cave in INT3 padding at 4D83DD1
        // Patches 5 bytes at 4D83DAB (sub+cmp) with JMP to cave, cave computes eax += r14d*500
        private const long TICKET_MULTIPLY500_PATCH_ADDRESS = 0x4D83DAB;
        private static readonly byte[] MULTIPLY500_PATCH_BYTES = new byte[] { 0xE9, 0x21, 0x00, 0x00, 0x00 }; // JMP to cave at 4D83DD1
        private static readonly byte[] MULTIPLY500_PATCH_ORIGINAL = new byte[] { 0x41, 0x2B, 0xC6, 0x3B, 0xC7 }; // sub eax,r14d; cmp eax,edi
        private const long TICKET_MULTIPLY500_CAVE_ADDRESS = 0x4D83DD1; // 6 lines below the freeze bytes
        private static readonly byte[] MULTIPLY500_CAVE_BYTES = new byte[]
        {
            0x45, 0x69, 0xF6, 0xF4, 0x01, 0x00, 0x00,       // imul r14d,r14d,500
            0x41, 0x03, 0xC6,                               // add eax,r14d
            0x3B, 0xC7,                                     // cmp eax,edi  (restored)
            0x90,                                           // nop
            0xEB, 0xD0                                      // jmp short -0x30 → back to 4D83DB0
        };
        private static readonly byte[] MULTIPLY500_CAVE_ORIGINAL = new byte[]
        {
            0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
            0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
            0xCC, 0xCC, 0xCC, 0xCC, 0xCC
        };

        public MemoryEditorViewModel()
        {
            _memoryService = new MemoryEditorService();

            AttachToProcessCommand = new RelayCommand(AttachToProcess, CanAttachToProcess);
            DetachFromProcessCommand = new RelayCommand(DetachFromProcess, CanDetachFromProcess);
            RefreshValuesCommand = new RelayCommand(RefreshValues, CanRefreshValues);
            ApplyValueCommand = new RelayCommand(ApplyValue, CanApplyValue);
            SelectTicketEditorCommand = new RelayCommand(() => SelectedTool = "tickets");
            BackToMenuCommand = new RelayCommand(() => SelectedTool = "menu");
            ToggleTicketsFreezeCommand = new RelayCommand(ToggleTicketsFreeze, CanToggleTicketsFreeze);
            ToggleTicketsMultiplyCommand = new RelayCommand(ToggleTicketsMultiply, CanToggleTicketsMultiply);
            ToggleTicketsMultiply500Command = new RelayCommand(ToggleTicketsMultiply500, CanToggleTicketsMultiply500);

            MemoryValues = new ObservableCollection<MemoryValue>
            {
                new MemoryValue
                {
                    Name = "Tickets",
                    Description = "In-game currency for purchases",
                    BaseAddress = 0x151058DC,
                    Offsets = new int[] { 0x60, 0x10, 0x1D8, 0x108, 0x2D8, 0xD8, 0x58 },
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

        public bool IsTicketsFrozen
        {
            get => _isTicketsFrozen;
            set
            {
                _isTicketsFrozen = value;
                OnPropertyChanged();
            }
        }

        public bool IsTicketsMultiplied
        {
            get => _isTicketsMultiplied;
            set
            {
                _isTicketsMultiplied = value;
                OnPropertyChanged();
            }
        }

        public bool IsTicketsMultiply500
        {
            get => _isTicketsMultiply500;
            set
            {
                _isTicketsMultiply500 = value;
                OnPropertyChanged();
            }
        }

        public ICommand AttachToProcessCommand { get; }
        public ICommand DetachFromProcessCommand { get; }
        public ICommand RefreshValuesCommand { get; }
        public ICommand ApplyValueCommand { get; }
        public ICommand SelectTicketEditorCommand { get; }
        public ICommand BackToMenuCommand { get; }
        public ICommand ToggleTicketsFreezeCommand { get; }
        public ICommand ToggleTicketsMultiplyCommand { get; }
        public ICommand ToggleTicketsMultiply500Command { get; }

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

                // Restore original bytes if frozen
                if (IsTicketsFrozen)
                {
                    _memoryService.WriteBytes(TICKET_FREEZE_ADDRESS, ORIGINAL_BYTES);
                }

                if (IsTicketsMultiplied)
                {
                    _memoryService.WriteBytes(TICKET_MULTIPLY_ADDRESS, MULTIPLY_ORIGINAL_BYTES);
                }

                if (IsTicketsMultiply500)
                {
                    _memoryService.WriteBytes(TICKET_MULTIPLY500_PATCH_ADDRESS, MULTIPLY500_PATCH_ORIGINAL);
                    _memoryService.WriteBytes(TICKET_MULTIPLY500_CAVE_ADDRESS, MULTIPLY500_CAVE_ORIGINAL);
                }

                IsTicketsFrozen = false;
                IsTicketsMultiplied = false;
                IsTicketsMultiply500 = false;
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
                    IsTicketsFrozen = false;
                    IsTicketsMultiplied = false;
                    IsTicketsMultiply500 = false;
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

        private bool CanToggleTicketsFreeze(object? parameter)
        {
            return IsAttached;
        }

        private void ToggleTicketsFreeze(object? parameter)
        {
            if (!IsAttached)
                return;

            try
            {
                bool success;
                if (!IsTicketsFrozen)
                {
                    // Write NOP bytes to freeze
                    success = _memoryService.WriteBytes(TICKET_FREEZE_ADDRESS, FREEZE_BYTES);
                    if (success)
                    {
                        IsTicketsFrozen = true;
                        StatusMessage = "Tickets frozen - unlimited tickets enabled!";
                    }
                    else
                    {
                        StatusMessage = "Failed to freeze tickets";
                        MessageBox.Show(
                            "Failed to freeze tickets. Make sure the game is running and you are attached to the process.",
                            "Freeze Failed",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }
                }
                else
                {
                    // Restore original bytes to unfreeze
                    success = _memoryService.WriteBytes(TICKET_FREEZE_ADDRESS, ORIGINAL_BYTES);
                    if (success)
                    {
                        IsTicketsFrozen = false;
                        StatusMessage = "Tickets unfrozen";
                    }
                    else
                    {
                        StatusMessage = "Failed to unfreeze tickets";
                        MessageBox.Show(
                            "Failed to unfreeze tickets.",
                            "Unfreeze Failed",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error toggling freeze: {ex.Message}";
                MessageBox.Show(
                    $"Error occurred while toggling freeze:\n\n{ex.Message}",
                    "Error",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
            }

            ((RelayCommand)ToggleTicketsFreezeCommand).RaiseCanExecuteChanged();
        }

        private bool CanToggleTicketsMultiply(object? parameter)
        {
            return IsAttached;
        }

        private void ToggleTicketsMultiply(object? parameter)
        {
            if (!IsAttached)
                return;

            try
            {
                bool success;
                if (!IsTicketsMultiplied)
                {
                    success = _memoryService.WriteBytes(TICKET_MULTIPLY_ADDRESS, MULTIPLY_BYTES);
                    if (success)
                    {
                        IsTicketsMultiplied = true;
                        StatusMessage = "Ticket multiply enabled - spending now gains tickets!";
                    }
                    else
                    {
                        StatusMessage = "Failed to enable ticket multiply";
                        MessageBox.Show(
                            "Failed to enable ticket multiply. Make sure the game is running and you are attached to the process.",
                            "Multiply Failed",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }
                }
                else
                {
                    success = _memoryService.WriteBytes(TICKET_MULTIPLY_ADDRESS, MULTIPLY_ORIGINAL_BYTES);
                    if (success)
                    {
                        IsTicketsMultiplied = false;
                        StatusMessage = "Ticket multiply disabled";
                    }
                    else
                    {
                        StatusMessage = "Failed to disable ticket multiply";
                        MessageBox.Show(
                            "Failed to disable ticket multiply.",
                            "Disable Failed",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error toggling multiply: {ex.Message}";
                MessageBox.Show(
                    $"Error occurred while toggling multiply:\n\n{ex.Message}",
                    "Error",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
            }

            ((RelayCommand)ToggleTicketsMultiplyCommand).RaiseCanExecuteChanged();
        }

        private bool CanToggleTicketsMultiply500(object? parameter)
        {
            return IsAttached;
        }

        private void ToggleTicketsMultiply500(object? parameter)
        {
            if (!IsAttached)
                return;

            try
            {
                bool success;
                if (!IsTicketsMultiply500)
                {
                    // Write cave first, then JMP patch (safest order)
                    success = _memoryService.WriteBytes(TICKET_MULTIPLY500_CAVE_ADDRESS, MULTIPLY500_CAVE_BYTES);
                    if (success)
                        success = _memoryService.WriteBytes(TICKET_MULTIPLY500_PATCH_ADDRESS, MULTIPLY500_PATCH_BYTES);

                    if (success)
                    {
                        IsTicketsMultiply500 = true;
                        StatusMessage = "Tickets x500 enabled - each purchase gains 500× the ticket cost!";
                    }
                    else
                    {
                        _memoryService.WriteBytes(TICKET_MULTIPLY500_CAVE_ADDRESS, MULTIPLY500_CAVE_ORIGINAL);
                        StatusMessage = "Failed to enable tickets x500";
                        MessageBox.Show(
                            "Failed to enable tickets x500. Make sure the game is running and you are attached to the process.",
                            "x500 Failed",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }
                }
                else
                {
                    // Restore patch first so game stops jumping to cave, then wipe cave
                    success = _memoryService.WriteBytes(TICKET_MULTIPLY500_PATCH_ADDRESS, MULTIPLY500_PATCH_ORIGINAL);
                    _memoryService.WriteBytes(TICKET_MULTIPLY500_CAVE_ADDRESS, MULTIPLY500_CAVE_ORIGINAL);

                    if (success)
                    {
                        IsTicketsMultiply500 = false;
                        StatusMessage = "Tickets x500 disabled";
                    }
                    else
                    {
                        StatusMessage = "Failed to disable tickets x500";
                        MessageBox.Show(
                            "Failed to disable tickets x500.",
                            "Disable Failed",
                            MessageBoxButton.OK,
                            MessageBoxImage.Error);
                    }
                }
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error toggling x500: {ex.Message}";
                MessageBox.Show(
                    $"Error occurred while toggling x500:\n\n{ex.Message}",
                    "Error",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
            }

            ((RelayCommand)ToggleTicketsMultiply500Command).RaiseCanExecuteChanged();
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
