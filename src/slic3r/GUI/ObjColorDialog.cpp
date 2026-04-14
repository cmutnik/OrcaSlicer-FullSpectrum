#include <algorithm>
#include <map>
#include <sstream>
//#include "libslic3r/FlushVolCalc.hpp"
#include "ObjColorDialog.hpp"
#include "BitmapCache.hpp"
#include "GUI.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "MsgDialog.hpp"
#include "Widgets/Button.hpp"
#include "slic3r/Utils/ColorSpaceConvert.hpp"
#include "MainFrame.hpp"
#include "libslic3r/Config.hpp"
#include "BitmapComboBox.hpp"
#include "Widgets/ComboBox.hpp"
#include <wx/sizer.h>

#include "libslic3r/ObjColorUtils.hpp"
#include "libslic3r/MixedFilament.hpp"

using namespace Slic3r;
using namespace Slic3r::GUI;

int objcolor_scale(const int val) { return val * Slic3r::GUI::wxGetApp().em_unit() / 10; }
int OBJCOLOR_ITEM_WIDTH() { return objcolor_scale(30); }
static const wxColour g_text_color = wxColour(107, 107, 107, 255);
const int HEADER_BORDER  = 5;
const int CONTENT_BORDER = 3;
const int PANEL_WIDTH = 430;
const int COLOR_LABEL_WIDTH = 165;

#undef  ICON_SIZE
#define ICON_SIZE                 wxSize(FromDIP(16), FromDIP(16))
#define MIN_OBJCOLOR_DIALOG_WIDTH FromDIP(460)
#define FIX_SCROLL_HEIGTH         FromDIP(400)
#define BTN_SIZE                  wxSize(FromDIP(58), FromDIP(24))
#define BTN_GAP                   FromDIP(20)

static void update_ui(wxWindow* window)
{
    Slic3r::GUI::wxGetApp().UpdateDarkUI(window);
}

static const char g_min_cluster_color = 1;
//static const char g_max_cluster_color = 15;
static const char g_max_color = 16;
static constexpr float BLEND_MATCH_THRESHOLD = 15.0f; // try blend when nearest physical DeltaE > this
static constexpr float BLEND_ACCEPT_MAX_DE   = 30.0f; // only create mix if blend DeltaE < this

// Convert CIE Lab to a gamut-clamped wxColour (D65 white point, sRGB gamma).
static wxColour lab_to_wxcolour(float L, float a, float b) {
    float fy = (L + 16.f) / 116.f;
    float fx = a / 500.f + fy;
    float fz = fy - b / 200.f;
    auto finv = [](float t) { return t > 0.206897f ? t*t*t : (t - 16.f/116.f)/7.787f; };
    float X = 0.95047f * finv(fx);
    float Y =             finv(fy);
    float Z = 1.08883f * finv(fz);
    float r  =  3.2406f*X - 1.5372f*Y - 0.4986f*Z;
    float g  = -0.9689f*X + 1.8758f*Y + 0.0415f*Z;
    float bv =  0.0557f*X - 0.2040f*Y + 1.0570f*Z;
    auto gc = [](float c) {
        c = std::max(0.f, std::min(1.f, c));
        return c <= 0.0031308f ? 12.92f*c : 1.055f*std::pow(c, 1.f/2.4f) - 0.055f;
    };
    return wxColour(
        static_cast<unsigned char>(std::round(gc(r)  * 255.f)),
        static_cast<unsigned char>(std::round(gc(g)  * 255.f)),
        static_cast<unsigned char>(std::round(gc(bv) * 255.f)));
}

const  StateColor ok_btn_bg(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed),
                     std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                     std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
const StateColor  ok_btn_disable_bg(std::pair<wxColour, int>(wxColour(205, 201, 201), StateColor::Pressed),
                                   std::pair<wxColour, int>(wxColour(205, 201, 201), StateColor::Hovered),
                                   std::pair<wxColour, int>(wxColour(205, 201, 201), StateColor::Normal));
wxBoxSizer* ObjColorDialog::create_btn_sizer(long flags)
{
    auto btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer();

    StateColor ok_btn_bd(
        std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal)
    );
    StateColor ok_btn_text(
        std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal)
    );
    StateColor cancel_btn_bg(
        std::pair<wxColour, int>(wxColour(206, 206, 206), StateColor::Pressed),
        std::pair<wxColour, int>(wxColour(238, 238, 238), StateColor::Hovered),
        std::pair<wxColour, int>(wxColour(255, 255, 255), StateColor::Normal)
    );
    StateColor cancel_btn_bd_(
        std::pair<wxColour, int>(wxColour(38, 46, 48), StateColor::Normal)
    );
    StateColor cancel_btn_text(
        std::pair<wxColour, int>(wxColour(38, 46, 48), StateColor::Normal)
    );
    StateColor calc_btn_bg(
        std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed),
        std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
        std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal)
    );
    StateColor calc_btn_bd(
        std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal)
    );
    StateColor calc_btn_text(
        std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal)
    );
    if (flags & wxOK) {
        Button* ok_btn = new Button(this, _L("OK"));
        ok_btn->SetMinSize(BTN_SIZE);
        ok_btn->SetCornerRadius(FromDIP(12));
        ok_btn->Enable(false);
        ok_btn->SetBackgroundColor(ok_btn_disable_bg);
        ok_btn->SetBorderColor(ok_btn_bd);
        ok_btn->SetTextColor(ok_btn_text);
        ok_btn->SetFocus();
        ok_btn->SetId(wxID_OK);
        btn_sizer->Add(ok_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, BTN_GAP);
        m_button_list[wxOK] = ok_btn;
    }
    if (flags & wxCANCEL) {
        Button* cancel_btn = new Button(this, _L("Cancel"));
        cancel_btn->SetMinSize(BTN_SIZE);
        cancel_btn->SetCornerRadius(FromDIP(12));
        cancel_btn->SetBackgroundColor(cancel_btn_bg);
        cancel_btn->SetBorderColor(cancel_btn_bd_);
        cancel_btn->SetTextColor(cancel_btn_text);
        cancel_btn->SetId(wxID_CANCEL);
        btn_sizer->Add(cancel_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, BTN_GAP);
        m_button_list[wxCANCEL] = cancel_btn;
    }
    return btn_sizer;
}

void ObjColorDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    for (auto button_item : m_button_list)
    {
        if (button_item.first == wxRESET)
        {
            button_item.second->SetMinSize(wxSize(FromDIP(75), FromDIP(24)));
            button_item.second->SetCornerRadius(FromDIP(12));
        }
        if (button_item.first == wxOK) {
            button_item.second->SetMinSize(BTN_SIZE);
            button_item.second->SetCornerRadius(FromDIP(12));
        }
        if (button_item.first == wxCANCEL) {
            button_item.second->SetMinSize(BTN_SIZE);
            button_item.second->SetCornerRadius(FromDIP(12));
        }
    }
    m_panel_ObjColor->msw_rescale();
    this->Refresh();
};

