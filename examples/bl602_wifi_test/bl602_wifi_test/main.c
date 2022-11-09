/*
 * Copyright (c) 2016-2022 Bouffalolab.
 *
 * This file is part of
 *     *** Bouffalolab Software Dev Kit ***
 *      (see www.bouffalolab.com).
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of Bouffalo Lab nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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
#include <bl_gpio_cli.h>
#include <bl_wdt_cli.h>
#include <hosal_uart.h>
#include <hosal_adc.h>
#include <hal_sys.h>
#include <hal_gpio.h>
#include <hal_boot2.h>
#include <hal_board.h>
#include <looprt.h>
#include <loopset.h>
#include <sntp.h>
#include <bl_sys_time.h>
#include <bl_sys.h>
#include <bl_sys_ota.h>
#include <bl_romfs.h>
#include <fdt.h>
#include <device/vfs_uart.h>
#include <easyflash.h>
#include <bl60x_fw_api.h>
#include <wifi_mgmr_ext.h>
#include <utils_log.h>
#include <libfdt.h>
#include <blog.h>
#include <bl_wps.h>

#define WIFI_AP_PSM_INFO_SSID           "iperf"
#define WIFI_AP_PSM_INFO_PASSWORD       "12345678"

#define STA_SSID "iperf"
#define STA_PASSWORD "12345678"
#define STA_MAX_CONN_cCOUNT 15
#define STA_STATIC_IP   "192.168.0.100"
typedef struct
{
    uint32_t ip;
    uint32_t gateway;
    uint32_t netmask;
} wifi_ip_params_t;
#define IP_SET_ADDR(a, b, c, d) (((uint32_t)((a)&0xff)) |       \
                                 ((uint32_t)((b)&0xff) << 8) |  \
                                 ((uint32_t)((c)&0xff) << 16) | \
                                 ((uint32_t)((d)&0xff) << 24))


static wifi_interface_t g_wifi_sta_interface = NULL;
static int g_wifi_sta_is_connected = 0;

static wifi_interface_t wifi_interface;
#define IPERF_IP_LOCAL      "0.0.0.0"
extern void iperf_server_udp_entry(const char *name);

extern void ble_stack_start(void);
/* TODO: const */
volatile uint32_t uxTopUsedPriority __attribute__((used)) =  configMAX_PRIORITIES - 1;

static wifi_conf_t conf =
{
    .country_code = "CN",
};


int check_dts_config(char ssid[33], char password[64])
{
    bl_wifi_ap_info_t sta_info;

    if (bl_wifi_sta_info_get(&sta_info)) {
        /*no valid sta info is got*/
        return -1;
    }

    strncpy(ssid, (const char*)sta_info.ssid, 32);
    ssid[31] = '\0';
    strncpy(password, (const char*)sta_info.psk, 64);
    password[63] = '\0';

    return 0;
}

static void wifi_sta_connect(char *ssid, char *password)
{
    wifi_interface_t wifi_interface;
    wifi_interface = wifi_mgmr_sta_enable();
    wifi_mgmr_sta_connect(wifi_interface, ssid, password, NULL, NULL, 0, 0);
}
wifi_ip_params_t sta_ip_params = {0};

int wifi_ip_connect(const char *sta_ip)
{
    struct ap_connect_adv ext_param = {0};
    g_wifi_sta_interface = wifi_mgmr_sta_enable();
    int freq;
    uint8_t ip[4] = {0}, i = 0, j = 0;
    char *temp_arg = (char *)calloc(1, 6);
    int state = WIFI_STATE_IDLE;

    memset(ip, 0, sizeof(ip));
    memset(&sta_ip_params, 0, sizeof(sta_ip_params));
    memset(temp_arg, 0, sizeof(temp_arg));

    for (int i = 0; i < 4; i++)
    {
        temp_arg = sta_ip;
        while (*(sta_ip++) != '.')
        {
            j++;
        }
        temp_arg[j] = '\0';
        ip[i] = atoi(temp_arg);
        j = 0;
    }

    sta_ip_params.ip = IP_SET_ADDR(ip[0], ip[1], ip[2], ip[3]);
    sta_ip_params.gateway = IP_SET_ADDR(ip[0], ip[1], ip[2], 1);
    sta_ip_params.netmask = IP_SET_ADDR(255, 255, 255, 0);

    ext_param.psk = NULL;
    ext_param.ap_info.type = AP_INFO_TYPE_PRESIST;
    ext_param.ap_info.time_to_live = 5;
    ext_param.ap_info.band = 0;

    if (g_wifi_sta_is_connected == 1)
    {
        printf("sta has connect\r\n");
        return 0;
    }
    else
    {
        wifi_mgmr_sta_autoconnect_enable();
        wifi_mgmr_sta_ip_set(sta_ip_params.ip, sta_ip_params.netmask, sta_ip_params.gateway, sta_ip_params.gateway, 0);
        printf("connect to wifi %s\r\n", STA_SSID);
        return wifi_mgmr_sta_connect_ext(g_wifi_sta_interface, STA_SSID, STA_PASSWORD, &ext_param);
    }
    free(temp_arg);
}

