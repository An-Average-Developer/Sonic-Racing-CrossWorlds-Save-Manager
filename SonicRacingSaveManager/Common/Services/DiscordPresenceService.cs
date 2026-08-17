using System;
using DiscordRPC;
using DiscordRPC.Logging;

namespace SonicRacingSaveManager.Common.Services
{
    // ── Discord Rich Presence ────────────────────────────────────────────────
    //
    // Application registered at https://discord.com/developers/applications
    // as "Sonic Racing CrossWorlds Save Manager". Rich Presence art assets
    // uploaded there: "savemanager" (main/large image), "updates", "error".
    //
    // Only three states are tracked — this app doesn't have the many editor
    // screens the Rich Presence system was originally built for, so it stays
    // simple on purpose:
    //   Normal  = default state, shown on every tab except Updates/Reports
    //   Updates = the Updates tab is open (checking/downloading a new version)
    //   Error   = the Reports tab is open (reporting a bug)
    //
    // LAYOUT:
    //   Large image = "savemanager" (always)
    //   Small image = "updates" / "error" depending on mode, otherwise none
    //   Details (upper line) = mode-specific text, else the app name
    //   State   (lower line) = version
    public sealed class DiscordPresenceService : IDisposable
    {
        public enum PresenceMode
        {
            Normal,
            Updates,
            Error
        }

        private const string AppId = "AppID"; //AppID
        private const string AppName = "Sonic Racing CrossWorlds Save Manager";

        private DiscordRpcClient? _client;
        private readonly DateTime _sessionStart = DateTime.UtcNow;

        private string _versionLine = "";
        private bool _showActivity = true;
        private PresenceMode _mode = PresenceMode.Normal;

        public DiscordPresenceService()
        {
            try
            {
                _client = new DiscordRpcClient(AppId) { Logger = new NullLogger() };

                _client.OnReady += (_, _) => Send();

                _client.Initialize();
            }
            catch
            {
                _client = null;
            }
        }

        /// <summary>
        /// Enables or disables the presence. Call this on startup and whenever
        /// the user toggles the "Show Activity on Discord" setting.
        /// </summary>
        public void ApplySettings(bool showActivity)
        {
            _showActivity = showActivity;

            if (!_showActivity)
            {
                if (_client is null) return;
                try { _client.ClearPresence(); } catch { }
                return;
            }

            Send();
        }

        /// <summary>Sets the version shown on the State line (e.g. "v2.0.0").</summary>
        public void SetVersion(string version)
        {
            _versionLine = version;
            Send();
        }

        /// <summary>Switches between the three tracked presence states.</summary>
        public void SetMode(PresenceMode mode)
        {
            if (mode == _mode) return;
            _mode = mode;
            Send();
        }

        private void Send()
        {
            if (_client is null || !_showActivity) return;

            var (details, smallImageKey, smallImageText) = _mode switch
            {
                PresenceMode.Updates => ("Checking for Updates", "updates", "Checking for Updates"),
                PresenceMode.Error => ("Reporting a Bug", "error", "Reporting a Bug"),
                _ => (AppName, (string?)null, (string?)null),
            };

            var presence = new RichPresence
            {
                Details = details,
                State = _versionLine,
                Timestamps = new Timestamps(_sessionStart),
                Assets = new Assets
                {
                    LargeImageKey = "savemanager",
                    LargeImageText = AppName,
                    SmallImageKey = smallImageKey,
                    SmallImageText = smallImageText,
                },
            };

            try { _client.SetPresence(presence); }
            catch { }
        }

        public void Dispose()
        {
            if (_client is null) return;
            try
            {
                _client.ClearPresence();
                _client.Dispose();
            }
            catch { }
            _client = null;
        }
    }
}
