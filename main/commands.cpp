#include "commands.h"

#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "storage_keys.h"
#include "driver/uart.h"
#include <stdio.h>
#include <string.h>

#include "constants.h"

#define STORAGE_NAMESPACE "storage"

namespace commands
{
    static struct
    {
        struct arg_int *uart;
        struct arg_int *enable;
        struct arg_int *bauds;
        struct arg_int *tcp_port;
        struct arg_int *tx_pin;
        struct arg_int *rx_pin;
        struct arg_int *tx_buffer;
        struct arg_int *rx_buffer;
        struct arg_int *data_bits;
        struct arg_int *parity;
        struct arg_int *stop_bits;
        struct arg_end *end;
    } uart_args;

    static struct
    {
        struct arg_int *mode;
        struct arg_str *ssid;
        struct arg_str *password;
        struct arg_int *channel;
        struct arg_end *end;
    } wifi_args;

    static TaskHandle_t task_handle = NULL;

    static void register_commands();
    // UART
    static void register_uart_commands();
    static int uart_configure_command(int argc, char **argv);
    // Wifi
    static void register_wifi_commands();
    static int wifi_configure_command(int argc, char **argv);
    // Reboot
    static void register_reboot_command();
    static int reboot_command(int argc, char **argv);
    // Clear NVS
    static void register_clear_nvs_commands();
    static int clear_nvs_command(int argc, char **argv);
    void run_console(void *arg);

