#include "mbed.h"
#include "MQTTClient.h"
#include "MQTTEthernet.h"

#include <iostream>
#include <vector>
#include <algorithm>

#define NOTIFICATION_PAUSE_TIME 10 // One hour = 3600
#define BAUD 38400
#define WATER_SAMPLE_SIZE 50
#define WATER_CRITICAL 0.3
#define WATER_LOW 0.4
#define WATER_MEDIUM 0.45
#define WATER_HIGH 0.5

#define DEBUG_ 1
/*
* MQTT Stuffs are heavily *inspired* by the example linked to on moodle
*/
static Serial host (USBTX, USBRX);
// Interrupts
InterruptIn sw2_int (PTC6);
InterruptIn sw3_int (PTA4);

AnalogIn waterSensor(PTC10); // Water Sensor
PwmOut servo(PTC11); // Servo, 10 % power is left, 20 % power is right.

static volatile int sw2_trig = 0; // Button responsible for refilling food container
static volatile int sw3_trig = 0; // Button responsible for activating automatic mode that's not really implemented yet

int feed();
int notifyWaterStatus(Timer&);
int sw2_interrupt();
int sw3_interrupt();
int reportFoodRefill();
int reportToMQTT(int);
void messageReceived(MQTT::MessageData&);
int addWaterSampleGetSize();
float getWaterSampleMedian();

typedef struct Mqtt_params
{
    char* host;
    char* waterTopic;
    char* foodRefillTopic;
    char* clientId;
    int port;
} Mqtt_params;

static Mqtt_params mqtt_config =
{
    //host: "129.12.44.120",
    host: "192.168.1.11",
    waterTopic: "unikent/users/adjn2/iot/waterLevel",
    //foodRefillTopic: "unikent/users/adjn2/iot/foodCyclesSinceRefill",
    foodRefillTopic: "hello/foodRefill",
    clientId: "adjn2",
    port: 1883
};

/*
* Program to run a rabbit feeder / water bottle monitor
* Calculation on how much food is left is done in the website / webpanel, based on how many times feeding has been activated
* Relative to the volume size of the food container, feeding container and when the food container was last filled (reset).
* I will use ethernet connection for now since I don't have a wi-fi component :)
*/
std::vector<float> waterSamples;
float currentWaterLevel(1.0f); // Last best known water value.
bool timeToFeederino = false;
bool automaticMode = false; // Not implemented as of now.

MQTTEthernet ethernet;
MQTT::Client<MQTTEthernet, Countdown> client = MQTT::Client<MQTTEthernet, Countdown>(ethernet);    
MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

int main()
{
    host.baud(BAUD);
    //char* subTopic = "unikent/users/adjn2/iot/feedNow";
    char* subTopic = "hello/food";
    Timer spamTimer; // Makes sure atleast NOTIFICATION_PAUSE_TIME # of seconds has passed since the last low water notification.
    spamTimer.start();
    sw2_int.mode (PullUp);
    //sw2_int.fall (&sw2_interrupt); // Stopped working after changing mbed library // Ill investigate this later
    sw3_int.mode (PullUp);
    //sw3_int.fall (&sw3_interrupt);
    // Setup connection
    ethernet = MQTTEthernet();
    const char* ip;
    ip = ethernet.get_ip_address();
    #if DEBUG_ == 1
    std::cout << "IP is: " << ip << std::endl;
    #endif
    ethernet.open(ethernet.getEth());
    if(!ethernet.connect(mqtt_config.host, mqtt_config.port))
    {
        #if DEBUG_ == 1
        std::cout << "Successfully Connected Ethernet" << std::endl;
        Thread::wait(1000);
        #endif
    }
    #if DEBUG_ == 1
    else std::cout << "Ethernet failed to connecterino" << std::endl; // If this else happens we are doomed.
    Thread::wait(1000);
    #endif
    data.MQTTVersion = 3; // I guess this is the version of the MQTT server?
    data.clientID.cstring = mqtt_config.clientId;
    if(!client.connect(data))
    {
        #if DEBUG_ == 1
        std::cout << "Client successfully connected" << std::endl;
        Thread::wait(1000);
        #endif
    }
    #if DEBUG_ == 1
    else std::cout << "Client failed to connect" << std::endl; // If this else happens we are also doomed
    #endif
    std::cout << "Trying to subscribe" << std::endl;
    client.subscribe(subTopic, MQTT::QOS1, messageReceived);
    std::cout<< "Subscribed" << std::endl;
    while (true) // Program loop
    {
        if(!client.isConnected()) // Could have had a while here but maybe not so good if the future "automatic mode" is on.
        {
            #if DEBUG_ == 1
            std::cout << "The client lost the connection.. Attempting to reconnect." << std::endl;
            #endif
            client.connect(data);
        }
        client.yield(10);
        if(sw2_trig == 1)
        {
            sw2_trig = 0;
            automaticMode = true;
        }
        if(sw3_trig == 1)
        {
            sw3_trig = 0;
            if(reportFoodRefill() == -1) // Report refill of food container
                if(!client.connect(data)) reportFoodRefill(); // Try again..
        }
        if(addWaterSampleGetSize() >= WATER_SAMPLE_SIZE) // To ensure robustness of water level sensor
        {
            currentWaterLevel = getWaterSampleMedian();
            std::cout << "Current water level: " << currentWaterLevel << std::endl;
            waterSamples.clear();
        }
        if(timeToFeederino) feed();
        if((/* currentWaterLevel <= WATER_LOW && */spamTimer >= NOTIFICATION_PAUSE_TIME))
        {
            std::cout << "Water if entered" << std::endl;
            if(notifyWaterStatus(spamTimer) == -1)
                if(!client.connect(data)) notifyWaterStatus(spamTimer); // try again..
        }
        Thread::wait(500);
    }
}

