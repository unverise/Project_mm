#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include "semi_touch_interface.h"
#include "head_def.h"
//USD: add spi cs timing by yun.luo start
//#ifdef CONFIG_SPI_MT65XX
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
#include <linux/platform_data/spi-mt65xx.h>
#endif
//#endif
//USD: add spi cs timing by yun.luo end


#define HAL_MAX_TRY                 1

#if SEMI_TOUCH_DMA_TRANSFER
#define DMA_BUFFER_LENGTH                   (MAX_IO_BUFFER_LEN + 4)
unsigned char *dma_txbuff_virtual_addr = NULL;
unsigned char *dma_rxbuff_virtual_addr = NULL;
dma_addr_t dma_txbuff_physical_addr = 0;
dma_addr_t dma_rxbuff_physical_addr = 0;

int spi_write_bytes(struct hal_io_packet* ppacket)
{
    int ret = -1;
    int retry = 0;
    static struct spi_transfer xfer;
    struct spi_device *spi;
    spi = (struct spi_device*)ppacket->hal_adapter;

    memcpy(dma_txbuff_virtual_addr, (unsigned char*)ppacket, ppacket->io_length + sizeof(int));
    memcpy(dma_rxbuff_virtual_addr, (unsigned char*)ppacket, ppacket->io_length + sizeof(int));

    xfer.tx_buf = dma_txbuff_virtual_addr;
    xfer.rx_buf = dma_rxbuff_virtual_addr;
    xfer.tx_dma = dma_txbuff_physical_addr;
    xfer.rx_dma = dma_rxbuff_physical_addr;
    xfer.len = ppacket->io_length + sizeof(int);
    for(retry = 0; retry < HAL_MAX_TRY; retry++)
    {
        ret = spi_sync_transfer(spi, &xfer, 1);
        if(ret == 0) break;
    }

    return ret;
}

int spi_read_bytes(struct hal_io_packet* ppacket)
{
    int ret = -1;
    int retry = 0;
    static struct spi_transfer xfer;
    struct spi_device *spi;
    spi = (struct spi_device*)ppacket->hal_adapter;

    memcpy(dma_txbuff_virtual_addr, (unsigned char*)ppacket, ppacket->io_length + sizeof(int));
    memcpy(dma_rxbuff_virtual_addr, (unsigned char*)ppacket, ppacket->io_length + sizeof(int));


    xfer.tx_buf = dma_txbuff_virtual_addr;
    xfer.rx_buf = dma_rxbuff_virtual_addr;
    xfer.tx_dma = dma_txbuff_physical_addr;
    xfer.rx_dma = dma_rxbuff_physical_addr;
    xfer.len = ppacket->io_length + sizeof(int);
    for(retry = 0; retry < HAL_MAX_TRY; retry++)
    {
        ret = spi_sync_transfer(spi, &xfer, 1);
        if(ret == 0) break;
    }

    memcpy(ppacket->io_buffer, (dma_rxbuff_virtual_addr + sizeof(int)), ppacket->io_length);

    return ret;
}
#if (HAL_INTERFACE_TYPE == HAL_INTERFACE_SPI)
//USD: add spi cs timing by yun.luo start
//#ifdef CONFIG_SPI_MT65XX
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
static struct mtk_chip_config spi_ctrdata = {
    .cs_setuptime = 2100, //25*9.6ns  6us == 625*9.6 ns
    //.rx_mlsb = 1,
    //.tx_mlsb = 1,
};
#endif
//#endif
//USD: add spi cs timing by yun.luo end

int semi_touch_hall_init(void* hal)
{
    struct hal_device *spi = (struct hal_device*)hal;
    st_dev.client = spi;
    st_dev.hal.hal_write_fun = spi_write_bytes;
    st_dev.hal.hal_read_fun = spi_read_bytes;
    st_dev.hal.hal_param = spi;
    spi->mode = SPI_MODE_0;
    spi->max_speed_hz = SPI_MAX_SPEED_HZ;
    spi->bits_per_word = 8;
    //USD:add spi cs timing by  linsheng.liu 20250410 start
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    spi->cs_setup.value = 10; //delay 1
    spi->cs_setup.unit = 0;  //delay us
    #endif
    //USD:add spi cs timing by  linsheng.liu 20250410 end
    //BSP: modify spi frequency  by longtao.ran 20230914 start
    //spi->max_speed_hz = 4000000;
    //BSP: modify spi frequency  by longtao.ran 20230914 end
//USD: add spi cs timing by yun.luo start
//#ifdef CONFIG_SPI_MT65XX
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
    st_dev.client->controller_data = (void *)& spi_ctrdata;
    kernel_log_d("set spi ctrdata dma\n");
#endif
//#endif
//USD: add spi cs timing by yun.luo end

    spi_setup(spi);

    return 0;
}


int semi_touch_hall_init_buff(struct sm_touch_dev *st_dev)
{
    struct hal_device *client = st_dev->client;
    if (NULL == dma_txbuff_virtual_addr && NULL == dma_rxbuff_virtual_addr)
    {
        kernel_log_d("enter semi_touch_hall_init_buff111");
        client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
        dma_txbuff_virtual_addr = (unsigned char *)dma_alloc_coherent(&client->dev, DMA_BUFFER_LENGTH, &dma_txbuff_physical_addr, GFP_KERNEL);
    kernel_log_d("enter semi_touch_hall_init_buff222");
        dma_rxbuff_virtual_addr = (unsigned char *)dma_alloc_coherent(&client->dev, DMA_BUFFER_LENGTH, &dma_rxbuff_physical_addr, GFP_KERNEL);
        kernel_log_d("enter semi_touch_hall_init_buff333");
        if (!dma_txbuff_virtual_addr || !dma_rxbuff_virtual_addr)
        {
            kernel_log_d("Allocate DMA SPI Buffer failed!!");
            return -ENOMEM;
        }
    }

    return 0;
}

