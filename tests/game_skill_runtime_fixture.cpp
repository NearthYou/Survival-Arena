#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>
using namespace std;
struct XMVECTOR {};
struct Texture {};
enum class PlayerStateType { Wait,Run,Skill_1,Skill_2,Skill_3,Skill_4 };
struct StateMachine { PlayerStateType state=PlayerStateType::Wait; PlayerStateType GetCurrentState() const { return state; } };
struct Status { int stamina=0; float cooldownReduction=0; };
struct Player { Status status; StateMachine machine; Status& GetStatus() { return status; } void SetStamina(int value) { status.stamina=value; } StateMachine* GetPlayerStateMachine() { return &machine; } };
struct Sound { void PlaySound(const wstring&,int,float) {} } sound;
#define SOUND (&sound)
float delta=0.1f;
#define DT delta
#include "BaseSkill.h"
#include "SkillProgression.h"
#include "OriginalSkillRuntimeMethods.inc"
struct Skill:BaseSkill {
    int played=0;
    explicit Skill(shared_ptr<Player> player):BaseSkill(player,2) { m_skillCooldown=5; ConfigureProgression(40); }
    void PlaySkill() override { ++played; }
};
void require(bool ok,const char* text) { if(!ok) { cerr<<text<<endl; exit(1); } }
bool close(float a,float b) { return abs(a-b)<0.0001f; }
int main() {
    auto player=make_shared<Player>(); player->status.stamina=100;
    Skill skill(player);
    require(!skill.CanExecuteSkill(),"An unlearned skill was usable");
    skill.SkillLevelUp();
    require(skill.GetStaminaCost()==40 && close(skill.GetMaxCooldown(),5),"Rank-one skill configuration is wrong");
    player->status.stamina=39; skill.ExecuteSkill();
    require(skill.played==0 && player->status.stamina==39,"An unaffordable skill executed or spent stamina");
    player->status.stamina=40; player->machine.state=PlayerStateType::Skill_3;
    skill.ExecuteSkill(); skill.ExecuteSkill();
    require(skill.played==1 && player->status.stamina==0,"An accepted cast did not charge exactly once");
    player->machine.state=PlayerStateType::Wait; skill.UpdateSkillCoolDown();
    require(close(skill.GetCurrentCooldown(),5),"E did not start cooldown after its accepted state ended");
    skill.UpdateSkillCoolDown(); const float remaining=skill.GetCurrentCooldown(); skill.SkillEnd();
    require(close(skill.GetCurrentCooldown(),remaining),"Duplicate skill end reset the running cooldown");
    skill.SkillLevelUp();
    require(close(skill.GetMaxCooldown(),4.5f) && skill.GetStaminaCost()==36 && close(skill.GetDamageMultiplier(),1.1f),"Rank increase did not update all three values");
    require(close(skill.GetCurrentCooldown(),remaining),"Leveling changed an already running cooldown");
    skill.UpdateCooldown(100); player->status.stamina=70; player->machine.state=PlayerStateType::Skill_3;
    skill.ExecuteSkill();
    require(skill.played==2 && player->status.stamina==34,"Rank-two cast did not consume its reduced cost");
    player->machine.state=PlayerStateType::Wait; skill.UpdateSkillCoolDown();
    require(close(skill.GetCurrentCooldown(),4.5f),"Rank-two cast used the old cooldown");
}
