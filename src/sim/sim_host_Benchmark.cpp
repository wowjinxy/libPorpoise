#include <simulator/sim_host_Benchmark.h>
#include <simulator/sim_host_Allocator.hpp>

#include <SDL2/SDL.h>
#include <simulator/glad/glad.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr std::uint64_t DefaultEndFrame = 599u;
constexpr int CaptureWidth = 640;
constexpr int CaptureHeight = 480;
constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

enum class VertexStreamRequest {
    Auto,
    Legacy,
};

struct BenchmarkConfig {
    bool enabled = false;
    bool noPacing = false;
    bool neutralInput = false;
    std::uint64_t startFrame = 0u;
    std::uint64_t endFrame = DefaultEndFrame;
    std::string label = "benchmark";
    std::filesystem::path outputDirectory;
    std::vector<std::uint64_t> captureFrames;
    VertexStreamRequest vertexStream = VertexStreamRequest::Auto;
};

struct GlCounters {
    std::uint64_t uniformCalls = 0u;
    std::uint64_t bufferDataCalls = 0u;
    std::uint64_t bufferDataBytes = 0u;
    std::uint64_t bufferStorageCalls = 0u;
    std::uint64_t bufferStorageBytes = 0u;
    std::uint64_t bufferMapCalls = 0u;
    std::uint64_t bufferMapBytes = 0u;
    std::uint64_t bufferMapSuccesses = 0u;
    std::uint64_t fenceCalls = 0u;
    std::uint64_t fenceWaitCalls = 0u;
    std::uint64_t drawCalls = 0u;
    std::uint64_t drawVertices = 0u;
    std::uint64_t textureUploadCalls = 0u;
    std::uint64_t textureUploadBytes = 0u;
};

struct FrameRecord {
    std::uint64_t presentId = 0u;
    std::uint32_t retraceId = 0u;
    int drawableWidth = 0;
    int drawableHeight = 0;
    double intervalMs = 0.0;
    double workMs = 0.0;
    double presentMs = 0.0;
    double swapMs = 0.0;
    double paceMs = 0.0;
    double captureMs = 0.0;
    GlCounters counters;
    bool captured = false;
    std::string captureFile;
    std::uint64_t captureHash = 0u;
};

BenchmarkConfig Config;
bool Configured = false;
bool ConfigureSucceeded = false;
bool GlInitialized = false;
bool CounterWrappersInstalled = false;
bool BufferStorageAdvertised = false;
bool BufferStorageEffective = false;
std::string GlVendor;
std::string GlRenderer;
std::string GlVersion;
GlCounters FrameCounters;
GlCounters LifetimeCounters;
std::vector<FrameRecord> Frames;
FrameRecord PendingFrame;
bool PendingFrameActive = false;
bool PendingFramePresented = false;
std::uint64_t NextPresentId = 0u;
std::uint64_t LastPreSwapTick = 0u;
std::uint64_t LastPresentedRetraceEndTick = 0u;
std::uint64_t PresentStartTick = 0u;
double AccumulatedPaceMs = 0.0;
double FinalGpuDrainMs = 0.0;
std::uint64_t MeasurementStartTick = 0u;
std::uint64_t MeasurementEndTick = 0u;
std::uint64_t MeasurementFrequency = 0u;
bool MeasurementStarted = false;
bool MeasurementStartApproximate = false;
int LastDrawableWidth = 0;
int LastDrawableHeight = 0;

PFNGLUNIFORMMATRIX4FVPROC RealUniformMatrix4fv = nullptr;
PFNGLUNIFORM1IPROC RealUniform1i = nullptr;
PFNGLUNIFORM1IVPROC RealUniform1iv = nullptr;
PFNGLUNIFORM4IVPROC RealUniform4iv = nullptr;
PFNGLUNIFORM2IVPROC RealUniform2iv = nullptr;
PFNGLUNIFORM4FVPROC RealUniform4fv = nullptr;
PFNGLUNIFORM1FVPROC RealUniform1fv = nullptr;
PFNGLUNIFORM3FVPROC RealUniform3fv = nullptr;
PFNGLUNIFORM1FPROC RealUniform1f = nullptr;
PFNGLUNIFORM1UIPROC RealUniform1ui = nullptr;
PFNGLBUFFERDATAPROC RealBufferData = nullptr;
PFNGLBUFFERSTORAGEPROC RealBufferStorage = nullptr;
PFNGLMAPBUFFERRANGEPROC RealMapBufferRange = nullptr;
PFNGLFENCESYNCPROC RealFenceSync = nullptr;
PFNGLCLIENTWAITSYNCPROC RealClientWaitSync = nullptr;
PFNGLDRAWARRAYSPROC RealDrawArrays = nullptr;
PFNGLTEXIMAGE2DPROC RealTexImage2D = nullptr;

double TicksToMilliseconds(std::uint64_t ticks, std::uint64_t frequency) {
    if (frequency == 0u) {
        return 0.0;
    }
    return static_cast<double>(ticks) * 1000.0 /
           static_cast<double>(frequency);
}