int semi_touch_hall_exit(void)
{
    if (dma_txbuff_virtual_addr || dma_rxbuff_virtual_addr)
    {
        dma_free_coherent(&st_dev.client->dev, DMA_BUFFER_LENGTH, dma_txbuff_virtual_addr, dma_txbuff_physical_addr);
        dma_free_coherent(&st_dev.client->dev, DMA_BUFFER_LENGTH, dma_rxbuff_virtual_addr, dma_rxbuff_physical_addr);
        dma_txbuff_virtual_addr = NULL;
        dma_rxbuff_virtual_addr = NULL;
        dma_txbuff_physical_addr = 0;
        dma_rxbuff_physical_addr = 0;
        kernel_log_d("Allocated DMA SPI Buffer release!!");
    }

    return 0;
}
#endif

#else
int spi_write_bytes(struct hal_io_packet* ppacket)
{
    int ret = -1;
    u8 *tx_buf = NULL;
    struct spi_device *spi;
    struct spi_message msg;
    struct spi_transfer xfer;
    spi = (struct spi_device *)ppacket->hal_adapter;

    tx_buf = kzalloc(MAX_IO_BUFFER_LEN + 4, GFP_KERNEL);
    if (!tx_buf)
    {
        kernel_log_e("alloc tx_buf failed, size:%d",
            MAX_IO_BUFFER_LEN + 4);
        return -ENOMEM;
    }
    spi_message_init(&msg);
    memset(&xfer, 0, sizeof(xfer));

    memcpy(tx_buf, (unsigned char *)ppacket, ppacket->io_length + sizeof(int));

    xfer.tx_buf = tx_buf;
    //xfer.rx_buf = (unsigned char *)ppacket;
    xfer.len = ppacket->io_length + sizeof(int);
    xfer.cs_change = 0;

    spi_message_add_tail(&xfer, &msg);
    ret = spi_sync(spi, &msg);
    if (ret < 0) {
        kernel_log_e("spi transfer error:%d", ret);
    }

    kfree(tx_buf);

    return ret;

}

int spi_read_bytes(struct hal_io_packet* ppacket)
{
    int ret = -1;
    u8 *tx_buf = NULL;
    u8 *rx_buf = NULL;
    struct spi_message msg;
    struct spi_transfer xfer;
    struct spi_device * spi;

    spi = (struct spi_device *)ppacket->hal_adapter;

    tx_buf = kzalloc(MAX_IO_BUFFER_LEN + 4, GFP_KERNEL);
    rx_buf = kzalloc(MAX_IO_BUFFER_LEN + 4, GFP_KERNEL);

    if (!rx_buf || !tx_buf)
    {
        kernel_log_e("alloc tx/rx_buf failed, size:%d",
            MAX_IO_BUFFER_LEN + 4);
        return -ENOMEM;
    }

    spi_message_init(&msg);
    memset(&xfer, 0, sizeof(xfer));

    memcpy(tx_buf, (unsigned char*)ppacket, 4);

    xfer.tx_buf = tx_buf;
    xfer.rx_buf = rx_buf;
    xfer.len = ppacket->io_length + sizeof(int);
    xfer.cs_change = 0;

    spi_message_add_tail(&xfer, &msg);
    ret = spi_sync(spi, &msg);
    if (ret < 0) {
        kernel_log_e("spi transfer error:%d", ret);
        goto exit;
    }

    memcpy(ppacket->io_buffer, &rx_buf[4], ppacket->io_length);

    exit:
    kfree(tx_buf);
    kfree(rx_buf);

    return ret;

}
#if (HAL_INTERFACE_TYPE == HAL_INTERFACE_SPI)
//USD: add spi cs timing by yun.luo start
//#ifdef CONFIG_SPI_MT65XX
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
static struct mtk_chip_config spi_ctrdata = {
    .cs_setuptime = 2100, //25*9.6ns  6us == 625*9.6 ns
    //.rx_mlsb = 1,
    //.tx_mlsb = 1,
};
#endif
//#endif
//USD: add spi cs timing by yun.luo end

int semi_touch_hall_init(void* hal)
{
    struct hal_device *spi = (struct hal_device*)hal;
    st_dev.client = spi;
    st_dev.hal.hal_write_fun = spi_write_bytes;
    st_dev.hal.hal_read_fun = spi_read_bytes;
    st_dev.hal.hal_param = spi;
    spi->mode = SPI_MODE_0;
    spi->max_speed_hz = SPI_MAX_SPEED_HZ;
    spi->bits_per_word = 8;
    //USD:add spi cs timing by  linsheng.liu 20250410 start
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    spi->cs_setup.value = 10; //delay 1
    spi->cs_setup.unit = 0;  //delay us
    #endif
    //USD:add spi cs timing by  linsheng.liu 20250410 end
    //BSP: modify spi frequency  by longtao.ran 20230914 start
    //spi->max_speed_hz = 4000000;
    //BSP: modify spi frequency  by longtao.ran 20230914 end

//USD: add spi cs timing by yun.luo start
//#ifdef CONFIG_SPI_MT65XX
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
    st_dev.client->controller_data = (void *)& spi_ctrdata;
    kernel_log_d("set spi ctrdata \n");
#endif
//#endif
//USD: add spi cs timing by yun.luo end

    spi_setup(spi);

    return 0;
}
int semi_touch_hall_exit(void)
{
    return 0;
}
#endif
#endif
