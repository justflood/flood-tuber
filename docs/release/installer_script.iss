; Flood Tuber — Inno Setup Installer Script (OBS Studio 33+ ProgramData layout)
#define MyAppName "Flood Tuber"
#ifndef MyAppVersion
  #define MyAppVersion "1.2.0"
#endif
#define MyAppPublisher "justflood"
#define MyAppURL "https://github.com/justflood/flood-tuber"
#ifndef RepoRoot
  #define RepoRoot "..\.."
#endif
#ifndef SourceStageDir
  #define SourceStageDir "..\..\release\RelWithDebInfo\flood-tuber"
#endif

[Setup]
AppId={{A3590500-2475-4C3D-9477-802F3F82D368}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonappdata}\obs-studio\plugins\flood-tuber
UsePreviousAppDir=no
DisableDirPage=yes
DirExistsWarning=no
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\icon.ico
PrivilegesRequired=admin
OutputBaseFilename=FloodTuber-Installer-{#MyAppVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#RepoRoot}\docs\release\icon.ico
LicenseFile={#RepoRoot}\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[CustomMessages]
; --- English ---
english.info_title=About Flood Tuber
english.info_subtitle=A free, open-source VTuber avatar plugin for OBS Studio (OBS 33+)
english.info_body=Flood Tuber  v{#MyAppVersion}%nby justflood%n%nhttps://github.com/justflood/flood-tuber%n%n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%n%nYour avatar reacts to your microphone. When you speak it animates,%nwhen you go silent it returns to idle. That's the core idea.%n%nFEATURES%n  ·  Talking animation with up to 3 mouth frames  (A → B → C)%n  ·  Blink animation with randomized intervals%n  ·  Idle action animation  (e.g. sipping coffee)%n  ·  Talking motion effects:  Bounce  or  Shake%n  ·  Built-in Avatar Library with 7 ready-to-use avatars%n  ·  PNG, GIF, WebP, and Animated PNG (APNG) support%n  ·  Full English, Turkish, and French localization%n%n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%n%nREQUIREMENT:  OBS Studio 33.0 or newer must be installed.%n(Legacy custom avatars in Program Files are automatically migrated & backed up to ProgramData.)%n%nAfter installation:%n  Open OBS Studio → Add Source → "Flood Tuber Avatar"

; --- Turkish ---
turkish.info_title=Flood Tuber Hakkında
turkish.info_subtitle=OBS Studio için ücretsiz, açık kaynaklı bir VTuber avatar eklentisi (OBS 33+)
turkish.info_body=Flood Tuber  v{#MyAppVersion}%njustflood tarafından%n%nhttps://github.com/justflood/flood-tuber%n%n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%n%nAvatarınız mikrofonunuza tepki verir. Konuştuğunuzda animasyon yapar,%nsessiz kaldığınızda boşta durumuna döner.%n%nÖZELLİKLER%n  ·  3 adede kadar ağız karesi ile konuşma animasyonu  (A → B → C)%n  ·  Rastgele aralıklı göz kırpma animasyonu%n  ·  Boşta aksiyon animasyonu  (örn: kahve içme)%n  ·  Konuşma hareket efektleri:  Zıplama  veya  Titreme%n  ·  7 kullanıma hazır avatar ile dahili Avatar Kütüphanesi%n  ·  PNG, GIF, WebP ve Animasyonlu PNG (APNG) desteği%n  ·  Tam İngilizce, Türkçe ve Fransızca dil desteği%n%n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%n%nGEREKSİNİM:  OBS Studio 33.0 veya üzeri kurulu olmalıdır.%n(Program Files içindeki eski sürüm özel avatarlarınız otomatik olarak ProgramData klasörüne taşınır ve yedeklenir.)%n%nKurulum sonrası:%n  OBS Studio'yu açın → Kaynak Ekle → "Flood Tuber Avatar"

[InstallDelete]
; Clean up legacy bin/64bit subfolder if present in ProgramData
Type: filesandordirs; Name: "{app}\bin"
; Clean up legacy v1.1.0 binary files from OBS Studio Program Files folder
; Note: Legacy data/obs-plugins/flood-tuber is migrated and backed up in [Code] before deletion
Type: files; Name: "{code:GetOBSDir}\obs-plugins\64bit\flood-tuber.dll"
Type: files; Name: "{code:GetOBSDir}\obs-plugins\64bit\flood-tuber.pdb"

[Files]
; Plugin DLL and data folder installed into C:\ProgramData\obs-studio\plugins\flood-tuber
Source: "{#SourceStageDir}\flood-tuber.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceStageDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#RepoRoot}\docs\release\icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Code]
var
  InfoPage: TOutputMsgMemoWizardPage;

// Creates a localized info page after the Welcome screen
procedure InitializeWizard;
begin
  InfoPage := CreateOutputMsgMemoPage(
    wpWelcome,
    CustomMessage('info_title'),
    CustomMessage('info_subtitle'),
    '',
    CustomMessage('info_body')
  );
end;

// Detects OBS Studio installation directory from Windows Registry (for legacy file cleanup)
function GetOBSDir(Param: String): String;
var
  InstallPath: String;
begin
  if RegQueryStringValue(HKLM64, 'SOFTWARE\OBS Studio', '', InstallPath) then
    Result := InstallPath
  else if RegQueryStringValue(HKLM32, 'SOFTWARE\OBS Studio', '', InstallPath) then
    Result := InstallPath
  else
    Result := ExpandConstant('{commonpf64}\obs-studio');
end;

// Recursively copies all files and subdirectories from SrcDir to DestDir
function CopyDirRecursive(const SrcDir, DestDir: String): Boolean;
var
  FindRec: TFindRec;
  SrcFilePath, DestFilePath: String;
begin
  Result := True;
  if not ForceDirectories(DestDir) then
  begin
    Log('Failed to create directory: ' + DestDir);
    Result := False;
    Exit;
  end;

  if FindFirst(SrcDir + '\*', FindRec) then
  begin
    try
      repeat
        if (FindRec.Name <> '.') and (FindRec.Name <> '..') then
        begin
          SrcFilePath := SrcDir + '\' + FindRec.Name;
          DestFilePath := DestDir + '\' + FindRec.Name;
          if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
          begin
            if not CopyDirRecursive(SrcFilePath, DestFilePath) then
              Result := False;
          end
          else
          begin
            if not CopyFile(SrcFilePath, DestFilePath, False) then
            begin
              Log('Failed to copy file: ' + SrcFilePath + ' -> ' + DestFilePath);
              Result := False;
            end;
          end;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

// Migrates and backs up legacy v1.1.0 avatars from Program Files before cleaning up the legacy folder
procedure MigrateAndCleanLegacyData;
var
  LegacyDataDir, LegacyAvatarsDir, NewAvatarsDir, BackupAvatarsDir: String;
  BackupOk, MigrateOk: Boolean;
begin
  LegacyDataDir := GetOBSDir('') + '\data\obs-plugins\flood-tuber';
  if not DirExists(LegacyDataDir) then
    Exit;

  LegacyAvatarsDir := LegacyDataDir + '\avatars';
  if DirExists(LegacyAvatarsDir) then
  begin
    BackupAvatarsDir := ExpandConstant('{app}\backup_legacy_avatars');
    NewAvatarsDir := ExpandConstant('{app}\data\avatars');

    Log('Migrating legacy avatars from: ' + LegacyAvatarsDir);
    BackupOk := CopyDirRecursive(LegacyAvatarsDir, BackupAvatarsDir);
    MigrateOk := CopyDirRecursive(LegacyAvatarsDir, NewAvatarsDir);

    if BackupOk and MigrateOk then
    begin
      Log('Legacy avatars successfully backed up and migrated. Cleaning up: ' + LegacyDataDir);
      DelTree(LegacyDataDir, True, True, True);
    end
    else
    begin
      Log('Warning: Legacy avatar migration encountered an error; keeping legacy avatars folder safe.');
      DelTree(LegacyDataDir + '\locale', True, True, True);
    end;
  end
  else
  begin
    DelTree(LegacyDataDir, True, True, True);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
  begin
    MigrateAndCleanLegacyData;
  end;
end;
