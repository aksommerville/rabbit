#include "rb_evdev_internal.h"

/* First 128 KEY codes.
 */
 
static uint32_t rb_hidusage_for_key_0_127[128]={
  [KEY_ESC]=0x00070029,
  [KEY_1]=0x0007001e,
  [KEY_2]=0x0007001f,
  [KEY_3]=0x00070020,
  [KEY_4]=0x00070021,
  [KEY_5]=0x00070022,
  [KEY_6]=0x00070023,
  [KEY_7]=0x00070024,
  [KEY_8]=0x00070025,
  [KEY_9]=0x00070026,
  [KEY_0]=0x00070027,
  [KEY_MINUS]=0x0007002d,
  [KEY_EQUAL]=0x0007002e,
  [KEY_BACKSPACE]=0x0007002a,
  [KEY_TAB]=0x0007002b,
  [KEY_Q]=0x00070014,
  [KEY_W]=0x0007001a,
  [KEY_E]=0x00070008,
  [KEY_R]=0x00070015,
  [KEY_T]=0x00070017,
  [KEY_Y]=0x0007001c,
  [KEY_U]=0x00070018,
  [KEY_I]=0x0007000c,
  [KEY_O]=0x00070012,
  [KEY_P]=0x00070013,
  [KEY_LEFTBRACE]=0x0007002f,
  [KEY_RIGHTBRACE]=0x00070030,
  [KEY_ENTER]=0x00070028,
  [KEY_LEFTCTRL]=0x000700e0,
  [KEY_A]=0x00070004,
  [KEY_S]=0x00070016,
  [KEY_D]=0x00070007,
  [KEY_F]=0x00070009,
  [KEY_G]=0x0007000a,
  [KEY_H]=0x0007000b,
  [KEY_J]=0x0007000d,
  [KEY_K]=0x0007000e,
  [KEY_L]=0x0007000f,
  [KEY_SEMICOLON]=0x00070033,
  [KEY_APOSTROPHE]=0x00070034,
  [KEY_GRAVE]=0x00070035,
  [KEY_LEFTSHIFT]=0x000700e1,
  [KEY_BACKSLASH]=0x00070031,
  [KEY_Z]=0x0007001d,
  [KEY_X]=0x0007001b,
  [KEY_C]=0x00070006,
  [KEY_V]=0x00070019,
  [KEY_B]=0x00070005,
  [KEY_N]=0x00070011,
  [KEY_M]=0x00070010,
  [KEY_COMMA]=0x00070036,
  [KEY_DOT]=0x00070037,
  [KEY_SLASH]=0x00070038,
  [KEY_RIGHTSHIFT]=0x000700e5,
  [KEY_KPASTERISK]=0x00070055,
  [KEY_LEFTALT]=0x000700e2,
  [KEY_SPACE]=0x0007002c,
  [KEY_CAPSLOCK]=0x00070039,
  [KEY_F1]=0x0007003a,
  [KEY_F2]=0x0007003b,
  [KEY_F3]=0x0007003c,
  [KEY_F4]=0x0007003d,
  [KEY_F5]=0x0007003e,
  [KEY_F6]=0x0007003f,
  [KEY_F7]=0x00070040,
  [KEY_F8]=0x00070041,
  [KEY_F9]=0x00070042,
  [KEY_F10]=0x00070043,
  [KEY_SCROLLLOCK]=0x00070047,
  [KEY_KP7]=0x0007005f,
  [KEY_KP8]=0x00070060,
  [KEY_KP9]=0x00070061,
  [KEY_KPMINUS]=0x00070056,
  [KEY_KP4]=0x0007005c,
  [KEY_KP5]=0x0007005d,
  [KEY_KP6]=0x0007005e,
  [KEY_KPPLUS]=0x00070057,
  [KEY_KP1]=0x00070059,
  [KEY_KP2]=0x0007005a,
  [KEY_KP3]=0x0007005b,
  [KEY_KP0]=0x00070062,
  [KEY_KPDOT]=0x00070063,
  [KEY_F11]=0x00070044,
  [KEY_F12]=0x00070045,
  [KEY_KPENTER]=0x00070058,
  [KEY_RIGHTCTRL]=0x000700e4,
  [KEY_KPSLASH]=0x00070054,
  [KEY_SYSRQ]=0x0007009a,
  [KEY_RIGHTALT]=0x000700e6,
  [KEY_HOME]=0x0007004a,
  [KEY_UP]=0x00070052,
  [KEY_PAGEUP]=0x0007004b,
  [KEY_LEFT]=0x00070050,
  [KEY_RIGHT]=0x0007004f,
  [KEY_END]=0x0007004d,
  [KEY_DOWN]=0x00070051,
  [KEY_PAGEDOWN]=0x0007004e,
  [KEY_INSERT]=0x00070049,
  [KEY_DELETE]=0x0007004c,
  [KEY_MUTE]=0x0007007f,
  [KEY_VOLUMEDOWN]=0x00070081,
  [KEY_VOLUMEUP]=0x00070080,
  [KEY_POWER]=0x00070066,
  [KEY_KPEQUAL]=0x00070067,
  [KEY_KPPLUSMINUS]=0x000700d7,
  [KEY_PAUSE]=0x00070048,
  [KEY_KPCOMMA]=0x00070085,
  [KEY_LEFTMETA]=0x000700e3,
  [KEY_RIGHTMETA]=0x000700e7,
};

