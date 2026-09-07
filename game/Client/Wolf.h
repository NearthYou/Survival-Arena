#pragma once
#include "Monster.h"
class Wolf :
    public Monster
{
    using Super = Monster;
public:
    Wolf(shared_ptr<Shader> _shader);
    virtual ~Wolf();

public:
    virtual void Start() override;
    virtual void Update() override;
    virtual void LateUpdate() override;
    virtual void FixedUpdate() override;

    //Collision ฐüทร
    virtual void OnCollision(shared_ptr<GameObject> _other) override;
    virtual void OnCollisionEnter(shared_ptr<GameObject> _other) override;
    virtual void OnCollisionExit(shared_ptr<GameObject> _other) override;

private:
    void UpdateState();

private:
    void InitWolfModel();
    void InitWolfAnimation();
    void InitWolfComponent();
    void InitWolfAI();
    void InitWolfMSM();
    void InitWolfStats();
};

