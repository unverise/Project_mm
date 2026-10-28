// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 * Author: Claude Yen <claude.yen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6881-clk-aiot-pcie.h>

#define MT_CCF_BRINGUP		0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs pext_cg_regs = {
	.set_ofs = 0x18,
	.clr_ofs = 0x1C,
	.sta_ofs = 0x14,
};

#define GATE_PEXT(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &pext_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_PEXT_V(_id, _name, _parent) {    \
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
	}

static const struct mtk_gate pext_clks[] = {
	GATE_PEXT(CLK_PEXTP_P0_MAC_REF, "pextp_p0_mac_ref",
			"cksys_f26m_ck"/* parent */, 0),
	GATE_PEXT_V(CLK_PEXTP_P0_MAC_REF_PCIE, "pextp_p0_mac_ref_pcie",
			"pextp_p0_mac_ref"/* parent */),
	GATE_PEXT(CLK_PEXTP_P0_MAC_PL_P, "pextp_p0_mac_pl_p",
			"cksys_pextp_mbist_ck"/* parent */, 1),
	GATE_PEXT_V(CLK_PEXTP_P0_MAC_PL_P_PCIE, "pextp_p0_mac_pl_p_pcie",
			"pextp_p0_mac_pl_p"/* parent */),
	GATE_PEXT(CLK_PEXTP_P0_MAC_TL, "pextp_p0_mac_tl",
			"cksys_tl_ck"/* parent */, 2),
	GATE_PEXT_V(CLK_PEXTP_P0_MAC_TL_PCIE, "pextp_p0_mac_tl_pcie",
			"pextp_p0_mac_tl"/* parent */),
	GATE_PEXT(CLK_PEXTP_P0_MAC_AXI, "pextp_p0_mac_axi",
			"cksys_mem_sub_ck"/* parent */, 3),
	GATE_PEXT_V(CLK_PEXTP_P0_MAC_AXI_PCIE, "pextp_p0_mac_axi_pcie",
			"pextp_p0_mac_axi"/* parent */),
	GATE_PEXT(CLK_PEXTP_P0_MAC_AHB_APB, "pextp_p0_mac_ahb_apb",
			"cksys_axi_post_ck"/* parent */, 4),
	GATE_PEXT_V(CLK_PEXTP_P0_MAC_AHB_APB_PCIE, "pextp_p0_mac_ahb_apb_pcie",
			"pextp_p0_mac_ahb_apb"/* parent */),
	GATE_PEXT(CLK_PEXTP_P0_PHY_REF, "pextp_p0_phy_ref",
			"cksys_f26m_ck"/* parent */, 5),
	GATE_PEXT_V(CLK_PEXTP_P0_PHY_REF_PCIE, "pextp_p0_phy_ref_pcie",
			"pextp_p0_phy_ref"/* parent */),
	GATE_PEXT(CLK_PEXTP_P0_PHY_MCBUS, "pextp_p0_phy_mcbus",
			"cksys_f26m_ck"/* parent */, 6),
	GATE_PEXT_V(CLK_PEXTP_P0_PHY_MCBUS_PCIE, "pextp_p0_phy_mcbus_pcie",
			"pextp_p0_phy_mcbus"/* parent */),
};

static const struct mtk_clk_desc pext_mcd = {
	.clks = pext_clks,
	.num_clks = CLK_PEXT_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6881_aiot_pcie_cg[] = {
	{
		.compatible = "mediatek,mt6881-pextpcfg_ao",
		.data = &pext_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6881_cg_aiot_pcie_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6881_cg_aiot_pcie_drv = {
	.probe = clk_mt6881_cg_aiot_pcie_grp_probe,
	.driver = {
		.name = "clk-mt6881-aiot-pcie-cg",
		.of_match_table = of_match_clk_mt6881_aiot_pcie_cg,
	},
};

module_platform_driver(clk_mt6881_cg_aiot_pcie_drv);
MODULE_LICENSE("GPL");
