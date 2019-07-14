#include <Arduino.h>
#include <bluefruit.h>

uint8_t Service_UUID[] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e};

uint8_t Write_UUID[] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e};

uint8_t Notify_UUID[] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e};

uint8_t ADV_COMPLETE_LOCAL_NAME[] = {
    'C',
    '1',
    ' ',
    'P',
    'l',
    'u',
    's',
};

static BLEClientService WearfitService(Service_UUID);
static BLEClientCharacteristic WearfitNotify(Notify_UUID);
static BLEClientCharacteristic WearfitWrite(Write_UUID);
static bool WearfitAlive = false;

struct Wearfit
{
    static void printReport(const ble_gap_evt_adv_report_t *report)
    {
        Serial.print("  rssi: ");
        Serial.println(report->rssi);
        Serial.print("  scan_rsp: ");
        Serial.println(report->type.scan_response);
        // Serial.print("  type: ");
        // Serial.println(report->type);
        Serial.print("  dlen: ");
        Serial.println(report->data.len);
        Serial.print("  data: ");
        for (int i = 0; i < report->data.len; i += sizeof(uint8_t))
        {
            Serial.printf("%02X-", report->data.p_data[i]);
        }
        Serial.println("");
    }

    static void printHexList(uint8_t *buffer, uint8_t len)
    {
        // print forward order
        for (int i = 0; i < len; i++)
        {
            Serial.printf("%02X-", buffer[i]);
        }
        Serial.println();
    }

    static void disconnect_callback(uint16_t conn_handle, uint8_t reason)
    {
        (void)conn_handle;
        (void)reason;
        WearfitAlive = false;
        Serial.println("Disconnected");
    }

    static void connect_callback(uint16_t conn_handle)
    {
        Serial.println("");
        Serial.print("Connect Callback, conn_handle: ");
        Serial.println(conn_handle);

        /* Complete Local Name */
        uint8_t buffer[BLE_GAP_ADV_SET_DATA_SIZE_MAX] = {0};

        // If Service is not found, disconnect and return
        Serial.print("Discovering Wearfit Service ... ");
        if (!WearfitService.discover(conn_handle))
        {
            Serial.println("No Service Found");

            // disconect since we couldn't find service
            Bluefruit.disconnect(conn_handle);

            return;
        }
        Serial.println("Service Found");

        // Once service is found, we continue to discover the primary characteristic
        Serial.print("Discovering WearfitNotify Characteristic ... ");
        if (!WearfitNotify.discover())
        {
            // Measurement chr is mandatory, if it is not found (valid), then disconnect
            Serial.println("No Characteristic Found. Characteristic is mandatory but not found. ");
            Bluefruit.disconnect(conn_handle);
            return;
        }
        Serial.println("Characteristic Found");

        Serial.print("Enabling Notify on the Characteristic ... ");
        if (WearfitNotify.enableNotify())
        {
            Serial.println("enableNotify success, now ready to receive Characteristic values");
        }
        else
        {
            Serial.println("Couldn't enable notify for Characteristic. Increase DEBUG LEVEL for troubleshooting");
        }

        Serial.print("Discovering WearfitWrite Characteristic ... ");
        if (!WearfitWrite.discover())
        {
            Serial.println("No Characteristic Found. Characteristic is mandatory but not found.");
            Bluefruit.disconnect(conn_handle);
            return;
        }
        else
        {
            set_ring_shake();
            WearfitAlive = true;
        }
        Serial.println("Characteristic Found");
    }

