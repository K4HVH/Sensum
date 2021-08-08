#include "http.h"
#include "utils.h"
#include "console.h"

struct HTTPRequestCompleted_t
{
	enum
	{
		k_iCallback = 2101
	};

	HTTPRequestHandle	m_hRequest;
	std::uint64_t		m_ulContextValue;
	bool				m_bRequestSuccessful;
	int					m_eStatusCode;
	std::uint32_t		m_unBodySize;
};

class RequestHandler
{
public:
	virtual void Run(HTTPRequestCompleted_t* param);
	virtual void Run(HTTPRequestCompleted_t* param, bool io_failure, SteamAPICall_t api_call)
	{
		Run(param);
	}

	virtual int GetCallbackSizeBytes()
	{
		return sizeof(HTTPRequestCompleted_t);
	}

	static void SetCallback(std::function<void(std::string)> f)
	{
		callback = f;
	}
protected:
	std::uint8_t m_nCallbackFlags = 0;
	int m_iCallback = HTTPRequestCompleted_t::k_iCallback;
private:
	static std::function<void(std::string)> callback;
};

std::function<void(std::string)> RequestHandler::callback;

void RequestHandler::Run(HTTPRequestCompleted_t* param)
{
	if (param->m_eStatusCode == 200)
	{
		std::string body;

		body.resize(param->m_unBodySize + 1);

		const auto body_cstr = reinterpret_cast<std::uint8_t*>(const_cast<char*>(body.data()));

		g::steam_http->GetHTTPResponseBodyData(param->m_hRequest, body_cstr, param->m_unBodySize);

		body_cstr[param->m_unBodySize] = 0;

		try
		{
			callback(body);

#ifdef _DEBUG
			console::print("successfully received response");
#endif
		}
		catch (const std::exception& msg)
		{
#ifdef _DEBUG
			console::print(msg.what());
#endif
		}
	}

	g::steam_http->ReleaseHTTPRequest(param->m_hRequest);
}

static void register_callback(RequestHandler* callback, const SteamAPICall_t handle)
{
	static const auto steam_api = utils::get_module("steam_api.dll");
	static const auto addr = reinterpret_cast<void*>(GetProcAddress(steam_api, "SteamAPI_RegisterCallResult"));

	reinterpret_cast<void(__cdecl*)(RequestHandler*, SteamAPICall_t)>(addr)(callback, handle);
}

void http::get(const char* url, std::function<void(std::string)> callback)
{
	post(url, {}, callback);
}

void http::get(const char* url, std::map<std::string, std::string> headers, std::function<void(std::string)> callback)
{
	post(url, {}, headers, callback);
}

void http::post(const char* url, std::map<std::string, std::string> params, std::function<void(std::string)> callback)
{
	post(url, params, {}, callback);
}

void http::post(const char* url, std::map<std::string, std::string> params, std::map<std::string, std::string> headers, std::function<void(std::string)> callback)
{
	RequestHandler::SetCallback(callback);

	const auto handle = g::steam_http->CreateHTTPRequest(params.empty() ? EHTTPMethod::GET : EHTTPMethod::POST, url);

	for (auto& header : headers)
	{
		g::steam_http->SetHTTPRequestHeaderValue(handle, header.first.c_str(), header.second.c_str());
	}

	for (auto& param : params)
	{
		g::steam_http->SetHTTPRequestGetOrPostParameter(handle, param.first.c_str(), param.second.c_str());
	}

	SteamAPICall_t api_call;
	g::steam_http->SendHTTPRequest(handle, &api_call);

	static RequestHandler s_request_handler;
	register_callback(&s_request_handler, api_call);
}