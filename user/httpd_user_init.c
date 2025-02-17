

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
#include "httpdespfs.h"
#include "cgiwifi.h"
#include "auth.h"
#include "debug.h"

//Function that tells the authentication system what users/passwords live on the system.
//This is disabled in the default build; if you want to try it, enable the authBasic line in
//the builtInUrls below.
int myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
	if (no==0) {
		os_strcpy(user, "admin");
		os_strcpy(pass, "s3cr3t");
		return 1;
//Add more users this way. Check against incrementing no for each user added.
//	} else if (no==1) {
//		os_strcpy(user, "user1");
//		os_strcpy(pass, "something");
//		return 1;
	}
	return 0;
}


/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[]={
//Routines to make the /wifi URL and everything beneath it work.
#ifdef IMPULSE
	{"/generate_204", cgiRedirect, "/wifi/impulse_meter_wifi_setup.tpl"},		// iOS captive portal pop up web config
	{"/hotspot-detect.html", cgiRedirect, "/wifi/impulse_meter_wifi_setup.tpl"},	// android captive portal pop up web config
	{"/", cgiRedirect, "/wifi/impulse_meter_wifi_setup.tpl"},
	{"/wifi", cgiRedirect, "/wifi/impulse_meter_wifi_setup.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/impulse_meter_wifi_setup.tpl"},
	{"/wifi/impulse_meter_wifi_setup.tpl", cgiEspFsTemplate, tplSetup},
#else
	{"/generate_204", cgiRedirect, "/wifi/wifi_setup.tpl"},					// iOS captive portal pop up web config
	{"/hotspot-detect.html", cgiRedirect, "/wifi/wifi_setup.tpl"},			// android captive portal pop up web config
	{"/", cgiRedirect, "/wifi/wifi_setup.tpl"},
	{"/wifi", cgiRedirect, "/wifi/wifi_setup.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/wifi_setup.tpl"},
	{"/wifi/wifi_setup.tpl", cgiEspFsTemplate, tplSetup},
#endif
	{"/wifi/setup.cgi", cgiSetup, NULL},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},

	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};


//Main routine. Initialize stdout, the I/O and the webserver and we're done.
ICACHE_FLASH_ATTR
void httpd_user_init(void) {
	httpdInit(builtInUrls, 80);
	INFO("\nReady\n");
}
