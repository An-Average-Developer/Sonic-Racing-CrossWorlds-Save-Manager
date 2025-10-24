using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using Microsoft.Win32;
using SonicRacingSaveManager.Models;
using SonicRacingSaveManager.Services;

namespace SonicRacingSaveManager.ViewModels
{
    public class MainViewModel : ViewModelBase
    {
        private readonly SaveManagerService _saveManager;

        private ObservableCollection<SaveAccount> _accounts = new();
        private ObservableCollection<BackupInfo> _backups = new();
        private SaveAccount? _selectedAccount;
        private SaveAccount? _selectedRestoreAccount;
        private BackupInfo? _selectedBackup;
        private BackupInfo? _selectedManageBackup;
        private string _backupName = string.Empty;
        private string _statusMessage = "Ready";
        private bool _isLoading;

        public MainViewModel()
        {
            _saveManager = new SaveManagerService();

            // Commands
            RefreshAccountsCommand = new RelayCommand(async () => await RefreshAccountsAsync());
            RefreshBackupsCommand = new RelayCommand(async () => await RefreshBackupsAsync());
            CreateBackupCommand = new RelayCommand(async () => await CreateBackupAsync(), () => SelectedAccount != null);
            RestoreBackupCommand = new RelayCommand(async () => await RestoreBackupAsync(), () => SelectedBackup != null);
            RestoreBackupToCustomCommand = new RelayCommand(async () => await RestoreBackupToCustomAsync(), () => SelectedBackup != null);
            DeleteBackupCommand = new RelayCommand(async () => await DeleteBackupAsync(), () => SelectedManageBackup != null);
            ExportBackupCommand = new RelayCommand(async () => await ExportBackupAsync(), () => SelectedManageBackup != null);
            ImportBackupCommand = new RelayCommand(async () => await ImportBackupAsync());
            ImportFilesDirectlyCommand = new RelayCommand(async () => await ImportFilesDirectlyAsync());
            OpenBackupFolderCommand = new RelayCommand(() => _saveManager.OpenBackupFolder());
            OpenSaveFolderCommand = new RelayCommand(() => _saveManager.OpenSaveFolder());

            // Initial load
            _ = InitializeAsync();
        }

        public ObservableCollection<SaveAccount> Accounts
        {
            get => _accounts;
            set => SetProperty(ref _accounts, value);
        }

        public ObservableCollection<BackupInfo> Backups
        {
            get => _backups;
            set => SetProperty(ref _backups, value);
        }

        public SaveAccount? SelectedAccount
        {
            get => _selectedAccount;
            set
            {
                SetProperty(ref _selectedAccount, value);
                ((RelayCommand)CreateBackupCommand).RaiseCanExecuteChanged();
            }
        }

        public SaveAccount? SelectedRestoreAccount
        {
            get => _selectedRestoreAccount;
            set
            {
                SetProperty(ref _selectedRestoreAccount, value);
                ((RelayCommand)RestoreBackupToCustomCommand).RaiseCanExecuteChanged();
            }
        }

        public BackupInfo? SelectedBackup
        {
            get => _selectedBackup;
            set
            {
                SetProperty(ref _selectedBackup, value);
                ((RelayCommand)RestoreBackupCommand).RaiseCanExecuteChanged();
                ((RelayCommand)RestoreBackupToCustomCommand).RaiseCanExecuteChanged();
            }
        }

        public BackupInfo? SelectedManageBackup
        {
            get => _selectedManageBackup;
            set
            {
                SetProperty(ref _selectedManageBackup, value);
                ((RelayCommand)DeleteBackupCommand).RaiseCanExecuteChanged();
                ((RelayCommand)ExportBackupCommand).RaiseCanExecuteChanged();
            }
        }

        public string BackupName
        {
            get => _backupName;
            set => SetProperty(ref _backupName, value);
        }

        public string StatusMessage
        {
            get => _statusMessage;
            set => SetProperty(ref _statusMessage, value);
        }

        public bool IsLoading
        {
            get => _isLoading;
            set => SetProperty(ref _isLoading, value);
        }

