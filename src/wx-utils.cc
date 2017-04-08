#include "wx-utils.h"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/spinctrl.h>
#include <wx/xrc/xmlres.h>

#include "wx-dialogbox.h"
#include "wx-app.h"

int wx_messagebox(void*, char* message, char* title = NULL, int style = 5)
{
        return wxMessageBox(message, title, style);
}

void wx_setwindowtitle(void* window, char* s)
{
        ((wxFrame*) window)->SetTitle(s);
}

int wx_xrcid(const char* s)
{
        return XRCID(s);
}

int wx_filedialog(void* window, const char* title, const char* path,
                const char* extensions, int open, char* file)
{
        wxFileDialog dlg((wxWindow*) window, title, "", path, extensions,
                        open ? wxFD_OPEN : wxFD_SAVE);
        if (dlg.ShowModal() == wxID_OK)
        {
                strcpy(file, dlg.GetPath().mb_str());
                return 0;
        }
        return 1;
}

void wx_checkmenuitem(void* menu, int id, int checked)
{
        wxMenuItem* item = ((wxMenuBar*) menu)->FindItem(id);
        if (item)
                item->Check(checked);
}

void wx_enablemenuitem(void* menu, int id, int enable)
{

        wxMenuItem* item = ((wxMenuBar*) menu)->FindItem(id);
        if (item)
                item->Enable(enable);
}

void* wx_getmenu(void* window)
{
        return ((wxFrame*) window)->GetMenuBar();
}

void* wx_getdlgitem(void* window, int id)
{
        return ((wxWindow*) window)->FindWindow(id);
}

void wx_setdlgitemtext(void* window, int id, char* str)
{
        wxWindow* w = ((wxWindow*) window)->FindWindow(id);
        if (w->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                ((wxTextCtrl*) w)->SetValue(str);
        else
                ((wxStaticText*) w)->SetLabel(str);
}

void wx_enablewindow(void* window, int enabled)
{
        ((wxWindow*) window)->Enable(enabled);
}

void wx_showwindow(void* window, int show)
{
        ((wxWindow*) window)->Show(show);
}

void wx_togglewindow(void* window)
{
        wxCommandEvent* event = new wxCommandEvent(WX_TOGGLE_WINDOW_EVENT, ((wxWindow*)window)->GetId());
        event->SetEventObject((wxWindow*)window);
        wxQueueEvent((wxWindow*)window, event);
}

void wx_enddialog(void* window, int ret_code)
{
        ((wxDialog*) window)->EndModal(ret_code);
}

int wx_sendmessage(void* window, int type, INT_PARAM param1, LONG_PARAM param2)
{
        switch (type)
        {
        case WX_CB_ADDSTRING:
                ((wxComboBox*) window)->Append((char*) param2);
                break;
        case WX_CB_SETCURSEL:
                ((wxComboBox*) window)->Select(param1);
                break;
        case WX_CB_GETCURSEL:
                return ((wxComboBox*) window)->GetCurrentSelection();
                break;
        case WX_CB_GETLBTEXT:
                strcpy((char*) param2, ((wxComboBox*) window)->GetString(param1));
                break;
        case WX_CB_RESETCONTENT:
        {
#ifndef __WXOSX_MAC__
                ((wxComboBox*) window)->Clear();
#else
                /* Clear() does not work on OSX */
                wxComboBox* cb = (wxComboBox*) window;
                int count = cb->GetCount();
                while (count--)
                        cb->Delete(0);
#endif
                break;
        }
        case WX_BM_SETCHECK:
                ((wxCheckBox*) window)->SetValue(param1);
                break;
        case WX_BM_GETCHECK:
                return ((wxCheckBox*) window)->GetValue();
                break;
        case WX_WM_SETTEXT:
        {
                if (((wxWindow*) window)->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                        ((wxTextCtrl*) window)->SetValue((char*) param2);
                else
                        ((wxStaticText*) window)->SetLabel((char*) param2);
        }
                break;
        case WX_WM_GETTEXT:
                if (((wxWindow*) window)->GetClassInfo()->IsKindOf(CLASSINFO(wxTextCtrl)))
                        strcpy((char*) param2, ((wxTextCtrl*) window)->GetValue());
                else
                        strcpy((char*) param2, ((wxStaticText*) window)->GetLabel());
                break;
        case WX_UDM_SETPOS:
                ((wxSpinCtrl*) window)->SetValue(param2);
                break;
        case WX_UDM_GETPOS:
                return ((wxSpinCtrl*) window)->GetValue();
        case WX_UDM_SETRANGE:
        {
                int min = (param2 >> 16) & 0xffff;
                int max = param2 & 0xffff;
                ((wxSpinCtrl*) window)->SetRange(min, max);
                break;
        }
        case WX_UDM_SETINCR:
        {
                /* SetIncrement-method may not exist on all platforms */
#ifdef __WXGTK__
                ((wxSpinCtrl*) window)->SetIncrement(param2);
#endif
                break;
        }
        }

        ((wxWindow*)window)->Fit();

        return 0;
}

int wx_dialogbox(void* window, char* name, int (*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2))
{
        PCemDialogBox dlg((wxWindow*) window, name, callback);
        dlg.OnInit();
        int ret = dlg.ShowModal();
        dlg.Destroy();
        return ret;
}

void wx_exit(void* window, int value) {
        wxCommandEvent* event = new wxCommandEvent(WX_EXIT_EVENT, ((wxWindow*)window)->GetId());
        event->SetEventObject((wxWindow*)window);
        event->SetInt(value);
        wxQueueEvent((wxWindow*)window, event);
}

int wx_yield()
{
	return wxYield();
}

/* Timer stuff */

class PCemTimer : public wxTimer
{
public:
        PCemTimer(void (*fn)())
        {
                this->fn = fn;
        }
        virtual ~PCemTimer() {};

        void Notify()
        {
                fn();
        }
private:
        void (*fn)();
};

void* wx_createtimer(void (*fn)())
{
        return new PCemTimer(fn);
}

void wx_starttimer(void* timer, int milliseconds, int once)
{
        wxTimer* t = (wxTimer*)timer;
        t->Start(milliseconds, once);
}

void wx_stoptimer(void* timer)
{
        wxTimer* t = (wxTimer*)timer;
        t->Stop();
}

void wx_destroytimer(void* timer)
{
        wx_stoptimer(timer);
        delete (wxTimer*)timer;
}
