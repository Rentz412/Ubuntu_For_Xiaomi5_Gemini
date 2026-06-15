// SPDX-License-Identifier: GPL-2.0-only
/*
 * DRM driver for LG Display FHD TD4322 DSI command mode panel.
 *
 * Generated from Xiaomi downstream MDSS DSI panel description:
 * arch/arm/boot/dts/qcom/dsi-panel-lgd-fhd-td4322-cmd.dtsi
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct lgd_fhd_td4322 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline struct lgd_fhd_td4322 *to_lgd_fhd_td4322(struct drm_panel *panel)
{
	return container_of(panel, struct lgd_fhd_td4322, panel);
}

static void lgd_fhd_td4322_reset(struct lgd_fhd_td4322 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(100);
}

static int lgd_fhd_td4322_on(struct lgd_fhd_td4322 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0xff);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x00);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, 0x30, 0x00, 0x00, 0x02, 0xa7);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(20);

	mipi_dsi_generic_write_seq(dsi, 0xb0, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x84, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0xc8, 0x11);
	mipi_dsi_generic_write_seq(dsi, 0xca, 0x1c);
	mipi_dsi_generic_write_seq(dsi, 0xb0, 0x03);

	return 0;
}

static int lgd_fhd_td4322_off(struct lgd_fhd_td4322 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	return 0;
}

static int lgd_fhd_td4322_prepare(struct drm_panel *panel)
{
	struct lgd_fhd_td4322 *ctx = to_lgd_fhd_td4322(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	lgd_fhd_td4322_reset(ctx);

	ret = lgd_fhd_td4322_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int lgd_fhd_td4322_unprepare(struct drm_panel *panel)
{
	struct lgd_fhd_td4322 *ctx = to_lgd_fhd_td4322(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = lgd_fhd_td4322_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode lgd_fhd_td4322_mode = {
	.clock = (1080 + 80 + 10 + 40) * (1920 + 4 + 2 + 4) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 80,
	.hsync_end = 1080 + 80 + 10,
	.htotal = 1080 + 80 + 10 + 40,
	.vdisplay = 1920,
	.vsync_start = 1920 + 4,
	.vsync_end = 1920 + 4 + 2,
	.vtotal = 1920 + 4 + 2 + 4,
	.width_mm = 64,
	.height_mm = 114,
};

static int lgd_fhd_td4322_get_modes(struct drm_panel *panel,
				    struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &lgd_fhd_td4322_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs lgd_fhd_td4322_panel_funcs = {
	.prepare = lgd_fhd_td4322_prepare,
	.unprepare = lgd_fhd_td4322_unprepare,
	.get_modes = lgd_fhd_td4322_get_modes,
};

static int lgd_fhd_td4322_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lgd_fhd_td4322 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &lgd_fhd_td4322_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		return ret;
	}

	return 0;
}

static void lgd_fhd_td4322_remove(struct mipi_dsi_device *dsi)
{
	struct lgd_fhd_td4322 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id lgd_fhd_td4322_of_match[] = {
	{ .compatible = "lgd,fhd-td4322" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lgd_fhd_td4322_of_match);

static struct mipi_dsi_driver lgd_fhd_td4322_driver = {
	.probe = lgd_fhd_td4322_probe,
	.remove = lgd_fhd_td4322_remove,
	.driver = {
		.name = "panel-lgd-fhd-td4322",
		.of_match_table = lgd_fhd_td4322_of_match,
	},
};
module_mipi_dsi_driver(lgd_fhd_td4322_driver);

MODULE_AUTHOR("Raffaele Tranquillini <raffaele.tranquillini@gmail.com>");
MODULE_DESCRIPTION("DRM driver for LGD FHD TD4322 DSI panel, command mode");
MODULE_LICENSE("GPL v2");