bool ParseUnsigned(const char* text, std::uint64_t& value) {
    if (text == nullptr || *text == '\0') {
        return false;
    }
    const char* end = text;
    while (*end != '\0') {
        if (*end < '0' || *end > '9') {
            return false;
        }
        ++end;
    }
    const auto result = std::from_chars(text, end, value, 10);
    return result.ec == std::errc() && result.ptr == end;
}

bool ParseOptionalUnsigned(
    const char* name,
    std::uint64_t defaultValue,
    std::uint64_t& value) {
    const char* text = std::getenv(name);
    if (text == nullptr) {
        value = defaultValue;
        return true;
    }
    if (!ParseUnsigned(text, value)) {
        std::fprintf(
            stderr,
            "libPorpoise benchmark: %s must be an unsigned decimal integer\n",
            name);
        return false;
    }
    return true;
}

bool ParseOptionalBoolean(const char* name, bool& value) {
    const char* text = std::getenv(name);
    if (text == nullptr) {
        value = false;
        return true;
    }
    if (std::string_view(text) == "0") {
        value = false;
        return true;
    }
    if (std::string_view(text) == "1") {
        value = true;
        return true;
    }
    std::fprintf(
        stderr,
        "libPorpoise benchmark: %s must be exactly 0 or 1\n",
        name);
    return false;
}

bool ParseCaptureFrames(
    const char* text,
    std::vector<std::uint64_t>& frames) {
    frames.clear();
    if (text == nullptr || *text == '\0') {
        return true;
    }

    const std::string_view input(text);
    std::size_t begin = 0u;
    while (begin < input.size()) {
        const std::size_t comma = input.find(',', begin);
        const std::size_t end =
            comma == std::string_view::npos ? input.size() : comma;
        if (end == begin) {
            return false;
        }
        const std::string token(input.substr(begin, end - begin));
        std::uint64_t frame = 0u;
        if (!ParseUnsigned(token.c_str(), frame)) {
            return false;
        }
        frames.push_back(frame);
        if (comma == std::string_view::npos) {
            break;
        }
        begin = comma + 1u;
        if (begin == input.size()) {
            return false;
        }
    }

    std::sort(frames.begin(), frames.end());
    if (std::adjacent_find(frames.begin(), frames.end()) != frames.end()) {
        return false;
    }
    return true;
}

bool ValidateLabel(const std::string& label) {
    if (label.empty() || label.size() > 128u) {
        return false;
    }
    return std::all_of(
        label.begin(),
        label.end(),
        [](unsigned char value) { return value >= 0x20u && value != 0x7fu; });
}

std::string JsonEscape(const std::string& value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20u) {
                    output << "\\u"
                           << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<unsigned int>(character)
                           << std::dec << std::setfill(' ');
                } else {
                    output << static_cast<char>(character);
                }
                break;
        }
    }
    return output.str();
}

std::string CsvEscape(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) {
        return value;
    }
    std::string escaped = "\"";
    for (const char character : value) {
        if (character == '"') {
            escaped += "\"\"";
        } else {
            escaped += character;
        }
    }
    escaped += '"';
    return escaped;
}

std::string GlString(GLenum name) {
    const GLubyte* value = glGetString(name);
    return value == nullptr
        ? std::string()
        : std::string(reinterpret_cast<const char*>(value));
}

void AddUniformCall(void) {
    ++FrameCounters.uniformCalls;
    ++LifetimeCounters.uniformCalls;
}

void APIENTRY CountUniformMatrix4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
    AddUniformCall();
    RealUniformMatrix4fv(location, count, transpose, value);
}

void APIENTRY CountUniform1i(GLint location, GLint value) {
    AddUniformCall();
    RealUniform1i(location, value);
}

void APIENTRY CountUniform1iv(
    GLint location,
    GLsizei count,
    const GLint* value) {
    AddUniformCall();
    RealUniform1iv(location, count, value);
}

void APIENTRY CountUniform4iv(
    GLint location,
    GLsizei count,
    const GLint* value) {
    AddUniformCall();
    RealUniform4iv(location, count, value);
}

void APIENTRY CountUniform2iv(
    GLint location,
    GLsizei count,
    const GLint* value) {
    AddUniformCall();
    RealUniform2iv(location, count, value);
}

void APIENTRY CountUniform4fv(
    GLint location,
    GLsizei count,
    const GLfloat* value) {
    AddUniformCall();
    RealUniform4fv(location, count, value);
}

void APIENTRY CountUniform1fv(
    GLint location,
    GLsizei count,
    const GLfloat* value) {
    AddUniformCall();
    RealUniform1fv(location, count, value);
}

void APIENTRY CountUniform3fv(
    GLint location,
    GLsizei count,
    const GLfloat* value) {
    AddUniformCall();
    RealUniform3fv(location, count, value);
}

void APIENTRY CountUniform1f(GLint location, GLfloat value) {
    AddUniformCall();
    RealUniform1f(location, value);
}

void APIENTRY CountUniform1ui(GLint location, GLuint value) {
    AddUniformCall();
    RealUniform1ui(location, value);
}

