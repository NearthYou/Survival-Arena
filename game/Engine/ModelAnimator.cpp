#include "pch.h"
#include "ModelAnimator.h"
#include "Model.h"
#include "Material.h"
#include "Shader.h"
#include "ModelMesh.h"
#include "ModelAnimation.h"
#include "Camera.h"
#include "Light.h"

ModelAnimator::ModelAnimator(shared_ptr<Shader> _shader)
    : Super(ComponentType::Animator), m_shader(_shader)
{
    // FSM 구조로 변경되면서 상태 관리 변수들 제거
    // 단순히 애니메이션 재생에만 집중
}

ModelAnimator::~ModelAnimator()
{
    m_srv.Reset();
    m_texture.Reset();
}

void ModelAnimator::Update()
{
    // FSM에서 상태 관리를 담당하므로 여기서는 기본 업데이트만 수행
}

void ModelAnimator::UpdateTweenData()

{
    // 시퀀스 업데이트 (시퀀스 모드일 때만)
    if (m_isSequenceMode)
    {
        UpdateSequence();
    }

    // 트윈 데이터 업데이트
    UpdateTweenFrames();
}

void ModelAnimator::UpdateTweenFrames()
{
    TweenDesc& desc = m_tweenDesc;
    desc.m_curr.m_sumTime += DT;

    // 현재 애니메이션 프레임 업데이트
    UpdateCurrentAnimation();

    // 다음 애니메이션 블렌딩 처리
    if (desc.m_next.m_animIndex >= 0)
    {
        UpdateNextAnimation();
    }
}

void ModelAnimator::UpdateCurrentAnimation()
{
    TweenDesc& desc = m_tweenDesc;
    wstring currentTag = m_indexToTag[desc.m_curr.m_animIndex];
    shared_ptr<ModelAnimation> currentAnim = m_model->GetAnimationByTag(currentTag);

    if (!currentAnim) return;

    if (GetGameObject()->GetName().compare(L"Nicky") == 0)
    {
        wstring name = currentAnim->m_name;

        string n(name.begin(), name.end());

        if (INPUT->GetButtonDown(KEY_TYPE::S))
            cout << "현재 애니메이션 : " << n << endl;
    }

    float timePerFrame = 1.0f / (currentAnim->m_frameRate * desc.m_curr.m_speed);
    if (desc.m_curr.m_sumTime >= timePerFrame)
    {
        desc.m_curr.m_sumTime = 0;
        desc.m_curr.m_currFrame = (desc.m_curr.m_currFrame + 1) % currentAnim->m_frameCount;
        //cout << "현재 프레임 : " << desc.m_curr.m_currFrame << endl;
        desc.m_curr.m_nextFrame = (desc.m_curr.m_currFrame + 1) % currentAnim->m_frameCount;
    }

    desc.m_curr.m_ratio = desc.m_curr.m_sumTime / timePerFrame;
}

void ModelAnimator::UpdateNextAnimation()
{
    //TweenDesc& desc = m_tweenDesc;
    //desc.m_tweenSumTime += DT;
    //desc.m_tweenRatio = desc.m_tweenSumTime / desc.m_tweenDuration;

    //if (desc.m_tweenRatio >= 1.0f)
    //{
    //    // 블렌딩 완료
    //    desc.m_curr = desc.m_next;
    //    desc.ClearNextAnim();
    //    return;
    //}
    TweenDesc& desc = m_tweenDesc;
    // 블렌딩 속도 배수 적용
    desc.m_tweenSumTime += DT * m_tweenSpeedMultiplier;
    desc.m_tweenRatio = desc.m_tweenSumTime / desc.m_tweenDuration;

    if (desc.m_tweenRatio >= 1.0f)
    {
        // 블렌딩 완료
        desc.m_curr = desc.m_next;
        desc.ClearNextAnim();
        return;
    }

    // 블렌딩 중 - 다음 애니메이션 프레임 계산
    wstring nextTag = m_indexToTag[desc.m_next.m_animIndex];
    shared_ptr<ModelAnimation> nextAnim = m_model->GetAnimationByTag(nextTag);

    if (!nextAnim) return;

    desc.m_next.m_sumTime += DT;
    float timePerFrame = 1.0f / (nextAnim->m_frameRate * desc.m_next.m_speed);

    if (desc.m_next.m_ratio >= 1.0f)
    {
        desc.m_next.m_sumTime = 0;
        desc.m_next.m_currFrame = (desc.m_next.m_currFrame + 1) % nextAnim->m_frameCount;
        desc.m_next.m_nextFrame = (desc.m_next.m_currFrame + 1) % nextAnim->m_frameCount;
    }

    desc.m_next.m_ratio = desc.m_next.m_sumTime / timePerFrame;
}

