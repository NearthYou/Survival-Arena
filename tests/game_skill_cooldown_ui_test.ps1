[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$CMakeExecutable)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($RepositoryRoot)
$source = [Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes((Join-Path $root 'game/Client/GameHUDPanelUI.cpp')))
$method = [regex]::Matches($source, '(?ms)^void GameHUDPanelUI::UpdateSkillCoolDown\(\)\s*\{.*?^\}')
if ($method.Count -ne 1) { throw 'Cooldown UI method was not found' }
$fixture = @'
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <iostream>
#include <string>
#include <vector>
using namespace std;
struct ISkill { float cooldown=0; float GetCurrentCooldown() const { return cooldown; } };
struct Player { array<ISkill,4> skills; ISkill* GetSkill(int i) { return &skills[i]; } };
struct D2DText { wstring text; bool visible=false; void SetText(wstring value) { text=value; } void SetVisible(bool value) { visible=value; } };
struct Panel { map<wstring,shared_ptr<D2DText>> labels; Panel* GetUIPanel() { return this; } shared_ptr<D2DText> GetD2DText(const wstring& name) { return labels[name]; } };
struct GameHUDPanelUI { shared_ptr<Player> m_player=make_shared<Player>(); shared_ptr<Panel> m_panel=make_shared<Panel>(); void UpdateSkillCoolDown(); };
'@
$checks = @'
int main() {
    GameHUDPanelUI hud;
    const array<wstring,4> names{L"QSkillCoolDown",L"WSkillCoolDown",L"ESkillCoolDown",L"RSkillCoolDown"};
    for (const auto& name:names) hud.m_panel->labels[name]=make_shared<D2DText>();
    hud.m_player->skills[0].cooldown=5.f;
    hud.m_player->skills[1].cooldown=1.25f;
    hud.m_player->skills[2].cooldown=0.01f;
    hud.UpdateSkillCoolDown();
    const array<wstring,4> expected{L"5",L"2",L"1",L"0"};
    for(int i=0;i<4;++i) {
        auto label=hud.m_panel->labels[names[i]];
        if(label->text!=expected[i] || label->visible!=(i<3)) {
            cerr << "Cooldown UI must remain visible through the last fraction of a second" << endl;
            return 1;
        }
    }
    for(auto& skill:hud.m_player->skills) skill.cooldown=0;
    hud.UpdateSkillCoolDown();
    for(const auto& name:names) if(hud.m_panel->labels[name]->visible) return 2;
}
'@
$output = Join-Path $root ('out/cooldown-ui-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $output | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $output 'main.cpp'), $fixture + "`n" + $method[0].Value + "`n" + $checks, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $output 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(SkillCooldownUI LANGUAGES CXX)`nadd_executable(cooldown_ui main.cpp)`ntarget_compile_features(cooldown_ui PRIVATE cxx_std_17)`ntarget_compile_options(cooldown_ui PRIVATE /utf-8)`n")
    & $CMakeExecutable -S $output -B (Join-Path $output 'build') -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cooldown UI fixture configuration failed' }
    $compiled = & $CMakeExecutable --build (Join-Path $output 'build') --config Debug 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Cooldown UI fixture build failed: $compiled" }
    & (Join-Path $output 'build/Debug/cooldown_ui.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Cooldown UI hides an unavailable skill too early' }
    'PASS: positive cooldowns round up; zero hides the label'
} finally {
    $allowed = (Join-Path $root 'out') + [IO.Path]::DirectorySeparatorChar
    if (-not $output.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $output) -notmatch '^cooldown-ui-[0-9a-f]{8}$') { throw 'Cooldown UI cleanup boundary mismatch' }
    Remove-Item -LiteralPath $output -Recurse -Force
}
