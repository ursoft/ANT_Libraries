namespace ZwiftHandler
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.lbHosts = new System.Windows.Forms.ListBox();
            this.label1 = new System.Windows.Forms.Label();
            this.toolTip = new System.Windows.Forms.ToolTip(this.components);
            this.fswHosts = new System.IO.FileSystemWatcher();
            this.cmdPatch = new System.Windows.Forms.Button();
            this.cmdOriginal = new System.Windows.Forms.Button();
            this.txtZwifOfflineServerIp = new System.Windows.Forms.TextBox();
            this.txtLog = new System.Windows.Forms.TextBox();
            this.cmdStartZwift = new System.Windows.Forms.Button();
            this.chkPatchZwift = new System.Windows.Forms.CheckBox();
            this.cmdRefresh = new System.Windows.Forms.Button();
            this.txtSchwinnLog = new System.Windows.Forms.TextBox();
            this.txtEdgeRemoteId = new System.Windows.Forms.TextBox();
            this.label3 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.ctxMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.moveZwiftHereToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.storeZwiftPositionToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.recallZwiftPositionToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.suspendZwiftToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            this.zwiftPositionToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            ((System.ComponentModel.ISupportInitialize)(this.fswHosts)).BeginInit();
            this.ctxMenu.SuspendLayout();
            this.SuspendLayout();
            // 
            // lbHosts
            // 
            this.lbHosts.FormattingEnabled = true;
            this.lbHosts.ItemHeight = 16;
            this.lbHosts.Location = new System.Drawing.Point(16, 43);
            this.lbHosts.Margin = new System.Windows.Forms.Padding(4);
            this.lbHosts.Name = "lbHosts";
            this.lbHosts.Size = new System.Drawing.Size(174, 100);
            this.lbHosts.Sorted = true;
            this.lbHosts.TabIndex = 2;
            this.lbHosts.SelectedIndexChanged += new System.EventHandler(this.lbHosts_SelectedIndexChanged);
            this.lbHosts.DoubleClick += new System.EventHandler(this.lbHosts_DoubleClick);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(16, 13);
            this.label1.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(45, 16);
            this.label1.TabIndex = 1;
            this.label1.Text = "Hosts:";
            // 
            // fswHosts
            // 
            this.fswHosts.EnableRaisingEvents = true;
            this.fswHosts.SynchronizingObject = this;
            this.fswHosts.Changed += new System.IO.FileSystemEventHandler(this.fswHosts_Changed);
            this.fswHosts.Created += new System.IO.FileSystemEventHandler(this.fswHosts_Created);
            this.fswHosts.Deleted += new System.IO.FileSystemEventHandler(this.fswHosts_Deleted);
            this.fswHosts.Renamed += new System.IO.RenamedEventHandler(this.fswHosts_Renamed);
            // 
            // cmdPatch
            // 
            this.cmdPatch.Enabled = false;
            this.cmdPatch.Font = new System.Drawing.Font("Microsoft Sans Serif", 14.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.cmdPatch.Location = new System.Drawing.Point(197, 9);
            this.cmdPatch.Name = "cmdPatch";
            this.cmdPatch.Size = new System.Drawing.Size(184, 50);
            this.cmdPatch.TabIndex = 3;
            this.cmdPatch.Text = "Patch ANT_DLL";
            this.cmdPatch.UseVisualStyleBackColor = true;
            this.cmdPatch.Click += new System.EventHandler(this.cmdPatch_Click);
            // 
            // cmdOriginal
            // 
            this.cmdOriginal.Enabled = false;
            this.cmdOriginal.Font = new System.Drawing.Font("Microsoft Sans Serif", 15.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.cmdOriginal.Location = new System.Drawing.Point(388, 9);
            this.cmdOriginal.Name = "cmdOriginal";
            this.cmdOriginal.Size = new System.Drawing.Size(184, 50);
            this.cmdOriginal.TabIndex = 5;
            this.cmdOriginal.Text = "Revert to original";
            this.cmdOriginal.UseVisualStyleBackColor = true;
            this.cmdOriginal.Click += new System.EventHandler(this.cmdOriginal_Click);
            // 
            // txtZwifOfflineServerIp
            // 
            this.txtZwifOfflineServerIp.Location = new System.Drawing.Point(69, 10);
            this.txtZwifOfflineServerIp.Margin = new System.Windows.Forms.Padding(4);
            this.txtZwifOfflineServerIp.Name = "txtZwifOfflineServerIp";
            this.txtZwifOfflineServerIp.Size = new System.Drawing.Size(121, 22);
            this.txtZwifOfflineServerIp.TabIndex = 1;
            this.txtZwifOfflineServerIp.Text = global::ZwiftHandler.Properties.Settings.Default.ZwiftOfflineServerIp;
            this.txtZwifOfflineServerIp.Validating += new System.ComponentModel.CancelEventHandler(this.txtZwifOfflineServerIp_Validating);
            // 
            // txtLog
            // 
            this.txtLog.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtLog.Location = new System.Drawing.Point(16, 154);
            this.txtLog.Multiline = true;
            this.txtLog.Name = "txtLog";
            this.txtLog.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.txtLog.Size = new System.Drawing.Size(865, 295);
            this.txtLog.TabIndex = 10;
            // 
            // cmdStartZwift
            // 
            this.cmdStartZwift.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.cmdStartZwift.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(128)))), ((int)(((byte)(0)))));
            this.cmdStartZwift.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("cmdStartZwift.BackgroundImage")));
            this.cmdStartZwift.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.cmdStartZwift.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.cmdStartZwift.Location = new System.Drawing.Point(747, 9);
            this.cmdStartZwift.Name = "cmdStartZwift";
            this.cmdStartZwift.Size = new System.Drawing.Size(135, 135);
            this.cmdStartZwift.TabIndex = 7;
            this.cmdStartZwift.Text = "GO";
            this.cmdStartZwift.UseVisualStyleBackColor = false;
            this.cmdStartZwift.Click += new System.EventHandler(this.cmdStartZwift_Click);
            this.cmdStartZwift.MouseDown += new System.Windows.Forms.MouseEventHandler(this.cmdStartZwift_MouseDown);
            // 
            // chkPatchZwift
            // 
            this.chkPatchZwift.AutoSize = true;
            this.chkPatchZwift.Location = new System.Drawing.Point(229, 67);
            this.chkPatchZwift.Name = "chkPatchZwift";
            this.chkPatchZwift.Size = new System.Drawing.Size(139, 20);
            this.chkPatchZwift.TabIndex = 4;
            this.chkPatchZwift.Text = "Patch Zwift run-time";
            this.chkPatchZwift.UseVisualStyleBackColor = true;
            // 
            // cmdRefresh
            // 
            this.cmdRefresh.Font = new System.Drawing.Font("Microsoft Sans Serif", 15.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.cmdRefresh.Location = new System.Drawing.Point(578, 9);
            this.cmdRefresh.Name = "cmdRefresh";
            this.cmdRefresh.Size = new System.Drawing.Size(160, 50);
            this.cmdRefresh.TabIndex = 6;
            this.cmdRefresh.Text = "Refresh";
            this.cmdRefresh.UseVisualStyleBackColor = true;
            this.cmdRefresh.Click += new System.EventHandler(this.cmdRefresh_Click);
            // 
            // txtSchwinnLog
            // 
            this.txtSchwinnLog.Location = new System.Drawing.Point(198, 121);
            this.txtSchwinnLog.Name = "txtSchwinnLog";
            this.txtSchwinnLog.Size = new System.Drawing.Size(540, 22);
            this.txtSchwinnLog.TabIndex = 9;
            this.txtSchwinnLog.DoubleClick += new System.EventHandler(this.txtSchwinnLog_DoubleClick);
            // 
            // txtEdgeRemoteId
            // 
            this.txtEdgeRemoteId.Location = new System.Drawing.Point(634, 93);
            this.txtEdgeRemoteId.Name = "txtEdgeRemoteId";
            this.txtEdgeRemoteId.Size = new System.Drawing.Size(104, 22);
            this.txtEdgeRemoteId.TabIndex = 8;
            this.txtEdgeRemoteId.DoubleClick += new System.EventHandler(this.txtEdgeRemoteId_DoubleClick);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(631, 71);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(107, 16);
            this.label3.TabIndex = 12;
            this.label3.Text = "EdgeRemote ID:";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(197, 102);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(141, 16);
            this.label2.TabIndex = 13;
            this.label2.Text = "Schwinn power log file:";
            // 
            // ctxMenu
            // 
            this.ctxMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.moveZwiftHereToolStripMenuItem,
            this.storeZwiftPositionToolStripMenuItem,
            this.recallZwiftPositionToolStripMenuItem,
            this.toolStripSeparator1,
            this.suspendZwiftToolStripMenuItem});
            this.ctxMenu.Name = "ctxMenu";
            this.ctxMenu.Size = new System.Drawing.Size(182, 120);
            // 
            // moveZwiftHereToolStripMenuItem
            // 
            this.moveZwiftHereToolStripMenuItem.Name = "moveZwiftHereToolStripMenuItem";
            this.moveZwiftHereToolStripMenuItem.Size = new System.Drawing.Size(181, 22);
            this.moveZwiftHereToolStripMenuItem.Text = "Move Zwift here";
            this.moveZwiftHereToolStripMenuItem.Click += new System.EventHandler(this.moveZwiftHereToolStripMenuItem_Click);
            // 
            // storeZwiftPositionToolStripMenuItem
            // 
            this.storeZwiftPositionToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.zwiftPositionToolStripMenuItem});
            this.storeZwiftPositionToolStripMenuItem.Name = "storeZwiftPositionToolStripMenuItem";
            this.storeZwiftPositionToolStripMenuItem.Size = new System.Drawing.Size(181, 22);
            this.storeZwiftPositionToolStripMenuItem.Text = "Store";
            // 
            // recallZwiftPositionToolStripMenuItem
            // 
            this.recallZwiftPositionToolStripMenuItem.Name = "recallZwiftPositionToolStripMenuItem";
            this.recallZwiftPositionToolStripMenuItem.Size = new System.Drawing.Size(181, 22);
            this.recallZwiftPositionToolStripMenuItem.Text = "Recall Zwift Position";
            this.recallZwiftPositionToolStripMenuItem.Click += new System.EventHandler(this.recallZwiftPositionToolStripMenuItem_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(178, 6);
            // 
            // suspendZwiftToolStripMenuItem
            // 
            this.suspendZwiftToolStripMenuItem.Name = "suspendZwiftToolStripMenuItem";
            this.suspendZwiftToolStripMenuItem.Size = new System.Drawing.Size(181, 22);
            this.suspendZwiftToolStripMenuItem.Text = "Suspend Zwift";
            this.suspendZwiftToolStripMenuItem.Click += new System.EventHandler(this.suspendZwiftToolStripMenuItem_Click);
            // 
            // timer1
            // 
            this.timer1.Enabled = true;
            this.timer1.Interval = 1000;
            this.timer1.Tick += new System.EventHandler(this.timer1_Tick);
            // 
            // zwiftPositionToolStripMenuItem
            // 
            this.zwiftPositionToolStripMenuItem.Name = "zwiftPositionToolStripMenuItem";
            this.zwiftPositionToolStripMenuItem.Size = new System.Drawing.Size(180, 22);
            this.zwiftPositionToolStripMenuItem.Text = "Zwift position";
            this.zwiftPositionToolStripMenuItem.Click += new System.EventHandler(this.zwiftPositionToolStripMenuItem_Click);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(893, 461);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.txtEdgeRemoteId);
            this.Controls.Add(this.txtSchwinnLog);
            this.Controls.Add(this.cmdRefresh);
            this.Controls.Add(this.chkPatchZwift);
            this.Controls.Add(this.cmdStartZwift);
            this.Controls.Add(this.txtLog);
            this.Controls.Add(this.cmdOriginal);
            this.Controls.Add(this.cmdPatch);
            this.Controls.Add(this.txtZwifOfflineServerIp);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.lbHosts);
            this.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(4);
            this.MinimumSize = new System.Drawing.Size(909, 500);
            this.Name = "Form1";
            this.Text = "ZwiftHandler";
            this.Load += new System.EventHandler(this.Form1_Load);
            ((System.ComponentModel.ISupportInitialize)(this.fswHosts)).EndInit();
            this.ctxMenu.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ListBox lbHosts;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox txtZwifOfflineServerIp;
        private System.Windows.Forms.ToolTip toolTip;
        private System.IO.FileSystemWatcher fswHosts;
        private System.Windows.Forms.Button cmdPatch;
        private System.Windows.Forms.Button cmdOriginal;
        private System.Windows.Forms.TextBox txtLog;
        private System.Windows.Forms.Button cmdStartZwift;
        private System.Windows.Forms.CheckBox chkPatchZwift;
        private System.Windows.Forms.Button cmdRefresh;
        private System.Windows.Forms.TextBox txtSchwinnLog;
        private System.Windows.Forms.TextBox txtEdgeRemoteId;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.ContextMenuStrip ctxMenu;
        private System.Windows.Forms.ToolStripMenuItem moveZwiftHereToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem storeZwiftPositionToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem recallZwiftPositionToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripMenuItem suspendZwiftToolStripMenuItem;
        private System.Windows.Forms.Timer timer1;
        private System.Windows.Forms.ToolStripMenuItem zwiftPositionToolStripMenuItem;
    }
}