void ModelAnimator::SetModel(shared_ptr<Model> _model)
{
    m_model = _model;

    // 첫 번째 머티리얼 설정
    const auto& materials = m_model->GetMaterials();
    for (auto& material : materials)
    {
        material->SetShader(m_shader);
        m_material = material;
        break;
    }

    // 태그-인덱스 매핑 생성
    CreateTagIndexMapping();
}

void ModelAnimator::CreateTagIndexMapping()
{
    m_tagToIndex.clear();
    m_indexToTag.clear();

    uint32 index = 0;
    for (const auto& pair : m_model->GetAnimations())
    {
        m_tagToIndex[pair.first] = index;
        m_indexToTag.push_back(pair.first);
        index++;
    }
}

// ============================================================================
// 애니메이션 제어 메서드들 (FSM에서 호출)
// ============================================================================

shared_ptr<Shader> ModelAnimator::GetShader()
{
    return m_material->GetShader();
}

void ModelAnimator::SetAnimation(uint32 _animIndex, bool _immediate)
{
    if (_animIndex >= m_model->GetAnimationCount())
        return;

    if (_immediate)
    {
        SetAnimationImmediate(_animIndex);
    }
    else
    {
        SetNextAnimation(_animIndex);
    }
}

void ModelAnimator::SetAnimationImmediate(uint32 _animIndex)
{
    m_tweenDesc.m_curr.m_animIndex = _animIndex;
    m_tweenDesc.m_curr.m_currFrame = 0;
    m_tweenDesc.m_curr.m_nextFrame = 1;
    m_tweenDesc.m_curr.m_sumTime = 0.0f;
    m_tweenDesc.m_curr.m_ratio = 0.0f;
    m_tweenDesc.ClearNextAnim();
}

void ModelAnimator::SetAnimationByTag(const wstring& _tag, bool _immediate)
{
    auto it = m_tagToIndex.find(_tag);
    if (it != m_tagToIndex.end())
    {
        SetAnimation(it->second, _immediate);
    }
}

void ModelAnimator::SetNextAnimation(uint32 _animIndex, bool _tweenDuration)
{
    if (_animIndex >= m_model->GetAnimationCount())
        return;

    m_tweenDesc.m_next.m_animIndex = _animIndex;
    m_tweenDesc.m_next.m_currFrame = 0;
    m_tweenDesc.m_next.m_nextFrame = 1;
    m_tweenDesc.m_next.m_sumTime = 0.0f;
    m_tweenDesc.m_next.m_ratio = 0.0f;
    m_tweenDesc.m_tweenDuration = _tweenDuration;
    m_tweenDesc.m_tweenSumTime = 0.0f;
    m_tweenDesc.m_tweenRatio = 0.0f;
}

void ModelAnimator::SetNextAnimationByTag(const wstring& _tag, bool _tweenDuration)
{
    auto it = m_tagToIndex.find(_tag);
    if (it != m_tagToIndex.end())
    {
        SetNextAnimation(it->second, _tweenDuration);
    }
}

// ModelAnimator.cpp에 구현
void ModelAnimator::SetCurrentAnimationSpeed(float _speed)
{
    m_tweenDesc.m_curr.m_speed = _speed;
}

void ModelAnimator::SetNextAnimationSpeed(float _speed)
{
    if (m_tweenDesc.m_next.m_animIndex >= 0)
        m_tweenDesc.m_next.m_speed = _speed;
}


void ModelAnimator::SetAnimationSpeed(float _speed)
{
    m_tweenDesc.m_curr.m_speed = _speed;
    if (m_tweenDesc.m_next.m_animIndex >= 0)
        m_tweenDesc.m_next.m_speed = _speed;
}

