#include "esp_wifi.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "storage.h"
#include "wifi.h"
#include "commands.h"
#include "constants.h"
#include "storage_keys.h"
#include "ethernet.h"
#include "uart_server.h"

const char *TAG = "SER2IP32";

void configure_uart(uart_port_t uartNum,
                    int bauds,
                    gpio_num_t tx_pin,
                    gpio_num_t rx_pin,
                    gpio_num_t rts_pin,
                    gpio_num_t cts_pin,
                    int buff_size_tx = UART_DEFAULT_BUFFER,
                    int buff_size_rx = UART_DEFAULT_BUFFER,
                    uart_word_length_t data_bits = UART_DEFAULT_DATA_BITS,
                    uart_parity_t parity = (uart_parity_t)UART_DEFAULT_PARITY,
                    uart_stop_bits_t stop_bits = UART_DEFAULT_STOP_BITS,
                    uart_hw_flowcontrol_t flow_control = UART_HW_FLOWCTRL_DISABLE)
{
  const uart_config_t uart_config = {
      .baud_rate = bauds,
      .data_bits = data_bits,
      .parity = parity,
      .stop_bits = stop_bits,
      .flow_ctrl = flow_control};
  uart_param_config(uartNum, &uart_config);
  uart_set_pin(uartNum, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(uartNum, buff_size_rx, buff_size_tx, 0, NULL, 0);
}

void start_wifi()
{
  // Wifi Mode
  int mode = WIFI_MODE_AP;
  if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_WIFI_MODE, &mode) != ESP_OK)
    mode = WIFI_MODE_AP; // AP

  // Wifi SSID
  char wifi_ssid[WIFI_SSID_MAX_LENGTH + 1];
  size_t ssid_length = WIFI_SSID_MAX_LENGTH + 1;
  if (storage::read_string(STORAGE_NAMESPACE, STORAGE_WIFI_SSID, wifi_ssid, &ssid_length) != ESP_OK)
    strlcpy(wifi_ssid, WIFI_AP_DEFAULT_SSID, WIFI_SSID_MAX_LENGTH + 1);

  // Wifi Passwd
  char wifi_passwd[WIFI_PASSWD_MAX_LENGTH + 1];
  size_t passwd_length = WIFI_PASSWD_MAX_LENGTH + 1;
  if (storage::read_string(STORAGE_NAMESPACE, STORAGE_WIFI_PASSWD, wifi_passwd, &passwd_length) != ESP_OK)
    strlcpy(wifi_passwd, WIFI_AP_DEFAULT_PASSWD, WIFI_PASSWD_MAX_LENGTH + 1);

  // Wifi channel
  int channel = WIFI_AP_DEFAULT_CHANNEL;
  if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_WIFI_CHANNEL, &channel) != ESP_OK)
    channel = WIFI_AP_DEFAULT_CHANNEL;

  // Connect Wifi
  if (mode == WIFI_MODE_AP)
    wifi::wifi_init_softap(wifi_ssid, wifi_passwd, 8, channel);
  else
    wifi::wifi_init_sta(wifi_ssid, wifi_passwd);
}

void start_uarts()
{
  asio::io_context io_context;
  uart_server *servers[3];

  for (int i = 0; i < 3; i++)
  {
    // Aux vars
    char STORAGE_KEY[50];

    // ENABLE
    int32_t enabled = 0;
    sprintf(STORAGE_KEY, STORAGE_UART_ENABLE, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &enabled) != ESP_OK)
      enabled = UART_DEFAULT_ENABLE[i];

    if (enabled == 0)
      continue;

    // Bauds
    int32_t bauds = 0;
    sprintf(STORAGE_KEY, STORAGE_UART_BAUDS, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &bauds) != ESP_OK)
      bauds = UART_DEFAULT_BAUDS;

    // TCP Port
    int32_t tcp_port = 0;
    sprintf(STORAGE_KEY, STORAGE_UART_TCP_PORT, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &tcp_port) != ESP_OK)
      tcp_port = UART_DEFAULT_TCP_PORT[i];

    // TX Pin
    gpio_num_t tx_pin;
    sprintf(STORAGE_KEY, STORAGE_UART_TX_PIN, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &tx_pin) != ESP_OK)
      tx_pin = UART_DEFAULT_TX_PIN[i];

    // RX Pin
    gpio_num_t rx_pin;
    sprintf(STORAGE_KEY, STORAGE_UART_RX_PIN, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &rx_pin) != ESP_OK)
      rx_pin = UART_DEFAULT_RX_PIN[i];

    // TX Buffer
    int32_t tx_buffer;
    sprintf(STORAGE_KEY, STORAGE_UART_TX_BUFFER, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &tx_buffer) != ESP_OK)
      tx_buffer = UART_DEFAULT_BUFFER;

    // RX Buffer
    int32_t rx_buffer = 0;
    sprintf(STORAGE_KEY, STORAGE_UART_RX_BUFFER, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &rx_buffer) != ESP_OK)
      rx_buffer = UART_DEFAULT_BUFFER;

    // Data bits
    uart_word_length_t data_bits;
    sprintf(STORAGE_KEY, STORAGE_UART_DATA_BITS, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &data_bits) != ESP_OK)
      data_bits = UART_DEFAULT_DATA_BITS;

    // Parity
    uart_parity_t parity;
    sprintf(STORAGE_KEY, STORAGE_UART_PARITY, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &parity) != ESP_OK)
      parity = UART_DEFAULT_PARITY;

    // Stop bits
    uart_stop_bits_t stop_bits;
    sprintf(STORAGE_KEY, STORAGE_UART_STOP_BITS, i);
    if (storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &stop_bits) != ESP_OK)
      stop_bits = UART_DEFAULT_STOP_BITS;

    gpio_num_t rts = UART_DEFAULT_RTS_PIN[i];
    gpio_num_t cts = UART_DEFAULT_CTS_PIN[i];

    ESP_LOGI("START_UART", "Uart N: %i, Enabled: %i, Bauds: %i, TCP: %d, "
      "TXPin: %i, RXPin: %i, TXBuff: %d, RXBuff: %d, DataBits: %i, Parity: %i, StopBits: %i", 
      i, enabled, bauds, tcp_port, tx_pin, rx_pin, tx_buffer, rx_buffer, data_bits, parity, stop_bits);
    configure_uart((uart_port_t)i, bauds, tx_pin, rx_pin, rts, cts, tx_buffer, rx_buffer, data_bits, (uart_parity_t)parity, stop_bits);
    ESP_LOGI("START_UART", "Server Uart N: %i", i);
    servers[i] = new uart_server(&io_context, tcp_port, (uart_port_t)i);
  }

  // Block here forever
  io_context.run();

  ESP_LOGI("start_uart", "exit");
}

extern "C" void app_main()
{
  // Initialize NVS
  storage::init_nvs();

  // Check button to enter console mode
  gpio_set_pull_mode(CONSOLE_ACTIVATE_PIN, GPIO_PULLUP_ONLY);
  vTaskDelay(1);
  bool enable_console = !gpio_get_level(CONSOLE_ACTIVATE_PIN);
  if (enable_console)
  {
    commands::start_console_task();
  }
  else
  {
    esp_log_level_set("*", ESP_LOG_NONE);
  }

  ESP_LOGI("MAIN", "Init");

  // Start diaplay
  vTaskDelay(pdMS_TO_TICKS(100));
  // Start Wifi
  start_wifi();
  // Start ethernet
  eth::init();
  // Start Uarts
  start_uarts(); //Never returns

  ESP_LOGI("MAIN", "Exit");
}