using System.IO;
using System.Reflection;

namespace SonicRacingSaveManager.Common.Services;

// Extracts the embedded Resources\Steam\** artwork (icon, banner, capsule, poster, logo) to a
// temp folder at startup, so the Steam shortcut/artwork feature can read them as loose files —
// needed because Steam's grid artwork and shortcut icon must be real files on disk, and this app
// ships as a single self-contained exe with no loose asset files alongside it.
public static class SteamAssetExtractor
{
    public static readonly string TempDir =
        Path.Combine(Path.GetTempPath(), "SonicRacingSaveManager");

    public static void Extract()
    {
        if (Directory.Exists(TempDir))
            Directory.Delete(TempDir, recursive: true);

        var asm = Assembly.GetExecutingAssembly();
        const string prefix = "assets___";

        foreach (var name in asm.GetManifestResourceNames())
        {
            if (!name.StartsWith(prefix)) continue;

            var relPath = name[prefix.Length..];
            var dest = Path.Combine(TempDir, "Assets", relPath);
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);

            using var stream = asm.GetManifestResourceStream(name)!;
            using var file = File.Create(dest);
            stream.CopyTo(file);
        }
    }
}
