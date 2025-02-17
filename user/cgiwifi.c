/*
Cgi/template routines for the /wifi url.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "httpd.h"
#include "captdns.h"
#include "led.h"
#include "config.h"
#include "debug.h"

#include "utils.h"
#include "tinyprintf.h"

//Enable this to disallow any changes in AP settings
//#define DEMO_MODE

//WiFi access point data
typedef struct {
	char ssid[32];
	sint8_t rssi;
	AUTH_MODE enc;
} ApData;

//Scan result
typedef struct {
	char scanInProgress; //if 1, don't access the underlying stuff from the webpage.
	ApData **apData;
	int noAps;
} ScanResultData;

//Static scan status storage.
ScanResultData cgiWifiAps;

static os_timer_t resetTimer;

//Callback the code calls when a wlan ap scan is done. Basically stores the result in
//the cgiWifiAps struct.
void ICACHE_FLASH_ATTR wifiScanDoneCb(void *arg, STATUS status) {
	int n;
	struct bss_info *bss_link = (struct bss_info *)arg;
	INFO("wifiScanDoneCb %d\n", status);
	if (status!=OK) {
		cgiWifiAps.scanInProgress=0;
		return;
	}

	//Clear prev ap data if needed.
	if (cgiWifiAps.apData!=NULL) {
		for (n=0; n<cgiWifiAps.noAps; n++) os_free(cgiWifiAps.apData[n]);
		os_free(cgiWifiAps.apData);
	}

	//Count amount of access points found.
	n=0;
	while (bss_link != NULL) {
		bss_link = bss_link->next.stqe_next;
		n++;
	}
	//Allocate memory for access point data
	cgiWifiAps.apData=(ApData **)os_malloc(sizeof(ApData *)*n);
	cgiWifiAps.noAps=n;
	INFO("Scan done: found %d APs\n", n);

	//Copy access point data to the static struct
	n=0;
	bss_link = (struct bss_info *)arg;
	while (bss_link != NULL) {
		if (n>=cgiWifiAps.noAps) {
			//This means the bss_link changed under our nose. Shouldn't happen!
			//Break because otherwise we will write in unallocated memory.
			INFO("Huh? I have more than the allocated %d aps!\n", cgiWifiAps.noAps);
			break;
		}
		//Save the ap data.
		cgiWifiAps.apData[n]=(ApData *)os_malloc(sizeof(ApData));
		cgiWifiAps.apData[n]->rssi=bss_link->rssi;
		cgiWifiAps.apData[n]->enc=bss_link->authmode;
		strncpy(cgiWifiAps.apData[n]->ssid, (char*)bss_link->ssid, 32);

		bss_link = bss_link->next.stqe_next;
		n++;
	}
	//We're done.
	cgiWifiAps.scanInProgress=0;
}


//Routine to start a WiFi access point scan.
static void ICACHE_FLASH_ATTR wifiStartScan() {
//	int x;
	if (cgiWifiAps.scanInProgress) return;
	cgiWifiAps.scanInProgress=1;
	wifi_station_scan(NULL, wifiScanDoneCb);
}

//This CGI is called from the bit of AJAX-code in wifi.tpl. It will initiate a
//scan for access points and if available will return the result of an earlier scan.
//The result is embedded in a bit of JSON parsed by the javascript in wifi.tpl.
int ICACHE_FLASH_ATTR cgiWiFiScan(HttpdConnData *connData) {
	int len;
	int i;
	char buff[1024];
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/json");
	httpdEndHeaders(connData);

	if (cgiWifiAps.scanInProgress==1) {
		//We're still scanning. Tell Javascript code that.
		len = tfp_snprintf(buff, 1024, "{\n \"result\": { \n\"inProgress\": \"1\"\n }\n}\n");
		if (httpdSend(connData, buff, len) == 0) {
			return HTTPD_CGI_SERVER_ERROR;
		}
	} else {
		//We have a scan result. Pass it on.
		len = tfp_snprintf(buff, 1024, "{\n \"result\": { \n\"inProgress\": \"0\", \n\"APs\": [\n");
		if (httpdSend(connData, buff, len) == 0) {
			return HTTPD_CGI_SERVER_ERROR;
		}
		if (cgiWifiAps.apData==NULL) cgiWifiAps.noAps=0;
		for (i=0; i<cgiWifiAps.noAps; i++) {
			//Fill in json code for an access point
			len = tfp_snprintf(buff, 1024, "{\"essid\": \"%s\", \"rssi\": \"%d\", \"enc\": \"%d\"}%s\n", 
					cgiWifiAps.apData[i]->ssid, cgiWifiAps.apData[i]->rssi, 
					cgiWifiAps.apData[i]->enc, (i==cgiWifiAps.noAps-1)?"":",");
			if (httpdSend(connData, buff, len) == 0) {
				return HTTPD_CGI_SERVER_ERROR;
			}
		}
		len = tfp_snprintf(buff, 1024, "]\n}\n}\n");
		if (httpdSend(connData, buff, len) == 0) {
			return HTTPD_CGI_SERVER_ERROR;
		}
		//Also start a new scan.
		wifiStartScan();
	}
	return HTTPD_CGI_DONE;
}

//Temp store for new ap info.
//static struct station_config stconf;


//This routine is ran some time after a connection attempt to an access point. If
//the connect succeeds, this gets the module in STA-only mode.
static void ICACHE_FLASH_ATTR resetTimerCb(void *arg) {
//	int x=wifi_station_get_connect_status();
//	if (x==STATION_GOT_IP) {
//		//Go to STA mode. This needs a reset, so do that.
//		INFO("Got IP. Going into STA mode..\n");
	if (cgiWifiAps.scanInProgress) {
		// scanner still running, we wait for it to complete before restarting to avoid crash
#ifdef DEBUG
		os_printf("scanner still running, defering restart 1 second...\n");
#endif
		os_timer_disarm(&resetTimer);
		os_timer_setfn(&resetTimer, resetTimerCb, NULL);
		os_timer_arm(&resetTimer, 1000, 0);	// try again one second later
		
		return;
	}
#ifdef DEBUG
	os_printf("restarting...\n");
#endif
//		wifi_set_opmode(1);
	os_timer_disarm(&resetTimer);
		
	system_restart_defered();
//	} else {
//		INFO("Connect fail. Not going into STA-only mode.\n");
//		//Maybe also pass this through on the webpage?
//	}
}

//This cgi uses the routines above to connect to a specific access point with the
//given ESSID using the given password.
int ICACHE_FLASH_ATTR cgiSetup(HttpdConnData *connData) {
	char essid[128];
	char passwd[128];
	char mqtthost[64];
#ifdef IMPULSE
	char impulse_meter_serial[32 + 1];
	char impulse_meter_units_string[32 + 1];
	float impulse_meter_units;
	char impulses_per_unit[8 + 1];
#endif
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	
	httpdFindArg(connData->postBuff, "essid", essid, sizeof(essid));
	httpdFindArg(connData->postBuff, "passwd", passwd, sizeof(passwd));
	httpdFindArg(connData->postBuff, "mqtthost", mqtthost, sizeof(mqtthost));
#ifdef IMPULSE
	httpdFindArg(connData->postBuff, "impulse_meter_serial", impulse_meter_serial, sizeof(impulse_meter_serial));
	httpdFindArg(connData->postBuff, "impulse_meter_units", impulse_meter_units_string, sizeof(impulse_meter_units_string));
	httpdFindArg(connData->postBuff, "impulses_per_unit", impulses_per_unit, sizeof(impulses_per_unit));
#endif

	os_strncpy((char*)sys_cfg.sta_ssid, essid, 32);
	os_strncpy((char*)sys_cfg.sta_pwd, passwd, 64);
	os_strncpy((char*)sys_cfg.mqtt_host, mqtthost, 64);
#ifdef IMPULSE
	os_strncpy((char*)sys_cfg.impulse_meter_serial, impulse_meter_serial, 32 + 1);
	tfp_vsscanf(impulse_meter_units_string, "%f", &impulse_meter_units);
	tfp_snprintf(impulse_meter_units_string, 32 + 1, "%.3f", impulse_meter_units / 1000.0);
	os_strncpy((char*)sys_cfg.impulse_meter_units, impulse_meter_units_string, 32 + 1);
	os_strncpy((char*)sys_cfg.impulses_per_unit, impulses_per_unit, 8 + 1);
	sys_cfg.impulse_meter_count = 0;
#endif

	cfg_save(NULL, NULL);

	INFO("Try to connect to AP %s pw %s\n", essid, passwd);

#ifdef IMPULSE
	httpdRedirect(connData, "impulse_meter_setting_up.html");
#else
	httpdRedirect(connData, "setting_up.html");
#endif
	
	// restart to go directly to sample mode after 5 seconds
	os_timer_disarm(&resetTimer);
	os_timer_setfn(&resetTimer, resetTimerCb, NULL);
	os_timer_arm(&resetTimer, 5000, 0);

	return HTTPD_CGI_DONE;
}

//This cgi uses the routines above to connect to a specific access point with the
//given ESSID using the given password.
int ICACHE_FLASH_ATTR cgiWifiSetMode(HttpdConnData *connData) {
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->getArgs, "mode", buff, sizeof(buff));
	if (len!=0) {
		INFO("cgiWifiSetMode: %s\n", buff);
#ifndef DEMO_MODE
		wifi_set_opmode(atoi(buff));
		system_restart();
#endif
	}
	httpdRedirect(connData, "/wifi");
	return HTTPD_CGI_DONE;
}

//Template code for the WLAN page.
void ICACHE_FLASH_ATTR tplSetup(HttpdConnData *connData, char *token, void **arg) {
	char buff[1024];
	int x;
	//static struct station_config stconf;
	if (token==NULL) return;
	//wifi_station_get_config(&stconf);
	cfg_load();

	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "WiFiMode")==0) {
		x=wifi_get_opmode();
		if (x == 1) os_strcpy(buff, "Client");
		if (x == 2) os_strcpy(buff, "SoftAP");
		if (x == 3) os_strcpy(buff, "STA+AP");
	}
	else if (os_strcmp(token, "currSsid") == 0) {
		os_strcpy(buff, (char*)sys_cfg.sta_ssid);
	}
	else if (os_strcmp(token, "WiFiPasswd") == 0) {
		os_strcpy(buff, (char*)sys_cfg.sta_pwd);
	}
	else if (os_strcmp(token, "WiFiapwarn") == 0) {
		x = wifi_get_opmode();
		if (x == 2) {
			os_strcpy(buff, "<b>Can't scan in this mode.</b> Click <a href=\"setmode.cgi?mode=3\">here</a> to go to STA+AP mode.");
		}
		else {
			os_strcpy(buff, "Click <a href=\"setmode.cgi?mode=2\">here</a> to go to standalone AP mode.");
		}
	}
	else if (os_strcmp(token, "MqttHost") == 0) {
		os_strcpy(buff, (char*)sys_cfg.mqtt_host);
	}
#ifdef IMPULSE
	else if (os_strcmp(token, "ImpulseMeterSerial") == 0) {
		os_strcpy(buff, (char*)sys_cfg.impulse_meter_serial);
	}
	else if (os_strcmp(token, "ImpulseMeterUnits") == 0) {
		tfp_snprintf(buff, 32 + 1, "%.3f", (atoi(sys_cfg.impulse_meter_units) + sys_cfg.impulse_meter_count * (1000.0 / atoi(sys_cfg.impulses_per_unit))) / 1000.0);	
	}
	else if (os_strcmp(token, "ImpulsesPerUnit") == 0) {
		os_strcpy(buff, (char*)sys_cfg.impulses_per_unit);
	}
#endif

	httpdSend(connData, buff, -1);
}

void ICACHE_FLASH_ATTR cgiWifiInit() {
	memset(&cgiWifiAps, 0, sizeof(ScanResultData));
	wifiStartScan();
}

void ICACHE_FLASH_ATTR cgiWifiDestroy() {
	int n;
	//Clear prev ap data if needed.
	if (cgiWifiAps.apData!=NULL) {
		for (n=0; n<cgiWifiAps.noAps; n++) os_free(cgiWifiAps.apData[n]);
		os_free(cgiWifiAps.apData);
	}
}
