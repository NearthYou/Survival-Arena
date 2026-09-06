# Apply only the explicitly requested presentation adjustment to a private copy.
function Add-ReferenceCollisionDebugToggle([string]$Text) {
    if ($Text.Contains('referenceCollisionDebugVisible')) { throw 'Collision debug toggle is already present' }
    $sites = [regex]::Matches($Text, 'void LumiaIsland::LateUpdate\(\)\s*\{\s*Super::LateUpdate\(\);')
    if ($sites.Count -ne 1) { throw 'Unexpected collision debug hook site' }
    $newline = if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $code = @'
    // Debug geometry only: leave collider activation and collision tests unchanged.
    static bool referenceCollisionDebugVisible = false;
    if (INPUT->GetButtonDown(static_cast<KEY_TYPE>(VK_F3)))
        referenceCollisionDebugVisible = !referenceCollisionDebugVisible;
    for (const auto& object : GetObjects())
        if (const auto collider = object->GetCollider())
            collider->SetVisible(referenceCollisionDebugVisible);
'@
    $code = $code.Replace("`r`n", "`n").Replace("`n", $newline)
    return $Text.Insert($sites[0].Index + $sites[0].Length, $newline + $code + $newline)
}

function Add-ReferenceCraftStatePreparation([string]$Text) {
    # Preparation precedes the queued Craft transition. Configure that target,
    # not the still-current idle/movement state. Keep both source timing policies.
    $replacements = @(
        @('m_player->GetPlayerStateMachine()->GetCurrentStatePtr()->SetRecipeIndex(',
          'm_player->GetPlayerStateMachine()->GetState(PlayerStateType::Craft)->SetRecipeIndex('),
        @('m_player->GetAnimationStateMachine()->GetCurrentStatePtr()->SetExpectedDuration(',
          'm_player->GetAnimationStateMachine()->GetState(AnimationStateType::Craft)->SetExpectedDuration(')
    )
    foreach ($pair in $replacements) {
        if ($Text.Split($pair[0],[StringSplitOptions]::None).Count -ne 3) { throw 'Unexpected craft preparation hook sites' }
        $Text = $Text.Replace($pair[0],$pair[1])
    }
    return $Text
}

function Add-ReferenceStableCharacterSelection([string]$Text) {
    $assignment = 'm_selectCharIdx = charindex - 1;'
    $transition = 'LumiaIslandScene->SetSelectedCharacter(m_selectCharIdx);'
    foreach ($site in @($assignment, $transition)) {
        if ($Text.Split($site, [StringSplitOptions]::None).Count -ne 2) {
            throw 'Unexpected character selection hook site'
        }
    }
    # Lobby cards are one-based; only the scene boundary converts to actor 0/1.
    # Confirming again must not consume that conversion a second time.
    return $Text.Replace($assignment, 'm_selectCharIdx = charindex;').Replace(
        $transition, 'LumiaIslandScene->SetSelectedCharacter(m_selectCharIdx == 2 ? 1 : 0);')
}

function Add-ReferenceIndependentSkinSelection([string]$Text) {
    $methods = [regex]::Matches($Text, '(?ms)^void CharacterSelectScene::UpdateSkinList\([^)]*\)\s*\{.*?^\}')
    if ($methods.Count -ne 1) { throw 'Unexpected skin list hook site' }
    $method = $methods[0]
    $calls = [regex]::Matches($method.Value, '(?m)^[\t ]*OnCharacterSelectButtonClicked\(i\);[\t ]*\r?\n')
    if ($calls.Count -ne 1) { throw 'Unexpected skin confirmation hook site' }
    # A skin index is not a character index and must not confirm quick start.
    $updated = $method.Value.Remove($calls[0].Index, $calls[0].Length)
    return $Text.Remove($method.Index, $method.Length).Insert($method.Index, $updated)
}

function Add-ReferencePickingMatrixSync([string]$Text) {
    if ($Text.Contains('Synchronize camera matrices before caching screen bounds')) { throw 'Picking matrix synchronization is already present' }
    $sites = [regex]::Matches($Text, 'void SceneObjectManager::UpdateQuadTree\(\)\s*\{')
    if ($sites.Count -ne 1) { throw 'Unexpected picking matrix hook site' }
    return $Text.Insert($sites[0].Index + $sites[0].Length,
        "`r`n    // Synchronize camera matrices before caching screen bounds.`r`n    GetMainCamera()->GetCamera()->UpdateMatrix();`r`n")
}

