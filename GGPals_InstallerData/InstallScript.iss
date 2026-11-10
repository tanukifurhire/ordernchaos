; TCR compliant Sample install script.
; Basically put this file in the root for what you want included in the installer,
;   then put everything into the subdirectories as listed in the installer below.
;   Remember to change the AppId
;   Thanks to Dan Weiss (dweiss@digipen.edu) for the original version.

; Order N Chaos Installer Script
; GGPals Team - DigiPen Institute of Technology 2025

[Setup]
; !!NOTE!!: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click "Tools" | "Create GUID" inside the Visual Studio IDE.)
AppId={{E9A1FAD4-1A7E-40DE-A237-FC8313779BAF}}

; Standard app data stuff
AppName=Order N Chaos
AppVerName=Order N Chaos Version 1.0
AppPublisher=DigiPen Institute of Technology
AppPublisherURL=http://www.digipen.edu/
AppSupportURL=http://www.digipen.edu/

; Icon shown in Control Panel's Programs and Features
UninstallDisplayIcon={app}\GameIcon.ico

; Default path to the file storage directory.
; {pf} is the default program files directory set by Windows
DefaultDirName={pf}\DigiPen\OrderNChaos

; Start menu directory
DefaultGroupName=DigiPen\Order N Chaos

; Output directory for the installer.
OutputDir=.\INSTALLER

; Setup executable installer
OutputBaseFilename=OrderNChaos_Setup

; Path to the DigiPen EULA (Needed to pass TCRs)
LicenseFile=INSTALLERFILES\DigiPen_EULA.txt

; Path to the icon for the installer (TCR check requires custom icon)
SetupIconFile=.\INSTALLERFILES\SetupIcon.ico

; Compression scheme for the installer. Check Inno Setup help files for more options.
Compression=lzma
SolidCompression=yes

; admin causes a UAC prompt so the installer can write to Program Files and
; the VC++ redist sub-installer can write to the registry and System32.
PrivilegesRequired=admin

; Close running application before uninstall
CloseApplications=yes
CloseApplicationsFilter=*.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
; Creates an installer option to allow/disallow desktop shortcut
; Checked by default
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

; This is the list of files that the installer should grab and install.
;
; Destination Directory Notes
;   {app} is the root directory that you will be installing to.
;   {temp} is a temporary directory that Windows sets that gets deleted after the
;      installer quits.
;   {userdocs} is the directory for My Documents/Documents on Windows XP, Vista, and 7.
;
; For more information on default paths to install to, check the "Constants" article
;   in the Inno Setup 5 documentation.
[Files]
; The game directory is exactly what you want your install directory in program files to look like
Source: .\GAMEDIRECTORY\*; DestDir: {app}; Flags: ignoreversion recursesubdirs createallsubdirs
; Game icon for Start Menu and Desktop shortcuts
Source: .\INSTALLERFILES\GameIcon.ico; DestDir: {app}; Flags: ignoreversion
; VC++ 2015-2022 x64 Redistributable bundled so players do not need to install it manually.
; The [Code] section below skips execution if the runtime is already present on the machine,
; preventing the 0x80070666 "another version already installed" error dialog.
; DO NOT COMMIT this file to git -- download it from:
;   https://aka.ms/vs/17/release/vc_redist.x64.exe
; and place it at: GGPals_InstallerData\REDIST\VC_redist.x64.exe
Source: .\REDIST\VC_redist.x64.exe; DestDir: {tmp}; Flags: ignoreversion

; This is the list of shortcuts that the installer will setup for you.
; Of note, this will create the uninstaller automatically.
;
; Directory Notes
;   {group} is the start menu location that the game will install shortcuts to.
;   {commondesktop} is your Windows desktop directory.

[Icons]
; Start Menu shortcut
Name: {group}\Order N Chaos; Filename: {app}\GGPals_CustomEngine\GGEngine\bin\Release\OrderNChaos.exe; WorkingDir: {app}\GGPals_CustomEngine\GGEngine\bin\Release; IconFilename: {app}\GameIcon.ico
; Uninstaller shortcut
Name: {group}\{cm:UninstallProgram,Order N Chaos}; Filename: {uninstallexe}
; Desktop shortcut (optional)
Name: {commondesktop}\Order N Chaos; Filename: {app}\GGPals_CustomEngine\GGEngine\bin\Release\OrderNChaos.exe; Tasks: desktopicon; WorkingDir: {app}\GGPals_CustomEngine\GGEngine\bin\Release; IconFilename: {app}\GameIcon.ico

; List of items to execute in the installer.
; Note that this will run all executables in their silent versions as required by the TCRs.
;
; The last item being run is the installer option to automatically launch the game after
;   the installer exits as required by the TCRs.
[Run]
; Install VC++ 2015-2022 x64 Redistributable silently.
; The Check function skips this entirely if the runtime is already present,
; preventing the 0x80070666 "another version already installed" error dialog.
Filename: {tmp}\VC_redist.x64.exe; Parameters: /install /quiet /norestart; StatusMsg: Installing Visual C++ 2022 Redistributable...; Check: VCRedistNeedsInstall

; Launch game after installation
Filename: {app}\GGPals_CustomEngine\GGEngine\bin\Release\OrderNChaos.exe; Description: {cm:LaunchProgram,Order N Chaos}; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Remove all files and folders in the install directory during uninstall.
; This ensures complete cleanup even if runtime-generated files exist.
Type: filesandordirs; Name: "{app}\GGPals_CustomEngine"
Type: filesandordirs; Name: "{app}"

; To build this installer: install Inno Setup from https://jrsoftware.org/isinfo.php
; then right-click this file and select "Compile" or run BuildInstaller.bat.

[Code]
// Check whether the VC++ 2015-2022 x64 runtime is already installed.
// Called by the [Run] entry above -- returns True to run the redist,
// False to skip it. Skipping avoids the 0x80070666 error on machines
// that already have a newer version of the runtime installed.
function VCRedistNeedsInstall: Boolean;
var
    InstalledVersion: String;
begin
    // The VC++ 2015-2022 x64 runtime writes its version under this key.
    // If the key exists the runtime is already present, so skip the install.
    if RegQueryStringValue(
        HKLM,
        'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
        'Version',
        InstalledVersion) then
    begin
        Result := False;
    end
    else
    begin
        Result := True;
    end;
end;
