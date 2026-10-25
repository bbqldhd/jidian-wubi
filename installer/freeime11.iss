; FreeIME 11 —— Inno Setup 安装脚本（v11.0.1）
; 生成标准 Windows 安装包：向导界面 + 添加/删除程序注册 + 自动卸载 + TSF 自注册

[Setup]
AppId={{FreeIME11-11.0.1}
AppName=极点五笔 11
AppVersion=11.0.1
AppPublisher=FreeIME Project
AppPublisherURL=https://github.com/bbqldhd/jidian-wubi
AppSupportURL=https://gitee.com/red-egg-carrying-ice-cream/jidian-wubi
DefaultDirName={autopf}\FreeIME11
DefaultGroupName=极点五笔 11
DisableProgramGroupPage=yes
OutputDir=..\build
OutputBaseFilename=FreeIME11-11.0.1-setup
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=admin
CloseApplications=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\freeime11-settings.exe
UninstallDisplayName=极点五笔 11
WizardStyle=modern

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式(&D)"; GroupDescription: "附加任务:"

[Files]
; 程序文件 -> Program Files\FreeIME11
Source: "..\build\FreeIME11.dll"; DestDir: "{app}"; Flags: ignoreversion uninsrestartdelete
Source: "..\build\freeime11-migrate.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\freeime11-settings.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\freeime11-uninstall.exe"; DestDir: "{app}"; Flags: ignoreversion
; 词库 -> %APPDATA%\FreeIME11\data
Source: "..\..\upstream-freewb\data\mb\default\wbzx.mb"; DestDir: "{userappdata}\FreeIME11\data\mb\default"; Flags: ignoreversion
Source: "..\..\upstream-freewb\data\mb\default\pinyin.mb"; DestDir: "{userappdata}\FreeIME11\data\mb\default"; Flags: ignoreversion
Source: "..\..\upstream-freewb\data\dict\freeime_chinese_dict.db"; DestDir: "{userappdata}\FreeIME11\data"; Flags: ignoreversion

[Run]
; 注册 TSF 文本服务
Filename: "regsvr32.exe"; Parameters: "/s ""{app}\FreeIME11.dll"""; Flags: runhidden; StatusMsg: "注册输入法..."

[UninstallRun]
; 注销 TSF 文本服务
Filename: "regsvr32.exe"; Parameters: "/u /s ""{app}\FreeIME11.dll"""; Flags: runhidden; RunOnceId: "UnregTSF"

[Icons]
Name: "{group}\极点五笔 11 设置"; Filename: "{app}\freeime11-settings.exe"
Name: "{group}\迁移 10.8.9 数据"; Filename: "{app}\freeime11-migrate.exe"
Name: "{group}\卸载极点五笔 11"; Filename: "{app}\freeime11-uninstall.exe"
Name: "{commondesktop}\极点五笔 11 设置"; Filename: "{app}\freeime11-settings.exe"; Tasks: desktopicon

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
