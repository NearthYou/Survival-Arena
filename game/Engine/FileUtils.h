#pragma once
#include <stdexcept>

enum FileMode : uint8
{
	Write,
	Read,
};

class FileUtils
{
public:
	FileUtils();
	~FileUtils();

	void Open(wstring filePath, FileMode mode);

	template<typename T>
	void Write(const T& data)
	{
		DWORD numOfBytes = 0;
		//표준에서 제공하는 걸로 파일을 써주기. 
		//Return받는게 numOfBytes

		//String은 sizeof로 하는 게 맞나? 따라서 버전을 2개 쓴다. 
		if (!::WriteFile(_handle, &data, sizeof(T), &numOfBytes, nullptr) || numOfBytes != sizeof(T))
            throw std::runtime_error("Incomplete binary file write.");
	}

	//String용도. 템플릿 특수화. (일종의 예외처리)
	template<>
	void Write<string>(const string& data)
	{
		return Write(data);
	}

	void Write(void* data, uint32 dataSize);
	void Write(const string& data);

	template<typename T>
	void Read(OUT T& data)
	{
		DWORD numOfBytes = 0;
		if (!::ReadFile(_handle, &data, sizeof(T), &numOfBytes, nullptr) || numOfBytes != sizeof(T))
            throw std::runtime_error("Incomplete binary file read.");
	}

	template<typename T>
	T Read()
	{
		T data{};
		Read(data);
		return data;
	}

	void Read(void** data, uint32 dataSize);
	void Read(OUT string& data);

private:
	HANDLE _handle = INVALID_HANDLE_VALUE;
};

