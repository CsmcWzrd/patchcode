using System;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
namespace PatchCodeGui
{
    public sealed class DiffView : UserControl
    {
        readonly DataGridView grid = new DataGridView();
        readonly List<DiffRow> rows;
        List<DiffRow> shown;
        bool merged;
        public DiffView(List<DiffRow> rows, bool merged)
        {
            this.rows = rows; Dock = DockStyle.Fill;
            grid.Dock = DockStyle.Fill; grid.ReadOnly = true; grid.VirtualMode = true;
            grid.AllowUserToAddRows = false; grid.AllowUserToDeleteRows = false;
            grid.RowHeadersVisible = false; grid.AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.None;
            grid.RowTemplate.Height = 23; grid.Font = new Font("Consolas", 10);
            grid.BackgroundColor = Color.White; grid.SelectionMode = DataGridViewSelectionMode.CellSelect;
            grid.CellValueNeeded += Value; grid.CellFormatting += Format;
            Controls.Add(grid); SetMode(merged);
        }
        public void SetMode(bool value)
        {
            merged = value; grid.RowCount = 0; grid.Columns.Clear(); shown = new List<DiffRow>();
            grid.Columns.Add("oldNo", "Old #"); grid.Columns.Add("newNo", "New #");
            if (merged)
            {
                grid.Columns.Add("mark", "+ / −"); grid.Columns.Add("text", "Merged view — additions Blue / deletions Red");
                foreach (var row in rows)
                {
                    if (!row.Changed) shown.Add(row);
                    else { if (row.OldNumber > 0) shown.Add(new DiffRow { Old = row.Old, OldNumber = row.OldNumber });
                           if (row.NewNumber > 0) shown.Add(new DiffRow { New = row.New, NewNumber = row.NewNumber }); }
                }
            }
            else { grid.Columns.Add("old", "Folder 1 / old code"); grid.Columns.Add("new", "Folder 2 / new code"); shown.AddRange(rows); }
            grid.Columns[0].Width = 65; grid.Columns[1].Width = 65;
            if (merged) { grid.Columns[2].Width = 48; grid.Columns[3].Width = 1000; }
            else { grid.Columns[2].Width = 620; grid.Columns[3].Width = 620; }
            grid.RowCount = shown.Count;
        }
        void Value(object sender, DataGridViewCellValueEventArgs e)
        {
            var r = shown[e.RowIndex];
            if (e.ColumnIndex == 0) e.Value = r.OldNumber == 0 ? "" : r.OldNumber.ToString();
            else if (e.ColumnIndex == 1) e.Value = r.NewNumber == 0 ? "" : r.NewNumber.ToString();
            else if (merged) e.Value = e.ColumnIndex == 2 ? (!r.Changed ? " " : r.OldNumber > 0 ? "−" : "+") : Compare.Display(r.New ?? r.Old);
            else e.Value = Compare.Display(e.ColumnIndex == 2 ? r.Old : r.New);
        }
        void Format(object sender, DataGridViewCellFormattingEventArgs e)
        {
            var r = shown[e.RowIndex]; Color back = Color.White, fore = Color.Black;
            if (r.Changed && e.ColumnIndex >= 2)
            {
                bool deletion = merged ? r.OldNumber > 0 : e.ColumnIndex == 2;
                bool gap = !merged && (e.ColumnIndex == 2 ? r.OldNumber == 0 : r.NewNumber == 0);
                back = gap ? Color.LightGray : deletion ? Color.Red : Color.Blue; fore = gap ? Color.Black : Color.White;
            }
            e.CellStyle.BackColor = back; e.CellStyle.ForeColor = fore;
            e.CellStyle.SelectionBackColor = Color.DarkSlateGray; e.CellStyle.SelectionForeColor = Color.White;
        }
        public void Navigate(int direction)
        {
            if (shown.Count == 0) return;
            int current = grid.CurrentCell == null ? -1 : grid.CurrentCell.RowIndex;
            int i = current;
            if(direction>0) { if(i<0)i=0; else { while(i<shown.Count && shown[i].Changed)i++; if(i==current)i++; } }
            else { i--; while(i>=0 && shown[i].Changed)i--; }
            while (i >= 0 && i < shown.Count)
            {
                if(shown[i].Changed) {
                    while(i>0 && shown[i-1].Changed)i--;
                    grid.CurrentCell=grid[2,i];grid.FirstDisplayedScrollingRowIndex=i;return;
                }
                i+=direction;
            }
        }
    }
}
