﻿//------------------------------------------------------------------------------
// <auto-generated>
//     This code was generated by a tool.
//     Runtime Version:4.0.30319.42000
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
// </auto-generated>
//------------------------------------------------------------------------------

namespace ZwiftHandler.Properties {
    
    
    [global::System.Runtime.CompilerServices.CompilerGeneratedAttribute()]
    [global::System.CodeDom.Compiler.GeneratedCodeAttribute("Microsoft.VisualStudio.Editors.SettingsDesigner.SettingsSingleFileGenerator", "17.0.3.0")]
    internal sealed partial class Settings : global::System.Configuration.ApplicationSettingsBase {
        
        private static Settings defaultInstance = ((Settings)(global::System.Configuration.ApplicationSettingsBase.Synchronized(new Settings())));
        
        public static Settings Default {
            get {
                return defaultInstance;
            }
        }
        
        [global::System.Configuration.UserScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("192.168.1.40")]
        public string ZwiftOfflineServerIp {
            get {
                return ((string)(this["ZwiftOfflineServerIp"]));
            }
            set {
                this["ZwiftOfflineServerIp"] = value;
            }
        }
        
        [global::System.Configuration.UserScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("z:\\Zwift\\ZG-Win\\")]
        public string PatchSharedPath {
            get {
                return ((string)(this["PatchSharedPath"]));
            }
            set {
                this["PatchSharedPath"] = value;
            }
        }
        
        [global::System.Configuration.UserScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("C:\\Users\\Youra\\Downloads\\ZWiftOffline\\start.bat")]
        public string ZofflineBatch {
            get {
                return ((string)(this["ZofflineBatch"]));
            }
            set {
                this["ZofflineBatch"] = value;
            }
        }
        
        [global::System.Configuration.UserScopedSettingAttribute()]
        [global::System.Diagnostics.DebuggerNonUserCodeAttribute()]
        [global::System.Configuration.DefaultSettingValueAttribute("0, 0, 0, 0")]
        public global::System.Drawing.Rectangle ZwiftWindow {
            get {
                return ((global::System.Drawing.Rectangle)(this["ZwiftWindow"]));
            }
            set {
                this["ZwiftWindow"] = value;
            }
        }
    }
}
