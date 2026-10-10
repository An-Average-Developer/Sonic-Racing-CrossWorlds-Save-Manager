using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
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
        private List<long>? _ticketCandidates;
        private int _ticketSearchValue;
        private bool _isTicketSearching;
        private int _ticketSearchStep = 1;
        private string _ticketSearchHint = "";
        private string _selectedTool = "menu";
        private bool _isTicketsFrozen = false;
        private bool _isTicketsMultiplied = false;
        private bool _isTicketsMultiply500 = false;
        private bool _isAutoAttachEnabled = true;
        private GameProcessInfo? _selectedGameProcess;

        // Freeze configuration for tickets
        private const long TICKET_FREEZE_ADDRESS = 0x4D830ED;
        private static readonly byte[] FREEZE_BYTES = new byte[] { 0x90, 0x90, 0x90 }; // NOP instructions
        private static readonly byte[] ORIGINAL_BYTES = new byte[] { 0x89, 0x5E, 0x58 }; // mov [rsi+58],ebx

        // Multiply configuration for tickets (flips sub→add so spending gains tickets)
        private const long TICKET_MULTIPLY_ADDRESS = 0x4D830DB; //6 time up of the freeze bytes
        private static readonly byte[] MULTIPLY_BYTES = new byte[] { 0x41, 0x03, 0xC6 }; // add eax,r14d
        private static readonly byte[] MULTIPLY_ORIGINAL_BYTES = new byte[] { 0x41, 0x2B, 0xC6 }; // sub eax,r14d

        // Multiply x500 configuration - code cave in INT3 padding at 4D83101
        // Patches 5 bytes at 4D830DB (sub+cmp) with JMP to cave, cave computes eax += r14d*500
        private const long TICKET_MULTIPLY500_PATCH_ADDRESS = 0x4D830DB;
        private static readonly byte[] MULTIPLY500_PATCH_BYTES = new byte[] { 0xE9, 0x21, 0x00, 0x00, 0x00 }; // JMP to cave at 4D83101
        private static readonly byte[] MULTIPLY500_PATCH_ORIGINAL = new byte[] { 0x41, 0x2B, 0xC6, 0x3B, 0xC7 }; // sub eax,r14d; cmp eax,edi
        private const long TICKET_MULTIPLY500_CAVE_ADDRESS = 0x4D83101; // 6 lines below the freeze bytes
        private static readonly byte[] MULTIPLY500_CAVE_BYTES = new byte[]
        {
            0x45, 0x69, 0xF6, 0xF4, 0x01, 0x00, 0x00,       // imul r14d,r14d,500
            0x41, 0x03, 0xC6,                               // add eax,r14d
            0x3B, 0xC7,                                     // cmp eax,edi  (restored)
            0x90,                                           // nop
            0xEB, 0xD0                                      // jmp short -0x30 → back to 4D830E0
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

            DetectedProcesses = new ObservableCollection<GameProcessInfo>();

            AttachToProcessCommand = new RelayCommand(AttachToProcess, CanAttachToProcess);
            DetachFromProcessCommand = new RelayCommand(DetachFromProcess, CanDetachFromProcess);
            ToggleAutoAttachCommand = new RelayCommand(ToggleAutoAttach);
            RefreshProcessListCommand = new RelayCommand(RefreshDetectedProcesses);
            AttachToSelectedProcessCommand = new RelayCommand(AttachToSelectedProcess, CanAttachToSelectedProcess);
            RefreshValuesCommand = new RelayCommand(RefreshValues, CanRefreshValues);
            ApplyValueCommand = new RelayCommand(ApplyValue, CanApplyValue);
            SelectTicketEditorCommand = new RelayCommand(() => SelectedTool = "tickets");
            BackToMenuCommand = new RelayCommand(() => SelectedTool = "menu");
            ToggleTicketsFreezeCommand = new RelayCommand(ToggleTicketsFreeze, CanToggleTicketsFreeze);
            ToggleTicketsMultiplyCommand = new RelayCommand(ToggleTicketsMultiply, CanToggleTicketsMultiply);
            ToggleTicketsMultiply500Command = new RelayCommand(ToggleTicketsMultiply500, CanToggleTicketsMultiply500);
            SearchTicketsCommand = new RelayCommand(SearchTickets, CanSearchTickets);
            NewTicketSearchCommand = new RelayCommand(_ => ResetTicketSearch(), _ => IsAttached && !IsTicketSearching);
            NextTicketStepCommand = new RelayCommand(NextTicketStep);

            MemoryValues = new ObservableCollection<MemoryValue>
            {
                new MemoryValue
                {
                    Name = "Tickets",
                    Description = "In-game currency for purchases",
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

            if (_isAutoAttachEnabled)
            {
                _autoAttachTimer.Start();
                TryAutoAttach();
            }
        }

        public ObservableCollection<MemoryValue> MemoryValues { get; }

        private MemoryValue Tickets => MemoryValues[0];

        /// <summary>The ticket amount the user typed for the current search step.</summary>
        public int TicketSearchValue
        {
            get => _ticketSearchValue;
            set
            {
                _ticketSearchValue = value;
                OnPropertyChanged();
            }
        }

        public bool IsTicketSearching
        {
            get => _isTicketSearching;
            set
            {
                _isTicketSearching = value;
                OnPropertyChanged();
                RaiseTicketSearchCommands();
            }
        }

        public bool IsTicketLocated => Tickets.Address != 0;

        /// <summary>
        /// Guided search step: 1 = go to Friendship, 2 = type tickets + Search, 3 = go to
        /// Customize Machine, 4 = back to Friendship, 5 = spend 1 ticket + Search, 6 = found.
        /// </summary>
        public int TicketSearchStep
        {
            get => _ticketSearchStep;
            set
            {
                _ticketSearchStep = value;
                OnPropertyChanged();
                OnPropertyChanged(nameof(TicketStepText));
                OnPropertyChanged(nameof(TicketStepTitle));
                OnPropertyChanged(nameof(IsTicketNextStep));
                OnPropertyChanged(nameof(IsTicketSearchInputStep));
            }
        }

        public string TicketStepTitle => TicketSearchStep == 6 ? "Done" : $"Step {TicketSearchStep} of 5";

        public string TicketStepText => TicketSearchStep switch
        {
            1 => "First, go to the Friendship menu in-game.",
            2 => "Type the amount of tickets you have right now and press Search.",
            3 => "Go back to the Customize Machine menu.",
            4 => "Go back to the Friendship menu.",
            5 => "Spend 1 ticket, then type your new amount of tickets and press Search.",
            _ => "Tickets found! You can now edit them.",
        };

        public bool IsTicketNextStep => TicketSearchStep is 1 or 3 or 4;

        public bool IsTicketSearchInputStep => TicketSearchStep is 2 or 5;

        /// <summary>Extra result info under the current step (match count, errors). Empty when there's nothing to add.</summary>
        public string TicketSearchHint
        {
            get => _ticketSearchHint;
            set
            {
                _ticketSearchHint = value;
                OnPropertyChanged();
            }
        }

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
                RaiseTicketSearchCommands();
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

        public bool IsAutoAttachEnabled
        {
            get => _isAutoAttachEnabled;
            set
            {
                _isAutoAttachEnabled = value;
                OnPropertyChanged();
            }
        }

        public ObservableCollection<GameProcessInfo> DetectedProcesses { get; }

        public GameProcessInfo? SelectedGameProcess
        {
            get => _selectedGameProcess;
            set
            {
                _selectedGameProcess = value;
                OnPropertyChanged();
                ((RelayCommand)AttachToSelectedProcessCommand).RaiseCanExecuteChanged();
            }
        }

        public bool HasDetectedProcesses => DetectedProcesses.Count > 0;

        public ICommand AttachToProcessCommand { get; }
        public ICommand DetachFromProcessCommand { get; }
        public ICommand ToggleAutoAttachCommand { get; }
        public ICommand RefreshProcessListCommand { get; }
        public ICommand AttachToSelectedProcessCommand { get; }
        public ICommand RefreshValuesCommand { get; }
        public ICommand ApplyValueCommand { get; }
        public ICommand SelectTicketEditorCommand { get; }
        public ICommand BackToMenuCommand { get; }
        public ICommand ToggleTicketsFreezeCommand { get; }
        public ICommand ToggleTicketsMultiplyCommand { get; }
        public ICommand ToggleTicketsMultiply500Command { get; }
        public ICommand SearchTicketsCommand { get; }
        public ICommand NewTicketSearchCommand { get; }
        public ICommand NextTicketStepCommand { get; }

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

        private void ToggleAutoAttach(object? parameter)
        {
            IsAutoAttachEnabled = !IsAutoAttachEnabled;

            if (IsAttached)
                return;

            if (IsAutoAttachEnabled)
            {
                _autoAttachTimer.Start();
                TryAutoAttach();
            }
            else
            {
                _autoAttachTimer.Stop();
                RefreshDetectedProcesses(null);
                StatusMessage = "Auto-attach disabled. Pick a process below to attach manually.";
            }
        }

        private void RefreshDetectedProcesses(object? parameter)
        {
            DetectedProcesses.Clear();
            foreach (var p in MemoryEditorService.GetCandidateProcesses())
                DetectedProcesses.Add(new GameProcessInfo { Id = p.Id, DisplayName = $"{p.ProcessName}.exe  (PID: {p.Id})" });

            OnPropertyChanged(nameof(HasDetectedProcesses));
            ((RelayCommand)AttachToSelectedProcessCommand).RaiseCanExecuteChanged();
        }

        private bool CanAttachToSelectedProcess(object? parameter)
        {
            return !IsAttached && SelectedGameProcess != null;
        }

        private void AttachToSelectedProcess(object? parameter)
        {
            if (SelectedGameProcess == null)
                return;

            try
            {
                bool success = _memoryService.AttachToProcess(SelectedGameProcess.Id);

                if (success)
                {
                    IsAttached = true;
                    StatusMessage = $"Successfully attached to \"{_memoryService.ProcessName}\"";
                    _autoAttachTimer.Stop();

                    RefreshValues(null);

                    if (AutoRefresh)
                    {
                        _updateTimer.Start();
                    }
                }
                else
                {
                    StatusMessage = "Failed to attach to the selected process.";
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
                ResetTicketSearch();

                if (IsAutoAttachEnabled)
                {
                    StatusMessage = "Detached from game process. Searching for game...";
                    _autoAttachTimer.Start();
                }
                else
                {
                    StatusMessage = "Detached from game process.";
                }
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
                    if (memValue.Address == 0)
                        continue;

                    int value = _memoryService.ReadInt32(memValue.Address);
                    memValue.CurrentValue = value;

                    if (memValue.NewValue == 0 || memValue.NewValue == memValue.CurrentValue)
                    {
                        memValue.NewValue = value;
                    }
                }

                if (IsTicketLocated)
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
            return IsAttached && SelectedValue != null && SelectedValue.Address != 0;
        }

        private void ApplyValue(object? parameter)
        {
            if (SelectedValue == null || !IsAttached || SelectedValue.Address == 0)
                return;

            try
            {
                bool success = _memoryService.WriteInt32(SelectedValue.Address, SelectedValue.NewValue);

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
                            "Make sure the game is running and you are attached to the process. " +
                            "If you restarted the game, press New Search and find your tickets again.",
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
                    ResetTicketSearch();

                    if (IsAutoAttachEnabled)
                    {
                        StatusMessage = "Game closed. Waiting for game to start...";
                        _autoAttachTimer.Start();
                    }
                    else
                    {
                        StatusMessage = "Game closed.";
                    }
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
            if (!IsAutoAttachEnabled || IsAttached)
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

        // ── Ticket search (Cheat Engine style: search, change the value in-game, search again) ──

        private bool CanSearchTickets(object? parameter)
        {
            return IsAttached && !IsTicketSearching;
        }

        private async void SearchTickets(object? parameter)
        {
            if (!IsAttached || IsTicketSearching)
                return;

            int value = TicketSearchValue;
            var previous = TicketSearchStep == 5 ? _ticketCandidates : null;
            IsTicketSearching = true;
            TicketSearchHint = "Searching...";

            try
            {
                var matches = await Task.Run(() => previous == null
                    ? _memoryService.ScanForInt32(value)
                    : _memoryService.FilterInt32(previous, value));

                if (matches.Count == 1)
                {
                    _ticketCandidates = null;
                    SetTicketAddress(matches[0]);
                    TicketSearchStep = 6;
                    TicketSearchHint = "";
                    StatusMessage = "Tickets located";
                    RefreshValues(null);
                }
                else if (matches.Count == 0)
                {
                    _ticketCandidates = null;
                    TicketSearchStep = previous == null ? 2 : 1;
                    TicketSearchHint = previous == null
                        ? "Nothing found. Check the amount you typed and try again."
                        : "Nothing matched that amount, so the search was restarted.";
                    StatusMessage = "Ticket search found nothing";
                }
                else
                {
                    // Still several candidates: repeat the menu round-trip and spend another ticket.
                    _ticketCandidates = matches;
                    TicketSearchStep = 3;
                    TicketSearchHint = $"{matches.Count:N0} possible addresses left.";
                    StatusMessage = $"Ticket search: {matches.Count:N0} matches";
                }
            }
            catch (Exception ex)
            {
                TicketSearchHint = "Search failed. Try again or press New Search.";
                StatusMessage = $"Ticket search error: {ex.Message}";
            }
            finally
            {
                IsTicketSearching = false;
            }
        }

        private void ResetTicketSearch()
        {
            _ticketCandidates = null;
            SetTicketAddress(0);
            Tickets.CurrentValue = 0;
            Tickets.NewValue = 0;
            TicketSearchStep = 1;
            TicketSearchHint = "";
        }

        private void NextTicketStep(object? parameter)
        {
            if (IsTicketNextStep)
            {
                TicketSearchHint = "";
                TicketSearchStep++;
            }
        }

        private void SetTicketAddress(long address)
        {
            Tickets.Address = address;
            OnPropertyChanged(nameof(IsTicketLocated));
            ((RelayCommand)ApplyValueCommand).RaiseCanExecuteChanged();
        }

        private void RaiseTicketSearchCommands()
        {
            (SearchTicketsCommand as RelayCommand)?.RaiseCanExecuteChanged();
            (NewTicketSearchCommand as RelayCommand)?.RaiseCanExecuteChanged();
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
