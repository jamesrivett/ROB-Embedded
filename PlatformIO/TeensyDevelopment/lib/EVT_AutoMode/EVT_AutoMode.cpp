
#include <SPI.h>
#include <sstream>
#include "EVT_VescDriver.h"
#include "EVT_ODriver.h"
#include "EVT_Ethernet.h"

// IN AUTO MODE, THERE IS NO REVERSE. REVERSE BRAKES IN THIS CASE.

// Global variables for UDP control data
float erpm = 0.0;
float throttle = 0.0;
float steering = 0.0;
float brakeCurrent = 0.0;
float throttleRpm = 0.0;
float SteeringPos = 0.0;
bool emergency = false;
bool brakeState = false;
bool coasting = false;

// JRIVETT: A single global instance for control variables is probably fine, but you could put them in a struct to keep them together
struct ControlPacket {
    float erpm = 0.0;
    float throttle = 0.0;
    float steering = 0.0;
    float brakeCurrent = 0.0;
    float throttleRpm = 0.0;
    float SteeringPos = 0.0;
    bool emergency = false;
    bool brakeState = false;
    bool coasting = false;
} controlPacket;


// Function to parse UDP data and update control variables
// Expected format: "throttle,steering,emergency"
// JRIVETT: now you can pass any packet in as a pointer
void setControls(const std::string &udpData, ControlPacket* packet) {
    // Copy string to modifiable buffer
    char udpCopy[128];
    // JRIVETT: if you're giong to use strncpy, you need to make sure you're only copying over the distance of the smallest buffer.
    //          otherwise, udpCopy might be bigger than udpData, and strncpy might read right out-of-bounds on udpData.
    //          of course, this is only if udpData isn't null terminated. this is *actually very likely* since it's literally UDP data though. 
    strncpy(udpCopy, udpData.c_str(), std::min(sizeof(udpCopy),sizeof(udpData)) - 1);
    // JRIVETT: See? you even stick a null on the end of it in case you didn't get one off the data coming in.
    udpCopy[sizeof(udpCopy) - 1] = '\0';  // Ensure null termination

    char* token = strtok(udpCopy, ",");
    int index = 0;

    while (token != nullptr) {
        switch (index) {
            case 0:
                // JRIVETT: and set the value of whatever packet was passed.
                //          this scales up better because it's now agnostic to where the packet came from.
                packet->throttle = atof(token);
                break;
            case 1:
                packet->steering = atof(token);
                break;
            case 2:
                packet->emergency = (atoi(token) != 0);
                break;
        }

        index++;
        token = strtok(nullptr, ",");
    }

    if (index < 3) {
        Serial.print("Malformed control packet (expected 3 fields): ");
        Serial.println(udpData.c_str());
    }

}

// JRIVETT: you can also do the same thing in these functions
void CtrlVesc(ControlPacket* packet) {
if (packet->emergency == true) {
        vesc1.setBrakeCurrent(20.0f); // set to max brake current
        return;
}
      if (packet->throttle < 0.0f) {
        packet->brakeCurrent = packet->throttle *-1.0f / 5; // brake current = throttle value divided by 5. if max throttle is -100 then max brake current is 20A for now. val can be changed
        packet->brakeState = true;
        vesc1.setBrakeCurrent(packet->brakeCurrent);
        return;
    }

    else if (packet->throttle == 0.0f) {
        packet->coasting = true;  
        vesc1.setBrakeCurrent(0.0f);
        vesc1.setCurrent(0.0f);
        return;
    }

    else if (packet->throttle > 0.0f) {
        packet->throttleRpm = (throttle / 100.0f) * 7500.0f; // map throttle 0-100 to 0 - maxRPM (7500 for old vescrpm,14800 new theoretical vesc)
        packet->brakeState = false;
        packet->coasting = false;
        vesc1.setRPM(packet->throttleRpm);
        return;
    }


}

// JRIVETT: Not gonna do it in these other functions but you get the point
void CtrlOdrive() {
    // Map steering (-100 to +100) to ODrive position range (-maxPos to +maxPos)
    if (steering <-0.25f){
        SteeringPos = (steering / 100.0f) * 2.25f; // map steering -100 to 0 to -maxPos to 0
    }
    else if (steering > 0.25f) {
        SteeringPos = (steering / 100.0f) * 2.25f; // map steering 0 to +100 to 0 to +maxPos
    }
    else {
        SteeringPos = 0.0f; // center position
    }
        
        odrive.trapezoidalMove(SteeringPos);
        
}


void updateAutonomousMode() {
    // Set autonomous mode debug message.
    odrvDebug = "Autonomous mode active.";
    
    std::string rawCommands = receiveUdp();

        Serial.print(" | Throttle(rpm): ");
        Serial.print(throttleRpm);
        Serial.print(" steering(turns): ");
        Serial.print(SteeringPos);
        Serial.print(" | Emergency: ");
        Serial.println(emergency ? "YES" : "NO");
        Serial.println(brakeState);
        Serial.println(coasting);


    CtrlVesc(&controlPacket);
    CtrlOdrive();
    sendTelemetry();

    if (!rawCommands.empty()) {
        // JRIVETT: Just pass the address of the one we created up top, but you could totally call this elsewhere 
        //          with a control packet from a different source. That's the whole idea behind this.
        setControls(rawCommands, &controlPacket);
    }

}