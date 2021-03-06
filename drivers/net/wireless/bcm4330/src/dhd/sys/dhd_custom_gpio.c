/*
* Customer code to add GPIO control during WLAN start/stop
* Copyright (C) 1999-2011, Broadcom Corporation
* 
*         Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2 (the "GPL"),
* available at http://www.broadcom.com/licenses/GPLv2.php, with the
* following added to such license:
* 
*      As a special exception, the copyright holders of this software give you
* permission to link this software with independent modules, and to copy and
* distribute the resulting executable under terms of your choice, provided that
* you also meet, for each linked independent module, the terms and conditions of
* the license of that module.  An independent module is a module which is not
* derived from this software.  The special exception does not apply to any
* modifications of the software.
* 
*      Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*
* $Id: dhd_custom_gpio.c,v 1.2.42.1 2010-10-19 00:41:09 Exp $
*/

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <bcmutils.h>

#include <dngl_stats.h>
#include <dhd.h>

#include <wlioctl.h>
#include <wl_iw.h>

#define WL_ERROR(x) printf x
#define WL_TRACE(x)

#ifdef CUSTOMER_HW
extern  void bcm_wlan_power_off(int);
extern  void bcm_wlan_power_on(int);
#endif /* CUSTOMER_HW */
#if  defined(CUSTOMER_HW2)
int wifi_set_carddetect(int on);
int wifi_set_power(int on, unsigned long msec);
int wifi_get_irq_number(unsigned long *irq_flags_ptr);
int wifi_get_mac_addr(unsigned char *buf);
#endif

#if defined(OOB_INTR_ONLY)

#if defined(BCMLXSDMMC)
extern int sdioh_mmc_irq(int irq);
#endif /* (BCMLXSDMMC)  */

#ifdef CUSTOMER_HW3
#include <mach/gpio.h>
#endif

/* Customer specific Host GPIO defintion  */
static int dhd_oob_gpio_num = -1;

module_param(dhd_oob_gpio_num, int, 0644);
MODULE_PARM_DESC(dhd_oob_gpio_num, "DHD oob gpio number");

/* This function will return:
 *  1) return :  Host gpio interrupt number per customer platform
 *  2) irq_flags_ptr : Type of Host interrupt as Level or Edge
 *
 *  NOTE :
 *  Customer should check his platform definitions
 *  and his Host Interrupt spec
 *  to figure out the proper setting for his platform.
 *  Broadcom provides just reference settings as example.
 *
 */
int dhd_customer_oob_irq_map(unsigned long *irq_flags_ptr)
{
	int  host_oob_irq = 0;

#ifdef CUSTOMER_HW2
	host_oob_irq = wifi_get_irq_number(irq_flags_ptr);

#else /* for NOT  CUSTOMER_HW2 */
#if defined(CUSTOM_OOB_GPIO_NUM)
	if (dhd_oob_gpio_num < 0) {
		dhd_oob_gpio_num = CUSTOM_OOB_GPIO_NUM;
	}
#endif

	if (dhd_oob_gpio_num < 0) {
		WL_ERROR(("%s: ERROR customer specific Host GPIO is NOT defined \n",
			__FUNCTION__));
		return (dhd_oob_gpio_num);
	}

	WL_ERROR(("%s: customer specific Host GPIO number is (%d)\n",
	         __FUNCTION__, dhd_oob_gpio_num));

#if defined CUSTOMER_HW
	#if !defined(HARD_KERNEL)
		host_oob_irq = MSM_GPIO_TO_INT(dhd_oob_gpio_num);
	#endif	
#elif defined CUSTOMER_HW3
	gpio_request(dhd_oob_gpio_num, "oob irq");
	host_oob_irq = gpio_to_irq(dhd_oob_gpio_num);
	gpio_direction_input(dhd_oob_gpio_num);
#endif /* CUSTOMER_HW */
#endif /* CUSTOMER_HW2 */

	return (host_oob_irq);
}
#endif /* defined(OOB_INTR_ONLY) */