ObjColorDialog::ObjColorDialog(wxWindow *                      parent,
                               std::vector<Slic3r::RGBA> &     input_colors,
                               bool                            is_single_color,
                               const std::vector<std::string> &extruder_colours,
                               std::vector<unsigned char> &    filament_ids,
                               unsigned char &                 first_extruder_id)
    : DPIDialog(parent ? parent : static_cast<wxWindow *>(wxGetApp().mainframe),
                wxID_ANY,
                _(L("Obj file Import color")),
                wxDefaultPosition,
                wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE /* | wxRESIZE_BORDER*/)
    , m_filament_ids(filament_ids)
    , m_first_extruder_id(first_extruder_id)
{
    std::string icon_path = (boost::format("%1%/images/Snapmaker_OrcaTitle.ico") % Slic3r::resources_dir()).str();
    SetIcon(wxIcon(Slic3r::encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    auto m_line_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    m_line_top->SetBackgroundColour(wxColour(166, 169, 170));

    this->SetBackgroundColour(*wxWHITE);
    this->SetMinSize(wxSize(MIN_OBJCOLOR_DIALOG_WIDTH, -1));

    m_panel_ObjColor = new ObjColorPanel(this, input_colors, is_single_color, extruder_colours, filament_ids, first_extruder_id);

    auto main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(m_line_top, 0, wxEXPAND, 0);
    // set min sizer width according to extruders count
    auto sizer_width = (int) (2.8 * OBJCOLOR_ITEM_WIDTH());
    sizer_width      = sizer_width > MIN_OBJCOLOR_DIALOG_WIDTH ? sizer_width : MIN_OBJCOLOR_DIALOG_WIDTH;
    main_sizer->SetMinSize(wxSize(sizer_width, -1));
    main_sizer->Add(m_panel_ObjColor, 1, wxEXPAND | wxALL, 0);

    auto btn_sizer = create_btn_sizer(wxOK | wxCANCEL);
    {
        m_button_list[wxOK]->Bind(wxEVT_UPDATE_UI, ([this](wxUpdateUIEvent &e) {
           if (m_panel_ObjColor->is_ok() == m_button_list[wxOK]->IsEnabled()) { return; }
           m_button_list[wxOK]->Enable(m_panel_ObjColor->is_ok());
           m_button_list[wxOK]->SetBackgroundColor(m_panel_ObjColor->is_ok() ? ok_btn_bg : ok_btn_disable_bg);
         }));
    }
    main_sizer->Add(btn_sizer, 0, wxBOTTOM | wxRIGHT | wxEXPAND, BTN_GAP);
    SetSizer(main_sizer);
    main_sizer->SetSizeHints(this);

    if (this->FindWindowById(wxID_OK, this)) {
        this->FindWindowById(wxID_OK, this)->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {// if OK button is clicked..
              m_panel_ObjColor->update_filament_ids();
              EndModal(wxID_OK);
            }, wxID_OK);
    }
    if (this->FindWindowById(wxID_CANCEL, this)) {
        update_ui(static_cast<wxButton*>(this->FindWindowById(wxID_CANCEL, this)));
        this->FindWindowById(wxID_CANCEL, this)->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxCANCEL); });
    }
    this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) { EndModal(wxCANCEL); });

    wxGetApp().UpdateDlgDarkUI(this);
}
RGBA     convert_to_rgba(const wxColour &color)
{
    RGBA rgba;
    rgba[0] = std::clamp(color.Red() / 255.f, 0.f, 1.f);
    rgba[1] = std::clamp(color.Green() / 255.f, 0.f, 1.f);
    rgba[2] = std::clamp(color.Blue() / 255.f, 0.f, 1.f);
    rgba[3] = std::clamp(color.Alpha() / 255.f, 0.f, 1.f);
    return rgba;
}
wxColour convert_to_wxColour(const RGBA &color)
{
    auto     r = std::clamp((int) (color[0] * 255.f), 0, 255);
    auto     g = std::clamp((int) (color[1] * 255.f), 0, 255);
    auto     b = std::clamp((int) (color[2] * 255.f), 0, 255);
    auto     a = std::clamp((int) (color[3] * 255.f), 0, 255);
    wxColour wx_color(r,g,b,a);
    return wx_color;
}
// This panel contains all control widgets for both simple and advanced mode (these reside in separate sizers)
ObjColorPanel::ObjColorPanel(wxWindow *                       parent,
                             std::vector<Slic3r::RGBA>&       input_colors,
                             bool                             is_single_color,
                             const std::vector<std::string>&  extruder_colours,
                             std::vector<unsigned char> &    filament_ids,
                             unsigned char &                 first_extruder_id)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize /*,wxBORDER_RAISED*/)
    , m_input_colors(input_colors)
    , m_filament_ids(filament_ids)
    , m_first_extruder_id(first_extruder_id)
{
    if (input_colors.size() == 0) { return; }
    for (const std::string& color : extruder_colours) {
        m_colours.push_back(wxColor(color));
    }
    //deal input_colors
    m_input_colors_size = input_colors.size();
    for (size_t i = 0; i < input_colors.size(); i++) {
        if (color_is_equal(input_colors[i] , UNDEFINE_COLOR)) { // not define color range:0~1
            input_colors[i]=convert_to_rgba( m_colours[0]);
        }
    }
    if (is_single_color && input_colors.size() >=1) {
        m_cluster_colors_from_algo.emplace_back(input_colors[0]);
        m_cluster_colours.emplace_back(convert_to_wxColour(input_colors[0]));
        m_cluster_labels_from_algo.reserve(m_input_colors_size);
        for (size_t i = 0; i < m_input_colors_size; i++) {
            m_cluster_labels_from_algo.emplace_back(0);
        }
        m_cluster_map_filaments.resize(m_cluster_colors_from_algo.size());
        m_color_num_recommend = m_color_cluster_num_by_algo = m_cluster_colors_from_algo.size();
    } else {//cluster deal
        deal_algo(-1);
    }
    //end first cluster
    //draw ui
    auto sizer_width = FromDIP(300);
    // Create two switched panels with their own sizers
    m_sizer_simple          = new wxBoxSizer(wxVERTICAL);
    m_page_simple			= new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_page_simple->SetSizer(m_sizer_simple);
    m_page_simple->SetBackgroundColour(*wxWHITE);

    update_ui(m_page_simple);
    // BBS
    m_sizer_simple->AddSpacer(FromDIP(10));
    // BBS: for tunning flush volumes
    {
        //color cluster results
        wxBoxSizer *  specify_cluster_sizer               = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText *specify_color_cluster_title = new wxStaticText(m_page_simple, wxID_ANY, _L("Specify number of colors:"));
        specify_color_cluster_title->SetFont(Label::Head_14);
        specify_cluster_sizer->Add(specify_color_cluster_title, 0, wxALIGN_CENTER | wxALL, FromDIP(5));

        m_color_cluster_num_by_user_ebox = new wxTextCtrl(m_page_simple, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(25), -1), wxTE_PROCESS_ENTER);
        m_color_cluster_num_by_user_ebox->SetValue(std::to_string(m_color_cluster_num_by_algo).c_str());
        {//event
            auto on_apply_color_cluster_text_modify = [this](wxEvent &e) {
                wxString str        = m_color_cluster_num_by_user_ebox->GetValue();
                int      number = wxAtoi(str);
                if (number > m_color_num_recommend || number < g_min_cluster_color) {
                    number = number < g_min_cluster_color ? g_min_cluster_color : m_color_num_recommend;
                    str    = wxString::Format(("%d"), number);
                    m_color_cluster_num_by_user_ebox->SetValue(str);
                    MessageDialog dlg(nullptr, wxString::Format(_L("The color count should be in range [%d, %d]."), g_min_cluster_color, m_color_num_recommend),
                                      _L("Warning"), wxICON_WARNING | wxOK);
                    dlg.ShowModal();
                }
                e.Skip();
            };
            m_color_cluster_num_by_user_ebox->Bind(wxEVT_TEXT_ENTER, on_apply_color_cluster_text_modify);
            m_color_cluster_num_by_user_ebox->Bind(wxEVT_KILL_FOCUS, on_apply_color_cluster_text_modify);
            m_color_cluster_num_by_user_ebox->Bind(wxEVT_COMMAND_TEXT_UPDATED, [this](wxCommandEvent &) {
                wxString str        = m_color_cluster_num_by_user_ebox->GetValue();
                int    number = wxAtof(str);
                if (number > m_color_num_recommend || number < g_min_cluster_color) {
                    number = number < g_min_cluster_color ? g_min_cluster_color : m_color_num_recommend;
                    str    = wxString::Format(("%d"), number);
                    m_color_cluster_num_by_user_ebox->SetValue(str);
                    m_color_cluster_num_by_user_ebox->SetInsertionPointEnd();
                }
                if (m_last_cluster_num != number) {
                    deal_algo(number, true);
                    Layout();
                    //Fit();
                    Refresh();
                    Update();
                    m_last_cluster_num = number;
                }
            });
            m_color_cluster_num_by_user_ebox->Bind(wxEVT_CHAR, [this](wxKeyEvent &e) {
                int keycode = e.GetKeyCode();
                wxString input_char = wxString::Format("%c", keycode);
                long     value;
                if (!input_char.ToLong(&value))
                    return;
                e.Skip();
            });
        }
        specify_cluster_sizer->AddSpacer(FromDIP(2));
        specify_cluster_sizer->Add(m_color_cluster_num_by_user_ebox, 0, wxALIGN_CENTER | wxALL, 0);
        specify_cluster_sizer->AddSpacer(FromDIP(15));
        wxStaticText *recommend_color_cluster_title = new wxStaticText(m_page_simple, wxID_ANY, "(" + std::to_string(m_color_num_recommend) + " " + _L("Recommended ") + ")");
        specify_cluster_sizer->Add(recommend_color_cluster_title, 0, wxALIGN_CENTER | wxALL, 0);

        m_sizer_simple->Add(specify_cluster_sizer, 0, wxEXPAND | wxLEFT, FromDIP(20));

        wxBoxSizer *  current_filaments_title_sizer  = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText *current_filaments_title = new wxStaticText(m_page_simple, wxID_ANY, _L("Current filament colors (\u2611 = allow optimizer to change):"));
        current_filaments_title->SetFont(Label::Head_14);
        current_filaments_title_sizer->Add(current_filaments_title, 0, wxALIGN_CENTER | wxALL, FromDIP(5));
        m_sizer_simple->Add(current_filaments_title_sizer, 0, wxEXPAND | wxLEFT, FromDIP(20));

        wxBoxSizer *  current_filaments_sizer = new wxBoxSizer(wxHORIZONTAL);
        current_filaments_sizer->AddSpacer(FromDIP(10));
        for (size_t i = 0; i < m_colours.size(); i++) {
            auto extruder_icon_sizer = create_extruder_icon_and_rgba_sizer(m_page_simple, i, m_colours[i]);
            current_filaments_sizer->Add(extruder_icon_sizer, 0, wxALIGN_CENTER | wxALIGN_CENTER_VERTICAL, FromDIP(10));
        }
        m_sizer_simple->Add(current_filaments_sizer, 0, wxEXPAND | wxLEFT, FromDIP(20));
        //colors table
        m_scrolledWindow = new wxScrolledWindow(m_page_simple,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxSB_VERTICAL);
        m_sizer_simple->Add(m_scrolledWindow, 0, wxEXPAND | wxALL, FromDIP(5));
        draw_table();
        //buttons
        wxBoxSizer *quick_set_sizer = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText *quick_set_title = new wxStaticText(m_page_simple, wxID_ANY, _L("Quick set:"));
        quick_set_title->SetFont(Label::Head_12);
        quick_set_sizer->Add(quick_set_title, 0, wxALIGN_CENTER | wxALL, 0);
        quick_set_sizer->AddSpacer(FromDIP(10));

        auto calc_approximate_match_btn_sizer  = create_approximate_match_btn_sizer(m_page_simple);
        auto calc_keep_all_colors_btn_sizer    = create_keep_all_colors_btn_sizer(m_page_simple);
        auto calc_add_btn_sizer                = create_add_btn_sizer(m_page_simple);
        auto calc_reset_btn_sizer              = create_reset_btn_sizer(m_page_simple);
        quick_set_sizer->Add(calc_add_btn_sizer, 0, wxALIGN_CENTER | wxALL, 0);
        quick_set_sizer->AddSpacer(FromDIP(10));
        quick_set_sizer->Add(calc_approximate_match_btn_sizer, 0, wxALIGN_CENTER | wxALL, 0);
        quick_set_sizer->AddSpacer(FromDIP(10));
        quick_set_sizer->Add(calc_keep_all_colors_btn_sizer, 0, wxALIGN_CENTER | wxALL, 0);
        quick_set_sizer->AddSpacer(FromDIP(10));
        quick_set_sizer->Add(calc_reset_btn_sizer, 0, wxALIGN_CENTER | wxALL, 0);
        quick_set_sizer->AddSpacer(FromDIP(14));
        m_optimize_colors_checkbox = new wxCheckBox(m_page_simple, wxID_ANY, _L("Optimize filament colors"));
        m_optimize_colors_checkbox->SetToolTip(_L("Adjust base filament colors to minimize blend error for this model's palette."));
        quick_set_sizer->Add(m_optimize_colors_checkbox, 0, wxALIGN_CENTER_VERTICAL, 0);
        quick_set_sizer->AddSpacer(FromDIP(10));
        m_sizer_simple->Add(quick_set_sizer, 0, wxEXPAND | wxLEFT, FromDIP(30));

        wxBoxSizer *warning_sizer = new wxBoxSizer(wxHORIZONTAL);
        m_warning_text = new wxStaticText(m_page_simple, wxID_ANY, "");
        warning_sizer->Add(m_warning_text, 0, wxALIGN_CENTER | wxALL, 0);
        m_sizer_simple->Add(warning_sizer, 0, wxEXPAND | wxLEFT, FromDIP(30));

        m_sizer_simple->AddSpacer(10);
    }
    deal_default_strategy();
    //page_simple//page_advanced
    m_sizer = new wxBoxSizer(wxVERTICAL);
    m_sizer->Add(m_page_simple, 0, wxEXPAND, 0);

    m_sizer->SetSizeHints(this);
    SetSizer(m_sizer);
    this->Layout();
}

