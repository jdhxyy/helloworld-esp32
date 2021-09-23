// Copyright 2020-2020 The TZIOT Authors. All rights reserved.
// 框架层主文件
// Authors: jdh99 <jdh821@163.com>

#include "framework.h"

#define TAG "framework"

// tzmalloc字节数
#define MALLOC_SIZE 1024

static uint64_t gPipe = 0;
static int mid = -1;

static void mainThread(void* param);
static void laganPrint(uint8_t* bytes, int size);
static void testPlatsa(void);
static void dealWifiScanResultFunc(WifiApInfo* apInfo, int arrayLen);
static void dealWifiConnectResultFunc(bool result);
static int wifiScanTask(void);
static void dealNetDataFunc(uint8_t* bytes, int size, uint32_t ip, uint16_t port);
static void corePipeSend(uint8_t* data, int size, uint8_t* dstIP, uint16_t dstPort);

static void dealBleRx(uint8_t* bytes, int size);
static int bleTxTask(void);

// testSevice 测试服务
// 遵循谁调用谁释放原则,resp需要由回调函数使用TZMalloc开辟空间,DCom负责释放空间
// 返回值为0表示回调成功,否则是错误码
static int testSevice(uint64_t pipe, uint64_t srcIA, uint8_t* req, int reqLen, 
    uint8_t** resp, int* respLen);

static int getTimeTask(void);
static void consoleThread(void* param);

// FrameworkLoad 模块载入
void FrameworkLoad(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        return;
    }
    if (esp_netif_init() != ESP_OK) {
        return;
    }
    if (esp_event_loop_create_default() != ESP_OK) {
        return;
    }

    BrorThreadCreate(mainThread, "mainThread", BROR_THREAD_PRIORITY_MIDDLE, 
        MAIN_THREAD_STACK_SIZE * 1024);
}

static void mainThread(void* param) {
    BrorDelayMS(1);

    // 日志模块载入
    LaganLoad(laganPrint, NULL, GetLocalTimeUs);

    // 申请内存
    void* addr = malloc(TZMALLOC_RAM_SIZE * 1024);
    if (addr == NULL) {
        LE(TAG, "main thread load failed!malloc failed!\n");
        goto EXIT;
    }
    TZMallocLoad(0, 20, TZMALLOC_RAM_SIZE * 1024, addr);
    LI(TAG, "TZMalloc load success.malloc ram:%d Kbyte", TZMALLOC_RAM_SIZE);

    // 申请一片内存
    mid = TZMallocRegister(0, TAG, MALLOC_SIZE);
    if (mid == -1) {
        LE(TAG, "main thread load failed!maoolc register failed!\n");
        goto EXIT;
    }

    if (TZFlashAdapterLoad("userdata") == false) {
        LE(TAG, "main thread load failed!tzflash adapter load failed\n");
        goto EXIT;
    }
    if (PlatsaLoad(TZMallocRegister(0, "platsa", 2048)) == false) {
        LE(TAG, "main thread load failed!platsa load failed\n");
        goto EXIT;
    }
    testPlatsa();

    // 时钟模块载入
    TZTimeLoad(GetLocalTimeUs);
    LI(TAG, "TZTimeLoad success");

    // WIFI载入
    if (WifiLoad() == false) {
        LE(TAG, "main thread load failed!wifi load failed!\n");
        goto EXIT;
    }
    LI(TAG, "wifi load success");

    // WIFI扫描
    if (WifiScan() == false) {
        LE(TAG, "main thread load failed!wifi scan failed!\n");
        goto EXIT;
    }
    // 设置扫描和连接回调
    WifiSetCallbackScanResult(dealWifiScanResultFunc);
    WifiSetCallbackConnectResult(dealWifiConnectResultFunc);

    // 周期扫描任务
    AsyncStart(wifiScanTask, ASYNC_SECOND);

    // udp模块载入
    if (UdpLoad() == false) {
        LE(TAG, "main thread load failed!udp load failed!\n");
        goto EXIT;
    }
    // 注册接收数据回调
    UdpRegisterObserver(dealNetDataFunc);

    if (BleServerLoad("ble_jdh99") == false) {
        LE(TAG, "main thread load failed!ble server load failed!\n");
        goto EXIT;
    }
    BleServerRegisterObserver(dealBleRx);
    // ble发送任务
    AsyncStart(bleTxTask, ASYNC_SECOND);

    // tziot协议栈载入
    TZIotLoad(LOCAL_IA, LOCAL_PWD);
    gPipe = TZIotBindPipeCore(corePipeSend, WifiIsConnect);
    if (gPipe == 0) {
        LE(TAG, "main thread load failed!bind pipe failed!\n");
        goto EXIT;
    }

    // 注册测试服务
    TZIotRegister(1, testSevice);

    // 读取网络时间任务
    AsyncStart(getTimeTask, 10 * ASYNC_SECOND);

    // 创建控制台输入线程
    BrorThreadCreate(consoleThread, "consoleThread", BROR_THREAD_PRIORITY_MIDDLE, 
        10 * 1024);

    while (1) {
        AsyncRun();
        BrorDelayMS(1);
    }

EXIT:
    BrorThreadDeleteMe();
}

