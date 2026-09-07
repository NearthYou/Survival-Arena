#pragma once
#include "Renderer.h"
#include "ModelAnimation.h"

// ============================================================================
// 전방 선언 및 구조체
// ============================================================================

class Model;

// 애니메이션 변환 구조체
struct AnimTransform
{
    using TransformArrayType = array<Matrix, MAX_BONE_TRANSFORMS>;
    array<TransformArrayType, MAX_MODEL_KEYFRAMES> transforms;
};

// 애니메이션 시퀀스 구조체
struct AnimationSequence
{
    wstring name;                           // 시퀀스 이름
    vector<wstring> animationTags;          // 애니메이션 태그 목록
    vector<float> animationDurations;      // 각 애니메이션별 재생시간
    uint32 currentIndex = 0;                // 현재 재생 중인 애니메이션 인덱스
    float currentTime = 0.0f;               // 현재 애니메이션 재생 시간
    bool isPlaying = false;                 // 시퀀스 재생 중인지
    bool isLoop = false;                    // 시퀀스 반복 재생 여부
    function<void()> onComplete = nullptr;  // 완료 콜백

    // 현재 애니메이션의 재생시간 반환 (커스텀 또는 기본값)
    float GetCurrentAnimationDuration() const
    {
        if (currentIndex < animationDurations.size())
            return animationDurations[currentIndex];
        return -1.0f; // 기본값 사용
    }
};

// ============================================================================
// ModelAnimator 클래스 - FSM 구조 적용 후 정리
// ============================================================================

class ModelAnimator : public Renderer
{
    using Super = Renderer;

public:
    ModelAnimator(shared_ptr<Shader> _shader);
    ~ModelAnimator();

    // ============================================================================
    // 핵심 애니메이션 기능
    // ============================================================================
    virtual void Update() override;
    void UpdateTweenData();
    void SetModel(shared_ptr<Model> _model);
    shared_ptr<Model> GetModel() { return m_model; }
    void SetDisplayTint(const Vec4& tint) { m_displayTint = tint; }
    Vec4 GetDisplayTint() const { return m_displayTint; }
    void SetPass(uint8 _pass) { m_pass = _pass; }
    shared_ptr<Shader> GetShader();

    // ============================================================================
    // 애니메이션 제어 메서드들 (FSM에서 호출)
    // ============================================================================
    void SetAnimation(uint32 _animIndex, bool _immediate = false);
    void SetAnimationByTag(const wstring& _tag, bool _immediate = false);
    void SetNextAnimation(uint32 _animIndex, bool _tweenDuration = 1.0f);
    void SetNextAnimationByTag(const wstring& _tag, bool _tweenDuration = 1.0f);
    // ModelAnimator.cpp에 구현
    void SetCurrentAnimationSpeed(float _speed);
    void SetNextAnimationSpeed(float _speed);

    void SetAnimationSpeed(float _speed);

    // ============================================================================
    // 애니메이션 상태 조회
    // ============================================================================
    uint32 GetCurrentAnimationIndex() const;
    wstring GetCurrentAnimationTag() const;
    float GetAnimationDuration(const wstring& animTag) const;
    bool IsAnimationTransitioning() const;
    bool IsAnimationFinished() const;

    // ============================================================================
    // 시퀀스 관리 (FSM State에서 사용)
    // ============================================================================
    void CreateSequence(const wstring& name, const vector<wstring>& animTags, bool loop = false);
    void CreateSequence(const wstring& name, const vector<wstring>& animTags, const vector<float>& durations, bool loop = false);
    void PlaySequence(const wstring& name);
    void StopSequence();
    bool IsSequencePlaying() const { return m_currentSequence != nullptr && m_currentSequence->isPlaying; }
    void SetSequenceCompleteCallback(const wstring& name, function<void()> callback);

    // ============================================================================
    // 렌더링 관련
    // ============================================================================
    void RenderInstancing(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech);
    void RenderInstancingDeferred(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech);

    InstanceID GetInstanceID();
    TweenDesc GetTweenDesc() { return m_tweenDesc; }

    // ============================================================================
    // 유틸리티 메서드들
    // ============================================================================
    float GetCorrectedFrameRate(const wstring& animTag, float speed = 1.0f);
    float GetTimePerFrame(const wstring& animTag, float speed = 1.0f);
    void SetSequenceAnimationDuration(const wstring& sequenceName, uint32 animIndex, float duration);
    float GetSequenceAnimationDuration(const wstring& sequenceName);
    vector<float> GetSequenceAnimationDurations(const wstring& sequenceName);

    void SetSequenceAnimationDurations(const wstring& sequenceName, const vector<float>& durations);

private:
    Vec4 m_displayTint = Vec4(1,1,1,1);
    // ============================================================================
    // 내부 구현 메서드들
    // ============================================================================

    // 텍스처 생성 관련
    void CreateTexture();
    void CreateAnimationTransform(const wstring& _tag);

    // 태그 매핑 관련
    void CreateTagIndexMapping();
    uint32 GetAnimationIndexByTag(const wstring& _tag);

    // 트윈 업데이트 관련
    void UpdateTweenFrames();
    void UpdateCurrentAnimation();
    void UpdateNextAnimation();

    // 렌더링 관련
    void SetupBoneData();
    void RenderMeshes(shared_ptr<class InstancingBuffer>& _buffer, bool _isShadowTech);

    // 시퀀스 관련
    void UpdateSequence();
    //bool IsCurrentAnimationInSequenceFinished();
    void TransitionToNextInSequence();
    float GetCurrentSequenceDuration();
    void CompleteSequence();
    void ResetToWaitAnimation();

    // 애니메이션 설정 관련
    void SetAnimationImmediate(uint32 _animIndex);

    //bool IsCurrentAnimationInSequenceFinished();
private:
    // ============================================================================
    // 멤버 변수들
    // ============================================================================

    // 셰이더 및 모델 관련
    shared_ptr<Shader> m_shader;
    shared_ptr<Model> m_model;
    uint8 m_pass = 0;

    // 애니메이션 데이터
    unordered_map<wstring, AnimTransform> m_animTransform;
    ComPtr<ID3D11Texture2D> m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;

    // 태그-인덱스 매핑
    unordered_map<wstring, uint32> m_tagToIndex;
    vector<wstring> m_indexToTag;

    // 트윈 데이터
    TweenDesc m_tweenDesc;

    // 시퀀스 관련
    unordered_map<wstring, AnimationSequence> m_sequences;
    AnimationSequence* m_currentSequence = nullptr;
    bool m_isSequenceMode = false;

private:
    bool m_hasUpdatedThisFrame = false;  // 이번 프레임에 업데이트했는지 확인
    uint32 m_lastUpdateFrame = 0;        // 마지막 업데이트 프레임 번호

public:
    Vec3 GetAnimatedBonePosition(const wstring& boneName);
    Matrix GetAnimatedBoneTransform(const wstring& boneName);

private:
    float m_tweenSpeedMultiplier = 2.0f;

public:
    void SetTweenSpeed(float speedMultiplier) { m_tweenSpeedMultiplier = speedMultiplier; }
    float GetTweenSpeed() const { return m_tweenSpeedMultiplier; }
};