void ObjColorPanel::msw_rescale()
{
    for (unsigned int i = 0; i < m_extruder_icon_list.size(); ++i) {
        auto bitmap = *get_extruder_color_icon(m_colours[i].GetAsString(wxC2S_HTML_SYNTAX).ToStdString(), std::to_string(i + 1), FromDIP(16), FromDIP(16));
        m_extruder_icon_list[i]->SetBitmap(bitmap);
    }
   /* for (unsigned int i = 0; i < m_color_cluster_icon_list.size(); ++i) {
        auto bitmap = *get_extruder_color_icon(m_cluster_colours[i].GetAsString(wxC2S_HTML_SYNTAX).ToStdString(), std::to_string(i + 1), FromDIP(16), FromDIP(16));
        m_color_cluster_icon_list[i]->SetBitmap(bitmap);
    }*/
}

bool ObjColorPanel::is_ok() {
    for (auto item : m_result_icon_list) {
        if (item->bitmap_combox->IsShown()) {
            auto selection = item->bitmap_combox->GetSelection();
            if (selection < 1) {
                return false;
            }
        }
    }
    return true;
}

void ObjColorPanel::update_filament_ids()
{
    const int existing_filament_count = static_cast<int>(m_colours.size());
    std::map<int, int> appended_filament_id_map;

    // Collect all "new" combobox slots referenced by any cluster
    std::vector<int> selected_new_indices;
    selected_new_indices.reserve(m_cluster_map_filaments.size());
    for (int id : m_cluster_map_filaments) {
        if (id > existing_filament_count)
            selected_new_indices.emplace_back(id);
    }
    std::sort(selected_new_indices.begin(), selected_new_indices.end());
    selected_new_indices.erase(
        std::unique(selected_new_indices.begin(), selected_new_indices.end()),
        selected_new_indices.end());

    // Pass 1: add new physical filaments (slots NOT in m_mix_proposals_by_slot)
    int next_physical_id = existing_filament_count + 1;
    for (int slot : selected_new_indices) {
        if (m_mix_proposals_by_slot.count(slot)) continue;
        const int idx = slot - existing_filament_count - 1;
        if (idx < 0 || idx >= static_cast<int>(m_new_add_colors.size())) continue;
        wxGetApp().sidebar().add_custom_filament(m_new_add_colors[idx]);
        appended_filament_id_map.emplace(slot, next_physical_id++);
    }

    // Apply optimized physical filament colors before creating mixed entries so that
    // MixedFilamentManager computes display colors from the updated base colors.
    if (!m_optimized_physical_colors.empty())
        wxGetApp().sidebar().set_filament_colors_from_import(m_optimized_physical_colors);

    // Pass 2: add mixed filament proposals (after all physical adds, so virtual IDs are correct)
    for (int slot : selected_new_indices) {
        auto it = m_mix_proposals_by_slot.find(slot);
        if (it == m_mix_proposals_by_slot.end()) continue;
        auto &prop = it->second;
        unsigned int virtual_id = wxGetApp().sidebar().add_mixed_filament_from_import(
            prop.component_a, prop.component_b, prop.mix_b_percent);
        appended_filament_id_map.emplace(slot, static_cast<int>(virtual_id));
    }

    auto resolve_filament_id = [&appended_filament_id_map](int mapped_filament_id) {
        const auto it = appended_filament_id_map.find(mapped_filament_id);
        const int resolved = it == appended_filament_id_map.end() ? mapped_filament_id : it->second;
        return static_cast<unsigned char>(resolved);
    };

    m_filament_ids.clear();
    m_filament_ids.reserve(m_input_colors_size);
    for (size_t i = 0; i < m_input_colors_size; i++)
        m_filament_ids.emplace_back(resolve_filament_id(m_cluster_map_filaments[m_cluster_labels_from_algo[i]]));
    m_first_extruder_id = resolve_filament_id(m_cluster_map_filaments[0]);
}