    static void scan_callback(ble_gap_evt_adv_report_t *report)
    {
        Serial.println("");
        Serial.println("Scan Callback");
        printReport(report);

        uint8_t buffer[BLE_GAP_ADV_SET_DATA_SIZE_MAX] = {0};

        /* Display the timestamp and device address */
        if (report->type.scan_response)
        {
            Serial.printf("[SR%10d] Packet received from ", millis());
        }
        else
        {
            Serial.printf("[ADV%9d] Packet received from ", millis());
        }
        // MAC is in little endian --> print reverse
        Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
        Serial.print("\n");

        /* Raw buffer contents */
        Serial.printf("%14s %d bytes\n", "PAYLOAD", report->data.len);
        if (report->data.len)
        {
            Serial.printf("%15s", " ");
            Serial.printBuffer(report->data.p_data, report->data.len, '-');
            Serial.println();
        }

        /* RSSI value */
        Serial.printf("%14s %d dBm\n", "RSSI", report->rssi);

        /* Adv Type */
        Serial.printf("%14s ", "ADV TYPE");
        if (report->type.connectable)
        {
            Serial.print("Connectable ");
        }
        else
        {
            Serial.print("Non-connectable ");
        }

        if (report->type.directed)
        {
            Serial.println("directed");
        }
        else
        {
            Serial.println("undirected");
        }

        // 此处做设备地址的绑定和判断 4B:FC:1B:0C:77:99 // F6:37:07:D8:2A:F8
        Serial.printBuffer(report->peer_addr.addr, 6, '-'), Serial.println(" mac\n");
        if (0 == memcmp((const char *)report->peer_addr.addr, "\xF8\x2A\xD8\x07\x37\xF6", 6)) // F8-2A-D8-07-37-F6
        {
            Serial.print("Parsing report for Local Name ... ");
            if (Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer)))
            {
                Serial.println("Found Local Name");
                Serial.printf("%14s %s\n", "Local Name:", buffer);

                Serial.print("   Local Name data: ");
                printHexList(buffer, BLE_GAP_ADV_SET_DATA_SIZE_MAX);

                Serial.print("Determining Local Name Match ... ");
                if (!memcmp(buffer, ADV_COMPLETE_LOCAL_NAME, sizeof(ADV_COMPLETE_LOCAL_NAME)))
                {
                    Serial.println("Local Name Match!");

                    Serial.println("Connecting to Peripheral ... ");
                    Bluefruit.Central.connect(report);
                }
                else
                {
                    Serial.println("No Match");
                    Bluefruit.Scanner.resume(); // continue scanning
                }
            }
        }
        else
        {
            Serial.println("Failed");

            // For Softdevice v6: after received a report, scanner will be paused
            // We need to call Scanner resume() to continue scanning
            Bluefruit.Scanner.resume();
        }
    }

    void setup()
    {
        Serial.setPins(26, 25);
        Serial.begin(115200);
        while (!Serial)
            delay(10); // for nrf52840 with native usb

        Serial.println("Zhiwu Wearfit Central");
        Bluefruit.begin(0, 1);
        Bluefruit.setName("Wearfit Central");

        // Initialize Service
        WearfitService.begin();

        // set up callback for receiving measurement
        WearfitNotify.setNotifyCallback(Wearfit::notify_callback);
        WearfitNotify.begin();

        // set up the characteristic to enable the optical component on the device
        WearfitWrite.begin();

        // Increase Blink rate to different from PrPh advertising mode
        Bluefruit.setConnLedInterval(250);

        // Callbacks for Central
        Bluefruit.Central.setDisconnectCallback(Wearfit::disconnect_callback);
        Bluefruit.Central.setConnectCallback(Wearfit::connect_callback);

        Bluefruit.Scanner.setRxCallback(Wearfit::scan_callback);
        Bluefruit.Scanner.restartOnDisconnect(true);
        Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
        // Bluefruit.Scanner.filterUuid();      // do not not set anything here or Scan Response won't work
        Bluefruit.Scanner.useActiveScan(true); // required for SensorTag to reveal the Local Name in the advertisement.
        Bluefruit.Scanner.start(0);            // 0 = Don't stop scanning after n seconds

        Scheduler.startLoop(unit_test);
    }

    static void notify_callback(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
    {
        Serial.print("Optical data: ");
        // Serial.printf("Optical len %d data: %.*s\n", len, len, data);
        printHexList(data, len);

        recv_forward(data, len);
    }

    static void recv_forward(uint8_t *buffer, uint8_t len)
    {
        if (buffer[4] == 0x31)
        {
            if (buffer[5] == 0x0A)
            {
                Serial.printf("H:%02X\n", buffer[6]); // 心率
            }
        }
        if (buffer[4] == 0x91)
        {
            Serial.printf("B:%02X\n", buffer[7]); // 电量
        }
    }

    static void get_heart_rate()
    {
        WearfitWrite.write("\xAB\x00\x04\xFF\x31\x0A\x01", 7);
        Serial.write("get_heart_rate\n");
    }

    // static void get_battery()
    // {
    //     WearfitWrite.write("\xAB\x00\x03\xFF\xB2\x80", 6);
    //     Serial.write("get_battery\n");
    // }

    static void set_ring_shake()
    {
        WearfitWrite.write("\xAB\x00\x04\xFF\xB1\x80\x01", 7);
        Serial.write("set_ring_shake\n");
    }

    static void set_notice(char *info)
    {
        char buf[30] = "\xAB\x00\x05\xFF\x72\x80\x03\x02"; // 8
        // WearfitWrite.write("\xAB\x00\x05\xFF\x72\x80\x09\x01", 8); // 0
        sprintf(buf + 8, "%s", info);
        buf[2] = buf[2] + strlen(info);
        WearfitWrite.write(buf, 8 + strlen(info)); // 0
        // printHexList((uint8_t *)buf, 8 + strlen(info));
        Serial.write("set_notice\n");
    }

    static void unit_test()
    {
        delay(10000);
        // set_notice("test");
        set_notice("开始测量");
        if (WearfitAlive)
        {
            // WearfitWrite.write("\xAB\x00\x04\xFF\xB1\x80\x01", 7);

            // WearfitWrite.write("\xAB\x00\x04\xFF\x32\x80\x01", 7);

            //set_notice("开始测量");

            // {

            //     static int count = 0, state = 0;
            //     tm += 5;
            //     switch (state)
            //     {
            //         case 0:
            //             WearfitWrite.write("\xAB\x00\x04\xFF\x32\x80\x01", 7); // 启动测量
            //             state = 1;
            //             break;

            //         case 1:
            //             if (tm == 60)
            //             {
            //                 WearfitWrite.write("\xAB\x00\x04\xFF\x32\x80\x00", 7); // 停止测量
            //                 state = 0;
            //             }
            //             break;

            //     }
            // }
        }
        // WearfitWrite.write("\xAB\x00\x04\xFF\x31\x09\x01", 7);
    }

    void loop()
    {
        if (Serial.available())
        {
            // Serial.write(Serial.read());

            if ('T' == Serial.read())
            {
                WearfitWrite.write("\xAB\x00\x04\xFF\xB1\x80\x01", 7);
            }

            if ('B' == Serial.read())
            {
                WearfitWrite.write("\xAB\x00\x03\xFF\xB2\x80", 6);
            }

            if ('F' == Serial.read())
            {
                WearfitWrite.write("\xAB\x00\x03\xFF\x71\x80\x00", 7);
            }

            if ('S' == Serial.read())
            {
                WearfitWrite.write("\xAB\x00\x04\xFF\x31\x22\x01", 7);
            }

            if ('G' == Serial.read())
            {
                WearfitWrite.write("\xAB\x00\x04\xFF\x31\x22\x01", 7);
            }
        }
    }
};
