#include "CellularSMS.h"

bool sms_done = false;

void smsRead() {
    char sms_buf[SMS_MAX_SIZE_GSM7_SINGLE_SMS_SIZE];  // for raw receive and send
    char sms_buf_lower[sizeof(sms_buf)];
    char num[SMS_MAX_PHONE_NUMBER_SIZE];

    char timestamp[SMS_MAX_TIME_STAMP_SIZE];
    int overflow = 0;

    CellularSMS *sms;
    sms = mdm_device->open_sms();

    if (sms) {
        while (true) {
            nsapi_size_or_error_t sms_len = sms->get_sms(
                                                sms_buf, sizeof(sms_buf),
                                                num, sizeof(num),
                                                timestamp, sizeof(timestamp),
                                                &overflow);

            if (sms_len < NSAPI_ERROR_OK) {
                // debug("SMS error: %i\n", sms_len);
                return;
            }

            for (uint8_t i = 0; i < sizeof(sms_buf); i++) {
                sms_buf_lower[i] = tolower(sms_buf[i]);
            }

            // Trim trailing spaces, CR, LF
            sms_len = strlen(sms_buf_lower);

            while (sms_buf_lower[sms_len - 1] == ' ' ||    // space
                    sms_buf_lower[sms_len - 1] == 0x0A ||  // CR
                    sms_buf_lower[sms_len - 1] == 0x0D) {  // LF
                sms_buf_lower[sms_len - 1] = 0;
                sms_len = strlen(sms_buf_lower);
            }

            debug("Got SMS from \"%s\" with text (%u) \"%s\"\n", num, sms_len, sms_buf_lower);
        }
    }
}

void smsSetup() {
    if (!sms_done) {
        LOG_DEBUG_MDM("SMS setup\n");

        CellularSMS *sms;
        sms = mdm_device->open_sms();

        if (sms) {
            if (sms->initialize(CellularSMS::CellularSMSMmodeText) == NSAPI_ERROR_OK) {
                sms->set_sms_callback(eQueue.event(callback(smsRead)));

                // if (sms->set_cpms("ME", "ME", "ME") == NSAPI_ERROR_OK) {
                debug("SMS setup OK\n");
                sms_done = true;
                smsRead();
                // }

            } else {
                debug("SMS init failed\n");
            }

        } else {
            debug("SMS open failed\n");
        }

        int qid = eQueue.call_in(5000, smsSetup);

        if (!qid) {
            debug("Calling SMS failed, no memory\n");
        }
    }
}
