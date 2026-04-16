#include "FilamentInventoryDialog.hpp"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/listctrl.h>
#include <wx/splitter.h>
#include <wx/gauge.h>
#include <wx/msgdlg.h>
#include <wx/colordlg.h>
#include <wx/clrpicker.h>
#include <wx/valnum.h>
#include <wx/panel.h>
#include <wx/wupdlock.h>

#include "libslic3r/FilamentSpool.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "format.hpp"
#include "I18N.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/Button.hpp"

namespace Slic3r {
namespace GUI {

// ── Helpers ───────────────────────────────────────────────────────────────────

static const std::vector<std::string> MATERIAL_TYPES = {
    "PLA", "PETG", "ABS", "ASA", "TPU", "PC", "PA", "PA-CF", "PLA-CF",
    "PETG-CF", "PVA", "HIPS", "BVOH", "PP", "PHA", "SBS", "Other"
};

wxColour FilamentInventoryDialog::hex_to_colour(const std::string& hex)
{
    wxColour c(hex);
    return c.IsOk() ? c : wxColour(200, 200, 200);
}

wxString FilamentInventoryDialog::format_grams(double g)
{
    if (g >= 1000.0)
        return wxString::Format("%.2f kg", g / 1000.0);
    return wxString::Format("%.0f g", g);
}

wxString FilamentInventoryDialog::event_label(const SpoolEvent& ev)
{
    wxString ts = ev.timestamp.size() >= 10
                      ? wxString(ev.timestamp.substr(0, 10))
                      : wxString(ev.timestamp);
    switch (ev.type) {
    case SpoolEventType::Print:
        return wxString::Format("%s  Print \"%s\"  –%s",
                                ts, ev.print_name, format_grams(ev.weight_g));
    case SpoolEventType::SwapPurge:
        return wxString::Format("%s  Swap purge  –%s",
                                ts, format_grams(ev.weight_g));
    case SpoolEventType::WeighIn:
        return wxString::Format("%s  Weigh-in  →%s remaining",
                                ts, format_grams(ev.weight_g));
    case SpoolEventType::ManualAdjust: {
        wxString sign = ev.weight_g >= 0 ? "-" : "+";
        return wxString::Format("%s  Adjustment %s%s  %s",
                                ts, sign, format_grams(std::abs(ev.weight_g)),
                                wxString(ev.notes));
    }
    }
    return wxString(ev.timestamp);
}

// ── SpoolEditDialog ───────────────────────────────────────────────────────────

SpoolEditDialog::SpoolEditDialog(wxWindow* parent,
                                 const FilamentSpool& spool,
                                 bool is_new)
    : DPIDialog(parent, wxID_ANY,
                is_new ? _L("Add New Spool") : _L("Edit Spool"),
                wxDefaultPosition, wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_is_new(is_new)
    , m_spool(spool)
{
    SetFont(wxGetApp().normal_font());
    SetBackgroundColour(*wxWHITE);
    build();
    Fit();
    CenterOnParent();
}

void SpoolEditDialog::build()
{
    const int em = wxGetApp().em_unit();
    auto* main = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, FromDIP(8), FromDIP(12));
    grid->AddGrowableCol(1, 1);

    auto add_row = [&](const wxString& label, wxWindow* ctrl) {
        grid->Add(new wxStaticText(this, wxID_ANY, label),
                  0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
        grid->Add(ctrl, 0, wxEXPAND);
    };

    auto make_txt = [&](const std::string& val, int width = 220) -> wxTextCtrl* {
        auto* t = new wxTextCtrl(this, wxID_ANY, val,
                                  wxDefaultPosition, wxSize(FromDIP(width), -1));
        return t;
    };

    // Name
    m_txt_name = make_txt(m_spool.name);
    add_row(_L("Name:"), m_txt_name);

    // Brand
    m_txt_brand = make_txt(m_spool.brand);
    add_row(_L("Brand:"), m_txt_brand);

    // Material
    m_ch_material = new wxChoice(this, wxID_ANY);
    for (const auto& mt : MATERIAL_TYPES)
        m_ch_material->Append(mt);
    {
        int idx = 0;
        for (int i = 0; i < (int)MATERIAL_TYPES.size(); ++i) {
            if (MATERIAL_TYPES[i] == m_spool.material) { idx = i; break; }
        }
        m_ch_material->SetSelection(idx);
    }
    add_row(_L("Material:"), m_ch_material);

    // Color row (swatch + hex field + pick button)
    {
        auto* color_row = new wxBoxSizer(wxHORIZONTAL);
        m_color_swatch = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(24), FromDIP(24)));
        m_color_swatch->SetBackgroundColour(wxColour(m_spool.color_hex.empty() ? "#FFFFFF" : m_spool.color_hex));
        m_txt_color = new wxTextCtrl(this, wxID_ANY, m_spool.color_hex,
                                      wxDefaultPosition, wxSize(FromDIP(90), -1));
        auto* pick_btn = new wxButton(this, wxID_ANY, _L("Pick…"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        color_row->Add(m_color_swatch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
        color_row->Add(m_txt_color, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
        color_row->Add(pick_btn, 0, wxALIGN_CENTER_VERTICAL);
        pick_btn->Bind(wxEVT_BUTTON, &SpoolEditDialog::on_color_pick, this);
        m_txt_color->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            wxColour c(m_txt_color->GetValue());
            if (c.IsOk()) m_color_swatch->SetBackgroundColour(c);
            m_color_swatch->Refresh();
        });
        add_row(_L("Color (hex):"), color_row->GetItem((size_t)0)->GetWindow()
                                   ? this : this); // placeholder – add sizer directly
        // Adjust: remove the placeholder and add the sizer
        grid->Detach(grid->GetItemCount() - 1);
        grid->Add(color_row, 0, wxEXPAND);
    }

    // Initial weight
    m_txt_initial_wt = make_txt(wxString::Format("%.0f", m_spool.initial_weight_g).ToStdString(), 100);
    add_row(_L("Initial weight (g):"), m_txt_initial_wt);

    // Tare weight
    m_txt_tare_wt = make_txt(wxString::Format("%.0f", m_spool.tare_weight_g).ToStdString(), 100);
    add_row(_L("Spool tare (g):"), m_txt_tare_wt);

    // Cost
    m_txt_cost = make_txt(wxString::Format("%.2f", m_spool.cost_per_kg).ToStdString(), 100);
    add_row(_L("Cost per kg:"), m_txt_cost);

    // Low threshold
    m_txt_threshold = make_txt(wxString::Format("%.0f", m_spool.low_threshold_g).ToStdString(), 100);
    add_row(_L("Low threshold (g):"), m_txt_threshold);

    // Purchase date
    m_txt_purchase = make_txt(m_spool.purchase_date, 120);
    m_txt_purchase->SetHint("YYYY-MM-DD");
    add_row(_L("Purchase date:"), m_txt_purchase);

    // Linked preset
    m_txt_preset = make_txt(m_spool.preset_name);
    m_txt_preset->SetHint(_L("(optional) filament preset name"));
    add_row(_L("Preset link:"), m_txt_preset);

    // Notes
    m_txt_notes = new wxTextCtrl(this, wxID_ANY, m_spool.notes,
                                  wxDefaultPosition, wxSize(FromDIP(220), FromDIP(50)),
                                  wxTE_MULTILINE);
    add_row(_L("Notes:"), m_txt_notes);

    main->Add(grid, 1, wxEXPAND | wxALL, FromDIP(12));

    // Buttons
    auto* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer();
    auto* btn_cancel = new Button(this, _L("Cancel"));
    m_btn_ok = new Button(this, m_is_new ? _L("Add Spool") : _L("Save"));
    m_btn_ok->SetStyle(ButtonStyle::Confirm);
    btn_sizer->Add(btn_cancel, 0, wxRIGHT, FromDIP(8));
    btn_sizer->Add(m_btn_ok, 0);
    main->Add(btn_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    btn_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });
    m_btn_ok->Bind(wxEVT_BUTTON, &SpoolEditDialog::on_ok, this);

    SetSizer(main);
}

void SpoolEditDialog::on_color_pick(wxCommandEvent&)
{
    wxColourData cd;
    wxColour cur(m_txt_color->GetValue());
    if (cur.IsOk()) cd.SetColour(cur);
    wxColourDialog dlg(this, &cd);
    if (dlg.ShowModal() == wxID_OK) {
        wxColour c = dlg.GetColourData().GetColour();
        m_txt_color->SetValue(wxString::Format("#%02X%02X%02X", c.Red(), c.Green(), c.Blue()));
        m_color_swatch->SetBackgroundColour(c);
        m_color_swatch->Refresh();
    }
}

void SpoolEditDialog::on_ok(wxCommandEvent&)
{
    // Basic validation
    if (m_txt_name->GetValue().IsEmpty()) {
        wxMessageBox(_L("Please enter a spool name."), _L("Validation"), wxICON_WARNING | wxOK, this);
        m_txt_name->SetFocus();
        return;
    }
    EndModal(wxID_OK);
}

FilamentSpool SpoolEditDialog::get_spool() const
{
    FilamentSpool s = m_spool;
    s.name          = m_txt_name->GetValue().ToStdString();
    s.brand         = m_txt_brand->GetValue().ToStdString();
    int mat_sel     = m_ch_material->GetSelection();
    s.material      = (mat_sel >= 0 && mat_sel < (int)MATERIAL_TYPES.size())
                          ? MATERIAL_TYPES[mat_sel] : "PLA";
    s.color_hex     = m_txt_color->GetValue().ToStdString();
    s.preset_name   = m_txt_preset->GetValue().ToStdString();
    s.purchase_date = m_txt_purchase->GetValue().ToStdString();
    s.notes         = m_txt_notes->GetValue().ToStdString();

    double v = 0.0;
    if (m_txt_initial_wt->GetValue().ToDouble(&v))  s.initial_weight_g = v;
    if (m_txt_tare_wt->GetValue().ToDouble(&v))     s.tare_weight_g    = v;
    if (m_txt_cost->GetValue().ToDouble(&v))         s.cost_per_kg      = v;
    if (m_txt_threshold->GetValue().ToDouble(&v))    s.low_threshold_g  = v;
    return s;
}

// ── SpoolWeighInDialog ────────────────────────────────────────────────────────

SpoolWeighInDialog::SpoolWeighInDialog(wxWindow* parent, const FilamentSpool& spool)
    : DPIDialog(parent, wxID_ANY,
                wxString::Format(_L("Weigh Spool: %s"), spool.name),
                wxDefaultPosition, wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE)
{
    SetFont(wxGetApp().normal_font());
    SetBackgroundColour(*wxWHITE);
    build(spool);
    Fit();
    CenterOnParent();
}

void SpoolWeighInDialog::build(const FilamentSpool& spool)
{
    auto* main = new wxBoxSizer(wxVERTICAL);
    main->AddSpacer(FromDIP(12));

    // Instructions
    wxString info = wxString::Format(
        _L("Place the spool on a scale and enter the total weight.\n"
           "Spool tare: %.0f g  |  Net filament remaining will be calculated automatically."),
        spool.tare_weight_g);
    auto* lbl_info = new wxStaticText(this, wxID_ANY, info);
    lbl_info->Wrap(FromDIP(340));
    main->Add(lbl_info, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    main->AddSpacer(FromDIP(12));

    // Weight input
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(this, wxID_ANY, _L("Total weight (g):")),
             0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_txt_weight = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxSize(FromDIP(100), -1));
    row->Add(m_txt_weight, 0, wxALIGN_CENTER_VERTICAL);
    main->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    main->AddSpacer(FromDIP(8));

    // Note input
    auto* row2 = new wxBoxSizer(wxHORIZONTAL);
    row2->Add(new wxStaticText(this, wxID_ANY, _L("Note (optional):")),
              0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_txt_note = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                 wxDefaultPosition, wxSize(FromDIP(180), -1));
    row2->Add(m_txt_note, 1, wxALIGN_CENTER_VERTICAL);
    main->Add(row2, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    main->AddSpacer(FromDIP(12));

    // Buttons
    auto* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer();
    auto* btn_cancel = new Button(this, _L("Cancel"));
    auto* btn_ok     = new Button(this, _L("Record Weigh-in"));
    btn_ok->SetStyle(ButtonStyle::Confirm);
    btn_sizer->Add(btn_cancel, 0, wxRIGHT, FromDIP(8));
    btn_sizer->Add(btn_ok, 0);
    main->Add(btn_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    btn_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });
    btn_ok->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        double v = 0.0;
        if (!m_txt_weight->GetValue().ToDouble(&v) || v < 0.0) {
            wxMessageBox(_L("Please enter a valid weight in grams."),
                          _L("Validation"), wxICON_WARNING | wxOK, this);
            return;
        }
        EndModal(wxID_OK);
    });

