/*
teensy_control_tl_cam
2022-07-26 Created by: Wataru Ito

Function:
    Generates GPIO output to trigger the cameras.
Development env:
    PlatformIO/VS Code
Dependency:
    SimpleCLI   https://github.com/SpacehuhnTech/SimpleCLI
*/

#include <Arduino.h>

// User defined constants
#define DUR_CS 120.0 // The duration of CS, 120.0 seconds
#define CS_START 241 // 241th frame is the first video frame.

#define TRIG_OUT 12        // output to RPi/RPG
#define TRIG_IN 24         // input from FreezeFrame
#define SELECT_FREEZE_0 30 // select fz_start_frame_0
#define SELECT_FREEZE_1 31 // select fz_start_frame_1
#define SELECT_FREEZE_2 32 // select fz_start_frame_2

#define TIME_SAMPLING_FREQ 100000  // GPIO input/output at 100k Hz
#define SEC_IN_MICRO 1000000.0     // constant: 1 second in microseconds
#define LED_BLINK_DUR_IN_MILLI 100 // LED blinking at 5 Hz, 100 milliseconds * 2 intervals
#define ARRAY_SIZE 100             // for fz_start, fz_end

// Freezing simulation data
// Test data
// int fz_start_frames[] = {249, 255, 267, 294, 331, 353, 364, 375, 403, 424, 430, 519, 550, 563, 577, 599, 634, 652, 689};
// int fz_end_frames[] = {253, 262, 272, 299, 338, 359, 368, 383, 409, 429, 435, 525, 558, 568, 594, 615, 640, 657, 694};
// 20190408_testing_1_m10ab subject1
int fz_start_frames_0[] = {266, 291, 296, 312, 335, 348, 372, 388, 454, 466, 493, 510, 533, 549, 579, 605, 626, 682, 702};
int fz_end_frames_0[] = {282, 295, 303, 327, 344, 366, 387, 437, 462, 486, 506, 526, 542, 572, 596, 622, 641, 692, 715};
// 20190408_testing_1_m6ab, subjects 1
int fz_start_frames_1[] = {258, 272, 336, 364, 400, 434, 465, 486, 504, 543, 560, 578, 596, 628, 714};
int fz_end_frames_1[] = {262, 323, 350, 395, 410, 457, 484, 499, 535, 552, 573, 591, 618, 655, 720};
// 20190408_testing_1_m6ab, subjects 2
int fz_start_frames_2[] = {245, 257, 276, 299, 327, 372, 433, 442, 516, 580, 589, 628, 699};
int fz_end_frames_2[] = {249, 268, 285, 318, 359, 407, 439, 507, 572, 588, 622, 676, 710};

int freeze_select; // selected freezing simulation data
int size_fz_start;
int size_fz_end;

float fz_start[ARRAY_SIZE];
float fz_end[ARRAY_SIZE];

unsigned long fz_simulation_start = 0; // monitor elapsed time after simulation starts
unsigned long led_start = 0;           // monitor elapsed time after LED starts blinking cycle
const int led = LED_BUILTIN;           // LED pin
bool led_on = LOW;                     // LED status
bool trig = LOW;                       // trigger status
bool fz_simulation = false;            // flag to indicate whether the simulation is running or not

// Create IntervalTimer object
//      https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
IntervalTimer int_timer;

// Convert from frame to seconds
void conv_frame_to_sec(int start_end_frames[], float start_end[], int size_array)
{
    int i;
    for (i = 0; i < size_array; i++)
    {
        start_end[i] = float(start_end_frames[i] - CS_START) / 4.0;
    }
}

// Return the index of the nearest earlier events
int earlier_nearest(float x, float target_array[], int size_array)
{
    int i;

    // Check the first element
    float distance = x - target_array[0];
    if (distance < 0)
        return -1;
    // Check the following elements
    for (i = 1; i < size_array; i++)
    {
        distance = x - target_array[i];
        if (distance < 0)
        {
            return i - 1;
        }
    }
    return -1;
}

// Control the LED blinking
void led_blink()
{
    unsigned long time_now = millis();
    if (time_now - led_start > LED_BLINK_DUR_IN_MILLI * 2)
    {
        led_on = HIGH;
        led_start = time_now;
    }
    else if (time_now - led_start > LED_BLINK_DUR_IN_MILLI)
    {
        led_on = LOW;
    }

    digitalWrite(led, led_on);
}

