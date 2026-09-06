[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$SourceFile,
      [Parameter(Mandatory)][string]$CMakeExecutable,
      [switch]$ApplyPatch,
      [switch]$ApplySkinPatch)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = [Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes($SourceFile))
if ($ApplyPatch -or $ApplySkinPatch) {
    . (Join-Path $RepositoryRoot 'scripts/reference_game_patches.ps1')
    if ($ApplyPatch) { $source = Add-ReferenceStableCharacterSelection $source }
    if ($ApplySkinPatch) { $source = Add-ReferenceIndependentSkinSelection $source }
}
$methods = foreach ($name in @('OnCharacterImageButtonClicked','OnCharacterSelectButtonClicked','StartLumiaIsland')) {
    $matches = [regex]::Matches($source, ('(?ms)^void CharacterSelectScene::' + $name + '\([^)]*\)\s*\{.*?^\}'))
    if ($matches.Count -ne 1) { throw "Unexpected character selection method: $name" }
    $matches[0].Value
}
$skinMethods = [regex]::Matches($source, '(?ms)^void CharacterSelectScene::UpdateSkinList\([^)]*\)\s*\{.*?^\}')
if ($skinMethods.Count -ne 1) { throw 'Unexpected skin list method' }
$skinBindings = [regex]::Matches($skinMethods[0].Value, '(?s)button->OnClick\s*\+=\s*\[this, button, i\]\(\)\s*\{.*?\};')
if ($skinBindings.Count -ne 1) { throw 'Unexpected skin click event binding' }
$skinBinding = "shared_ptr<Button> CharacterSelectScene::CreateSkinButton(int i) {`n    auto button = make_shared<Button>();`n" + $skinBindings[0].Value + "`n    return button;`n}"
$fixture = @'
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <functional>
using namespace std;
struct Sound { void PlaySound(const wstring&, int, float) {} } sound;
#define SOUND (&sound)
const vector<wstring> charcaterSelectVoice{L"default", L"Bianca", L"Nicky"};
struct LumiaIsland {
    int selected = -1;
    void SetSelectedCharacter(int value) { selected = value; }
};
struct Scenes {
    shared_ptr<LumiaIsland> current;
    void ChangeScene(shared_ptr<LumiaIsland> scene) { current = scene; }
} scenes;
#define SCENE (&scenes)
struct ClickEvent {
    vector<function<void()>> handlers;
    void operator+=(function<void()> handler) { handlers.push_back(move(handler)); }
    void Invoke() { for (const auto& handler : handlers) handler(); }
};
struct Button { ClickEvent OnClick; };
struct CharacterSelectScene {
    int m_selectCharIdx = 0;
    float m_selectElapsedTime = 0;
    int previewSkin = -1;
    void OnCharacterImageButtonClicked(int);
    void OnCharacterSelectButtonClicked(int);
    void StartLumiaIsland();
    void ClickStart() { OnCharacterSelectButtonClicked(m_selectCharIdx); }
    shared_ptr<Button> CreateSkinButton(int i);
    void UpdateFullImage(shared_ptr<Button>, int skin) { previewSkin = skin; }
};
'@
$checks = @'
void require(bool value, const char* message) { if (!value) { cerr << message << endl; exit(1); } }
int main() {
    // Invoke the original registered skin delegate, not a hand-written proxy.
    // Skin 2 previously changed Bianca to Nicky and accelerated the countdown.
    for (int character : {1, 2}) {
        CharacterSelectScene selection;
        selection.OnCharacterImageButtonClicked(character);
        for (float elapsed : {0.f, 12.75f, 46.f}) {
            selection.m_selectElapsedTime = elapsed;
            for (int skin : {2, 0, 1, 2}) {
                auto button = selection.CreateSkinButton(skin);
                button->OnClick.Invoke();
                button->OnClick.Invoke();
                require(selection.previewSkin == skin, "Skin click did not update its preview");
                require(selection.m_selectCharIdx == character, "Skin click changed the selected character");
                require(selection.m_selectElapsedTime == elapsed, "Skin click triggered quick start");
                selection.StartLumiaIsland();
                require(scenes.current->selected == character - 1, "Skin click changed the timed-start actor");
            }
        }
        selection.m_selectElapsedTime = 12.75f;
        selection.ClickStart();
        require(selection.m_selectElapsedTime == 44.5f, "Explicit quick start no longer accelerates the countdown");
        selection.CreateSkinButton(0)->OnClick.Invoke();
        selection.CreateSkinButton(2)->OnClick.Invoke();
        require(selection.m_selectElapsedTime == 44.5f, "Skin click reset an already-confirmed countdown");
        selection.ClickStart();
        selection.StartLumiaIsland();
        require(scenes.current->selected == character - 1, "Quick start after skin changes selected the wrong actor");
    }
    CharacterSelectScene nicky;
    nicky.OnCharacterImageButtonClicked(2);
    nicky.ClickStart();
    nicky.StartLumiaIsland();
    require(scenes.current->selected == 1, "Nicky single confirm must spawn Nicky");
    nicky.ClickStart();
    nicky.StartLumiaIsland();
    require(scenes.current->selected == 1, "Repeated start changed Nicky to Bianca");
    nicky.ClickStart();
    nicky.StartLumiaIsland();
    require(scenes.current->selected == 1, "Third start changed Nicky");
    require(nicky.m_selectElapsedTime == 44.5f, "Confirm changed the original countdown");
    CharacterSelectScene timedNicky;
    timedNicky.OnCharacterImageButtonClicked(2);
    timedNicky.StartLumiaIsland();
    require(scenes.current->selected == 1, "Countdown must spawn the selected Nicky");
    CharacterSelectScene bianca;
    bianca.OnCharacterImageButtonClicked(1);
    bianca.ClickStart(); bianca.ClickStart(); bianca.StartLumiaIsland();
    require(scenes.current->selected == 0, "Bianca confirm must remain Bianca");
    bianca.OnCharacterImageButtonClicked(2);
    bianca.ClickStart(); bianca.StartLumiaIsland();
    require(scenes.current->selected == 1, "Reselection to Nicky was lost");
    CharacterSelectScene fallback;
    fallback.StartLumiaIsland();
    require(scenes.current->selected == 0, "Unselected countdown changed its default");
}
'@
# Real reference methods stay in a private temporary build, outside the repository.
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('rcs-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $temporary 'main.cpp'), $fixture + "`n" + ($methods -join "`n") + "`n" + $skinBinding + "`n" + $checks)
    [IO.File]::WriteAllText((Join-Path $temporary 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(CharacterSelection LANGUAGES CXX)`nadd_executable(character_selection main.cpp)`ntarget_compile_options(character_selection PRIVATE /utf-8)`n")
    $build = Join-Path $temporary 'build'
    & $CMakeExecutable -S $temporary -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Character selection fixture configuration failed' }
    & $CMakeExecutable --build $build --config Debug --target character_selection | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Character selection fixture compilation failed' }
    & (Join-Path $build 'Debug/character_selection.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Character selection behavior failed' }
    Write-Output 'PASS: actual skin delegates preserve character/countdown and update previews; explicit quick start, repeated confirm, timed start and reselection remain correct for Bianca and Nicky'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^rcs-[0-9a-f]{8}$') { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