wxBoxSizer *ObjColorPanel::create_approximate_match_btn_sizer(wxWindow *parent)
{
    auto       btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    StateColor calc_btn_bg(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed), std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                           std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_bd(std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_text(std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal));
    //create btn
    m_quick_approximate_match_btn = new Button(parent, _L("Color match"));
    m_quick_approximate_match_btn->SetToolTip(_L("Approximate color matching."));
    auto cur_btn         = m_quick_approximate_match_btn;
    cur_btn->SetFont(Label::Body_13);
    cur_btn->SetMinSize(wxSize(FromDIP(60), FromDIP(20)));
    cur_btn->SetCornerRadius(FromDIP(10));
    cur_btn->SetBackgroundColor(calc_btn_bg);
    cur_btn->SetBorderColor(calc_btn_bd);
    cur_btn->SetTextColor(calc_btn_text);
    cur_btn->SetFocus();
    btn_sizer->Add(cur_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    cur_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        deal_approximate_match_btn();
    });
    return btn_sizer;
}

wxBoxSizer *ObjColorPanel::create_keep_all_colors_btn_sizer(wxWindow *parent)
{
    auto       btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    StateColor calc_btn_bg(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed),
                           std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                           std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_bd(std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_text(std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal));
    m_quick_keep_all_colors_btn = new Button(parent, _L("Keep all colors"));
    m_quick_keep_all_colors_btn->SetToolTip(
        _L("Map all incoming colors using only the existing filaments and auto-generated Mixed Filament blends. No new physical filament slots are added."));
    auto cur_btn = m_quick_keep_all_colors_btn;
    cur_btn->SetFont(Label::Body_13);
    cur_btn->SetMinSize(wxSize(FromDIP(80), FromDIP(20)));
    cur_btn->SetCornerRadius(FromDIP(10));
    cur_btn->SetBackgroundColor(calc_btn_bg);
    cur_btn->SetBorderColor(calc_btn_bd);
    cur_btn->SetTextColor(calc_btn_text);
    btn_sizer->Add(cur_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    cur_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        deal_keep_all_colors_btn();
    });
    return btn_sizer;
}

wxBoxSizer *ObjColorPanel::create_add_btn_sizer(wxWindow *parent)
{
    auto       btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    StateColor calc_btn_bg(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed), std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                           std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_bd(std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_text(std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal));
    // create btn
    m_quick_add_btn = new Button(parent, _L("Append all"));
    m_quick_add_btn->SetToolTip(_L("Add all clustered colors after the existing filaments."));
    auto cur_btn    = m_quick_add_btn;
    cur_btn->SetFont(Label::Body_13);
    cur_btn->SetMinSize(wxSize(FromDIP(60), FromDIP(20)));
    cur_btn->SetCornerRadius(FromDIP(10));
    cur_btn->SetBackgroundColor(calc_btn_bg);
    cur_btn->SetBorderColor(calc_btn_bd);
    cur_btn->SetTextColor(calc_btn_text);
    cur_btn->SetFocus();
    btn_sizer->Add(cur_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    cur_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        deal_add_btn();
    });
    return btn_sizer;
}

wxBoxSizer *ObjColorPanel::create_reset_btn_sizer(wxWindow *parent)
{
    auto       btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    StateColor calc_btn_bg(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed), std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                           std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_bd(std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_text(std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal));
    // create btn
    m_quick_reset_btn = new Button(parent, _L("Reset"));
    m_quick_reset_btn->SetToolTip(_L("Reset mapped extruders."));
    auto cur_btn      = m_quick_reset_btn;
    cur_btn->SetFont(Label::Body_13);
    cur_btn->SetMinSize(wxSize(FromDIP(60), FromDIP(20)));
    cur_btn->SetCornerRadius(FromDIP(10));
    cur_btn->SetBackgroundColor(calc_btn_bg);
    cur_btn->SetBorderColor(calc_btn_bd);
    cur_btn->SetTextColor(calc_btn_text);
    cur_btn->SetFocus();
    btn_sizer->Add(cur_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 0);
    cur_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        deal_reset_btn();
    });
    return btn_sizer;
}

wxBoxSizer *ObjColorPanel::create_extruder_icon_and_rgba_sizer(wxWindow *parent, int id, const wxColour &color)
{
    // Vertical sizer: color icon on top, lock checkbox below
    auto outer_sizer = new wxBoxSizer(wxVERTICAL);

    wxButton *icon = new wxButton(parent, wxID_ANY, {}, wxDefaultPosition, ICON_SIZE, wxBORDER_NONE | wxBU_AUTODRAW);
    icon->SetBitmap(*get_extruder_color_icon(color.GetAsString(wxC2S_HTML_SYNTAX).ToStdString(), std::to_string(id + 1), FromDIP(16), FromDIP(16)));
    icon->SetCanFocus(false);
    m_extruder_icon_list.emplace_back(icon);
    outer_sizer->Add(icon, 0, wxALIGN_CENTER, 0);

    // Checkbox: checked = allow optimizer to change this filament's color
    wxCheckBox *lock_cb = new wxCheckBox(parent, wxID_ANY, wxEmptyString);
    lock_cb->SetValue(true); // default: changeable
    lock_cb->SetToolTip(_L("Allow optimizer to change this filament color"));
    m_filament_lock_checkboxes.emplace_back(lock_cb);
    outer_sizer->Add(lock_cb, 0, wxALIGN_CENTER | wxTOP, FromDIP(2));

    // Wrap in a small horizontal sizer with right spacer to match original spacing
    auto icon_sizer = new wxBoxSizer(wxHORIZONTAL);
    icon_sizer->Add(outer_sizer, 0, wxALIGN_CENTER_VERTICAL, 0);
    icon_sizer->AddSpacer(FromDIP(5));
    return icon_sizer;
}

std::string ObjColorPanel::get_color_str(const wxColour &color) {
    std::string str = ("R:" + std::to_string(color.Red()) +
                          std::string(" G:") + std::to_string(color.Green()) +
                          std::string(" B:") + std::to_string(color.Blue()) +
                          std::string(" A:") + std::to_string(color.Alpha()));
    return str;
}

ComboBox *ObjColorPanel::CreateEditorCtrl(wxWindow *parent, int id) // wxRect labelRect,, const wxVariant &value
{
    std::vector<wxBitmap *> icons = get_extruder_color_icons();
    const double            em          = Slic3r::GUI::wxGetApp().em_unit();
    bool                    thin_icon   = false;
    const int               icon_width  = lround((thin_icon ? 2 : 4.4) * em);
    const int               icon_height = lround(2 * em);
    m_combox_icon_width                 = icon_width;
    m_combox_icon_height                = icon_height;
    wxColour undefined_color(0,255,0,255);
    icons.insert(icons.begin(), get_extruder_color_icon(undefined_color.GetAsString(wxC2S_HTML_SYNTAX).ToStdString(), std::to_string(-1), icon_width, icon_height));
    if (icons.empty())
        return nullptr;

    ::ComboBox *c_editor = new ::ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(m_combox_width), -1), 0, nullptr,
                                          wxCB_READONLY | CB_NO_DROP_ICON | CB_NO_TEXT);
    c_editor->SetMinSize(wxSize(FromDIP(m_combox_width), -1));
    c_editor->SetMaxSize(wxSize(FromDIP(m_combox_width), -1));
    c_editor->GetDropDown().SetUseContentWidth(true);
    for (size_t i = 0; i < icons.size(); i++) {
        c_editor->Append(wxString::Format("%d", i), *icons[i]);
        if (i == 0) {
            c_editor->SetItemTooltip(i,undefined_color.GetAsString(wxC2S_HTML_SYNTAX));
        } else {
            c_editor->SetItemTooltip(i, m_colours[i-1].GetAsString(wxC2S_HTML_SYNTAX));
        }
    }
    c_editor->SetSelection(0);
    c_editor->SetName(wxString::Format("%d", id));
    c_editor->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &evt) {
        auto *com_box = static_cast<ComboBox *>(evt.GetEventObject());
        int   i       = atoi(com_box->GetName().c_str());
        if (i < m_cluster_map_filaments.size()) { m_cluster_map_filaments[i] = com_box->GetSelection(); }
        evt.StopPropagation();
    });
    return c_editor;
}

