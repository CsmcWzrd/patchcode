using System;
using System.Drawing;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using PatchCodeGui;
class GuiSmokeTests
{
    static object Field(MainForm form,string name) { return typeof(MainForm).GetField(name,BindingFlags.Instance|BindingFlags.NonPublic).GetValue(form); }
    static void Check(bool condition,string message) { if(!condition)throw new Exception(message); Console.WriteLine("PASS "+message); }
    [STAThread] static void Main()
    {
        Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
        using(var f=new MainForm())
        {
            f.Show(); Application.DoEvents();
            Check(((TabControl)Field(f,"navigation")).TabCount==3,"three navigation tabs");
            var rows=Compare.Align(Compare.Lines("a\nb\nc\n"),Compare.Lines("a\nB\nc\n"),CancellationToken.None);
            var page=new TabPage("sample.txt"); var view=new DiffView(rows,false);page.Controls.Add(view);
            var docs=(TabControl)Field(f,"documents");docs.TabPages.Add(page);docs.SelectedTab=page;
            Application.DoEvents();var grid=(DataGridView)view.Controls[0];
            Check(grid.RowCount==3&&grid.ColumnCount==4,"aligned comparison grid");
            view.SetMode(true);Application.DoEvents();
            Check(grid.RowCount==4&&grid.Columns[3].HeaderText.Contains("Merged"),"merged view");
            Check(grid.Rows[1].Cells[3].InheritedStyle.BackColor!=Color.Empty,"grid styles available");
            using(var bitmap=new Bitmap(f.Width,f.Height)) { f.DrawToBitmap(bitmap,new Rectangle(Point.Empty,f.Size)); }
            Check(f.Visible,"window renders");
            f.Close();
        }
    }
}
