#ifndef __CODEY_MENU_H__
#define __CODEY_MENU_H__

#include <MenuItemInterface.h>

/*********************************************************************
** Codey Rocky on-board sensors and actuators
** (light, sound, knob, IMU, RGB LED, speaker, Rocky base)
*********************************************************************/
class CodeyMenu : public MenuItemInterface {
public:
    CodeyMenu() : MenuItemInterface("Sensors") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    bool hasTheme() { return false; }
    String themePath() { return ""; }

private:
    void lightApp(void);
    void soundApp(void);
    void knobApp(void);
    void imuApp(void);
    void rgbApp(void);
    void speakerApp(void);
    void facesApp(void);
    void rockyApp(void);
    void rockyDriveMenu(void);
    void rockyStatusView(void);
    void rockyLineView(void);
};

#endif