function Add-ReferencePlayerAnimationSync([string]$Text) {
    if ($Text.Contains('ReferenceAcceptedPlayerAnimation')) { throw 'Player animation synchronization is already present' }
    $include = '#include "PlayerStateMachine.h"'
    if ($Text.Split($include, [StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected animation synchronization include site' }
    $mapping = @'
static bool ReferenceAcceptedPlayerAnimation(const shared_ptr<GameObject>& object, AnimationStateType& animation)
{
    if (!object) return false;
    const auto player = object->GetPlayerStateMachine();
    if (!player) return false;
    switch (player->GetCurrentState())
    {
    case PlayerStateType::Wait: animation = AnimationStateType::Wait; break;
    case PlayerStateType::Run: animation = AnimationStateType::Run; break;
    case PlayerStateType::Skill_1: animation = AnimationStateType::Skill_1; break;
    case PlayerStateType::Skill_2: animation = AnimationStateType::Skill_2; break;
    case PlayerStateType::Skill_3: animation = AnimationStateType::Skill_3; break;
    case PlayerStateType::Skill_4: animation = AnimationStateType::Skill_4; break;
    case PlayerStateType::Craft: animation = AnimationStateType::Craft; break;
    case PlayerStateType::BaseAttack: animation = AnimationStateType::BaseAttack; break;
    case PlayerStateType::Counter: animation = AnimationStateType::Counter; break;
    default: return false;
    }
    return true;
}
'@
    $Text = $Text.Replace($include, $include + "`r`n`r`n" + $mapping.Replace("`r`n", "`n").Replace("`n", "`r`n"))
    $sites = [regex]::Matches($Text, 'bool AnimationStateMachine::CanChangeState\(AnimationStateType newState\)\s*\{')
    if ($sites.Count -ne 1) { throw 'Unexpected animation transition gate' }
    $gate = @'
    // A real accepted action can cancel the previous visual tail. Idle alone
    // does not truncate it, and duplicate requests do not restart a clip.
    AnimationStateType accepted;
    if (ReferenceAcceptedPlayerAnimation(GetGameObject(), accepted) && accepted == newState &&
        newState != AnimationStateType::Wait && newState != GetCurrentState() &&
        m_states.find(newState) != m_states.end())
        return true;
'@
    $Text = $Text.Insert($sites[0].Index + $sites[0].Length, "`r`n" + $gate.Replace("`r`n", "`n").Replace("`n", "`r`n") + "`r`n")
    $sites = [regex]::Matches($Text, 'void AnimationStateMachine::HandleAutoTransitions\(\)\s*\{')
    if ($sites.Count -ne 1) { throw 'Unexpected animation reconciliation site' }
    $sync = @'
    // Recover requests that arrived before the gameplay transition was applied.
    // Non-player machines retain their original transition behavior.
    AnimationStateType accepted;
    if (ReferenceAcceptedPlayerAnimation(GetGameObject(), accepted) && m_states.find(accepted) != m_states.end())
    {
        if (accepted != GetCurrentState() && CanChangeState(accepted))
            RequestStateChange(accepted);
        return;
    }
'@
    return $Text.Insert($sites[0].Index + $sites[0].Length, "`r`n" + $sync.Replace("`r`n", "`n").Replace("`n", "`r`n") + "`r`n")
}

function Add-ReferenceAnimationEpisodeHeader([string]$Text) {
    if ($Text.Contains('m_referenceAcceptedAnimation')) { throw 'Animation episode tracking is already present' }
    $site = 'AnimationStateType m_initialStateType;'
    if ($Text.Split($site,[StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected animation episode header hook' }
    $fields = @'

    // Stop recovering an accepted action after its animation has been observed.
    AnimationStateType m_referenceAcceptedAnimation = AnimationStateType::Wait;
    bool m_referenceHasAcceptedAnimation = false;
    bool m_referenceAnimationPending = false;
'@
    $newline = if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" }
    return $Text.Replace($site, $site + $fields.Replace("`r`n", "`n").Replace("`n", $newline))
}

function Add-ReferenceAnimationEpisodeSync([string]$Text) {
    if ($Text.Contains('m_referenceAcceptedAnimation')) { throw 'Animation episode synchronization is already present' }
    $methods = [regex]::Matches($Text, '(?ms)^void AnimationStateMachine::HandleAutoTransitions\(\)\s*\{.*?^\}')
    if ($methods.Count -ne 1) { throw 'Unexpected animation episode method hook' }
    $method = $methods[0]
    $blocks = [regex]::Matches($method.Value, '(?s)    AnimationStateType accepted;\s+if \(ReferenceAcceptedPlayerAnimation\(GetGameObject\(\), accepted\) && m_states.find\(accepted\) != m_states.end\(\)\)\s*\{.*?\r?\n    \}')
    if ($blocks.Count -ne 1) { throw 'Animation episode synchronization requires base player synchronization' }
    $replacement = @'
    AnimationStateType accepted;
    if (ReferenceAcceptedPlayerAnimation(GetGameObject(), accepted) && m_states.find(accepted) != m_states.end())
    {
        if (!m_referenceHasAcceptedAnimation || m_referenceAcceptedAnimation != accepted)
        {
            m_referenceAcceptedAnimation = accepted;
            m_referenceHasAcceptedAnimation = true;
            m_referenceAnimationPending = true;
        }
        // Completion events may lag the visual tail. Do not replay an action
        // whose accepted animation already ran while that event is pending.
        if (accepted == GetCurrentState())
            m_referenceAnimationPending = false;
        else if (m_referenceAnimationPending && CanChangeState(accepted))
            RequestStateChange(accepted);
        return;
    }
    m_referenceHasAcceptedAnimation = false;
    m_referenceAnimationPending = false;
'@
    $newline = if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $replacement = $replacement.Replace("`r`n", "`n").Replace("`n", $newline)
    $updated = $method.Value.Remove($blocks[0].Index, $blocks[0].Length).Insert($blocks[0].Index, $replacement)
    return $Text.Remove($method.Index, $method.Length).Insert($method.Index, $updated)
}

function Add-ReferenceWalkablePathSmoothing([string]$Text) {
    if ($Text.Contains('Cover the entire XZ segment')) { throw 'Walkable path smoothing is already present' }
    $sites = [regex]::Matches($Text, '(?ms)^bool NavMesh::IsLineOnNavMesh\([^\r\n]*\)\s*\{.*?^\}')
    if ($sites.Count -ne 1) { throw 'Unexpected navigation line test site' }
    $replacement = @'
bool NavMesh::IsLineOnNavMesh(const Vec3& start, const Vec3& end, float stepSize)
{
    // NavMeshAgent moves in XZ and resamples elevation on each step.
    // Cover the entire XZ segment with triangle interiors: sparse samples and
    // a one-unit padding can accept a corner that the agent cannot cross.
    (void)stepSize;
    vector<pair<double, double>> intervals;
    intervals.reserve(m_triangles.size());
    const double epsilon = 1e-7;
    for (const auto& triangle : m_triangles)
    {
        const auto& a = triangle.vertices[0];
        const auto& b = triangle.vertices[1];
        const auto& c = triangle.vertices[2];
        if (max(max(a.x, b.x), c.x) < min(start.x, end.x) - 0.001f ||
            min(min(a.x, b.x), c.x) > max(start.x, end.x) + 0.001f ||
            max(max(a.z, b.z), c.z) < min(start.z, end.z) - 0.001f ||
            min(min(a.z, b.z), c.z) > max(start.z, end.z) + 0.001f)
            continue;
        const double azcz = static_cast<double>(a.z) - c.z;
        const double bzcz = static_cast<double>(b.z) - c.z;
        const double axcx = static_cast<double>(a.x) - c.x;
        const double cxbx = static_cast<double>(c.x) - b.x;
        const double denominator = bzcz * axcx + cxbx * azcz;
        if (abs(denominator) < 1e-12) continue;
        const auto weightA = [&](const Vec3& point) {
            return (bzcz * (static_cast<double>(point.x) - c.x) + cxbx * (static_cast<double>(point.z) - c.z)) / denominator;
        };
        const auto weightB = [&](const Vec3& point) {
            return (-azcz * (static_cast<double>(point.x) - c.x) + axcx * (static_cast<double>(point.z) - c.z)) / denominator;
        };
        const double startA = weightA(start), endA = weightA(end);
        const double startB = weightB(start), endB = weightB(end);
        double first = 0, last = 1;
        const auto clip = [&](double initial, double final) {
            const double change = final - initial;
            if (abs(change) < 1e-12) return initial >= -epsilon;
            const double crossing = (-epsilon - initial) / change;
            if (change > 0) first = max(first, crossing);
            else last = min(last, crossing);
            return first <= last;
        };
        if (clip(startA, endA) && clip(startB, endB) && clip(1 - startA - startB, 1 - endA - endB))
            intervals.emplace_back(first, last);
    }
    sort(intervals.begin(), intervals.end());
    double covered = 0;
    for (const auto& interval : intervals)
    {
        if (interval.first > covered + epsilon) return false;
        covered = max(covered, interval.second);
        if (covered >= 1 - epsilon) return true;
    }
    return false;
}
'@
    $Text = $Text.Remove($sites[0].Index, $sites[0].Length).Insert($sites[0].Index,
        $replacement.Replace("`r`n", "`n").Replace("`n", "`r`n"))
    $shortCut = 'return GetDistance(start, end) < 0.1f || IsLineOnNavMesh(start, end);'
    if ($Text.Split($shortCut, [StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected navigation short-distance gate' }
    return $Text.Replace($shortCut, 'return IsLineOnNavMesh(start, end);')
}

function Add-ReferenceCombatAnimationSync([string]$Text) {
    if ($Text.Contains('Combat intent still includes movement to the target')) { throw 'Combat animation synchronization is already present' }
    $include = '#include "PlayerStateMachine.h"'
    if ($Text.Split($include, [StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected combat animation include site' }
    if (-not $Text.Contains('#include "NavMeshAgent.h"')) {
        $Text = $Text.Replace($include, $include + "`r`n" + '#include "NavMeshAgent.h"')
    }
    $site = 'case PlayerStateType::BaseAttack: animation = AnimationStateType::BaseAttack; break;'
    if ($Text.Split($site, [StringSplitOptions]::None).Count -ne 2) { throw 'Combat animation requires the accepted player-action mapping' }
    $replacement = @'
case PlayerStateType::BaseAttack:
    {
        // Combat intent still includes movement to the target.
        const auto navigation = object->GetNavMeshAgent();
        animation = navigation && navigation->IsMoving() ? AnimationStateType::Run : AnimationStateType::BaseAttack;
        break;
    }
'@
    return $Text.Replace($site, $replacement.Replace("`r`n", "`n").Replace("`n", "`r`n"))
}

function Add-ReferenceAudioSettingsHeader([string]$Text) {
    if ($Text.Contains('m_BGMGroup')) { throw 'Audio category groups are already present' }
    $site = 'FMOD::System* m_System;'
    if ($Text.Split($site,[StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected audio system declaration' }
    return $Text.Replace($site, $site + "`r`n    FMOD::ChannelGroup* m_BGMGroup = nullptr;`r`n    FMOD::ChannelGroup* m_SFXGroup = nullptr;")
}

function Add-ReferenceAudioSettings([string]$Text) {
    if ($Text.Contains('createChannelGroup("BGM"')) { throw 'Audio category routing is already present' }
    $site = 'LoadSoundFile();'
    if ($Text.Split($site,[StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected audio category initialization site' }
    $groups = @'
FMOD::ChannelGroup* master = nullptr;
    if (m_System->createChannelGroup("BGM", &m_BGMGroup) != FMOD_OK ||
        m_System->createChannelGroup("SFX", &m_SFXGroup) != FMOD_OK ||
        m_System->getMasterChannelGroup(&master) != FMOD_OK ||
        master->addGroup(m_BGMGroup) != FMOD_OK || master->addGroup(m_SFXGroup) != FMOD_OK)
        return E_FAIL;
    m_BGMGroup->setVolume(m_BGMvolume);
    m_SFXGroup->setVolume(m_SFXvolume);
    LoadSoundFile();
'@
    $Text = $Text.Replace($site, $groups.Replace("`r`n", "`n").Replace("`n", "`r`n"))
    foreach ($route in @(
        @('m_System->playSound(iter->second, 0, false, &m_Channels[_eID]);','m_System->playSound(iter->second, m_SFXGroup, false, &m_Channels[_eID]);'),
        @('m_System->playSound(iter->second, 0, false, &m_Channels[0]);','m_System->playSound(iter->second, m_BGMGroup, false, &m_Channels[0]);')
    )) {
        if ($Text.Split($route[0],[StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected category playback route' }
        $Text = $Text.Replace($route[0],$route[1])
    }
    $replacements = @{
        SetBGMVolume = @'
void SoundManager::SetBGMVolume(float _volume)
{
    m_BGMvolume = _volume;
    if (m_BGMGroup) m_BGMGroup->setVolume(_volume);
}
'@
        SetSFXVolume = @'
void SoundManager::SetSFXVolume(float _volume)
{
    m_SFXvolume = _volume;
    if (m_SFXGroup) m_SFXGroup->setVolume(_volume);
}
'@
        StopAll = @'
void SoundManager::StopAll()
{
    if (m_BGMGroup) m_BGMGroup->stop();
    if (m_SFXGroup) m_SFXGroup->stop();
}
'@
    }
    foreach ($name in $replacements.Keys) {
        $match = [regex]::Matches($Text,"(?ms)^void SoundManager::$name\([^\r\n]*\)\s*\{.*?^\}")
        if ($match.Count -ne 1) { throw "Unexpected audio setting method: $name" }
        $replacement = $replacements[$name].Replace("`r`n", "`n").Replace("`n", "`r`n")
        $Text = $Text.Remove($match[0].Index,$match[0].Length).Insert($match[0].Index,$replacement)
    }
    return $Text
}
