#include "PublishPipeline.hpp"

#include "../Project/ProjectFile.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace publish
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::string_view kReleaseMarker = ".frigga-release-config";

#if defined(_WIN32)
        constexpr std::string_view kPlatform = "windows";
#elif defined(__APPLE__)
        constexpr std::string_view kPlatform = "macos";
#else
        constexpr std::string_view kPlatform = "linux";
#endif

        struct Range
        {
            float begin;
            float end;
        };

        // Overall progress budget per stage; Build dominates.
        constexpr Range StageRange(Stage stage)
        {
            switch(stage)
            {
            case Stage::Validate:
                return {0.00f, 0.04f};
            case Stage::Configure:
                return {0.04f, 0.14f};
            case Stage::Build:
                return {0.14f, 0.84f};
            case Stage::Install:
                return {0.84f, 0.88f};
            case Stage::Cook:
                return {0.88f, 0.94f};
            case Stage::Stage:
                return {0.94f, 0.96f};
            case Stage::Finalize:
                return {0.96f, 1.00f};
            }
            return {0.0f, 1.0f};
        }

        fs::path ResolveAgainst(const fs::path &root, const fs::path &path)
        {
            return path.is_absolute() ? path : root / path;
        }

        bool IsSameOrInside(const fs::path &parent, const fs::path &child)
        {
            std::error_code ec;
            const auto p   = fs::weakly_canonical(parent, ec).lexically_normal();
            const auto c   = fs::weakly_canonical(child, ec).lexically_normal();
            const auto rel = c.lexically_relative(p);
            return !rel.empty() && *rel.begin() != "..";
        }
    } // namespace

    std::string_view ToString(Profile profile)
    {
        return profile == Profile::Development ? "development" : "shipping";
    }

    std::string_view ToString(Stage stage)
    {
        switch(stage)
        {
        case Stage::Validate:
            return "validate";
        case Stage::Configure:
            return "configure";
        case Stage::Build:
            return "build";
        case Stage::Install:
            return "install";
        case Stage::Cook:
            return "cook";
        case Stage::Stage:
            return "stage";
        case Stage::Finalize:
            return "finalize";
        }
        return "unknown";
    }

    std::string_view StageLabel(Stage stage)
    {
        switch(stage)
        {
        case Stage::Validate:
            return "Validating project…";
        case Stage::Configure:
            return "Configuring (CMake)…";
        case Stage::Build:
            return "Compiling…";
        case Stage::Install:
            return "Installing runtime…";
        case Stage::Cook:
            return "Cooking assets…";
        case Stage::Stage:
            return "Staging project files…";
        case Stage::Finalize:
            return "Finalizing package…";
        }
        return "Publishing…";
    }

    std::optional<Profile> ParseProfile(std::string_view text)
    {
        std::string lowered(text);
        std::ranges::transform(lowered, lowered.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        if(lowered == "development" || lowered == "dev")
        {
            return Profile::Development;
        }
        if(lowered == "shipping" || lowered == "ship")
        {
            return Profile::Shipping;
        }
        return std::nullopt;
    }

    std::string Fnv1a64Hex(std::string_view data)
    {
        std::uint64_t hash = 14695981039346656037ull;
        for(const char ch: data)
        {
            hash ^= static_cast<unsigned char>(ch);
            hash *= 1099511628211ull;
        }
        char buffer[17];
        std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(hash));
        return buffer;
    }

    std::optional<std::string> HashFileHex(const fs::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if(!file)
        {
            return std::nullopt;
        }
        std::uint64_t hash = 14695981039346656037ull;
        char buffer[64 * 1024];
        while(file)
        {
            file.read(buffer, sizeof(buffer));
            const auto count = file.gcount();
            for(std::streamsize i = 0; i < count; ++i)
            {
                hash ^= static_cast<unsigned char>(buffer[i]);
                hash *= 1099511628211ull;
            }
        }
        char out[17];
        std::snprintf(out, sizeof(out), "%016llx", static_cast<unsigned long long>(hash));
        return std::string(out);
    }

    std::string ReadTextFile(const fs::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if(!file)
        {
            return {};
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        return contents.str();
    }

    std::string EscapeJson(std::string_view value)
    {
        std::string out;
        out.reserve(value.size() + 8);
        for(const char ch: value)
        {
            switch(ch)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if(static_cast<unsigned char>(ch) < 0x20)
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                    out += buffer;
                }
                else
                {
                    out.push_back(ch);
                }
                break;
            }
        }
        return out;
    }

    std::string FormatUtcTimestamp()
    {
        using clock   = std::chrono::system_clock;
        const auto tt = clock::to_time_t(clock::now());
        std::tm tm {};
#if defined(_WIN32)
        gmtime_s(&tm, &tt);
#else
        gmtime_r(&tt, &tm);
#endif
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900,
                      tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        return buffer;
    }

    std::string QuotePath(const fs::path &path)
    {
        std::string raw = path.generic_string();
        std::string out;
        out.reserve(raw.size() + 2);
        out.push_back('"');
        for(const char ch: raw)
        {
            if(ch == '"')
            {
                out += "\\\"";
            }
            else
            {
                out.push_back(ch);
            }
        }
        out.push_back('"');
        return out;
    }

    bool IsValidVersion(std::string_view version)
    {
        int parts     = 0;
        std::size_t i = 0;
        while(true)
        {
            const auto start = i;
            while(i < version.size() && std::isdigit(static_cast<unsigned char>(version[i])))
            {
                ++i;
            }
            if(i == start)
            {
                return false;
            }
            ++parts;
            if(i == version.size())
            {
                break;
            }
            if(version[i] != '.')
            {
                return false;
            }
            ++i;
        }
        return parts == 3;
    }

    std::string DiscoverCxxCompiler(const ProjectDescriptor &engine,
                                    const fs::path &executableDir)
    {
        const fs::path candidates[] = {
            engine.friggaBuild,
            engine.friggaSdk,
            engine.friggaBuild.parent_path(),
            engine.friggaSdk.parent_path(),
            executableDir,
        };
        std::error_code ec;
        for(const auto &dir: candidates)
        {
            if(dir.empty() || !fs::exists(dir / "CMakeCache.txt", ec))
            {
                continue;
            }
            std::ifstream file(dir / "CMakeCache.txt");
            std::string line;
            const std::string prefix = "CMAKE_CXX_COMPILER:";
            while(std::getline(file, line))
            {
                if(line.rfind(prefix, 0) != 0)
                {
                    continue;
                }
                if(const auto eq = line.find('='); eq != std::string::npos)
                {
                    const auto value = line.substr(eq + 1);
                    if(!value.empty())
                    {
                        return value;
                    }
                    break;
                }
            }
            // Cache exists but carries no compiler entry: keep looking.
        }
        return {};
    }

    std::string PublishedModuleLibrary(std::string_view target)
    {
#if defined(_WIN32)
        return "Modules/" + std::string(target) + ".dll";
#elif defined(__APPLE__)
        return "Modules/lib" + std::string(target) + ".dylib";
#else
        return "Modules/lib" + std::string(target) + ".so";
#endif
    }

    std::string ReleaseBuildSignature(const fs::path &projectRoot, const fs::path &sdk,
                                      std::string_view compiler, Profile profile)
    {
        const auto manifest  = ReadTextFile(projectRoot / ProjectFile::FileName);
        const auto cmake     = ReadTextFile(projectRoot / "CMakeLists.txt");
        const auto sdkConfig = ReadTextFile(sdk / "FriggaSdkConfig.cmake");
        std::ostringstream signature;
        signature << "schema=3\n";
        signature << "configuration=Release\n";
        signature << "profile=" << ToString(profile) << '\n';
        signature << "platform=" << kPlatform << '\n';
        signature << "compiler=" << compiler << '\n';
        signature << "sdk=" << sdk.lexically_normal().generic_string() << '\n';
        signature << "sdkConfigHash=" << Fnv1a64Hex(sdkConfig) << '\n';
        signature << "manifestHash=" << Fnv1a64Hex(manifest) << '\n';
        signature << "cmakeHash=" << Fnv1a64Hex(cmake) << '\n';
        return signature.str();
    }

    bool PrepareReleaseBuild(const fs::path &buildDir, const std::string &signature,
                             std::string &error)
    {
        const auto marker = buildDir / kReleaseMarker;
        std::error_code ec;
        const bool exists = fs::exists(buildDir, ec);
        if(exists && (!fs::exists(marker, ec) || ReadTextFile(marker) != signature))
        {
            fs::remove_all(buildDir, ec);
            if(ec)
            {
                error = "Unable to reset incompatible Release build: " + ec.message();
                return false;
            }
        }

        fs::create_directories(buildDir, ec);
        if(ec)
        {
            error = "Unable to create Release build directory: " + ec.message();
            return false;
        }
        return true;
    }

    bool WriteReleaseBuildMarker(const fs::path &buildDir, const std::string &signature)
    {
        std::ofstream marker(buildDir / kReleaseMarker, std::ios::binary | std::ios::trunc);
        if(!marker)
        {
            return false;
        }
        marker << signature;
        return static_cast<bool>(marker);
    }

    // ---------------------------------------------------------------- Pipeline

    Pipeline::Pipeline(Options options, skr::Arc<IProcessRunner> runner,
                       skr::Arc<IAssetService> assets, skr::Arc<skr::Logger<Pipeline>> logger,
                       Observer observer) :
        mOptions(std::move(options)),
        mRunner(std::move(runner)), mAssets(std::move(assets)),
        mLogger(std::move(logger)), mObserver(std::move(observer))
    {
    }

    PipelineFactory::PipelineFactory(skr::Arc<IProcessRunner> runner,
                                     skr::Arc<IAssetService> assets,
                                     skr::Arc<skr::Logger<Pipeline>> logger) :
        mRunner(std::move(runner)), mAssets(std::move(assets)), mLogger(std::move(logger))
    {
    }

    Pipeline PipelineFactory::Create(Options options, Observer observer) const
    {
        return Pipeline(std::move(options), mRunner, mAssets, mLogger, std::move(observer));
    }

    void Pipeline::progress(Stage stage, float fraction, bool determinate)
    {
        if(mLogger)
        {
            mLogger->LogDebug("publish stage={} progress={:.2f} determinate={}", ToString(stage),
                              static_cast<double>(fraction), determinate);
        }
        if(!mObserver.onProgress)
        {
            return;
        }
        const auto range = StageRange(stage);
        mObserver.onProgress(stage, range.begin + (range.end - range.begin) * fraction,
                             determinate);
    }

    void Pipeline::log(std::string_view line)
    {
        if(mLogger)
        {
            // Process output is verbose; keep it at Debug so the file log
            // stays complete without flooding the console at Information.
            mLogger->LogDebug("{}", line);
        }
        if(mObserver.onLog)
        {
            mObserver.onLog(line);
        }
    }

    bool Pipeline::fail(std::string message, int exitCode)
    {
        if(mReport.error.empty())
        {
            mReport.error = message;
        }
        mReport.exitCode   = exitCode == 0 ? 1 : exitCode;
        mReport.failedStage = mCurrent;
        if(mLogger)
        {
            mLogger->LogError("publish stage={} failed: {}", ToString(mCurrent), mReport.error);
        }
        if(mObserver.onLog)
        {
            mObserver.onLog(mReport.error + "\n");
        }
        return false;
    }

    Report Pipeline::Run()
    {
        if(!mRunner || !mAssets)
        {
            mCurrent = Stage::Validate;
            fail(!mRunner ? "Process runner not injected (Skirnir wiring missing)"
                          : "Asset service not injected (Skirnir wiring missing)");
            mReport.ok = false;
            return std::move(mReport);
        }

        using Step = bool (Pipeline::*)();
        struct Entry
        {
            Stage stage;
            Step step;
        };
        const Entry steps[] = {
            {Stage::Validate, &Pipeline::validate}, {Stage::Configure, &Pipeline::configure},
            {Stage::Build, &Pipeline::build},       {Stage::Install, &Pipeline::install},
            {Stage::Cook, &Pipeline::cook},         {Stage::Stage, &Pipeline::stageProject},
            {Stage::Finalize, &Pipeline::finalize},
        };

        if(mLogger)
        {
            mLogger->LogInformation("Publishing {} profile={} to {}", mOptions.projectRoot.string(),
                                    ToString(mOptions.profile), mOptions.destination.string());
        }

        bool ok = true;
        for(const auto &entry: steps)
        {
            mCurrent = entry.stage;
            if(mLogger)
            {
                mLogger->LogInformation("publish stage started: {}", ToString(entry.stage));
            }
            progress(entry.stage, 0.0f, false);
            const auto begin = std::chrono::steady_clock::now();
            ok               = (this->*entry.step)();
            const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - begin;
            mReport.timings.push_back({entry.stage, elapsed.count()});
            if(!ok)
            {
                break;
            }
            progress(entry.stage, 1.0f);
        }

        if(!ok)
        {
            // Failed publications never touch the destination; drop the partial staging.
            std::error_code ec;
            if(!mStaging.empty())
            {
                fs::remove_all(mStaging, ec);
                if(ec && mLogger)
                {
                    mLogger->LogWarning("Unable to clean publish staging {}: {}", mStaging.string(),
                                        ec.message());
                }
            }
        }
        else if(mLogger)
        {
            mLogger->LogInformation("Publish succeeded -> {} ({} files)",
                                    mReport.destination.string(), mReport.files.size());
        }
        mReport.ok = ok;
        if(ok)
        {
            mReport.exitCode = 0;
        }
        return std::move(mReport);
    }

    bool Pipeline::validate()
    {
        std::error_code ec;
        const auto &root = mOptions.projectRoot;
        if(root.empty() || !fs::exists(root / ProjectFile::FileName, ec))
        {
            return fail("Project file not found in " + root.string());
        }
        if(!fs::exists(root / "CMakeLists.txt", ec))
        {
            return fail("Project CMakeLists.txt not found in " + root.string());
        }
        if(mOptions.destination.empty())
        {
            return fail("Publication destination is empty");
        }

        auto destination = fs::absolute(mOptions.destination, ec).lexically_normal();
        if(!destination.has_filename())
        {
            destination = destination.parent_path();
        }
        if(destination.empty() || !destination.has_filename())
        {
            return fail("Invalid publication destination: " + mOptions.destination.string());
        }
        const auto canonicalRoot = fs::weakly_canonical(root, ec).lexically_normal();
        if(IsSameOrInside(destination, canonicalRoot))
        {
            return fail("Publication destination must not contain the project: " +
                        destination.string());
        }
        if(IsSameOrInside(canonicalRoot, destination))
        {
            return fail("Publication destination must be outside the project folder: " +
                        destination.string());
        }
        mReport.destination = destination;
        if(fs::exists(destination, ec))
        {
            if(!fs::is_directory(destination, ec))
            {
                return fail("Publication destination is not a directory: " + destination.string());
            }
            if(!fs::is_empty(destination, ec) && !mOptions.cleanDestination)
            {
                return fail("Publication destination must be empty (or pass --clean): " +
                            destination.string());
            }
        }

        const auto &branding = mOptions.descriptor.branding;
        if(!IsValidVersion(branding.version))
        {
            return fail("Invalid version '" + branding.version +
                        "': expected MAJOR.MINOR.PATCH (numbers only)");
        }
        for(const auto *icon: {&branding.iconWindows, &branding.iconLinux, &branding.iconMacOS})
        {
            if(!icon->empty() && !fs::exists(ResolveAgainst(root, *icon), ec))
            {
                return fail("Icon file not found: " + icon->string());
            }
        }

        mSdk = mOptions.descriptor.friggaSdk;
        if(mSdk.empty() || !fs::exists(mSdk / "cmake" / "FriggaSdk.cmake", ec))
        {
            return fail("Frigga SDK not found (expected cmake/FriggaSdk.cmake under '" +
                        mSdk.string() + "')");
        }

        const auto resources = root / ProjectDescriptor::ResourcesDirName;
        if(fs::exists(resources, ec))
        {
            const auto validation = mAssets->ValidateManifest(resources);
            if(!validation.ok)
            {
                return fail(validation.error.empty() ? "Invalid asset manifest" : validation.error);
            }
        }

        mBuildDir = mOptions.buildDir.empty() ? root / "build-release" : mOptions.buildDir;
        mStaging  = destination.parent_path() /
                   ("." + destination.filename().string() + ".frigga-staging");
        return true;
    }

    bool Pipeline::configure()
    {
        mSignature =
            ReleaseBuildSignature(mOptions.projectRoot, mSdk, mOptions.cxxCompiler, mOptions.profile);
        std::string error;
        if(!PrepareReleaseBuild(mBuildDir, mSignature, error))
        {
            return fail(std::move(error));
        }

        const auto &desc = mOptions.descriptor;
        std::string cmd  = "cmake -S " + QuotePath(mOptions.projectRoot) + " -B " +
                           QuotePath(mBuildDir) +
                           " -G Ninja -DCMAKE_BUILD_TYPE=Release"
                           " -DCMAKE_CXX_STANDARD=26"
                           " -DCMAKE_CXX_STANDARD_REQUIRED=ON"
                           " -DCMAKE_CXX_EXTENSIONS=ON"
                           " -DFRIGGA_MODULES_LINK_GAME=ON";
        const auto appendPath = [&cmd](const char *name, const fs::path &path) {
            if(!path.empty())
            {
                cmd += std::string(" -D") + name + "=" + QuotePath(path);
            }
        };
        // Always pass SDK paths so CMakeCache is overwritten on host/OS switches.
        appendPath("FRIGGA_SDK", desc.friggaSdk);
        appendPath("FRIGGA_BUILD", desc.friggaBuild);
        if(!mOptions.cxxCompiler.empty())
        {
            cmd += " -DCMAKE_CXX_COMPILER=" + QuotePath(mOptions.cxxCompiler);
        }

        const int code = mRunner->Run(cmd, [this](std::string_view line) { log(line); });
        if(code != 0)
        {
            return fail("CMake configure failed (exit " + std::to_string(code) + ")", code);
        }
        if(!WriteReleaseBuildMarker(mBuildDir, mSignature))
        {
            return fail("Unable to write Release build configuration marker");
        }
        return true;
    }

    bool Pipeline::build()
    {
        const auto cmd = "cmake --build " + QuotePath(mBuildDir) + " --parallel";
        const int code = mRunner->Run(cmd, [this](std::string_view line) {
            log(line);
            float fraction = 0.0f;
            if(TryParseNinjaProgress(line, fraction))
            {
                progress(Stage::Build, fraction, true);
            }
        });
        if(code != 0)
        {
            return fail("Build failed (exit " + std::to_string(code) + ")", code);
        }
        return true;
    }

    bool Pipeline::install()
    {
        std::error_code ec;
        fs::remove_all(mStaging, ec);
        fs::create_directories(mStaging, ec);
        if(ec)
        {
            return fail("Unable to create publication staging directory: " + ec.message());
        }
        const auto cmd =
            "cmake --install " + QuotePath(mBuildDir) + " --prefix " + QuotePath(mStaging);
        const int code = mRunner->Run(cmd, [this](std::string_view line) { log(line); });
        if(code != 0)
        {
            return fail("Install failed (exit " + std::to_string(code) + ")", code);
        }
        return true;
    }

    bool Pipeline::cook()
    {
        const auto projectResources = mOptions.projectRoot / ProjectDescriptor::ResourcesDirName;
        std::error_code ec;
        if(!fs::exists(projectResources, ec))
        {
            return true;
        }
        // Engine resources were installed into staging/Resources; project assets are
        // cooked over them (single merged tree, as the Runtime expects).
        const auto published = mStaging / ProjectDescriptor::ResourcesDirName;
        const auto outcome   = mAssets->Cook(projectResources, published);
        if(!outcome.ok)
        {
            return fail("Unable to cook project resources: " + outcome.error);
        }
        if(mLogger)
        {
            mLogger->LogInformation("Cooked {} asset(s)", outcome.cookedCount);
        }
        log("Cooked " + std::to_string(outcome.cookedCount) + " asset(s)\n");
        return true;
    }

    bool Pipeline::stageProject()
    {
        auto published = mOptions.descriptor;
        published.friggaSdk.clear();
        published.friggaRoot.clear();
        published.friggaBuild.clear();
        for(auto &entry: published.modules)
        {
            const auto target     = entry.target.empty() ? entry.id : entry.target;
            entry.libraryRelative = PublishedModuleLibrary(target);
        }
        if(!ProjectFile::Save(mStaging / ProjectFile::FileName, published))
        {
            return fail("Unable to write sanitized published project manifest");
        }
        return true;
    }

    bool Pipeline::finalize()
    {
        std::error_code ec;

        // Deterministic file inventory with content hashes. The release
        // manifest itself is written afterwards, so it is intentionally
        // not part of the inventory (no self-hash).
        std::vector<fs::path> files;
        for(const auto &entry: fs::recursive_directory_iterator(mStaging, ec))
        {
            if(entry.is_regular_file(ec))
            {
                files.push_back(entry.path());
            }
        }
        if(ec)
        {
            return fail("Unable to scan staged files: " + ec.message());
        }
        std::ranges::sort(files);
        for(const auto &file: files)
        {
            const auto hash = HashFileHex(file);
            if(!hash)
            {
                return fail("Unable to hash staged file: " + file.string());
            }
            FileEntry entry;
            entry.path = fs::relative(file, mStaging, ec).generic_string();
            entry.size = fs::file_size(file, ec);
            entry.hash = *hash;
            mReport.files.push_back(std::move(entry));
        }

        const auto &desc = mOptions.descriptor;
        std::ostringstream json;
        json << "{\n";
        json << "  \"schema\": 1,\n";
        json << "  \"name\": \"" << EscapeJson(desc.name) << "\",\n";
        json << "  \"displayName\": \"" << EscapeJson(desc.branding.displayName) << "\",\n";
        json << "  \"identifier\": \"" << EscapeJson(desc.branding.identifier) << "\",\n";
        json << "  \"publisher\": \"" << EscapeJson(desc.branding.publisher) << "\",\n";
        json << "  \"version\": \"" << EscapeJson(desc.branding.version) << "\",\n";
        json << "  \"profile\": \"" << ToString(mOptions.profile) << "\",\n";
        json << "  \"platform\": \"" << kPlatform << "\",\n";
        json << "  \"compiler\": \"" << EscapeJson(mOptions.cxxCompiler) << "\",\n";
        json << "  \"buildSignature\": \"" << Fnv1a64Hex(mSignature) << "\",\n";
        json << "  \"createdUtc\": \"" << FormatUtcTimestamp() << "\",\n";
        json << "  \"stages\": [";
        for(std::size_t i = 0; i < mReport.timings.size(); ++i)
        {
            const auto &timing = mReport.timings[i];
            json << (i == 0 ? "\n" : ",\n") << "    {\"stage\": \"" << ToString(timing.stage)
                 << "\", \"seconds\": " << timing.seconds << "}";
        }
        json << "\n  ],\n";
        json << "  \"files\": [";
        for(std::size_t i = 0; i < mReport.files.size(); ++i)
        {
            const auto &file = mReport.files[i];
            json << (i == 0 ? "\n" : ",\n") << "    {\"path\": \"" << EscapeJson(file.path)
                 << "\", \"size\": " << file.size << ", \"hash\": \"" << file.hash << "\"}";
        }
        json << "\n  ]\n}\n";

        {
            std::ofstream out(mStaging / ReleaseManifestName, std::ios::binary | std::ios::trunc);
            if(!out)
            {
                return fail("Unable to write release manifest");
            }
            out << json.str();
        }

        // Swap into place. Staging sits next to the destination so rename stays on one volume
        // and the destination is either the old tree or the complete new one.
        const auto &destination = mReport.destination;
        fs::create_directories(destination.parent_path(), ec);
        fs::path backup;
        if(fs::exists(destination, ec))
        {
            backup = destination.parent_path() /
                     ("." + destination.filename().string() + ".frigga-old");
            fs::remove_all(backup, ec);
            fs::rename(destination, backup, ec);
            if(ec)
            {
                return fail("Unable to replace existing destination: " + ec.message());
            }
        }
        fs::rename(mStaging, destination, ec);
        if(ec)
        {
            const auto message = ec.message();
            if(!backup.empty())
            {
                fs::rename(backup, destination, ec); // restore the previous tree
            }
            return fail("Unable to move publication into place: " + message);
        }
        if(!backup.empty())
        {
            fs::remove_all(backup, ec);
        }
        mStaging.clear();
        mReport.releaseManifest = destination / ReleaseManifestName;
        return true;
    }
} // namespace publish