    SetSizer(main);
}

double SpoolWeighInDialog::get_total_weight() const
{
    double v = 0.0;
    m_txt_weight->GetValue().ToDouble(&v);
    return v;
}

wxString SpoolWeighInDialog::get_note() const
{
    return m_txt_note->GetValue();
}

// ── SpoolLogUsageDialog ───────────────────────────────────────────────────────

SpoolLogUsageDialog::SpoolLogUsageDialog(wxWindow* parent,
                                          const std::vector<ExtruderUsage>& extruder_usage,
                                          const std::string& print_name)
    : DPIDialog(parent, wxID_ANY, _L("Log Filament Usage"),
                wxDefaultPosition, wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_rows(extruder_usage)
{
    SetFont(wxGetApp().normal_font());
    SetBackgroundColour(*wxWHITE);
    build(print_name);
    Fit();
    CenterOnParent();
}

void SpoolLogUsageDialog::build(const std::string& print_name)
{
    auto& inv   = SpoolInventory::get();
    const int em = wxGetApp().em_unit();

    auto* main = new wxBoxSizer(wxVERTICAL);
    main->AddSpacer(FromDIP(8));

    auto* lbl_title = new wxStaticText(this, wxID_ANY,
        wxString::Format(_L("Print: \"%s\""), print_name));
    lbl_title->SetFont(lbl_title->GetFont().Bold());
    main->Add(lbl_title, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    main->AddSpacer(FromDIP(8));

    auto* lbl_desc = new wxStaticText(this, wxID_ANY,
        _L("Select which spool was used for each extruder. Filament usage will be\n"
           "deducted from the selected spool when you click \"Record Usage\"."));
    lbl_desc->Wrap(FromDIP(440));
    main->Add(lbl_desc, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    main->AddSpacer(FromDIP(12));

    // Build a "none" + all active spools choice list
    wxArrayString spool_names;
    spool_names.Add(_L("(skip – don't log)"));
    std::vector<std::string> spool_ids;
    spool_ids.push_back(""); // corresponds to (skip)
    for (const auto& s : inv.spools()) {
        if (!s.archived) {
            spool_names.Add(wxString::Format("%s  [%s, %.0fg left]",
                                             s.name, s.material,
                                             s.remaining_weight_g()));
            spool_ids.push_back(s.id);
        }
    }

    // Grid: Extruder | Model | Flush | Total | Spool picker
    auto* grid = new wxFlexGridSizer(5, FromDIP(4), FromDIP(10));
    grid->AddGrowableCol(4, 1);

    auto hdr = [&](const wxString& txt) {
        auto* l = new wxStaticText(this, wxID_ANY, txt);
        l->SetFont(l->GetFont().Bold());
        grid->Add(l, 0, wxALIGN_CENTER_VERTICAL);
    };
    hdr(_L("Extruder")); hdr(_L("Model")); hdr(_L("Flush"));
    hdr(_L("Total")); hdr(_L("Spool"));

    m_spool_choices.clear();
    for (size_t i = 0; i < m_rows.size(); ++i) {
        auto& row = m_rows[i];
        grid->Add(new wxStaticText(this, wxID_ANY,
                                    wxString::Format(_L("E%d"), row.extruder_idx + 1)),
                  0, wxALIGN_CENTER_VERTICAL);
        grid->Add(new wxStaticText(this, wxID_ANY, format_grams(row.model_g)),
                  0, wxALIGN_CENTER_VERTICAL);
        grid->Add(new wxStaticText(this, wxID_ANY, format_grams(row.flush_g)),
                  0, wxALIGN_CENTER_VERTICAL);
        grid->Add(new wxStaticText(this, wxID_ANY,
                                    format_grams(row.model_g + row.flush_g)),
                  0, wxALIGN_CENTER_VERTICAL);

        auto* ch = new wxChoice(this, wxID_ANY, wxDefaultPosition,
                                 wxSize(FromDIP(220), -1), spool_names);
        // Pre-select if spool_id is set
        int sel = 0;
        if (!row.spool_id.empty()) {
            for (int j = 1; j < (int)spool_ids.size(); ++j) {
                if (spool_ids[j] == row.spool_id) { sel = j; break; }
            }
        }
        ch->SetSelection(sel);
        // Store spool_ids alongside choice via closure
        ch->Bind(wxEVT_CHOICE, [this, i, spool_ids](wxCommandEvent& e) {
            int s = e.GetInt();
            m_rows[i].spool_id = (s >= 0 && s < (int)spool_ids.size())
                                     ? spool_ids[s] : "";
            refresh_total();
        });
        m_spool_choices.push_back(ch);
        grid->Add(ch, 0, wxEXPAND);

        // Initialize spool_id from selection
        m_rows[i].spool_id = (sel >= 0 && sel < (int)spool_ids.size())
                                 ? spool_ids[sel] : "";
    }

    main->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    main->AddSpacer(FromDIP(8));

    // Total summary
    m_lbl_total = new wxStaticText(this, wxID_ANY, wxEmptyString);
    main->Add(m_lbl_total, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));
    main->AddSpacer(FromDIP(12));
    refresh_total();

    // Buttons
    auto* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer();
    auto* btn_cancel = new Button(this, _L("Cancel"));
    auto* btn_ok     = new Button(this, _L("Record Usage"));
    btn_ok->SetStyle(ButtonStyle::Confirm);
    btn_sizer->Add(btn_cancel, 0, wxRIGHT, FromDIP(8));
    btn_sizer->Add(btn_ok, 0);
    main->Add(btn_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    btn_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });
    btn_ok->Bind(wxEVT_BUTTON,     [this](wxCommandEvent&) { EndModal(wxID_OK); });

    SetSizer(main);
}

void SpoolLogUsageDialog::refresh_total()
{
    double total = 0.0;
    int    count = 0;
    for (const auto& r : m_rows) {
        if (!r.spool_id.empty()) {
            total += r.model_g + r.flush_g;
            ++count;
        }
    }
    if (m_lbl_total) {
        m_lbl_total->SetLabel(
            wxString::Format(_L("Total to deduct: %s across %d spool(s)"),
                             format_grams(total), count));
    }
}

// ── FilamentInventoryDialog ───────────────────────────────────────────────────

FilamentInventoryDialog::FilamentInventoryDialog(wxWindow* parent)
    : DPIDialog(parent, wxID_ANY, _L("Filament Inventory"),
                wxDefaultPosition, wxSize(FromDIP(780), FromDIP(520)),
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    SetFont(wxGetApp().normal_font());
    SetBackgroundColour(*wxWHITE);
    build();
    CenterOnParent();
}

void FilamentInventoryDialog::build()
{
    auto* main = new wxBoxSizer(wxVERTICAL);

    // Splitter: left = list, right = detail
    auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition,
                                           wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3DSASH);
    splitter->SetMinimumPaneSize(FromDIP(200));

    auto* left  = build_list_panel(splitter);
    auto* right = build_detail_panel(splitter);
    splitter->SplitVertically(left, right, FromDIP(290));

    main->Add(splitter, 1, wxEXPAND | wxALL, FromDIP(8));

    // Bottom buttons
    auto* bot = new wxBoxSizer(wxHORIZONTAL);
    m_chk_archived = new wxCheckBox(this, wxID_ANY, _L("Show archived spools"));
    m_chk_archived->Bind(wxEVT_CHECKBOX, &FilamentInventoryDialog::on_show_archived, this);
    bot->Add(m_chk_archived, 0, wxALIGN_CENTER_VERTICAL);
    bot->AddStretchSpacer();
    auto* btn_close = new Button(this, _L("Close"));
    btn_close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_OK); });
    bot->Add(btn_close, 0);
    main->Add(bot, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));

    SetSizer(main);
    populate_list();
    populate_detail(nullptr);
    update_button_states();
}

