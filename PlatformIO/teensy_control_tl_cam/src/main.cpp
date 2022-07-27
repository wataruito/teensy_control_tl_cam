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

/*
Relevant commands
    commu_arduino("set_camera -cameras_number " + str(cameraNum))
    commu_arduino("set_camera -start")
    commu_arduino("set_camera -fps " + str(fps) + " -exposure " + str(exposure))
    commu_arduino("set_camera -status")
    commu_arduino("set_camera -stop")
    commu_arduino("start paradigm -button")
*/

#include <Arduino.h>

// Initialize SimpleCLI command line interface
#include <SimpleCLI.h>
SimpleCLI cli;
// Command cmdSetVerbose;
// Command cmdSetParadigm;
// Command cmdSetTone;
Command cmdSetCamera;
// Command cmdStart;

#define TRIG_IN 24                 // input from FreezeFrame
#define LED_BLINK_DUR_IN_MILLI 100 // LED blinking at 5 Hz, 100 milliseconds * 2 intervals
#define SEC_IN_MICRO 1000000.0     // constant: 1 second in microseconds
#define SERIAL_TIMEOUT 100000      // Serial timeout in milliseconds
#define TIME_SAMPLING_FREQ 100000  // GPIO input/output at 100k Hz

// Global variables and constants
// digital pin assignment
const int Sound_B_Cont[7] = {7, 8, 9, 10, 11, 12, 13};
const int Sound_B_Cont_OutMode[7] = {1, 1, 1, 1, 0, 1, 0};
const int Sound_B_VOL_Plus = Sound_B_Cont[0];
const int Sound_B_VOL_Minus = Sound_B_Cont[1];
const int Sound_B_UG = Sound_B_Cont[2];
const int Sound_B_RX = Sound_B_Cont[3];
const int Sound_B_TX = Sound_B_Cont[4];
const int Sound_B_RST = Sound_B_Cont[5];
const int Sound_B_ACT = Sound_B_Cont[6];

const int Button[2] = {30, 34};
const int Button_LED[2] = {32, 36};
const int Sound_B[8] = {38, 40, 42, 44, 46, 48, 50, 52};
// const int Camera[3] = {31, 33, 35};
const int pin_cam = 12;

// global variables
bool verboseStatus = true;

// camera
unsigned long cameraTime[3] = {micros() - 30000, micros() - 30000, micros() - 30000};
unsigned long cameraExposure = 20000;    // Default exposure time: 20 ms
unsigned long cameraExpInterval = 33333; // Default fps: 30
int camerasNumber = 1;
int cameraStatus = 0;
bool cameraBool[3] = {false, false, false};

int fps = 30;                            // Default fps: 30
float exposure = 0.02;                   // Default exposure time: 20 ms
int gain = 1;                            // Default gain: 1
float cycle_duration = 1.0 / float(fps); // Default cycle duration: 1/30 sec

unsigned long led_start = 0; // monitor elapsed time after LED starts blinking cycle
const int led = LED_BUILTIN; // LED pin
bool led_on = LOW;           // LED status

unsigned long fz_simulation_start = 0; // monitor elapsed time after simulation starts
bool fz_simulation = false;            // flag to indicate whether the simulation is running or not

// Create IntervalTimer object
//      https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
IntervalTimer int_timer;

void camera(int i)
{

    if (cameraBool[i] == true)
    {
        if ((micros() - cameraTime[i]) > cameraExposure)
        {
            digitalWrite(pin_cam, LOW); // turn the LED off by making the voltage LOW
            cameraBool[i] = false;
        }
    }
    else
    {
        if ((micros() - cameraTime[i]) > cameraExpInterval)
        {
            digitalWrite(pin_cam, HIGH); // turn the LED on (HIGH is the voltage level
            cameraBool[i] = true;
            cameraTime[i] = micros();
        }
    }
}