static void event_cb_wifi_event(input_event_t *event, void *private_data)
{
    static char *ssid;
    static char *password;
    struct sm_connect_tlv_desc* ele = NULL;

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
            wifi_mgmr_sta_connect(wifi_interface, WIFI_AP_PSM_INFO_SSID, WIFI_AP_PSM_INFO_PASSWORD, NULL, NULL, 0, 0);
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
            // ipus_test_cmd();
            iperf_server_udp_entry(IPERF_IP_LOCAL);
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
            printf("[APP] [EVT] [PROV] [CONNECT] %lld\r\n", aos_now_ms());
            printf("connecting to %s:%s...\r\n", ssid, password);
            // wifi_sta_connect(ssid, password);
            wifi_ip_connect(STA_STATIC_IP);
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

static void cmd_pka(char *buf, int len, int argc, char **argv)
{
    bl_pka_test();
}

static void cmd_wifi(char *buf, int len, int argc, char **argv)
{
void mm_sec_keydump(void);
    mm_sec_keydump();
}

static void cmd_sha(char *buf, int len, int argc, char **argv)
{
    bl_sec_sha_test();
}

static void cmd_trng(char *buf, int len, int argc, char **argv)
{
    bl_sec_test();
}

static void cmd_aes(char *buf, int len, int argc, char **argv)
{
    bl_sec_aes_test();
}

static void cmd_cks(char *buf, int len, int argc, char **argv)
{
    bl_cks_test();
}

static void cmd_dma(char *buf, int len, int argc, char **argv)
{
    bl_dma_test();
}

static void cmd_exception_load(char *buf, int len, int argc, char **argv)
{
    bl_irq_exception_trigger(BL_IRQ_EXCEPTION_TYPE_LOAD_MISALIGN, (void*)0x22008001);
}

static void cmd_exception_l_illegal(char *buf, int len, int argc, char **argv)
{
    bl_irq_exception_trigger(BL_IRQ_EXCEPTION_TYPE_ACCESS_ILLEGAL, (void*)0x00200000);
}

static void cmd_exception_store(char *buf, int len, int argc, char **argv)
{
    bl_irq_exception_trigger(BL_IRQ_EXCEPTION_TYPE_STORE_MISALIGN, (void*)0x22008001);
}

static void cmd_exception_illegal_ins(char *buf, int len, int argc, char **argv)
{
    bl_irq_exception_trigger(BL_IRQ_EXCEPTION_TYPE_ILLEGAL_INSTRUCTION, (void*)0x22008001);
}

#define MAXBUF          128
#define BUFFER_SIZE     (12*1024)

#define PORT 80

static int client_demo(char *hostname)
{
    int sockfd;
    /* Get host address from the input name */
    struct hostent *hostinfo = gethostbyname(hostname);
    uint8_t *recv_buffer;

    if (!hostinfo) {
        printf("gethostbyname Failed\r\n");
        return -1;
    }

    struct sockaddr_in dest;

    char buffer[MAXBUF];
    /* Create a socket */
    /*---Open socket for streaming---*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error in socket\r\n");
        return -1;
    }

    /*---Initialize server address/port struct---*/
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    dest.sin_addr = *((struct in_addr *) hostinfo->h_addr);
//    char ip[16];
    uint32_t address = dest.sin_addr.s_addr;
    char *ip = inet_ntoa(address);

    printf("Server ip Address : %s\r\n", ip);
    /*---Connect to server---*/
    if (connect(sockfd,
             (struct sockaddr *)&dest,
             sizeof(dest)) != 0) {
        printf("Error in connect\r\n");
        return -1;
    }
    /*---Get "Hello?"---*/
    memset(buffer, 0, MAXBUF);
    char wbuf[]
        = "GET /ddm/ContentResource/music/204.mp3 HTTP/1.1\r\nHost: nf.cr.dandanman.com\r\nUser-Agent: wmsdk\r\nAccept: */*\r\n\r\n";
    write(sockfd, wbuf, sizeof(wbuf) - 1);

    int ret = 0;
    int total = 0;
    int debug_counter = 0;
    uint32_t ticks_start, ticks_end, time_consumed;

    ticks_start = xTaskGetTickCount();
    recv_buffer = pvPortMalloc(BUFFER_SIZE);
    if (NULL == recv_buffer) {
        goto out;
    }
    while (1) {
        ret = read(sockfd, recv_buffer, BUFFER_SIZE);
        if (ret == 0) {
            printf("eof\n\r");
            break;
        } else if (ret < 0) {
            printf("ret = %d, err = %d\n\r", ret, errno);
            break;
        } else {
            total += ret;
            /*use less debug*/
            if (0 == ((debug_counter++) & 0xFF)) {
                printf("total = %d, ret = %d\n\r", total, ret);
            }
            //vTaskDelay(2);
            if (total > 82050000) {
                ticks_end = xTaskGetTickCount();
                time_consumed = ((uint32_t)(((int32_t)ticks_end) - ((int32_t)ticks_start))) / 1000;
                printf("Download comlete, total time %u s, speed %u Kbps\r\n",
                        (unsigned int)time_consumed,
                        (unsigned int)(total / time_consumed * 8 / 1000)
                );
                break;
            }
        }
    }

    vPortFree(recv_buffer);
out:
    close(sockfd);
    return 0;
}

static void http_test_cmd(char *buf, int len, int argc, char **argv)
{
    // http://nf.cr.dandanman.com/ddm/ContentResource/music/204.mp3
    client_demo("nf.cr.dandanman.com");
}

static void cb_httpc_result(void *arg, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err)
{
    httpc_state_t **req = (httpc_state_t**)arg;

    printf("[HTTPC] Transfer finished. rx_content_len is %lu\r\n", rx_content_len);
    *req = NULL;
}

err_t cb_httpc_headers_done_fn(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len)
{
    printf("[HTTPC] hdr_len is %u, content_len is %lu\r\n", hdr_len, content_len);
    return ERR_OK;
}

static err_t cb_altcp_recv_fn(void *arg, struct altcp_pcb *conn, struct pbuf *p, err_t err)
{
    //printf("[HTTPC] Received %u Bytes\r\n", p->tot_len);
    static int count = 0;

    puts(".");
    if (0 == ((count++) & 0x3F)) {
        puts("\r\n");
    }
    altcp_recved(conn, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

static void cmd_httpc_test(char *buf, int len, int argc, char **argv)
{
    static httpc_connection_t settings;
    static httpc_state_t *req;

    if (req) {
        printf("[CLI] req is on-going...\r\n");
        return;
    }
    memset(&settings, 0, sizeof(settings));
    settings.use_proxy = 0;
    settings.result_fn = cb_httpc_result;
    settings.headers_done_fn = cb_httpc_headers_done_fn;

    httpc_get_file_dns(
            "nf.cr.dandanman.com",
            80,
            "/ddm/ContentResource/music/204.mp3",
            &settings,
            cb_altcp_recv_fn,
            &req,
            &req
   );
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

#define wps_dbg(fmt, ...) printf("[WPS] " fmt "\r\n", ##__VA_ARGS__)
#define WPS_PBC_CMD "wps-pbc"
#define WPS_PIN_CMD "wps-pin"
static void wps_event_callback_(bl_wps_event_t event, void *payload, void *cb_arg)
{
    bl_wps_ap_credential_t *ap_cred;
    bl_wps_pin_t *pin;
    wifi_interface_t wifi_interface;

    switch (event) {
    case BL_WPS_EVENT_PIN:
        pin = (bl_wps_pin_t *)payload;
        wps_dbg("PIN %s", pin->pin);
        vPortFree(pin);
        break;
    case BL_WPS_EVENT_COMPLETE:
        wps_dbg("completed");
        ap_cred = (bl_wps_ap_credential_t *)payload;
        // ap_cred should always be valid
        wps_dbg("AP SSID %s", ap_cred->ssid);
        wps_dbg("AP passphrase %s", ap_cred->passphrase);
        wps_dbg("connecting...");
        wifi_interface = wifi_mgmr_sta_enable();
        wifi_mgmr_sta_connect(wifi_interface, (char *)ap_cred->ssid, ap_cred->passphrase, NULL,  NULL, 0, 0);
        vPortFree(ap_cred);
        break;
    case BL_WPS_EVENT_TIMEOUT:
        wps_dbg("timed out");
        break;
    default:
        wps_dbg("error occured, event: %d", event);
    }
}

static void cmd_wps_pbc_(char *buf, int len, int argc, char **argv)
{
    wps_type_t type;
    if (!strcmp(argv[0], WPS_PBC_CMD)) {
        type = WPS_TYPE_PBC;
    } else {
        type = WPS_TYPE_PIN;
    }
    const struct bl_wps_config config = {
        .type = type,
        .event_cb = wps_event_callback_,
    };

    bl_wps_err_t ret = bl_wifi_wps_start(&config);
    if (ret != BL_WPS_ERR_OK) {
        wps_dbg("bl_wifi_wps_start failed with code %d", (int)ret);
    }
}

#include <utils_log.h>
static void cmd_pmk(char *buf, int len, int argc, char **argv)
{
int utils_wifi_psk_cal_fast(char *password, char *ssid, int ssid_len, char *output);
int pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len,
		int iterations, uint8_t *buf, size_t buflen);
    const char *password = "12345678";
    const char *ssid = "testme";
    char output[65];

#if 0
    memset(output, 0, sizeof(output));
    utils_wifi_psk_cal_fast(password, ssid, strlen(ssid), output);
    printf("PMK: %s\r\n", output);
#endif

    memset(output, 0, sizeof(output));
    pbkdf2_sha1(password, ssid, strlen(ssid), 4096, (void *)output, 32);
    log_buf(output, 32);
}

const static struct cli_command cmds_user[] STATIC_CLI_CMD_ATTRIBUTE = {
        // { "aws", "aws iot demo", cmd_aws},
        { "pka", "pka iot demo", cmd_pka},
        { "wifi", "wifi", cmd_wifi},
        { "sha", "sha iot demo", cmd_sha},
        { "trng", "trng test", cmd_trng},
        { "aes", "trng test", cmd_aes},
        { "cks", "cks test", cmd_cks},
        { "dma", "dma test", cmd_dma},
        { "exception_load", "exception load test", cmd_exception_load},
        { "exception_l_illegal", "exception load test", cmd_exception_l_illegal},
        { "exception_store", "exception store test", cmd_exception_store},
        { "exception_inst_illegal", "exception illegal instruction", cmd_exception_illegal_ins},
        /*Stack Command*/
        { "stack_wifi", "Wi-Fi Stack", cmd_stack_wifi},
        /*TCP/IP network test*/
        {"http", "http client download test based on socket", http_test_cmd},
        {"httpc", "http client download test based on RAW TCP", cmd_httpc_test},
        {"pmk", "http client download test based on RAW TCP", cmd_pmk},
        {WPS_PBC_CMD, "WPS Push Button demo", cmd_wps_pbc_},
        {WPS_PIN_CMD, "WPS Device PIN demo", cmd_wps_pbc_},
};

static void _cli_init()
{
    /*Put CLI which needs to be init here*/
int codex_debug_cli_init(void);
    codex_debug_cli_init();
    easyflash_cli_init();
    network_netutils_iperf_cli_register();
    network_netutils_tcpserver_cli_register();
    network_netutils_tcpclinet_cli_register();
    network_netutils_netstat_cli_register();
    network_netutils_ping_cli_register();
    sntp_cli_init();
    bl_sys_time_cli_init();
    bl_sys_ota_cli_init();
    blfdt_cli_init();
    wifi_mgmr_cli_init();
    bl_wdt_cli_init();
    bl_gpio_cli_init();
    looprt_test_cli_init();
}

static void proc_main_entry(void *pvParameters)
{
    easyflash_init();

    _cli_init();

    aos_register_event_filter(EV_WIFI, event_cb_wifi_event, NULL);
    cmd_stack_wifi(NULL, 0, 0, NULL);
    vTaskDelete(NULL);
}

static void system_thread_init()
{
    /*nothing here*/
}

/* init adc for tsen*/
#ifdef CONF_ADC_ENABLE_TSEN
static hosal_adc_dev_t adc0;

static void adc_tsen_init()
{
    int ret = -1;

    adc0.port = 0;
    adc0.config.sampling_freq = 300;
    adc0.config.pin = 4;
    adc0.config.mode = 0;

    ret = hosal_adc_init(&adc0);
    if (ret) {
        log_error("adc init error!\r\n");
        return;
    }
}
#endif

void main()
{
    bl_sys_init();

    system_thread_init();

#ifdef CONF_ADC_ENABLE_TSEN
    adc_tsen_init();
#endif

    // puts("[OS] Starting proc_hellow_entry task...\r\n");
    // xTaskCreateStatic(proc_hellow_entry, (char*)"hellow", 512, NULL, 15, proc_hellow_stack, &proc_hellow_task);
    // puts("[OS] Starting proc_mian_entry task...\r\n");
    xTaskCreate(proc_main_entry, (char*)"main_entry", 1024, NULL, 15, NULL);
    // puts("[OS] Starting TCP/IP Stack...\r\n");
    tcpip_init(NULL, NULL);
}
