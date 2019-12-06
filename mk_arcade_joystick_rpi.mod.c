#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xac902c3f, "module_layout" },
	{ 0x4845c423, "param_array_ops" },
	{ 0x15692c87, "param_ops_int" },
	{ 0xc2165d85, "__arm_iounmap" },
	{ 0x3b05df25, "malloc_sizes" },
	{ 0x37a0cba, "kfree" },
	{ 0x20cf8094, "input_unregister_device" },
	{ 0x7876cebf, "input_free_device" },
	{ 0xa874d9b2, "input_register_device" },
	{ 0xfb176b98, "input_set_abs_params" },
	{ 0x6cc0821a, "dev_set_drvdata" },
	{ 0xb81960ca, "snprintf" },
	{ 0x8d0c0b04, "input_allocate_device" },
	{ 0xfb0e29f, "init_timer_key" },
	{ 0xb04877fb, "__mutex_init" },
	{ 0x3d22b4f3, "kmem_cache_alloc_trace" },
	{ 0x40a6f522, "__arm_ioremap" },
	{ 0x27e1a049, "printk" },
	{ 0x37befc70, "jiffies_to_msecs" },
	{ 0x7d11c268, "jiffies" },
	{ 0x8834396c, "mod_timer" },
	{ 0x77ffa018, "mutex_lock_interruptible" },
	{ 0x1883a8d6, "mutex_unlock" },
	{ 0xd5f2172f, "del_timer_sync" },
	{ 0x5f4b4b97, "mutex_lock" },
	{ 0xc14bed20, "dev_get_drvdata" },
	{ 0x301b91ee, "input_event" },
	{ 0x2e5810c6, "__aeabi_unwind_cpp_pr1" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

