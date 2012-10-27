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
 * 2012-10-26  0.2  Add POWER_HINT_INTERACTION . -- River

 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG 			"OMAP PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define INTERACTIVE_MAX_FREQ		"1200000"
#define SLEEP_MAX_FREQ			"300000"
//#define BOOSTPUSLE_DEFAULT_TIME	"5"

#define CPUFREQ_INTERACTIVE		"/sys/devices/system/cpu/cpufreq/interactive/"
#define CPUFREQ_CPU0			"/sys/devices/system/cpu/cpu0/cpufreq/"
#define BOOSTPULSE_PATH			(CPUFREQ_INTERACTIVE "boostpulse")

#define MAX_FREQ_NUMBER			10
#define NOM_FREQ_INDEX			2

struct power_module_device {
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
    int inited;
};

static struct power_module_device power_module_omap_device = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .boostpulse_fd = -1,
    .boostpulse_warned = 0,
    .inited = 0,
};

static struct power_module_device *omap_device = &power_module_omap_device;

static int str_to_tokens(char *str, char **token, int max_token_idx)
{
    char *pos, *start_pos = str;
    char *token_pos;
    int token_idx = 0;

    if (!str || !token || !max_token_idx) {
        return 0;
    }

    do {
        token_pos = strtok_r(start_pos, " \t\r\n", &pos);

        if (token_pos)
            token[token_idx++] = strdup(token_pos);
        start_pos = NULL;
    } while (token_pos && token_idx < max_token_idx);

    return token_idx;
}

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

static int sysfs_read(char *path, char *s, int s_size)
{
    char buf[80];
    int len, i;
    int fd;

    if (!path || !s || !s_size) {
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return fd;
    }

    len = read(fd, s, s_size-1);
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error reading from %s: %s\n", path, buf);
    } else {
        s[len] = '\0';
    }

    close(fd);
    return len;
}

static int get_scaling_governor(char governor[], int size)
{
    if (sysfs_read(CPUFREQ_CPU0 "scaling_governor", governor, size) < 0) {
        // Can't obtain the scaling governor. Return.
        return -1;
    } else {
        // Strip newline at the end.
        int len = strlen(governor);

        len--;

        while (len >= 0 && (governor[len] == '\n' || governor[len] == '\r'))
            governor[len--] = '\0';
    }

    return 0;
}

static void omap_power_init(struct power_module *module)
{
    omap_device = &power_module_omap_device;

    sysfs_write(CPUFREQ_INTERACTIVE "go_hispeed_load", 		"50");
    sysfs_write(CPUFREQ_INTERACTIVE "input_boost", 		"1");
    sysfs_write(CPUFREQ_INTERACTIVE "timer_rate", 		"20000");
    sysfs_write(CPUFREQ_INTERACTIVE "min_sample_time",		"60000");
    sysfs_write(CPUFREQ_INTERACTIVE "above_hispeed_delay", 	"100000");

    ALOGI("Initialized successfully");
    omap_device->inited = 1;
}

static int boostpulse_open(void)
{
    char buf[80];
    char governor[80];

    pthread_mutex_lock(&omap_device->lock);

    if (omap_device->boostpulse_fd < 0) {
        if (get_scaling_governor(governor, sizeof(governor)) < 0) {
            ALOGE("Can't read scaling governor.");
            omap_device->boostpulse_warned = 1;
        } else {
            if (strncmp(governor, "interactive", 11) == 0) {
                omap_device->boostpulse_fd = open(BOOSTPULSE_PATH, O_WRONLY);

                if (omap_device->boostpulse_fd < 0) {
                    if (!omap_device->boostpulse_warned) {
                        strerror_r(errno, buf, sizeof(buf));
                        ALOGE("Error opening %s: %s\n", BOOSTPULSE_PATH, buf);
                        omap_device->boostpulse_warned = 1;
                    } else if (omap_device->boostpulse_fd > 0)
                        ALOGD("Opened %s boostpulse interface", governor);
                }
            } else
                ALOGD("%s boostpulse not supported", governor);
        }    
    }

    pthread_mutex_unlock(&omap_device->lock);
    return omap_device->boostpulse_fd;
}

static void omap_power_set_interactive(struct power_module *module, int on)
{
    /*
     * Lower maximum frequency when screen is off.  CPU 0 and 1 share a
     * cpufreq policy.
     */

    sysfs_write(CPUFREQ_CPU0        	"scaling_max_freq", 	on ? INTERACTIVE_MAX_FREQ : SLEEP_MAX_FREQ);
    sysfs_write(CPUFREQ_INTERACTIVE 	"input_boost", 		on ? "1" : "0");
    sysfs_write(CPUFREQ_INTERACTIVE	"boost", 		on ? "1" : "0");

}

static void omap_power_hint(struct power_module *module, power_hint_t hint, void *data)
{
    char buf[80];
    char governor[80];
    int len;
    int duration = 1;

    if (!omap_device->inited)
        return;

    switch (hint) {
    case POWER_HINT_INTERACTION:
        if (boostpulse_open() > 0) {
            if (get_scaling_governor(governor, sizeof(governor)) < 0) {
                ALOGE("Can't read scaling governor.");
                omap_device->boostpulse_warned = 1;
            } else {
                if (strncmp(governor, "interactive", 11) == 0) {
                    if (data != NULL)
                        duration = (int) data;

                    snprintf(buf, sizeof(buf), "%d", duration);
                    len = write(omap_device->boostpulse_fd, buf, strlen(buf));

                    if (len < 0) {
                        strerror_r(errno, buf, sizeof(buf));
                        ALOGE("Error writing to %s: %s\n", BOOSTPULSE_PATH, buf);

                        pthread_mutex_lock(&omap_device->lock);	
                        close(omap_device->boostpulse_fd);
                        omap_device->boostpulse_fd = -1;
                        omap_device->boostpulse_warned = 0;
                        pthread_mutex_unlock(&omap_device->lock);
                    }
                }
            }/* else
                ALOGD("%s boostpulse not supported", governor);*/
        }
        break;

    case POWER_HINT_VSYNC:
        break;

    default:
        break;
    }
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
        .name 			= "OMAP Power HAL",
        .author 		= "The Android Open Source Project",
        .methods 		= &power_module_methods,
    },

    .init 		= omap_power_init,
    .setInteractive 	= omap_power_set_interactive,
    .powerHint		= omap_power_hint,
};