wxWindow* FilamentInventoryDialog::build_list_panel(wxWindow* parent)
{
    auto* panel = new wxPanel(parent, wxID_ANY);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    // Toolbar
    auto* tb = new wxBoxSizer(wxHORIZONTAL);
    auto* btn_add = new Button(panel, _L("+ Add"));
    btn_add->SetStyle(ButtonStyle::Confirm);
    m_btn_edit    = new Button(panel, _L("Edit"));
    m_btn_archive = new Button(panel, _L("Archive"));
    m_btn_delete  = new Button(panel, _L("Delete"));
    tb->Add(btn_add,      0, wxRIGHT, FromDIP(4));
    tb->Add(m_btn_edit,   0, wxRIGHT, FromDIP(4));
    tb->Add(m_btn_archive,0, wxRIGHT, FromDIP(4));
    tb->Add(m_btn_delete, 0);
    sizer->Add(tb, 0, wxEXPAND | wxALL, FromDIP(6));

    // List
    m_list = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SIMPLE);
    m_list->InsertColumn(0, wxEmptyString, wxLIST_FORMAT_LEFT, FromDIP(20)); // color dot
    m_list->InsertColumn(1, _L("Name"),   wxLIST_FORMAT_LEFT, FromDIP(140));
    m_list->InsertColumn(2, _L("Mat."),   wxLIST_FORMAT_LEFT, FromDIP(50));
    m_list->InsertColumn(3, _L("Left"),   wxLIST_FORMAT_RIGHT, FromDIP(65));
    sizer->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(6));

    panel->SetSizer(sizer);

    btn_add->Bind(wxEVT_BUTTON,       &FilamentInventoryDialog::on_add,     this);
    m_btn_edit->Bind(wxEVT_BUTTON,    &FilamentInventoryDialog::on_edit,    this);
    m_btn_archive->Bind(wxEVT_BUTTON, &FilamentInventoryDialog::on_archive, this);
    m_btn_delete->Bind(wxEVT_BUTTON,  &FilamentInventoryDialog::on_delete,  this);
    m_list->Bind(wxEVT_LIST_ITEM_SELECTED,   &FilamentInventoryDialog::on_list_select, this);
    m_list->Bind(wxEVT_LIST_ITEM_DESELECTED, [this](wxListEvent&) {
        populate_detail(nullptr); update_button_states(); });
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &FilamentInventoryDialog::on_edit_activated, this);

    return panel;
}

