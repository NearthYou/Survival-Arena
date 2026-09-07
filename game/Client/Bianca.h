#pragma once
#include "Player.h"

class Shader;
class Bianca :
    public Player
{
    using Super = Player;
public:
    Bianca(shared_ptr<Shader> _defaultShader);
    virtual ~Bianca();
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
    void InitBiancaModel();
    void InitBiancaAnimation();
    void InitBiancaPSM();
    void InitBiancaComponent();
    void InitBiancaSkill();
    void InitBiancaStats();
public:
    virtual void Birth() override;
    virtual void Death() override;
    virtual void MakeItem() override;
    virtual void MakeFood() override;
};