void ObjColorPanel::deal_keep_all_colors_btn()
{
    if (!m_new_add_colors.empty() || !m_mix_proposals_by_slot.empty())
        deal_reset_btn();

    m_warning_text->SetLabelText("");
    if (m_result_icon_list.empty()) return;
    const int map_count = static_cast<int>(m_colours.size());
    if (map_count < 1) return;

    auto calc_de = [](wxColour c1, wxColour c2) {
        float lab[2][3];
        RGB2Lab(c1.Red(), c1.Green(), c1.Blue(), &lab[0][0], &lab[0][1], &lab[0][2]);
        RGB2Lab(c2.Red(), c2.Green(), c2.Blue(), &lab[1][0], &lab[1][1], &lab[1][2]);
        return DeltaE76(lab[0][0], lab[0][1], lab[0][2], lab[1][0], lab[1][1], lab[1][2]);
    };

    for (size_t i = 0; i < m_cluster_colours.size(); i++) {
        auto c = m_cluster_colours[i];

        // Find best physical match using combobox tooltips (same as approximate match)
        std::vector<ColorDistValue> dists(map_count);
        for (int j = 0; j < map_count; j++) {
            auto tip_color = m_result_icon_list[0]->bitmap_combox->GetItemTooltip(j + 1);
            wxColour candidate(tip_color);
            dists[j].distance = calc_de(c, candidate);
            dists[j].id = j + 1;
        }
        std::sort(dists.begin(), dists.end(), [](ColorDistValue &a, ColorDistValue &b) {
            return a.distance < b.distance;
        });

        int new_index = dists[0].id;

        // Always attempt a blend. Use it whenever it's better than the nearest physical,
        // even by a small margin. Never fall back to adding a new physical filament slot.
        MixProposal prop;
        float       blend_de;
        wxColour    blended;
        if (find_best_blend(c, m_colours, prop, blend_de, blended)
            && blend_de < dists[0].distance)
        {
            // Deduplicate: reuse an existing proposal slot if close enough (±2%)
            int existing_slot = 0;
            for (auto &kv : m_mix_proposals_by_slot) {
                if (kv.second.component_a == prop.component_a &&
                    kv.second.component_b == prop.component_b &&
                    std::abs(kv.second.mix_b_percent - prop.mix_b_percent) <= 2) {
                    existing_slot = kv.first;
                    break;
                }
            }
            int slot = existing_slot > 0
                ? existing_slot
                : append_mixed_proposal_option(prop.component_a, prop.component_b,
                                               prop.mix_b_percent, blended);
            if (slot > 0)
                new_index = slot;
        }

        m_result_icon_list[i]->bitmap_combox->SetSelection(new_index);
        m_cluster_map_filaments[i] = new_index;
    }

    m_warning_text->SetLabelText(
        _L("Colors mapped using existing filaments and Mixed Filament blends only."));
    update_keep_color_buttons();
}

void ObjColorPanel::deal_approximate_match_btn()
{
    if (!m_new_add_colors.empty()) {
        deal_reset_btn();
    }

    auto calc_color_distance = [](wxColour c1, wxColour c2) {
        float lab[2][3];
        RGB2Lab(c1.Red(), c1.Green(), c1.Blue(), &lab[0][0], &lab[0][1], &lab[0][2]);
        RGB2Lab(c2.Red(), c2.Green(), c2.Blue(), &lab[1][0], &lab[1][1], &lab[1][2]);

        return DeltaE76(lab[0][0], lab[0][1], lab[0][2], lab[1][0], lab[1][1], lab[1][2]);
    };
    m_warning_text->SetLabelText("");
    if (m_result_icon_list.size() == 0) { return; }
    auto map_count = static_cast<int>(m_colours.size());
    if (map_count < 1) { return; }
    for (size_t i = 0; i < m_cluster_colours.size(); i++) {
        auto    c = m_cluster_colours[i];
        std::vector<ColorDistValue> color_dists;
        color_dists.resize(map_count);
        for (size_t j = 0; j < map_count; j++) {
            auto tip_color       = m_result_icon_list[0]->bitmap_combox->GetItemTooltip(j+1);
            wxColour candidate_c(tip_color);
            color_dists[j].distance = calc_color_distance(c, candidate_c);
            color_dists[j].id = j + 1;
        }
        std::sort(color_dists.begin(), color_dists.end(), [](ColorDistValue &a, ColorDistValue& b) {
            return a.distance < b.distance;
            });
        auto new_index = color_dists[0].id;

        // When the nearest physical filament is a poor match, try to find a better
        // approximation by blending two existing physical filaments.
        if (color_dists[0].distance > BLEND_MATCH_THRESHOLD) {
            MixProposal prop;
            float       blend_de;
            wxColour    blended;
            if (find_best_blend(c, m_colours, prop, blend_de, blended)
                && blend_de < color_dists[0].distance
                && blend_de < BLEND_ACCEPT_MAX_DE)
            {
                // Deduplicate: reuse existing proposal slot if close enough (±2%)
                int existing_slot = 0;
                for (auto &kv : m_mix_proposals_by_slot) {
                    auto &p = kv.second;
                    if (p.component_a == prop.component_a
                        && p.component_b == prop.component_b
                        && std::abs(p.mix_b_percent - prop.mix_b_percent) <= 2) {
                        existing_slot = kv.first;
                        break;
                    }
                }
                int slot = existing_slot > 0
                    ? existing_slot
                    : append_mixed_proposal_option(prop.component_a, prop.component_b,
                                                   prop.mix_b_percent, blended);
                if (slot > 0)
                    new_index = slot;
            }
        }

        m_result_icon_list[i]->bitmap_combox->SetSelection(new_index);
        m_cluster_map_filaments[i] = new_index;
    }
    update_keep_color_buttons();

    // When the checkbox is checked and blends were proposed, run the optimizer
    // to find physical filament colors that minimize total blend error.
    if (m_optimize_colors_checkbox && m_optimize_colors_checkbox->IsChecked()
        && !m_mix_proposals_by_slot.empty())
    {
        // Build locked vector: true = locked (checkbox unchecked = don't change)
        std::vector<bool> locked(m_colours.size(), false);
        for (size_t li = 0; li < m_filament_lock_checkboxes.size() && li < locked.size(); ++li)
            locked[li] = !m_filament_lock_checkboxes[li]->GetValue(); // unchecked = locked

        m_optimized_physical_colors = optimize_physical_colors(
            m_cluster_colours, m_cluster_map_filaments, m_mix_proposals_by_slot, m_colours, locked, 25);

        // Reset proposals and combobox entries, then re-run matching with optimized colors
        m_mix_proposals_by_slot.clear();
        for (auto *item : m_result_icon_list) {
            if (!item->bitmap_combox) continue;
            while (item->bitmap_combox->GetCount() > m_colours.size() + 1)
                item->bitmap_combox->DeleteOneItem(item->bitmap_combox->GetCount() - 1);
        }
        m_new_add_colors.clear();

        auto calc_de_opt = [](wxColour c1, wxColour c2) {
            float lab[2][3];
            RGB2Lab(c1.Red(), c1.Green(), c1.Blue(), &lab[0][0], &lab[0][1], &lab[0][2]);
            RGB2Lab(c2.Red(), c2.Green(), c2.Blue(), &lab[1][0], &lab[1][1], &lab[1][2]);
            return DeltaE76(lab[0][0], lab[0][1], lab[0][2], lab[1][0], lab[1][1], lab[1][2]);
        };
        const int map_count_opt = static_cast<int>(m_colours.size());
        for (size_t i = 0; i < m_cluster_colours.size(); ++i) {
            auto c = m_cluster_colours[i];
            std::vector<ColorDistValue> dists(map_count_opt);
            for (int j = 0; j < map_count_opt; ++j) {
                dists[j].distance = calc_de_opt(c, m_optimized_physical_colors[j]);
                dists[j].id = j + 1;
            }
            std::sort(dists.begin(), dists.end(), [](auto &a, auto &b) { return a.distance < b.distance; });
            int new_index_opt = dists[0].id;
            if (dists[0].distance > BLEND_MATCH_THRESHOLD) {
                MixProposal prop; float blend_de; wxColour blended;
                if (find_best_blend(c, m_optimized_physical_colors, prop, blend_de, blended)
                    && blend_de < dists[0].distance && blend_de < BLEND_ACCEPT_MAX_DE)
                {
                    int existing_slot = 0;
                    for (auto &kv : m_mix_proposals_by_slot) {
                        if (kv.second.component_a == prop.component_a &&
                            kv.second.component_b == prop.component_b &&
                            std::abs(kv.second.mix_b_percent - prop.mix_b_percent) <= 2) {
                            existing_slot = kv.first; break;
                        }
                    }
                    int slot = existing_slot > 0
                        ? existing_slot
                        : append_mixed_proposal_option(prop.component_a, prop.component_b,
                                                       prop.mix_b_percent, blended);
                    if (slot > 0) new_index_opt = slot;
                }
            }
            m_result_icon_list[i]->bitmap_combox->SetSelection(new_index_opt);
            m_cluster_map_filaments[i] = new_index_opt;
        }

        // Preview: update extruder icon buttons to show optimized colors
        for (size_t i = 0; i < m_optimized_physical_colors.size() && i < m_extruder_icon_list.size(); ++i) {
            auto bmp = *get_extruder_color_icon(
                m_optimized_physical_colors[i].GetAsString(wxC2S_HTML_SYNTAX).ToStdString(),
                std::to_string(i + 1), m_combox_icon_width, m_combox_icon_height);
            m_extruder_icon_list[i]->SetBitmap(bmp);
        }
        m_warning_text->SetLabelText(_L("Note: Filament colors will be updated to the previewed colors on OK."));
        update_keep_color_buttons();
    }
}

