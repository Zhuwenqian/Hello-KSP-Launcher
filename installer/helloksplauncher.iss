; Inno Setup script for Hello KSP Launcher.
; Compiled by make-installer.ps1 (ISCC.exe). MyAppVersion is auto-updated
; from src/appversion.h on every build -- do not bump it manually here.

#define MyAppName "Hello KSP Launcher"
#define MyAppVersion "1.4.2"
#define MyAppPublisher "Zhu Wenqian"
#define MyAppExeName "HelloKSPLauncher.exe"

[Setup]
; Fixed GUID: changing it breaks upgrade/uninstall association.
AppId={{7A1C0E52-4F3B-4E9D-8B26-C5D8A0F14E93}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
; Per-user install (no UAC): {userpf} = C:\Users\<user>\AppData\Local\Programs
; The dialog still allows installing with admin rights if the user prefers.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DefaultDirName={userpf}\HelloKSPLauncher
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
LicenseFile=..\LICENSE
SetupIconFile=..\resources\appicon.ico
; Always show the language selection dialog (zh_CN / en_US / ...)
ShowLanguageDialog=yes
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
OutputDir=..
OutputBaseFilename=HelloKSPLauncher-{#MyAppVersion}-setup

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; Full deploy tree from dist/. Exclude runtime-created files: the launcher
; recreates them next to the exe on first run.
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "HKSPL.json,backups,ckan_cache,generic,*.log"

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Runtime data the launcher writes next to the exe (portable-style config).
Type: filesandordirs; Name: "{app}\backups"
Type: filesandordirs; Name: "{app}\ckan_cache"
Type: filesandordirs; Name: "{app}\generic"
Type: files; Name: "{app}\HKSPL.json"
Type: files; Name: "{app}\*.log"
