#include "EditorMcpService.hpp"

#include <Frigga/Asset/AssetManifest.hpp>
#include <Frigga/Asset/AssetCooker.hpp>
#include <Frigga/Serialization/FormatVersions.hpp>

#include <simdjson.h>

#include <chrono>
#include <cctype>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#    define NOMINMAX
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <arpa/inet.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

namespace
{
using Socket = std::intptr_t;

constexpr Socket InvalidSocket = -1;

Socket AsSocket(std::intptr_t value)
{
    return value;
}

void CloseSocket(Socket socket)
{
    if(socket == InvalidSocket)
    {
        return;
    }
#if defined(_WIN32)
    ::closesocket(static_cast<SOCKET>(socket));
#else
    ::close(static_cast<int>(socket));
#endif
}

std::string JsonEscape(std::string_view value)
{
    std::string result;
    result.reserve(value.size() + 2);
    for(const char ch : value)
    {
        switch(ch)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += ch;
            break;
        }
    }
    return result;
}

std::string Field(std::string_view json, std::string_view name)
{
    const std::string needle = "\"" + std::string(name) + "\"";
    const auto key = json.find(needle);
    if(key == std::string_view::npos)
    {
        return {};
    }
    auto cursor = json.find(':', key + needle.size());
    if(cursor == std::string_view::npos)
    {
        return {};
    }
    cursor = json.find('"', cursor + 1);
    if(cursor == std::string_view::npos)
    {
        return {};
    }
    ++cursor;
    std::string value;
    bool escaped = false;
    for(; cursor < json.size(); ++cursor)
    {
        const char ch = json[cursor];
        if(escaped)
        {
            value += ch;
            escaped = false;
        }
        else if(ch == '\\')
        {
            escaped = true;
        }
        else if(ch == '"')
        {
            break;
        }
        else
        {
            value += ch;
        }
    }
    return value;
}

bool BoolField(std::string_view json, std::string_view name)
{
    const std::string needle = "\"" + std::string(name) + "\"";
    const auto key = json.find(needle);
    if(key == std::string_view::npos)
    {
        return false;
    }
    const auto cursor = json.find(':', key + needle.size());
    return cursor != std::string_view::npos &&
           json.substr(cursor + 1).find("true") < json.substr(cursor + 1).find_first_of(",}");
}

bool IsPathInside(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    std::error_code error;
    const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
    if(error)
    {
        return false;
    }
    const auto canonicalCandidate = std::filesystem::weakly_canonical(candidate, error);
    if(error)
    {
        return false;
    }
    auto rootIt = canonicalRoot.begin();
    auto candidateIt = canonicalCandidate.begin();
    for(; rootIt != canonicalRoot.end() && candidateIt != canonicalCandidate.end();
        ++rootIt, ++candidateIt)
    {
        if(*rootIt != *candidateIt)
        {
            return false;
        }
    }
    return rootIt == canonicalRoot.end();
}

std::string RawObjectField(std::string_view json, std::string_view name)
{
    const auto key = json.find("\"" + std::string(name) + "\"");
    if(key == std::string_view::npos)
    {
        return {};
    }
    auto start = json.find('{', json.find(':', key));
    if(start == std::string_view::npos)
    {
        return {};
    }
    std::size_t depth = 0;
    bool quoted = false;
    bool escaped = false;
    for(std::size_t i = start; i < json.size(); ++i)
    {
        const char ch = json[i];
        if(escaped)
        {
            escaped = false;
            continue;
        }
        if(ch == '\\' && quoted)
        {
            escaped = true;
            continue;
        }
        if(ch == '"')
        {
            quoted = !quoted;
        }
        if(quoted)
        {
            continue;
        }
        if(ch == '{')
            ++depth;
        else if(ch == '}' && --depth == 0)
            return std::string(json.substr(start, i - start + 1));
    }
    return {};
}

std::string IdField(std::string_view json)
{
    const auto key = json.find("\"id\"");
    if(key == std::string_view::npos)
    {
        return "null";
    }
    auto cursor = json.find(':', key);
    if(cursor == std::string_view::npos)
    {
        return "null";
    }
    ++cursor;
    while(cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor])))
    {
        ++cursor;
    }
    if(cursor < json.size() && json[cursor] == '"')
    {
        return "\"" + JsonEscape(Field(json.substr(key), "id")) + "\"";
    }
    const auto end = json.find_first_of(",}", cursor);
    return std::string(json.substr(cursor, end == std::string_view::npos ? end : end - cursor));
}

