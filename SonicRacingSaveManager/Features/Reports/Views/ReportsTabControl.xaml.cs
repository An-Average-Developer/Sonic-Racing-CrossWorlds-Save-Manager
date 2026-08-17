using System;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using Microsoft.Win32;
using SonicRacingSaveManager.Common.Services;
using SonicRacingSaveManager.Configuration;
using IOPath = System.IO.Path;

namespace SonicRacingSaveManager.Features.Reports.Views
{
    public partial class ReportsTabControl : UserControl
    {
        // TODO: replace with the Discord webhook URL for the bug-reports channel.
        private const string WebhookUrl = "WebhookUrl"; //WebhookUrl

        private string? _bugImagePath;
        private (string Id, string Username)? _discordUser;
        private CancellationTokenSource? _discordAuthCts;

        public ReportsTabControl()
        {
            InitializeComponent();
            Loaded += ReportsTabControl_Loaded;
        }

        private void ReportsTabControl_Loaded(object sender, RoutedEventArgs e)
        {
            var cached = DiscordOAuthService.LoadCached();
            if (cached.HasValue)
            {
                _discordUser = cached;
                ShowConnected(cached.Value.Username);
            }
            else
            {
                ShowDisconnected();
            }
        }

        // ── Discord identity ────────────────────────────────────────────────────

        private async void DiscordConnectBtn_Click(object sender, RoutedEventArgs e)
        {
            _discordAuthCts?.Cancel();
            _discordAuthCts = new CancellationTokenSource();

            DiscordConnectRow.Visibility = Visibility.Collapsed;
            DiscordWaitingRow.Visibility = Visibility.Visible;

            var user = await DiscordOAuthService.AuthenticateAsync(_discordAuthCts.Token);

            DiscordWaitingRow.Visibility = Visibility.Collapsed;

            if (user.HasValue)
            {
                _discordUser = user;
                DiscordOAuthService.SaveCached(user.Value.Id, user.Value.Username);
                ShowConnected(user.Value.Username);
            }
            else
            {
                ShowDisconnected();
            }
        }

        private void DiscordCancelAuthBtn_Click(object sender, RoutedEventArgs e)
        {
            _discordAuthCts?.Cancel();
            DiscordWaitingRow.Visibility = Visibility.Collapsed;
            ShowDisconnected();
        }

        private void DiscordDisconnectBtn_Click(object sender, RoutedEventArgs e)
        {
            DiscordOAuthService.ClearCached();
            _discordUser = null;
            ShowDisconnected();
        }

        private void DiscordInfoBtn_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new DiscordInfoDialog
            {
                Owner = Window.GetWindow(this)
            };
            dialog.ShowDialog();
        }

        private void ShowConnected(string username)
        {
            DiscordUsernameText.Text        = username;
            DiscordConnectedPanel.Visibility = Visibility.Visible;
            DiscordConnectRow.Visibility     = Visibility.Collapsed;
            DiscordWaitingRow.Visibility      = Visibility.Collapsed;
        }

        private void ShowDisconnected()
        {
            DiscordConnectedPanel.Visibility = Visibility.Collapsed;
            DiscordConnectRow.Visibility     = Visibility.Visible;
        }

        // ── Screenshot ──────────────────────────────────────────────────────────

        private void BugImageBtn_Click(object sender, RoutedEventArgs e)
        {
            var picker = new OpenFileDialog
            {
                Filter = "Images|*.jpg;*.jpeg;*.png;*.gif;*.webp;*.bmp",
            };
            if (picker.ShowDialog() != true) return;

            _bugImagePath        = picker.FileName;
            BugImageFileName.Text       = IOPath.GetFileName(_bugImagePath);
            BugImageFileName.Foreground = (Brush)FindResource("PrimaryHueMidBrush");
        }

        // ── Send report ─────────────────────────────────────────────────────────

