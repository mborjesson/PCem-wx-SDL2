#include "wx-app.h"

#include <wx/xrc/xmlres.h>
#include <wx/event.h>

extern "C"
{
int wx_start(void*);
void wx_stop(void*);
void wx_show(void*);
void wx_handle_menu(void*, int);
int window_remember;
}

int wx_window_x = 0;
int wx_window_y = 0;

extern void InitXmlResource();

wxDEFINE_EVENT(WX_EXIT_EVENT, wxCommandEvent);

wxBEGIN_EVENT_TABLE(Frame, wxFrame)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP_NO_MAIN(App);

App::App()
{
        this->frame = NULL;
}

bool App::OnInit()
{
        wxXmlResource::Get()->InitAllHandlers();
//        if (!wxXmlResource::Get()->Load("src/pc.xrc"))
//        {
//                std::cout << "Could not load resource file" << std::endl;
//                return false;
//        }
        InitXmlResource();

        frame = new Frame(this, "PCem Machine", wxPoint(50, 50),
                        wxSize(DEFAULT_WINDOW_WIDTH, 200));
        frame->Start();
        return true;
}

int App::OnRun()
{
        return wxApp::OnRun();
}

Frame::Frame(App* app, const wxString& title, const wxPoint& pos,
                const wxSize& size) :
                wxFrame(NULL, wxID_ANY, title, pos, size, wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER))
{
        this->closed = false;
        this->statusPane = new StatusPane(this);
        SetMenuBar(wxXmlResource::Get()->LoadMenuBar(wxT("main_menu")));

        Bind(wxEVT_CLOSE_WINDOW, &Frame::OnClose, this);
        Bind(wxEVT_MENU, &Frame::OnCommand, this);
        Bind(wxEVT_SHOW, &Frame::OnShow, this);
        Bind(wxEVT_MOVE, &Frame::OnMoveWindow, this);
        Bind(WX_EXIT_EVENT, &Frame::OnExit, this);

        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(statusPane, 1, wxEXPAND);
        SetSizer(sizer);

        statusTimer = new StatusTimer(statusPane);
}

void Frame::Start() {
        Show();
}

void Frame::OnShow(wxShowEvent& event) {
        if (wx_start(this)) {
                statusTimer->Start();
                if (window_remember) {
                        SetPosition(wxPoint(wx_window_x, wx_window_y));
                } else {
                        CenterOnScreen();
                }
        } else {
                Quit();
        }

}

void Frame::OnExit(wxCommandEvent& event)
{
        Quit();
}

void Frame::OnCommand(wxCommandEvent& event)
{
        wx_handle_menu(this, event.GetId());
}

void Frame::OnClose(wxCloseEvent& event)
{
        Quit();
}

void Frame::Quit(bool stop_emulator) {
        if (closed)
        {
                return;
        }
        closed = true;
        if (stop_emulator)
        {
                statusTimer->Stop();
                wx_stop(this);
        }
        Destroy();
}

void Frame::OnMoveWindow(wxMoveEvent& event)
{
        if (window_remember) {
                wx_window_x = event.GetPosition().x;
                wx_window_y = event.GetPosition().y;
        }
}

