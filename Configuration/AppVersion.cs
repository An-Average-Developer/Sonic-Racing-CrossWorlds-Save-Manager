namespace SonicRacingSaveManager.Configuration
{
    public static class AppVersion
    {
        public const string CURRENT_VERSION = "1.1.3";

        public const string GITHUB_OWNER = "An-Average-Developer";
        public const string GITHUB_REPO = "Sonic-Racing-CrossWorlds-Save-Manager";
        public const string GITHUB_BRANCH = "main";

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