        private async void SendBugReportBtn_Click(object sender, RoutedEventArgs e)
        {
            var title = BugTitleBox.Text.Trim();
            var desc  = BugDescBox.Text.Trim();

            if (string.IsNullOrEmpty(title) || string.IsNullOrEmpty(desc))
            {
                ShowStatus("Please fill in both the title and description.", Colors.Orange);
                return;
            }

            if (_discordUser is null)
            {
                ShowStatus("Please connect with Discord so we know who sent this report.", Colors.Orange);
                return;
            }

            SendBugReportBtn.IsEnabled = false;
            SendBugReportBtnText.Text  = "Sending...";
            BugStatusText.Visibility   = Visibility.Collapsed;

            var ok = await SendDiscordBugReportAsync(_discordUser.Value, title, desc, _bugImagePath);

            if (ok)
            {
                ShowStatus("Report sent! Thank you.", Colors.LimeGreen);
                SendBugReportBtnText.Text = "Send Report";

                BugTitleBox.Text       = "";
                BugDescBox.Text        = "";
                _bugImagePath          = null;
                BugImageFileName.Text  = "No file selected";

                await Task.Delay(2000);
                BugStatusText.Visibility = Visibility.Collapsed;
                SendBugReportBtn.IsEnabled = true;
            }
            else
            {
                ShowStatus("Failed to send the report. Please try again.", Colors.OrangeRed);
                SendBugReportBtnText.Text  = "Send Report";
                SendBugReportBtn.IsEnabled = true;
            }
        }

        private void ShowStatus(string message, Color color)
        {
            BugStatusText.Text       = message;
            BugStatusText.Foreground = new SolidColorBrush(color);
            BugStatusText.Visibility = Visibility.Visible;
        }

        private static async Task<bool> SendDiscordBugReportAsync(
            (string Id, string Username) discordUser, string title, string description, string? imagePath)
        {
            try
            {
                using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };

                var authorName = $"{discordUser.Username}  ·  ID: {discordUser.Id}";
                var footer     = $"Sonic Racing CrossWorlds Save Manager  ·  {AppVersion.GetDisplayVersion()}";

                string json;
                if (imagePath != null && File.Exists(imagePath))
                {
                    var fileName = IOPath.GetFileName(imagePath);
                    json = JsonSerializer.Serialize(new
                    {
                        embeds = new[] { new
                        {
                            author      = new { name = authorName },
                            title,
                            description,
                            color       = 0x0066CC,
                            image       = new { url = $"attachment://{fileName}" },
                            footer      = new { text = footer },
                            timestamp   = DateTime.UtcNow.ToString("o"),
                        }}
                    });

                    using var form = new MultipartFormDataContent();
                    form.Add(new StringContent(json, Encoding.UTF8, "application/json"), "payload_json");

                    var bytes       = await File.ReadAllBytesAsync(imagePath);
                    var fileContent = new ByteArrayContent(bytes);
                    fileContent.Headers.ContentType = new MediaTypeHeaderValue(GetImageMimeType(imagePath));
                    form.Add(fileContent, "files[0]", fileName);

                    var resp = await client.PostAsync(WebhookUrl, form);
                    return resp.IsSuccessStatusCode;
                }
                else
                {
                    json = JsonSerializer.Serialize(new
                    {
                        embeds = new[] { new
                        {
                            author      = new { name = authorName },
                            title,
                            description,
                            color       = 0x0066CC,
                            footer      = new { text = footer },
                            timestamp   = DateTime.UtcNow.ToString("o"),
                        }}
                    });

                    var content = new StringContent(json, Encoding.UTF8, "application/json");
                    var resp = await client.PostAsync(WebhookUrl, content);
                    return resp.IsSuccessStatusCode;
                }
            }
            catch { return false; }
        }

        private static string GetImageMimeType(string path) =>
            IOPath.GetExtension(path).ToLowerInvariant() switch
            {
                ".jpg" or ".jpeg" => "image/jpeg",
                ".png"            => "image/png",
                ".gif"            => "image/gif",
                ".webp"           => "image/webp",
                ".bmp"            => "image/bmp",
                _                 => "application/octet-stream",
            };
    }
}
