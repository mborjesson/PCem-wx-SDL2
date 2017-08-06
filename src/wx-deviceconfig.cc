#include "wx-deviceconfig.h"

#include "wx-utils.h"
#include <wx/wxprec.h>

#include "wx-dialogbox.h"

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include <wx/spinctrl.h>

extern "C"
{
#include "config.h"
#include "plat-midi.h"
void saveconfig(char*);
void resetpchard();
int deviceconfig_dlgproc(void* hdlg, int message, INT_PARAM wParam,
                LONG_PARAM lParam);
device_t *config_device;
int confirm();
}
#define IDC_CONFIG_BASE 1000

int deviceconfig_dlgproc(void* hdlg, int message, INT_PARAM wParam,
                LONG_PARAM lParam)
{
        switch (message)
        {
        case WX_INITDIALOG:
        {
                int id = IDC_CONFIG_BASE;
                device_config_t *config = config_device->config;
                int c;

                while (config->type != -1)
                {
                        device_config_selection_t *selection = config->selection;
                        void* h = 0;
                        int val_int;
                        int num;
                        char s[100];

                        switch (config->type)
                        {
                        case CONFIG_BINARY:
                                h = wx_getdlgitem(hdlg, id);
                                val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                wx_sendmessage(h, WX_BM_SETCHECK, val_int, 0);

                                id++;
                                break;

                        case CONFIG_SELECTION:
                                h = wx_getdlgitem(hdlg, id);
                                val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                c = 0;
                                while (selection->description[0])
                                {
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) selection->description);
                                        if (val_int == selection->value)
                                                wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                                        selection++;
                                        c++;
                                }

                                id++;
                                break;

                        case CONFIG_MIDI:
                                h = wx_getdlgitem(hdlg, id);
                                val_int = config_get_int(CFG_MACHINE, NULL, config->name, config->default_int);

                                num = plat_midi_get_num_devs();
                                for (c = 0; c < num; c++)
                                {
                                        plat_midi_get_dev_name(c, s);
                                        wx_sendmessage(h, WX_CB_ADDSTRING, 0, (LONG_PARAM) s);
                                        if (val_int == c)
                                                wx_sendmessage(h, WX_CB_SETCURSEL, c, 0);
                                }

                                id++;
                                break;

                        case CONFIG_FILE:
                        {
                                h = wx_getdlgitem(hdlg, id);
                                char* str = config_get_string(CFG_MACHINE, config_device->name, config->name, 0);
                                if (str)
                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)str);

                                id += 2;
                                break;
                        }
                        case CONFIG_SPINNER:
                        {
                                h = wx_getdlgitem(hdlg, id);
                                val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                wx_sendmessage(h, WX_UDM_SETPOS, 0, val_int);

                                id++;
                                break;
                        }
                        }
                        config++;
                }
        }
                return TRUE;

        case WX_COMMAND:
        {
                if (wParam == wxID_OK)
                {
                        int id = IDC_CONFIG_BASE;
                        device_config_t *config = config_device->config;
                        int c;
                        int changed = 0;
                        char s[256];

                        while (config->type != -1)
                        {
                                device_config_selection_t *selection = config->selection;
                                void* h = 0;
                                int val_int;

                                switch (config->type)
                                {
                                case CONFIG_BINARY:
                                        h = wx_getdlgitem(hdlg, id);
                                        val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                        if (val_int != wx_sendmessage(h, WX_BM_GETCHECK, 0, 0))
                                                changed = 1;

                                        id++;
                                        break;

                                case CONFIG_SELECTION:
                                        h = wx_getdlgitem(hdlg, id);
                                        val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                        for (; c > 0; c--)
                                                selection++;

                                        if (val_int != selection->value)
                                                changed = 1;

                                        id++;
                                        break;

                                case CONFIG_MIDI:
                                        h = wx_getdlgitem(hdlg, id);
                                        val_int = config_get_int(CFG_MACHINE, NULL, config->name, config->default_int);

                                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);

                                        if (val_int != c)
                                                changed = 1;

                                        id++;
                                        break;

                                case CONFIG_FILE:
                                {
                                        h = wx_getdlgitem(hdlg, id);
                                        char* str = config_get_string(CFG_MACHINE, config_device->name, config->name, (char*)"");
                                        wx_sendmessage(h, WX_WM_GETTEXT, 0, (LONG_PARAM)s);
                                        if (strcmp(str, s))
                                                changed = 1;

                                        id += 2;
                                        break;
                                }
                                case CONFIG_SPINNER:
                                {
                                        h = wx_getdlgitem(hdlg, id);
                                        val_int = config_get_int(CFG_MACHINE, config_device->name, config->name, config->default_int);

                                        c = wx_sendmessage(h, WX_UDM_GETPOS, 0, 0);

                                        if (val_int != c)
                                                changed = 1;

                                        id++;
                                        break;
                                }
                                }

                                config++;
                        }

                        if (!changed)
                        {
                                wx_enddialog(hdlg, 0);
                                return TRUE;
                        }

                        if (has_been_inited && !confirm())
                        {
                                wx_enddialog(hdlg, 0);
                                return TRUE;
                        }

                        id = IDC_CONFIG_BASE;
                        config = config_device->config;

                        while (config->type != -1)
                        {
                                device_config_selection_t *selection = config->selection;
                                void* h = 0;

                                switch (config->type)
                                {
                                case CONFIG_BINARY:
                                        h = wx_getdlgitem(hdlg, id);
                                        config_set_int(CFG_MACHINE, config_device->name, config->name, wx_sendmessage(h, WX_BM_GETCHECK, 0, 0));

                                        id++;
                                        break;

                                case CONFIG_SELECTION:
                                        h = wx_getdlgitem(hdlg, id);
                                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                        for (; c > 0; c--)
                                                selection++;
                                        config_set_int(CFG_MACHINE, config_device->name, config->name, selection->value);

                                        id++;
                                        break;

                                case CONFIG_MIDI:
                                        h = wx_getdlgitem(hdlg, id);
                                        c = wx_sendmessage(h, WX_CB_GETCURSEL, 0, 0);
                                        config_set_int(CFG_MACHINE, NULL, config->name, c);

                                        id++;
                                        break;

                                case CONFIG_FILE:
                                {
                                        h = wx_getdlgitem(hdlg, id);
                                        wx_sendmessage(h, WX_WM_GETTEXT, 0, (LONG_PARAM)s);
                                        config_set_string(CFG_MACHINE, config_device->name, config->name, s);

                                        id += 2;
                                        break;
                                }
                                case CONFIG_SPINNER:
                                {
                                        h = wx_getdlgitem(hdlg, id);
                                        c = wx_sendmessage(h, WX_UDM_GETPOS, 0, 0);
                                        config_set_int(CFG_MACHINE, config_device->name, config->name, c);

                                        id++;
                                        break;
                                }
                                }
                                config++;
                        }

                        if (has_been_inited)
                        {
                                saveconfig(NULL);
                                resetpchard();
                        }

                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                else if (wParam == wxID_CANCEL)
                {
                        wx_enddialog(hdlg, 0);
                        return TRUE;
                }
                else
                {
                        int id = IDC_CONFIG_BASE;
                        device_config_t *config = config_device->config;
                        int c, d;
                        char s[256];
                        char file_filter[512];

                        while (config->type != -1)
                        {
                                void* h = 0;

                                switch (config->type)
                                {
                                case CONFIG_BINARY:
                                case CONFIG_SELECTION:
                                case CONFIG_SPINNER:
                                case CONFIG_MIDI:
                                        id++;
                                        break;

                                case CONFIG_FILE:
                                        if (wParam == id+1)
                                        {
                                                h = wx_getdlgitem(hdlg, id);
                                                wx_sendmessage(h, WX_WM_GETTEXT, 0, (LONG_PARAM)s);
                                                file_filter[0] = 0;

                                                c = 0;
                                                while (config->file_filter[c].description[0])
                                                {
                                                        if (c > 0)
                                                                strcat(file_filter, "|");
                                                        strcat(file_filter, config->file_filter[c].description);
                                                        strcat(file_filter, " (");
                                                        d = 0;
                                                        while (config->file_filter[c].extensions[d][0])
                                                        {
                                                                if (d > 0)
                                                                        strcat(file_filter, ";");
                                                                strcat(file_filter, "*.");
                                                                strcat(file_filter, config->file_filter[c].extensions[d]);
                                                                d++;
                                                        }
                                                        strcat(file_filter, ")|");
                                                        d = 0;
                                                        while (config->file_filter[c].extensions[d][0])
                                                        {
                                                                if (d > 0)
                                                                        strcat(file_filter, ";");
                                                                strcat(file_filter, "*.");
                                                                strcat(file_filter, config->file_filter[c].extensions[d]);
                                                                d++;
                                                        }
                                                        c++;
                                                }
                                                strcat(file_filter, "|All files (*.*)|*.*");

                                                int ret = !wx_filedialog(hdlg, "Open", s, file_filter, 0, 1, s);

                                                if (ret)
                                                        wx_sendmessage(h, WX_WM_SETTEXT, 0, (LONG_PARAM)s);
                                        }
                                        id += 2;
                                        break;
                                }
                                config++;
                        }

                }
                break;
        }
        }
        return FALSE;
}

