[Setup]
AppName=Qt6Chess
AppVersion=1.0.3
AppPublisher=AhooraZen
AppPublisherURL=https://github.com/AhooraZen/Qt6Chess
AppSupportURL=https://github.com/AhooraZen/Qt6Chess/issues
DefaultDirName={autopf}\Qt6Chess
DefaultGroupName=Qt6Chess
OutputDir=.
OutputBaseFilename=Qt6Chess-Setup-x64
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
DisableProgramGroupPage=yes

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Qt6Chess"; Filename: "{app}\Qt6Chess.exe"
Name: "{autodesktop}\Qt6Chess"; Filename: "{app}\Qt6Chess.exe"

[Run]
Filename: "{app}\Qt6Chess.exe"; Description: "Launch Qt6Chess"; Flags: postinstall nowait skipifsilent
