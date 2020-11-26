#include "CellularContext.h"
#include "myModem.h"
#include "server.h"
#include "sms.h"

nsapi_connection_status_t connection_status = NSAPI_STATUS_DISCONNECTED;
uint8_t registration_status = CellularNetwork::StatusNotAvailable;
int mdm_connect_id = 0;

void mdmCb(nsapi_event_t type, intptr_t ptr);

void mdmConnect() {
    debug("MDM connect\n");
    nsapi_error_t ret = mdm->connect();

    debug("Connection: %i\n", ret);

    if (ret == NSAPI_ERROR_OK || ret == NSAPI_ERROR_IN_PROGRESS) {
        debug("Network connected\n");
        mdm_connect_id = 0;
        return;
    }

    if (ret == NSAPI_ERROR_NO_MEMORY) {
        debug("No memory, reseting MCU\n");
        NVIC_SystemReset();
    }

    debug("Connecting failed\n");
    mdm_connect_id = eQueue.call_in(5s, mdmConnect);

    if (!mdm_connect_id) {
        debug("Calling mdm connect failed, no memory\n");
    }
}

void mdmReconnect() {
    if (!mdm_connect_id) {
        debug("Reconnecting network\n");
        mdmConnect();

    } else {
        debug("mdm connect in progress\n");
    }
}

bool mdmSetup() {
    debug("Device setup\n");
    mdm = CellularContext::get_default_instance();

    if (mdm != nullptr) {
        mdm_device = mdm->get_device();

        if (mdm_device != nullptr) {
            mdm_device->hard_power_on();

            uint16_t timeout[7] = {1, 4, 8, 16, 32, 64, 128};
            mdm_device->set_retry_timeout_array(timeout, 7);

            mdm->set_credentials(MBED_CONF_APP_APN);

            mdm->attach(mdmCb);
            mdm->set_blocking(false);

#if defined(MBED_CONF_APP_SIM_PIN)
            mdm->set_sim_pin(MBED_CONF_APP_SIM_PIN);
#endif

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

void mdmOffHelper() {
    mdm->attach(nullptr);
    mdm->set_blocking(true);
    mdm->disconnect();
    mdm_device->shutdown();
}

void mdmOff() {
    debug("Turning OFF\n");
    mdmOffHelper();
}

void mdmReset() {
    debug("Reseting MDM\n");
    mdmOffHelper();
    mdm_device->hard_power_off();

    int qid = eQueue.call_in(5s, mdmSetup);

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
            if (registration_status != ptr_data->status_data && ptr_data->status_data != CellularNetwork::StatusNotAvailable) {
                debug("Registration: %i\n", ptr_data->status_data);
                registration_status = ptr_data->status_data;

                if (registration_status == CellularNetwork::RegisteredHomeNetwork ||
                        registration_status == CellularNetwork::RegisteredRoaming ||
                        registration_status == CellularNetwork::AlreadyRegistered) {
                    if (connection_status == NSAPI_STATUS_DISCONNECTED) {
                        debug("Disconnect\n");
                    }

                } else {
                    if (connection_status != NSAPI_STATUS_CONNECTING) {
                        mdmCb(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, NSAPI_STATUS_DISCONNECTED);
                    }
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
            if (ptr == NSAPI_STATUS_GLOBAL_UP) {
                int qid = eQueue.call(smsSetup);

                if (!qid) {
                    debug("Calling SMS failed, no memory\n");
                }

                qid = eQueue.call(serverReconnect);

                if (!qid) {
                    debug("Calling server connect failed, no memory\n");
                }


            }
        }

        connection_status = (nsapi_connection_status_t)ptr;
    }
}