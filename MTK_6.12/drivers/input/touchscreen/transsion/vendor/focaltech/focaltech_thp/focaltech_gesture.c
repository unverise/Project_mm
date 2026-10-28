//USD:add rain touch code by linsheng.liu 20240813 start
/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
*
* File Name: focaltech_gestrue.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_core.h"
#ifdef KEY_FINGER
#else
#define KEY_FINGER 195
#endif

#ifndef KEY_F26
#define KEY_F26 198
#endif

/*****************************************************************************
* Static variables
*****************************************************************************/
static struct fts_gesture_st fts_gesture_data;

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static ssize_t fts_gesture_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    u8 val = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    mutex_lock(&ts_data->input_dev->mutex);
    fts_read_reg(FTS_REG_GESTURE_EN, &val);
    count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
                     ts_data->gesture_support ? "On" : "Off");
    count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d\n", val);
    mutex_unlock(&ts_data->input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    if (ts_data->suspended) {
        FTS_INFO("In suspend,not operation gesture mode!");
        return count;
    }
    mutex_lock(&ts_data->input_dev->mutex);
    if (FTS_SYSFS_ECHO_ON(buf)) {
        FTS_DEBUG("enable gesture");
        ts_data->gesture_support = ENABLE;
    } else if (FTS_SYSFS_ECHO_OFF(buf)) {
        FTS_DEBUG("disable gesture");
        ts_data->gesture_support = DISABLE;
    }
    mutex_unlock(&ts_data->input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_buf_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    int i = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);
    struct input_dev *input_dev = ts_data->input_dev;
    struct fts_gesture_st *gesture = &fts_gesture_data;

    mutex_lock(&input_dev->mutex);
    count = snprintf(buf, PAGE_SIZE, "Gesture ID:%d\n", gesture->gesture_id);
    count += snprintf(buf + count, PAGE_SIZE, "Gesture PointNum:%d\n",
                      gesture->point_num);
    count += snprintf(buf + count, PAGE_SIZE, "Gesture Points Buffer:\n");

    /* save point data,max:6 */
    for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
        count += snprintf(buf + count, PAGE_SIZE, "%3d(%4d,%4d) ", i,
                          gesture->coordinate_x[i], gesture->coordinate_y[i]);
        if ((i + 1) % 4 == 0)
            count += snprintf(buf + count, PAGE_SIZE, "\n");
    }
    count += snprintf(buf + count, PAGE_SIZE, "\n");
    mutex_unlock(&input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_buf_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}

static ssize_t fts_gesture_bm_show(
    struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    mutex_lock(&ts_data->input_dev->mutex);
    count = snprintf(buf, PAGE_SIZE, "gesture bmode:%d\n",
                     ts_data->gesture_bmode);
    mutex_unlock(&ts_data->input_dev->mutex);

    return count;
}

static ssize_t fts_gesture_bm_store(
    struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);
    int value = 0xFF;
    int ret = 0;

    mutex_lock(&ts_data->input_dev->mutex);
    ret = sscanf(buf, "%d", &value);
    if (ret == 1) {
        FTS_DEBUG("gesture bmode:%d->%d", ts_data->gesture_bmode, value);
        ts_data->gesture_bmode = value;
    }
    mutex_unlock(&ts_data->input_dev->mutex);

    return count;
}

/* sysfs gesture node
 *   read example: cat  fts_gesture_mode       ---read gesture mode
 *   write example:echo 1 > fts_gesture_mode   --- write gesture mode to 1
 *
 */
static DEVICE_ATTR(fts_gesture_mode, S_IRUGO | S_IWUSR, fts_gesture_show,
                   fts_gesture_store);
/*
 *   read example: cat fts_gesture_buf        --- read gesture buf
 */
static DEVICE_ATTR(fts_gesture_buf, S_IRUGO | S_IWUSR,
                   fts_gesture_buf_show, fts_gesture_buf_store);

static DEVICE_ATTR(fts_gesture_bm, S_IRUGO | S_IWUSR,
                   fts_gesture_bm_show, fts_gesture_bm_store);

static struct attribute *fts_gesture_mode_attrs[] = {
    &dev_attr_fts_gesture_mode.attr,
    &dev_attr_fts_gesture_buf.attr,
    &dev_attr_fts_gesture_bm.attr,
    NULL,
};

