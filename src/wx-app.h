#ifndef SRC_WX_APP_H_
#define SRC_WX_APP_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "wx-status.h"

wxDECLARE_EVENT(WX_EXIT_EVENT, wxCommandEvent);

class Frame;

class App: public wxApp
{
public:
        App();
        virtual bool OnInit();
        virtual int OnRun();
        Frame* GetFrame()
        {
                return frame;
        }
private:
        Frame* frame;
};

class Frame: public wxFrame
{
public:
        Frame(App* app, const wxString& title, const wxPoint& pos,
                        const wxSize& size);

        virtual ~Frame() {}

        void Start();

private:
        void OnCommand(wxCommandEvent& event);
        void OnExit(wxCommandEvent& event);
        void OnClose(wxCloseEvent& event);
        void OnShow(wxShowEvent& event);
        void OnMoveWindow(wxMoveEvent& event);
        StatusPane* statusPane;
        StatusTimer* statusTimer;

        bool closed;

        void Quit(bool stop_emulator = 1);

        wxDECLARE_EVENT_TABLE();
};

#endif /* SRC_WX_APP_H_ */
