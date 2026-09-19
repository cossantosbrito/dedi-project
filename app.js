// =====================================================
// MQTT CONFIGURATION
// =====================================================

// HiveMQ Cloud WebSocket URL
const MQTT_HOST = "YOUR_HIVEMQ_WEBSOCKET_HOST";

const MQTT_PORT = 8884;

const MQTT_USERNAME = "YOUR_MQTT_USERNAME";

const MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";


// MQTT topic
const MQTT_TOPIC = "coldroom/temperature";


// =====================================================
// MQTT CONNECTION
// =====================================================

const client = mqtt.connect(
    `wss://${MQTT_HOST}:${MQTT_PORT}/mqtt`,
    {
        username: MQTT_USERNAME,

        password: MQTT_PASSWORD,

        clientId:
            "web-dashboard-" +
            Math.random()
                .toString(16)
                .substring(2),

        clean: true,

        connectTimeout: 10000,

        reconnectPeriod: 5000
    }
);


// =====================================================
// HTML ELEMENTS
// =====================================================

const mqttStatus =
    document.getElementById(
        "mqttStatus"
    );


const temperatureElement =
    document.getElementById(
        "temperature"
    );


const deviceElement =
    document.getElementById(
        "device"
    );


const topicElement =
    document.getElementById(
        "topic"
    );


const lastUpdateElement =
    document.getElementById(
        "lastUpdate"
    );


const messageElement =
    document.getElementById(
        "message"
    );


// =====================================================
// MQTT CONNECTED
// =====================================================

client.on(
    "connect",
    function () {

        console.log(
            "Connected to MQTT broker"
        );


        mqttStatus.textContent =
            "ONLINE";


        mqttStatus.classList.remove(
            "offline"
        );


        mqttStatus.classList.add(
            "online"
        );


        // Subscribe
        client.subscribe(
            MQTT_TOPIC,
            function (error) {

                if (error) {

                    console.error(
                        "Subscribe error:",
                        error
                    );

                    return;
                }


                console.log(
                    "Subscribed to:",
                    MQTT_TOPIC
                );

            }
        );

    }
);


// =====================================================
// MQTT MESSAGE
// =====================================================

client.on(
    "message",
    function (
        topic,
        message
    ) {

        console.log(
            "Message received:",
            message.toString()
        );


        try {

            // Convert MQTT message
            // to JSON

            const data =
                JSON.parse(
                    message.toString()
                );


            // Temperature

            temperatureElement.textContent =
                Number(
                    data.temperature
                ).toFixed(2)
                + " °C";


            // Device

            deviceElement.textContent =
                data.device || "--";


            // Topic

            topicElement.textContent =
                topic;


            // Current time

            const now =
                new Date();


            lastUpdateElement.textContent =
                now.toLocaleString();


            // Show raw JSON

            messageElement.textContent =
                JSON.stringify(
                    data,
                    null,
                    2
                );


        }

        catch (error) {

            console.error(
                "JSON parsing error:",
                error
            );

        }

    }
);


// =====================================================
// MQTT ERROR
// =====================================================

client.on(
    "error",
    function (error) {

        console.error(
            "MQTT error:",
            error
        );

        mqttStatus.textContent =
            "ERROR";

    }
);


// =====================================================
// MQTT OFFLINE
// =====================================================

client.on(
    "offline",
    function () {

        console.log(
            "MQTT offline"
        );


        mqttStatus.textContent =
            "OFFLINE";


        mqttStatus.classList.remove(
            "online"
        );


        mqttStatus.classList.add(
            "offline"
        );

    }
);


// =====================================================
// MQTT RECONNECT
// =====================================================

client.on(
    "reconnect",
    function () {

        console.log(
            "Trying to reconnect..."
        );

        mqttStatus.textContent =
            "RECONNECTING";

    }
);