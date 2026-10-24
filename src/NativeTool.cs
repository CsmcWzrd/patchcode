using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
namespace PatchCodeGui
{
    public static class NativeTool
    {
        // CommandLineToArgvW / Microsoft CRT quoting, including trailing backslashes.
        public static string Quote(string s)
        {
            var b = new StringBuilder("\""); int slash = 0;
            foreach (char c in s) { if (c == '\\') slash++; else { if (c == '"') b.Append('\\', slash * 2 + 1); else b.Append('\\', slash); b.Append(c); slash = 0; } }
            b.Append('\\', slash * 2); return b.Append('"').ToString();
        }
        public static async Task<string> Generate(string a, string b, CancellationToken token)
        {
            string exe = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "tools", "diffcode.exe");
            if (!File.Exists(exe)) throw new FileNotFoundException("Build the complete solution to produce tools/diffcode.exe.", exe);
            // Upstream uses ANSI Win32 paths and byte-oriented metadata. Avoid silent filename corruption.
            if ((a+b).Any(c => c > 127 || c == '\r' || c == '\n')) throw new IOException("Native PatchCode export requires ASCII input paths. Copy the inputs to ASCII paths first. Unicode file content is supported as bytes.");
            await Task.Run(() => {
                var inventory=Compare.Inventory(a, token).Values.Concat(Compare.Inventory(b, token).Values);
                if(inventory.Any(path=>path.Any(c=>c>127||c=='\r'||c=='\n'))) throw new IOException("Native export requires ASCII filenames throughout both input trees.");
            }, token);
            string output = Path.Combine(Path.GetTempPath(), "pcg-" + Guid.NewGuid().ToString("N") + ".patch");
            if (output.Any(c => c > 127)) throw new IOException("Set TEMP to an ASCII path for the native tools.");
            try
            {
                var psi = new ProcessStartInfo(exe, string.Join(" ", new[] { "--check-binary", "-o", Quote(output), Quote(a), Quote(b) }))
                    { UseShellExecute = false, CreateNoWindow = true, RedirectStandardError = true, RedirectStandardOutput = true };
                using (var process = Process.Start(psi))
                {
                    var stdout = process.StandardOutput.ReadToEndAsync(); var stderr = process.StandardError.ReadToEndAsync();
                    using (var timeout = CancellationTokenSource.CreateLinkedTokenSource(token))
                    {
                        timeout.CancelAfter(TimeSpan.FromMinutes(2));
                        using (timeout.Token.Register(() => { try { if (!process.HasExited) process.Kill(); } catch (InvalidOperationException) { } }))
                        { await Task.Run(() => process.WaitForExit()); }
                        string log = await stdout + Environment.NewLine + await stderr;
                        if (timeout.IsCancellationRequested) throw new OperationCanceledException("Native export cancelled or exceeded two minutes.");
                        if (process.ExitCode != 0) throw new IOException("diffcode exit " + process.ExitCode + Environment.NewLine + log);
                        string patch = "";
                        try { if(File.Exists(output)) patch=new UTF8Encoding(false,true).GetString(File.ReadAllBytes(output)); }
                        catch(DecoderFallbackException) { throw new IOException("Native patch contains non-UTF-8 bytes. Use UTF-8 input content for editing/export in this client.\r\n"+log); }
                        return log + "\0" + patch;
                    }
                }
            }
            finally { if (File.Exists(output)) File.Delete(output); }
        }
    }
}
