/*
Copyright 2020 Pavel Slama

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.

See the License for the specific language governing permissions and
limitations under the License.
*/
#include "mbed.h"
#include "trace.h"

using namespace std::chrono;

DigitalOut led(KL15_STATUS_LED_pin, 0);
EventQueue eQueue(32 * EVENTS_EVENT_SIZE);

#include "mdm.h"

int main() {
    trace_init();

    Thread eQueueThread;

    if (eQueueThread.start(callback(&eQueue, &EventQueue::dispatch_forever)) != osOK) {
        debug("eQueueThread error\n");
        return 0;
    }

    mdmSetup();

    while (1) {
        ThisThread::sleep_for(500ms);
        led = !led;

        if (server_done && sms_done) {
            mdmOff();
            break;
        }
    }

    debug("End\n");
    return 0;
}
