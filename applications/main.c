#include <rtthread.h>
#include <rtdevice.h>
#include <stdlib.h>

#include "fsl_common.h"
#include "fsl_port.h"
#include "fsl_clock.h"

#define PWM_DEV_NAME        "pwm0"      /* 对应 FLEXPWM0 */
#define PWM_DEV_CHANNEL     1           /* FlexPWM0_A0 通常映射为通道 1 */

/* 引脚定义: P3_0 (Port3, Pin0) */
#define PWM_PORT            PORT3
#define PWM_PIN             0U
#define PWM_PIN_ALT         kPORT_MuxAlt5 
#define PWM_PORT_CLK        kCLOCK_GateGPIO3

/* 呼吸灯参数 */
#define PWM_FREQ_HZ         1000                    /* 1kHz */
#define PWM_PERIOD_NS       (1000000000 / PWM_FREQ_HZ) 
#define STEP_MS             20 

static struct rt_device_pwm *pwm_dev = RT_NULL;
static rt_thread_t pwm_tid = RT_NULL;
static volatile rt_bool_t is_running = RT_FALSE;


/* 初始化 P3_0 引脚复用 */
static void pwm_pin_mux_init(void)
{
    CLOCK_EnableClock(PWM_PORT_CLK);
    PORT_SetPinMux(PWM_PORT, PWM_PIN, PWM_PIN_ALT);
}

/* ================= 线程入口 ================= */
static void pwm_thread_entry(void *parameter)
{
    rt_uint32_t pulse = 0;
    rt_uint32_t duty_percent = 0;
    int direction = 1;

    /* 查找设备 */
    pwm_dev = (struct rt_device_pwm *)rt_device_find(PWM_DEV_NAME);
    if (pwm_dev == RT_NULL)
    {
        rt_kprintf("[Error] Can't find %s device!\n", PWM_DEV_NAME);
        is_running = RT_FALSE;
        return;
    }

    /* 使能通道 */
    rt_pwm_enable(pwm_dev, PWM_DEV_CHANNEL);
    rt_kprintf("[Info] PWM thread started. LED is breathing...\n");

    while (is_running)
    {
        /* 计算脉宽 */
        pulse = (rt_uint32_t)((uint64_t)PWM_PERIOD_NS * duty_percent / 100);
        rt_pwm_set(pwm_dev, PWM_DEV_CHANNEL, PWM_PERIOD_NS, pulse);

        duty_percent += (direction * 2); 
        if (duty_percent >= 100)
        {
            duty_percent = 100;
            direction = -1;
        }
        else if (duty_percent <= 0)
        {
            duty_percent = 0;
            direction = 1;
        }

        rt_thread_mdelay(STEP_MS);
    }

    /* 退出循环后关闭 PWM */
    rt_pwm_disable(pwm_dev, PWM_DEV_CHANNEL);
    rt_kprintf("[Info] PWM thread stopped.\n");
}

/* ================= FinSH ================= */
static void pwm_breath(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("Usage:\n");
        rt_kprintf("  pwm_breath start - Start the breathing LED thread\n");
        rt_kprintf("  pwm_breath stop  - Stop the thread\n");
        return;
    }

    if (!rt_strcmp(argv[1], "start"))
    {
        if (is_running)
        {
            rt_kprintf("[Warning] Breath LED is already running!\n");
            return;
        }

        /* 硬件引脚初始化 */
        pwm_pin_mux_init();

        is_running = RT_TRUE;
        
        /* 创建动态线程 */
        pwm_tid = rt_thread_create("t_breath",
                                   pwm_thread_entry, 
                                   RT_NULL,
                                   1024,   /* 栈大小 */
                                   20,     /* 优先级 */
                                   10);    /* 时间片 */
        
        if (pwm_tid != RT_NULL)
        {
            rt_thread_startup(pwm_tid);
        }
        else
        {
            rt_kprintf("[Error] Failed to create thread.\n");
            is_running = RT_FALSE;
        }
    }
    else if (!rt_strcmp(argv[1], "stop"))
    {
        if (!is_running)
        {
            rt_kprintf("[Warning] Breath LED is not running.\n");
            return;
        }
        is_running = RT_FALSE;
    }
    else
    {
        rt_kprintf("Unknown command. Usage: pwm_breath [start|stop]\n");
    }
}

int main(void)
{
    rt_kprintf("'pwm_breath start' for pwm running\n");
    
    return 0;
}

/* 导出命令到 FinSH */
MSH_CMD_EXPORT(pwm_breath, Control breathing LED: pwm_breath start/stop);