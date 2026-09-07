#include <cstdlib>
#include <iostream>
#include "BiancaESkillCircle.h"
#include "OriginalSkillCollisionMethods.inc"

void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << std::endl; std::exit(1); }
}

namespace ConeCollision {
enum class OBJECTTYPE { PLAYER, MONSTER, PROP };
struct Vec3 { float y = 0; };
struct Transform { Vec3 GetPosition() const { return {}; } };
struct GameObject {
    virtual ~GameObject() = default;
    virtual void OnCollisionEnter(std::shared_ptr<GameObject>) {}
    OBJECTTYPE GetType() const { return type; }
    Transform* GetTransform() { return &transform; }
    OBJECTTYPE type = OBJECTTYPE::PROP;
    Transform transform;
};
struct Player : GameObject {
    struct Status { float hitAttack = 100; };
    Status GetStatus() const { return {}; }
};
struct Monster : GameObject {
    int hp = 300;
    void Damaged(std::shared_ptr<GameObject>, int damage) { hp -= damage; }
};
struct Sound { void PlaySound(const wchar_t*, int, float) {} } sound;
#define SOUND (&sound)
class BiancaQCone : public GameObject {
    using Super = GameObject;
public:
    explicit BiancaQCone(std::shared_ptr<GameObject> owner) : m_Owner(owner) {}
    void OnCollisionEnter(std::shared_ptr<GameObject>) override;
    std::shared_ptr<GameObject> m_Owner;
    std::shared_ptr<Player> m_targetPlayer;
    std::shared_ptr<Monster> m_targetMonster;
    bool m_isBind = false, debugFlag = false;
    float m_startY = 0, m_endY = 0, m_upElapsedTime = 0;
};
#include "OriginalConeCollisionMethod.inc"
#undef SOUND

void check() {
    const auto owner = std::make_shared<Player>();
    owner->type = OBJECTTYPE::PLAYER;
    const auto enemy = std::make_shared<Monster>();
    enemy->type = OBJECTTYPE::MONSTER;
    BiancaQCone cone(owner);
    cone.OnCollisionEnter(owner);
    require(!cone.m_isBind, "Q bound to its caster");
    cone.OnCollisionEnter(enemy);
    require(enemy->hp == 180, "Q did not damage the monster");
    cone.OnCollisionEnter(std::make_shared<GameObject>());
    require(enemy->hp == 180, "A prop collision repeated Q damage");
    const auto otherPlayer = std::make_shared<Player>();
    otherPlayer->type = OBJECTTYPE::PLAYER;
    cone.OnCollisionEnter(otherPlayer);
    cone.OnCollisionEnter(nullptr);
    require(enemy->hp == 180, "A non-monster collision reused an old Q target");
}
}

int main() {
    ConeCollision::check();
    BiancaESkillCircle circle;
    const auto enemy = std::make_shared<GameObject>();
    circle.DamageFlag(true);
    circle.OnCollision(enemy);
    circle.LateUpdate();
    require(circle.GetCollisionObjects().count(enemy) == 1, "Collision phase did not publish a target");
    circle.Update();
    require(circle.GetCollisionObjects().count(enemy) == 1,
        "Updating the effect before the player erased the pending skill hit");
    circle.LateUpdate();
    require(circle.GetCollisionObjects().empty(), "An enemy that left remained in the next collision snapshot");
    circle.OnCollision(enemy);
    circle.LateUpdate();
    circle.DamageFlag(false);
    require(circle.GetCollisionObjects().empty(), "Disabling damage retained stale targets");
    circle.OnCollision(enemy);
    circle.LateUpdate();
    require(circle.GetCollisionObjects().empty(), "Disabled damage collected a target");
    circle.DamageFlag(true);
    require(circle.GetCollisionObjects().empty(), "A new cast inherited old targets");
}
