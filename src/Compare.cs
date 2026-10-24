using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace PatchCodeGui
{
    public sealed class Entry
    {
        public string Relative, OldPath, NewPath, Status;
        public bool Binary;
        public override string ToString() { return Relative + " [" + Status + "]"; }
    }
    public sealed class DiffRow
    {
        public string Old, New;
        public int OldNumber, NewNumber;
        public bool Changed { get { return OldNumber == 0 || NewNumber == 0 || Old != New; } }
    }
    public static class Compare
    {
        public const long MaxPreviewBytes = 8 * 1024 * 1024;
        public static Dictionary<string, string> Inventory(string root, CancellationToken token)
        {
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if ((File.GetAttributes(root) & FileAttributes.ReparsePoint) != 0) throw new IOException("Symbolic links and junctions are not supported: " + root);
            if (File.Exists(root)) { result.Add(Path.GetFileName(root), root); return result; }
            var pending = new Stack<string>(); pending.Push(root);
            while (pending.Count > 0)
            {
                token.ThrowIfCancellationRequested();
                string dir = pending.Pop();
                if ((File.GetAttributes(dir) & FileAttributes.ReparsePoint) != 0)
                    throw new IOException("Symbolic links and junctions are not supported: " + dir);
                foreach (string p in Directory.EnumerateFileSystemEntries(dir))
                {
                    token.ThrowIfCancellationRequested();
                    var attr = File.GetAttributes(p);
                    if ((attr & FileAttributes.ReparsePoint) != 0) throw new IOException("Symbolic links and junctions are not supported: " + p);
                    if ((attr & FileAttributes.Directory) != 0) pending.Push(p);
                    else result.Add(p.Substring(root.TrimEnd('\\', '/').Length + 1), p);
                }
            }
            return result;
        }
        public static bool Equal(string a, string b, CancellationToken token)
        {
            using (var x = File.OpenRead(a)) using (var y = File.OpenRead(b))
            {
                if (x.Length != y.Length) return false;
                byte[] xb = new byte[65536], yb = new byte[65536];
                int n; while ((n = x.Read(xb, 0, xb.Length)) > 0)
                {
                    token.ThrowIfCancellationRequested();
                    int got = 0; while (got < n) { int r = y.Read(yb, got, n - got); if (r == 0) return false; got += r; }
                    for (int i = 0; i < n; i++) if (xb[i] != yb[i]) return false;
                }
                return true;
            }
        }
        public static bool Binary(string path, CancellationToken token)
        {
            if (path == null) return false;
            using (var f = File.OpenRead(path))
            {
                byte[] buffer = new byte[65536]; int n;
                while ((n = f.Read(buffer, 0, buffer.Length)) > 0) { token.ThrowIfCancellationRequested();
                    for (int i = 0; i < n; i++) { int c=buffer[i]; if (c == 0 || (c < 32 && c != 9 && c != 10 && c != 13 && c != 12)) return true; } }
            }
            return false;
        }
        public static List<Entry> Scan(string a, string b, bool folders, CancellationToken token)
        {
            var x = Inventory(a, token); var y = Inventory(b, token);
            if (!folders) y = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { { x.Keys.First(), b } };
            var entries = new List<Entry>();
            foreach (string rel in x.Keys.Union(y.Keys, StringComparer.OrdinalIgnoreCase).OrderBy(p => p, StringComparer.OrdinalIgnoreCase))
            {
                token.ThrowIfCancellationRequested(); string ap, bp; x.TryGetValue(rel, out ap); y.TryGetValue(rel, out bp);
                string status = ap == null ? "Added" : bp == null ? "Removed" : Equal(ap, bp, token) ? "Identical" : "Modified";
                entries.Add(new Entry { Relative = rel, OldPath = ap, NewPath = bp, Status = status,
                    Binary = Binary(ap, token) || Binary(bp, token) });
            }
            return entries;
        }
        public static string Read(string path)
        {
            if (path == null) return "";
            if (new FileInfo(path).Length > MaxPreviewBytes) throw new IOException("Preview limit is 8 MiB per file. Native patch export remains available.");
            byte[] bytes = File.ReadAllBytes(path);
            try { return new UTF8Encoding(false, true).GetString(bytes); }
            catch (DecoderFallbackException) { return Encoding.Default.GetString(bytes); }
        }
        // Physical lines retain terminators so CRLF/LF and final-newline differences are visible.
        public static string[] Lines(string text)
        {
            var r = new List<string>(); int start = 0;
            for (int i = 0; i < text.Length; i++) if (text[i] == '\n') { r.Add(text.Substring(start, i - start + 1)); start = i + 1; if(r.Count>200000) throw new IOException("Preview limit is 200,000 lines per file."); }
            if (start < text.Length) r.Add(text.Substring(start)); return r.ToArray();
        }
        public static string Display(string s)
        {
            if (s == null) return "";
            return s.EndsWith("\r\n") ? s.Substring(0, s.Length - 2) + "  [CRLF]" :
                s.EndsWith("\n") ? s.Substring(0, s.Length - 1) + "  [LF]" : s + "  [no newline]";
        }
        public static List<DiffRow> Align(string[] a, string[] b, CancellationToken token)
        {
            var r = new List<DiffRow>(); int prefix = 0;
            while (prefix < a.Length && prefix < b.Length && a[prefix] == b[prefix]) { r.Add(Row(a, b, prefix, prefix)); prefix++; }
            int ae = a.Length, be = b.Length;
            while (ae > prefix && be > prefix && a[ae - 1] == b[be - 1]) { ae--; be--; }
            int n = ae - prefix, m = be - prefix;
            if ((long)(n + 1) * (m + 1) <= 4000000)
            {
                var dp = new int[n + 1, m + 1];
                for (int i = n - 1; i >= 0; i--) { token.ThrowIfCancellationRequested(); for (int j = m - 1; j >= 0; j--) dp[i,j] = a[prefix+i] == b[prefix+j] ? dp[i+1,j+1]+1 : Math.Max(dp[i+1,j],dp[i,j+1]); }
                int x = 0, y = 0;
                while (x < n || y < m)
                {
                    token.ThrowIfCancellationRequested();
                    if (x < n && y < m && a[prefix+x] == b[prefix+y]) { r.Add(Row(a,b,prefix+x++,prefix+y++)); }
                    else if (x < n && (y == m || dp[x+1,y] >= dp[x,y+1])) r.Add(Row(a,b,prefix+x++,-1));
                    else r.Add(Row(a,b,-1,prefix+y++));
                }
            }
            else // Bounded coarse replacement, matching upstream's large-input strategy.
            {
                for (int i = prefix; i < ae; i++) { token.ThrowIfCancellationRequested(); r.Add(Row(a,b,i,-1)); }
                for (int j = prefix; j < be; j++) { token.ThrowIfCancellationRequested(); r.Add(Row(a,b,-1,j)); }
            }
            while (ae < a.Length) r.Add(Row(a,b,ae++,be++));
            // Align each replacement block into paired old/new rows.
            var aligned = new List<DiffRow>(); int at=0;
            while(at<r.Count) {
                if(!r[at].Changed) { aligned.Add(r[at++]); continue; }
                var deleted=new List<DiffRow>(); var added=new List<DiffRow>();
                while(at<r.Count && r[at].Changed) { var row=r[at++]; if(row.OldNumber>0)deleted.Add(row); if(row.NewNumber>0)added.Add(row); }
                for(int i=0;i<Math.Max(deleted.Count,added.Count);i++) aligned.Add(new DiffRow {
                    Old=i<deleted.Count?deleted[i].Old:null, OldNumber=i<deleted.Count?deleted[i].OldNumber:0,
                    New=i<added.Count?added[i].New:null, NewNumber=i<added.Count?added[i].NewNumber:0 });
            }
            return aligned;
        }
        public static string PreserveLineEndings(string original, string edited)
        {
            var a=Lines(original); var b=Lines(edited);
            var left=a.Select(x=>x.TrimEnd('\r','\n')).ToArray();
            var right=b.Select(x=>x.TrimEnd('\r','\n')).ToArray();
            var aligned=Align(left,right,CancellationToken.None); var output=new StringBuilder();
            foreach(var row in aligned) if(row.NewNumber>0) {
                if(!row.Changed) {
                    string physical=a[row.OldNumber-1];
                    if(row.NewNumber==b.Length && b[row.NewNumber-1].EndsWith("\n")!=physical.EndsWith("\n"))
                        output.Append(right[row.NewNumber-1]+(b[row.NewNumber-1].EndsWith("\n")?"\n":""));
                    else output.Append(physical);
                }
                else { output.Append(right[row.NewNumber-1]); if(b[row.NewNumber-1].EndsWith("\n")) output.Append("\n"); }
            }
            return output.ToString();
        }
        static DiffRow Row(string[] a, string[] b, int x, int y)
        { return new DiffRow { Old = x < 0 ? null : a[x], New = y < 0 ? null : b[y], OldNumber = x+1, NewNumber = y+1 }; }
    }
}