        public string SaveDirectory => _saveManager.BaseSaveDirectory;
        public string BackupDirectory => _saveManager.BackupDirectory;

        public ICommand RefreshAccountsCommand { get; }
        public ICommand RefreshBackupsCommand { get; }
        public ICommand CreateBackupCommand { get; }
        public ICommand RestoreBackupCommand { get; }
        public ICommand RestoreBackupToCustomCommand { get; }
        public ICommand DeleteBackupCommand { get; }
        public ICommand ExportBackupCommand { get; }
        public ICommand ImportBackupCommand { get; }
        public ICommand ImportFilesDirectlyCommand { get; }
        public ICommand OpenBackupFolderCommand { get; }
        public ICommand OpenSaveFolderCommand { get; }

        private async Task InitializeAsync()
        {
            await RefreshAccountsAsync();
            await RefreshBackupsAsync();

            // Auto-select first account if available
            if (Accounts.Any())
            {
                SelectedAccount = Accounts.First();
                SelectedRestoreAccount = Accounts.First();
            }
        }

        private async Task RefreshAccountsAsync()
        {
            IsLoading = true;
            StatusMessage = "Loading accounts...";

            try
            {
                await Task.Run(() =>
                {
                    var accounts = _saveManager.GetSaveAccounts();
                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        Accounts.Clear();
                        foreach (var account in accounts)
                        {
                            Accounts.Add(account);
                        }
                    });
                });

                StatusMessage = Accounts.Any()
                    ? $"Ready - {Accounts.Count} account(s) found"
                    : "No save files found";
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to load accounts:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }

