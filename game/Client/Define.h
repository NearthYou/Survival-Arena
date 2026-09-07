#pragma once

struct DamageInfo {
	int32 damage;
	// 0 이상일 경우 스턴. 
	float stunTime;
	shared_ptr<GameObject> attacker;
};