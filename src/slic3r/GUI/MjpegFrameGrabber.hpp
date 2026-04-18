#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>

namespace Slic3r {
namespace GUI {

// Fetches a single JPEG snapshot from an mjpg-streamer snapshot URL
// (http://<host>:<port>/?action=snapshot) using libcurl.
// Thread-safe; intended for use from the timelapse recorder.
class MjpegFrameGrabber
{
public:
    MjpegFrameGrabber()  = default;
    ~MjpegFrameGrabber() = default;

    // Synchronously fetches one JPEG frame from url.
    // Returns JPEG bytes, or empty vector on failure.
    // Timeout: 5 seconds.
    static std::vector<unsigned char> grab_snapshot(const std::string& url);

private:
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
};

} // namespace GUI
} // namespace Slic3r
