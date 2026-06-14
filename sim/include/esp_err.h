#ifndef DESKCLOCK_SIM_ESP_ERR_H
#define DESKCLOCK_SIM_ESP_ERR_H

using esp_err_t = int;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_WIFI_INIT_STATE 0x3001

#define ESP_ERROR_CHECK_WITHOUT_ABORT(expr) do { (void)(expr); } while (0)
#define ESP_ERROR_CHECK(expr) do { (void)(expr); } while (0)

#endif /* DESKCLOCK_SIM_ESP_ERR_H */
