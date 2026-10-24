using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
namespace PatchCodeGui
{
    sealed class EditorState { public string Path, Original; public bool Dirty; }
    public sealed class MainForm : Form
    {
        readonly TabControl navigation = new TabControl(), documents = new TabControl();
        readonly TreeView oldTree = new TreeView(), newTree = new TreeView(), diffTree = new TreeView();
        readonly ToolStripStatusLabel status = new ToolStripStatusLabel("Choose two files or two folders to compare.");
        readonly MenuStrip menu = new MenuStrip();
        readonly ToolStrip tools = new ToolStrip();
        List<Entry> entries = new List<Entry>(); string oldRoot, newRoot; bool folders, merged, busy;
        CancellationTokenSource cancellation;
        const string Template = "filename:\r\nauthor:\r\npatchname:\r\ntimestamp:\r\nenable:\r\nbugs:\r\nextra:\r\n<@+@+@>\r\n<@+@+@>\r\n<@+@+@>\r\n<@+@+@>\r\n";
        public MainForm()
        {
            Text = "PatchCode-GuiClient-Windows"; Width = 1450; Height = 900; MinimumSize = new Size(900,600);
            StartPosition = FormStartPosition.CenterScreen; AutoScaleMode = AutoScaleMode.Dpi;
            var file = new ToolStripMenuItem("&File"); menu.Items.Add(file);
            Add(file, "Compare two &files…", async () => await Choose(false), Keys.Control|Keys.O);
            Add(file, "Compare two f&olders…", async () => await Choose(true), Keys.Control|Keys.Shift|Keys.O);
            Add(file, "&Refresh comparison", async () => await RefreshComparison(), Keys.F5);
            file.DropDownItems.Add(new ToolStripSeparator());
            Add(file, "&Generate native patch…", async () => await Generate(), Keys.Control|Keys.G);
            Add(file, "&Open patch / metadata…", OpenPatch);
            Add(file, "&New empty patch template", () => Editor("Untitled patch", Template, null, true));
            Add(file, "&Save patch", () => Save(false), Keys.Control|Keys.S);
            Add(file, "Save patch &as…", () => Save(true), Keys.Control|Keys.Shift|Keys.S);
            Add(file, "&Close tab", CloseTab, Keys.Control|Keys.W);
            Add(file, "E&xit", Close);
            var edit = new ToolStripMenuItem("&Edit"); menu.Items.Add(edit);
            Add(edit, "&Undo", () => WithEditor(t => t.Undo()), Keys.Control|Keys.Z);
            Add(edit, "Cu&t", () => WithEditor(t => t.Cut()), Keys.Control|Keys.X);
            Add(edit, "&Copy", () => WithEditor(t => t.Copy()), Keys.Control|Keys.C);
            Add(edit, "&Paste", () => WithEditor(t => t.Paste()), Keys.Control|Keys.V);
            Add(edit, "Select &all", () => WithEditor(t => t.SelectAll()), Keys.Control|Keys.A);
            var view = new ToolStripMenuItem("&View"); menu.Items.Add(view);
            var lr = new ToolStripMenuItem("Left right comparison") { Checked = true };
            var mv = new ToolStripMenuItem("Merged view");
            lr.Click += (s,e) => { lr.Checked=true; mv.Checked=false; Mode(false); };
            mv.Click += (s,e) => { lr.Checked=false; mv.Checked=true; Mode(true); };
            view.DropDownItems.Add(lr); view.DropDownItems.Add(mv);
            Add(view, "Next difference", () => Navigate(1), Keys.F7);
            Add(view, "Previous difference", () => Navigate(-1), Keys.Shift|Keys.F7);
            Add(view, "Tool log", () => ShowText("Tool log", lastLog));
            var help = new ToolStripMenuItem("&Help"); menu.Items.Add(help);
            Add(help, "About / format limits", () => MessageBox.Show(this,
                "PatchCode-GuiClient-Windows\nTwo-way comparison and native PatchCode export.\n\n" +
                "The native <@+@+@> format is unchanged. Edit every metadata field directly in a patch text tab.\n" +
                "Binary/UTF-16 files are reported only. Deleted files are emptied by PatchCode, not removed.\n" +
                "Literal separator lines cannot be exported. Native filenames must be ASCII.\n" +
                "File previews use UTF-8, falling back to the Windows code page; [LF]/[CRLF] labels show line endings.\n" +
                "Preview limit: 8 MiB/file; large changes use coarse alignment.\nSee docs/REVIEW.md and README.md.", "About"));
            tools.Items.Add("Files…", null, async (s,e) => await Choose(false));
            tools.Items.Add("Folders…", null, async (s,e) => await Choose(true));
            tools.Items.Add("Refresh", null, async (s,e) => await RefreshComparison());
            tools.Items.Add("Generate patch", null, async (s,e) => await Generate());
            tools.Items.Add("Cancel", null, (s,e) => { if (cancellation != null) cancellation.Cancel(); });
            tools.Items.Add("Close tab", null, (s,e) => CloseTab());
            var split = new SplitContainer { Dock=DockStyle.Fill, Width=1400, SplitterDistance=300, Panel1MinSize=220, Panel2MinSize=400 };
            navigation.Dock=DockStyle.Fill; documents.Dock=DockStyle.Fill;
            AddTree("Folder 1 (old code)",oldTree); AddTree("Folder 2 (new code)",newTree); AddTree("Diff",diffTree);
            split.Panel1.Controls.Add(navigation); split.Panel2.Controls.Add(documents);
            var strip = new StatusStrip(); strip.Items.Add(status);
            Controls.Add(split); Controls.Add(tools); Controls.Add(menu); Controls.Add(strip);
            tools.Dock=DockStyle.Top; menu.Dock=DockStyle.Top; MainMenuStrip=menu;
            oldTree.AfterSelect += async (s,e) => await OpenEntry(e.Node.Tag as Entry, false, true);
            newTree.AfterSelect += async (s,e) => await OpenEntry(e.Node.Tag as Entry, false, false);
            diffTree.AfterSelect += async (s,e) => await OpenEntry(e.Node.Tag as Entry, true, false);
            FormClosing += ClosingForm;
        }
        string lastLog = "No native export has run.";
        static void Add(ToolStripMenuItem parent, string title, Action action, Keys shortcut = Keys.None)
        { var item = new ToolStripMenuItem(title) { ShortcutKeys=shortcut }; item.Click += (s,e) => action(); parent.DropDownItems.Add(item); }
        void AddTree(string title, TreeView tree)
        { tree.Dock=DockStyle.Fill; tree.HideSelection=false; var p=new TabPage(title); p.Controls.Add(tree); navigation.TabPages.Add(p); }
        async Task Run(Func<CancellationToken,Task> action)
        {
            if (busy) return; busy=true; cancellation=new CancellationTokenSource();
            status.Text="Working… Use Cancel to stop.";
            try { await action(cancellation.Token); }
            catch (OperationCanceledException) { status.Text="Cancelled."; }
            catch (Exception ex) { status.Text="Operation failed."; MessageBox.Show(this,ex.Message,"PatchCode",MessageBoxButtons.OK,MessageBoxIcon.Error); }
            finally { cancellation.Dispose(); cancellation=null; busy=false; }
        }
        async Task Choose(bool folderMode)
        {
            if (busy) return; string a, b;
            if (folderMode)
            {
                using (var d = new FolderBrowserDialog { Description="Select Folder 1 (old code)" }) { if(d.ShowDialog(this)!=DialogResult.OK)return; a=d.SelectedPath; }
                using (var d = new FolderBrowserDialog { Description="Select Folder 2 (new code)" }) { if(d.ShowDialog(this)!=DialogResult.OK)return; b=d.SelectedPath; }
            }
            else
            {
                using (var d = new OpenFileDialog { Title="Select old file", CheckFileExists=true }) { if(d.ShowDialog(this)!=DialogResult.OK)return; a=d.FileName; }
                using (var d = new OpenFileDialog { Title="Select new file", CheckFileExists=true }) { if(d.ShowDialog(this)!=DialogResult.OK)return; b=d.FileName; }
            }
            if (!CloseAll()) return;
            oldRoot=a; newRoot=b; folders=folderMode; await RefreshComparison();
        }
        async Task RefreshComparison()
        {
            if (oldRoot==null || busy) return;
            if (!CloseAll()) return;
            await Run(async token => {
                var scanned=await Task.Run(() => Compare.Scan(oldRoot,newRoot,folders,token),token);
                entries=scanned; Populate(oldTree,entries.Where(x=>x.OldPath!=null)); Populate(newTree,entries.Where(x=>x.NewPath!=null));
                Populate(diffTree,entries.Where(x=>x.Status!="Identical")); navigation.SelectedIndex=2;
                status.Text=entries.Count+" files; "+entries.Count(x=>x.Status!="Identical")+" differences. Old: "+oldRoot+" | New: "+newRoot;
            });
        }
        void Populate(TreeView tree, IEnumerable<Entry> items)
        {
            tree.BeginUpdate(); tree.Nodes.Clear();
            foreach(var entry in items)
            {
                var collection=tree.Nodes; string[] parts=entry.Relative.Split('\\','/'); string key="";
                for(int i=0;i<parts.Length;i++)
                {
                    key += "/"+parts[i]; TreeNode node=collection[key];
                    if(node==null) { node=new TreeNode(parts[i]) { Name=key }; collection.Add(node); }
                    if(i==parts.Length-1) { node.Tag=entry; node.Text=parts[i]+" ["+entry.Status+(entry.Binary?", binary":"")+"]"; }
                    collection=node.Nodes;
                }
            }
            tree.EndUpdate();
        }
        async Task OpenEntry(Entry entry, bool diff, bool old)
        {
            if(entry==null || busy)return;
            string key=(diff?"diff:":old?"old:":"new:")+entry.Relative;
            foreach(TabPage tab in documents.TabPages)if(tab.Name==key){ documents.SelectedTab=tab;return; }
            await Run(async token=> {
                if(entry.Binary) { ShowText(entry.Relative,"Binary or unsupported text encoding.\r\nOld: "+(entry.OldPath??"<missing>")+"\r\nNew: "+(entry.NewPath??"<missing>")+"\r\nStatus: "+entry.Status+"\r\nNative PatchCode does not generate binary patches.",key);return; }
                if(!diff) { string text=await Task.Run(()=>Compare.Read(old?entry.OldPath:entry.NewPath),token); ShowText(entry.Relative,text,key);return; }
                var rows=await Task.Run(()=>Compare.Align(Compare.Lines(Compare.Read(entry.OldPath)),Compare.Lines(Compare.Read(entry.NewPath)),token),token);
                var page=new TabPage(entry.Relative+" ["+entry.Status+"]") { Name=key, ToolTipText=(entry.OldPath??"<missing>")+" → "+(entry.NewPath??"<missing>") };
                page.Controls.Add(new DiffView(rows,merged)); documents.TabPages.Add(page); documents.SelectedTab=page;
                status.Text="Red: deletions | Blue: additions | "+entry.Relative;
            });
        }
        void ShowText(string title,string text,string key="")
        { var p=new TabPage(title) { Name=key }; p.Controls.Add(TextBox(text,true)); documents.TabPages.Add(p);documents.SelectedTab=p; }
        static RichTextBox TextBox(string text,bool readOnly)
        { return new RichTextBox { Dock=DockStyle.Fill,Font=new Font("Consolas",11),Text=text,ReadOnly=readOnly,WordWrap=false,DetectUrls=false,AcceptsTab=true,HideSelection=false }; }
        void Editor(string title,string text,string path,bool dirty)
        {
            var p=new TabPage(title); var state=new EditorState { Path=path,Original=text,Dirty=dirty }; p.Tag=state;
            var box=TextBox(text,false); box.TextChanged+=(s,e)=>{state.Dirty=true;p.Text=title+" *";}; p.Controls.Add(box);
            documents.TabPages.Add(p);documents.SelectedTab=p;
            status.Text="Edit all metadata and patch sections as plain text. Save writes UTF-8 without BOM. Blank template needs values before use.";
        }
        void OpenPatch()
        {
            using(var d=new OpenFileDialog { Filter="Patch files|*.patch|All files|*.*" }) if(d.ShowDialog(this)==DialogResult.OK)
            { try { if(new FileInfo(d.FileName).Length>Compare.MaxPreviewBytes)throw new IOException("Patch editor limit is 8 MiB."); string text=new UTF8Encoding(false,true).GetString(File.ReadAllBytes(d.FileName)); Editor(Path.GetFileName(d.FileName),text,d.FileName,false); }
              catch(Exception ex){MessageBox.Show(this,"Cannot open patch as UTF-8: "+ex.Message);} }
        }
        bool Save(bool saveAs)
        {
            var p=documents.SelectedTab; if(p==null || !(p.Tag is EditorState))return false;
            var state=(EditorState)p.Tag; string path=state.Path;
            if(saveAs||path==null)using(var d=new SaveFileDialog { Filter="Patch files|*.patch|All files|*.*",DefaultExt="patch",FileName=path==null?"changes.patch":Path.GetFileName(path) })
            {if(d.ShowDialog(this)!=DialogResult.OK)return false;path=d.FileName;}
            try { string content=state.Dirty?Compare.PreserveLineEndings(state.Original,((RichTextBox)p.Controls[0]).Text):state.Original; File.WriteAllText(path,content,new UTF8Encoding(false));state.Original=content;state.Path=path;state.Dirty=false;p.Text=Path.GetFileName(path);status.Text="Saved "+path;return true; }
            catch(Exception ex){MessageBox.Show(this,ex.Message,"Save failed");return false;}
        }
        async Task Generate()
        {
            if(oldRoot==null || busy)return;
            await Run(async token=> {
                string result=await NativeTool.Generate(oldRoot,newRoot,token); int index=result.IndexOf('\0');
                lastLog=result.Substring(0,index);string patch=result.Substring(index+1);
                if(patch.Length==0){ShowText("Tool log",lastLog+"\r\nNo representable text patch was generated.");return;}
                Editor("Generated patch",patch,null,true);status.Text="Native patch generated. Review metadata, then Save As. See View → Tool log for warnings.";
                if(lastLog.IndexOf("Warning:",StringComparison.OrdinalIgnoreCase)>=0)ShowText("Export warnings",lastLog);
            });
        }
        void WithEditor(Action<RichTextBox> action) { var p=documents.SelectedTab;if(p!=null&&p.Controls.Count>0&&p.Controls[0] is RichTextBox)action((RichTextBox)p.Controls[0]); }
        void Mode(bool mode){merged=mode;foreach(TabPage p in documents.TabPages)foreach(Control c in p.Controls)if(c is DiffView)((DiffView)c).SetMode(mode);}
        void Navigate(int direction){var p=documents.SelectedTab;if(p!=null&&p.Controls[0] is DiffView)((DiffView)p.Controls[0]).Navigate(direction);}
        bool CanClose(TabPage page)
        {
            var s=page.Tag as EditorState;if(s==null||!s.Dirty)return true;documents.SelectedTab=page;
            var answer=MessageBox.Show(this,"Save changes to "+page.Text+"?","Unsaved patch",MessageBoxButtons.YesNoCancel);
            return answer==DialogResult.No||(answer==DialogResult.Yes&&Save(false));
        }
        void CloseTab(){if(documents.SelectedTab!=null&&CanClose(documents.SelectedTab)){var p=documents.SelectedTab;documents.TabPages.Remove(p);p.Dispose();}}
        bool CloseAll(){foreach(TabPage p in documents.TabPages)if(!CanClose(p))return false;while(documents.TabPages.Count>0){var p=documents.TabPages[0];documents.TabPages.Remove(p);p.Dispose();}return true;}
        void ClosingForm(object sender,FormClosingEventArgs e){if(busy){cancellation.Cancel();e.Cancel=true;status.Text="Cancelling… Close again when the operation stops.";}else if(!CloseAll())e.Cancel=true;}
    }
}