std::string ErrorResult(std::string_view message, std::string_view code = "invalid_request")
{
    return "{\"ok\":false,\"error_code\":\"" + JsonEscape(code) + "\",\"message\":\"" +
           JsonEscape(message) + "\"}";
}

std::string OkResult(std::string_view data = "{}")
{
    return "{\"ok\":true,\"data\":" + std::string(data) + "}";
}

std::string ReadLine(Socket socket)
{
    std::string line;
    char ch = '\0';
    while(line.size() < 1024 * 1024)
    {
#if defined(_WIN32)
        const auto count = ::recv(static_cast<SOCKET>(socket), &ch, 1, 0);
#else
        const auto count = ::recv(static_cast<int>(socket), &ch, 1, 0);
#endif
        if(count <= 0)
        {
            return {};
        }
        if(ch == '\n')
        {
            return line;
        }
        if(ch != '\r')
        {
            line += ch;
        }
    }
    return {};
}

bool SendLine(Socket socket, std::string_view line)
{
    std::string payload(line);
    payload += '\n';
    std::size_t sent = 0;
    while(sent < payload.size())
    {
#if defined(_WIN32)
        const auto count = ::send(static_cast<SOCKET>(socket), payload.data() + sent,
                                  static_cast<int>(payload.size() - sent), 0);
#else
        const auto count = ::send(static_cast<int>(socket), payload.data() + sent,
                                  payload.size() - sent, MSG_NOSIGNAL);
#endif
        if(count <= 0)
        {
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

std::string RandomToken()
{
    std::random_device device;
    std::mt19937_64 generator(device());
    std::ostringstream stream;
    stream << std::hex << generator() << generator();
    return stream.str();
}
} // namespace

EditorMcpService::EditorMcpService(skr::Arc<ProjectSession> session,
                                   skr::Arc<fg::Scene> scene,
                                   skr::Arc<fg::SceneSimulationState> simulation,
                                   skr::Arc<skr::Logger<EditorMcpService>> logger)
    : mSession(std::move(session)),
      mScene(std::move(scene)),
      mSimulation(std::move(simulation)),
      mLogger(std::move(logger))
{
    mEndpointFile = std::filesystem::temp_directory_path() / "frigga-editor-mcp.endpoint";
}

EditorMcpService::~EditorMcpService()
{
    Stop();
}

bool EditorMcpService::Start(std::string &error)
{
    if(mRunning.exchange(true))
    {
        return true;
    }
#if defined(_WIN32)
    WSADATA data {};
    if(WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        error = "WSAStartup failed";
        mRunning = false;
        return false;
    }
#endif
    const Socket server = static_cast<Socket>(::socket(AF_INET, SOCK_STREAM, 0));
    if(server == InvalidSocket)
    {
        error = "Unable to create MCP socket";
        mRunning = false;
        return false;
    }
    int reuse = 1;
    ::setsockopt(
#if defined(_WIN32)
        static_cast<SOCKET>(server),
#else
        static_cast<int>(server),
#endif
        SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));

    sockaddr_in address {};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port        = 0;
    if(::bind(
#if defined(_WIN32)
           static_cast<SOCKET>(server),
#else
           static_cast<int>(server),
#endif
           reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0 ||
       ::listen(
#if defined(_WIN32)
           static_cast<SOCKET>(server),
#else
           static_cast<int>(server),
#endif
           1) != 0)
    {
        CloseSocket(server);
        error = "Unable to bind MCP loopback socket";
        mRunning = false;
        return false;
    }
    socklen_t length = sizeof(address);
    ::getsockname(
#if defined(_WIN32)
        static_cast<SOCKET>(server),
#else
        static_cast<int>(server),
#endif
        reinterpret_cast<sockaddr *>(&address), &length);
    mServerSocket = server;
    mToken         = RandomToken();
    std::ofstream endpoint(mEndpointFile, std::ios::trunc);
    endpoint << "{\"host\":\"127.0.0.1\",\"port\":" << ntohs(address.sin_port)
             << ",\"token\":\"" << JsonEscape(mToken) << "\"}\n";
    endpoint.close();
    if(!endpoint)
    {
        error = "Unable to write MCP endpoint file";
        Stop();
        return false;
    }
    mNetworkThread = std::thread(&EditorMcpService::NetworkLoop, this);
    return true;
}

void EditorMcpService::Stop()
{
    if(!mRunning.exchange(false))
    {
        return;
    }
    const auto server = mServerSocket.exchange(InvalidSocket);
    CloseSocket(AsSocket(server));
    const auto client = mClientSocket.exchange(InvalidSocket);
    CloseSocket(AsSocket(client));
    if(mNetworkThread.joinable())
    {
        mNetworkThread.join();
    }
    FailPending("Editor MCP service stopped");
    std::error_code error;
    std::filesystem::remove(mEndpointFile, error);
#if defined(_WIN32)
    WSACleanup();
#endif
}

void EditorMcpService::NetworkLoop()
{
    while(mRunning)
    {
        sockaddr_in clientAddress {};
#if defined(_WIN32)
        int length = sizeof(clientAddress);
        const Socket client = static_cast<Socket>(
            ::accept(static_cast<SOCKET>(mServerSocket.load()), reinterpret_cast<sockaddr *>(&clientAddress),
                     &length));
#else
        socklen_t length = sizeof(clientAddress);
        const Socket client = static_cast<Socket>(
            ::accept(static_cast<int>(mServerSocket.load()), reinterpret_cast<sockaddr *>(&clientAddress),
                     &length));
#endif
        if(client == InvalidSocket)
        {
            continue;
        }
        mClientSocket = client;
        const auto first = ReadLine(client);
        if(first.empty() || Field(first, "token") != mToken)
        {
            SendLine(client, "{\"ok\":false,\"error_code\":\"unauthorized\"}");
            CloseSocket(client);
            mClientSocket = InvalidSocket;
            continue;
        }
        if(!SendLine(client, "{\"ok\":true,\"data\":{\"transport\":\"loopback\"}}"))
        {
            CloseSocket(client);
            mClientSocket = InvalidSocket;
            continue;
        }
        while(mRunning)
        {
            const auto line = ReadLine(client);
            if(line.empty())
            {
                break;
            }
            auto request      = std::make_shared<Request>();
            request->id       = IdField(line);
            request->method   = Field(line, "method");
            request->params   = line;
            auto future       = request->response.get_future();
            {
                std::lock_guard lock(mQueueMutex);
                mRequests.push(request);
            }
            const auto response = future.get();
            if(!SendLine(client, response))
            {
                break;
            }
        }
        CloseSocket(client);
        mClientSocket = InvalidSocket;
    }
}

void EditorMcpService::Poll()
{
    std::queue<std::shared_ptr<Request>> requests;
    {
        std::lock_guard lock(mQueueMutex);
        requests.swap(mRequests);
    }
    while(!requests.empty())
    {
        auto request = std::move(requests.front());
        requests.pop();
        request->response.set_value(Dispatch(*request));
    }
}

void EditorMcpService::FailPending(std::string_view message)
{
    std::queue<std::shared_ptr<Request>> requests;
    {
        std::lock_guard lock(mQueueMutex);
        requests.swap(mRequests);
    }
    while(!requests.empty())
    {
        auto request = std::move(requests.front());
        requests.pop();
        request->response.set_value("{\"id\":" + request->id + ",\"result\":" +
                                    ErrorResult(message, "service_stopped") + "}");
    }
}

std::string EditorMcpService::Dispatch(const Request &request)
{
    mLogger->LogInformation("MCP request {}", request.method);
    std::string result;
    if(request.method == "project.inspect")
        result = ProjectInspect();
    else if(request.method == "scene.inspect")
        result = SceneInspect();
    else if(request.method == "scene.open")
        result = HandleSceneOpen(request.params);
    else if(request.method == "scene.create")
        result = HandleSceneCreate(request.params);
    else if(request.method == "scene.save")
        result = HandleSceneSave();
    else if(request.method == "scene.replace_snapshot")
        result = HandleSceneReplaceSnapshot(request.params);
    else if(request.method == "scene.update_component" || request.method == "scene.delete_entity")
        result = HandleSceneReplaceSnapshot(request.params);
    else if(request.method == "assets.validate")
        result = HandleAssetsValidate();
    else if(request.method == "assets.list")
        result = HandleAssetsList();
    else if(request.method == "assets.cook")
        result = HandleAssetsCook(request.params);
    else if(request.method == "runtime.start" || request.method == "runtime.stop" ||
            request.method == "runtime.status")
        result = HandleRuntime(request.method);
    else if(request.method == "editor.invoke")
        result = HandleEditorInvoke(request.params);
    else if(request.method == "logs.recent")
        result = HandleLogsRecent();
    else
        result = ErrorResult("Unknown Editor MCP method", "method_not_found");
    return "{\"id\":" + request.id + ",\"result\":" + result + "}";
}

std::string EditorMcpService::ProjectInspect() const
{
    const auto root = mSession->GetProjectRoot();
    return OkResult("{\"has_project\":" + std::string(mSession->HasProject() ? "true" : "false") +
                    ",\"project_file\":\"" +
                    JsonEscape(mSession->GetProjectFile()
                                   ? mSession->GetProjectFile()->generic_string()
                                   : "") +
                    "\",\"project_root\":\"" + JsonEscape(root ? root->generic_string() : "") +
                    "\",\"status\":\"" + JsonEscape(mSession->GetStatusMessage()) +
                    "\",\"error\":\"" + JsonEscape(mSession->GetLastError()) + "\"}");
}

std::string EditorMcpService::SceneInspect() const
{
    std::string snapshot;
    if(!mScene->CaptureSnapshot(snapshot))
    {
        return ErrorResult("Unable to capture the active scene", "scene_capture_failed");
    }
    return OkResult("{\"path\":\"" + JsonEscape(mScene->GetPath().generic_string()) +
                    "\",\"snapshot\":" + snapshot + "}");
}

std::string EditorMcpService::HandleSceneOpen(std::string_view params)
{
    const auto path = Field(params, "path");
    if(path.empty())
    {
        return ErrorResult("scene.open requires path");
    }
    const auto root = mSession->GetProjectRoot();
    const auto scenePath = std::filesystem::absolute(path);
    if(!root || !IsPathInside(*root, scenePath))
    {
        return ErrorResult("Scene path must be inside the open project", "path_forbidden");
    }
    if(!mSession->OpenSceneFile(scenePath))
    {
        return ErrorResult(mSession->GetLastError(), "scene_open_failed");
    }
    return OkResult("{\"path\":\"" + JsonEscape(mScene->GetPath().generic_string()) + "\"}");
}

std::string EditorMcpService::HandleSceneCreate(std::string_view params)
{
    const auto name = Field(params, "name");
    if(name.empty())
    {
        return ErrorResult("scene.create requires name");
    }
    if(name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
       name.find("..") != std::string::npos)
    {
        return ErrorResult("Scene name contains a forbidden path segment", "path_forbidden");
    }
    if(BoolField(params, "dry_run"))
    {
        return OkResult("{\"dry_run\":true,\"name\":\"" + JsonEscape(name) + "\"}");
    }
    if(!mSession->CreateScene(name, fg::SceneTemplate::D3, true))
    {
        return ErrorResult(mSession->GetLastError(), "scene_create_failed");
    }
    return OkResult("{\"path\":\"" + JsonEscape(mScene->GetPath().generic_string()) + "\"}");
}

std::string EditorMcpService::HandleSceneSave() const
{
    if(!mScene->SaveScene())
    {
        return ErrorResult("Unable to save the active scene", "scene_save_failed");
    }
    return OkResult("{\"path\":\"" + JsonEscape(mScene->GetPath().generic_string()) + "\"}");
}

std::string EditorMcpService::HandleSceneReplaceSnapshot(std::string_view params)
{
    const auto snapshot = RawObjectField(params, "snapshot");
    if(snapshot.empty())
    {
        return ErrorResult("scene.replace_snapshot requires snapshot");
    }
    if(BoolField(params, "dry_run"))
    {
        return OkResult("{\"dry_run\":true}");
    }
    if(!mScene->RestoreSnapshot(snapshot))
    {
        return ErrorResult("Unable to apply the scene snapshot", "scene_restore_failed");
    }
    return OkResult("{\"applied\":true}");
}

std::string EditorMcpService::HandleAssetsValidate() const
{
    const auto root = mSession->GetResourcesDirectory();
    if(root.empty())
    {
        return ErrorResult("No project is open", "project_not_open");
    }
    fg::AssetManifest manifest;
    std::string error;
    if(!manifest.Load(root, &error))
    {
        return ErrorResult(error, "asset_manifest_failed");
    }
    const auto validation = manifest.Validate(root);
    auto list = [](const std::vector<std::string> &items) {
        std::string result = "[";
        for(std::size_t i = 0; i < items.size(); ++i)
        {
            if(i)
                result += ',';
            result += "\"" + JsonEscape(items[i]) + "\"";
        }
        return result + "]";
    };
    return OkResult("{\"valid\":" + std::string(validation.IsValid() ? "true" : "false") +
                    ",\"missing\":" + list(validation.missing) +
                    ",\"changed\":" + list(validation.changed) +
                    ",\"orphaned\":" + list(validation.orphaned) + "}");
}

std::string EditorMcpService::HandleAssetsList() const
{
    const auto root = mSession->GetResourcesDirectory();
    if(root.empty())
    {
        return ErrorResult("No project is open", "project_not_open");
    }
    fg::AssetManifest manifest;
    std::string error;
    if(!manifest.Load(root, &error))
    {
        return ErrorResult(error, "asset_manifest_failed");
    }
    std::string records = "[";
    const auto entries = manifest.Records();
    for(std::size_t i = 0; i < entries.size(); ++i)
    {
        if(i)
            records += ',';
        records += "{\"path\":\"" + JsonEscape(entries[i].relativePath) + "\",\"type\":\"" +
                   JsonEscape(entries[i].type) + "\",\"guid\":\"" +
                   JsonEscape(entries[i].guid) + "\"}";
    }
    return OkResult("{\"records\":" + records + "]}");
}

std::string EditorMcpService::HandleAssetsCook(std::string_view params) const
{
    const auto root = mSession->GetProjectRoot();
    if(!root)
    {
        return ErrorResult("No project is open", "project_not_open");
    }
    const auto destination = std::filesystem::absolute(
        Field(params, "destination").empty()
            ? (*root / ".frigga-mcp-cooked")
            : std::filesystem::path(Field(params, "destination")));
    if(!IsPathInside(*root, destination))
    {
        return ErrorResult("Cook destination must be inside the project", "path_forbidden");
    }
    if(BoolField(params, "dry_run"))
    {
        return OkResult("{\"dry_run\":true,\"destination\":\"" +
                        JsonEscape(destination.generic_string()) + "\"}");
    }
    const auto result = fg::AssetCooker::Cook(mSession->GetResourcesDirectory(), destination);
    if(!result.ok)
    {
        return ErrorResult(result.error, "asset_cook_failed");
    }
    return OkResult("{\"destination\":\"" + JsonEscape(destination.generic_string()) +
                    "\",\"copied\":" + std::to_string(result.copied.size()) + "}");
}

std::string EditorMcpService::HandleLogsRecent() const
{
    return OkResult("{\"status\":\"" + JsonEscape(mSession->GetStatusMessage()) +
                    "\",\"error\":\"" + JsonEscape(mSession->GetLastError()) + "\"}");
}

std::string EditorMcpService::HandleRuntime(std::string_view method)
{
    if(method == "runtime.start")
        mSimulation->Play();
    else if(method == "runtime.stop")
        mSimulation->Stop();
    return OkResult("{\"playing\":" + std::string(mSimulation->IsPlaying() ? "true" : "false") +
                    ",\"paused\":" + std::string(mSimulation->IsPaused() ? "true" : "false") +
                    "}");
}

std::string EditorMcpService::HandleEditorInvoke(std::string_view params)
{
    const auto action = Field(params, "action");
    if(action == "save_scene")
        return HandleSceneSave();
    if(action == "play" || action == "stop")
        return HandleRuntime(action == "play" ? "runtime.start" : "runtime.stop");
    if(action == "validate_assets")
        return HandleAssetsValidate();
    return ErrorResult("Editor action is not allowlisted", "action_forbidden");
}
