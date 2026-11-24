[Setup]
; Stable AppId so every future version upgrades in place. Must stay
; "EdgeClock" — older installers omitted AppId, which defaults to AppName,
; so this keeps upgrades replacing existing installs instead of duplicating.
AppId=EdgeClock
AppName=EdgeClock
AppVersion=1.8.0.0
; No AppMutex: it is checked before the wizard even opens and only offers
; "close it yourself, then OK" (a silent install just aborts). Instead the
; [Code] section closes a running clock once the user commits to installing
; (PrepareToInstall) and starts it again afterwards ([Run], ClockWasRunning).
; Restart Manager stays on as the net for a clock in another user session.
CloseApplications=yes
RestartApplications=no
DefaultDirName={autopf}\EdgeClock
DefaultGroupName=EdgeClock
OutputDir=Output
OutputBaseFilename=EdgeClockSetup_v1.8.0
Compression=lzma2
SolidCompression=yes
SetupIconFile=clock_23989.ico
UninstallDisplayIcon={app}\EdgeClock.exe
; Require admin privileges for Program Files
PrivilegesRequired=admin
; HKCU Run key is intentional: it targets the installing user, and the app
; itself has a "Run on Startup" toggle to fix it for any other user.
UsedUserAreasWarning=no
; "x64" is deprecated (warns in Inno Setup 6.7, rejected in 7). "x64compatible"
; is the right replacement rather than "x64os": it also covers ARM64 running the
; x64 build under emulation, which "x64os" would exclude.
ArchitecturesInstallIn64BitMode=x64compatible
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
; The app's own "Run on Startup" toggle writes the same value, and the line
; above only cleans up after itself when its task was ticked at install. This
; entry writes nothing (ValueType: none) and always removes the value on
; uninstall, so no Run entry is left pointing at a deleted exe.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "EdgeClock"; Flags: uninsdeletevalue

[Run]
; Was running before the upgrade: bring it back, silent installs included.
Filename: "{app}\EdgeClock.exe"; Flags: nowait runasoriginaluser; Check: ClockWasRunning
; Fresh install: offer it on the last page.
Filename: "{app}\EdgeClock.exe"; Description: "Launch EdgeClock"; Flags: nowait postinstall skipifsilent runasoriginaluser; Check: not ClockWasRunning

[Code]
const
  WM_COMMAND = $0111;
  ID_MENU_EXIT = 2001;  { EdgeClock.cpp: the tray's Exit item }
  CLOCK_MUTEX = 'EdgeClock_GlobalInstance_Mutex';

var
  WasRunning: Boolean;

function ClockWasRunning: Boolean;
begin
  Result := WasRunning;
end;

{ Asks a running clock to exit the same way its tray menu does, and waits up
  to 5 s for its single-instance mutex to go. Every version handles this
  command; since 1.7.3 it also closes an open Settings or file dialog first
  (Cancel) rather than exiting underneath it. Returns True when no clock is
  left running in this session. }
function CloseClock: Boolean;
var
  Wnd: HWND;
  I: Integer;
begin
  Result := not CheckForMutexes(CLOCK_MUTEX);
  if Result then Exit;
  Wnd := FindWindowByClassName('EdgeClockTray');
  if Wnd <> 0 then
    PostMessage(Wnd, WM_COMMAND, ID_MENU_EXIT, 0);
  for I := 1 to 50 do
  begin
    Sleep(100);
    if not CheckForMutexes(CLOCK_MUTEX) then
    begin
      Log(Format('EdgeClock exited after %d ms.', [I * 100]));
      Result := True;
      Exit;
    end;
  end;
  Log('EdgeClock did not exit within 5 s.');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  WasRunning := CheckForMutexes(CLOCK_MUTEX);
  if WasRunning then
    Log('Closing a running EdgeClock; it will be started again afterwards.');
  if WasRunning and not CloseClock then
    Result := 'EdgeClock is still running and did not close. ' +
              'Exit it from its tray icon, then run Setup again.';
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  { usAppMutexCheck runs just before the uninstaller's own mutex check, which
    still fires on existing installs: unins000.dat keeps the AppMutex of every
    pre-1.7.3 install, so dropping the directive does not reach it. Closing
    the clock here gets ahead of that "close it yourself" prompt. }
  if CurUninstallStep = usAppMutexCheck then
  begin
    Log('Closing a running EdgeClock before uninstalling.');
    if not CloseClock then
    begin
      MsgBox('EdgeClock is still running and did not close. ' +
             'Exit it from its tray icon, then uninstall again.', mbError, MB_OK);
      Abort; { Nothing has been removed yet }
    end;
  end;
end;
