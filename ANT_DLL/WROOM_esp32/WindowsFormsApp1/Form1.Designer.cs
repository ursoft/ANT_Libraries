namespace WindowsFormsApp1
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
            this.listBoxLog = new System.Windows.Forms.ListBox();
            this.power = new System.Windows.Forms.Label();
            this.cadence = new System.Windows.Forms.Label();
            this.heart = new System.Windows.Forms.Label();
            this.resist = new System.Windows.Forms.NumericUpDown();
            this.Res = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.resist)).BeginInit();
            this.SuspendLayout();
            // 
            // listBoxLog
            // 
            this.listBoxLog.FormattingEnabled = true;
            this.listBoxLog.ItemHeight = 24;
            this.listBoxLog.Location = new System.Drawing.Point(24, 24);
            this.listBoxLog.Margin = new System.Windows.Forms.Padding(6);
            this.listBoxLog.Name = "listBoxLog";
            this.listBoxLog.Size = new System.Drawing.Size(857, 196);
            this.listBoxLog.TabIndex = 2;
            // 
            // power
            // 
            this.power.Font = new System.Drawing.Font("Arial Narrow", 96F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.power.Location = new System.Drawing.Point(40, 247);
            this.power.Name = "power";
            this.power.Size = new System.Drawing.Size(1402, 317);
            this.power.TabIndex = 2;
            this.power.Text = "power";
            this.power.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // cadence
            // 
            this.cadence.Font = new System.Drawing.Font("Arial Narrow", 96F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.cadence.Location = new System.Drawing.Point(40, 564);
            this.cadence.Name = "cadence";
            this.cadence.Size = new System.Drawing.Size(1402, 317);
            this.cadence.TabIndex = 3;
            this.cadence.Text = "cadence";
            this.cadence.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // heart
            // 
            this.heart.Font = new System.Drawing.Font("Arial Narrow", 96F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.heart.Location = new System.Drawing.Point(40, 881);
            this.heart.Name = "heart";
            this.heart.Size = new System.Drawing.Size(1402, 317);
            this.heart.TabIndex = 4;
            this.heart.Text = "heart rate";
            this.heart.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // resist
            // 
            this.resist.Font = new System.Drawing.Font("Microsoft Sans Serif", 72F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.resist.Location = new System.Drawing.Point(1143, 24);
            this.resist.Maximum = new decimal(new int[] {
            25,
            0,
            0,
            0});
            this.resist.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            0});
            this.resist.Name = "resist";
            this.resist.Size = new System.Drawing.Size(299, 198);
            this.resist.TabIndex = 0;
            this.resist.Value = new decimal(new int[] {
            4,
            0,
            0,
            0});
            this.resist.ValueChanged += new System.EventHandler(this.resist_ValueChanged);
            // 
            // Res
            // 
            this.Res.AutoSize = true;
            this.Res.Font = new System.Drawing.Font("Microsoft Sans Serif", 72F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(204)));
            this.Res.Location = new System.Drawing.Point(919, 26);
            this.Res.Name = "Res";
            this.Res.Size = new System.Drawing.Size(201, 190);
            this.Res.TabIndex = 6;
            this.Res.Text = "R";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(11F, 24F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1467, 1187);
            this.Controls.Add(this.Res);
            this.Controls.Add(this.resist);
            this.Controls.Add(this.heart);
            this.Controls.Add(this.cadence);
            this.Controls.Add(this.power);
            this.Controls.Add(this.listBoxLog);
            this.Margin = new System.Windows.Forms.Padding(6);
            this.Name = "Form1";
            this.Text = "FTMS control";
            this.Load += new System.EventHandler(this.Form1_Load);
            ((System.ComponentModel.ISupportInitialize)(this.resist)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ListBox listBoxLog;
        private System.Windows.Forms.Label power;
        private System.Windows.Forms.Label cadence;
        private System.Windows.Forms.Label heart;
        private System.Windows.Forms.NumericUpDown resist;
        private System.Windows.Forms.Label Res;
    }
}

