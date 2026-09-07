#include <cmath>
#include <cstdlib>
#include <iostream>
#include <windows.h>
#include "SkillProgression.h"
void require(bool ok,const char* message) { if(!ok) { std::cerr<<message<<std::endl; std::exit(1); } }
bool close(float a,float b) { return std::abs(a-b)<0.0001f; }
int main() {
    using namespace SkillProgression;
    require(close(Cooldown(5,1,5,0),5),"Rank-one cooldown changed");
    require(close(Cooldown(5,5,5,0),3),"Rank-five cooldown did not decrease");
    require(close(Cooldown(10,3,3,0),8),"Ultimate rank cap is wrong");
    require(StaminaCost(40,1,5)==40 && StaminaCost(40,5,5)==24,"Stamina cost does not decrease by rank");
    require(close(DamageMultiplier(1,5),1) && close(DamageMultiplier(5,5),1.4f),"Damage must increase with rank");
    require(close(Cooldown(5,99,5,0),3),"Rank above the cap changed the cooldown");
    require(close(Cooldown(5,0,5,0),5),"Unlearned rank did not retain base configuration");
    require(close(Cooldown(5,5,5,0.5f),1.5f),"Equipment reduction did not compose with rank");
    require(Cooldown(5,5,5,2.f)>0,"Excessive equipment reduction removed the cooldown");
    for(int rank=2;rank<=5;++rank) {
        require(Cooldown(5,rank,5,0)<Cooldown(5,rank-1,5,0),"Cooldown is not decreasing");
        require(StaminaCost(40,rank,5)<StaminaCost(40,rank-1,5),"Cost is not decreasing");
        require(DamageMultiplier(rank,5)>DamageMultiplier(rank-1,5),"Damage is not increasing");
    }
}
