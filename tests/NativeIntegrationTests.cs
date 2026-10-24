using System;
using System.IO;
using System.Threading;
using PatchCodeGui;
class NativeIntegrationTests
{
    static void Check(bool value,string message) { if(!value)throw new Exception(message); Console.WriteLine("PASS "+message); }
    static void Main()
    {
        string root=Path.Combine(Path.GetTempPath(),"pcg native "+Guid.NewGuid().ToString("N")); Directory.CreateDirectory(root);
        string a=Path.Combine(root,"old.txt"),b=Path.Combine(root,"new.txt");
        try {
            File.WriteAllText(a,"a\nb\nc\n");File.WriteAllText(b,"a\nB\nc\n");
            string result=NativeTool.Generate(a,b,CancellationToken.None).GetAwaiter().GetResult();
            string patch=result.Substring(result.IndexOf('\0')+1);
            Check(patch.Contains("<@+@+@>"),"native format preserved");
            Check(patch.Contains("filename:"+a.Replace('\\','/'))||patch.Contains("filename:"+a),"native target metadata");
            File.Copy(a,b,true);result=NativeTool.Generate(a,b,CancellationToken.None).GetAwaiter().GetResult();
            Check(result.EndsWith("\0"),"identical inputs produce no stale patch");
            File.WriteAllBytes(b,new byte[]{0,1,2});result=NativeTool.Generate(a,b,CancellationToken.None).GetAwaiter().GetResult();
            Check(result.Contains("binary")&&result.EndsWith("\0"),"binary report without patch");
        } finally { Directory.Delete(root,true); }
    }
}