std::uint64_t PositiveSize(GLsizeiptr size) {
    return size > 0 ? static_cast<std::uint64_t>(size) : 0u;
}

void APIENTRY CountBufferData(
    GLenum target,
    GLsizeiptr size,
    const void* data,
    GLenum usage) {
    const std::uint64_t bytes = PositiveSize(size);
    ++FrameCounters.bufferDataCalls;
    FrameCounters.bufferDataBytes += bytes;
    ++LifetimeCounters.bufferDataCalls;
    LifetimeCounters.bufferDataBytes += bytes;
    RealBufferData(target, size, data, usage);
}

void APIENTRY CountBufferStorage(
    GLenum target,
    GLsizeiptr size,
    const void* data,
    GLbitfield flags) {
    const std::uint64_t bytes = PositiveSize(size);
    ++FrameCounters.bufferStorageCalls;
    FrameCounters.bufferStorageBytes += bytes;
    ++LifetimeCounters.bufferStorageCalls;
    LifetimeCounters.bufferStorageBytes += bytes;
    RealBufferStorage(target, size, data, flags);
}

void* APIENTRY CountMapBufferRange(
    GLenum target,
    GLintptr offset,
    GLsizeiptr length,
    GLbitfield access) {
    const std::uint64_t bytes = PositiveSize(length);
    ++FrameCounters.bufferMapCalls;
    FrameCounters.bufferMapBytes += bytes;
    ++LifetimeCounters.bufferMapCalls;
    LifetimeCounters.bufferMapBytes += bytes;
    void* result = RealMapBufferRange(target, offset, length, access);
    if (result != nullptr) {
        ++FrameCounters.bufferMapSuccesses;
        ++LifetimeCounters.bufferMapSuccesses;
    }
    return result;
}

GLsync APIENTRY CountFenceSync(GLenum condition, GLbitfield flags) {
    ++FrameCounters.fenceCalls;
    ++LifetimeCounters.fenceCalls;
    return RealFenceSync(condition, flags);
}

GLenum APIENTRY CountClientWaitSync(
    GLsync sync,
    GLbitfield flags,
    GLuint64 timeout) {
    ++FrameCounters.fenceWaitCalls;
    ++LifetimeCounters.fenceWaitCalls;
    return RealClientWaitSync(sync, flags, timeout);
}

void APIENTRY CountDrawArrays(GLenum mode, GLint first, GLsizei count) {
    const std::uint64_t vertices =
        count > 0 ? static_cast<std::uint64_t>(count) : 0u;
    ++FrameCounters.drawCalls;
    FrameCounters.drawVertices += vertices;
    ++LifetimeCounters.drawCalls;
    LifetimeCounters.drawVertices += vertices;
    RealDrawArrays(mode, first, count);
}

std::uint64_t TextureUploadBytes(
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type) {
    if (width <= 0 || height <= 0 || type != GL_UNSIGNED_BYTE) {
        return 0u;
    }
    std::uint64_t components = 0u;
    switch (format) {
        case GL_RED: components = 1u; break;
        case GL_RG: components = 2u; break;
        case GL_RGB: components = 3u; break;
        case GL_RGBA:
        case GL_BGRA: components = 4u; break;
        default: return 0u;
    }
    return static_cast<std::uint64_t>(width) *
           static_cast<std::uint64_t>(height) * components;
}

void APIENTRY CountTexImage2D(
    GLenum target,
    GLint level,
    GLint internalFormat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    const void* pixels) {
    const std::uint64_t bytes = TextureUploadBytes(width, height, format, type);
    ++FrameCounters.textureUploadCalls;
    FrameCounters.textureUploadBytes += bytes;
    ++LifetimeCounters.textureUploadCalls;
    LifetimeCounters.textureUploadBytes += bytes;
    RealTexImage2D(
        target,
        level,
        internalFormat,
        width,
        height,
        border,
        format,
        type,
        pixels);
}