        private async Task RefreshBackupsAsync()
        {
            IsLoading = true;
            StatusMessage = "Loading backups...";

            try
            {
                await Task.Run(() =>
                {
                    var backups = _saveManager.ListBackups();
                    Application.Current.Dispatcher.Invoke(() =>
                    {
                        Backups.Clear();
                        foreach (var backup in backups)
                        {
                            Backups.Add(backup);
                        }
                    });
                });

                StatusMessage = Backups.Any()
                    ? $"Found {Backups.Count} backup(s)"
                    : "No backups found";
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to load backups:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }

        private async Task CreateBackupAsync()
        {
            if (SelectedAccount == null) return;

            IsLoading = true;
            StatusMessage = "Creating backup...";

            try
            {
                var customName = string.IsNullOrWhiteSpace(BackupName) ? null : BackupName;

                var (backupPath, fileCount) = await Task.Run(() =>
                    _saveManager.CreateBackup(SelectedAccount.AccountId, customName));

                BackupName = string.Empty;
                await RefreshBackupsAsync();

                StatusMessage = "Backup created successfully!";
                MessageBox.Show($"Backup created successfully!\n\nLocation: {backupPath}\nFiles backed up: {fileCount}",
                    "Success", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to create backup:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }

        private async Task RestoreBackupAsync()
        {
            if (SelectedBackup == null) return;

            var result = MessageBox.Show(
                $"Are you sure you want to restore backup '{SelectedBackup.Name}'?\n\n" +
                "This will overwrite existing save files!",
                "Confirm Restore",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);

            if (result != MessageBoxResult.Yes) return;

            IsLoading = true;
            StatusMessage = "Restoring backup...";

            try
            {
                var fileCount = await Task.Run(() =>
                    _saveManager.RestoreBackup(SelectedBackup.Name));

                StatusMessage = "Backup restored successfully!";
                MessageBox.Show($"Backup restored successfully!\n\n{fileCount} files restored.",
                    "Success", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to restore backup:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }

        private async Task RestoreBackupToCustomAsync()
        {
            if (SelectedBackup == null || SelectedRestoreAccount == null) return;

            var result = MessageBox.Show(
                $"Are you sure you want to restore backup '{SelectedBackup.Name}' to account:\n{SelectedRestoreAccount.ShortDisplay}\n\n" +
                "This will overwrite existing save files!",
                "Confirm Restore",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);

            if (result != MessageBoxResult.Yes) return;

            IsLoading = true;
            StatusMessage = "Restoring backup...";

            try
            {
                var fileCount = await Task.Run(() =>
                    _saveManager.RestoreBackup(SelectedBackup.Name, SelectedRestoreAccount.AccountId));

                StatusMessage = "Backup restored successfully!";
                MessageBox.Show($"Backup restored successfully!\n\n{fileCount} files restored to {SelectedRestoreAccount.ShortDisplay}.",
                    "Success", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to restore backup:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }

        private async Task DeleteBackupAsync()
        {
            if (SelectedManageBackup == null) return;

            var result = MessageBox.Show(
                $"Are you sure you want to delete backup '{SelectedManageBackup.Name}'?\n\n" +
                "This action cannot be undone!",
                "Confirm Delete",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);

            if (result != MessageBoxResult.Yes) return;

            IsLoading = true;
            StatusMessage = "Deleting backup...";

            try
            {
                await Task.Run(() => _saveManager.DeleteBackup(SelectedManageBackup.Name));

                await RefreshBackupsAsync();
                StatusMessage = "Backup deleted successfully!";
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to delete backup:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }

        private async Task ExportBackupAsync()
        {
            if (SelectedManageBackup == null) return;

            var dialog = new SaveFileDialog
            {
                Title = "Export Backup",
                Filter = "Zip files (*.zip)|*.zip|All files (*.*)|*.*",
                FileName = $"{SelectedManageBackup.Name}.zip",
                DefaultExt = ".zip"
            };

            if (dialog.ShowDialog() != true) return;

            IsLoading = true;
            StatusMessage = "Exporting backup...";

            try
            {
                var exportPath = await Task.Run(() =>
                    _saveManager.ExportBackup(SelectedManageBackup.Name, dialog.FileName));

                StatusMessage = "Backup exported successfully!";
                MessageBox.Show($"Backup exported successfully!\n\nLocation: {exportPath}",
                    "Success", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to export backup:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }

        private async Task ImportBackupAsync()
        {
            var dialog = new OpenFileDialog
            {
                Title = "Import Backup",
                Filter = "Zip files (*.zip)|*.zip|All files (*.*)|*.*",
                DefaultExt = ".zip"
            };

            if (dialog.ShowDialog() != true) return;

            IsLoading = true;
            StatusMessage = "Importing backup...";

            try
            {
                var backupName = await Task.Run(() =>
                    _saveManager.ImportBackup(dialog.FileName));

                await RefreshBackupsAsync();
                StatusMessage = "Backup imported successfully!";
                MessageBox.Show($"Backup imported successfully!\n\nBackup name: {backupName}",
                    "Success", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to import backup:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }

        private async Task ImportFilesDirectlyAsync()
        {
            if (SelectedAccount == null)
            {
                MessageBox.Show("No account selected. Cannot import files.", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }

            var dialog = new OpenFileDialog
            {
                Title = "Import Save Files",
                Filter = "Save Files (*.sav;*.zip)|*.sav;*.zip|Zip files (*.zip)|*.zip|Save files (*.sav)|*.sav|All files (*.*)|*.*",
                DefaultExt = ".sav"
            };

            if (dialog.ShowDialog() != true) return;

            var result = MessageBox.Show(
                $"This will replace the current save files for account:\n{SelectedAccount.ShortDisplay}\n\n" +
                "Are you sure you want to continue?",
                "Confirm Import",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);

            if (result != MessageBoxResult.Yes) return;

            IsLoading = true;
            StatusMessage = "Importing files...";

            try
            {
                var fileCount = await Task.Run(() =>
                    _saveManager.ImportFilesDirectly(dialog.FileName, SelectedAccount.AccountId));

                await RefreshAccountsAsync();
                StatusMessage = "Files imported successfully!";
                MessageBox.Show($"Successfully imported {fileCount} save file(s)!\n\n" +
                    "Your save files have been replaced.",
                    "Import Complete", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                StatusMessage = $"Error: {ex.Message}";
                MessageBox.Show($"Failed to import files:\n{ex.Message}", "Error",
                    MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsLoading = false;
            }
        }
    }
}