static void laganPrint(uint8_t* bytes, int size) {
    printf((char*)bytes);
}

static void testPlatsa(void) {
    if (PlatsaRecovery(0, 4096, 4096)) {
        LI(TAG, "platsa recovery1 success");
        LI(TAG, "platsa get:%s", (char*)PlatsaGet("name", NULL));
        LI(TAG, "platsa get:%s", (char*)PlatsaGet("title", NULL));
    } else {
        LW(TAG, "platsa recovery1 failed");
    }

    LI(TAG, "platsa set name:%d", PlatsaSet("name", (uint8_t*)"jdh99", 6));
    LI(TAG, "platsa set ok:%d", PlatsaSet("title", (uint8_t*)"ok", 3));
    LI(TAG, "platsa get:%s", (char*)PlatsaGet("name", NULL));
    LI(TAG, "platsa get:%s", (char*)PlatsaGet("title", NULL));

    LI(TAG, "platsa save:%d", PlatsaSave(0, 4096, 4096));

    if (PlatsaRecovery(0, 4096, 4096)) {
        LI(TAG, "platsa recovery2 success");
        LI(TAG, "platsa get:%s", (char*)PlatsaGet("name", NULL));
        LI(TAG, "platsa get:%s", (char*)PlatsaGet("title", NULL));
    } else {
        LW(TAG, "platsa recovery2 failed");
    }
}

static void dealWifiScanResultFunc(WifiApInfo* apInfo, int arrayLen) {
    for (int i = 0; i < arrayLen; i++) {
        LI("framework", "wifi ssid=%s\n", apInfo[i].Ssid);
        LaganPrintHex("framework", LAGAN_LEVEL_INFO, apInfo[i].Bssid, WIFI_BSSID_LEN);

        if (strcmp((char*)apInfo[i].Ssid, WIFI_SSID_DEFAULT) == 0) {
            LI("framework", "connect %s", WIFI_SSID_DEFAULT);
            WifiConnect(WIFI_SSID_DEFAULT, WIFI_PWD_DEFAULT);
        }
    }
}

static void dealWifiConnectResultFunc(bool result) {
    printf("connect result=%d\n", result);

    if (result) {
        WifiConnectInfo* info = WifiGetConnectInfo();
        if (info != NULL) {
            LI("framework ip", "ip:0x%x, gw:0x%x", info->IP, info->Gateway);

            // 发送测试帧.192.168.43.1
            UdpTx((uint8_t*)"jdh88", 5, info->Gateway, 8082);
        }
    }
}

static int wifiScanTask(void) {
    static struct pt pt = {0};

    PT_BEGIN(&pt);

    PT_WAIT_UNTIL(&pt, WifiIsConnect() == false && WifiIsBusy() == false);
    printf("scan=%d\n", WifiScan());

    PT_END(&pt);
}

static void dealNetDataFunc(uint8_t* bytes, int size, uint32_t ip, uint16_t port) {
    LI(TAG, "receive ip:0x%x, port:%d", ip, port);
    // LaganPrintHex(TAG, LAGAN_LEVEL_DEBUG, bytes, size);
    // UdpTx(bytes, size, ip, 8082);
    uint8_t srcIP[4] = {0};
    srcIP[0] = ip >> 24;
    srcIP[1] = ip >> 16;
    srcIP[2] = ip >> 8;
    srcIP[3] = ip;
    TZIotPipeCoreReceive(bytes, size, srcIP, port);
}

static void dealBleRx(uint8_t* bytes, int size) {
    LI(TAG, "ble rx data.len:%d", size);
}

static int bleTxTask(void) {
    static struct pt pt = {0};
    static char s[200] = {0};
    
    PT_BEGIN(&pt);

    PT_WAIT_UNTIL(&pt, BleServerIsConnect() == true);
    //BleTx((uint8_t*)"jdh99", 5);

    for (int i = 0; i < 180; i++) {
        s[i]= 'a' + i % 25;
    }
    BleTx((uint8_t*)s, 180);

    PT_END(&pt);
}