void InstallCounterWrappers(void) {
    if (CounterWrappersInstalled) {
        return;
    }

    RealUniformMatrix4fv = glad_glUniformMatrix4fv;
    RealUniform1i = glad_glUniform1i;
    RealUniform1iv = glad_glUniform1iv;
    RealUniform4iv = glad_glUniform4iv;
    RealUniform2iv = glad_glUniform2iv;
    RealUniform4fv = glad_glUniform4fv;
    RealUniform1fv = glad_glUniform1fv;
    RealUniform3fv = glad_glUniform3fv;
    RealUniform1f = glad_glUniform1f;
    RealUniform1ui = glad_glUniform1ui;
    RealBufferData = glad_glBufferData;
    RealBufferStorage = glad_glBufferStorage;
    RealMapBufferRange = glad_glMapBufferRange;
    RealFenceSync = glad_glFenceSync;
    RealClientWaitSync = glad_glClientWaitSync;
    RealDrawArrays = glad_glDrawArrays;
    RealTexImage2D = glad_glTexImage2D;

    glad_glUniformMatrix4fv = CountUniformMatrix4fv;
    glad_glUniform1i = CountUniform1i;
    glad_glUniform1iv = CountUniform1iv;
    glad_glUniform4iv = CountUniform4iv;
    glad_glUniform2iv = CountUniform2iv;
    glad_glUniform4fv = CountUniform4fv;
    glad_glUniform1fv = CountUniform1fv;
    glad_glUniform3fv = CountUniform3fv;
    glad_glUniform1f = CountUniform1f;
    glad_glUniform1ui = CountUniform1ui;
    glad_glBufferData = CountBufferData;
    if (RealBufferStorage != nullptr) {
        glad_glBufferStorage = CountBufferStorage;
    }
    if (RealMapBufferRange != nullptr) {
        glad_glMapBufferRange = CountMapBufferRange;
    }
    if (RealFenceSync != nullptr) {
        glad_glFenceSync = CountFenceSync;
    }
    if (RealClientWaitSync != nullptr) {
        glad_glClientWaitSync = CountClientWaitSync;
    }
    glad_glDrawArrays = CountDrawArrays;
    glad_glTexImage2D = CountTexImage2D;
    CounterWrappersInstalled = true;
}

void WriteLe16(std::array<std::uint8_t, 54>& header, std::size_t offset, std::uint16_t value) {
    header[offset] = static_cast<std::uint8_t>(value & 0xffu);
    header[offset + 1u] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
}

void WriteLe32(std::array<std::uint8_t, 54>& header, std::size_t offset, std::uint32_t value) {
    header[offset] = static_cast<std::uint8_t>(value & 0xffu);
    header[offset + 1u] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
    header[offset + 2u] = static_cast<std::uint8_t>((value >> 16u) & 0xffu);
    header[offset + 3u] = static_cast<std::uint8_t>((value >> 24u) & 0xffu);
}

std::uint64_t HashBytes(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t hash = FnvOffsetBasis;
    for (const std::uint8_t value : bytes) {
        hash ^= value;
        hash *= FnvPrime;
    }
    return hash;
}

