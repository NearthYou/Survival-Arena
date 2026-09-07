#include "pch.h"
#include "ModelAnimation.h"

shared_ptr<ModelKeyframe> ModelAnimation::GetKeyframe(const wstring& _name)
{
	auto findit = keyframes.find(_name);
	if (findit == keyframes.end())
		return nullptr;

	return findit->second;
}