void deviceconfig_open(void* hwnd, device_t *device)
{
        char s[257];
        config_device = device;

        PCemDialogBox dialog((wxWindow*) hwnd, deviceconfig_dlgproc);
//	dialog.SetWindowStyle(wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

        device_config_t *config = device->config;

        dialog.SetTitle("Device Configuration");

        wxFlexGridSizer* root = new wxFlexGridSizer(0, 1, 0, 0);
        root->SetFlexibleDirection(wxBOTH);
        root->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        dialog.SetSizer(root);

        wxFlexGridSizer* sizer = new wxFlexGridSizer(0, 2, 0, 0);
        sizer->SetFlexibleDirection(wxBOTH);
        sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
        sizer->AddGrowableCol(1);
        root->Add(sizer, 1, wxEXPAND, 5);

        int id = IDC_CONFIG_BASE;

        while (config->type != -1)
        {
                switch (config->type)
                {
                case CONFIG_BINARY:
                        sizer->Add(0, 0, 1, wxEXPAND, 5);
                        sizer->Add(new wxCheckBox(&dialog, id++, config->description), 0, wxALL, 5);
                        break;

                case CONFIG_SELECTION:
                case CONFIG_MIDI:
                {
                        sprintf(s, "%s:", config->description);
                        sizer->Add(new wxStaticText(&dialog, wxID_ANY, s), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
                        wxBoxSizer* comboSizer = new wxBoxSizer(wxHORIZONTAL);
                        sizer->Add(comboSizer, 1, wxEXPAND, 5);
                        wxComboBox* cb = new wxComboBox(&dialog, id++);
                        cb->SetEditable(false);
                        comboSizer->Add(cb, 1, wxALL, 5);
                        break;
                }
                case CONFIG_FILE:
                {
                        sprintf(s, "%s:", config->description);
                        sizer->Add(new wxStaticText(&dialog, wxID_ANY, s), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

                        wxBoxSizer* sz = new wxBoxSizer(wxHORIZONTAL);
                        sizer->Add(sz, 1, wxEXPAND, 5);

                        wxTextCtrl* tc = new wxTextCtrl(&dialog, id++);
                        tc->SetEditable(false);
                        sz->Add(tc, 1, wxALL, 5);

                        wxBitmapButton* button = new wxBitmapButton(&dialog, id++, wxXmlResource::Get()->LoadBitmap("BITMAP_FOLDER"));
                        sz->Add(button, 0, wxALL, 5);

                        break;
                }
                case CONFIG_SPINNER:
                {
                        sprintf(s, "%s:", config->description);
                        sizer->Add(new wxStaticText(&dialog, wxID_ANY, s), 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

                        wxBoxSizer* sz = new wxBoxSizer(wxHORIZONTAL);
                        sizer->Add(sz, 1, wxEXPAND, 5);

                        wxSpinCtrlDouble* spinner = new wxSpinCtrlDouble(&dialog, id++);
                        spinner->SetRange(config->spinner.min, config->spinner.max);
                        spinner->SetDigits(0);
                        spinner->SetIncrement(config->spinner.step > 0 ? config->spinner.step : 1);
                        spinner->SetValue(config->default_int);
                        sz->Add(spinner, 1, wxALL, 5);

                        break;
                }
                }

                config++;
        }

        wxBoxSizer* okCancelSizer = new wxBoxSizer(wxHORIZONTAL);
        root->Add(okCancelSizer, 1, wxEXPAND, 5);

        okCancelSizer->Add(0, 0, 1, wxEXPAND, 5);
        okCancelSizer->Add(new wxButton(&dialog, wxID_OK), 0, wxALL, 5);
        okCancelSizer->Add(new wxButton(&dialog, wxID_CANCEL), 0, wxALL, 5);

        dialog.Fit();
        dialog.OnInit();
        dialog.ShowModal();
        dialog.Destroy();
}
