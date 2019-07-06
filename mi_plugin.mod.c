#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
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
	{ 0x4d93359a, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xfcfdaeaf, __VMLINUX_SYMBOL_STR(complete_job) },
	{ 0x68c57fa5, __VMLINUX_SYMBOL_STR(unregister_sched_plugin) },
	{ 0xc359d617, __VMLINUX_SYMBOL_STR(register_sched_plugin) },
	{ 0x558b8204, __VMLINUX_SYMBOL_STR(edf_domain_init) },
	{ 0x6caa697d, __VMLINUX_SYMBOL_STR(try_module_get) },
	{ 0x1799162, __VMLINUX_SYMBOL_STR(init_domain_proc_info) },
	{ 0x63c4d61f, __VMLINUX_SYMBOL_STR(__bitmap_weight) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0xc0a3d105, __VMLINUX_SYMBOL_STR(find_next_bit) },
	{ 0x477e59a3, __VMLINUX_SYMBOL_STR(__cpu_online_mask) },
	{ 0x2320bb38, __VMLINUX_SYMBOL_STR(module_put) },
	{ 0x423eb2d1, __VMLINUX_SYMBOL_STR(destroy_domain_proc_info) },
	{ 0xac9d350c, __VMLINUX_SYMBOL_STR(litmus_preemption_in_progress) },
	{ 0x94e6486a, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x93c13efa, __VMLINUX_SYMBOL_STR(bheap_take) },
	{ 0xa24f95c, __VMLINUX_SYMBOL_STR(prepare_for_next_period) },
	{ 0xdae80100, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0x8b775b7d, __VMLINUX_SYMBOL_STR(resched_state) },
	{ 0xe259ae9e, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0xc917e655, __VMLINUX_SYMBOL_STR(debug_smp_processor_id) },
	{ 0x8e899a0d, __VMLINUX_SYMBOL_STR(preempt_if_preemptable) },
	{ 0xbbdb5f29, __VMLINUX_SYMBOL_STR(edf_preemption_needed) },
	{ 0x3a96d243, __VMLINUX_SYMBOL_STR(release_at) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x585dbe01, __VMLINUX_SYMBOL_STR(__add_release) },
	{ 0xf9fc0133, __VMLINUX_SYMBOL_STR(__add_ready) },
	{ 0xc87c1f84, __VMLINUX_SYMBOL_STR(ktime_get) },
	{ 0x1916e38c, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x680ec266, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0x5ecfeec6, __VMLINUX_SYMBOL_STR(__per_cpu_offset) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

