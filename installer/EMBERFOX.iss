#ifndef AppVersion
  #error AppVersion must be provided by build-installer.ps1
#endif
#ifndef PayloadDir
  #error PayloadDir must be provided by build-installer.ps1
#endif

[Setup]
AppId={{49A55B6A-451C-4559-9572-68E7B9D381B1}
AppName=EMBERFOX
AppVersion={#AppVersion}
AppPublisher=EmberSoft
AppCopyright=Copyright (C) 2026 EmberSoft
DefaultDirName={autopf}\EmberSoft\EMBERFOX
DefaultGroupName=EmberSoft\EMBERFOX
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
WizardStyle=modern
LicenseFile=..\LICENSE.txt
OutputDir={#OutputDir}
OutputBaseFilename={#InstallerFileName}
Compression=lzma2
SolidCompression=yes
CloseApplications=no
RestartApplications=no
SetupMutex=EmberSoftEMBERFOXSetup
UninstallDisplayIcon={app}\EMBERFOX.exe
#ifdef SignedBuild
SignTool=EmberSoft
SignedUninstaller=yes
#endif

[Types]
Name: full; Description: "VST3, standalone and offline documentation"
Name: plugin; Description: "VST3 and offline documentation"
Name: custom; Description: "Custom installation"; Flags: iscustom

[Components]
Name: vst3; Description: "EMBERFOX VST3 effect (Windows x64)"; Types: full plugin custom; Flags: fixed
Name: standalone; Description: "Standalone application"; Types: full

[Files]
Source: "{#RuntimeFile}"; Flags: dontcopy
Source: "{#PayloadDir}\EMBERFOX.vst3\*"; DestDir: "{code:GetVst3Dir}\EMBERFOX.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\EMBERFOX.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#PayloadDir}\Documentation\*"; DestDir: "{app}\Documentation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PayloadDir}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\THIRD_PARTY.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\release-status.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\EMBERFOX-{#AppVersion}-source.zip"; DestDir: "{app}\Source"; Flags: ignoreversion

[Registry]
Root: HKA; Subkey: "Software\EmberSoft\EMBERFOX"; ValueType: string; ValueName: "VST3Dir"; ValueData: "{code:GetVst3Dir}"; Flags: uninsdeletevalue uninsdeletekeyifempty

[Icons]
Name: "{autoprograms}\EmberSoft\EMBERFOX"; Filename: "{app}\EMBERFOX.exe"; Components: standalone
Name: "{autoprograms}\EmberSoft\EMBERFOX User Guide"; Filename: "{app}\Documentation\UserGuide.html"

[Code]
var
  Vst3Page: TInputDirWizardPage;

function GetVst3Dir(Param: String): String;
begin
  Result := Vst3Page.Values[0];
end;

procedure InitializeWizard;
var
  PreviousPath, SelectedPath: String;
begin
  Vst3Page := CreateInputDirPage(wpSelectDir, 'VST3 plug-in location',
    'Choose the folder scanned by your DAW.',
    'The complete EMBERFOX.vst3 bundle will be placed in this folder. Close your DAW before continuing.', False, '');
  Vst3Page.Add('VST3 folder:');
  PreviousPath := ExpandConstant('{autocf}\VST3');
  RegQueryStringValue(HKA, 'Software\EmberSoft\EMBERFOX', 'VST3Dir', PreviousPath);
  SelectedPath := ExpandConstant('{param:VST3DIR|}');
  if SelectedPath = '' then SelectedPath := PreviousPath;
  Vst3Page.Values[0] := SelectedPath;
end;

function RuntimeInstalled: Boolean;
var
  Installed, Major, Minor, Build: Cardinal;
  Key: String;
begin
  Key := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64';
  Result := RegQueryDWordValue(HKLM64, Key, 'Installed', Installed) and (Installed = 1)
    and RegQueryDWordValue(HKLM64, Key, 'Major', Major)
    and RegQueryDWordValue(HKLM64, Key, 'Minor', Minor)
    and RegQueryDWordValue(HKLM64, Key, 'Bld', Build);
  if Result then
    Result := (Major > 14) or ((Major = 14) and ((Minor > 50) or ((Minor = 50) and (Build >= 35717))));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExitCode: Integer;
  Started: Boolean;
begin
  Result := '';
  if RuntimeInstalled then begin
    Log('VC runtime meets the x64 14.50.35717 prerequisite; no shared runtime changes.');
    exit;
  end;
  Log('Installing bundled Microsoft x64 VC runtime prerequisite.');
  ExtractTemporaryFile('vc_redist.x64.exe');
  if IsAdminInstallMode then
    Started := Exec(ExpandConstant('{tmp}\vc_redist.x64.exe'), '/install /passive /norestart', '', SW_HIDE, ewWaitUntilTerminated, ExitCode)
  else
    Started := ShellExec('runas', ExpandConstant('{tmp}\vc_redist.x64.exe'), '/install /passive /norestart', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ExitCode);
  if not Started then begin
    Result := 'Microsoft Visual C++ runtime installation was cancelled or could not start. No EMBERFOX files have been installed.';
    exit;
  end;
  if (ExitCode <> 0) and (ExitCode <> 3010) and not RuntimeInstalled then begin
    Result := 'Microsoft Visual C++ runtime installation failed (code ' + IntToStr(ExitCode) + '). No EMBERFOX files have been installed.';
    exit;
  end;
  if ExitCode = 3010 then NeedsRestart := True;
  if not RuntimeInstalled then Result := 'The required x64 Visual C++ runtime is still unavailable. Restart Windows and run Setup again.';
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := MemoDirInfo + NewLine + NewLine + 'VST3 folder:' + NewLine + Space + GetVst3Dir('')
    + NewLine + NewLine + MemoComponentsInfo;
end;