bool WriteBmp(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& rgba) {
    constexpr std::uint32_t pixelBytes =
        static_cast<std::uint32_t>(CaptureWidth * CaptureHeight * 4);
    constexpr std::uint32_t fileBytes = 54u + pixelBytes;

    if (rgba.size() != pixelBytes) {
        return false;
    }

    std::array<std::uint8_t, 54> header = {};
    header[0] = 'B';
    header[1] = 'M';
    WriteLe32(header, 2u, fileBytes);
    WriteLe32(header, 10u, 54u);
    WriteLe32(header, 14u, 40u);
    WriteLe32(header, 18u, CaptureWidth);
    WriteLe32(header, 22u, CaptureHeight);
    WriteLe16(header, 26u, 1u);
    WriteLe16(header, 28u, 32u);
    WriteLe32(header, 34u, pixelBytes);

    std::vector<std::uint8_t> bgra(rgba.size());
    for (std::size_t pixel = 0u; pixel < rgba.size(); pixel += 4u) {
        bgra[pixel] = rgba[pixel + 2u];
        bgra[pixel + 1u] = rgba[pixel + 1u];
        bgra[pixel + 2u] = rgba[pixel];
        bgra[pixel + 3u] = rgba[pixel + 3u];
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(
        reinterpret_cast<const char*>(header.data()),
        static_cast<std::streamsize>(header.size()));
    output.write(
        reinterpret_cast<const char*>(bgra.data()),
        static_cast<std::streamsize>(bgra.size()));
    output.close();
    return output.good();
}

bool ShouldCapture(std::uint64_t presentId) {
    return std::binary_search(
        Config.captureFrames.begin(),
        Config.captureFrames.end(),
        presentId);
}

bool CaptureBackBuffer(FrameRecord& frame, std::string& error) {
    if (frame.drawableWidth != CaptureWidth ||
        frame.drawableHeight != CaptureHeight) {
        std::ostringstream message;
        message << "capture frame " << frame.presentId
                << " requires a 640x480 drawable, got "
                << frame.drawableWidth << 'x' << frame.drawableHeight;
        error = message.str();
        return false;
    }

    std::vector<std::uint8_t> rgba(
        static_cast<std::size_t>(CaptureWidth) *
        static_cast<std::size_t>(CaptureHeight) * 4u);
    GLint previousPackAlignment = 4;
    GLint previousReadBuffer = GL_BACK;
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    while (glGetError() != GL_NO_ERROR) {
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(
        0,
        0,
        CaptureWidth,
        CaptureHeight,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba.data());
    const GLenum readError = glGetError();
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    if (readError != GL_NO_ERROR) {
        std::ostringstream message;
        message << "glReadPixels failed for frame " << frame.presentId
                << " with GL error 0x" << std::hex << readError;
        error = message.str();
        return false;
    }

    std::ostringstream fileName;
    fileName << "frame-" << std::setw(10) << std::setfill('0')
             << frame.presentId << ".bmp";
    const std::filesystem::path outputPath =
        Config.outputDirectory / fileName.str();
    if (!WriteBmp(outputPath, rgba)) {
        error = "could not write " + outputPath.string();
        return false;
    }

    frame.captured = true;
    frame.captureFile = fileName.str();
    frame.captureHash = HashBytes(rgba);
    return true;
}

std::string RequestedVertexStreamName(void) {
    return Config.vertexStream == VertexStreamRequest::Legacy
        ? "legacy"
        : "auto";
}

std::string ObservedVertexStreamName(void) {
    if (LifetimeCounters.drawCalls == 0u) {
        return "uninitialized";
    }
    if (LifetimeCounters.bufferStorageCalls == 0u) {
        return "legacy";
    }
    if (LifetimeCounters.bufferMapSuccesses != 0u) {
        return "persistent";
    }
    return "legacy_fallback";
}

bool WriteFramesCsv(void) {
    const std::filesystem::path path = Config.outputDirectory / "frames.csv";
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        return false;
    }
    output
        << "schema_version,label,phase,present_id,retrace_id,drawable_width,drawable_height,"
        << "interval_ms,work_ms,present_ms,swap_ms,pace_ms,capture_ms,"
        << "uniform_calls,buffer_data_calls,buffer_data_bytes,"
        << "buffer_storage_calls,buffer_storage_bytes,buffer_map_calls,"
        << "buffer_map_bytes,fence_calls,fence_wait_calls,draw_calls,"
        << "draw_vertices,texture_upload_calls,texture_upload_bytes,"
        << "captured,capture_file,capture_pixel_fnv1a64\n";
    output << std::fixed << std::setprecision(6);
    for (const FrameRecord& frame : Frames) {
        output
            << 1 << ','
            << CsvEscape(Config.label) << ','
            << "measured,"
            << frame.presentId << ','
            << frame.retraceId << ','
            << frame.drawableWidth << ','
            << frame.drawableHeight << ','
            << frame.intervalMs << ','
            << frame.workMs << ','
            << frame.presentMs << ','
            << frame.swapMs << ','
            << frame.paceMs << ','
            << frame.captureMs << ','
            << frame.counters.uniformCalls << ','
            << frame.counters.bufferDataCalls << ','
            << frame.counters.bufferDataBytes << ','
            << frame.counters.bufferStorageCalls << ','
            << frame.counters.bufferStorageBytes << ','
            << frame.counters.bufferMapCalls << ','
            << frame.counters.bufferMapBytes << ','
            << frame.counters.fenceCalls << ','
            << frame.counters.fenceWaitCalls << ','
            << frame.counters.drawCalls << ','
            << frame.counters.drawVertices << ','
            << frame.counters.textureUploadCalls << ','
            << frame.counters.textureUploadBytes << ','
            << (frame.captured ? 1 : 0) << ','
            << CsvEscape(frame.captureFile) << ',';
        if (frame.captured) {
            output << std::hex << std::setw(16) << std::setfill('0')
                   << frame.captureHash
                   << std::dec << std::setfill(' ');
        }
        output << '\n';
    }
    output.close();
    return output.good();
}

bool WriteMetadata(const char* status, const std::string& error) {
    const std::filesystem::path path = Config.outputDirectory / "metadata.json";
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        return false;
    }

    output << "{\n";
    output << "  \"schema_version\": 1,\n";
    output << "  \"status\": \"" << JsonEscape(status) << "\",\n";
    output << "  \"label\": \"" << JsonEscape(Config.label) << "\",\n";
    output << "  \"frame_id_kind\": \"present_id\",\n";
    output << "  \"frame_range\": \"inclusive\",\n";
    output << "  \"start_frame\": " << Config.startFrame << ",\n";
    output << "  \"end_frame\": " << Config.endFrame << ",\n";
    output << "  \"no_pacing\": " << (Config.noPacing ? "true" : "false") << ",\n";
    output << "  \"warmup_pacing\": "
           << (Config.noPacing && Config.startFrame != 0u ? "true" : "false")
           << ",\n";
    output << "  \"neutral_input\": " << (Config.neutralInput ? "true" : "false") << ",\n";
    output << "  \"gx_vertex_stream_requested\": \""
           << RequestedVertexStreamName() << "\",\n";
    output << "  \"gx_vertex_stream_observed\": \""
           << ObservedVertexStreamName() << "\",\n";
    output << "  \"arb_buffer_storage_advertised\": "
           << (BufferStorageAdvertised ? "true" : "false") << ",\n";
    output << "  \"arb_buffer_storage_effective\": "
           << (BufferStorageEffective ? "true" : "false") << ",\n";
    output << "  \"gl_vendor\": \"" << JsonEscape(GlVendor) << "\",\n";
    output << "  \"gl_renderer\": \"" << JsonEscape(GlRenderer) << "\",\n";
    output << "  \"gl_version\": \"" << JsonEscape(GlVersion) << "\",\n";
    output << "  \"drawable_width\": " << LastDrawableWidth << ",\n";
    output << "  \"drawable_height\": " << LastDrawableHeight << ",\n";
    output << "  \"capture_width\": " << CaptureWidth << ",\n";
    output << "  \"capture_height\": " << CaptureHeight << ",\n";
    output << "  \"capture_format\": \"BMP BGRA8 bottom-up\",\n";
    output << "  \"capture_hash_format\": \"FNV-1a-64 over GL RGBA8 bottom-up bytes\",\n";
    output << "  \"frames_written\": " << Frames.size() << ",\n";
    output << "  \"measured_frame_count\": " << Frames.size() << ",\n";
    output << "  \"measurement_start_approximate\": "
           << (MeasurementStartApproximate ? "true" : "false") << ",\n";
    const double measuredTotalMs =
        MeasurementStarted && MeasurementEndTick >= MeasurementStartTick
            ? TicksToMilliseconds(
                  MeasurementEndTick - MeasurementStartTick,
                  MeasurementFrequency)
            : 0.0;
    output << "  \"measured_total_ms\": "
           << std::fixed << std::setprecision(6) << measuredTotalMs << ",\n";
    output << "  \"presents_seen\": " << NextPresentId << ",\n";
    output << "  \"final_gpu_drain_ms\": "
           << std::fixed << std::setprecision(6) << FinalGpuDrainMs << ",\n";
    output << "  \"lifetime_counters\": {\n";
    output << "    \"uniform_calls\": " << LifetimeCounters.uniformCalls << ",\n";
    output << "    \"buffer_data_calls\": " << LifetimeCounters.bufferDataCalls << ",\n";
    output << "    \"buffer_data_bytes\": " << LifetimeCounters.bufferDataBytes << ",\n";
    output << "    \"buffer_storage_calls\": " << LifetimeCounters.bufferStorageCalls << ",\n";
    output << "    \"buffer_storage_bytes\": " << LifetimeCounters.bufferStorageBytes << ",\n";
    output << "    \"buffer_map_calls\": " << LifetimeCounters.bufferMapCalls << ",\n";
    output << "    \"buffer_map_bytes\": " << LifetimeCounters.bufferMapBytes << ",\n";
    output << "    \"buffer_map_successes\": " << LifetimeCounters.bufferMapSuccesses << ",\n";
    output << "    \"fence_calls\": " << LifetimeCounters.fenceCalls << ",\n";
    output << "    \"fence_wait_calls\": " << LifetimeCounters.fenceWaitCalls << ",\n";
    output << "    \"draw_calls\": " << LifetimeCounters.drawCalls << ",\n";
    output << "    \"draw_vertices\": " << LifetimeCounters.drawVertices << ",\n";
    output << "    \"texture_upload_calls\": " << LifetimeCounters.textureUploadCalls << ",\n";
    output << "    \"texture_upload_bytes\": " << LifetimeCounters.textureUploadBytes << "\n";
    output << "  },\n";
    output << "  \"capture_frames\": [";
    for (std::size_t index = 0u; index < Config.captureFrames.size(); ++index) {
        if (index != 0u) {
            output << ", ";
        }
        output << Config.captureFrames[index];
    }
    output << "],\n";
    output << "  \"error\": \"" << JsonEscape(error) << "\"\n";
    output << "}\n";
    output.close();
    return output.good();
}

