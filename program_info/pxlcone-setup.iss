; SPDX-License-Identifier: GPL-3.0-only
; Compile with ISCC /DPayloadDir=... /DOutputDir=... /DReleaseVersion=0.1.0-alpha.1 /DNumericVersion=0.1.0.0
; PayloadDir must be a reviewed clean non-portable installation, never a working launcher directory.
#ifndef PayloadDir
  #error PayloadDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
#ifndef ReleaseVersion
  #error ReleaseVersion is required
#endif
#ifndef NumericVersion
  #error NumericVersion is required
#endif
#if FileExists(PayloadDir + "\portable.txt")
  #error A Setup payload must not include portable.txt
#endif
#if !FileExists(PayloadDir + "\pxlcone.exe")
  #error The Setup payload must contain pxlcone.exe
#endif
[Setup]
AppId=PXLConeLauncherAlpha
AppName=PXL Cone Launcher
AppVersion={#ReleaseVersion}
AppVerName=PXL Cone Launcher {#ReleaseVersion}
AppPublisher=PXL Cone Contributors
AppPublisherURL=https://github.com/kakixc/PXL-Cone-Launcher
AppSupportURL=https://github.com/kakixc/PXL-Cone-Launcher/issues
DefaultDirName={localappdata}\Programs\PXLConeLauncherAlpha
DefaultGroupName=PXL Cone Launcher Alpha
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=PXL-Cone-Windows-x64-Setup-{#ReleaseVersion}
SetupIconFile=pxlcone.ico
UninstallDisplayIcon={app}\pxlcone.exe
VersionInfoVersion={#NumericVersion}
VersionInfoProductVersion={#NumericVersion}
VersionInfoProductTextVersion={#ReleaseVersion}
VersionInfoTextVersion={#ReleaseVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes
CloseApplications=yes
RestartApplications=no
CloseApplicationsFilter=pxlcone.exe,pxlcone_filelink.exe
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\PXL Cone Launcher Alpha"; Filename: "{app}\pxlcone.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\PXL Cone Launcher Alpha"; Filename: "{app}\pxlcone.exe"; WorkingDir: "{app}"; Tasks: desktopicon
[Run]
Filename: "{app}\pxlcone.exe"; Description: "{cm:LaunchProgram,PXL Cone Launcher}"; Flags: nowait postinstall skipifsilent