/* Guess HID usage value from Linux input ID.
 */
 
uint32_t rb_evdev_guess_hidusage(uint8_t type,uint16_t code) {
  switch (type) {
    
    case EV_ABS: switch (code) {
        case ABS_X: return 0x00010030; // X
        case ABS_Y: return 0x00010031; // Y
        case ABS_Z: return 0x00010032; // Z
        case ABS_RX: return 0x00010033; // Rx
        case ABS_RY: return 0x00010034; // Ry
        case ABS_RZ: return 0x00010035; // Rz
        case ABS_WHEEL: return 0x00010038; // Wheel
        case ABS_HAT0X: return 0x00010030; // X
        case ABS_HAT0Y: return 0x00010031; // Y
        case ABS_HAT1X: return 0x00010030; // X
        case ABS_HAT1Y: return 0x00010031; // Y
        case ABS_HAT2X: return 0x00010030; // X
        case ABS_HAT2Y: return 0x00010031; // Y
        case ABS_HAT3X: return 0x00010030; // X
        case ABS_HAT3Y: return 0x00010031; // Y
      } return 0;
      
    case EV_KEY: {
        if (code<0x80) return rb_hidusage_for_key_0_127[code];
        if ((code>=BTN_MISC)&&(code<BTN_MISC+0x10)) {
          return 0x00090000+code-BTN_MISC;
        }
        if ((code>=BTN_MOUSE)&&(code<BTN_MOUSE+0x10)) {
          // Linux enumerates mouse buttons but HID does not.
          return 0x00090000+code-BTN_MOUSE;
        }
        switch (code) {
          case KEY_F13: return 0x00070068;
          case KEY_F14: return 0x00070069;
          case KEY_F15: return 0x0007006a;
          case KEY_F16: return 0x0007006b;
          case KEY_F17: return 0x0007006c;
          case KEY_F18: return 0x0007006d;
          case KEY_F19: return 0x0007006e;
          case KEY_F20: return 0x0007006f;
          case KEY_F21: return 0x00070070;
          case KEY_F22: return 0x00070071;
          case KEY_F23: return 0x00070072;
          case KEY_F24: return 0x00070073;
          // joystick, gamepad:
          case BTN_SELECT: return 0x0001003e; // Select
          case BTN_START: return 0x0001003d; // Start
        }
        if ((code>=0x120)&&(code<0x140)) { // JOYSTICK+GAMEPAD
          // Other JOYSTICK and GAMEPAD buttons, use HID "Gamepad Fire/Jump" or "Gamepad Trigger"
          if (code&1) return 0x00050039; // Trigger
          return 0x00050037; // Fire/Jump
        }
      } return 0;
      
  }
  return 0;
}