static struct attribute_group fts_gesture_group = {
    .attrs = fts_gesture_mode_attrs,
};

static int fts_create_gesture_sysfs(struct device *dev)
{
    int ret = 0;

    ret = sysfs_create_group(&dev->kobj, &fts_gesture_group);
    if (ret) {
        FTS_ERROR("gesture sys node create fail");
        sysfs_remove_group(&dev->kobj, &fts_gesture_group);
        return ret;
    }

    return 0;
}
#if 0
static void fts_gesture_report(struct input_dev *input_dev, int gesture_id)
{
    int gesture;

    FTS_DEBUG("gesture_id:0x%x", gesture_id);
    switch (gesture_id) {
    case GESTURE_LEFT:
        gesture = KEY_GESTURE_LEFT;
        break;
    case GESTURE_RIGHT:
        gesture = KEY_GESTURE_RIGHT;
        break;
    case GESTURE_UP:
        gesture = KEY_GESTURE_UP;
        break;
    case GESTURE_DOWN:
        gesture = KEY_GESTURE_DOWN;
        break;
    case GESTURE_DOUBLECLICK:
        gesture = KEY_GESTURE_U;
        break;
    case GESTURE_O:
        gesture = KEY_GESTURE_O;
        break;
    case GESTURE_W:
        gesture = KEY_GESTURE_W;
        break;
    case GESTURE_M:
        gesture = KEY_GESTURE_M;
        break;
    case GESTURE_E:
        gesture = KEY_GESTURE_E;
        break;
    case GESTURE_L:
        gesture = KEY_GESTURE_L;
        break;
    case GESTURE_S:
        gesture = KEY_GESTURE_S;
        break;
    case GESTURE_V:
        gesture = KEY_GESTURE_V;
        break;
    case GESTURE_Z:
        gesture = KEY_GESTURE_Z;
        break;
    case  GESTURE_C:
        gesture = KEY_GESTURE_C;
        break;
    default:
        gesture = -1;
        break;
    }
    /* report event key */
    if (gesture != -1) {
        FTS_DEBUG("Gesture Code=%d", gesture);
        input_report_key(input_dev, gesture, 1);
        input_sync(input_dev);
        input_report_key(input_dev, gesture, 0);
        input_sync(input_dev);
    }
}
#endif
/*****************************************************************************
* Name: fts_gesture_readdata
* Brief: Read information about gesture: enable flag/gesture points..., if ges-
*        ture enable, save gesture points' information, and report to OS.
*        It will be called this function every intrrupt when gesture is supported.
*
*        gesture data length: 1(enable) + 1(reserve) + 2(header) + 6 * 4
* Input: ts_data - global struct data
*        data    - gesture data buffer
* Output:
* Return: 0 - read gesture data successfully, the report data is gesture data
*         1 - tp not in suspend/gesture not enable in TP FW
*         -Exx - error
*****************************************************************************/
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *touch_buf)
{
    int ret = 0;
    int i = 0;
    int index = 0;
    u8 buf[FTS_GESTURE_DATA_LEN] = { 0 };
    u8 gesture_en = 0xFF;
    /*BSP: add transsion core by guoxin.zhu 20230613 start*/
    struct tran_core_ops *ops = NULL;
    /*BSP: add transsion core by guoxin.zhu 20220613 end*/	
    //struct input_dev *input_dev = ts_data->input_dev;
    struct fts_gesture_st *gesture = &fts_gesture_data;

    ret = fts_read_reg(FTS_REG_GESTURE_EN, &gesture_en);
    if (gesture_en != ENABLE) {
        FTS_DEBUG("gesture not enable in fw, don't process gesture");
        return 0;
    }

    if ((ts_data->gesture_bmode == GESTURE_BM_TOUCH) && touch_buf &&
        (TOUCH_DEFAULT == ((touch_buf[FTS_TOUCH_E_NUM] >> 4) & 0x0F))) {
        memcpy(buf, touch_buf + FTS_TOUCH_DATA_LEN, FTS_GESTURE_DATA_LEN);
    } else {
        buf[2] = FTS_REG_GESTURE_OUTPUT_ADDRESS;
        ret = fts_read(&buf[2], 1, &buf[2], FTS_GESTURE_DATA_LEN - 2);
        if (ret < 0) {
            FTS_ERROR("read gesture header data fail");
            return ret;
        }
    }

    /* init variable before read gesture point */
    memset(gesture->coordinate_x, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
    memset(gesture->coordinate_y, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
    gesture->gesture_id = buf[2];
    gesture->point_num = buf[3];
    FTS_DEBUG("gesture_id=%d, point_num=%d",
              gesture->gesture_id, gesture->point_num);

    /* save point data,max:6 */
    for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
        index = 4 * i + 4;
        gesture->coordinate_x[i] = (u16)(((buf[0 + index] & 0x0F) << 8)
                                         + buf[1 + index]);
        gesture->coordinate_y[i] = (u16)(((buf[2 + index] & 0x0F) << 8)
                                         + buf[3 + index]);
    }

    /* report gesture to OS */
    //fts_gesture_report(input_dev, gesture->gesture_id);
    /*BSP: filter FOD event by linsheng.liu 20230214 start*/
    if (ts_data->pdata->fod_func_support) {
        if (gesture->gesture_id == GESTURE_FOD) {
            return 1;
        }
    }
    /*BSP: filter FOD event by linsheng.liu 20230214 end*/
    if (get_tpd_ops(&ft3683g_controller, &ops)) {
        ret = ops->gesture_id_report(gesture->gesture_id,&ft3683g_controller);
        if (ret < 0) {
            FTS_ERROR("[GESTURE]gesture touch data report fail");
            return ret;
        }
    }	
    return FTS_RETVAL_IGNORE_TOUCHES;
}

void fts_gesture_recovery(struct fts_ts_data *ts_data)
{
    u8 state = 0xFF;
    if (ts_data->gesture_support && ts_data->suspended) {
        fts_write_reg(0xD1, 0xFF);
        fts_write_reg(0xD2, 0xFF);
        fts_write_reg(0xD5, 0xFF);
        fts_write_reg(0xD6, 0xFF);
        fts_write_reg(0xD7, 0xFF);
        fts_write_reg(0xD8, 0xFF);
        fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
        fts_msleep(1);
        fts_read_reg(FTS_REG_GESTURE_EN, &state);
        if (state != ENABLE) {
            FTS_ERROR("set gesture mode failed");
        }
    }
}

int fts_gesture_suspend(struct fts_ts_data *ts_data)
{
    int i = 0;
    u8 state = 0xFF;
    u8 state_thp = 0xFF;

    FTS_FUNC_ENTER();

    for (i = 0; i < FTS_MAX_RETRIES_WRITEREG; i++) {
        /*BSP: optimeze fod state for LI9H335-1158 by jiangzhu.yang 20231207 start*/
        if (ts_data->pdata->fod_func_support) {
            if (tran_fingerprint_status)
                fts_write_reg(FTS_FOD_MODE, FTS_FOD_OPEN);
            else
                fts_write_reg(FTS_FOD_MODE, FTS_FOD_CLOSE);
        }
        /*BSP: optimeze fod state for LI9H335-1158 by jiangzhu.yang 20231207 end*/
        fts_write_reg(0xD1, 0xFF);
        fts_write_reg(0xD2, 0xFF);
        fts_write_reg(0xD5, 0xFF);
        fts_write_reg(0xD6, 0xFF);
        fts_write_reg(0xD7, 0xFF);
        fts_write_reg(0xD8, 0xFF);
        fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
        fts_write_reg(0x00, 0x00);  //switch to normal mode
        fts_msleep(1);
        fts_read_reg(FTS_REG_GESTURE_EN, &state);
        fts_read_reg(0x00, &state_thp);
        if (state == ENABLE && state_thp == 0x00)
            break;
    }

    if (i >= FTS_MAX_RETRIES_WRITEREG)
        FTS_ERROR("make IC enter into gesture(suspend) fail,state:%x, state_thp:%x", state, state_thp);
    else
        FTS_INFO("Enter into gesture(suspend) successfully");

    /*USD: add for thp suspend status update by guoxin.zhu 20250305 start*/
    fhp_status_switch_trigger(1);//switch to mcu mode
    /*USD: add for thp suspend status update by guoxin.zhu 20250305 end*/
    FTS_FUNC_EXIT();
    return 0;
}

int fts_gesture_resume(struct fts_ts_data *ts_data)
{
    int i = 0;
    u8 state = 0xFF;

    FTS_FUNC_ENTER();
    for (i = 0; i < FTS_MAX_RETRIES_WRITEREG; i++) {
        fts_write_reg(FTS_REG_GESTURE_EN, DISABLE);
        fts_msleep(1);
        fts_read_reg(FTS_REG_GESTURE_EN, &state);
        if (state == DISABLE)
            break;
    }

    if (i >= FTS_MAX_RETRIES_WRITEREG)
        FTS_ERROR("make IC exit gesture(resume) fail,state:%x", state);
    else
        FTS_INFO("resume from gesture successfully");

    FTS_FUNC_EXIT();
    return 0;
}

int fts_gesture_init(struct fts_ts_data *ts_data)
{
    struct input_dev *input_dev = ts_data->input_dev;

    FTS_FUNC_ENTER();
    /*BSP: add XLQEHBY-884 by jiangzhu.yang 20220111 start*/
    input_set_capability(input_dev, EV_KEY, KEY_F26);
    input_set_capability(input_dev, EV_KEY, KEY_F20);
    /*BSP: add XLQEHBY-884 by jiangzhu.yang 20220111 end*/
    /*BSP: add fod feature XLJWHBWSAE-1 by linsheng.liu 20221122 start*/
    input_set_capability(input_dev, EV_KEY, KEY_FINGER);
    /*BSP: add fod feature XLJWHBWSAE-1 by linsheng.liu 20221122 end*/
    /*USD:add for pocket anti accidental touch by longtao.ran 20240605 start*/
    input_set_capability(input_dev, EV_KEY, KEY_LARGE_AREA);
    /*USD:add for pocket anti accidental touch by longtao.ran 20240605 end*/
    input_set_capability(input_dev, EV_KEY, KEY_POWER);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_U);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_UP);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_DOWN);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_LEFT);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_RIGHT);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_O);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_E);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_M);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_L);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_W);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_S);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_V);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_Z);
    input_set_capability(input_dev, EV_KEY, KEY_GESTURE_C);

    __set_bit(KEY_GESTURE_RIGHT, input_dev->keybit);
    __set_bit(KEY_GESTURE_LEFT, input_dev->keybit);
    __set_bit(KEY_GESTURE_UP, input_dev->keybit);
    __set_bit(KEY_GESTURE_DOWN, input_dev->keybit);
    __set_bit(KEY_GESTURE_U, input_dev->keybit);
    __set_bit(KEY_GESTURE_O, input_dev->keybit);
    __set_bit(KEY_GESTURE_E, input_dev->keybit);
    __set_bit(KEY_GESTURE_M, input_dev->keybit);
    __set_bit(KEY_GESTURE_W, input_dev->keybit);
    __set_bit(KEY_GESTURE_L, input_dev->keybit);
    __set_bit(KEY_GESTURE_S, input_dev->keybit);
    __set_bit(KEY_GESTURE_V, input_dev->keybit);
    __set_bit(KEY_GESTURE_C, input_dev->keybit);
    __set_bit(KEY_GESTURE_Z, input_dev->keybit);

    fts_create_gesture_sysfs(ts_data->dev);

    memset(&fts_gesture_data, 0, sizeof(struct fts_gesture_st));
    ts_data->gesture_bmode = GESTURE_BM_REG;
    ts_data->gesture_support = DISABLE;

    if (ts_data->bus_type == BUS_TYPE_SPI) {
        if ((ts_data->ic_info.ids.type <= 0x25)
            || (ts_data->ic_info.ids.type == 0x87)
            || (ts_data->ic_info.ids.type == 0x88)) {
            FTS_INFO("ic type:0x%02x,GESTURE_BM_TOUCH", ts_data->ic_info.ids.type);
            ts_data->touch_size += FTS_GESTURE_DATA_LEN;
            ts_data->gesture_bmode = GESTURE_BM_TOUCH;
        }
    }

    FTS_FUNC_EXIT();
    return 0;
}

int fts_gesture_exit(struct fts_ts_data *ts_data)
{
    FTS_FUNC_ENTER();
    sysfs_remove_group(&ts_data->dev->kobj, &fts_gesture_group);
    FTS_FUNC_EXIT();
    return 0;
}
//USD:add rain touch code by linsheng.liu 20240813 end
