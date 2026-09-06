#ifndef MyAppVersion
  #define MyAppVersion "4.2.dev0"
#endif

#define MyAppName "Seed Quarry"
#define MyAppExeName "seed-quarry.exe"
#define MyPortableDir "..\..\dist\Seed-Quarry-" + MyAppVersion + "-Windows-x64-Portable"
#define MySourceArchive "..\..\dist\Seed-Quarry-" + MyAppVersion + "-Source.zip"

[Setup]
AppId={{9EA88549-5B8A-493B-AD00-AA2FB6BE7D02}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=Seed Quarry Project
DefaultDirName={localappdata}\Programs\Seed Quarry
DefaultGroupName=Seed Quarry
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\..\dist
OutputBaseFilename=Seed-Quarry-{#MyAppVersion}-Windows-x64-Setup
SetupIconFile=..\..\rc\app_icon.ico
LicenseFile=..\..\LICENSE
InfoBeforeFile=..\..\LEGAL_NOTICE.md
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
CloseApplicationsFilter=seed-quarry.exe
RestartApplications=no
AppMutex=SeedQuarry_9EA88549_5B8A_493B_AD00_AA2FB6BE7D02
VersionInfoVersion=4.2.0.0
VersionInfoCompany=Seed Quarry Project
VersionInfoDescription=Seed Quarry Windows Installer
VersionInfoProductName=Seed Quarry
VersionInfoProductVersion=4.2.0.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MyPortableDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MySourceArchive}"; DestDir: "{app}\Source"; Flags: ignoreversion

[Icons]
Name: "{group}\Seed Quarry"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\Seed Quarry"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,Seed Quarry}"; Flags: nowait postinstall skipifsilent
