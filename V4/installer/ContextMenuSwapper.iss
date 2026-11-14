#ifndef PayloadDir
  #error PayloadDir must point to the verified V4.1 portable directory.
#endif
#ifndef InstallerOutput
  #define InstallerOutput "..\dist\setup"
#endif
#define AppName "Context Menu Swapper"
#define AppVersion "4.1.0"
#define AppExe "ContextMenuSwapper.exe"
#define ProjectURL "https://github.com/Lethudu70/Win11-Context-Menu-Swapper"

[Setup]
AppId={{786E729A-5920-4855-A954-A209C9227904}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} v4.1 - Leth Edition
AppPublisher=Lethudu70
AppPublisherURL={#ProjectURL}
AppSupportURL={#ProjectURL}/issues
AppUpdatesURL={#ProjectURL}/releases/tag/V4.1
DefaultDirName={localappdata}\Programs\LethTools\ContextMenuSwapper
DefaultGroupName=LethTools\Context Menu Swapper
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
SetupIconFile=..\resources\app.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName} v4.1 - Leth Edition
OutputDir={#InstallerOutput}
OutputBaseFilename=ContextMenuSwapper-v4.1-Setup
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Context Menu Swapper v4.1 Setup - Leth Edition
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern dark includetitlebar
WizardBackColor=#0b1110
WizardBackImageFile=assets\wizard-background.png
WizardImageFile=assets\wizard-banner.png
WizardImageBackColor=#0b1110
WizardSmallImageFile=assets\wizard-small.png
WizardSmallImageBackColor=#0b1110
WizardImageStretch=yes
WizardSizePercent=120,120
AllowNoIcons=yes
DisableWelcomePage=no
DisableReadyPage=no
DisableFinishedPage=no
ShowLanguageDialog=yes
LanguageDetectionMethod=none
UsePreviousLanguage=no
CloseApplications=yes
RestartApplications=no
Uninstallable=yes
CreateUninstallRegKey=yes
UsePreviousAppDir=yes
UsePreviousTasks=yes
UninstallLogging=yes

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"; InfoBeforeFile: "info-fr.txt"
Name: "english"; MessagesFile: "compiler:Default.isl"; InfoBeforeFile: "info-en.txt"

[Messages]
french.WelcomeLabel1=Bienvenue dans Context Menu Swapper v4.1
english.WelcomeLabel1=Welcome to Context Menu Swapper v4.1
french.WelcomeLabel2=Une interface claire pour choisir votre menu contextuel Windows.%n%nCet assistant installe l'application, ses bibliothèques et ses raccourcis pour votre compte.%n%nLes réglages du menu se choisissent ensuite dans l'application.
english.WelcomeLabel2=A clear interface for choosing your Windows context menu.%n%nThis wizard installs the app, its libraries and shortcuts for your account.%n%nChoose your menu settings afterward, inside the application.
french.FinishedHeadingLabel=Context Menu Swapper est prêt
english.FinishedHeadingLabel=Context Menu Swapper is ready
french.FinishedLabelNoIcons=L'application est installée. Vous pouvez maintenant choisir votre configuration et la sauvegarder avant toute modification.
english.FinishedLabelNoIcons=The application is installed. You can now choose your configuration and back it up before making changes.
french.ConfirmUninstall=Voulez-vous désinstaller %1 ?%n%nLes réglages Windows et les sauvegardes d'annulation seront conservés. Pour revenir au menu précédent, utilisez Annuler dans l'application avant de la désinstaller.
english.ConfirmUninstall=Do you want to uninstall %1?%n%nWindows settings and undo backups will be preserved. To return to the previous menu, use Undo in the application before uninstalling it.

[CustomMessages]
french.LaunchApp=Lancer Context Menu Swapper
english.LaunchApp=Launch Context Menu Swapper
french.ShortcutComment=Choisir et restaurer la configuration du menu contextuel Windows
english.ShortcutComment=Choose and restore the Windows context-menu configuration

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Excludes: "self-test.json,manifest-sha256.json"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Context Menu Swapper v4.1"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; IconFilename: "{app}\{#AppExe}"; Comment: "{cm:ShortcutComment}"; AppUserModelID: "LethTools.ContextMenuSwapper"
Name: "{autodesktop}\Context Menu Swapper v4.1"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; IconFilename: "{app}\{#AppExe}"; Tasks: desktopicon; Comment: "{cm:ShortcutComment}"

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchApp}"; Flags: nowait postinstall skipifsilent