[[noreturn]] void FinishAndExit(int exitCode, const std::string& error) {
    const char* status = exitCode == 0 ? "complete" : "error";
    const bool csvWritten = WriteFramesCsv();
    const bool metadataWritten = WriteMetadata(status, error);
    if (!csvWritten || !metadataWritten) {
        std::fprintf(
            stderr,
            "libPorpoise benchmark: could not write benchmark output to %s\n",
            Config.outputDirectory.string().c_str());
        std::exit(3);
    }
    if (exitCode != 0) {
        std::fprintf(stderr, "libPorpoise benchmark: %s\n", error.c_str());
    }
    std::exit(exitCode);
}

}  // namespace

extern "C" int SIM_HostBenchmarkConfigureFromEnvironment(void) {
    SIM::HostAllocationScope hostAllocations;

    if (Configured) {
        return ConfigureSucceeded ? 1 : 0;
    }
    Configured = true;

    const char* vertexStream = std::getenv("LIBPORPOISE_GX_VERTEX_STREAM");
    if (vertexStream == nullptr || std::string_view(vertexStream) == "auto") {
        Config.vertexStream = VertexStreamRequest::Auto;
    } else if (std::string_view(vertexStream) == "legacy") {
        Config.vertexStream = VertexStreamRequest::Legacy;
    } else {
        std::fprintf(
            stderr,
            "libPorpoise benchmark: LIBPORPOISE_GX_VERTEX_STREAM must be auto or legacy\n");
        return 0;
    }

    const char* output = std::getenv("LIBPORPOISE_BENCHMARK_OUTPUT");
    if (output == nullptr) {
        ConfigureSucceeded = true;
        return 1;
    }
    if (*output == '\0') {
        std::fprintf(
            stderr,
            "libPorpoise benchmark: LIBPORPOISE_BENCHMARK_OUTPUT must not be empty\n");
        return 0;
    }
    Config.enabled = true;
    Config.outputDirectory = std::filesystem::path(output);

    const char* label = std::getenv("LIBPORPOISE_BENCHMARK_LABEL");
    if (label != nullptr) {
        Config.label = label;
    }
    if (!ValidateLabel(Config.label)) {
        std::fprintf(
            stderr,
            "libPorpoise benchmark: LIBPORPOISE_BENCHMARK_LABEL must contain 1-128 printable characters\n");
        return 0;
    }
    if (!ParseOptionalUnsigned(
            "LIBPORPOISE_BENCHMARK_START_FRAME",
            0u,
            Config.startFrame) ||
        !ParseOptionalUnsigned(
            "LIBPORPOISE_BENCHMARK_END_FRAME",
            DefaultEndFrame,
            Config.endFrame) ||
        !ParseOptionalBoolean(
            "LIBPORPOISE_BENCHMARK_NO_PACING",
            Config.noPacing) ||
        !ParseOptionalBoolean(
            "LIBPORPOISE_BENCHMARK_NEUTRAL_INPUT",
            Config.neutralInput)) {
        return 0;
    }
    if (Config.startFrame > Config.endFrame) {
        std::fprintf(
            stderr,
            "libPorpoise benchmark: START_FRAME must be less than or equal to END_FRAME\n");
        return 0;
    }

    const char* captureFrames =
        std::getenv("LIBPORPOISE_BENCHMARK_CAPTURE_FRAMES");
    if (!ParseCaptureFrames(captureFrames, Config.captureFrames)) {
        std::fprintf(
            stderr,
            "libPorpoise benchmark: CAPTURE_FRAMES must be a duplicate-free CSV of unsigned frame IDs\n");
        return 0;
    }
    for (const std::uint64_t frame : Config.captureFrames) {
        if (frame < Config.startFrame || frame > Config.endFrame) {
            std::fprintf(
                stderr,
                "libPorpoise benchmark: capture frame IDs must be within START_FRAME..END_FRAME\n");
            return 0;
        }
    }

    std::error_code filesystemError;
    std::filesystem::create_directories(Config.outputDirectory, filesystemError);
    if (filesystemError ||
        !std::filesystem::is_directory(Config.outputDirectory, filesystemError) ||
        filesystemError) {
        std::fprintf(
            stderr,
            "libPorpoise benchmark: could not create output directory %s\n",
            Config.outputDirectory.string().c_str());
        return 0;
    }

    ConfigureSucceeded = true;
    return 1;
}