// ============================================================================
// 애니메이션 시퀀스 관리 (FSM State에서 사용)
// ============================================================================

void ModelAnimator::CreateSequence(const wstring& name, const vector<wstring>& animTags, bool loop)
{
    CreateSequence(name, animTags, vector<float>(), loop);
}

void ModelAnimator::CreateSequence(const wstring& name, const vector<wstring>& animTags,
    const vector<float>& durations, bool loop)
{
    AnimationSequence sequence;
    sequence.name = name;
    sequence.animationTags = animTags;
    sequence.animationDurations = durations;
    sequence.isLoop = loop;
    sequence.currentIndex = 0;
    sequence.currentTime = 0.0f;
    sequence.isPlaying = false;

    m_sequences[name] = sequence;
}

void ModelAnimator::PlaySequence(const wstring& name)
{
    auto it = m_sequences.find(name);
    if (it == m_sequences.end())
        return;

    StopSequence();

    m_currentSequence = &it->second;
    m_currentSequence->isPlaying = true;
    m_currentSequence->currentIndex = 0;
    m_currentSequence->currentTime = 0.0f;
    m_isSequenceMode = true;

    if (!m_currentSequence->animationTags.empty())
    {
        SetAnimationByTag(m_currentSequence->animationTags[0], true);
    }
}

void ModelAnimator::StopSequence()
{
    if (m_currentSequence)
    {
        m_currentSequence->isPlaying = false;
        m_currentSequence->currentIndex = 0;
        m_currentSequence->currentTime = 0.0f;
    }

    m_currentSequence = nullptr;
    m_isSequenceMode = false;
    m_tweenDesc.ClearNextAnim();

    // Wait 애니메이션으로 리셋
    ResetToWaitAnimation();
}

void ModelAnimator::ResetToWaitAnimation()
{
    if (GetGameObject()->GetName().compare(L"Nicky") == 0)
    {
        int a = 0;
    }


    auto waitIt = m_tagToIndex.find(L"Wait");
    if (waitIt != m_tagToIndex.end())
    {
        m_tweenDesc.m_curr.m_animIndex = waitIt->second;
        m_tweenDesc.m_curr.m_currFrame = 0;
        m_tweenDesc.m_curr.m_nextFrame = 1;
        m_tweenDesc.m_curr.m_sumTime = 0.0f;
        m_tweenDesc.m_curr.m_ratio = 0.0f;
    }
}

void ModelAnimator::UpdateSequence()
{
    if (!m_isSequenceMode || !m_currentSequence || !m_currentSequence->isPlaying)
        return;

    if (m_currentSequence->animationTags.empty())
        return;

    float currentAnimDuration = GetCurrentSequenceDuration();
   
    m_currentSequence->currentTime += DT;
    //cout << "이건가 : " << m_currentSequence->currentTime << endl;
    if (m_currentSequence->currentTime >= currentAnimDuration)
    {
        TransitionToNextInSequence();
    }
}

float ModelAnimator::GetCurrentSequenceDuration()
{
    float currentAnimDuration = m_currentSequence->GetCurrentAnimationDuration();
   
    if (currentAnimDuration < 0.0f)
    {
        const wstring& currentAnimTag = m_currentSequence->animationTags[m_currentSequence->currentIndex];
        currentAnimDuration = GetAnimationDuration(currentAnimTag);
    }

    return currentAnimDuration;
}

void ModelAnimator::TransitionToNextInSequence()
{
    if (!m_currentSequence)
        return;

    uint32 nextIndex = m_currentSequence->currentIndex + 1;

    if (nextIndex >= m_currentSequence->animationTags.size())
    {
        if (m_currentSequence->isLoop)
        {
            nextIndex = 0;
        }
        else
        {
            CompleteSequence();
            return;
        }
    }

    m_currentSequence->currentIndex = nextIndex;
    m_currentSequence->currentTime = 0.0f;

    const wstring& nextAnimTag = m_currentSequence->animationTags[nextIndex];
    SetAnimationByTag(nextAnimTag, true);
}

