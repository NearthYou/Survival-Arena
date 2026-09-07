#pragma once


struct ModelKeyframeData {
	float m_time;
	Vec3 m_scale;
	Quaternion m_rotation;
	Vec3 m_translation;
};

struct ModelKeyframe {
	wstring m_boneName;
	vector<ModelKeyframeData> m_transforms;
};

struct ModelAnimation
{
	wstring m_name;
	float m_duration = 0.f;
	float m_frameRate = 0.f;
	uint32 m_frameCount = 0;
	unordered_map<wstring, shared_ptr<ModelKeyframe>> keyframes;

	shared_ptr<ModelKeyframe> GetKeyframe(const wstring& _name);
};

