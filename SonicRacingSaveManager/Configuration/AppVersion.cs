namespace SonicRacingSaveManager.Configuration
{
    public static class AppVersion
    {
        public const string CURRENT_VERSION = "2.1.0";

        public const string GITHUB_OWNER = "An-Average-Developer";
        public const string GITHUB_REPO = "Sonic-Racing-CrossWorlds-Save-Manager";
        public const string GITHUB_BRANCH = "main";

        public const bool AUTO_UPDATE_ENABLED = true;

        // Controlled by <SummerMode> in the .csproj
#if SUMMER_MODE
        public const bool SUMMER_MODE = true;
#else
        public const bool SUMMER_MODE = false;
#endif

        // Controlled by <HalloweenMode> in the .csproj
#if HALLOWEEN_MODE
        public const bool HALLOWEEN_MODE = true;
#else
        public const bool HALLOWEEN_MODE = false;
#endif

        // Controlled by <ChristmasMode> in the .csproj
#if CHRISTMAS_MODE
        public const bool CHRISTMAS_MODE = true;
#else
        public const bool CHRISTMAS_MODE = false;
#endif

        /// <summary>The season theme baked into the .csproj at build time (used when the user's season theme setting is "default").</summary>
        public static string BuildDefaultTheme => SUMMER_MODE ? "summer" : HALLOWEEN_MODE ? "halloween" : CHRISTMAS_MODE ? "christmas" : "classic";

        public static string GetDisplayVersion()
        {
            return $"v{CURRENT_VERSION}";
        }

        public static string GetGitHubRepoUrl()
        {
            return $"https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}";
        }
    }
}
