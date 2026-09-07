#include "pch.h"
#include "Utils.h"

bool Utils::StartsWith(string _str, string _comp)
{
	wstring::size_type index = _str.find(_comp);
	if (index != wstring::npos && index == 0)
		return true;

	return false;
}

bool Utils::StartsWith(wstring _str, wstring _comp)
{
	wstring::size_type index = _str.find(_comp);
	if (index != wstring::npos && index == 0)
		return true;

	return false;
}

void Utils::Replace(OUT string& _str, string _comp, string _rep)
{
	string temp = _str;

	size_t start_pos = 0;
	while ((start_pos = temp.find(_comp, start_pos)) != wstring::npos)
	{
		temp.replace(start_pos, _comp.length(), _rep);
		start_pos += _rep.length();
	}

	_str = temp;
}

void Utils::Replace(OUT wstring& _str, wstring _comp, wstring _rep)
{
	wstring temp = _str;

	size_t start_pos = 0;
	while ((start_pos = temp.find(_comp, start_pos)) != wstring::npos)
	{
		temp.replace(start_pos, _comp.length(), _rep);
		start_pos += _rep.length();
	}

	_str = temp;
}

std::wstring Utils::ToWString(string _value)
{
	return wstring(_value.begin(), _value.end());
}

std::string Utils::ToString(wstring _value)
{
	return string(_value.begin(), _value.end());
}

string Utils::ToString(Vec3& _value)
{
	return to_string(_value.x) + ", " + to_string(_value.y) + ", " + to_string(_value.z);
}

float Utils::FLerp(float _start, float _end, float _t)
{
	return (1 - _t) * _start + _t * _end;
}

Vec3 Utils::Lerp(Vec3& _start, Vec3& _end, float _t)
{
	Vec3 result;
	result.x = FLerp(_start.x, _end.x, _t);
	result.y = FLerp(_start.y, _end.y, _t);
	result.z = FLerp(_start.z, _end.z, _t);
	return result;
}
