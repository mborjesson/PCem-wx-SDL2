#include "wx-dialogbox.h"

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/xrc/xmlres.h>

BEGIN_EVENT_TABLE(PCemDialogBox, wxDialog) END_EVENT_TABLE()

PCemDialogBox::PCemDialogBox(wxWindow* parent, int (*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2)) :
                wxDialog(parent, -1, "No title")
{
        this->callback = callback;
}

PCemDialogBox::PCemDialogBox(wxWindow* parent, char* name, int (*callback)(void* window, int message, INT_PARAM param1, LONG_PARAM param2))
{
        wxXmlResource::Get()->LoadDialog(this, parent, name);
        this->callback = callback;
}

void PCemDialogBox::OnInit()
{
        callback(this, WX_INITDIALOG, 0, 0);

        Bind(wxEVT_BUTTON, &PCemDialogBox::OnCommand, this);
        Bind(wxEVT_TEXT, &PCemDialogBox::OnCommand, this);
        Bind(wxEVT_COMBOBOX, &PCemDialogBox::OnCommand, this);

        Fit();
}

void PCemDialogBox::OnCommand(wxCommandEvent& event)
{
        callback(this, WX_COMMAND, event.GetId(), 0);
}

