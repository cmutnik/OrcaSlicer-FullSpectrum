#pragma once

#include <string>
#include <vector>
#include <memory>

namespace Slic3r {

// ── Single usage event recorded against a spool ──────────────────────────────

enum class SpoolEventType {
    Print,           // Filament consumed by a print job
    SwapPurge,       // Waste from loading/purging when swapping spools
    WeighIn,         // User weighed the spool and set an absolute remaining value
    ManualAdjust,    // Arbitrary +/- correction with a note
};

struct SpoolEvent {
    std::string     id;           // UUID
    std::string     timestamp;    // ISO 8601 (UTC)
    SpoolEventType  type;
    double          weight_g{0.0};      // for Print/SwapPurge/ManualAdjust: grams consumed (positive)
                                        // for WeighIn: new absolute remaining value in grams
    double          flush_waste_g{0.0}; // Print only: wipe-tower + flush purge portion
    std::string     print_name;         // Print/WeighIn: descriptive label
    std::string     notes;
};

// ── One physical spool of filament ───────────────────────────────────────────

struct FilamentSpool {
    std::string  id;               // UUID (generated on creation)
    std::string  name;             // User label, e.g. "Black PLA – Hatchbox"
    std::string  brand;
    std::string  material;         // "PLA", "PETG", "ABS", …
    std::string  color_hex;        // "#RRGGBB"
    std::string  preset_name;      // Optional: linked filament preset name

    double  initial_weight_g{1000.0};  // Net filament weight (no spool), e.g. 1000 g
    double  tare_weight_g{0.0};        // Empty spool weight for weigh-in calculations
    double  low_threshold_g{50.0};     // Warn when remaining drops below this
    double  cost_per_kg{0.0};          // Currency per kilogram

    std::string  purchase_date;    // "YYYY-MM-DD"
    std::string  notes;
    bool         archived{false};  // Retired/empty spools are archived, not deleted

    std::vector<SpoolEvent> events;

    // ── Computed helpers ─────────────────────────────────────────────────────

    // Grams of filament consumed (sum of all non-WeighIn events since last WeighIn)
    double used_weight_g() const;

    // Grams remaining on this spool
    double remaining_weight_g() const;

    // 0–100 percentage remaining
    double remaining_pct() const;

    // Monetary cost of all filament consumed
    double cost_used() const;

    // True when remaining ≤ low_threshold_g
    bool is_low() const { return remaining_weight_g() <= low_threshold_g; }
};

// ── Singleton inventory ───────────────────────────────────────────────────────

class SpoolInventory
{
public:
    static SpoolInventory& get();

    // Persistence
    void load(const std::string& path);
    void save() const;
    bool is_loaded() const { return m_loaded; }
    const std::string& path() const { return m_path; }

    // Collection access
    std::vector<FilamentSpool>&       spools()       { return m_spools; }
    const std::vector<FilamentSpool>& spools() const { return m_spools; }

    FilamentSpool*       find(const std::string& id);
    const FilamentSpool* find(const std::string& id) const;

    // CRUD
    FilamentSpool& add_spool(FilamentSpool s);   // assigns id, returns reference
    void           remove_spool(const std::string& id);

    // Event recording helpers
    void record_print(const std::string& spool_id,
                      double model_g, double flush_g,
                      const std::string& print_name);
    void record_swap_purge(const std::string& spool_id, double waste_g);
    void record_weigh_in(const std::string& spool_id,
                         double total_spool_g,   // total weight including tare
                         const std::string& note = {});
    void record_adjustment(const std::string& spool_id,
                           double delta_g,       // positive = more used, negative = correction
                           const std::string& note);

    // Utility
    static std::string generate_id();
    static std::string current_timestamp();

private:
    SpoolInventory() = default;
    std::vector<FilamentSpool> m_spools;
    std::string                m_path;
    bool                       m_loaded{false};
};

} // namespace Slic3r