    static void initialize_console()
    {
        /* Disable buffering on stdin */
        setvbuf(stdin, NULL, _IONBF, 0);

        /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
        esp_vfs_dev_uart_port_set_rx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_CR);
        // esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
        /* Move the caret to the beginning of the next line on '\n' */
        esp_vfs_dev_uart_port_set_tx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_CRLF);
        // esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

        /* Configure UART. Note that REF_TICK is used so that the baud rate remains
         * correct while APB frequency is changing in light sleep mode.
         */
        const uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .use_ref_tick = true};
        ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));

        /* Install UART driver for interrupt-driven reads and writes */
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0,
                                            256, 0, 0, NULL, 0));

        /* Tell VFS to use UART driver */
        esp_vfs_dev_uart_use_driver(UART_NUM_0);

        /* Initialize the console */
        esp_console_config_t console_config = {
            .max_cmdline_length = 256,
            .max_cmdline_args = 12,
        };
        ESP_ERROR_CHECK(esp_console_init(&console_config));

        /* Configure linenoise line completion library */
        /* Enable multiline editing. If not set, long commands will scroll within
         * single line.
         */
        linenoiseSetMultiLine(1);

        /* Tell linenoise where to get command completions and hints */
        linenoiseSetCompletionCallback(&esp_console_get_completion);
        linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

        /* Set command history size */
        linenoiseHistorySetMaxLen(100);
    }
    void start_console_task()
    {
        xTaskCreate(&commands::run_console, "CONSOLE", 4096, NULL, configMAX_PRIORITIES, &task_handle);
    }
    void run_console(void *arg)
    {
        // Init Console
        initialize_console();
        // Help command
        esp_console_register_help_command();
        // Add Commands
        register_commands();

        const char *prompt = "Ser2IP32> ";

        printf("\n"
               "Welcome to Ser2IP32 configuration console\n"
               "Type 'help' to get the list of commands.\n"
               "Use UP/DOWN arrows to navigate through command history.\n"
               "Press TAB when typing command name to auto-complete.\n");

        /* Figure out if the terminal supports escape sequences */
        int probe_status = linenoiseProbe();
        if (probe_status)
        { /* zero indicates success */
            printf("\n"
                   "Your terminal application does not support escape sequences.\n"
                   "Line editing and history features are disabled.\n"
                   "On Windows, try using Putty instead.\n");
            linenoiseSetDumbMode(1);
        }

        /* Main loop */
        while (true)
        {
            /* Get a line using linenoise.
             * The line is returned when ENTER is pressed.
             */
            char *line = linenoise(prompt);
            if (line == NULL)
            { /* Ignore empty lines */
                continue;
            }
            /* Add the command to the history */
            linenoiseHistoryAdd(line);

            /* Try to run the command */
            int ret;
            esp_err_t err = esp_console_run(line, &ret);
            if (err == ESP_ERR_NOT_FOUND)
            {
                printf("Unrecognized command\n");
            }
            else if (err == ESP_ERR_INVALID_ARG)
            {
                // command was empty
            }
            else if (err == ESP_OK && ret != ESP_OK)
            {
                printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(err));
            }
            else if (err != ESP_OK)
            {
                printf("Internal error: %s\n", esp_err_to_name(err));
            }
            /* linenoise allocates line buffer on the heap, so need to free it */
            linenoiseFree(line);
        }
    }
    void register_commands()
    {
        register_uart_commands();
        register_wifi_commands();
        register_reboot_command();
        register_clear_nvs_commands();
    }

    // UART
    int uart_configure_command(int argc, char **argv)
    {
        int nerrors = arg_parse(argc, argv, (void **)&uart_args);
        if (nerrors != 0)
        {
            arg_print_errors(stderr, uart_args.end, argv[0]);
            return 1;
        }

        // Uart number
        int uart_num = uart_args.uart->ival[0];
        if (uart_num > 2)
        {
            printf("Uart number is invalid, please insert 0, 1 or 2");
            return 1;
        }

        // Aux vars
        char STORAGE_KEY[50];
        int32_t aux_int = 0;
        char aux_str[100];

        // ENABLE
        sprintf(STORAGE_KEY, STORAGE_UART_ENABLE, uart_num);
        if (uart_args.enable->count == 0)
        {
            uart_args.enable->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : UART_DEFAULT_ENABLE[uart_num];
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.enable->ival[0]);

        // BAUDS
        sprintf(STORAGE_KEY, STORAGE_UART_BAUDS, uart_num);
        if (uart_args.bauds->count == 0)
        {
            uart_args.bauds->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : UART_DEFAULT_BAUDS;
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.bauds->ival[0]);

        // TCP Port
        sprintf(STORAGE_KEY, STORAGE_UART_TCP_PORT, uart_num);
        if (uart_args.tcp_port->count == 0)
        {
            uart_args.tcp_port->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : UART_DEFAULT_TCP_PORT[uart_num];
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.tcp_port->ival[0]);

        // TX_PIN
        sprintf(STORAGE_KEY, STORAGE_UART_TX_PIN, uart_num);
        if (uart_args.tx_pin->count == 0)
        {
            uart_args.tx_pin->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : UART_DEFAULT_TX_PIN[uart_num];
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.tx_pin->ival[0]);

        // RX_PIN
        sprintf(STORAGE_KEY, STORAGE_UART_RX_PIN, uart_num);
        if (uart_args.rx_pin->count == 0)
        {
            uart_args.rx_pin->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : UART_DEFAULT_RX_PIN[uart_num];
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.rx_pin->ival[0]);

        // TX_BUFFER
        sprintf(STORAGE_KEY, STORAGE_UART_TX_BUFFER, uart_num);
        if (uart_args.tx_buffer->count == 0)
        {
            uart_args.tx_buffer->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : UART_DEFAULT_BUFFER;
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.tx_buffer->ival[0]);

        // RX_BUFFER
        sprintf(STORAGE_KEY, STORAGE_UART_RX_BUFFER, uart_num);
        if (uart_args.rx_buffer->count == 0)
        {
            uart_args.rx_buffer->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : UART_DEFAULT_BUFFER;
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.rx_buffer->ival[0]);

        // DATA_BITS
        sprintf(STORAGE_KEY, STORAGE_UART_DATA_BITS, uart_num);
        if (uart_args.data_bits->count == 0)
        {
            uart_args.data_bits->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : 8;
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.data_bits->ival[0]);

        // PARITY
        sprintf(STORAGE_KEY, STORAGE_UART_PARITY, uart_num);
        if (uart_args.parity->count == 0)
        {
            uart_args.parity->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : UART_PARITY_DISABLE;
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.parity->ival[0]);

        // STOP_BITS
        sprintf(STORAGE_KEY, STORAGE_UART_STOP_BITS, uart_num);
        if (uart_args.stop_bits->count == 0)
        {
            uart_args.stop_bits->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_KEY, &aux_int) == ESP_OK ? aux_int : 1;
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_KEY, uart_args.stop_bits->ival[0]);

        return 0;
    }

    void register_uart_commands()
    {
        uart_args.uart = arg_int1(NULL, NULL, "<0|1|2>", "Uart number, 0, 1, 2");
        uart_args.enable = arg_int1(NULL, NULL, "<enable=1|disable=0>", "Enable or disable port (enable)");
        uart_args.bauds = arg_int1(NULL, NULL, "<bauds>", "Baud rate (115200)");
        uart_args.tcp_port = arg_int0(NULL, "tcp_port", "<tcp_port>", "Listening TCP Port");
        uart_args.tx_pin = arg_int0(NULL, "tx_pin", "<tx_pin>", "TX Pin");
        uart_args.rx_pin = arg_int0(NULL, "rx_pin", "<rx_pin>", "RX Pin");
        uart_args.tx_buffer = arg_int0(NULL, "tx_buffer", "<tx_buffer>", "Transmission buffer in bytes (2048)");
        uart_args.rx_buffer = arg_int0(NULL, "rx_buffer", "<rx_buffer>", "Reception buffer in bytes (2048)");
        uart_args.data_bits = arg_int0(NULL, "data_bits", "<data_bits>", "Number of data bits (8)");
        uart_args.parity = arg_int0(NULL, "parity", "<odd=3|even=2|none=0>", "Parity (none)");
        uart_args.stop_bits = arg_int0(NULL, "stop_bits", "<stop_bits>", "Number of stop bits (1)");
        uart_args.end = arg_end(8);

        static esp_console_cmd_t uart_config_cmd = {
            .command = "uart_config",
            .help = "Set uart parameters",
            .hint = NULL,
            .func = &uart_configure_command,
            .argtable = &uart_args};

        esp_console_cmd_register(&uart_config_cmd);
    }

    // WIFI
    int wifi_configure_command(int argc, char **argv)
    {
        int nerrors = arg_parse(argc, argv, (void **)&wifi_args);
        if (nerrors != 0)
        {
            arg_print_errors(stderr, wifi_args.end, argv[0]);
            return 1;
        }

        if (strlen(wifi_args.password->sval[0]) < 8)
        {
            printf("Wifi password cannot be less than 8 chars");
            return 1;
        }

        int32_t aux_int = 0;

        // WIFI Mode
        if (wifi_args.mode->count == 0)
        {
            wifi_args.mode->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_WIFI_MODE, &aux_int) == ESP_OK ? aux_int : 0;
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_WIFI_MODE, wifi_args.mode->ival[0]);

        // SSID
        storage::write_string(STORAGE_NAMESPACE, STORAGE_WIFI_SSID, wifi_args.ssid->sval[0]);

        // Password
        storage::write_string(STORAGE_NAMESPACE, STORAGE_WIFI_PASSWD, wifi_args.password->sval[0]);

        // WIFI channel
        if (wifi_args.channel->count == 0)
        {
            wifi_args.channel->ival[0] = storage::read_int32(STORAGE_NAMESPACE, STORAGE_WIFI_MODE, &aux_int) == ESP_OK ? aux_int : 6;
        }
        storage::write_int32(STORAGE_NAMESPACE, STORAGE_WIFI_MODE, wifi_args.channel->ival[0]);

        return 0;
    }

    void register_wifi_commands()
    {
        wifi_args.mode = arg_int1(NULL, NULL, "<ap=0|station=1>", "Wifi mode. AP or Station(client)");
        wifi_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of the wifi network");
        wifi_args.password = arg_str1(NULL, NULL, "<passwd>", "Wifi password. If no password, put \"\"");
        wifi_args.channel = arg_int0(NULL, "channel", "<channel>", "Wifi channel in AP mode");
        wifi_args.end = arg_end(2);

        static esp_console_cmd_t wifi_config_cmd = {
            .command = "wifi_config",
            .help = "Set wifi parameters",
            .hint = NULL,
            .func = &wifi_configure_command,
            .argtable = &wifi_args};

        esp_console_cmd_register(&wifi_config_cmd);
    }

    // Reboot
    int reboot_command(int argc, char **argv)
    {
        esp_restart();
    }

    void register_reboot_command()
    {
        const esp_console_cmd_t cmd = {
            .command = "reboot",
            .help = "Reboot",
            .hint = NULL,
            .func = &reboot_command,
        };
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    }

    // Factory settings
    int clear_nvs_command(int argc, char **argv)
    {
        // Format NVS
        storage::format_nvs();

        // Reboot
        esp_restart();
    }

    void register_clear_nvs_commands()
    {
        const esp_console_cmd_t cmd = {
            .command = "factory",
            .help = "Restore factory settings. Board will reboot",
            .hint = NULL,
            .func = &clear_nvs_command,
        };
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    }
} // namespace commands
