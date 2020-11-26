#include "UBLOX_AT.h"
#include "CellularContext.h"

DigitalOut lt(PB_1, 1);
CellularContext *mdm;
CellularDevice *mdm_device;

#define MDM_HW_FLOW

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
    pwrOn(MDM_PWRON_pin, 0),
    rst(MDM_RST_pin, 0) {
}

nsapi_error_t myUblox::hard_power_on() {
    rst.write(1);
    ThisThread::sleep_for(100ms);
    rst.write(0);
    ThisThread::sleep_for(100ms);
    return NSAPI_ERROR_OK;
}

nsapi_error_t myUblox::hard_power_off() {
    rst.write(1);

    return NSAPI_ERROR_OK;
}

nsapi_error_t myUblox::soft_power_on() {
    pwrOn.write(1);
    ThisThread::sleep_for(150ms);
    pwrOn.write(0);
    ThisThread::sleep_for(1s);

    return NSAPI_ERROR_OK;
}

nsapi_error_t myUblox::soft_power_off() {
    _at.lock();
    _at.cmd_start("AT+CPWROFF");
    _at.cmd_stop();

    nsapi_error_t ret = _at.unlock_return_error();

    ThisThread::sleep_for(100ms);
    return ret;
}

CellularDevice *CellularDevice::get_target_default_instance() {
    static BufferedSerial serial(MDM_TX_pin, MDM_RX_pin, 115200);

#if defined(MDM_HW_FLOW)
    serial.set_flow_control(SerialBase::RTSCTS, MDM_RTS_pin, MDM_CTS_pin);
#else
    static DigitalOut rts(MDM_RTS_pin, 0);
#endif

    static myUblox device(&serial);
    return &device;
}