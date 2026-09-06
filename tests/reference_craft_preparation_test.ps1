[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$ReferenceRoot,
      [Parameter(Mandatory)][string]$CMakeExecutable,
      [switch]$ApplyPatch)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$encoding = [Text.Encoding]::GetEncoding(949)
$scene = $encoding.GetString([IO.File]::ReadAllBytes((Join-Path $ReferenceRoot 'Client/LumiaIsland.cpp')))
$list = $encoding.GetString([IO.File]::ReadAllBytes((Join-Path $ReferenceRoot 'Client/CraftListPanelUI.cpp')))
if ($ApplyPatch) {
    . (Join-Path $RepositoryRoot 'scripts/reference_game_patches.ps1')
    $scene = Add-ReferenceCraftStatePreparation $scene
}
$callbacks = [regex]::Matches($scene, '(?s)m_player->GetPlayerStateMachine\(\)->OnTryCraft(?:First)?\.Push\(\[this\]\(bool& success\)\s*\{.*?\}\);')
if ($callbacks.Count -ne 2) { throw 'Unexpected source craft preparation callbacks' }
$methods = foreach ($name in @('OnCraftSlotClicked','GetCraftTimeByGrade')) {
    $match = [regex]::Matches($list, ('(?ms)^(?:void|float) CraftListPanelUI::' + $name + '\([^)]*\)\s*\{.*?^\}'))
    if ($match.Count -ne 1) { throw "Unexpected craft list method: $name" }
    $match[0].Value
}
$fixture = @'
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <cstdlib>
using namespace std;
enum class ITEMGRADE { COMMON, UNCOMMON, RARE, EPIC, LEGENDARY, UNKNOWN };
enum class PlayerStateType { Wait, Run, Craft };
enum class AnimationStateType { Wait, Run, Craft };
struct State {
    int recipe = 37;
    float duration = -17.f;
    void SetRecipeIndex(int index) { recipe = index; }
    void SetExpectedDuration(float value) { duration = value; }
};
struct PrepareEvent {
    vector<function<void(bool&)>> handlers;
    void Push(function<void(bool&)> handler) { handlers.push_back(move(handler)); }
    void Invoke(bool& success) { for (const auto& handler : handlers) handler(success); }
};
template<class Kind> struct Machine {
    shared_ptr<State> wait = make_shared<State>();
    shared_ptr<State> run = make_shared<State>();
    shared_ptr<State> craft = make_shared<State>();
    Kind current = Kind::Wait;
    vector<Kind> requested;
    PrepareEvent OnTryCraft;
    PrepareEvent OnTryCraftFirst;
    shared_ptr<State> GetState(Kind kind) {
        return kind == Kind::Craft ? craft : kind == Kind::Run ? run : wait;
    }
    shared_ptr<State> GetCurrentStatePtr() { return GetState(current); }
    void RequestStateChange(Kind next) { requested.push_back(next); }
};
struct Navigation { bool stopped = false; void Stop() { stopped = true; } };
struct Player {
    shared_ptr<Machine<PlayerStateType>> logic = make_shared<Machine<PlayerStateType>>();
    shared_ptr<Machine<AnimationStateType>> animation = make_shared<Machine<AnimationStateType>>();
    shared_ptr<Navigation> navigation = make_shared<Navigation>();
    auto GetPlayerStateMachine() { return logic; }
    auto GetAnimationStateMachine() { return animation; }
    auto GetNavMeshAgent() { return navigation; }
};
struct Item {
    ITEMGRADE grade;
    ITEMGRADE GetItemGrade() { return grade; }
};
struct Recipe {
    int result;
    int GetResultItemID() { return result; }
};
struct InventoryManager {
    vector<shared_ptr<Recipe>> recipes;
    vector<int> slots;
    static InventoryManager* GetInstance() { static InventoryManager instance; return &instance; }
    auto GetAvailableRecipes() { return recipes; }
    auto& GetInventorySlots() { return slots; }
};
struct ItemManager {
    map<int,shared_ptr<Item>> items;
    static ItemManager* GetInstance() { static ItemManager instance; return &instance; }
    shared_ptr<Item> GetItem(int id) { return items.count(id) ? items.at(id) : nullptr; }
};
struct GagePanel {
    bool visible = false;
    shared_ptr<Item> item;
    void SetVisible(bool value) { visible = value; }
    void SetItem(shared_ptr<Item> value) { item = value; }
};
struct UIManager {
    shared_ptr<GagePanel> gage = make_shared<GagePanel>();
    auto GetCraftGageUI() { return gage; }
};
struct LumiaIsland {
    shared_ptr<Player> m_player = make_shared<Player>();
    shared_ptr<UIManager> m_uiManager = make_shared<UIManager>();
    void RegisterPreparation();
};
struct CraftListPanelUI {
    shared_ptr<Player> m_player;
    shared_ptr<GagePanel> m_gagePanel;
    vector<shared_ptr<Recipe>> m_craftableRecipes;
    void OnCraftSlotClicked(int slotIndex);
    float GetCraftTimeByGrade(ITEMGRADE grade);
};
'@
$registration = "void LumiaIsland::RegisterPreparation() {`n" + (($callbacks | ForEach-Object Value) -join "`n") + "`n}"
$checks = @'
void require(bool value, const char* reason) {
    if (!value) { cerr << reason << endl; exit(1); }
}
int main() {
    const vector<ITEMGRADE> grades{ITEMGRADE::COMMON, ITEMGRADE::UNCOMMON, ITEMGRADE::RARE,
        ITEMGRADE::EPIC, ITEMGRADE::LEGENDARY, ITEMGRADE::UNKNOWN};
    const vector<float> durations{1.f, 3.f, 5.f, 7.f, 9.f, 11.f};
    const vector<float> legacyDurations{1.f, 1.5f, 2.f, 3.f, 3.f, 5.f};
    auto inventory = InventoryManager::GetInstance();
    auto items = ItemManager::GetInstance();
    for (bool moving : {false, true}) {
        LumiaIsland scene;
        scene.RegisterPreparation();
        auto logic = scene.m_player->logic;
        auto animation = scene.m_player->animation;
        logic->current = moving ? PlayerStateType::Run : PlayerStateType::Wait;
        animation->current = moving ? AnimationStateType::Run : AnimationStateType::Wait;
        for (size_t index = 0; index < grades.size(); ++index) {
            auto result = make_shared<Item>(Item{grades[index]});
            items->items[101] = result;
            inventory->recipes = {make_shared<Recipe>(Recipe{101})};
            logic->craft->recipe = 4; // A previous non-first recipe must not leak into Z crafting.
            animation->craft->duration = -17.f;
            bool success = false;
            logic->OnTryCraft.Invoke(success);
            require(success, "Valid recipe preparation failed");
            require(animation->craft->duration == durations[index], "Upcoming craft animation did not receive the source grade duration");
            require(logic->craft->recipe == 0, "Upcoming craft retained a previous recipe index");
            require(logic->GetCurrentStatePtr()->recipe == 37, "Craft preparation overwrote current idle or movement state");
            require(animation->GetCurrentStatePtr()->duration == -17.f, "Craft duration was written to current idle or movement animation");
            require(scene.m_uiManager->gage->visible && scene.m_uiManager->gage->item == result, "Craft preview no longer reflects the prepared result");
            require(logic->requested.empty() && animation->requested.empty(), "Preparation bypassed the original input handler's state requests");
            animation->craft->duration = -17.f;
            logic->craft->recipe = 4;
            logic->OnTryCraftFirst.Invoke(success);
            require(success && animation->craft->duration == legacyDurations[index], "Legacy preparation changed its separate source timing policy");
            require(logic->craft->recipe == 0, "Legacy preparation targeted the wrong logical state");
            require(animation->GetCurrentStatePtr()->duration == -17.f && logic->GetCurrentStatePtr()->recipe == 37,
                "Legacy preparation changed the active state instead of the upcoming craft");
        }
    }
    inventory->recipes.clear();
    LumiaIsland empty;
    empty.RegisterPreparation();
    bool success = true;
    empty.m_player->logic->OnTryCraft.Invoke(success);
    require(!success && !empty.m_uiManager->gage->visible, "Missing ingredients activated crafting");
    require(empty.m_player->logic->craft->recipe == 37 && empty.m_player->animation->craft->duration == -17.f,
        "Rejected preparation changed craft state");
    success = true;
    empty.m_player->logic->OnTryCraftFirst.Invoke(success);
    require(!success && !empty.m_uiManager->gage->visible, "Legacy preparation accepted missing ingredients");

    // The pinned list button previews a chosen recipe; this fix must not wire
    // a new start action into that existing button as a side effect.
    CraftListPanelUI list;
    list.m_player = make_shared<Player>();
    list.m_gagePanel = make_shared<GagePanel>();
    items->items[102] = make_shared<Item>(Item{ITEMGRADE::RARE});
    list.m_craftableRecipes = {make_shared<Recipe>(Recipe{101}), make_shared<Recipe>(Recipe{102})};
    list.OnCraftSlotClicked(1);
    require(list.m_gagePanel->visible && list.m_gagePanel->item == items->items[102], "List preview selected the wrong recipe");
    require(list.m_player->logic->requested.empty() && list.m_player->animation->requested.empty(), "List preview unexpectedly started crafting");
    require(list.m_player->logic->craft->recipe == 37, "Preview overwrote a pending craft recipe");
}
'@
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('rcp-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $temporary 'main.cpp'), $fixture + "`n" + $registration + "`n" + ($methods -join "`n") + "`n" + $checks)
    [IO.File]::WriteAllText((Join-Path $temporary 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(CraftPreparation LANGUAGES CXX)`nadd_executable(craft_preparation main.cpp)`ntarget_compile_options(craft_preparation PRIVATE /utf-8)`n")
    $build = Join-Path $temporary 'build'
    & $CMakeExecutable -S $temporary -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Craft preparation fixture configuration failed' }
    & $CMakeExecutable --build $build --config Debug --target craft_preparation | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Craft preparation fixture compilation failed' }
    & (Join-Path $build 'Debug/craft_preparation.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Craft preparation behavior failed' }
    Write-Output 'PASS: actual preparation callbacks configure upcoming Craft from Wait/Run, preserve grade durations, reject missing ingredients and leave list previews unchanged'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^rcp-[0-9a-f]{8}$') { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
