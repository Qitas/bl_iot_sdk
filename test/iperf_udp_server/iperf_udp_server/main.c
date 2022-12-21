#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <vfs.h>
#include <aos/kernel.h>
#include <aos/yloop.h>
#include <event_device.h>
#include <cli.h>
#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/tcp.h>
#include <lwip/err.h>
#include <http_client.h>
#include <netutils/netutils.h>
#include <bl602_glb.h>
#include <bl602_hbn.h>
#include <bl_uart.h>
#include <bl_chip.h>
#include <bl_wifi.h>
#include <hal_wifi.h>
#include <bl_sec.h>
#include <bl_cks.h>
#include <bl_irq.h>
#include <bl_dma.h>
#include <bl_timer.h>
#include <hal_sys.h>
#include <hal_gpio.h>
#include <hal_boot2.h>
#include <bl_sys_time.h>
#include <bl_sys.h>
#include <wifi_mgmr_ext.h>


#define STA_SSID        "outdoor"
#define STA_PASSWORD    "12345678"
#define IPERF_IP_LOCAL  "192.168.0.102"

// static int g_wifi_sta_is_connected = 0;
static wifi_interface_t wifi_interface = NULL;


extern void iperf_server_udp_entry(const char *name);
extern void iperf_client_tcp_entry(const char *name);

/* TODO: const */
volatile uint32_t uxTopUsedPriority __attribute__((used)) =  configMAX_PRIORITIES - 1;


static void wifi_sta_connect(char *ssid, char *password)
{
    wifi_interface_t wifi_interface;
    wifi_interface = wifi_mgmr_sta_enable();
    wifi_mgmr_sta_connect(wifi_interface, ssid, password, NULL, NULL, 0, 0);
}

