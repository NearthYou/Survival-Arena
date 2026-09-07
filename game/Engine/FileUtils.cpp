#include "pch.h"
#include "FileUtils.h"

FileUtils::FileUtils()
{

}

FileUtils::~FileUtils()
{
	if (_handle != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(_handle);
		_handle = INVALID_HANDLE_VALUE;
	}
}


void FileUtils::Open(wstring filePath, FileMode mode)
{
	if (mode == FileMode::Write)
	{
		_handle = ::CreateFile(
			filePath.c_str(),
			GENERIC_WRITE,
			0,
			nullptr,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);
	}
	else
	{
		_handle = ::CreateFile
		(
			filePath.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);
	}

	if (_handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open binary game file.");
}


void FileUtils::Write(void* data, uint32 dataSize)
{
	uint32 numOfBytes = 0;
	if (!::WriteFile(_handle, data, dataSize, reinterpret_cast<LPDWORD>(&numOfBytes), nullptr) || numOfBytes != dataSize)
        throw std::runtime_error("Incomplete binary buffer write.");
}

void FileUtils::Write(const string& data)
{
	uint32 size = (uint32)data.size();
	Write(size);

	if (data.size() == 0)
		return;

	Write((void*)data.data(), size);
}

void FileUtils::Read(void** data, uint32 dataSize)
{
	uint32 numOfBytes = 0;
	if (!::ReadFile(_handle, *data, dataSize, reinterpret_cast<LPDWORD>(&numOfBytes), nullptr) || numOfBytes != dataSize)
        throw std::runtime_error("Incomplete binary buffer read.");
}

void FileUtils::Read(OUT string& data)
{
    const uint32 size = Read<uint32>();
    data.resize(size);
    if (size == 0) return;
    void* buffer = data.data();
    Read(&buffer, size);
}