/* Customer function to control hw specific wlan gpios */
void
dhd_customer_gpio_wlan_ctrl(int onoff)
{
	switch (onoff) {
		case WLAN_RESET_OFF:
			WL_TRACE(("%s: call customer specific GPIO to insert WLAN RESET\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_off(2);
#endif /* CUSTOMER_HW */
#ifdef CUSTOMER_HW2
			wifi_set_power(0, 0);
#endif
			WL_ERROR(("=========== WLAN placed in RESET ========\n"));
		break;

		case WLAN_RESET_ON:
			WL_TRACE(("%s: callc customer specific GPIO to remove WLAN RESET\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_on(2);
#endif /* CUSTOMER_HW */
#ifdef CUSTOMER_HW2
			wifi_set_power(1, 0);
#endif
			WL_ERROR(("=========== WLAN going back to live  ========\n"));
		break;

		case WLAN_POWER_OFF:
			WL_TRACE(("%s: call customer specific GPIO to turn off WL_REG_ON\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_off(1);
#endif /* CUSTOMER_HW */
		break;

		case WLAN_POWER_ON:
			WL_TRACE(("%s: call customer specific GPIO to turn on WL_REG_ON\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_on(1);
#endif /* CUSTOMER_HW */
			/* Lets customer power to get stable */
			OSL_DELAY(200);
		break;
	}
}

#ifdef GET_CUSTOM_MAC_ENABLE

#if defined(HARD_KERNEL)
const   char *filepath = "/data/misc/wifi/mac_addr";

int dhd_read_macaddr(unsigned char *mac)
{
    struct file *fp      = NULL;

    char macbuffer[18]   = {0};
    mm_segment_t oldfs   = {0};
    char randommac[3]    = {0};

    int ret = 0;

    //MAC address copied from nv
    fp = filp_open(filepath, O_RDONLY, 0);

    if (IS_ERR(fp)) {
        fp = filp_open(filepath, O_RDWR | O_CREAT, 0666);

        if (IS_ERR(fp)) {
            printk("%s : Can't file(%s) create!!\n", __func__, filepath);
            return  -1;
        }

        oldfs = get_fs();
        set_fs(get_ds());

        /* Generating the Random Bytes for 3 last octects of the MAC address */
        get_random_bytes(randommac, 3);

        memset(macbuffer, 0x00, sizeof(macbuffer));
        sprintf(macbuffer,"%02X:%02X:%02X:%02X:%02X:%02X",
                0x00,0x90,0x4C,randommac[0],randommac[1],randommac[2]);
        printk("[%s] The Random Generated MAC ID : %s\n", __func__, macbuffer);

        if(fp->f_mode & FMODE_WRITE) {                  
            ret = fp->f_op->write(fp, (const char *)macbuffer, sizeof(macbuffer), &fp->f_pos);

            if(ret < 0)
                    printk("[%s] Mac address [%s] Failed to write into File: %s\n", __func__, macbuffer, filepath);
            else
                    printk("[%s] Mac address [%s] written into File: %s\n", __func__, macbuffer, filepath);
        }
        set_fs(oldfs);
    }

    memset(macbuffer, 0x00, sizeof(macbuffer));

    if((ret = kernel_read(fp, 0, macbuffer, 18)) < 0)   return  -1;      

    sscanf(macbuffer, "%02X:%02X:%02X:%02X:%02X:%02X", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

    printk("[%s] Mac address = %02X:%02X:%02X:%02X:%02X:%02X\n", __func__, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (fp)     filp_close(fp, NULL);

    return  0;
}
#endif  // #if defined(HARD_KERNEL)

/* Function to get custom MAC address */
int
dhd_custom_get_mac_address(unsigned char *buf)
{
	int ret = 0;

	WL_TRACE(("%s Enter\n", __FUNCTION__));
	if (!buf)
		return -EINVAL;

#if defined(HARD_KERNEL)
    ret = dhd_read_macaddr(buf);
#endif

	/* Customer access to MAC address stored outside of DHD driver */
#if defined(CUSTOMER_HW2) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
	ret = wifi_get_mac_addr(buf);
#endif

#ifdef EXAMPLE_GET_MAC
	/* EXAMPLE code */
	{
		struct ether_addr ea_example = {{0x00, 0x11, 0x22, 0x33, 0x44, 0xFF}};
		bcopy((char *)&ea_example, buf, sizeof(struct ether_addr));
	}
#endif /* EXAMPLE_GET_MAC */

	return ret;
}
#endif /* GET_CUSTOM_MAC_ENABLE */

/* Customized Locale table : OPTIONAL feature */
const struct cntry_locales_custom translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
#ifdef EXAMPLE_TABLE
	{"US", "US", 69}, /* input ISO "US" to : US regrev 69 */
	{"CA", "US", 69}, /* input ISO "CA" to : US regrev 69 */
	{"EU", "EU", 05}, /* input ISO "CA" to : US regrev 69 */
	{"FR", "EU", 05},
	{"DE", "EU", 05},
	{"IR", "EU", 05},
	{"UK", "EU", 05}, /* input ISO "UK" to : EU regrev 05 */
	{"KR", "XY", 03},
	{"AU", "XY", 03},
	{"CN", "XY", 03}, /* input ISO "CN" to : XY regrev 03 */
	{"TW", "XY", 03},
	{"AR", "XY", 03}
#endif /* EXMAPLE_TABLE */
};


/* Customized Locale convertor
*  input : ISO 3166-1 country abbreviation
*  output: customized cspec
*/
void get_customized_country_code(char *country_iso_code, wl_country_t *cspec)
{
	int size, i;

	size = ARRAYSIZE(translate_custom_table);

	if (cspec == 0)
		 return;

	if (size == 0)
		 return;

	for (i = 0; i < size; i++) {
	if (strcmp(country_iso_code, translate_custom_table[i].iso_abbrev) == 0) {
		memcpy(cspec->ccode,  translate_custom_table[i].custom_locale, WLC_CNTRY_BUF_SZ);
		cspec->rev = translate_custom_table[i].custom_locale_rev;
			break;
		}
	}
	return;
}
