using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace SonicRacingSaveManager.Common.Services
{
    // ── Discord OAuth2 (implicit grant) ───────────────────────────────────────────
    //
    // SETUP (one-time, in Discord Developer Portal):
    //   1. Go to https://discord.com/developers/applications → "Sonic Racing
    //      CrossWorlds Save Manager" (the same app used for Rich Presence) → OAuth2
    //   2. Under "Redirects", add exactly:  http://localhost:45689/callback
    //   3. Save changes.
    //
    // HOW IT WORKS:
    //   1. Opens the system browser to Discord's authorization page (scope: identify).
    //   2. Starts a local HTTP listener on port 45689.
    //   3. Discord redirects to http://localhost:45689/callback#access_token=...
    //      (the token is in the URL fragment — browsers don't send fragments to servers,
    //       so we serve a tiny HTML page that forwards them as query params to /token).
    //   4. /token handler fetches the user's profile via /api/users/@me and returns it.
    public static class DiscordOAuthService
    {
        private const string ClientId    = "ClientId"; //ClientId
        private const int    Port        = 45689;
        public  const string RedirectUri = "http://localhost:45689/callback";

        public static string BuildAuthUrl() =>
            "https://discord.com/oauth2/authorize" +
            $"?client_id={ClientId}" +
            $"&redirect_uri={Uri.EscapeDataString(RedirectUri)}" +
            "&response_type=token" +
            "&scope=identify";

        /// <summary>
        /// Opens the browser to Discord's OAuth page, waits up to 5 minutes for the user
        /// to authorize, then returns (Id, Username) or null on failure / cancellation.
        /// </summary>
        public static async Task<(string Id, string Username)?> AuthenticateAsync(CancellationToken ct = default)
        {
            using var listener = new HttpListener();
            listener.Prefixes.Add($"http://localhost:{Port}/");

            try { listener.Start(); }
            catch { return null; }

            try
            {
                Process.Start(new ProcessStartInfo(BuildAuthUrl()) { UseShellExecute = true });
            }
            catch { listener.Stop(); return null; }

            // 5-minute timeout linked to the caller's token
            using var timeoutCts = new CancellationTokenSource(TimeSpan.FromMinutes(5));
            using var linkedCts  = CancellationTokenSource.CreateLinkedTokenSource(ct, timeoutCts.Token);
            var linked = linkedCts.Token;

            (string Id, string Username)? result = null;

            try
            {
                while (!linked.IsCancellationRequested)
                {
                    // WaitAsync propagates cancellation cleanly without leaking GetContextAsync tasks
                    var ctx = await listener.GetContextAsync().WaitAsync(linked);
                    var path = ctx.Request.Url?.AbsolutePath ?? "";

                    if (path == "/callback")
                    {
                        // Serve a page that reads the fragment and POSTs it here.
                        // A fetch POST avoids a second browser navigation, which was
                        // the cause of the "stays charging" issue with window.location.replace.
                        await WriteHtml(ctx.Response, FragmentForwarderHtml);
                    }
                    else if (path == "/token" && ctx.Request.HttpMethod == "POST")
                    {
                        using var reader = new StreamReader(ctx.Request.InputStream, Encoding.UTF8);
                        var body  = await reader.ReadToEndAsync(linked);
                        var token = ParseParam(body, "access_token");

                        if (!string.IsNullOrEmpty(token))
                            result = await FetchUserAsync(token, linked);

                        // Allow the fetch response to be read by the page's JS
                        ctx.Response.Headers["Access-Control-Allow-Origin"] = "*";
                        await WriteHtml(ctx.Response, result.HasValue ? SuccessHtml : ErrorHtml);

                        // Give the OS a moment to actually flush the response to the browser
                        // before the listener below tears the socket down — otherwise the
                        // fetch() in FragmentForwarderHtml can see the connection reset and
                        // report "Error. Please close and try again." even though the account
                        // was already linked successfully on this end.
                        await Task.Delay(250, CancellationToken.None);
                        break;
                    }
                    else
                    {
                        ctx.Response.StatusCode = 404;
                        ctx.Response.Close();
                    }
                }
            }
            catch (OperationCanceledException) { }
            catch { }
            finally
            {
                try { listener.Stop(); } catch { }
            }

            return result;
        }

        // ── Persistent user cache ──────────────────────────────────────────────────

        private static readonly string CachePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SonicRacingSaveManager", "discord_user.json");

        public static (string Id, string Username)? LoadCached()
        {
            try
            {
                if (!File.Exists(CachePath)) return null;
                var raw = File.ReadAllText(CachePath);
                using var doc = JsonDocument.Parse(raw);
                var id   = doc.RootElement.GetProperty("id").GetString()       ?? "";
                var name = doc.RootElement.GetProperty("username").GetString() ?? "";
                return string.IsNullOrEmpty(id) ? null : (id, name);
            }
            catch { return null; }
        }

        public static void SaveCached(string id, string username)
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(CachePath)!);
                var json = JsonSerializer.Serialize(new { id, username });
                File.WriteAllText(CachePath, json);
            }
            catch { }
        }

        public static void ClearCached()
        {
            try { File.Delete(CachePath); } catch { }
        }

        // ── Internal helpers ───────────────────────────────────────────────────────

        private static async Task<(string Id, string Username)?> FetchUserAsync(string token, CancellationToken ct)
        {
            try
            {
                using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(10) };
                http.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);
                var json = await http.GetStringAsync("https://discord.com/api/users/@me", ct);
                using var doc = JsonDocument.Parse(json);
                var id   = doc.RootElement.GetProperty("id").GetString()       ?? "";
                var name = doc.RootElement.GetProperty("username").GetString() ?? "";
                return string.IsNullOrEmpty(id) ? null : (id, name);
            }
            catch { return null; }
        }

        private static string ParseParam(string urlEncoded, string key)
        {
            foreach (var pair in urlEncoded.Split('&'))
            {
                var idx = pair.IndexOf('=');
                if (idx < 0) continue;
                var k = Uri.UnescapeDataString(pair[..idx].Replace('+', ' '));
                var v = Uri.UnescapeDataString(pair[(idx + 1)..].Replace('+', ' '));
                if (k == key) return v;
            }
            return "";
        }

        private static async Task WriteHtml(HttpListenerResponse resp, string html)
        {
            try
            {
                var bytes = Encoding.UTF8.GetBytes(html);
                resp.ContentType     = "text/html; charset=utf-8";
                resp.ContentLength64 = bytes.Length;
                resp.StatusCode      = 200;
                await resp.OutputStream.WriteAsync(bytes);
                await resp.OutputStream.FlushAsync();
                resp.OutputStream.Close();
            }
            catch { }
        }

        // Browsers don't send URL fragments to servers, so we can't read #access_token
        // directly. Instead of a JS redirect (which needs a second navigation and can miss
        // the server loop), we POST the fragment as a form body via fetch — same origin,
        // no navigation, response is written back into the page.
        private const string FragmentForwarderHtml = """
            <!DOCTYPE html>
            <html>
            <body style="font-family:sans-serif;background:#1E1E2E;color:#0066CC;
                         text-align:center;padding:60px 40px;margin:0">
              <p id="s" style="font-size:14px">Connecting...</p>
              <script>
                var params = window.location.hash.substring(1) || 'error=no_token';
                fetch('/token', {
                  method: 'POST',
                  headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                  body: params
                })
                .then(function(r) { return r.text(); })
                .then(function(html) {
                  document.open('text/html'); document.write(html); document.close();
                })
                .catch(function() {
                  document.getElementById('s').textContent = 'Error. Please close and try again.';
                });
              </script>
            </body>
            </html>
            """;

        private const string SuccessHtml = """
            <!DOCTYPE html>
            <html>
            <body style="font-family:sans-serif;background:#1E1E2E;color:#00CC88;
                         text-align:center;padding:60px 40px;margin:0">
              <div style="font-size:52px;margin-bottom:16px">✓</div>
              <h2 style="margin:0 0 10px;font-size:20px">Connected!</h2>
              <p style="color:#4A7A5A;margin:0;font-size:14px">You can close this window.</p>
            </body></html>
            """;

        private const string ErrorHtml = """
            <!DOCTYPE html>
            <html>
            <body style="font-family:sans-serif;background:#1E1E2E;color:#CC4422;
                         text-align:center;padding:60px 40px;margin:0">
              <div style="font-size:52px;margin-bottom:16px">✕</div>
              <h2 style="margin:0 0 10px;font-size:20px">Not connected</h2>
              <p style="color:#7A3A22;margin:0;font-size:14px">You can close this window and try again.</p>
            </body></html>
            """;
    }
}
