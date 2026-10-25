; FreeIME 11 —— Inno Setup 安装脚本（v11.0.2）
; 生成标准 Windows 安装包：向导界面 + 添加/删除程序注册 + 自动卸载 + TSF 自注册

[Setup]
AppId={{FreeIME11-11.0.1}
AppName=极点五笔 11
AppVersion=11.0.4
AppPublisher=FreeIME Project
AppPublisherURL=https://github.com/bbqldhd/jidian-wubi
AppSupportURL=https://gitee.com/red-egg-carrying-ice-cream/jidian-wubi
DefaultDirName={autopf}\FreeIME11
DefaultGroupName=极点五笔 11
DisableProgramGroupPage=yes
OutputDir=..\build
OutputBaseFilename=FreeIME11-11.0.4-setup
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
; TSF DLL 常被输入法宿主（explorer/ctfmon/TextInputHost）加载，
; 升级时无法直接覆盖，用 restartreplace 安排重启后替换，否则会静默保留旧版。
Source: "..\build\FreeIME11.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "..\build\freeime11-migrate.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\freeime11-settings.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\freeime11-uninstall.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\freeime11-addlang.exe"; DestDir: "{app}"; Flags: ignoreversion
; 词库 -> 先随程序安装，再由 addlang 按用户复制到 %APPDATA%（多用户各自可用）
Source: "..\..\upstream-freewb\data\mb\default\wbzx.mb"; DestDir: "{app}\data\mb\default"; Flags: ignoreversion
Source: "..\..\upstream-freewb\data\mb\default\pinyin.mb"; DestDir: "{app}\data\mb\default"; Flags: ignoreversion
Source: "..\..\upstream-freewb\data\dict\freeime_chinese_dict.db"; DestDir: "{app}\data"; Flags: ignoreversion

[Run]
; 注册 TSF 文本服务
Filename: "regsvr32.exe"; Parameters: "/s ""{app}\FreeIME11.dll"""; Flags: runhidden; StatusMsg: "注册输入法..."
; 将输入法加入当前用户语言列表（以原用户身份，确保写入正确的 HKCU）。
; v11.0.3 起会自动写入 REG_DWORD 语言列表项，装完即可用，不再需要引导用户手动添加。
Filename: "{app}\freeime11-addlang.exe"; Flags: runhidden runasoriginaluser; StatusMsg: "启用输入法..."

[UninstallRun]
; 注销 TSF 文本服务
Filename: "regsvr32.exe"; Parameters: "/u /s ""{app}\FreeIME11.dll"""; Flags: runhidden; RunOnceId: "UnregTSF"

[Icons]
Name: "{group}\极点五笔 11 设置"; Filename: "{app}\freeime11-settings.exe"
Name: "{group}\迁移 10.8.9 数据"; Filename: "{app}\freeime11-migrate.exe"
Name: "{group}\卸载极点五笔 11"; Filename: "{app}\freeime11-uninstall.exe"
Name: "{commondesktop}\极点五笔 11 设置"; Filename: "{app}\freeime11-settings.exe"; Tasks: desktopicon

[Code]
// 卸载时若 DLL 被输入法宿主占用，Inno 会登记「重启后删除」。
// 这条记录会让下一次安装直接报「请先重启」，用户升级时极易卡住。
// 这里在安装开始前只剔除指向 FreeIME11 的条目，其它软件的待删项原样保留。
function PopToken(var Rest: String): String;
var
  P: Integer;
begin
  if Rest = '' then
  begin
    Result := '';
    Exit;
  end;
  P := Pos(#0, Rest);
  if P = 0 then
  begin
    Result := Rest;
    Rest := '';
  end
  else
  begin
    Result := Copy(Rest, 1, P - 1);
    Rest := Copy(Rest, P + 1, Length(Rest) - P);
  end;
end;

procedure ClearOurPendingRename();
var
  Key, All, Rest, Keep, A, B: String;
  Changed: Boolean;
begin
  Key := 'SYSTEM\CurrentControlSet\Control\Session Manager';
  if not RegQueryMultiStringValue(HKLM, Key, 'PendingFileRenameOperations', All) then
    Exit;

  Rest := All;
  Keep := '';
  Changed := False;
  while Rest <> '' do
  begin
    A := PopToken(Rest);
    B := PopToken(Rest);
    // 只剔除「指向本程序目录的纯删除项」（目标为空）。
    // 目标非空的是改名/替换（restartreplace），必须保留，否则升级后仍是旧文件。
    if (Pos('freeime11', Lowercase(A)) = 0) or (B <> '') then
      Keep := Keep + A + #0 + B + #0
    else
      Changed := True;
  end;

  if not Changed then
    Exit;

  // Keep 中每个条目对已自带结尾分隔符（源#0目标#0），不要裁掉，
  // 否则最后一对的空目标会丢失，导致整个列表少一项。
  if Keep = '' then
    RegDeleteValue(HKLM, Key, 'PendingFileRenameOperations')
  else
    RegWriteMultiStringValue(HKLM, Key, 'PendingFileRenameOperations', Keep);
end;

function InitializeSetup(): Boolean;
begin
  ClearOurPendingRename();
  Result := True;
end;
