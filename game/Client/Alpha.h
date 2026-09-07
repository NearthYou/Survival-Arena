#pragma once

#include "Monster.h"
class AlphaSkill;
class Alpha :
    public Monster
{
    using Super = Monster;
public:
    Alpha(shared_ptr<Shader> _shader);
    virtual ~Alpha();

public:
    virtual void Start() override;
    virtual void Update() override;
    virtual void LateUpdate() override;
    virtual void FixedUpdate() override;

    //Collision ฐüทร
    virtual void OnCollision(shared_ptr<GameObject> _other) override;
    virtual void OnCollisionEnter(shared_ptr<GameObject> _other) override;
    virtual void OnCollisionExit(shared_ptr<GameObject> _other) override;

public:
    void PlaySkill(shared_ptr<GameObject> _target);
private:
    void UpdateState();

private:
    void InitAlphaModel();
    void InitAlphaAnimation();
    void InitAlphaMSM();
    void InitAlphaComponent();
    
    void InitAlphaAI();
    void InitAlphaStats();

private:
    shared_ptr<AlphaSkill> m_skill;

};