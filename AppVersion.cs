namespace SonicRacingSaveManager
{
    public static class AppVersion
    {
        // Current version of the application
        // Format: Major.Minor.Patch (e.g., "1.0.0")
        // IMPORTANT: Update this when releasing new versions!
        public const string CURRENT_VERSION = "1.1.1";

        // GitHub repository information for update checking
        public const string GITHUB_OWNER = "An-Average-Developer";
        public const string GITHUB_REPO = "Sonic-Racing-CrossWorlds-Save-Manager";
        public const string GITHUB_BRANCH = "main";

        // Enable or disable automatic update checking
        public const bool AUTO_UPDATE_ENABLED = true;

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