wxWindow* FilamentInventoryDialog::build_detail_panel(wxWindow* parent)
{
    auto* panel = new wxPanel(parent, wxID_ANY);
    panel->SetBackgroundColour(wxColour(248, 248, 248));
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(FromDIP(8));

    // ── Identity row ──────────────────────────────────────────────────────────
    auto* id_row = new wxBoxSizer(wxHORIZONTAL);
    m_det_color = new wxPanel(panel, wxID_ANY, wxDefaultPosition,
                               wxSize(FromDIP(18), FromDIP(18)));
    m_det_color->SetBackgroundColour(wxColour(200, 200, 200));
    m_det_name  = new wxStaticText(panel, wxID_ANY, wxEmptyString);
    m_det_name->SetFont(m_det_name->GetFont().Bold().Larger());
    id_row->Add(m_det_color, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    id_row->Add(m_det_name,  0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(id_row, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
    sizer->AddSpacer(FromDIP(4));

    m_det_brand    = new wxStaticText(panel, wxID_ANY, wxEmptyString);
    m_det_material = new wxStaticText(panel, wxID_ANY, wxEmptyString);
    sizer->Add(m_det_brand,    0, wxLEFT | wxRIGHT, FromDIP(10));
    sizer->Add(m_det_material, 0, wxLEFT | wxRIGHT, FromDIP(10));
    sizer->AddSpacer(FromDIP(8));

    // ── Progress gauge ────────────────────────────────────────────────────────
    m_det_gauge = new wxGauge(panel, wxID_ANY, 100, wxDefaultPosition,
                               wxSize(-1, FromDIP(12)));
    sizer->Add(m_det_gauge, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
    m_det_remaining = new wxStaticText(panel, wxID_ANY, wxEmptyString);
    sizer->Add(m_det_remaining, 0, wxLEFT | wxRIGHT, FromDIP(10));
    sizer->AddSpacer(FromDIP(4));

    m_det_cost = new wxStaticText(panel, wxID_ANY, wxEmptyString);
    sizer->Add(m_det_cost, 0, wxLEFT | wxRIGHT, FromDIP(10));
    sizer->AddSpacer(FromDIP(10));

    // ── Quick-action buttons ──────────────────────────────────────────────────
    auto* act_row = new wxBoxSizer(wxHORIZONTAL);
    m_btn_weigh  = new Button(panel, _L("Weigh-in"));
    m_btn_swap   = new Button(panel, _L("Log Swap Purge"));
    m_btn_adjust = new Button(panel, _L("Manual Adjust"));
    act_row->Add(m_btn_weigh,  0, wxRIGHT, FromDIP(4));
    act_row->Add(m_btn_swap,   0, wxRIGHT, FromDIP(4));
    act_row->Add(m_btn_adjust, 0);
    sizer->Add(act_row, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
    sizer->AddSpacer(FromDIP(10));

    m_btn_weigh->Bind(wxEVT_BUTTON,  &FilamentInventoryDialog::on_weigh_in,      this);
    m_btn_swap->Bind(wxEVT_BUTTON,   &FilamentInventoryDialog::on_log_swap_purge, this);
    m_btn_adjust->Bind(wxEVT_BUTTON, &FilamentInventoryDialog::on_manual_adjust,  this);

    // ── Event history list ────────────────────────────────────────────────────
    auto* lbl_hist = new wxStaticText(panel, wxID_ANY, _L("History:"));
    lbl_hist->SetFont(lbl_hist->GetFont().Bold());
    sizer->Add(lbl_hist, 0, wxLEFT | wxRIGHT, FromDIP(10));
    sizer->AddSpacer(FromDIP(4));

    m_evt_list = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_NO_HEADER | wxBORDER_SIMPLE);
    m_evt_list->InsertColumn(0, wxEmptyString, wxLIST_FORMAT_LEFT, FromDIP(320));
    sizer->Add(m_evt_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

    panel->SetSizer(sizer);
    return panel;
}

void FilamentInventoryDialog::refresh()
{
    std::string cur_id = selected_id();
    populate_list();

    // Restore selection if the spool still exists
    if (!cur_id.empty()) {
        auto& spools = SpoolInventory::get().spools();
        for (int i = 0; i < m_list->GetItemCount(); ++i) {
            long data = m_list->GetItemData(i);
            if (data >= 0 && data < (long)spools.size() && spools[data].id == cur_id) {
                m_list->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
                break;
            }
        }
    }
    auto* s = cur_id.empty() ? nullptr : SpoolInventory::get().find(cur_id);
    populate_detail(s);
    update_button_states();
}

void FilamentInventoryDialog::populate_list()
{
    bool show_arch = m_chk_archived && m_chk_archived->GetValue();
    wxWindowUpdateLocker lock(m_list);
    m_list->DeleteAllItems();

    auto& spools = SpoolInventory::get().spools();
    for (size_t i = 0; i < spools.size(); ++i) {
        const auto& s = spools[i];
        if (s.archived && !show_arch) continue;

        long idx = m_list->InsertItem(m_list->GetItemCount(), wxEmptyString);
        m_list->SetItem(idx, 1, wxString(s.name));
        m_list->SetItem(idx, 2, wxString(s.material));

        double rem = s.remaining_weight_g();
        wxString rem_str = format_grams(rem);
        if (s.is_low())
            rem_str += " ⚠";
        m_list->SetItem(idx, 3, rem_str);

        // Store index into spools vector as item data (for lookup)
        m_list->SetItemData(idx, (long)i);

        // Color indicator: set item background tinted or just text color
        wxColour col = hex_to_colour(s.color_hex);
        m_list->SetItemBackgroundColour(idx,
            wxColour(std::min(255, (int)col.Red()   + 50),
                     std::min(255, (int)col.Green() + 50),
                     std::min(255, (int)col.Blue()  + 50)));

        if (s.archived)
            m_list->SetItemTextColour(idx, wxColour(160, 160, 160));
        else if (s.is_low())
            m_list->SetItemTextColour(idx, wxColour(200, 80, 0));
    }
}

void FilamentInventoryDialog::populate_detail(const FilamentSpool* spool)
{
    if (!spool) {
        m_det_name->SetLabel(_L("(no spool selected)"));
        m_det_brand->SetLabel(wxEmptyString);
        m_det_material->SetLabel(wxEmptyString);
        m_det_gauge->SetValue(0);
        m_det_remaining->SetLabel(wxEmptyString);
        m_det_cost->SetLabel(wxEmptyString);
        m_det_color->SetBackgroundColour(wxColour(200, 200, 200));
        populate_events(nullptr);
        return;
    }

    m_det_name->SetLabel(wxString(spool->name));
    m_det_brand->SetLabel(wxString(spool->brand));
    m_det_material->SetLabel(wxString(spool->material));
    m_det_color->SetBackgroundColour(hex_to_colour(spool->color_hex));
    m_det_color->Refresh();

    double rem = spool->remaining_weight_g();
    double pct = spool->remaining_pct();
    m_det_gauge->SetValue(std::clamp((int)pct, 0, 100));
    m_det_remaining->SetLabel(
        wxString::Format(_L("%s / %s remaining (%.0f%%)"),
                         format_grams(rem),
                         format_grams(spool->initial_weight_g),
                         pct));

    if (spool->cost_per_kg > 0.0)
        m_det_cost->SetLabel(
            wxString::Format(_L("Cost consumed: $%.2f  |  Cost/kg: $%.2f"),
                             spool->cost_used(), spool->cost_per_kg));
    else
        m_det_cost->SetLabel(wxEmptyString);

    populate_events(spool);

    m_det_name->GetParent()->Layout();
}

void FilamentInventoryDialog::populate_events(const FilamentSpool* spool)
{
    wxWindowUpdateLocker lock(m_evt_list);
    m_evt_list->DeleteAllItems();
    if (!spool) return;

    // Most-recent first
    for (int i = (int)spool->events.size() - 1; i >= 0; --i) {
        const auto& ev = spool->events[i];
        long idx = m_evt_list->InsertItem(m_evt_list->GetItemCount(), event_label(ev));
        if (ev.type == SpoolEventType::WeighIn)
            m_evt_list->SetItemTextColour(idx, wxColour(0, 120, 0));
        else if (ev.type == SpoolEventType::ManualAdjust && ev.weight_g < 0)
            m_evt_list->SetItemTextColour(idx, wxColour(0, 80, 200));
    }
}

std::string FilamentInventoryDialog::selected_id() const
{
    long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (sel < 0) return {};
    long data = m_list->GetItemData(sel);
    auto& spools = SpoolInventory::get().spools();
    if (data < 0 || data >= (long)spools.size()) return {};
    return spools[data].id;
}

void FilamentInventoryDialog::update_button_states()
{
    bool has_sel = !selected_id().empty();
    m_btn_edit->Enable(has_sel);
    m_btn_archive->Enable(has_sel);
    m_btn_delete->Enable(has_sel);
    m_btn_weigh->Enable(has_sel);
    m_btn_swap->Enable(has_sel);
    m_btn_adjust->Enable(has_sel);

    // Update archive button label
    if (has_sel) {
        auto* s = SpoolInventory::get().find(selected_id());
        if (s)
            m_btn_archive->SetLabel(s->archived ? _L("Unarchive") : _L("Archive"));
    }
}

// ── List event handlers ────────────────────────────────────────────────────────

void FilamentInventoryDialog::on_list_select(wxListEvent& e)
{
    std::string id = selected_id();
    const auto* s = SpoolInventory::get().find(id);
    populate_detail(s);
    update_button_states();
}

void FilamentInventoryDialog::on_show_archived(wxCommandEvent&)
{
    populate_list();
}

// ── CRUD handlers ─────────────────────────────────────────────────────────────

void FilamentInventoryDialog::on_add(wxCommandEvent&)
{
    FilamentSpool blank;
    blank.initial_weight_g = 1000.0;
    blank.tare_weight_g    = 240.0;
    blank.low_threshold_g  = 50.0;
    blank.color_hex        = "#FFFFFF";

    SpoolEditDialog dlg(this, blank, true);
    if (dlg.ShowModal() != wxID_OK) return;

    SpoolInventory::get().add_spool(dlg.get_spool());
    populate_list();
    // Select the newly added spool (last item)
    if (m_list->GetItemCount() > 0) {
        m_list->SetItemState(m_list->GetItemCount() - 1,
                             wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
    update_button_states();
}

void FilamentInventoryDialog::on_edit(wxCommandEvent&)
{
    std::string id = selected_id();
    if (id.empty()) return;
    auto* s = SpoolInventory::get().find(id);
    if (!s) return;

    SpoolEditDialog dlg(this, *s, false);
    if (dlg.ShowModal() != wxID_OK) return;

    *s = dlg.get_spool();
    s->id = id; // preserve original ID
    SpoolInventory::get().save();
    populate_list();
    populate_detail(SpoolInventory::get().find(id));
}

void FilamentInventoryDialog::on_edit_activated(wxListEvent&)
{
    wxCommandEvent dummy;
    on_edit(dummy);
}

void FilamentInventoryDialog::on_archive(wxCommandEvent&)
{
    std::string id = selected_id();
    if (id.empty()) return;
    auto* s = SpoolInventory::get().find(id);
    if (!s) return;

    s->archived = !s->archived;
    SpoolInventory::get().save();
    populate_list();
    update_button_states();
}

void FilamentInventoryDialog::on_delete(wxCommandEvent&)
{
    std::string id = selected_id();
    if (id.empty()) return;
    auto* s = SpoolInventory::get().find(id);
    if (!s) return;

    int ans = wxMessageBox(
        wxString::Format(_L("Delete spool \"%s\" and all its history? This cannot be undone.\n"
                            "Consider archiving instead."), s->name),
        _L("Confirm Delete"),
        wxYES_NO | wxICON_WARNING, this);
    if (ans != wxYES) return;

    SpoolInventory::get().remove_spool(id);
    populate_list();
    populate_detail(nullptr);
    update_button_states();
}

// ── Action handlers ───────────────────────────────────────────────────────────

void FilamentInventoryDialog::on_weigh_in(wxCommandEvent&)
{
    std::string id = selected_id();
    if (id.empty()) return;
    auto* s = SpoolInventory::get().find(id);
    if (!s) return;

    SpoolWeighInDialog dlg(this, *s);
    if (dlg.ShowModal() != wxID_OK) return;

    SpoolInventory::get().record_weigh_in(id, dlg.get_total_weight(),
                                          dlg.get_note().ToStdString());
    populate_detail(SpoolInventory::get().find(id));
    populate_list();
}

void FilamentInventoryDialog::on_log_swap_purge(wxCommandEvent&)
{
    std::string id = selected_id();
    if (id.empty()) return;
    auto* s = SpoolInventory::get().find(id);
    if (!s) return;

    // Simple input dialog
    wxString val = wxGetTextFromUser(
        wxString::Format(
            _L("How much filament (grams) was purged when loading/swapping this spool?\n"
               "This accounts for the material consumed priming the nozzle.")),
        _L("Log Swap Purge"), "2.0", this);
    if (val.IsEmpty()) return;

    double g = 0.0;
    if (!val.ToDouble(&g) || g < 0.0) {
        wxMessageBox(_L("Please enter a valid weight."), _L("Validation"),
                     wxICON_WARNING | wxOK, this);
        return;
    }

    SpoolInventory::get().record_swap_purge(id, g);
    populate_detail(SpoolInventory::get().find(id));
    populate_list();
}

void FilamentInventoryDialog::on_manual_adjust(wxCommandEvent&)
{
    std::string id = selected_id();
    if (id.empty()) return;

    wxString val = wxGetTextFromUser(
        _L("Enter adjustment in grams.\n"
           "Positive = filament was used (reduce remaining).\n"
           "Negative = correction that adds filament back."),
        _L("Manual Adjustment"), "0", this);
    if (val.IsEmpty()) return;

    double g = 0.0;
    if (!val.ToDouble(&g)) {
        wxMessageBox(_L("Please enter a valid number."), _L("Validation"),
                     wxICON_WARNING | wxOK, this);
        return;
    }

    wxString note = wxGetTextFromUser(_L("Note (optional):"), _L("Adjustment Note"), {}, this);
    SpoolInventory::get().record_adjustment(id, g, note.ToStdString());
    populate_detail(SpoolInventory::get().find(id));
    populate_list();
}

void FilamentInventoryDialog::on_dpi_changed(const wxRect& /*suggested_rect*/)
{
    Refresh();
}

} // namespace GUI
} // namespace Slic3r
