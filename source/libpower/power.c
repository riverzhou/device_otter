/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 
 * History
 * 2012-10-21  0.1  Create By RiverZhou. It's based on asus grouper's PowerHAL

 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INTERACTIVE_MAX_FREQ	"1200000"
#define SLEEP_MAX_FREQ		"300000"

#define CPUFREQ_INTERACTIVE 	"/sys/devices/system/cpu/cpufreq/interactive/"
#define CPUFREQ_CPU0 		"/sys/devices/system/cpu/cpu0/cpufreq/"

#define LOG_TAG 		"OMAP PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static void omap_power_init(struct power_module *module)
{

    sysfs_write(CPUFREQ_INTERACTIVE "timer_rate",
                "20000");

    sysfs_write(CPUFREQ_INTERACTIVE "min_sample_time",
                "30000");

    sysfs_write(CPUFREQ_INTERACTIVE "go_hispeed_load",
                "85");

    sysfs_write(CPUFREQ_INTERACTIVE "input_boost",
		"1");

}

static void omap_power_set_interactive(struct power_module *module, int on)
{
    /*
     * Lower maximum frequency when screen is off.  CPU 0 and 1 share a
     * cpufreq policy.
     */

    sysfs_write(CPUFREQ_CPU0        "scaling_max_freq",
                on ? INTERACTIVE_MAX_FREQ : SLEEP_MAX_FREQ);

    sysfs_write(CPUFREQ_INTERACTIVE "input_boost",
                on ? "1" : "0");

}

static void omap_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
	return ;
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag 			= HARDWARE_MODULE_TAG,
        .module_api_version 	= POWER_MODULE_API_VERSION_0_2,
        .hal_api_version 	= HARDWARE_HAL_API_VERSION,
        .id 			= POWER_HARDWARE_MODULE_ID,
        .name 			= "TI OMAP Power HAL",
        .author 		= "The Android Open Source Project",
        .methods 		= &power_module_methods,
    },

    .init 		= omap_power_init,
    .setInteractive 	= omap_power_set_interactive,
    .powerHint 		= omap_power_hint,
};