bool ObjColorPanel::colors_are_equal(const wxColour &lhs, const wxColour &rhs)
{
    return lhs.Red() == rhs.Red() && lhs.Green() == rhs.Green() && lhs.Blue() == rhs.Blue() && lhs.Alpha() == rhs.Alpha();
}

bool ObjColorPanel::find_best_blend(const wxColour &target,
                                     const std::vector<wxColour> &physicals,
                                     MixProposal &out, float &out_de, wxColour &out_blended)
{
    if (physicals.size() < 2) return false;

    float tL, ta, tb;
    RGB2Lab(target.Red(), target.Green(), target.Blue(), &tL, &ta, &tb);

    float best_de = std::numeric_limits<float>::max();
    int   best_i  = -1, best_j = -1;
    float best_t  = 0.5f;

    for (int i = 0; i < (int)physicals.size(); ++i) {
        float L1, a1, b1;
        RGB2Lab(physicals[i].Red(), physicals[i].Green(), physicals[i].Blue(), &L1, &a1, &b1);

        for (int j = i + 1; j < (int)physicals.size(); ++j) {
            float L2, a2, b2;
            RGB2Lab(physicals[j].Red(), physicals[j].Green(), physicals[j].Blue(), &L2, &a2, &b2);

            // Closed-form: project target onto line segment c1..c2 in Lab space
            float dL = L2 - L1, da = a2 - a1, db = b2 - b1;
            float len2 = dL*dL + da*da + db*db;
            float t = 0.5f;
            if (len2 > 1e-6f) {
                t = ((tL - L1)*dL + (ta - a1)*da + (tb - b1)*db) / len2;
                t = std::max(0.0f, std::min(1.0f, t));
            }

            float bL = L1 + t*(L2 - L1);
            float ba = a1 + t*(a2 - a1);
            float bb_ = b1 + t*(b2 - b1);
            float de = DeltaE76(tL, ta, tb, bL, ba, bb_);

            if (de < best_de) {
                best_de = de;
                best_i  = i;
                best_j  = j;
                best_t  = t;
            }
        }
    }

    if (best_i < 0) return false;

    out_de            = best_de;
    out.component_a   = static_cast<unsigned int>(best_i + 1);
    out.component_b   = static_cast<unsigned int>(best_j + 1);
    out.mix_b_percent = static_cast<int>(std::round(best_t * 100.0f));

    // Compute accurate blended color via FilamentMixer (same algorithm as the sidebar preview)
    std::string hex_a = physicals[best_i].GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    std::string hex_b = physicals[best_j].GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    int pct_b         = out.mix_b_percent;
    std::string blended_hex = Slic3r::MixedFilamentManager::blend_color(hex_a, hex_b,
                                                                          100 - pct_b, pct_b);
    if (blended_hex.size() >= 7)
        out_blended = wxColour(blended_hex.substr(0, 7));
    else
        out_blended = wxColour(
            static_cast<int>(physicals[best_i].Red()   * (1.0f - best_t) + physicals[best_j].Red()   * best_t),
            static_cast<int>(physicals[best_i].Green() * (1.0f - best_t) + physicals[best_j].Green() * best_t),
            static_cast<int>(physicals[best_i].Blue()  * (1.0f - best_t) + physicals[best_j].Blue()  * best_t));
    return true;
}

int ObjColorPanel::append_mixed_proposal_option(unsigned int a, unsigned int b, int pct,
                                                  const wxColour &blended)
{
    int slot = append_new_filament_option(blended);
    if (slot > 0)
        m_mix_proposals_by_slot.emplace(slot, MixProposal{a, b, pct});
    return slot;
}

std::vector<wxColour> ObjColorPanel::optimize_physical_colors(
    const std::vector<wxColour>      &cluster_targets,
    const std::vector<int>           &cluster_to_slot,
    const std::map<int, MixProposal> &proposals,
    const std::vector<wxColour>      &physicals,
    const std::vector<bool>          &locked,
    int max_iter)
{
    const int N = static_cast<int>(physicals.size());
    const int K = static_cast<int>(cluster_targets.size());

    // Convert to Lab
    std::vector<std::array<float,3>> P(N), T(K);
    for (int i = 0; i < N; ++i)
        RGB2Lab(physicals[i].Red(), physicals[i].Green(), physicals[i].Blue(),
                &P[i][0], &P[i][1], &P[i][2]);
    for (int k = 0; k < K; ++k)
        RGB2Lab(cluster_targets[k].Red(), cluster_targets[k].Green(), cluster_targets[k].Blue(),
                &T[k][0], &T[k][1], &T[k][2]);

    // Map each cluster to its blend assignment (0-based filament indices)
    struct BlendAsgn { int i, j; float t; };
    std::vector<BlendAsgn> asgn(K, {-1, -1, 0.5f});
    for (int k = 0; k < K; ++k) {
        if (k >= static_cast<int>(cluster_to_slot.size())) continue;
        auto it = proposals.find(cluster_to_slot[k]);
        if (it != proposals.end()) {
            asgn[k] = { static_cast<int>(it->second.component_a) - 1,
                        static_cast<int>(it->second.component_b) - 1,
                        it->second.mix_b_percent / 100.f };
        }
    }

    for (int iter = 0; iter < max_iter; ++iter) {
        // Step 1: re-estimate blend ratios via closed-form projection
        for (int k = 0; k < K; ++k) {
            if (asgn[k].i < 0) continue;
            int i = asgn[k].i, j = asgn[k].j;
            std::array<float,3> d = {P[j][0]-P[i][0], P[j][1]-P[i][1], P[j][2]-P[i][2]};
            float len2 = d[0]*d[0] + d[1]*d[1] + d[2]*d[2];
            if (len2 < 1e-6f) continue;
            float dot = (T[k][0]-P[i][0])*d[0] + (T[k][1]-P[i][1])*d[1] + (T[k][2]-P[i][2])*d[2];
            asgn[k].t = std::max(0.f, std::min(1.f, dot / len2));
        }
        // Step 2: coordinate descent — update each physical filament
        auto P_new = P;
        for (int f = 0; f < N; ++f) {
            // Skip filaments the user has locked (checkbox unchecked)
            if (f < static_cast<int>(locked.size()) && locked[f]) continue;

            std::array<float,3> sum_ab = {0.f, 0.f, 0.f};
            float sum_a2 = 0.f;
            for (int k = 0; k < K; ++k) {
                if (asgn[k].i < 0) continue;
                int i = asgn[k].i, j = asgn[k].j;
                float alpha = (f == i) ? (1.f - asgn[k].t) : (f == j ? asgn[k].t : 0.f);
                if (alpha < 0.05f) continue;
                int other = (f == i) ? j : i;
                for (int c = 0; c < 3; ++c)
                    sum_ab[c] += alpha * (T[k][c] - (1.f - alpha) * P[other][c]);
                sum_a2 += alpha * alpha;
            }
            if (sum_a2 < 1e-6f) continue; // filament not in any blend — leave unchanged
            for (int c = 0; c < 3; ++c) P_new[f][c] = sum_ab[c] / sum_a2;
            // Gamut enforcement: Lab -> clamp to RGB cube -> Lab
            wxColour clamped = lab_to_wxcolour(P_new[f][0], P_new[f][1], P_new[f][2]);
            RGB2Lab(clamped.Red(), clamped.Green(), clamped.Blue(),
                    &P_new[f][0], &P_new[f][1], &P_new[f][2]);
        }
        P = P_new;
    }

    std::vector<wxColour> result(N);
    for (int i = 0; i < N; ++i)
        result[i] = lab_to_wxcolour(P[i][0], P[i][1], P[i][2]);
    return result;
}

