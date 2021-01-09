#pragma once

#include "../inc/MarlinConfig.h"

#ifdef X1_BUTTONS 
    void x1_setup();
    void x1_idle();

    #define STARTK_PIN 27
    #define STARTK_LED_PIN 10
    #define PLUSK_PIN 28
    #define PLUSK_LED_PIN 11
    #define MINUSK_PIN 29
    #define MINUSK_LED_PIN 16
    #define HOMEK_PIN 30
    #define HOMEK_LED_PIN 17

#endif