/*  
 *  cmpe283-1.c - Kernel module for CMPE283 assignment 1
 */
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <asm/msr.h>

#define MAX_MSG 80
#define IA32_VMX_PINBASED_CTLS 0x481
#define IA32_VMX_PROCBASED_CTLS 0x482
#define IA32_VMX_PROCBASED_CTLS2 0x48B
#define IA32_VMX_EXIT_CTLS 0x483
#define IA32_VMX_ENTRY_CTLS 0x484
#define IA32_VMX_PROCBASED_CTLS3 0x492

struct capability_info {
	uint8_t bit;
	const char *name;
};

struct capability_info proc[22] =
{
	{ 2, "INTERRUPT_WINDOW_EXITING" },
	{ 3, "USE_TSC_OFFSETTING" },
	{ 7, "HLT_EXITING" },
	{ 9, "INVLPG_EXITING" },
	{ 10, "MWAIT_EXITING" },
	{ 11, "RDPMC_EXITING" },
	{ 12, "RDTSC_EXITING" },
	{ 15, "CR3_LOAD_EXITING" },
	{ 16, "CR3_STORE_EXITING" },
      	{ 17, "ACTIVATE_TERTIARY_CONTROLS"},
	{ 19, "CR8_LOAD_EXITING" },
	{ 20, "CR8_STORE_EXITING" },
	{ 21, "USE_TPR_SHADOW" },
	{ 22, "NMI_WINDOW_EXITING" },
	{ 23, "MOV_DR_EXITING" },
	{ 24, "UNCONDITIONAL_IO_EXITING" },
	{ 25, "USE_IO_BITMAPS" },
	{ 27, "MONITOR_TRAP_FLAG" },
	{ 28, "USE_MSR_BITMAPS" },
	{ 29, "MONITOR_EXITING" },
	{ 30, "PAUSE_EXITING" },
	{ 31, "ACTIVATE_SECONDARY_CONTROLS" }
};

struct capability_info pin[5] =
{
	{ 0, "External Interrupt Exiting" },
	{ 3, "NMI Exiting" },
	{ 5, "Virtual NMIs" },
	{ 6, "Activate VMX Preemption Timer" },
	{ 7, "Process Posted Interrupts" }
};


struct capability_info exit1[14] =
{
	{ 2, "SAVE_DEBUG_CONTROLS"},
	{ 9, "HOST_ADDRESS_SAPCE_SIZE"},
	{ 12, "LOAD_IA32_PERF_GLOBAL_CTRL"},
	{ 15, "ACKNOWLEDGE_INTERRUPT_ON_EXIT"},
	{ 18, "SAVE_IA32_PAT"},
	{ 19, "LOAD_IA32_PAT"},
	{ 20, "SAVE_IA32_EFER"},
	{ 21, "LOAD_IA32_EFER"},
	{ 22, "SAVE_VMX_PREEMPTION_TIMER_VALUE"},
	{ 23, "CLEAR_IA32_BNDCFGS"},
	{ 24, "CONCEAL_VMX_FROM_PT"},
	{ 25, "CLEAR_IA32_RTIT_CTL"},
	{ 28, "LOAD_CET_STATE"},
	{ 29, "LOAD_PKRS"}
};



struct capability_info procBase[27] =
{
	{ 0, "VIRTUALIZE_APIC_ACCESSES" },
	{ 1, "ENABLE_EPT" },
	{ 2, "DESCRIPTOR_TABLE_EXITING" },
	{ 3, "ENABLE_RDTSCP" },
	{ 4, "VIRTUALIZE_X2APIC_MODE" },
	{ 5, "ENABLE_VPID" },
	{ 6, "WBINVD_EXITING" },
	{ 7, "UNRESTRICTED_GUEST" },
	{ 8, "APIC_REGISTER_VIRTUALIZATION" },
	{ 9, "VIRTUAL_INTERRUPT_DELIVERY" },
	{ 10, "PAUSE_LOOP_EXITING" },
	{ 11, "RDRAND_EXITING" },
	{ 12, "ENABLE_INVPCID" },
	{ 13, "ENABLE_VM_FUNCTIONS" },
	{ 14, "VMCS_SHADOWING" },
	{ 15, "ENABLE_ENCLS_EXITING" },
	{ 16, "RDSEED_EXITING" },
	{ 17, "ENABLE_PML" },
	{ 18, "EPT_VIOLATION_#VE" },
	{ 19, "CONCEAL_VMX_FROM_PT" },
	{ 20, "ENABLE_XSAVES/XRSTORS" },
	{ 22, "MODE_BASED_EXECUTE_CONTROL_FOR_EPT"},
	{ 23, "SUB_PAGE_WRITE_PERMISSIONS_FOR_EPT"},
	{ 24, "INTEL_PT_USES_GUEST_PHYSICAL_ADDRESSES"},
	{ 25, "USE_TSC_SCALING"},
	{ 26, "ENABLE_USER_WAIT_AND_PAUSE"},
	{ 28, "ENABLE_ENCLV_EXITING"}
};

