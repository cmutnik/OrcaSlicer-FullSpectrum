#pragma once
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>

namespace Slic3r {
namespace GUI {

enum class TimelapseMode {
    PerLayer,
    TimeInterval,
};

// Records timelapse frames from an mjpg-streamer snapshot URL and assembles
// them into an MP4 via system ffmpeg when stopped.
class TimelapseRecorder
{
public:
    TimelapseRecorder()  = default;
    ~TimelapseRecorder();

    // Begin a timelapse session.
    // snapshot_url  – http://<ip>:<port>/?action=snapshot
    // output_dir    – directory to write frames and final video
    // mode          – PerLayer (caller drives via capture_frame) or TimeInterval (internal timer)
    // interval_secs – seconds between frames when mode==TimeInterval
    void start(const std::string& snapshot_url,
               const std::string& output_dir,
               TimelapseMode      mode,
               int                interval_secs);

    // Capture one frame right now (called on layer change for PerLayer mode,
    // or by the internal timer thread for TimeInterval mode).
    void capture_frame();

    // Stop capture and assemble frames into MP4 with ffmpeg at the given fps.
    // Returns path to output video, or empty string if assembly failed.
    // On failure, frames are preserved in output_dir with a message via on_assembly_failed.
    std::string stop_and_assemble(int fps = 30);

    bool is_recording() const { return m_running.load(); }
    int  frame_count()  const { return m_frame_count.load(); }

    // Optional callback invoked on the calling thread when ffmpeg is not found.
    // Provides the frame directory and a suggested command the user can run manually.
    std::function<void(const std::string& frame_dir, const std::string& cmd)> on_assembly_failed;

private:
    void timer_thread_func(int interval_secs);
    std::string frames_dir() const;

    std::string         m_snapshot_url;
    std::string         m_output_dir;
    TimelapseMode       m_mode{TimelapseMode::PerLayer};
    std::atomic<bool>   m_running{false};
    std::atomic<int>    m_frame_count{0};
    std::thread         m_timer_thread;
    mutable std::mutex  m_mutex;
    std::string         m_session_id;  // timestamp-based, set on start()
};

} // namespace GUI
} // namespace Slic3r
