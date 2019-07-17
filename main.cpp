/*
Copyright 2019 Pavel Slama

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
#include "UBLOX_AT.h"
#include "CellularContext.h"

#if MBED_CONF_MBED_TRACE_ENABLE
    #include "CellularLog.h"
#endif

EventQueue eQueue(32 * EVENTS_EVENT_SIZE);
CellularContext *mdm;
CellularDevice *mdm_device;

int mdm_connect_id = 0;
int server_connect_id = 0;
nsapi_connection_status_t connection_status = NSAPI_STATUS_DISCONNECTED;

bool  mdmSetup();

class myUblox : public UBLOX_AT {
  public:
    explicit myUblox(FileHandle *fh);

    virtual nsapi_error_t hard_power_on();
    virtual nsapi_error_t hard_power_off();
    virtual nsapi_error_t soft_power_on();
    virtual nsapi_error_t soft_power_off();

  private:
    DigitalOut pwrOn;
    DigitalOut rst;
};

myUblox::myUblox(FileHandle *fh) :
    UBLOX_AT(fh),
    pwrOn(MDM_PRWON_pin, 0),
    rst(MDM_RST_pin, 1) {
}

nsapi_error_t myUblox::hard_power_on() {
    rst.write(0);
    ThisThread::sleep_for(1000);
    return NSAPI_ERROR_OK;
}

nsapi_error_t myUblox::hard_power_off() {
    rst.write(1);

    return NSAPI_ERROR_OK;
}

nsapi_error_t myUblox::soft_power_on() {
    pwrOn.write(1);
    ThisThread::sleep_for(150);
    pwrOn.write(0);
    ThisThread::sleep_for(100);

    return NSAPI_ERROR_OK;
}

nsapi_error_t myUblox::soft_power_off() {
    pwrOn.write(1);
    ThisThread::sleep_for(1000);
    pwrOn.write(0);

    return NSAPI_ERROR_OK;
}

CellularDevice *CellularDevice::get_target_default_instance() {
    static UARTSerial serial(MDM_TX_pin, MDM_RX_pin, 115200);

#if defined(MDM_HW_FLOW)
    serial.set_flow_control(SerialBase::RTSCTS, MDM_RTS_pin, MDM_CTS_pin);
#else
    static DigitalOut rts(MDM_RTS_pin, 0);
#endif

    static myUblox device(&serial);
    return &device;
}

void serverDisconnect() {
    debug("Server disconnected\n");
}

bool serverConnect() {
    debug("Server connect\n");
    server_connect_id = 0;
    return true;
}

void serverReconnect() {
    if (!server_connect_id) {
        debug("Reconnecting server\n");
        bool status = serverConnect();

        if (!status) {
            debug("Reconnecting failed\n");
            server_connect_id = eQueue.call_in(5000, serverConnect);

            if (!server_connect_id) {
                debug("Calling server connect failed, no memory\n");
            }
        }

    } else {
        debug("server connect in progress\n");
    }
}

void mdmDisconnect() {
    debug("Disconnecting from network\n");
    mdm->disconnect();
}

bool mdmConnect() {
    debug("MDM connect\n");
    nsapi_error_t ret = mdm->connect();

    if (ret == NSAPI_ERROR_OK || ret == NSAPI_ERROR_IN_PROGRESS) {
        mdm_connect_id = 0;
        return true;
    }

    debug("Setup connect error: %i\n", ret);

    if (ret == NSAPI_ERROR_NO_MEMORY) {
        debug("No memory, reseting MCU\n");
        NVIC_SystemReset();
    }

    return false;
}

void mdmReconnect() {
    if (!mdm_connect_id) {
        debug("Reconnecting network\n");
        bool status = mdmConnect();

        if (!status) {
            debug("Reconnecting failed\n");
            mdm_connect_id = eQueue.call_in(5000, mdmConnect);

            if (!mdm_connect_id) {
                debug("Calling mdm connect failed, no memory\n");
            }
        }

    } else {
        debug("mdm connect in progress\n");
    }
}

void mdmHardOff() {
    mdm_device->soft_power_off();
    mdm_device->hard_power_off();
    // mdm_device->stop();
}

void mdmOffHelper() {
    mdm->attach(NULL);
    mdmDisconnect();
    mdm_device->shutdown();
}

void mdmOff() {
    debug("Turning OFF\n");
    mdmOffHelper();

    ATHandler *at_cmd = mdm_device->get_at_handler();
    at_cmd->lock();
    at_cmd->cmd_start("AT+CPWROFF");
    at_cmd->cmd_stop();
    at_cmd->unlock();
    mdm_device->release_at_handler(at_cmd);

    int qid = eQueue.call_in(5000, mdmHardOff);

    if (!qid) {
        debug("Could turn off mdm\n");
    }
}

void mdmReset() {
    debug("Reseting MDM\n");
    mdmOffHelper();
    mdmHardOff();

    int qid = eQueue.call_in(5000, mdmSetup);

    if (!qid) {
        debug("Could not setup mdm\n");
    }
}

void mdmCb(nsapi_event_t type, intptr_t ptr) {
    if (type >= NSAPI_EVENT_CELLULAR_STATUS_BASE && type <= NSAPI_EVENT_CELLULAR_STATUS_END) {
        cell_callback_data_t *ptr_data = reinterpret_cast<cell_callback_data_t *>(ptr);
        cellular_connection_status_t event = (cellular_connection_status_t)type;

        if (ptr_data->final_try && ptr_data->error == NSAPI_ERROR_OK) {
            debug("Final_try\n");
            eQueue.call(mdmReset);
        }

        if (event == CellularDeviceReady && ptr_data->error == NSAPI_ERROR_OK) {
            debug("DeviceReady\n");

        } else if (event == CellularSIMStatusChanged && ptr_data->error == NSAPI_ERROR_OK) {
            debug("SIM: %i\n", static_cast<int>(ptr_data->status_data));

        } else if (event == CellularRegistrationStatusChanged && ptr_data->error == NSAPI_ERROR_OK) {
            debug("Registration: %i\n", ptr_data->status_data);

            if (ptr_data->status_data == CellularNetwork::RegisteredHomeNetwork ||
                    ptr_data->status_data == CellularNetwork::RegisteredRoaming ||
                    ptr_data->status_data == CellularNetwork::AlreadyRegistered) {
                debug("blink led\n");

                if (connection_status == NSAPI_STATUS_DISCONNECTED) {
                    int qid = eQueue.call(mdmReconnect);

                    if (!qid) {
                        debug("Calling mdm connect failed, no memory\n");
                    }
                }

            } else {
                debug("blink off\n");

                if (connection_status != NSAPI_STATUS_CONNECTING) {
                    mdmCb(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, NSAPI_STATUS_DISCONNECTED);
                }
            }

        } else if (event == CellularAttachNetwork && ptr_data->error == NSAPI_ERROR_OK) {
            if (ptr_data->status_data == CellularNetwork::Attached) {
                debug("Attached\n");

            } else {
                debug("Dettached\n");
            }

        } else if (event == CellularStateRetryEvent && ptr_data->error == NSAPI_ERROR_OK) {
            int retry = *(const int *)ptr_data->data;
            debug("Retry count: %i\n", retry);

        } else if (event == CellularCellIDChanged && ptr_data->error == NSAPI_ERROR_OK) {
            debug("Cell ID changed: %i\n", static_cast<int>(ptr_data->status_data));

        } else if (event == CellularRegistrationTypeChanged && ptr_data->error == NSAPI_ERROR_OK) {
            debug("RegistrationType changed: %i\n", static_cast<int>(ptr_data->status_data));
        }

    } else if (type == NSAPI_EVENT_CONNECTION_STATUS_CHANGE) {
        debug("Connection status: %i\n", ptr);

        if (connection_status != ptr) {
            int qid = -1;

            if (ptr == NSAPI_STATUS_GLOBAL_UP) {
                qid = eQueue.call(serverReconnect);

            } else if (ptr == NSAPI_STATUS_DISCONNECTED) {
                qid = eQueue.call(serverDisconnect);
            }

            if (!qid) {
                debug("Calling server connect failed, no memory\n");
            }
        }

        connection_status = (nsapi_connection_status_t)ptr;
    }
}

bool mdmSetup() {
    debug("Device setup\n");
    mdm = CellularContext::get_default_instance();

    if (mdm != NULL) {
        mdm_device = mdm->get_device();

        if (mdm_device != NULL) {
            mdm_device->hard_power_on();

            uint16_t timeout[8] = {1, 2, 4, 8, 16, 32, 64, 128};
            mdm_device->set_retry_timeout_array(timeout, 5);

            mdm->set_credentials(MBED_CONF_APP_APN);

#if defined(MBED_CONF_APP_SIM_PIN)
            mdm->set_sim_pin(MBED_CONF_APP_SIM_PIN);
#endif

            mdm->attach(&mdmCb);
            mdm->set_blocking(false);

            mdmReconnect();

            return true;

        } else {
            debug("No interface\n");
        }

    } else {
        debug("No device\n");
    }

    return false;
}


#if MBED_CONF_MBED_TRACE_ENABLE
static Mutex trace_mutex;

static void trace_wait() {
    trace_mutex.lock();
}

static void trace_release() {
    trace_mutex.unlock();
}

void trace_init() {
    mbed_trace_init();

    mbed_trace_mutex_wait_function_set(trace_wait);
    mbed_trace_mutex_release_function_set(trace_release);

    mbed_cellular_trace::mutex_wait_function_set(trace_wait);
    mbed_cellular_trace::mutex_release_function_set(trace_release);
}
#endif

int main() {
#if MBED_CONF_MBED_TRACE_ENABLE
    trace_init();
#endif

    Thread eQueueThread;

    if (eQueueThread.start(callback(&eQueue, &EventQueue::dispatch_forever)) != osOK) {
        debug("eQueueThread error\n");
    }

    mdmSetup();

    while (1) {

    }
}