// testSevice 测试服务
// 遵循谁调用谁释放原则,resp需要由回调函数使用TZMalloc开辟空间,DCom负责释放空间
// 返回值为0表示回调成功,否则是错误码
static int testSevice(uint64_t pipe, uint64_t srcIA, uint8_t* req, int reqLen, 
    uint8_t** resp, int* respLen) {
    LI("service1", "pipe:%d srcIA:0x%x reqLen:%d\n", pipe, srcIA, reqLen);

    LaganPrintHex("service1", LAGAN_LEVEL_INFO, req, reqLen);

    uint8_t* arr = TZMalloc(mid, 260);
    if (arr == NULL) {
        return 0;
    }
    for (int i = 0; i < 260; i++) {
        arr[i] = i;
    }
    *resp = arr;
    *respLen = 260;
    return 0;
}

static int getTimeTask(void) {
    static struct pt pt;
    static uint8_t arr[3] = {1,2,3};
    static int result = 0;
    static uint8_t* resp = NULL;
    static int respLen = 0;
    static intptr_t handle;

    PT_BEGIN(&pt);

    PT_WAIT_UNTIL(&pt, TZIotIsConn());

    handle = TZIotCallCreateHandle(gPipe, 0x2141000000000004, 1, 5000, NULL, 0, 
        &resp, &respLen, &result);
    PT_WAIT_THREAD(&pt, TZIotCall(handle));

    if (result == 0 && resp != NULL) {
        LI("case2", "rx:%d %s", respLen, (char*)resp);
    } else {
        LW("case2", "call failed:0x%x\n", result);
    }
    
    if (resp != NULL) {
        TZFree(resp);
        resp = NULL;
    }

    PT_END(&pt);
}

static void corePipeSend(uint8_t* data, int size, uint8_t* dstIP, uint16_t dstPort) {
    uint32_t ip = (dstIP[0] << 24) + (dstIP[1] << 16) + (dstIP[2] << 8) + dstIP[3];
    UdpTx(data, size, ip, dstPort);
}

static void dealCmd(char* cmd) {
    if (strcmp(cmd, "abc") == 0) {
        TZMallocStatus status = TZMallocGetStatus(0);
        printf("UsedSize:%ld FreeSize:%ld MaxFreeSize:%ld MallocNum:%ld FreeNum:%ld\n", 
            status.UsedSize, status.FreeSize, status.MaxFreeSize, 
            status.MallocNum, status.FreeNum);

        int num = TZMallocGetUserNum(0);
        int mid = -1;
        TZMallocUser* user = NULL;

//         typedef struct {
//     int RamIndex;
//     char Tag[TZMALLOC_TAG_LEN_MAX + 1];
//     uint32_t Total;
//     uint32_t Used;
//     uint32_t MallocNum;
//     uint32_t FreeNum;
//     uint32_t ExceptionNum;
// } TZMallocUser;

        for (int i = 0; i < num; i++) {
            mid = TZMallocGetUserMid(0, i);
            user = TZMallocGetUser(mid);
            printf("tag=%s,total=%d,used=%d,malloc num=%d,free num = %d,exception num = %d\n", 
                user->Tag, user->Total, user->Used, user->MallocNum, user->FreeNum, user->ExceptionNum);
        }
        return;
    }
}

static void consoleThread(void* param) {
    BrorDelayMS(1);

    char input[32] = {0};
    int len = 0;
    int num = 0;
    char ch;
    uint8_t* addr = NULL;
    
    int failNum = 0;
    int SuccessNum = 0;
    int mid = TZMallocRegister(0, "testthread1", 1024);
    if (mid == -1) {
        printf("mid register failed\n");
    }

    TZMallocStatus status = TZMallocGetStatus(0);
    printf("UsedSize:%ld FreeSize:%ld MaxFreeSize:%ld MallocNum:%ld FreeNum:%ld\n", 
            status.UsedSize, status.FreeSize, status.MaxFreeSize, 
            status.MallocNum, status.FreeNum);
    while (1) {
        // num = fread(input, 1, 30, stdin);
        // if (num > 0) {
        //     printf("input=%d %s\n", num, input);
        // }

        ch = fgetc(stdin);
        if (ch != 0xff) {
            // printf("ch=%c\n", ch);

            if (ch == '\n') {
                // 处理命令
                printf("cmd=%s\n", input);
                dealCmd(input);

                len = 0;
                memset(input, 0, 32);
            } else {
                input[len++] = ch;
                if (len >= 32) {
                    len = 0;
                    memset(input, 0, 32);
                }
            }
        }

        // if (SuccessNum < 1000000) {
        //     addr = TZMalloc(mid, 600);
        //     if (addr == NULL) {
        //         failNum++;
        //         printf("malloc failed:%d %d\n", SuccessNum, failNum);
        //     } else {
        //         SuccessNum++;
        //         printf("malloc success:%d %d\n", SuccessNum, failNum);
        //         TZFree(addr);
        //         addr = NULL;
        //     }
        // }
        BrorDelayMS(1);
    }

    BrorThreadDeleteMe();
}