int ObjColorPanel::find_filament_selection_by_color(const wxColour &color) const
{
    for (size_t i = 0; i < m_colours.size(); ++i) {
        if (colors_are_equal(m_colours[i], color)) {
            return static_cast<int>(i + 1);
        }
    }

    for (size_t i = 0; i < m_new_add_colors.size(); ++i) {
        if (colors_are_equal(m_new_add_colors[i], color)) {
            return static_cast<int>(m_colours.size() + i + 1);
        }
    }

    return 0;
}

int ObjColorPanel::append_new_filament_option(const wxColour &color)
{
    if (m_colours.size() + m_new_add_colors.size() >= g_max_color) {
        return 0;
    }

    m_new_add_colors.emplace_back(color);
    const int selection = static_cast<int>(m_colours.size() + m_new_add_colors.size());
    auto *    bitmap    = get_extruder_color_icon(color.GetAsString(wxC2S_HTML_SYNTAX).ToStdString(), std::to_string(selection), m_combox_icon_width, m_combox_icon_height);

    for (auto *item : m_result_icon_list) {
        if (item->bitmap_combox == nullptr) {
            continue;
        }

        item->bitmap_combox->Append(wxString::Format("%d", item->bitmap_combox->GetCount()), *bitmap);
        item->bitmap_combox->SetItemTooltip(item->bitmap_combox->GetCount() - 1, color.GetAsString(wxC2S_HTML_SYNTAX));
    }

    return selection;
}

void ObjColorPanel::update_keep_color_buttons()
{
    for (size_t i = 0; i < m_result_icon_list.size(); ++i) {
        auto *item = m_result_icon_list[i];
        if (item->keep_color_btn == nullptr) {
            continue;
        }

        const bool has_cluster_color = i < m_cluster_colours.size();
        const bool show_keep_color   = has_cluster_color && find_filament_selection_by_color(m_cluster_colours[i]) == 0;
        item->keep_color_btn->Show(show_keep_color);
        item->keep_color_btn->Enable(show_keep_color);
    }

    if (m_scrolledWindow != nullptr) {
        m_scrolledWindow->Layout();
    }
    Layout();
}

void ObjColorPanel::deal_keep_color_btn(int id)
{
    if (id < 0 || id >= static_cast<int>(m_cluster_colours.size())) {
        return;
    }

    int selection = find_filament_selection_by_color(m_cluster_colours[id]);
    if (selection == 0) {
        selection = append_new_filament_option(m_cluster_colours[id]);
    }

    if (selection == 0) {
        m_warning_text->SetLabelText(_L("Warning: The count of newly added and \ncurrent extruders exceeds 16."));
        return;
    }

    m_result_icon_list[id]->bitmap_combox->SetSelection(selection);
    m_cluster_map_filaments[id] = selection;
    m_warning_text->SetLabelText(_L("Note: The color has been selected, you can choose OK \nto continue or manually adjust it."));
    update_keep_color_buttons();
}

void ObjColorPanel::show_sizer(wxSizer *sizer, bool show)
{
    wxSizerItemList items = sizer->GetChildren();
    for (wxSizerItemList::iterator it = items.begin(); it != items.end(); ++it) {
        wxSizerItem *item   = *it;
        if (wxWindow *window = item->GetWindow()) {
            window->Show(show);
        }
        if (wxSizer *son_sizer = item->GetSizer()) {
            show_sizer(son_sizer, show);
        }
    }
}

void ObjColorPanel::redraw_part_table() {
    //show all and set -1
    deal_reset_btn();
    for (size_t i = 0; i < m_row_sizer_list.size(); i++) {
        show_sizer(m_row_sizer_list[i], true);
    }
    if (m_cluster_colours.size() < m_row_sizer_list.size()) { // show part
        for (size_t i = m_cluster_colours.size(); i < m_row_sizer_list.size(); i++) {
            show_sizer(m_row_sizer_list[i], false);
            //m_row_panel_list[i]->Show(false); // show_sizer(m_left_color_cluster_boxsizer_list[i],false);
           // m_result_icon_list[i]->bitmap_combox->Show(false);
        }
    } else if (m_cluster_colours.size() > m_row_sizer_list.size()) {
        for (size_t i = m_row_sizer_list.size(); i < m_cluster_colours.size(); i++) {
            int      id                       = i;
            wxPanel *row_panel = new wxPanel(m_scrolledWindow);
            row_panel->SetBackgroundColour((i+1) % 2 == 0 ? *wxWHITE : wxColour(238, 238, 238));
            auto row_sizer = new wxBoxSizer(wxHORIZONTAL);
            row_panel->SetSizer(row_sizer);

            row_panel->SetMinSize(wxSize(FromDIP(PANEL_WIDTH), -1));
            row_panel->SetMaxSize(wxSize(FromDIP(PANEL_WIDTH), -1));

            auto cluster_color_icon_sizer = create_color_icon_and_rgba_sizer(row_panel, id, m_cluster_colours[id]);
            row_sizer->Add(cluster_color_icon_sizer, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
            row_sizer->AddStretchSpacer();
            row_sizer->Add(create_result_button_sizer(row_panel, id), 0, wxALIGN_CENTER_VERTICAL, 0);

            m_row_sizer_list.emplace_back(row_sizer);
            m_gridsizer->Add(row_panel, 0, wxALIGN_LEFT | wxALL, FromDIP(HEADER_BORDER));
        }
        m_gridsizer->Layout();
    }
    for (size_t i = 0; i < m_cluster_colours.size(); i++) { // update data
        // m_color_cluster_icon_list//m_color_cluster_text_list
        update_color_icon_and_rgba_sizer(i, m_cluster_colours[i]);
    }
    update_keep_color_buttons();
    m_scrolledWindow->Refresh();
}

void ObjColorPanel::draw_table()
{
    auto row                = std::max(m_cluster_colours.size(), m_colours.size()) + 1;
    m_gridsizer             = new wxGridSizer(row, 1, 1, 3); //(int rows, int cols, int vgap, int hgap );

    m_color_cluster_icon_list.clear();
    m_extruder_icon_list.clear();
    float row_height = 0;
    for (size_t ii = 0; ii < row; ii++) {
        wxPanel *row_panel = new wxPanel(m_scrolledWindow);
        row_panel->SetBackgroundColour(ii % 2 == 0 ? *wxWHITE : wxColour(238, 238, 238));
        auto row_sizer = new wxBoxSizer(wxHORIZONTAL);
        row_panel->SetSizer(row_sizer);

        row_panel->SetMinSize(wxSize(FromDIP(PANEL_WIDTH), -1));
        row_panel->SetMaxSize(wxSize(FromDIP(PANEL_WIDTH), -1));
        if (ii == 0) {
            wxStaticText *colors_left_title = new wxStaticText(row_panel, wxID_ANY, _L("Cluster colors"));
            colors_left_title->SetFont(Label::Head_14);
            row_sizer->Add(colors_left_title, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(40));
            row_sizer->AddStretchSpacer();

            wxStaticText *colors_middle_title = new wxStaticText(row_panel, wxID_ANY, _L("Map Filament"));
            colors_middle_title->SetFont(Label::Head_14);
            row_sizer->Add(colors_middle_title, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(HEADER_BORDER));
        } else {
            int id = ii - 1;
            if (id < m_cluster_colours.size()) {
                auto cluster_color_icon_sizer = create_color_icon_and_rgba_sizer(row_panel, id, m_cluster_colours[id]);
                row_sizer->Add(cluster_color_icon_sizer, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
                row_sizer->AddStretchSpacer();
                row_sizer->Add(create_result_button_sizer(row_panel, id), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(CONTENT_BORDER));
            }
        }
        row_height = row_panel->GetSize().GetHeight();
        if (ii>=1) {
            m_row_sizer_list.emplace_back(row_sizer);
        }
        m_gridsizer->Add(row_panel, 0, wxALIGN_LEFT | wxALL, FromDIP(HEADER_BORDER));
    }
    m_scrolledWindow->SetSizer(m_gridsizer);
    int totalHeight = row_height *(row+1) * 2;
    m_scrolledWindow->SetVirtualSize(MIN_OBJCOLOR_DIALOG_WIDTH, totalHeight);
    auto look = FIX_SCROLL_HEIGTH;
    if (totalHeight > FIX_SCROLL_HEIGTH) {
        m_scrolledWindow->SetMinSize(wxSize(MIN_OBJCOLOR_DIALOG_WIDTH, FIX_SCROLL_HEIGTH));
        m_scrolledWindow->SetMaxSize(wxSize(MIN_OBJCOLOR_DIALOG_WIDTH, FIX_SCROLL_HEIGTH));
    }
    else {
        m_scrolledWindow->SetMinSize(wxSize(MIN_OBJCOLOR_DIALOG_WIDTH, totalHeight));
    }
    m_scrolledWindow->EnableScrolling(false, true);
    m_scrolledWindow->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_DEFAULT);//wxSHOW_SB_ALWAYS
    m_scrolledWindow->SetScrollRate(20, 20);
    update_keep_color_buttons();
}

void ObjColorPanel::deal_algo(char cluster_number, bool redraw_ui)
{
    if (m_last_cluster_number == cluster_number) {
        return;
    }
    m_last_cluster_number = cluster_number;
    QuantKMeans quant(10);
    quant.apply(m_input_colors, m_cluster_colors_from_algo, m_cluster_labels_from_algo, (int)cluster_number);
    m_cluster_colours.clear();
    m_cluster_colours.reserve(m_cluster_colors_from_algo.size());
    for (size_t i = 0; i < m_cluster_colors_from_algo.size(); i++) {
        m_cluster_colours.emplace_back(convert_to_wxColour(m_cluster_colors_from_algo[i]));
    }
    if (m_cluster_colours.size() == 0) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ",m_cluster_colours.size() = 0\n";
        return;
    }
    m_cluster_map_filaments.resize(m_cluster_colors_from_algo.size());
    m_color_cluster_num_by_algo = m_cluster_colors_from_algo.size();
    if (cluster_number == -1) {
        m_color_num_recommend = m_color_cluster_num_by_algo;
    }
    //redraw ui
    if (redraw_ui) {
        redraw_part_table();
        deal_default_strategy();
    }
}

