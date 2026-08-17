using System;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Windows.Input;
using SonicRacingSaveManager.Common.Infrastructure;
using SonicRacingSaveManager.Configuration;
using SonicRacingSaveManager.Features.Updates.Services;

namespace SonicRacingSaveManager.Features.Updates.ViewModels
{
    // Fallback for when the automatic updater doesn't detect or install a new version.
    // Always fetches the latest published release — regardless of what the local version
    // comparison thinks — so the user can force a reinstall, or fall back to downloading
    // it manually in a browser.
    public class ManualUpdateDialogViewModel : ViewModelBase
    {
        private readonly UpdateService _updateService;
        private UpdateInfo? _releaseInfo;

        private string _statusTitle = "Checking for the latest release...";
        private string _statusMessage = string.Empty;
        private bool _canDownload;
        private bool _isDownloading;
        private bool _isCheckFailed;
        private int _downloadProgress;
        private string _downloadProgressText = string.Empty;

        public ManualUpdateDialogViewModel(UpdateService updateService)
        {
            _updateService = updateService;

            DownloadCommand = new RelayCommand(async () => await DownloadAsync(), () => CanDownload && !IsDownloading);
            OpenBrowserCommand = new RelayCommand(OpenBrowser);

            _ = CheckAsync();
        }

        public string StatusTitle
        {
            get => _statusTitle;
            set => SetProperty(ref _statusTitle, value);
        }

        public string StatusMessage
        {
            get => _statusMessage;
            set => SetProperty(ref _statusMessage, value);
        }

        public bool CanDownload
        {
            get => _canDownload;
            set
            {
                SetProperty(ref _canDownload, value);
                ((RelayCommand)DownloadCommand).RaiseCanExecuteChanged();
            }
        }

        public bool IsDownloading
        {
            get => _isDownloading;
            set
            {
                SetProperty(ref _isDownloading, value);
                ((RelayCommand)DownloadCommand).RaiseCanExecuteChanged();
            }
        }

        public bool IsCheckFailed
        {
            get => _isCheckFailed;
            set => SetProperty(ref _isCheckFailed, value);
        }

        public int DownloadProgress
        {
            get => _downloadProgress;
            set => SetProperty(ref _downloadProgress, value);
        }

        public string DownloadProgressText
        {
            get => _downloadProgressText;
            set => SetProperty(ref _downloadProgressText, value);
        }

        public ICommand DownloadCommand { get; }
        public ICommand OpenBrowserCommand { get; }

        private async Task CheckAsync()
        {
            _releaseInfo = await _updateService.GetLatestReleaseInfoAsync();

            if (!string.IsNullOrWhiteSpace(_releaseInfo.DownloadUrl))
            {
                StatusTitle = $"v{_releaseInfo.LatestVersion} is ready to download";
                StatusMessage = $"{_releaseInfo.FileName}  |  {UpdateService.FormatFileSize(_releaseInfo.FileSize)}";
                CanDownload = true;
            }
            else
            {
                IsCheckFailed = true;
                StatusTitle = "Couldn't reach GitHub";
                StatusMessage = "Try again later, or open the releases page in your browser.";
            }
        }

        private async Task DownloadAsync()
        {
            if (IsDownloading || !CanDownload) return;

            IsDownloading = true;
            IsCheckFailed = false;
            DownloadProgress = 0;
            DownloadProgressText = "Starting download...";

            var progress = new Progress<int>(pct =>
            {
                DownloadProgress = pct;
                DownloadProgressText = pct < 100 ? $"Downloading... {pct}%" : "Installing...";
            });

            var success = await _updateService.DownloadAndInstallExeAsync(progress);

            if (!success)
            {
                IsDownloading = false;
                IsCheckFailed = true;
                StatusTitle = "Download failed";
                StatusMessage = "Please try again, or download manually from the browser.";
            }
        }

        private void OpenBrowser()
        {
            var url = string.IsNullOrWhiteSpace(_releaseInfo?.ReleaseUrl)
                ? $"{AppVersion.GetGitHubRepoUrl()}/releases/latest"
                : _releaseInfo.ReleaseUrl;

            try
            {
                Process.Start(new ProcessStartInfo { FileName = url, UseShellExecute = true });
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"[ManualUpdateDialogViewModel] Failed to open browser: {ex.Message}");
            }
        }
    }
}
