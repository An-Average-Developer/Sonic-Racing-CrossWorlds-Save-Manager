using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using Microsoft.Win32;

namespace SonicRacingSaveManager.Common.Services
{
    /// <summary>A node in Valve's binary VDF format. Values are <see cref="string"/>, <see cref="int"/>,
    /// or another <see cref="VdfNode"/>. Insertion order is preserved (Steam requires shortcuts to be
    /// keyed "0","1","2",... in order).</summary>
    public sealed class VdfNode : Dictionary<string, object>;

    /// <summary>
    /// Adds/updates this app's own entry in Steam's "Add a Non-Steam Game" shortcuts list
    /// (userdata\&lt;id&gt;\config\shortcuts.vdf, Valve's binary VDF format) so the Save Manager can
    /// add itself to Steam without the user manually creating a shortcut.
    /// </summary>
    public static class SteamShortcutService
    {
        public const string AppName = "Sonic Racing CrossWorlds Save Manager";

        // ── Steam / active-user discovery ────────────────────────────────────────

        public static string? FindSteamPath()
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam");
                if (key?.GetValue("SteamPath") is string p && Directory.Exists(p))
                    return p.Replace('/', '\\');
            }
            catch { }

            try
            {
                using var key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Valve\Steam")
                              ?? Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Valve\Steam");
                if (key?.GetValue("InstallPath") is string p && Directory.Exists(p))
                    return p;
            }
            catch { }

            foreach (var fallback in new[] { @"C:\Program Files (x86)\Steam", @"C:\Program Files\Steam" })
                if (File.Exists(Path.Combine(fallback, "steam.exe")))
                    return fallback;

