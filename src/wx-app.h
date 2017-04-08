#ifndef SRC_WX_APP_H_
#define SRC_WX_APP_H_

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "wx-status.h"

wxDECLARE_EVENT(WX_EXIT_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_EXIT_COMPLETE_EVENT, wxCommandEvent);
wxDECLARE_EVENT(WX_TOGGLE_WINDOW_EVENT, wxCommandEvent);

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
        int FilterEvent(wxEvent& event);
};

class CExitThread: public wxThread
{
public:
	CExitThread(Frame* frame);
    virtual ~CExitThread() {}
private:
    wxThread::ExitCode Entry();
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
        void OnExitEvent(wxCommandEvent& event);
        void OnExitCompleteEvent(wxCommandEvent& event);
        void OnToggleWindowEvent(wxCommandEvent& event);
        void OnClose(wxCloseEvent& event);
        void OnMoveWindow(wxMoveEvent& event);
        void OnIdle(wxIdleEvent& event);
        StatusPane* statusPane;
        StatusTimer* statusTimer;

        CExitThread* exitThread;

        bool closed;

        void Quit(bool stop_emulator = 1);

        wxDECLARE_EVENT_TABLE();
};

#endif /* SRC_WX_APP_H_ */