void ModelAnimator::CompleteSequence()
{
    if (m_currentSequence->onComplete)
    {
        m_currentSequence->onComplete();
    }

    StopSequence();
}

// ============================================================================
// 렌더링 관련
// ============================================================================

void ModelAnimator::RenderInstancing(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech)
{
    if (!m_model || !Super::Render(_isShadowTech))
        return;

    // 텍스처 생성 (최초 한 번)
    if (!m_texture)
        CreateTexture();

    // 변환 맵 설정
    m_shader->GetSRV("TransformMap")->SetResource(m_srv.Get());

    // 본 데이터 설정
    SetupBoneData();

    // 메시 렌더링
    RenderMeshes(_buffer, _isShadowTech);
}

void ModelAnimator::RenderInstancingDeferred(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech)
{
    if (m_model == nullptr)
        return;

    // 기존 검증 로직 완전 활용
    if (Super::Render(_isShadowTech) == false)
        return;

    // 텍스처 생성 (최초 한 번)
    if (!m_texture)
        CreateTexture();

    // 변환 맵 설정
    m_shader->GetSRV("TransformMap")->SetResource(m_srv.Get());

    // 본 데이터 설정
    SetupBoneData();

    // 메시 렌더링
    RenderMeshes(_buffer, _isShadowTech);
}

void ModelAnimator::SetupBoneData()
{
    BoneDesc boneDesc;
    const uint32 boneCount = m_model->GetBoneCount();

    for (uint32 i = 0; i < boneCount; ++i)
    {
        shared_ptr<ModelBone> bone = m_model->GetBoneByIndex(i);
        boneDesc.transforms[i] = bone->m_transform;
    }

    m_shader->PushBoneData(boneDesc);

  
}

void ModelAnimator::RenderMeshes(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech)
{
    auto tint = m_shader->GetVector("ActorTint");
    if (tint && tint->IsValid()) tint->SetFloatVector(&m_displayTint.x);
    if (GRAPHICS->IsCurrentPassGeometry()) {
        //m_shader->SetTechnique(L"GBufferTech");
    }
    else {
        //m_shader->SetTechnique(L"T0");
    }

    const auto& meshes = m_model->GetMeshes();

    for (auto& mesh : meshes)
    {
        if (mesh->m_material)
            mesh->m_material->Update();

        m_shader->GetScalar("BoneIndex")->SetInt(mesh->m_boneIndex);

        mesh->m_vertexBuffer->PushData();
        mesh->m_indexBuffer->PushData();
        _buffer->PushData();

        if (GRAPHICS->IsCurrentPassGeometry()) {
            m_shader->DrawIndexedInstancedCurTech(m_pass,
                mesh->m_indexBuffer->GetCount(),
                _buffer->GetCount());
        }
        else {
            m_shader->DrawIndexedInstanced(GET_TECH(_isShadowTech), m_pass,
                mesh->m_indexBuffer->GetCount(),
                _buffer->GetCount());
        }
    }
}

InstanceID ModelAnimator::GetInstanceID()
{
    return make_pair((uint64)m_model.get(), m_displayTint == Vec4(1,1,1,1) ? (uint64)m_shader.get() : (uint64)this);
}

// ============================================================================
// 텍스처 생성 (기존 코드와 동일)
// ============================================================================

