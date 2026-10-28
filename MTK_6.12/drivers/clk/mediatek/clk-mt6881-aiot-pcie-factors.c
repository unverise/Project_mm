// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/trace_events.h>

#include "clk-mtk.h"
#include "clk-pll.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6881-clk-aiot-pcie.h>

/* bringup config */
#define MT_CCF_BRINGUP		0
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1


static const struct mtk_fixed_factor cksys_reg_aiot_pcie_divs[] = {
	FACTOR(CLK_CKSYS_REG_PEXTP_MBIST, "cksys_pextp_mbist_ck",
		"cksys_pextp_mbist_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_TL, "cksys_tl_ck",
		"cksys_tl_sel", 1, 1),
	FACTOR(CLK_CKSYS_REG_MEM_SUB, "cksys_mem_sub_ck",
	"cksys_mem_sub_sel", 1, 1),
};

static int clk_mt6881_cksys_reg_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	//void __iomem *base;
	//struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	//base = devm_ioremap_resource(&pdev->dev, res);
	//if (IS_ERR(base)) {
	//	pr_err("%s(): ioremap failed\n", __func__);
	//	return PTR_ERR(base);
	//}

	clk_data = mtk_alloc_clk_data(CLK_CKSYS_PCIE_REG_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(cksys_reg_aiot_pcie_divs, ARRAY_SIZE(cksys_reg_aiot_pcie_divs),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6881_aiot_pcie_factors[] = {
	{
		.compatible = "mediatek,mt6881-topckgen-aiot-pcie",
		.data = clk_mt6881_cksys_reg_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6881_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6881_aiot_pcie_factors_drv = {
	.probe = clk_mt6881_probe,
	.driver = {
		.name = "clk-mt6881-aiot-pcie-factors",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6881_aiot_pcie_factors,
	},
};

module_platform_driver(clk_mt6881_aiot_pcie_factors_drv);
MODULE_LICENSE("GPL");

