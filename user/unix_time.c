#include <esp8266.h>
#include "debug.h"
#include "unix_time.h"
#include <sntp.h>

static os_timer_t sntp_check_timer;

uint64_t init_time = 0;
uint64_t current_unix_time;
uint64_t ntp_offline_second_counter = 0;

static os_timer_t ntp_offline_second_counter_timer;

ICACHE_FLASH_ATTR void static sntp_check_timer_func(void *arg) {
	current_unix_time = sntp_get_current_timestamp();	// DEBUG: possible wrapping error here, when casting from 32 bit to 64 bit variable
	
	if (current_unix_time == 0) {
		os_timer_disarm(&sntp_check_timer);
		os_timer_arm(&sntp_check_timer, 2000, 0);
	} else {
		os_timer_disarm(&sntp_check_timer);
		// save init time for use in get_uptime()
		os_timer_disarm(&ntp_offline_second_counter_timer);		// stop offline second counter
		if (init_time == 0) {		// only set init_time at boot			
			init_time = current_unix_time;							// save the unix time stamp we go ntp time
		}
	}
}

ICACHE_FLASH_ATTR void static ntp_offline_second_counter_timer_func(void *arg) {
	ntp_offline_second_counter++;
}

ICACHE_FLASH_ATTR void init_unix_time(void) {
	// init sntp
	sntp_setservername(0, NTP_SERVER_1); // set server 0 by domain name
	sntp_setservername(1, NTP_SERVER_2); // set server 1 by domain name
	sntp_set_timezone(0);	// UTC time
	sntp_init();
	
	// start timer to make sure we go ntp reply
	os_timer_disarm(&sntp_check_timer);
	os_timer_setfn(&sntp_check_timer, (os_timer_func_t *)sntp_check_timer_func, NULL);
	os_timer_arm(&sntp_check_timer, 2000, 0);

	ntp_offline_second_counter = 0;
	os_timer_disarm(&ntp_offline_second_counter_timer);
	os_timer_setfn(&ntp_offline_second_counter_timer, (os_timer_func_t *)ntp_offline_second_counter_timer_func, NULL);
	os_timer_arm(&ntp_offline_second_counter_timer, 1000, 1);		// every seconds
}

ICACHE_FLASH_ATTR uint64_t get_unix_time(void) {
	current_unix_time = sntp_get_current_timestamp();	// DEBUG: possible wrapping error here, when casting from 32 bit to 64 bit variable

	return current_unix_time;
}

ICACHE_FLASH_ATTR uint64_t get_uptime(void) {
	current_unix_time = sntp_get_current_timestamp();	// DEBUG: possible wrapping error here, when casting from 32 bit to 64 bit variable
	if (init_time == 0) {	// just booted
		return ntp_offline_second_counter;
	}
	else {
		return current_unix_time - init_time + ntp_offline_second_counter;
	}
}

ICACHE_FLASH_ATTR void destroy_unix_time(void) {
	// only call this before restarting system_restart() to stop all timers
	os_timer_disarm(&sntp_check_timer);
	os_timer_disarm(&ntp_offline_second_counter_timer);
	sntp_stop();		// stop ntp client
}