void ObjColorPanel::deal_default_strategy()
{
    if (m_colours.empty()) {
        deal_add_btn();
        return;
    }

    deal_approximate_match_btn();
    m_warning_text->SetLabelText(_L("Note: The color has been selected, you can choose OK \nto continue or manually adjust it."));
}

void ObjColorPanel::deal_add_btn()
{
    if (m_colours.size() >= g_max_color) { return; }
    deal_reset_btn();
    bool is_exceed = false;
    std::vector<int> appended_selections;
    appended_selections.reserve(m_cluster_colors_from_algo.size());
    for (size_t i = 0; i < m_cluster_colors_from_algo.size(); i++) {
        const wxColour cur_color  = convert_to_wxColour(m_cluster_colors_from_algo[i]);
        const int      selection  = append_new_filament_option(cur_color);
        if (selection == 0) {
            is_exceed = true;
            break;
        }
        appended_selections.emplace_back(selection);
    }
    if (is_exceed) {
        deal_approximate_match_btn();
        m_warning_text->SetLabelText(_L("Warning: The count of newly added and \ncurrent extruders exceeds 16."));
        return;
    }

    for (size_t i = 0; i < m_cluster_colours.size() && i < appended_selections.size(); i++) {
        m_result_icon_list[i]->bitmap_combox->SetSelection(appended_selections[i]);
        m_cluster_map_filaments[i] = appended_selections[i];
    }

    update_keep_color_buttons();
}

void ObjColorPanel::deal_reset_btn()
{
    for (size_t i = 0; i < m_result_icon_list.size(); ++i) {
        auto *item = m_result_icon_list[i];
        // delete redundant bitmap
        while (item->bitmap_combox->GetCount() > m_colours.size()+ 1) {
            item->bitmap_combox->DeleteOneItem(item->bitmap_combox->GetCount() - 1);
        }
        item->bitmap_combox->SetSelection(0);
        if (i < m_cluster_map_filaments.size()) {
            m_cluster_map_filaments[i] = 0;
        }
    }
    m_new_add_colors.clear();
    m_mix_proposals_by_slot.clear();
    if (!m_optimized_physical_colors.empty()) {
        m_optimized_physical_colors.clear();
        for (size_t i = 0; i < m_colours.size() && i < m_extruder_icon_list.size(); ++i) {
            auto bmp = *get_extruder_color_icon(
                m_colours[i].GetAsString(wxC2S_HTML_SYNTAX).ToStdString(),
                std::to_string(i + 1), m_combox_icon_width, m_combox_icon_height);
            m_extruder_icon_list[i]->SetBitmap(bmp);
        }
    }
    m_warning_text->SetLabelText("");
    update_keep_color_buttons();
}

wxBoxSizer *ObjColorPanel::create_result_button_sizer(wxWindow *parent, int id)
{
    for (size_t i = m_result_icon_list.size(); i < id + 1; i++) {
        m_result_icon_list.emplace_back(new ButtonState());
    }

    m_result_icon_list[id]->bitmap_combox = CreateEditorCtrl(parent,id);
    auto *result_sizer = new wxBoxSizer(wxHORIZONTAL);
    result_sizer->Add(m_result_icon_list[id]->bitmap_combox, 0, wxALIGN_CENTER_VERTICAL, 0);

    StateColor calc_btn_bg(std::pair<wxColour, int>(wxColour(0, 137, 123), StateColor::Pressed), std::pair<wxColour, int>(wxColour(38, 166, 154), StateColor::Hovered),
                           std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_bd(std::pair<wxColour, int>(wxColour(0, 150, 136), StateColor::Normal));
    StateColor calc_btn_text(std::pair<wxColour, int>(wxColour(255, 255, 254), StateColor::Normal));

    auto *keep_color_btn = new Button(parent, _L("Keep color"));
    keep_color_btn->SetToolTip(_L("Add this cluster color as a new filament."));
    keep_color_btn->SetFont(Label::Body_13);
    keep_color_btn->SetMinSize(wxSize(FromDIP(88), FromDIP(24)));
    keep_color_btn->SetCornerRadius(FromDIP(12));
    keep_color_btn->SetBackgroundColor(calc_btn_bg);
    keep_color_btn->SetBorderColor(calc_btn_bd);
    keep_color_btn->SetTextColor(calc_btn_text);
    keep_color_btn->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) {
        deal_keep_color_btn(id);
    });
    m_result_icon_list[id]->keep_color_btn = keep_color_btn;

    result_sizer->AddSpacer(FromDIP(8));
    result_sizer->Add(keep_color_btn, 0, wxALIGN_CENTER_VERTICAL, 0);
    return result_sizer;
}

wxBoxSizer *ObjColorPanel::create_color_icon_and_rgba_sizer(wxWindow *parent, int id, const wxColour& color)
{
    auto      icon_sizer = new wxBoxSizer(wxHORIZONTAL);
    icon_sizer->AddSpacer(FromDIP(40));
    wxButton *icon       = new wxButton(parent, wxID_ANY, {}, wxDefaultPosition, ICON_SIZE, wxBORDER_NONE | wxBU_AUTODRAW);
    icon->SetBitmap(*get_extruder_color_icon(color.GetAsString(wxC2S_HTML_SYNTAX).ToStdString(), std::to_string(id + 1), FromDIP(16), FromDIP(16)));
    icon->SetCanFocus(false);
    m_color_cluster_icon_list.emplace_back(icon);
    icon_sizer->Add(icon, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0); // wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM
    icon_sizer->AddSpacer(FromDIP(10));

    std::string   message    = get_color_str(color);
    wxStaticText *rgba_title = new wxStaticText(parent, wxID_ANY, message.c_str());
    m_color_cluster_text_list.emplace_back(rgba_title);
    rgba_title->SetMinSize(wxSize(FromDIP(COLOR_LABEL_WIDTH), -1));
    rgba_title->SetMaxSize(wxSize(FromDIP(COLOR_LABEL_WIDTH), -1));
    //rgba_title->SetFont(Label::Head_12);
    icon_sizer->Add(rgba_title, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);
    return icon_sizer;
}

void ObjColorPanel::update_color_icon_and_rgba_sizer(int id, const wxColour &color)
{
    if (id < m_color_cluster_text_list.size()) {
        auto icon = m_color_cluster_icon_list[id];
        icon->SetBitmap(*get_extruder_color_icon(color.GetAsString(wxC2S_HTML_SYNTAX).ToStdString(), std::to_string(id + 1), FromDIP(16), FromDIP(16)));
        std::string message = get_color_str(color);
        m_color_cluster_text_list[id]->SetLabelText(message.c_str());
    }
}
