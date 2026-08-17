#pragma once

#include <string>
#include <vector>

struct ObxViewSnapshot
{
	std::wstring path;
	int64_t currentFrame = 0;
	double zoom = 1.0;
	double panX = 0.0;
	double panY = 0.0;
	unsigned rotationDeg = 0;
	int winLeft = 0;
	int winTop = 0;
	int winWidth = 0;
	int winHeight = 0;
	bool maximized = false;
};

struct ObxSessionSnapshot
{
	std::wstring name;
	double playFps = 30.0;
	std::vector<ObxViewSnapshot> views;
};

class ObjectBoxStore
{
public:
	static ObjectBoxStore& Instance();

	bool Open();
	void Close();
	bool IsOpen() const;

	// Working session persistence (multi-view)
	bool SaveSession(const ObxSessionSnapshot& session);
	bool LoadLatestSession(ObxSessionSnapshot& outSession);

private:
	ObjectBoxStore() = default;
	~ObjectBoxStore();

	ObjectBoxStore(const ObjectBoxStore&) = delete;
	ObjectBoxStore& operator=(const ObjectBoxStore&) = delete;

	struct Impl;
	Impl* m_impl = nullptr;

	std::wstring DefaultDirectory() const;
};