static void event_cb_wifi_event(input_event_t *event, void *private_data)
{
    static char *ssid;
    static char *password;
    struct sm_connect_tlv_desc* ele = NULL;
    static wifi_conf_t conf =
    {
        .country_code = "CN",
    };

    switch (event->code) {
        case CODE_WIFI_ON_INIT_DONE:
        {
            printf("[APP] [EVT] INIT DONE %lld\r\n", aos_now_ms());
            wifi_mgmr_start_background(&conf);
        }
        break;
        case CODE_WIFI_ON_MGMR_DONE:
        {
            printf("[APP] [EVT] MGMR DONE %lld, now %lums\r\n", aos_now_ms(), bl_timer_now_us()/1000);
            wifi_interface = wifi_mgmr_sta_enable();
            wifi_mgmr_sta_connect(wifi_interface, STA_SSID, STA_PASSWORD, NULL, NULL, 0, 0);
        }
        break;
        case CODE_WIFI_ON_MGMR_DENOISE:
        {
            printf("[APP] [EVT] Microwave Denoise is ON %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_SCAN_DONE:
        {
            printf("[APP] [EVT] SCAN Done %lld\r\n", aos_now_ms());
            wifi_mgmr_cli_scanlist();
        }
        break;
        case CODE_WIFI_ON_SCAN_DONE_ONJOIN:
        {
            printf("[APP] [EVT] SCAN On Join %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_DISCONNECT:
        {
            printf("[APP] [EVT] disconnect %lld, Reason: %s\r\n",
                aos_now_ms(),
                wifi_mgmr_status_code_str(event->value)
            );
#if 1
            /* Get every ele in diagnose tlv data */
            while ((ele = wifi_mgmr_diagnose_tlv_get_ele()))
            {
                printf("[APP] [EVT] diagnose tlv data %lld, id: %d, len: %d, data: %p\r\n", aos_now_ms(), 
                    ele->id, ele->len, ele->data);

                /* MUST free diagnose tlv data ele */
                wifi_mgmr_diagnose_tlv_free_ele(ele);
            }

            printf("[SYS] Memory left is %d KBytes\r\n", xPortGetFreeHeapSize()/1024);
#endif
        }
        break;
        case CODE_WIFI_ON_CONNECTING:
        {
            printf("[APP] [EVT] Connecting %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_CMD_RECONNECT:
        {
            printf("[APP] [EVT] Reconnect %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_CONNECTED:
        {
#if 1
            /* Get every ele in diagnose tlv data */
            while ((ele = wifi_mgmr_diagnose_tlv_get_ele()))
            {
                printf("[APP] [EVT] diagnose tlv data %lld, id: %d, len: %d, data: %p\r\n", aos_now_ms(), 
                    ele->id, ele->len, ele->data);

                /* MUST free diagnose tlv data */
                wifi_mgmr_diagnose_tlv_free_ele(ele);
            }

            printf("[SYS] Memory left is %d KBytes\r\n", xPortGetFreeHeapSize()/1024);
#endif
            printf("[APP] [EVT] connected %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_PRE_GOT_IP:
        {
            printf("[APP] [EVT] connected %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_GOT_IP:
        {
            wifi_mgmr_sta_autoconnect_enable();
            printf("[APP] [EVT] GOT IP %lld\r\n", aos_now_ms());
            printf("[SYS] Memory left is %d KBytes\r\n", xPortGetFreeHeapSize()/1024);
            iperf_server_udp_entry(IPERF_IP_LOCAL);
            // iperf_client_tcp_entry(IPERF_IP_LOCAL);
        }
        break;
        case CODE_WIFI_ON_EMERGENCY_MAC:
        {
            printf("[APP] [EVT] EMERGENCY MAC %lld\r\n", aos_now_ms());
            hal_reboot();//one way of handling emergency is reboot. Maybe we should also consider solutions
        }
        break;
        case CODE_WIFI_ON_PROV_SSID:
        {
            printf("[APP] [EVT] [PROV] [SSID] %lld: %s\r\n",
                    aos_now_ms(),
                    event->value ? (const char*)event->value : "UNKNOWN"
            );
            if (ssid) {
                vPortFree(ssid);
                ssid = NULL;
            }
            ssid = (char*)event->value;
        }
        break;
        case CODE_WIFI_ON_PROV_BSSID:
        {
            printf("[APP] [EVT] [PROV] [BSSID] %lld: %s\r\n",
                    aos_now_ms(),
                    event->value ? (const char*)event->value : "UNKNOWN"
            );
            if (event->value) {
                vPortFree((void*)event->value);
            }
        }
        break;
        case CODE_WIFI_ON_PROV_PASSWD:
        {
            printf("[APP] [EVT] [PROV] [PASSWD] %lld: %s\r\n", aos_now_ms(),
                    event->value ? (const char*)event->value : "UNKNOWN"
            );
            if (password) {
                vPortFree(password);
                password = NULL;
            }
            password = (char*)event->value;
        }
        break;
        case CODE_WIFI_ON_PROV_CONNECT:
        {
            // printf("[APP] [EVT] [PROV] [CONNECT] %lld\r\n", aos_now_ms());
            // printf("connecting to %s:%s...\r\n", ssid, password);
            wifi_sta_connect(STA_SSID, STA_PASSWORD);
            // wifi_sta_connect_static_ip(STA_STATIC_IP);
        }
        break;
        case CODE_WIFI_ON_PROV_DISCONNECT:
        {
            printf("[APP] [EVT] [PROV] [DISCONNECT] %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_AP_STA_ADD:
        {
            printf("[APP] [EVT] [AP] [ADD] %lld, sta idx is %lu\r\n", aos_now_ms(), (uint32_t)event->value);
        }
        break;
        case CODE_WIFI_ON_AP_STA_DEL:
        {
            printf("[APP] [EVT] [AP] [DEL] %lld, sta idx is %lu\r\n", aos_now_ms(), (uint32_t)event->value);
        }
        break;
        default:
        {
            printf("[APP] [EVT] Unknown code %u, %lld\r\n", event->code, aos_now_ms());
            /*nothing*/
        }
    }
}


static void cmd_stack_wifi(char *buf, int len, int argc, char **argv)
{
    /*wifi fw stack and thread stuff*/
    static uint8_t stack_wifi_init  = 0;
    if (1 == stack_wifi_init) {
        puts("Wi-Fi Stack Started already!!!\r\n");
        return;
    }
    stack_wifi_init = 1;

    printf("Start Wi-Fi fw @%lums\r\n", bl_timer_now_us()/1000);
    hal_wifi_start_firmware_task();
    /*Trigger to start Wi-Fi*/
    printf("Start Wi-Fi fw is Done @%lums\r\n", bl_timer_now_us()/1000);
    aos_post_event(EV_WIFI, CODE_WIFI_ON_INIT_DONE, 0);

}


static void proc_main_entry(void *pvParameters)
{
    aos_register_event_filter(EV_WIFI, event_cb_wifi_event, NULL);
    cmd_stack_wifi(NULL, 0, 0, NULL);
    vTaskDelete(NULL);
}


void main(void)
{
    bl_sys_init();
    xTaskCreate(proc_main_entry, (char*)"main_entry", 1024, NULL, 15, NULL);
    tcpip_init(NULL, NULL);
}
