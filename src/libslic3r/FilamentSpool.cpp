#include "FilamentSpool.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/log/trivial.hpp>

#include "nlohmann/json.hpp"

using namespace nlohmann;
namespace fs = boost::filesystem;

namespace Slic3r {

// ── FilamentSpool computed helpers ───────────────────────────────────────────

double FilamentSpool::used_weight_g() const
{
    return initial_weight_g - remaining_weight_g();
}

double FilamentSpool::remaining_weight_g() const
{
    double remaining = initial_weight_g;

    for (const auto& ev : events) {
        switch (ev.type) {
        case SpoolEventType::Print:
        case SpoolEventType::SwapPurge:
            remaining -= ev.weight_g;
            break;
        case SpoolEventType::ManualAdjust:
            remaining -= ev.weight_g; // negative delta = correction that adds back
            break;
        case SpoolEventType::WeighIn:
            remaining = ev.weight_g;  // absolute override
            break;
        }
    }
    return std::max(0.0, remaining);
}

double FilamentSpool::remaining_pct() const
{
    if (initial_weight_g <= 0.0)
        return 0.0;
    return (remaining_weight_g() / initial_weight_g) * 100.0;
}

double FilamentSpool::cost_used() const
{
    if (cost_per_kg <= 0.0)
        return 0.0;
    return used_weight_g() / 1000.0 * cost_per_kg;
}

// ── JSON serialisation helpers ────────────────────────────────────────────────

namespace {

std::string event_type_to_str(SpoolEventType t)
{
    switch (t) {
    case SpoolEventType::Print:         return "print";
    case SpoolEventType::SwapPurge:     return "swap_purge";
    case SpoolEventType::WeighIn:       return "weigh_in";
    case SpoolEventType::ManualAdjust:  return "manual_adjust";
    }
    return "print";
}

SpoolEventType event_type_from_str(const std::string& s)
{
    if (s == "swap_purge")   return SpoolEventType::SwapPurge;
    if (s == "weigh_in")     return SpoolEventType::WeighIn;
    if (s == "manual_adjust") return SpoolEventType::ManualAdjust;
    return SpoolEventType::Print;
}

json event_to_json(const SpoolEvent& e)
{
    json j;
    j["id"]            = e.id;
    j["timestamp"]     = e.timestamp;
    j["type"]          = event_type_to_str(e.type);
    j["weight_g"]      = e.weight_g;
    j["flush_waste_g"] = e.flush_waste_g;
    j["print_name"]    = e.print_name;
    j["notes"]         = e.notes;
    return j;
}

SpoolEvent event_from_json(const json& j)
{
    SpoolEvent e;
    e.id            = j.value("id", "");
    e.timestamp     = j.value("timestamp", "");
    e.type          = event_type_from_str(j.value("type", "print"));
    e.weight_g      = j.value("weight_g", 0.0);
    e.flush_waste_g = j.value("flush_waste_g", 0.0);
    e.print_name    = j.value("print_name", "");
    e.notes         = j.value("notes", "");
    return e;
}

json spool_to_json(const FilamentSpool& s)
{
    json j;
    j["id"]               = s.id;
    j["name"]             = s.name;
    j["brand"]            = s.brand;
    j["material"]         = s.material;
    j["color_hex"]        = s.color_hex;
    j["preset_name"]      = s.preset_name;
    j["initial_weight_g"] = s.initial_weight_g;
    j["tare_weight_g"]    = s.tare_weight_g;
    j["low_threshold_g"]  = s.low_threshold_g;
    j["cost_per_kg"]      = s.cost_per_kg;
    j["purchase_date"]    = s.purchase_date;
    j["notes"]            = s.notes;
    j["archived"]         = s.archived;

    json events = json::array();
    for (const auto& ev : s.events)
        events.push_back(event_to_json(ev));
    j["events"] = events;

    return j;
}

FilamentSpool spool_from_json(const json& j)
{
    FilamentSpool s;
    s.id               = j.value("id", "");
    s.name             = j.value("name", "");
    s.brand            = j.value("brand", "");
    s.material         = j.value("material", "");
    s.color_hex        = j.value("color_hex", "#FFFFFF");
    s.preset_name      = j.value("preset_name", "");
    s.initial_weight_g = j.value("initial_weight_g", 1000.0);
    s.tare_weight_g    = j.value("tare_weight_g", 0.0);
    s.low_threshold_g  = j.value("low_threshold_g", 50.0);
    s.cost_per_kg      = j.value("cost_per_kg", 0.0);
    s.purchase_date    = j.value("purchase_date", "");
    s.notes            = j.value("notes", "");
    s.archived         = j.value("archived", false);

    if (j.contains("events") && j["events"].is_array()) {
        for (const auto& ev : j["events"])
            s.events.push_back(event_from_json(ev));
    }
    return s;
}

} // anonymous namespace

// ── SpoolInventory ────────────────────────────────────────────────────────────

SpoolInventory& SpoolInventory::get()
{
    static SpoolInventory instance;
    return instance;
}

void SpoolInventory::load(const std::string& path)
{
    m_path = path;
    m_loaded = true;
    m_spools.clear();

    fs::path p(path);
    if (!fs::exists(p)) {
        BOOST_LOG_TRIVIAL(info) << "SpoolInventory: no file at " << path << ", starting empty";
        return;
    }

    try {
        fs::ifstream ifs(p);
        json root;
        ifs >> root;

        if (root.contains("spools") && root["spools"].is_array()) {
            for (const auto& sj : root["spools"])
                m_spools.push_back(spool_from_json(sj));
        }
        BOOST_LOG_TRIVIAL(info) << "SpoolInventory: loaded " << m_spools.size() << " spools from " << path;
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "SpoolInventory: failed to load " << path << ": " << ex.what();
    }
}

void SpoolInventory::save() const
{
    if (m_path.empty()) return;

    try {
        json root;
        root["version"] = 1;
        json spools_arr = json::array();
        for (const auto& s : m_spools)
            spools_arr.push_back(spool_to_json(s));
        root["spools"] = spools_arr;

        fs::path p(m_path);
        fs::create_directories(p.parent_path());
        fs::ofstream ofs(p);
        ofs << root.dump(2);
        BOOST_LOG_TRIVIAL(info) << "SpoolInventory: saved " << m_spools.size() << " spools to " << m_path;
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "SpoolInventory: failed to save " << m_path << ": " << ex.what();
    }
}

FilamentSpool* SpoolInventory::find(const std::string& id)
{
    for (auto& s : m_spools)
        if (s.id == id) return &s;
    return nullptr;
}

const FilamentSpool* SpoolInventory::find(const std::string& id) const
{
    for (const auto& s : m_spools)
        if (s.id == id) return &s;
    return nullptr;
}

FilamentSpool& SpoolInventory::add_spool(FilamentSpool s)
{
    if (s.id.empty())
        s.id = generate_id();
    m_spools.push_back(std::move(s));
    save();
    return m_spools.back();
}

void SpoolInventory::remove_spool(const std::string& id)
{
    m_spools.erase(
        std::remove_if(m_spools.begin(), m_spools.end(),
                       [&](const FilamentSpool& s) { return s.id == id; }),
        m_spools.end());
    save();
}

void SpoolInventory::record_print(const std::string& spool_id,
                                   double model_g, double flush_g,
                                   const std::string& print_name)
{
    auto* s = find(spool_id);
    if (!s) return;

    SpoolEvent ev;
    ev.id           = generate_id();
    ev.timestamp    = current_timestamp();
    ev.type         = SpoolEventType::Print;
    ev.weight_g     = model_g + flush_g;
    ev.flush_waste_g = flush_g;
    ev.print_name   = print_name;
    s->events.push_back(std::move(ev));
    save();
}

void SpoolInventory::record_swap_purge(const std::string& spool_id, double waste_g)
{
    auto* s = find(spool_id);
    if (!s) return;

    SpoolEvent ev;
    ev.id        = generate_id();
    ev.timestamp = current_timestamp();
    ev.type      = SpoolEventType::SwapPurge;
    ev.weight_g  = waste_g;
    s->events.push_back(std::move(ev));
    save();
}

void SpoolInventory::record_weigh_in(const std::string& spool_id,
                                      double total_spool_g,
                                      const std::string& note)
{
    auto* s = find(spool_id);
    if (!s) return;

    // Subtract tare to get net filament remaining
    double net = total_spool_g - s->tare_weight_g;
    if (net < 0.0) net = 0.0;

    SpoolEvent ev;
    ev.id        = generate_id();
    ev.timestamp = current_timestamp();
    ev.type      = SpoolEventType::WeighIn;
    ev.weight_g  = net;
    ev.notes     = note;
    s->events.push_back(std::move(ev));
    save();
}

void SpoolInventory::record_adjustment(const std::string& spool_id,
                                        double delta_g,
                                        const std::string& note)
{
    auto* s = find(spool_id);
    if (!s) return;

    SpoolEvent ev;
    ev.id        = generate_id();
    ev.timestamp = current_timestamp();
    ev.type      = SpoolEventType::ManualAdjust;
    ev.weight_g  = delta_g;
    ev.notes     = note;
    s->events.push_back(std::move(ev));
    save();
}

// ── Static utilities ──────────────────────────────────────────────────────────

std::string SpoolInventory::generate_id()
{
    static std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << dist(rng)
        << std::setw(16) << std::setfill('0') << dist(rng);
    return oss.str();
}

std::string SpoolInventory::current_timestamp()
{
    auto now   = std::chrono::system_clock::now();
    auto t     = std::chrono::system_clock::to_time_t(now);
    std::tm   tm_utc{};
#ifdef _WIN32
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace Slic3r