void set_cameraCallback(cmd *cmdPtr)
{
    Command cmd(cmdPtr);
    Argument arg = cmd.getArgument("start");
    bool _set_cameraStart = arg.isSet();
    if (_set_cameraStart)
    {
        cameraStatus = 1;
        // Serial.println(cameraStatus);
    }

    arg = cmd.getArgument("stop");
    bool _set_cameraStop = arg.isSet();
    if (_set_cameraStop)
    {
        cameraStatus = 0;
        // Serial.println(cameraStatus);
    }

    arg = cmd.getArgument("status");
    bool _set_cameraStatus = arg.isSet();
    if (_set_cameraStatus)
    {
        Serial.print("Camera status: ");
        Serial.print(cameraStatus);
        Serial.print(", number: ");
        Serial.print(camerasNumber);
        Serial.print(", fps: ");
        //    Serial.print(cameraExpInterval);
        //    Serial.print(" : ");
        Serial.print(1000000 / cameraExpInterval);
        Serial.print(", exposure: ");
        Serial.print(cameraExposure / 1000);
        Serial.println(" ms");
    }

    arg = cmd.getArgument("cameras_number");
    int _cameras_number = arg.getValue().toInt();
    if (_cameras_number != -1)
        camerasNumber = _cameras_number;

    arg = cmd.getArgument("fps");
    int _fps = arg.getValue().toInt();
    if (_fps != -1)
    {
        cameraExpInterval = long(1000000) / long(_fps);
        //    Serial.print("_fps: ");
        //    Serial.print(_fps);
        //    Serial.print(", cameraExpInterval: ");
        //    Serial.println(cameraExpInterval);
    }

    arg = cmd.getArgument("exposure");
    int _exposure = arg.getValue().toInt();
    if (_exposure != -1)
    {
        cameraExposure = long(_exposure) * long(1000);
        Serial.print("_exposure: ");
        Serial.print(_exposure);
        Serial.print(", cameraExposure: ");
        Serial.println(cameraExposure);
        if (cameraExposure > cameraExpInterval)
        {
            cameraExposure = cameraExpInterval;
            Serial.println("ERROR: cameraExposure = cameraExpInterval");
        }
    }
}

void errorCallback(cmd_error *errorPtr)
{
    CommandError e(errorPtr);

    Serial.println("ERROR: " + e.toString());

    if (e.hasCommand())
    {
        Serial.println("Did you mean? " + e.getCommand().toString());
    }
    else
    {
        Serial.println(cli.toString());
    }
}

void command_setup()
{
    // Register commands for Command line interface

    // Command "set_camera"
    cmdSetCamera = cli.addCommand("set_camera", set_cameraCallback);
    cmdSetCamera.addFlagArgument("start");
    cmdSetCamera.addFlagArgument("stop");
    cmdSetCamera.addFlagArgument("status");
    cmdSetCamera.addArgument("cameras_number", "-1");
    cmdSetCamera.addArgument("fps", "-1");
    cmdSetCamera.addArgument("exposure", "-1");

    // In case of error
    cli.setOnError(errorCallback);
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

        if (elapsed_time < cycle_duration)
        {
            if (elapsed_time < exposure)
            {
                digitalWrite(pin_cam, HIGH);
                digitalWrite(led, HIGH);
            }
            else
            {
                digitalWrite(pin_cam, LOW);
                digitalWrite(led, LOW);
            }
        }
        else
        {
            fz_simulation_start = micros();
        }
    }
    else
    {
        digitalWrite(pin_cam, LOW);
        // digitalWrite(led, LOW);
        led_blink();
    }
}

void setup()
{
    // initialize digital pins.
    // Cameras
    pinMode(led, OUTPUT);
    pinMode(pin_cam, OUTPUT);
    digitalWrite(pin_cam, LOW);
    pinMode(TRIG_IN, INPUT);

    // Initialize the USB serial connection with the PC at 115200 baud
    Serial.setTimeout(SERIAL_TIMEOUT);
    Serial.begin(115200);
    // Wait for the serial connection to be established
    delay(1000);

    // Initialize the command line interface
    command_setup();

    // Generate callback for the timer
    int timer_micros = 1000000 / TIME_SAMPLING_FREQ;
    int_timer.begin(interval_timer_callback, timer_micros);
}

void loop()
{

    // Command line interface
    if (Serial.available())
    {
        String input = Serial.readStringUntil('\n');
        if (verboseStatus)
        {
            Serial.print("> ");
            Serial.println(input);
        }
        cli.parse(input);

        // Serial.print("> ");
    }

    // Generate trigger for three cameras
    if (cameraStatus)
    {
        for (int i = 0; i < 3; i++)
        {
            camera(i); // currently 30 ms interval.
        }
    }

    // Generate sinusoidal LED driving signals
}