void ModelAnimator::CreateTexture()
{
    if (m_model->GetAnimationCount() == 0)
        return;

    uint32 maxFrameCount = 0;

    for (const auto& pair : m_model->GetAnimations())
    {
        maxFrameCount = max(maxFrameCount, pair.second->m_frameCount);
        CreateAnimationTransform(pair.first);
    }

    // Create Texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
    desc.Width = MAX_BONE_TRANSFORMS * 4;
    desc.Height = maxFrameCount;
    desc.ArraySize = m_model->GetAnimationCount();
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;

    const uint32 dataSize = MAX_BONE_TRANSFORMS * sizeof(Matrix);
    const uint32 pageSize = dataSize * maxFrameCount;

    void* mallocPtr = ::malloc(pageSize * m_model->GetAnimationCount());

    uint32 animIndex = 0;
    for (const auto& pair : m_model->GetAnimations())
    {
        uint32 startOffset = animIndex * pageSize;
        BYTE* pageStartPtr = reinterpret_cast<BYTE*>(mallocPtr) + startOffset;

        for (uint32 f = 0; f < maxFrameCount; f++)
        {
            void* ptr = pageStartPtr + dataSize * f;
            ::memcpy(ptr, m_animTransform[pair.first].transforms[f].data(), dataSize);
        }
        animIndex++;
    }

    vector<D3D11_SUBRESOURCE_DATA> subResources(m_model->GetAnimationCount());

    for (uint32 c = 0; c < m_model->GetAnimationCount(); c++)
    {
        void* ptr = (BYTE*)mallocPtr + c * pageSize;
        subResources[c].pSysMem = ptr;
        subResources[c].SysMemPitch = dataSize;
        subResources[c].SysMemSlicePitch = pageSize;
    }

    HRESULT hr = DEVICE->CreateTexture2D(&desc, subResources.data(), m_texture.GetAddressOf());
    CHECK(hr);

    ::free(mallocPtr);

    // Create SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ArraySize = m_model->GetAnimationCount();

    hr = DEVICE->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srv.GetAddressOf());
    CHECK(hr);
}

void ModelAnimator::CreateAnimationTransform(const wstring& _tag)
{
    vector<Matrix> tempAnimBoneTransforms(MAX_BONE_TRANSFORMS, Matrix::Identity);
    shared_ptr<ModelAnimation> animation = m_model->GetAnimationByTag(_tag);

    if (!animation) return;

    for (uint32 frame = 0; frame < animation->m_frameCount; ++frame)
    {
        for (uint32 bone = 0; bone < m_model->GetBoneCount(); ++bone)
        {
            shared_ptr<ModelBone> tbone = m_model->GetBoneByIndex(bone);
            Matrix matAnimation = Matrix::Identity;

            shared_ptr<ModelKeyframe> tframe = animation->GetKeyframe(tbone->m_name);
            if (tframe != nullptr)
            {
                ModelKeyframeData& data = tframe->m_transforms[frame];

                Matrix S = Matrix::CreateScale(data.m_scale.x, data.m_scale.y, data.m_scale.z);
                Matrix R = Matrix::CreateFromQuaternion(data.m_rotation);
                Matrix T = Matrix::CreateTranslation(data.m_translation.x, data.m_translation.y, data.m_translation.z);

                matAnimation = S * R * T;
            }

            Matrix matParent = Matrix::Identity;
            if (tbone->m_parentIndex >= 0)
                matParent = tempAnimBoneTransforms[tbone->m_parentIndex];

            tempAnimBoneTransforms[bone] = matAnimation * matParent;
            m_animTransform[_tag].transforms[frame][bone] = tbone->m_offsetMatrix * tempAnimBoneTransforms[bone];
        }
    }


}

// ============================================================================
// 유틸리티 메서드들
// ============================================================================

uint32 ModelAnimator::GetCurrentAnimationIndex() const
{
    return m_tweenDesc.m_curr.m_animIndex;
}

wstring ModelAnimator::GetCurrentAnimationTag() const
{
    if (m_tweenDesc.m_curr.m_animIndex < m_indexToTag.size())
        return m_indexToTag[m_tweenDesc.m_curr.m_animIndex];
    return L"";
}

bool ModelAnimator::IsAnimationTransitioning() const
{
    return m_tweenDesc.m_next.m_animIndex >= 0;
}

bool ModelAnimator::IsAnimationFinished() const
{
    if (m_tweenDesc.m_next.m_animIndex >= 0)
        return false;

    wstring currentTag = m_indexToTag[m_tweenDesc.m_curr.m_animIndex];
    shared_ptr<ModelAnimation> currentAnim = m_model->GetAnimationByTag(currentTag);

    if (currentAnim)
    {
        return m_tweenDesc.m_curr.m_currFrame >= currentAnim->m_frameCount - 1;
    }

    return true;
}

uint32 ModelAnimator::GetAnimationIndexByTag(const wstring& _tag)
{
    auto it = m_tagToIndex.find(_tag);
    return (it != m_tagToIndex.end()) ? it->second : 0;
}