extern "C" int SIM_HostBenchmarkInitializeGl(void) {
    SIM::HostAllocationScope hostAllocations;

    if (!Configured && !SIM_HostBenchmarkConfigureFromEnvironment()) {
        return 0;
    }
    if (!ConfigureSucceeded) {
        return 0;
    }
    if (GlInitialized) {
        return 1;
    }

    GlVendor = GlString(GL_VENDOR);
    GlRenderer = GlString(GL_RENDERER);
    GlVersion = GlString(GL_VERSION);
    BufferStorageAdvertised =
        GLAD_GL_ARB_buffer_storage != 0 &&
        glad_glBufferStorage != nullptr &&
        glad_glMapBufferRange != nullptr &&
        glad_glFenceSync != nullptr &&
        glad_glClientWaitSync != nullptr &&
        glad_glDeleteSync != nullptr;
    if (Config.vertexStream == VertexStreamRequest::Legacy) {
        GLAD_GL_ARB_buffer_storage = 0;
    }
    BufferStorageEffective =
        GLAD_GL_ARB_buffer_storage != 0 &&
        glad_glBufferStorage != nullptr &&
        glad_glMapBufferRange != nullptr &&
        glad_glFenceSync != nullptr &&
        glad_glClientWaitSync != nullptr &&
        glad_glDeleteSync != nullptr;

    if (Config.enabled) {
        InstallCounterWrappers();
        if (Config.startFrame == 0u) {
            /* There is no preceding presentation on which to drain the GPU.
             * This includes host/game startup in the primary interval and is
             * intentionally marked approximate in metadata. */
            glFinish();
            FrameCounters = {};
            MeasurementFrequency = SDL_GetPerformanceFrequency();
            MeasurementStartTick = SDL_GetPerformanceCounter();
            MeasurementStarted = true;
            MeasurementStartApproximate = true;
        }
        const std::uint64_t requestedFrameCount =
            Config.endFrame - Config.startFrame;
        Frames.reserve(static_cast<std::size_t>(
            std::min<std::uint64_t>(
                requestedFrameCount == std::numeric_limits<std::uint64_t>::max()
                    ? requestedFrameCount
                    : requestedFrameCount + 1u,
                1000000u)));
    }
    GlInitialized = true;
    return 1;
}

extern "C" int SIM_HostBenchmarkEnabled(void) {
    return Config.enabled ? 1 : 0;
}

