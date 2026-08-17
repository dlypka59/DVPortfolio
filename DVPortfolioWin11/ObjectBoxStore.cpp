#include "pch.h"
#include "ObjectBoxStore.h"

#include "objectbox.h"

#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

// ---- Minimal JSON helpers (no third-party JSON lib) ----

static std::string Narrow(const std::wstring& w)
{
	if (w.empty()) return {};
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

static std::wstring Widen(const std::string& s)
{
	if (s.empty()) return {};
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
	return w;
}

static std::string EscapeJson(const std::string& s)
{
	std::string o;
	o.reserve(s.size() + 8);
	for (char c : s)
	{
		switch (c)
		{
		case '\\': o += "\\\\"; break;
		case '"':  o += "\\\""; break;
		case '\n': o += "\\n"; break;
		case '\r': o += "\\r"; break;
		case '\t': o += "\\t"; break;
		default:   o += c; break;
		}
	}
	return o;
}

static std::string SessionToJson(const ObxSessionSnapshot& session)
{
	std::ostringstream os;
	os << "{\n";
	os << "  \"name\": \"" << EscapeJson(Narrow(session.name)) << "\",\n";
	os << "  \"playFps\": " << session.playFps << ",\n";
	os << "  \"views\": [\n";
	for (size_t i = 0; i < session.views.size(); ++i)
	{
		const auto& v = session.views[i];
		os << "    {\n";
		os << "      \"path\": \"" << EscapeJson(Narrow(v.path)) << "\",\n";
		os << "      \"currentFrame\": " << v.currentFrame << ",\n";
		os << "      \"zoom\": " << v.zoom << ",\n";
		os << "      \"panX\": " << v.panX << ",\n";
		os << "      \"panY\": " << v.panY << ",\n";
		os << "      \"rotationDeg\": " << v.rotationDeg << ",\n";
		os << "      \"winLeft\": " << v.winLeft << ",\n";
		os << "      \"winTop\": " << v.winTop << ",\n";
		os << "      \"winWidth\": " << v.winWidth << ",\n";
		os << "      \"winHeight\": " << v.winHeight << ",\n";
		os << "      \"maximized\": " << (v.maximized ? "true" : "false") << "\n";
		os << "    }" << (i + 1 < session.views.size() ? "," : "") << "\n";
	}
	os << "  ]\n";
	os << "}\n";
	return os.str();
}

// Very small reader for our own JSON shape (not a general parser).
static bool ParseSessionJson(const std::string& json, ObxSessionSnapshot& out)
{
	out = {};
	auto findNum = [&](const char* key, double& dest)
		{
			std::string k = std::string("\"") + key + "\"";
			size_t p = json.find(k);
			if (p == std::string::npos) return;
			p = json.find(':', p);
			if (p == std::string::npos) return;
			dest = std::atof(json.c_str() + p + 1);
		};
	auto findInt = [&](const char* key, int64_t& dest)
		{
			double d = 0;
			findNum(key, d);
			dest = static_cast<int64_t>(d);
		};
	auto findStr = [&](const char* key, std::wstring& dest)
		{
			std::string k = std::string("\"") + key + "\"";
			size_t p = json.find(k);
			if (p == std::string::npos) return;
			p = json.find(':', p);
			if (p == std::string::npos) return;
			p = json.find('"', p);
			if (p == std::string::npos) return;
			size_t p2 = json.find('"', p + 1);
			if (p2 == std::string::npos) return;
			dest = Widen(json.substr(p + 1, p2 - p - 1));
		};

	findStr("name", out.name);
	findNum("playFps", out.playFps);

	size_t pos = 0;
	while (true)
	{
		pos = json.find("\"path\"", pos);
		if (pos == std::string::npos) break;

		ObxViewSnapshot v;
		// parse from this view object start
		size_t brace = json.rfind('{', pos);
		size_t end = json.find('}', pos);
		if (brace == std::string::npos || end == std::string::npos) break;
		std::string block = json.substr(brace, end - brace + 1);

		auto localStr = [&](const char* key, std::wstring& dest)
			{
				std::string k = std::string("\"") + key + "\"";
				size_t p = block.find(k);
				if (p == std::string::npos) return;
				p = block.find('"', block.find(':', p));
				if (p == std::string::npos) return;
				size_t p2 = block.find('"', p + 1);
				if (p2 == std::string::npos) return;
				dest = Widen(block.substr(p + 1, p2 - p - 1));
			};
		auto localNum = [&](const char* key, double& dest)
			{
				std::string k = std::string("\"") + key + "\"";
				size_t p = block.find(k);
				if (p == std::string::npos) return;
				p = block.find(':', p);
				if (p == std::string::npos) return;
				dest = std::atof(block.c_str() + p + 1);
			};
		auto localInt = [&](const char* key, int64_t& dest)
			{
				double d = 0;
				localNum(key, d);
				dest = static_cast<int64_t>(d);
			};
		auto localBool = [&](const char* key, bool& dest)
			{
				std::string k = std::string("\"") + key + "\"";
				size_t p = block.find(k);
				if (p == std::string::npos) return;
				p = block.find(':', p);
				if (p == std::string::npos) return;
				dest = (block.find("true", p) != std::string::npos);
			};

		localStr("path", v.path);
		localInt("currentFrame", v.currentFrame);
		localNum("zoom", v.zoom);
		localNum("panX", v.panX);
		localNum("panY", v.panY);
		{
			int64_t r = 0;
			localInt("rotationDeg", r);
			v.rotationDeg = static_cast<unsigned>(r);
		}
		{
			int64_t x = 0, y = 0, w = 0, h = 0;
			localInt("winLeft", x); v.winLeft = (int)x;
			localInt("winTop", y); v.winTop = (int)y;
			localInt("winWidth", w); v.winWidth = (int)w;
			localInt("winHeight", h); v.winHeight = (int)h;
		}
		localBool("maximized", v.maximized);

		if (!v.path.empty())
			out.views.push_back(v);

		pos = end + 1;
	}

	return !out.views.empty() || !out.name.empty();
}

// ---- ObjectBox model (registered; blob payload for phase 1) ----

// Entity: SessionBlob
//   id   : Long, ID
//   name : String
//   json : String
static const obx_schema_id ENT_SESSIONBLOB = 1;
static const obx_uid       ENT_SESSIONBLOB_UID = 1001;

static const obx_schema_id PROP_ID = 1;
static const obx_uid       PROP_ID_UID = 1002;
static const obx_schema_id PROP_NAME = 2;
static const obx_uid       PROP_NAME_UID = 1003;
static const obx_schema_id PROP_JSON = 3;
static const obx_uid       PROP_JSON_UID = 1004;

struct ObjectBoxStore::Impl
{
	OBX_store* store = nullptr;
};

ObjectBoxStore& ObjectBoxStore::Instance()
{
	static ObjectBoxStore inst;
	return inst;
}

ObjectBoxStore::~ObjectBoxStore()
{
	Close();
}

std::wstring ObjectBoxStore::DefaultDirectory() const
{
	wchar_t* path = nullptr;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
	{
		std::wstring dir = path;
		CoTaskMemFree(path);
		dir += L"\\DVPortfolio\\objectbox";
		return dir;
	}
	return L"objectbox";
}

bool ObjectBoxStore::Open()
{
	if (m_impl && m_impl->store)
		return true;

	m_impl = new Impl();

	OBX_model* model = obx_model();
	if (!model)
		return false;

	// SessionBlob entity
	obx_model_entity(model, "SessionBlob", ENT_SESSIONBLOB, ENT_SESSIONBLOB_UID);
	obx_model_property(model, "id", OBXPropertyType_Long, PROP_ID, PROP_ID_UID);
	obx_model_property_flags(model, OBXPropertyFlags_ID);
	obx_model_property(model, "name", OBXPropertyType_String, PROP_NAME, PROP_NAME_UID);
	obx_model_property(model, "json", OBXPropertyType_String, PROP_JSON, PROP_JSON_UID);
	obx_model_entity_last_property_id(model, PROP_JSON, PROP_JSON_UID);
	obx_model_last_entity_id(model, ENT_SESSIONBLOB, ENT_SESSIONBLOB_UID);

	if (obx_model_error_code(model) != OBX_SUCCESS)
	{
		obx_model_free(model);
		delete m_impl;
		m_impl = nullptr;
		return false;
	}

	const std::wstring wdir = DefaultDirectory();
	fs::create_directories(wdir);
	const std::string dir = Narrow(wdir);

	OBX_store_options* opt = obx_opt();
	obx_opt_model(opt, model);          // model consumed by options/store
	obx_opt_directory(opt, dir.c_str());

	m_impl->store = obx_store_open(opt);
	if (!m_impl->store)
	{
		delete m_impl;
		m_impl = nullptr;
		return false;
	}

	return true;
}

void ObjectBoxStore::Close()
{
	if (!m_impl)
		return;
	if (m_impl->store)
	{
		obx_store_close(m_impl->store);
		m_impl->store = nullptr;
	}
	delete m_impl;
	m_impl = nullptr;
}

bool ObjectBoxStore::IsOpen() const
{
	return m_impl && m_impl->store;
}

bool ObjectBoxStore::SaveSession(const ObxSessionSnapshot& session)
{
	if (!IsOpen() && !Open())
		return false;

	// Phase 1: persist JSON next to the DB (reliable without FlatBuffers generator).
	// ObjectBox store is open and model is registered for upcoming typed entities.
	const std::wstring dir = DefaultDirectory();
	fs::create_directories(dir);
	const std::wstring file = dir + L"\\latest_session.json";

	const std::string json = SessionToJson(session);
	std::ofstream out(fs::path(file), std::ios::binary | std::ios::trunc);
	if (!out)
		return false;
	out.write(json.data(), (std::streamsize)json.size());
	return (bool)out;
}

bool ObjectBoxStore::LoadLatestSession(ObxSessionSnapshot& outSession)
{
	const std::wstring file = DefaultDirectory() + L"\\latest_session.json";
	std::ifstream in(fs::path(file), std::ios::binary);
	if (!in)
		return false;

	std::ostringstream ss;
	ss << in.rdbuf();
	return ParseSessionJson(ss.str(), outSession);
}