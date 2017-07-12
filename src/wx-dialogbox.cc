#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>
#include <wx/notebook.h>

#include "wx-dialogbox.h"


BEGIN_EVENT_TABLE(PCemDialogBox, wxDialog) END_EVENT_TABLE()

PCemDialogBox::PCemDialogBox(wxWindow* parent, int (*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2)) :
                wxDialog(parent, -1, "No title")
{
        this->callback = callback;
        this->commandActive = false;
}

PCemDialogBox::PCemDialogBox(wxWindow* parent, const char* name, int (*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2))
{
        wxXmlResource::Get()->LoadDialog(this, parent, name);
        this->callback = callback;
        this->commandActive = false;
}

void PCemDialogBox::OnInit()
{
        if (callback)
        {
                callback(this, WX_INITDIALOG, 0, 0);
                Bind(wxEVT_BUTTON, &PCemDialogBox::OnCommand, this);
                Bind(wxEVT_RADIOBUTTON, &PCemDialogBox::OnCommand, this);
                Bind(wxEVT_TEXT, &PCemDialogBox::OnCommand, this);
                Bind(wxEVT_COMBOBOX, &PCemDialogBox::OnCommand, this);
                Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &PCemDialogBox::OnNotebookChanged, this);
        }

        wxWindow* root = FindWindow(XRCID("ROOT_PANEL"));
        if (root)
                root->Fit();
        Fit();
}

int PCemDialogBox::processEvent(int message, INT_PARAM param1, LONG_PARAM param2)
{
        int result = 0;
        /* lock the current command to prevent strange behavior */
        if (!commandActive)
        {
                commandActive = true;
                result = callback(this, message, param1, param2);
                commandActive = false;
        }
        return result;
}

void PCemDialogBox::OnNotebookChanged(wxCommandEvent& event)
{
        wxBookCtrlEvent* ev = (wxBookCtrlEvent*)&event;
        processEvent(WX_COMMAND, ev->GetId(), ev->GetSelection());
}

void PCemDialogBox::OnCommand(wxCommandEvent& event)
{
        processEvent(WX_COMMAND, event.GetId(), 0);
}