            return null;
        }

        /// <summary>Resolves the SteamID3 (account id) folder under userdata\ for the active user,
        /// falling back to loginusers.vdf (plain-text VDF) and then to the only userdata subfolder
        /// present, if there's just one.</summary>
        public static string? FindActiveUserId(string steamPath)
        {
            try
            {
                using var key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam\ActiveProcess");
                if (key?.GetValue("ActiveUser") is int id && id != 0
                    && Directory.Exists(Path.Combine(steamPath, "userdata", id.ToString())))
                    return id.ToString();
            }
            catch { }

            try
            {
                var loginPath = Path.Combine(steamPath, "config", "loginusers.vdf");
                if (File.Exists(loginPath))
                {
                    var text = File.ReadAllText(loginPath);
                    string? bestId = null;
                    long bestTimestamp = -1;

                    foreach (Match m in Regex.Matches(text, "\"(7656\\d{13})\"\\s*\\{([^{}]*)\\}"))
                    {
                        var id64 = ulong.Parse(m.Groups[1].Value);
                        var block = m.Groups[2].Value;
                        var accountId = (id64 - 76561197960265728UL).ToString();

                        if (Regex.IsMatch(block, "\"MostRecent\"\\s*\"1\""))
                            return Directory.Exists(Path.Combine(steamPath, "userdata", accountId)) ? accountId : null;

                        var tsMatch = Regex.Match(block, "\"Timestamp\"\\s*\"(\\d+)\"");
                        var ts = tsMatch.Success ? long.Parse(tsMatch.Groups[1].Value) : 0;
                        if (ts > bestTimestamp) { bestTimestamp = ts; bestId = accountId; }
                    }

                    if (bestId != null && Directory.Exists(Path.Combine(steamPath, "userdata", bestId)))
                        return bestId;
                }
            }
            catch { }

            try
            {
                var folders = Directory.GetDirectories(Path.Combine(steamPath, "userdata"))
                    .Select(Path.GetFileName)
                    .Where(n => n != "0" && n != null)
                    .ToList();
                if (folders.Count == 1) return folders[0];
            }
            catch { }

            return null;
        }

        // ── Binary VDF read/write ────────────────────────────────────────────────

        public static VdfNode ReadShortcutsVdf(string path)
        {
            using var stream = File.OpenRead(path);
            using var reader = new BinaryReader(stream);
            return ParseObject(reader);
        }

        private static VdfNode ParseObject(BinaryReader reader)
        {
            var node = new VdfNode();
            while (true)
            {
                var type = reader.ReadByte();
                if (type == 0x08) return node;

                var key = ReadCString(reader);
                node[key] = type switch
                {
                    0x00 => ParseObject(reader),
                    0x01 => ReadCString(reader),
                    0x02 => reader.ReadInt32(),
                    _ => throw new InvalidDataException($"Unknown VDF type byte 0x{type:X2} for key '{key}'"),
                };
            }
        }

        private static string ReadCString(BinaryReader reader)
        {
            var bytes = new List<byte>();
            byte b;
            while ((b = reader.ReadByte()) != 0) bytes.Add(b);
            return Encoding.UTF8.GetString(bytes.ToArray());
        }

        public static void WriteShortcutsVdf(string path, VdfNode root)
        {
            using var stream = File.Create(path);
            using var writer = new BinaryWriter(stream);
            WriteObject(writer, root);
        }

        private static void WriteObject(BinaryWriter writer, VdfNode node)
        {
            foreach (var (key, value) in node)
            {
                switch (value)
                {
                    case VdfNode child:
                        writer.Write((byte)0x00);
                        WriteCString(writer, key);
                        WriteObject(writer, child);
                        break;
                    case string s:
                        writer.Write((byte)0x01);
                        WriteCString(writer, key);
                        WriteCString(writer, s);
                        break;
                    case int i:
                        writer.Write((byte)0x02);
                        WriteCString(writer, key);
                        writer.Write(i);
                        break;
                    default:
                        throw new InvalidDataException($"Unsupported VDF value type for key '{key}'");
                }
            }
            writer.Write((byte)0x08);
        }

        private static void WriteCString(BinaryWriter writer, string s)
        {
            writer.Write(Encoding.UTF8.GetBytes(s));
            writer.Write((byte)0);
        }

        // ── AppID (Steam's legacy shortcut id: CRC32("quoted exe" + AppName) | 0x80000000) ─────────

        private static readonly uint[] Crc32Table = BuildCrc32Table();

        private static uint[] BuildCrc32Table()
        {
            const uint poly = 0xEDB88320;
            var table = new uint[256];
            for (uint i = 0; i < 256; i++)
            {
                var c = i;
                for (var k = 0; k < 8; k++)
                    c = (c & 1) != 0 ? poly ^ (c >> 1) : c >> 1;
                table[i] = c;
            }
            return table;
        }

        private static uint Crc32Compute(string s)
        {
            var crc = 0xFFFFFFFFu;
            foreach (var b in Encoding.UTF8.GetBytes(s))
                crc = Crc32Table[(crc ^ b) & 0xFF] ^ (crc >> 8);
            return crc ^ 0xFFFFFFFFu;
        }

        public static uint ComputeAppId(string exePath, string appName) =>
            Crc32Compute($"\"{exePath}\"{appName}") | 0x80000000;

        // ── Entry add-or-update ──────────────────────────────────────────────────

        /// <summary>Adds a new shortcut entry or updates the existing one matching <paramref name="appName"/>
        /// in place, then re-indexes all entries as "0","1","2",... as Steam requires.</summary>
        public static void AddOrUpdateEntry(VdfNode root, string exePath, string startDir, string appName, string iconPath)
        {
            if (!root.TryGetValue("shortcuts", out var existingShortcuts) || existingShortcuts is not VdfNode)
            {
                existingShortcuts = new VdfNode();
                root["shortcuts"] = existingShortcuts;
            }
            var shortcuts = (VdfNode)existingShortcuts;

            var entries = shortcuts.Values.OfType<VdfNode>().ToList();
            var entry = entries.FirstOrDefault(e => e.TryGetValue("AppName", out var n) && n as string == appName);

            var isNew = entry is null;
            entry ??= new VdfNode();

            entry["appid"] = unchecked((int)ComputeAppId(exePath, appName));
            entry["AppName"] = appName;
            entry["Exe"] = $"\"{exePath}\"";
            entry["StartDir"] = startDir;
            entry["icon"] = iconPath;
            entry["ShortcutPath"] = entry.TryGetValue("ShortcutPath", out var sp) ? sp : "";
            entry["LaunchOptions"] = entry.TryGetValue("LaunchOptions", out var lo) ? lo : "";
            entry["IsHidden"] = entry.TryGetValue("IsHidden", out var ih) ? ih : 0;
            entry["AllowDesktopConfig"] = 1;
            entry["AllowOverlay"] = 1;
            entry["OpenVR"] = entry.TryGetValue("OpenVR", out var vr) ? vr : 0;
            entry["Devkit"] = entry.TryGetValue("Devkit", out var dk) ? dk : 0;
            entry["DevkitGameID"] = entry.TryGetValue("DevkitGameID", out var dgid) ? dgid : "";
            entry["DevkitOverrideAppID"] = entry.TryGetValue("DevkitOverrideAppID", out var doa) ? doa : 0;
            entry["LastPlayTime"] = (int)DateTimeOffset.UtcNow.ToUnixTimeSeconds();
            entry["FlatpakAppID"] = entry.TryGetValue("FlatpakAppID", out var fid) ? fid : "";
            entry["tags"] = entry.TryGetValue("tags", out var tags) ? tags : new VdfNode();

            if (isNew) entries.Add(entry);

            shortcuts.Clear();
            for (var i = 0; i < entries.Count; i++)
                shortcuts[i.ToString()] = entries[i];
        }

        // ── Icon: copy the chosen PNG to a stable, permanent location ────────────

        public static string CopyIconToStableLocation(string sourceIconPath)
        {
            var destDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "SonicRacingSaveManager", "SteamIcon");
            Directory.CreateDirectory(destDir);
            var destPath = Path.Combine(destDir, Path.GetFileName(sourceIconPath));
            File.Copy(sourceIconPath, destPath, overwrite: true);
            return destPath;
        }

        // ── Library artwork: Poster/Capsule/Hero/Logo grid images ────────────────

        /// <summary>Copies the app's Poster/Capsule/Banner/Logo artwork into Steam's
        /// userdata\&lt;id&gt;\config\grid\ folder under this shortcut's app id, using Steam's
        /// non-Steam-game naming convention (&lt;appid&gt;p.jpg = Poster, &lt;appid&gt;.jpg = Capsule,
        /// &lt;appid&gt;_hero.jpg = Banner, &lt;appid&gt;_logo.png = Logo). Without these, Steam shows
        /// only the small list icon and no artwork on the library/home pages.</summary>
        /// <exception cref="InvalidOperationException">Thrown if none of the source images were found
        /// (e.g. a build missing the embedded assets), so the failure isn't silent.</exception>
        public static void ApplyArtwork(string steamPath, string userId, uint appId32)
        {
            var assetsDir = Path.Combine(SteamAssetExtractor.TempDir, "Assets", "Steam");
            var gridDir = Path.Combine(steamPath, "userdata", userId, "config", "grid");
            Directory.CreateDirectory(gridDir);

            var copied = 0;
            copied += CopyIfExists(Path.Combine(assetsDir, "Poster.jpg"), Path.Combine(gridDir, $"{appId32}p.jpg"));
            copied += CopyIfExists(Path.Combine(assetsDir, "Capsule.jpg"), Path.Combine(gridDir, $"{appId32}.jpg"));
            copied += CopyIfExists(Path.Combine(assetsDir, "Banner.jpg"), Path.Combine(gridDir, $"{appId32}_hero.jpg"));
            copied += CopyIfExists(Path.Combine(assetsDir, "Logo.png"), Path.Combine(gridDir, $"{appId32}_logo.png"));

            if (copied == 0)
                throw new InvalidOperationException(
                    $"Shortcut was added, but no library artwork was found to copy from '{assetsDir}'.");
        }

        private static int CopyIfExists(string src, string dest)
        {
            if (!File.Exists(src)) return 0;
            File.Copy(src, dest, overwrite: true);
            return 1;
        }

        // ── Steam process control ────────────────────────────────────────────────

        private static readonly string[] SteamProcessNames = { "steam", "steamwebhelper", "steamservice" };

        public static bool IsSteamRunning() => Process.GetProcessesByName("steam").Length > 0;

        /// <summary>Gracefully shuts Steam down (steam.exe -shutdown), waiting up to 20s, then
        /// force-kills any stragglers. Steam caches shortcuts.vdf and grid artwork in memory while
        /// running, so it must be fully closed before those files are edited on disk.</summary>
        public static void CloseSteam(string steamPath)
        {
            if (!IsSteamRunning()) return;

            try { Process.Start(Path.Combine(steamPath, "steam.exe"), "-shutdown"); }
            catch { }

            var deadline = DateTime.UtcNow.AddSeconds(20);
            while (DateTime.UtcNow < deadline && IsSteamRunning())
                Thread.Sleep(500);

            foreach (var name in SteamProcessNames)
            foreach (var p in Process.GetProcessesByName(name))
            {
                try { p.Kill(entireProcessTree: true); }
                catch { }
            }
            Thread.Sleep(500);
        }

        public static void StartSteam(string steamPath)
        {
            try { Process.Start(Path.Combine(steamPath, "steam.exe")); }
            catch { }
        }

        // ── Top-level orchestration ──────────────────────────────────────────────

        /// <summary>Adds or updates this app's shortcut entry. Steam must already be closed by the
        /// caller (see <see cref="CloseSteam"/>) before calling this. Returns the shortcuts.vdf path used.</summary>
        public static string AddOrUpdateShortcut(string exePath, string iconSourcePath)
        {
            var steamPath = FindSteamPath()
                ?? throw new InvalidOperationException("Steam installation not found.");
            var userId = FindActiveUserId(steamPath)
                ?? throw new InvalidOperationException("Could not determine the active Steam user.");

            var vdfPath = Path.Combine(steamPath, "userdata", userId, "config", "shortcuts.vdf");
            var iconPath = CopyIconToStableLocation(iconSourcePath);
            var startDir = Path.GetDirectoryName(exePath) + Path.DirectorySeparatorChar;

            VdfNode root;
            if (File.Exists(vdfPath))
            {
                File.Copy(vdfPath, vdfPath + ".bak", overwrite: true);
                root = ReadShortcutsVdf(vdfPath);
            }
            else
            {
                Directory.CreateDirectory(Path.GetDirectoryName(vdfPath)!);
                root = new VdfNode();
            }

            AddOrUpdateEntry(root, exePath, startDir, AppName, iconPath);
            WriteShortcutsVdf(vdfPath, root);

            ApplyArtwork(steamPath, userId, ComputeAppId(exePath, AppName));

            return vdfPath;
        }
    }
}