extern "C" int SIM_HostBenchmarkNoPacing(void) {
    // Keep pre-measurement game timers on the emulated retrace cadence. The
    // measured interval begins after the start-frame boundary has drained the
    // GPU, at which point rendering can run uncapped without making boot-time
    // wall-clock gates depend on renderer speed.
    return Config.enabled && Config.noPacing && MeasurementStarted ? 1 : 0;
}

extern "C" int SIM_HostBenchmarkNeutralInput(void) {
    return Config.enabled && Config.neutralInput ? 1 : 0;
}

extern "C" void SIM_HostBenchmarkBeforeSwap(
    std::uint32_t retraceId,
    int drawableWidth,
    int drawableHeight) {
    SIM::HostAllocationScope hostAllocations;

    if (!Config.enabled) {
        return;
    }
    if (PendingFrameActive) {
        FinishAndExit(2, "presentation began before the previous frame completed");
    }

    PendingFrame = {};
    PendingFrame.presentId = NextPresentId++;
    PendingFrame.retraceId = retraceId;
    PendingFrame.drawableWidth = drawableWidth;
    PendingFrame.drawableHeight = drawableHeight;
    PendingFrame.counters = FrameCounters;
    LastDrawableWidth = drawableWidth;
    LastDrawableHeight = drawableHeight;

    const std::uint64_t frequency = SDL_GetPerformanceFrequency();
    const std::uint64_t preSwap = SDL_GetPerformanceCounter();
    if (LastPreSwapTick != 0u) {
        PendingFrame.intervalMs = TicksToMilliseconds(
            preSwap - LastPreSwapTick,
            frequency);
    }
    if (LastPresentedRetraceEndTick != 0u) {
        const double elapsedSincePresentMs = TicksToMilliseconds(
            preSwap - LastPresentedRetraceEndTick,
            frequency);
        PendingFrame.workMs = std::max(
            0.0,
            elapsedSincePresentMs - AccumulatedPaceMs);
    }
    LastPreSwapTick = preSwap;

    if (ShouldCapture(PendingFrame.presentId)) {
        const std::uint64_t captureStart = SDL_GetPerformanceCounter();
        std::string captureError;
        if (!CaptureBackBuffer(PendingFrame, captureError)) {
            FinishAndExit(2, captureError);
        }
        PendingFrame.captureMs = TicksToMilliseconds(
            SDL_GetPerformanceCounter() - captureStart,
            frequency);
    }

    PresentStartTick = SDL_GetPerformanceCounter();
    PendingFrameActive = true;
    PendingFramePresented = false;
}

extern "C" void SIM_HostBenchmarkAfterSwap(
    std::uint32_t retraceId,
    std::uint64_t swapTicks,
    std::uint64_t performanceFrequency) {
    SIM::HostAllocationScope hostAllocations;

    if (!Config.enabled) {
        return;
    }
    if (!PendingFrameActive || PendingFrame.retraceId != retraceId) {
        FinishAndExit(2, "presentation completion did not match its retrace ID");
    }
    PendingFrame.swapMs =
        TicksToMilliseconds(swapTicks, performanceFrequency);
    PendingFrame.presentMs = TicksToMilliseconds(
        SDL_GetPerformanceCounter() - PresentStartTick,
        performanceFrequency);
    PendingFramePresented = true;
}

extern "C" void SIM_HostBenchmarkOnRetraceEnd(
    std::uint32_t retraceId,
    std::uint64_t paceTicks,
    std::uint64_t performanceFrequency) {
    SIM::HostAllocationScope hostAllocations;

    if (!Config.enabled) {
        return;
    }

    AccumulatedPaceMs +=
        TicksToMilliseconds(paceTicks, performanceFrequency);
    if (!PendingFrameActive) {
        return;
    }
    if (!PendingFramePresented || PendingFrame.retraceId != retraceId) {
        FinishAndExit(2, "retrace completed before its presentation completed");
    }

    PendingFrame.paceMs = AccumulatedPaceMs;
    if (PendingFrame.presentId >= Config.startFrame) {
        Frames.push_back(PendingFrame);
    }
    const std::uint64_t completedPresentId = PendingFrame.presentId;
    PendingFrameActive = false;
    PendingFramePresented = false;
    AccumulatedPaceMs = 0.0;
    FrameCounters = {};
    LastPresentedRetraceEndTick = SDL_GetPerformanceCounter();

    if (Config.startFrame != 0u &&
        completedPresentId == Config.startFrame - 1u) {
        glFinish();
        FrameCounters = {};
        MeasurementFrequency = SDL_GetPerformanceFrequency();
        MeasurementStartTick = SDL_GetPerformanceCounter();
        MeasurementStarted = true;
        MeasurementStartApproximate = false;
        LastPreSwapTick = 0u;
        LastPresentedRetraceEndTick = MeasurementStartTick;
    }

    if (completedPresentId == Config.endFrame) {
        const std::uint64_t drainStart = SDL_GetPerformanceCounter();
        glFinish();
        MeasurementEndTick = SDL_GetPerformanceCounter();
        FinalGpuDrainMs = TicksToMilliseconds(
            MeasurementEndTick - drainStart,
            performanceFrequency);
        FinishAndExit(0, std::string());
    }
}