float ModelAnimator::GetAnimationDuration(const wstring& animTag) const
{
    shared_ptr<ModelAnimation> anim = m_model->GetAnimationByTag(animTag);
    if (!anim)
        return 0.0f;

    return static_cast<float>(anim->m_frameCount) / anim->m_frameRate;
}

void ModelAnimator::SetSequenceCompleteCallback(const wstring& name, function<void()> callback)
{
    auto it = m_sequences.find(name);
    if (it != m_sequences.end())
    {
        it->second.onComplete = callback;
    }
}

void ModelAnimator::SetSequenceAnimationDuration(const wstring& sequenceName, uint32 animIndex, float duration)
{
    auto it = m_sequences.find(sequenceName);
    if (it == m_sequences.end())
        return;

    AnimationSequence& sequence = it->second;

    if (sequence.animationDurations.size() <= animIndex)
    {
        sequence.animationDurations.resize(animIndex + 1, -1.0f);
    }

    sequence.animationDurations[animIndex] = duration;
}

float ModelAnimator::GetSequenceAnimationDuration(const wstring& sequenceName)
{
    auto it = m_sequences.find(sequenceName);
    if (it == m_sequences.end())
        return -1.f;

    AnimationSequence& sequence = it->second;

    float sum = 0.f;
    for (size_t i = 0; i < sequence.animationDurations.size(); i++)
        sum += sequence.animationDurations[i];

    return sum;
}

vector<float> ModelAnimator::GetSequenceAnimationDurations(const wstring& sequenceName)
{
    auto it = m_sequences.find(sequenceName);
    if (it == m_sequences.end())
        return vector<float>();

    AnimationSequence& sequence = it->second;

    return sequence.animationDurations;
}

void ModelAnimator::SetSequenceAnimationDurations(const wstring& sequenceName, const vector<float>& durations)
{
    auto it = m_sequences.find(sequenceName);
    if (it != m_sequences.end())
    {
        it->second.animationDurations = durations;
    }
}

float ModelAnimator::GetCorrectedFrameRate(const wstring& animTag, float speed)
{
    shared_ptr<ModelAnimation> anim = m_model->GetAnimationByTag(animTag);
    if (!anim)
        return 25.0f;

    return anim->m_frameRate * speed;
}

float ModelAnimator::GetTimePerFrame(const wstring& animTag, float speed)
{
    return 1.0f / GetCorrectedFrameRate(animTag, speed);
}




// ModelAnimator.cpp에 구현
Vec3 ModelAnimator::GetAnimatedBonePosition(const wstring& boneName)
{
    Matrix transform = GetAnimatedBoneTransform(boneName);
    return Vec3(transform._41, transform._42, transform._43);
}

Matrix ModelAnimator::GetAnimatedBoneTransform(const wstring& boneName)
{
    shared_ptr<ModelBone> bone = m_model->GetBoneByName(boneName);
    if (!bone)
        return Matrix::Identity;

    // 현재 애니메이션 태그
    wstring currentTag = GetCurrentAnimationTag();

    // 현재 프레임과 다음 프레임
    uint32 currentFrame = m_tweenDesc.m_curr.m_currFrame;
    uint32 nextFrame = m_tweenDesc.m_curr.m_nextFrame;
    float ratio = m_tweenDesc.m_curr.m_ratio;

    // 이미 계산된 변환 데이터에서 가져오기
    auto it = m_animTransform.find(currentTag);
    if (it == m_animTransform.end())
        return Matrix::Identity;

    const auto& transforms = it->second.transforms;

    // 프레임 보간
    Matrix currentTransform = transforms[currentFrame][bone->m_index];
    Matrix nextTransform = transforms[nextFrame][bone->m_index];

    // 단순 선형 보간 (위치만)
    Vec3 currentPos(currentTransform._41, currentTransform._42, currentTransform._43);
    Vec3 nextPos(nextTransform._41, nextTransform._42, nextTransform._43);
    Vec3 interpolatedPos = Vec3::Lerp(currentPos, nextPos, ratio);

    Matrix result = currentTransform;
    result._41 = interpolatedPos.x;
    result._42 = interpolatedPos.y;
    result._43 = interpolatedPos.z;

    return result;
}