#pragma once

#include <string>
#include <vector>

#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>

#include "libslic3r/FilamentSpool.hpp"
#include "GUI_Utils.hpp"
#include "Widgets/Button.hpp"

namespace Slic3r {
namespace GUI {

// ── SpoolEditDialog ───────────────────────────────────────────────────────────
// Modal dialog for creating or editing a single FilamentSpool.

class SpoolEditDialog : public DPIDialog
{
public:
    // Pass an existing spool to edit, or a default-constructed one to add.
    SpoolEditDialog(wxWindow* parent, const FilamentSpool& spool, bool is_new);

    // Returns the spool with values the user entered.
    FilamentSpool get_spool() const;

private:
    void build();
    void on_ok(wxCommandEvent&);
    void on_color_pick(wxCommandEvent&);

    bool           m_is_new;
    FilamentSpool  m_spool;

    wxTextCtrl*    m_txt_name       {nullptr};
    wxTextCtrl*    m_txt_brand      {nullptr};
    wxChoice*      m_ch_material    {nullptr};
    wxTextCtrl*    m_txt_color      {nullptr};   // hex field
    wxPanel*       m_color_swatch   {nullptr};   // visual preview
    wxTextCtrl*    m_txt_initial_wt {nullptr};
    wxTextCtrl*    m_txt_tare_wt    {nullptr};
    wxTextCtrl*    m_txt_cost       {nullptr};
    wxTextCtrl*    m_txt_threshold  {nullptr};
    wxTextCtrl*    m_txt_purchase   {nullptr};
    wxTextCtrl*    m_txt_preset     {nullptr};
    wxTextCtrl*    m_txt_notes      {nullptr};
    Button*        m_btn_ok         {nullptr};
};

// ── SpoolWeighInDialog ────────────────────────────────────────────────────────
// Quick dialog: user enters total spool+filament weight from a scale.

class SpoolWeighInDialog : public DPIDialog
{
public:
    SpoolWeighInDialog(wxWindow* parent, const FilamentSpool& spool);

    double  get_total_weight() const;  // grams including tare
    wxString get_note() const;

private:
    void build(const FilamentSpool& spool);

    wxTextCtrl* m_txt_weight {nullptr};
    wxTextCtrl* m_txt_note   {nullptr};
};

// ── SpoolLogUsageDialog ───────────────────────────────────────────────────────
// Shown after slicing: lets user assign spools per extruder and log the usage.

struct ExtruderUsage {
    int     extruder_idx;
    double  model_g;
    double  flush_g;
    std::string spool_id;  // "" = not assigned / skip
};

class SpoolLogUsageDialog : public DPIDialog
{
public:
    // extruder_usage: one entry per active extruder, pre-filled from slice stats.
    // print_name: suggested label for the events (usually the file name).
    SpoolLogUsageDialog(wxWindow* parent,
                        const std::vector<ExtruderUsage>& extruder_usage,
                        const std::string& print_name);

    // Returns the (possibly edited) list; spool_id == "" means skip that extruder.
    std::vector<ExtruderUsage> get_result() const { return m_rows; }

private:
    void build(const std::string& print_name);
    void refresh_total();

    std::vector<ExtruderUsage>   m_rows;
    std::vector<wxChoice*>       m_spool_choices;  // one per extruder row
    wxStaticText*                m_lbl_total {nullptr};
};

// ── FilamentInventoryDialog ───────────────────────────────────────────────────
// Main spool management window.

class FilamentInventoryDialog : public DPIDialog
{
public:
    explicit FilamentInventoryDialog(wxWindow* parent);

    // Refresh the list (call after external inventory changes).
    void refresh();

protected:
    void on_dpi_changed(const wxRect&) override;

private:
    // Build
    void build();
    wxWindow* build_list_panel(wxWindow* parent);
    wxWindow* build_detail_panel(wxWindow* parent);

    // Actions
    void on_add(wxCommandEvent&);
    void on_edit(wxCommandEvent&);
    void on_edit_activated(wxListEvent&);
    void on_archive(wxCommandEvent&);
    void on_delete(wxCommandEvent&);
    void on_weigh_in(wxCommandEvent&);
    void on_log_swap_purge(wxCommandEvent&);
    void on_manual_adjust(wxCommandEvent&);

    void on_list_select(wxListEvent&);
    void on_show_archived(wxCommandEvent&);

    // Helpers
    void         populate_list();
    void         populate_detail(const FilamentSpool* spool);
    void         populate_events(const FilamentSpool* spool);
    std::string  selected_id() const;
    void         update_button_states();
    static wxColour hex_to_colour(const std::string& hex);
    static wxString event_label(const SpoolEvent& ev);
    static wxString format_grams(double g);

    // Widgets
    wxListCtrl*    m_list          {nullptr};
    wxCheckBox*    m_chk_archived  {nullptr};

    // Detail panel
    wxStaticText*  m_det_name      {nullptr};
    wxStaticText*  m_det_brand     {nullptr};
    wxStaticText*  m_det_material  {nullptr};
    wxPanel*       m_det_color     {nullptr};
    wxGauge*       m_det_gauge     {nullptr};
    wxStaticText*  m_det_remaining {nullptr};
    wxStaticText*  m_det_cost      {nullptr};
    wxListCtrl*    m_evt_list      {nullptr};

    // Action buttons
    Button*        m_btn_edit      {nullptr};
    Button*        m_btn_archive   {nullptr};
    Button*        m_btn_delete    {nullptr};
    Button*        m_btn_weigh     {nullptr};
    Button*        m_btn_swap      {nullptr};
    Button*        m_btn_adjust    {nullptr};
};

} // namespace GUI
} // namespace Slic3r
