[Setup]
; Stable AppId so every future version upgrades in place. Must stay
; "EdgeClock" — older installers omitted AppId, which defaults to AppName,
; so this keeps upgrades replacing existing installs instead of duplicating.
AppId=EdgeClock
AppName=EdgeClock
AppVersion=1.5.0.0
; Detect the app's single-instance mutex: installer asks the user to close
; a running EdgeClock instead of failing on a locked exe
AppMutex=EdgeClock_GlobalInstance_Mutex
; Auto close/restart the running app during upgrade where possible
CloseApplications=yes
RestartApplications=no
DefaultDirName={autopf}\EdgeClock
DefaultGroupName=EdgeClock
OutputDir=Output
OutputBaseFilename=EdgeClockSetup_v1.5
Compression=lzma2
SolidCompression=yes
SetupIconFile=clock_23989.ico
UninstallDisplayIcon={app}\EdgeClock.exe
; Require admin privileges for Program Files
PrivilegesRequired=admin
; HKCU Run key is intentional: it targets the installing user, and the app
; itself has a "Run on Startup" toggle to fix it for any other user.
UsedUserAreasWarning=no
ArchitecturesInstallIn64BitMode=x64
DisableDirPage=no

[Files]
Source: "EdgeClock.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "clock_23989.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\EdgeClock"; Filename: "{app}\EdgeClock.exe"
Name: "{group}\Uninstall EdgeClock"; Filename: "{uninstallexe}"
Name: "{commondesktop}\EdgeClock"; Filename: "{app}\EdgeClock.exe"; Tasks: desktopicon
; We don't explicitly need to create a Startup shortcut here since EdgeClock has a "Run on Startup" toggle in its own menu

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "startup"; Description: "Run on startup"; GroupDescription: "Additional tasks:"; Flags: checkedonce

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "EdgeClock"; ValueData: """{app}\EdgeClock.exe"""; Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "{app}\EdgeClock.exe"; Description: "Launch EdgeClock"; Flags: nowait postinstall skipifsilent runasoriginaluser