int addWaterSampleGetSize()
{
    waterSamples.push_back(waterSensor.read());
    return waterSamples.size();
}

// I thought median was better in case both ends would have some kind of eratic values..
float getWaterSampleMedian()
{
    // Sort the vector
    std::sort(waterSamples.begin(), waterSamples.end()); // Its okay for the vector to remain sorted outside this function
    return waterSamples.at(waterSamples.size() / 2);
}

int feed() // Activate Servo motion and reset timeToFeederino bool.
{
    std::cout << "Feeding" << std::endl;
    servo.write(0.1f); // Rotate Servo Clock-wise.
    Thread::wait(3000); // Let's imagine it takes < 3000 ms to reach whatever will stop the servo from turning more
    servo.write(0.2f);
    Thread::wait(3000);
    timeToFeederino = false;
    return 0;
}

int notifyWaterStatus(Timer& spamTimer)
{
    // Tell our other client to send an email
    if(reportToMQTT(1) == -1) return -1;
    // Then reset our spamTimer :)
    spamTimer.reset();
    return 0;
}

int sw2_interrupt()
{
    return sw2_trig = 1; // Perform operation and report success status..
}


int sw3_interrupt()
{
    return sw3_trig = 1;
}

int reportFoodRefill()
{
    if(reportToMQTT(0) == -1) return -1;
    sw2_trig = 0;
    return 0;
}

void messageReceived(MQTT::MessageData& messageData)
{
    std::cout << "Message received" << std::endl;
    MQTT::Message& message = messageData.message;
    #if DEBUG_ == 1
    std::cout << "Message Received: " << message.payload << std::endl;
    #endif
    // The only MQTT message we listen for is the time to feed, since I decided to make automatic mode in the server side instead.
    // Just assume it is feed flag.
    timeToFeederino = true;
}

/*
* reportFlag of:
* 0 -> report that we refilled the food container
* 1 -> report water level
* 2 -> ???
* 3 -> ???
*/
int reportToMQTT(int reportFlag) // Reports messages about "reportFlag" to MQTT Server
{
    MQTT::Message message;
    char buffer[100]; // Allocates memory for 100 * sizeof(char)
    message.qos = MQTT::QOS1; // Best effort delivery of message
    message.retained = false; // I think this is something about not having to match the topic name perfectly..
    message.dup = false; // Probs about not wanting duplicate topics?
    int result;
    message.payload = (void*)buffer; // Bomb.
    message.payloadlen = strlen(buffer)+1;
    switch(reportFlag) // Cases have some bit of repetetiveness but that is for future clean up / future Alex to sort out.
    {
        case 0: // Food Refill Message
        // The other client receives this, and interacts with the database (for the webpanel).
        sprintf(buffer, "Blabla"); // blabla
        message.qos = MQTT::QOS1;
        result = client.publish(mqtt_config.foodRefillTopic, message);
        break;
        case 1: // Water Level Message
        std::cout << "Reporting Water Level" << std::endl;
        sprintf(buffer, "%.2f", currentWaterLevel); // Our other client will receive this, and send an email warning about this to the owner if low
        result = client.publish(mqtt_config.waterTopic, message);
        break;
    }
    if(!result) // Did publishing to server succeed?
    {    
        #if DEBUG_ == 1
        std::cout << "Success publishing to MQTT Server! :) " << std::endl;
        Thread::wait(1000);
        #endif
        return 0;
    }
    else
    {
        #if DEBUG_ == 1
        std::cout << "Failure publishing! :(" << std::endl;
        #endif
        return -1;
    }
}