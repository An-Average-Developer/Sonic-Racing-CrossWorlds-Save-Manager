using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Input;
using SonicRacingSaveManager.Models;
using SonicRacingSaveManager.Services;

namespace SonicRacingSaveManager.ViewModels
{
    public class AccountSelectionViewModel : ViewModelBase
    {
        private readonly SaveManagerService _saveManager;
        private ObservableCollection<SaveAccount> _accounts = new();
        private SaveAccount? _selectedAccount;

        public event EventHandler? AccountSelected;

        public AccountSelectionViewModel()
        {
            _saveManager = new SaveManagerService();

            RefreshCommand = new RelayCommand(async () => await RefreshAccountsAsync());
            SelectAccountCommand = new RelayCommand(OnAccountSelected, () => SelectedAccount != null);

            _ = RefreshAccountsAsync();
        }

        public ObservableCollection<SaveAccount> Accounts
        {
            get => _accounts;
            set => SetProperty(ref _accounts, value);
        }

        public SaveAccount? SelectedAccount
        {
            get => _selectedAccount;
            set
            {
                SetProperty(ref _selectedAccount, value);
                OnPropertyChanged(nameof(HasSelectedAccount));
                ((RelayCommand)SelectAccountCommand).RaiseCanExecuteChanged();
            }
        }

        public bool HasSelectedAccount => SelectedAccount != null;
        public bool HasNoAccounts => !Accounts.Any();

        public ICommand RefreshCommand { get; }
        public ICommand SelectAccountCommand { get; }

        public SaveAccount? GetSelectedAccount() => SelectedAccount;

        private async Task RefreshAccountsAsync()
        {
            await Task.Run(() =>
            {
                var accounts = _saveManager.GetSaveAccounts();
                System.Windows.Application.Current.Dispatcher.Invoke(() =>
                {
                    Accounts.Clear();
                    foreach (var account in accounts)
                    {
                        Accounts.Add(account);
                    }
                    OnPropertyChanged(nameof(HasNoAccounts));

                    // Auto-select first account if only one
                    if (Accounts.Count == 1)
                    {
                        SelectedAccount = Accounts[0];
                    }
                });
            });
        }

        private void OnAccountSelected()
        {
            if (SelectedAccount != null)
            {
                AccountSelected?.Invoke(this, EventArgs.Empty);
            }
        }
    }
}