// IntervalTimer callback function
void interval_timer_callback()
{
    // Monitor TRIG_IN
    if (digitalRead(TRIG_IN) == HIGH)
    {
        if (!fz_simulation)
        {
            fz_simulation_start = micros();
        }
        fz_simulation = true;
    }
    else
    {
        fz_simulation = false;
    }

    // Scan fz array and set TRIG_OUT
    if (fz_simulation)
    {
        unsigned long time_current = micros();
        float elapsed_time = float(time_current - fz_simulation_start) / SEC_IN_MICRO;

        // Serial.print("time_current_f: ");
        // Serial.println(time_current_f);

        if (elapsed_time < DUR_CS)
        {
            // Serial.print("elapsed_time: ");
            // Serial.println(elapsed_time);

            int fz_index = earlier_nearest(elapsed_time, fz_start, size_fz_start);

            // Serial.print(fz_end[fz_index]);
            // Serial.print(" ");
            // Serial.println(elapsed_time);

            if (fz_end[fz_index] > elapsed_time)
            {
                // freeze - gratings stop
                digitalWrite(TRIG_OUT, LOW);
                digitalWrite(led, HIGH);
            }
            else
            {
                // non freeze - gratings start
                digitalWrite(TRIG_OUT, HIGH);
                digitalWrite(led, LOW);
            }
        }
        else
        {
            digitalWrite(TRIG_OUT, HIGH);
            // digitalWrite(led, LOW);
            led_blink();
        }
    }
    else
    {
        digitalWrite(TRIG_OUT, HIGH);
        led_blink();
    }
}

// Setup function
void setup()
{
    pinMode(led, OUTPUT);
    pinMode(TRIG_OUT, OUTPUT);
    pinMode(TRIG_IN, INPUT);

    Serial.begin(9600);

    // Identify the selected freezing simulation data
    pinMode(SELECT_FREEZE_0, INPUT);
    pinMode(SELECT_FREEZE_1, INPUT);
    pinMode(SELECT_FREEZE_2, INPUT);

    if (digitalRead(SELECT_FREEZE_0) == HIGH)
        freeze_select = 0;
    else if (digitalRead(SELECT_FREEZE_1) == HIGH)
        freeze_select = 1;
    else if (digitalRead(SELECT_FREEZE_2) == HIGH)
        freeze_select = 2;
    else
        freeze_select = 0;

    switch (freeze_select)
    {
    case 0:
        size_fz_start = sizeof(fz_start_frames_0) / sizeof(fz_start_frames_0[0]);
        size_fz_end = sizeof(fz_end_frames_0) / sizeof(fz_end_frames_0[0]);

        conv_frame_to_sec(fz_start_frames_0, fz_start, size_fz_start);
        conv_frame_to_sec(fz_end_frames_0, fz_end, size_fz_end);
        break;
    case 1:
        size_fz_start = sizeof(fz_start_frames_1) / sizeof(fz_start_frames_1[0]);
        size_fz_end = sizeof(fz_end_frames_1) / sizeof(fz_end_frames_1[0]);

        conv_frame_to_sec(fz_start_frames_1, fz_start, size_fz_start);
        conv_frame_to_sec(fz_end_frames_1, fz_end, size_fz_end);
        break;
    case 2:
        size_fz_start = sizeof(fz_start_frames_2) / sizeof(fz_start_frames_2[0]);
        size_fz_end = sizeof(fz_end_frames_2) / sizeof(fz_end_frames_2[0]);

        conv_frame_to_sec(fz_start_frames_2, fz_start, size_fz_start);
        conv_frame_to_sec(fz_end_frames_2, fz_end, size_fz_end);
        break;
    }

    // Wait for the serial connection to be established
    delay(1000);

    Serial.println("### control_gratings started ###");
    Serial.println("Stored freezing patterns:");
    Serial.print("fz_start (frame): ");
    for (int i = 0; i < size_fz_start; i++)
    {
        Serial.print(fz_start[i]);
        Serial.print(" ");
    }
    Serial.println("");
    Serial.print("fz_end (frame): ");
    for (int i = 0; i < size_fz_start; i++)
    {
        Serial.print(fz_end[i]);
        Serial.print(" ");
    }
    Serial.println("");

    // Generate callback for the timer
    int timer_micros = 1000000 / TIME_SAMPLING_FREQ;
    int_timer.begin(interval_timer_callback, timer_micros);
}

// Loop function
void loop()
{
}
//####################################