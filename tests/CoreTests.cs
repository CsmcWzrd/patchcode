using System;
using System.IO;
using System.Linq;
using System.Threading;
using PatchCodeGui;
class CoreTests
{
    static int count;
    static void Check(bool condition,string name) { if(!condition)throw new Exception("FAIL "+name); Console.WriteLine("PASS "+name);count++; }
    static void RoundTrip(string a,string b)
    {
        var x=Compare.Lines(a);var y=Compare.Lines(b);var rows=Compare.Align(x,y,CancellationToken.None);
        Check(string.Concat(rows.Where(r=>r.OldNumber>0).Select(r=>r.Old))==a,"old reconstruction");
        Check(string.Concat(rows.Where(r=>r.NewNumber>0).Select(r=>r.New))==b,"new reconstruction");
    }
    static void Main()
    {
        RoundTrip("","new\n");RoundTrip("old\n","");RoundTrip("a\nb\nc\n","a\nB\nc\n");
        RoundTrip("same\r\n","same\n");RoundTrip("a","a\n");RoundTrip("a\na\nb\n","a\nb\na\n");
        RoundTrip(string.Join("\n",Enumerable.Repeat("old",2100)),string.Join("\n",Enumerable.Repeat("new",2100)));
        var pair=Compare.Align(Compare.Lines("a\nb\nc\n"),Compare.Lines("a\nB\nc\n"),CancellationToken.None);
        Check(pair.Count==3&&pair[1].Old=="b\n"&&pair[1].New=="B\n","replacement paired");
        Check(NativeTool.Quote("C:\\path with space\\")=="\"C:\\path with space\\\\\"","trailing slash quoting");
        Check(NativeTool.Quote("a\"b")=="\"a\\\"b\"","embedded quote");
        string patch="author:A\n<@+@+@>\nx\r\n<@+@+@>\ny\r\n<@+@+@>\nz\r\n<@+@+@>\n";
        Check(Compare.PreserveLineEndings(patch,patch.Replace("\r\n","\n").Replace("author:A","author:B"))==patch.Replace("author:A","author:B"),"metadata edit preserves payload endings");
        Check(Compare.PreserveLineEndings("a","a\n")=="a\n","add final newline");
        Check(Compare.PreserveLineEndings("a\n","a")=="a","remove final newline");
        string root=Path.Combine(Path.GetTempPath(),"pcg-test-"+Guid.NewGuid().ToString("N"));
        try {
            Directory.CreateDirectory(Path.Combine(root,"old"));Directory.CreateDirectory(Path.Combine(root,"new"));
            File.WriteAllText(Path.Combine(root,"old","same"),"same\n");File.WriteAllText(Path.Combine(root,"new","same"),"same\n");
            File.WriteAllText(Path.Combine(root,"old","removed"),"old");File.WriteAllText(Path.Combine(root,"new","added"),"new");
            File.WriteAllText(Path.Combine(root,"old","changed"),"old");File.WriteAllText(Path.Combine(root,"new","changed"),"new");
            File.WriteAllBytes(Path.Combine(root,"new","binary"),new byte[]{0,1,2});
            var scan=Compare.Scan(Path.Combine(root,"old"),Path.Combine(root,"new"),true,CancellationToken.None);
            Check(scan.Count==5,"folder union");Check(scan.Single(e=>e.Relative=="same").Status=="Identical","identical");
            Check(scan.Single(e=>e.Relative=="added").Status=="Added","added");Check(scan.Single(e=>e.Relative=="removed").Status=="Removed","removed");
            Check(scan.Single(e=>e.Relative=="changed").Status=="Modified","modified");Check(scan.Single(e=>e.Relative=="binary").Binary,"binary detection");
            var cancelled=new CancellationToken(true);bool threw=false;try{Compare.Scan(root,root,true,cancelled);}catch(OperationCanceledException){threw=true;}
            Check(threw,"cancellation");
        }finally{Directory.Delete(root,true);}
        Console.WriteLine(count+" checks passed.");
    }
}
