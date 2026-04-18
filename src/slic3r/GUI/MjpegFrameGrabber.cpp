#include "MjpegFrameGrabber.hpp"
#include <curl/curl.h>

namespace Slic3r {
namespace GUI {

size_t MjpegFrameGrabber::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* buf = static_cast<std::vector<unsigned char>*>(userdata);
    size_t total = size * nmemb;
    buf->insert(buf->end(), ptr, ptr + total);
    return total;
}

std::vector<unsigned char> MjpegFrameGrabber::grab_snapshot(const std::string& url)
{
    std::vector<unsigned char> data;
    CURL* curl = curl_easy_init();
    if (!curl)
        return data;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return data;
}

} // namespace GUI
} // namespace Slic3r