struct capability_info tertiaryProcbased[4] =
{
	{ 0, "LOADIWKEY Exiting" },	
    	{ 1, "Enable HLAT" },
    	{ 2, "EPT Paging-write Control" },
    	{ 3, "Guest-paging Verification" }
};


struct capability_info entry[12] =
{
	{ 2, "LOAD_DEBUG_CONTROLS"},
	{ 9, "IA_32E_MODE_GUEST"},
	{ 10, "ENTRY_TO_SMM"},
	{ 11, "DEACTIVATE_DUAL_MONITOR_TREATMENT"},
	{ 13, "LOAD_IA32_PERF_GLOBA_L_CTRL"},
	{ 14, "LOAD_IA32_PAT"},
	{ 15, "LOAD_IA32_EFER"},
	{ 16, "LOAD_IA32_BNDCFGS"},
	{ 17, "CONCEAL_VMX_FROM_PT"},
	{ 18, "LOAD_IA32_RTIT_CTL"},
	{ 20, "LOAD_CET_STATE"},
	{ 22, "LOAD_PKRS"}
};

void
report_capability(struct capability_info *cap, uint8_t len, uint32_t lo,
    uint32_t hi)
{
	uint8_t i;
	struct capability_info *c;
	char msg[MAX_MSG];

	memset(msg, 0, sizeof(msg));

	for (i = 0; i < len; i++) {
		c = &cap[i];
		snprintf(msg, 79, "  %s: Can set=%s, Can clear=%s\n",
		    c->name,
		    (hi & (1 << c->bit)) ? "Yes" : "No",
		    !(lo & (1 << c->bit)) ? "Yes" : "No");
		printk(msg);
	}
}

/*
 * detect_vmx_features
 */
void
detect_vmx_features(void)
{
	uint32_t lo, hi;

	/* pin controls */
	rdmsr(IA32_VMX_PINBASED_CTLS, lo, hi);
	pr_info("pin Controls MSR: 0x%llx\n",
		(uint64_t)(lo | (uint64_t)hi << 32));
	report_capability(pin, 5, lo, hi);
	
	/* Procbased controls */
	rdmsr(IA32_VMX_PROCBASED_CTLS, lo, hi);
	pr_info("procbased Controls MSR: 0x%llx\n",
		(uint64_t)(lo | (uint64_t)hi << 32));
	report_capability(proc, 22, lo, hi);

	
	/* Procbased secondary controls */
	rdmsr(IA32_VMX_PROCBASED_CTLS2, lo, hi);
	pr_info("Procbased Secondary Controls MSR: 0x%llx\n",
		(uint64_t)(lo | (uint64_t)hi << 32));
	report_capability(procBase, 27, lo, hi);

	rdmsr(IA32_VMX_PROCBASED_CTLS3, lo, hi);
	pr_info("Procbased tertiary Controls MSR: 0x%llx\n",
		(uint64_t)(lo | (uint64_t)hi << 32));
	report_capability(tertiaryProcbased, 4, lo, hi);


	
	/* Exit controls */
	rdmsr(IA32_VMX_EXIT_CTLS, lo, hi);
	pr_info("Exit Controls MSR: 0x%llx\n",
		(uint64_t)(lo | (uint64_t)hi << 32));
	report_capability(exit1, 14, lo, hi);

	
	/* Entry controls */
	rdmsr(IA32_VMX_ENTRY_CTLS, lo, hi);
	pr_info("Entry Controls MSR: 0x%llx\n",
		(uint64_t)(lo | (uint64_t)hi << 32));
	report_capability(entry, 12, lo, hi);


}

/*
 * Return Values:
 */
int
init_module(void)
{
	printk(KERN_INFO "CMPE 283 Assignment 1 Module Start\n");

	detect_vmx_features();

	/* 
	 * A non 0 return means init_module failed; module can't be loaded. 
	 */
	return 0;
}

/*
 * cleanup_module
 */
void
cleanup_module(void)
{
	printk(KERN_INFO "CMPE 283 Assignment 1 Module Exits\n");
}