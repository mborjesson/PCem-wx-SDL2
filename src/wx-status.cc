#include "wx-status.h"
#include "wx-common.h"
#include <sstream>

extern "C" {
int get_status(char*, char*);
int readflash;
int fps;
int updatestatus;
int get_machine_info(char*);
}

int show_status = 0;
int show_speed_history = 0;
int show_disc_activity = 1;
int show_machine_info = 1;

#define ACTIVITY_TEXT wxT("Activity")

#define MAX(a, b) a > b ? a : b

StatusTimer::StatusTimer(StatusPane* pane)
{
        this->pane = pane;
}

void StatusTimer::Notify()
{
        pane->Refresh();
}

void StatusTimer::Start()
{
        wxTimer::Start(10);
}

BEGIN_EVENT_TABLE(StatusPane, wxPanel) EVT_PAINT(StatusPane::PaintEvent)
END_EVENT_TABLE()

StatusPane::StatusPane(wxFrame* parent) :
                wxPanel(parent)
{
        SetDoubleBuffered(true);
        machineInfoText[0] = statusMachineText[0] = statusDeviceText[0] = 0;
        lastSpeedUpdate = 0;
        memset(speedHistory, -1, sizeof(speedHistory));
}

StatusPane::~StatusPane() {
}

void StatusPane::PaintEvent(wxPaintEvent& evt)
{
        wxPaintDC dc(this);
        Render(dc);
}

void StatusPane::PaintNow()
{
        wxClientDC dc(this);
        Render(dc);
}

void StatusPane::Render(wxDC& dc)
{
        wxSize cSize = GetParent()->GetClientSize();

        int width = 0;
        int height = 0;
        wxLongLong millis = wxGetUTCTimeMillis();

        dc.SetBackground(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));
        dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
        dc.Clear();

        // draw model/cpu etc
        if (show_machine_info) {
                get_machine_info(machineInfoText);
                wxSize size = dc.GetMultiLineTextExtent(machineInfoText);
                dc.DrawText(machineInfoText, width+5, height+5);

                width = MAX(width, 5+size.x);
                height = MAX(height, 5+size.y);
        }

        // draw activity
        if (show_disc_activity) {
                int w = 50;
                int h = 25;

                if (readflash) {
                        readflash = 0;
                        dc.SetBrush(wxBrush(wxColour(0,255,0)));
                } else {
                        dc.SetBrush(wxBrush(wxColour(0,100,0)));
                }
                dc.DrawRectangle(cSize.x-w-5, 5, w, h);

                height = MAX(height, 5+25);
        }

        // draw status text
        if (show_status) {
                if (updatestatus) {
                        updatestatus = 0;
                        get_status(statusMachineText, statusDeviceText);
                }
                if (statusMachineText) {
                        int statusX = 5;
                        int statusY = height+5;
                        wxSize size = dc.GetMultiLineTextExtent(statusMachineText);
                        dc.DrawText(statusMachineText, statusX, statusY);
                        width = MAX(width, statusX+size.GetWidth());
                        height = MAX(height, statusY+size.GetHeight());
                        if (statusDeviceText) {
                                wxSize dSize = dc.GetMultiLineTextExtent(statusDeviceText);
                                dc.DrawText(statusDeviceText, statusX+ceil((size.GetWidth()+50)/100.0)*100, statusY);
                                width = MAX(width, statusX+ceil((size.GetWidth()+50)/100.0)*100+ceil((dSize.GetWidth()+50)/100.0)*100);
                                height = MAX(height, statusY+dSize.GetHeight());
                        }
                }
        }

        if (millis-lastSpeedUpdate > 500) {
                lastSpeedUpdate = millis;
                memmove(speedHistory+1, speedHistory, SPEED_HISTORY_LENGTH-2);
                speedHistory[0] = fps;
        }
        // draw speed history
        if (show_speed_history) {
                int speedGraphBorder = 5;
                int speedGraphX = 5;
                int speedGraphY = height+5;
                int speedGraphWidth = MAX(2*SPEED_HISTORY_LENGTH, width-5);
                double lineLength = (double)(speedGraphWidth-2)/(SPEED_HISTORY_LENGTH-1);
                int speedGraphHeight = 125;
                dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
                dc.DrawRectangle(speedGraphX, speedGraphY, speedGraphWidth, speedGraphHeight+speedGraphBorder*2);
                dc.SetPen(wxPen(wxColour(200, 200, 200)));
                dc.DrawLine(speedGraphX, speedGraphY+speedGraphBorder+speedGraphHeight-0, speedGraphX+speedGraphWidth, speedGraphY+speedGraphHeight+speedGraphBorder-0); // 0
                dc.DrawLine(speedGraphX, speedGraphY+speedGraphBorder+speedGraphHeight-50, speedGraphX+speedGraphWidth, speedGraphY+speedGraphHeight+speedGraphBorder-50); // 50
                dc.DrawLine(speedGraphX, speedGraphY+speedGraphBorder+speedGraphHeight-100, speedGraphX+speedGraphWidth, speedGraphY+speedGraphHeight+speedGraphBorder-100); // 100
                dc.DrawLine(speedGraphX, speedGraphY+speedGraphBorder+speedGraphHeight-125, speedGraphX+speedGraphWidth, speedGraphY+speedGraphHeight+speedGraphBorder-125); // 125
                dc.SetPen(wxPen(wxColour(0, 0, 0)));
                for (int i = 0; i < SPEED_HISTORY_LENGTH-1; ++i) {
                        int v0 = speedHistory[i];
                        int v1 = speedHistory[i+1];
                        if (v0 >= 0 && v1 >= 0) {
                                dc.DrawLine(speedGraphX+speedGraphWidth-i*lineLength-1, speedGraphY+speedGraphHeight+speedGraphBorder-v0,
                                                speedGraphX+speedGraphWidth-(i+1)*lineLength-1, speedGraphY+speedGraphHeight+speedGraphBorder-v1);
                        }
                }
                dc.SetBrush(wxBrush(wxColour(255, 255, 255), wxBrushStyle(wxBRUSHSTYLE_TRANSPARENT)));
                dc.DrawRectangle(speedGraphX, speedGraphY, speedGraphWidth, speedGraphHeight+speedGraphBorder*2);

                width = MAX(width, (speedGraphX+speedGraphWidth));
                height = MAX(height, speedGraphY+speedGraphHeight+speedGraphBorder*2);
        }

        width += 5;
        height += 5;
        width = width < DEFAULT_WINDOW_WIDTH ? DEFAULT_WINDOW_WIDTH : width;
        height = height < DEFAULT_WINDOW_HEIGHT ? DEFAULT_WINDOW_HEIGHT : height;
        if (cSize.GetWidth() != width || cSize.GetHeight() != height) {
                GetParent()->SetClientSize(width, height);
        }
}
