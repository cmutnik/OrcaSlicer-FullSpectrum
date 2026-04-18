#include "TimelapseRecorder.hpp"
#include "MjpegFrameGrabber.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#   include <direct.h>
#   define MKDIR(p) _mkdir(p)
#else
#   include <sys/stat.h>
#   define MKDIR(p) mkdir(p, 0755)
#endif

namespace Slic3r {
namespace GUI {

TimelapseRecorder::~TimelapseRecorder()
{
    if (m_running.load()) {
        m_running.store(false);
        if (m_timer_thread.joinable())
            m_timer_thread.join();
    }
}

static std::string make_session_id()
{
    auto t  = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string TimelapseRecorder::frames_dir() const
{
    return m_output_dir + "/frames_" + m_session_id;
}

void TimelapseRecorder::start(const std::string& snapshot_url,
                               const std::string& output_dir,
                               TimelapseMode      mode,
                               int                interval_secs)
{
    if (m_running.load())
        return;

    m_snapshot_url = snapshot_url;
    m_output_dir   = output_dir;
    m_mode         = mode;
    m_frame_count.store(0);
    m_session_id   = make_session_id();

    MKDIR(m_output_dir.c_str());
    MKDIR(frames_dir().c_str());

    m_running.store(true);

    if (mode == TimelapseMode::TimeInterval) {
        m_timer_thread = std::thread(&TimelapseRecorder::timer_thread_func, this, interval_secs);
    }
}

void TimelapseRecorder::capture_frame()
{
    if (!m_running.load())
        return;

    auto jpeg = MjpegFrameGrabber::grab_snapshot(m_snapshot_url);
    if (jpeg.empty())
        return;

    int idx;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        idx = m_frame_count.fetch_add(1);
    }

    char filename[512];
    std::snprintf(filename, sizeof(filename), "%s/frame_%04d.jpg",
                  frames_dir().c_str(), idx);

    std::ofstream f(filename, std::ios::binary);
    f.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
}

void TimelapseRecorder::timer_thread_func(int interval_secs)
{
    while (m_running.load()) {
        capture_frame();
        // Sleep in 200ms chunks so we respond promptly when stopped
        for (int i = 0; i < interval_secs * 5 && m_running.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

std::string TimelapseRecorder::stop_and_assemble(int fps)
{
    if (!m_running.load())
        return {};

    m_running.store(false);
    if (m_timer_thread.joinable())
        m_timer_thread.join();

    if (m_frame_count.load() == 0)
        return {};

    std::string dir    = frames_dir();
    std::string output = m_output_dir + "/timelapse_" + m_session_id + ".mp4";

    char cmd[2048];
    std::snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -framerate %d -i \"%s/frame_%%04d.jpg\" "
        "-c:v libx264 -pix_fmt yuv420p \"%s\" 2>/dev/null",
        fps, dir.c_str(), output.c_str());

    int rc = std::system(cmd);
    if (rc != 0) {
        // ffmpeg failed or not found — inform caller
        char manual_cmd[2048];
        std::snprintf(manual_cmd, sizeof(manual_cmd),
            "ffmpeg -framerate %d -i \"%s/frame_%%04d.jpg\" "
            "-c:v libx264 -pix_fmt yuv420p \"%s\"",
            fps, dir.c_str(), output.c_str());

        if (on_assembly_failed)
            on_assembly_failed(dir, std::string(manual_cmd));
        return {};
    }

    return output;
}

} // namespace GUI
} // namespace Slic3r
