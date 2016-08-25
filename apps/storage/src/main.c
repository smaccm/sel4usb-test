/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <allocman/bootstrap.h>
#include <allocman/vka.h>
#include <vka/capops.h>
#include <vka/object.h>

#include <vspace/vspace.h>
#include <simple/simple.h>
#include <simple-default/simple-default.h>
#include <platsupport/io.h>
#include <sel4platsupport/platsupport.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/arch/io.h>

#include <cpio/cpio.h>

#include <sel4utils/irq_server.h>
#include <sel4utils/vspace.h>
#include <sel4utils/thread.h>
#include <sel4utils/mapping.h>
#include <dma/dma.h>

#include <usb/drivers/storage.h>
#include <usb/drivers/cdc.h>
#include <usb/drivers/pl2303.h>

#include <sync/mutex.h>

#define DMA_VSTART  0x40000000

#ifndef DEBUG_BUILD
#define seL4_DebugHalt() do{ printf("Halting...\n"); while(1); } while(0)
#endif

/* allocator static pool */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 32)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

vka_t _vka;
simple_t _simple;
vspace_t _vspace;
sel4utils_alloc_data_t _alloc_data;
allocman_t *_allocator;
seL4_CPtr _fault_endpoint;
irq_server_t _irq_server;
sync_mutex_t _mutex;

struct ps_io_ops _io_ops;

extern char _cpio_archive[];


static void
print_cpio_info(void)
{
    struct cpio_info info;
    const char* name;
    unsigned long size;
    int i;

    cpio_info(_cpio_archive, &info);

    printf("CPIO: %d files found.\n", info.file_count);
    assert(info.file_count > 0);
    for (i = 0; i < info.file_count; i++) {
        void * addr;
        char buf[info.max_path_sz + 1];
        buf[info.max_path_sz] = '\0';
        addr = cpio_get_entry(_cpio_archive, i, &name, &size);
        assert(addr);
        strncpy(buf, name, info.max_path_sz);
        printf("%d) %-20s  0x%08x, %8ld bytes\n", i, buf, (uint32_t)addr, size);
    }
    printf("\n");
}

void debug_print_bootinfo(seL4_BootInfo *info)
{
}

static int
_dma_morecore(size_t min_size, int cached, struct dma_mem_descriptor* dma_desc)
{
    static uint32_t _vaddr = DMA_VSTART;
    struct seL4_ARCH_Page_GetAddress getaddr_ret;
    seL4_CPtr frame;
    seL4_CPtr pd;
    vka_t* vka;
    int err;

    pd = simple_get_pd(&_simple);
    vka = &_vka;

    /* Create a frame */
    frame = vka_alloc_frame_leaky(vka, 12);
    assert(frame);
    if (!frame) {
        return -1;
    }

    /* Try to map the page */
    err = seL4_ARCH_Page_Map(frame, pd, _vaddr, seL4_AllRights, 0);
    if (err) {
        seL4_CPtr pt;
        /* Allocate a page table */
        pt = vka_alloc_page_table_leaky(vka);
        if (!pt) {
            printf("Failed to create page table\n");
            return -1;
        }
        /* Map the page table */
        err = seL4_ARCH_PageTable_Map(pt, pd, _vaddr, 0);
        if (err) {
            printf("Failed to map page table\n");
            return -1;
        }
        /* Try to map the page again */
        err = seL4_ARCH_Page_Map(frame, pd, _vaddr, seL4_AllRights, 0);
        if (err) {
            printf("Failed to map page\n");
            return -1;
        }

    }

    /* Find the physical address of the page */
    getaddr_ret = seL4_ARCH_Page_GetAddress(frame);
    assert(!getaddr_ret.error);
    /* Setup dma memory description */
    dma_desc->vaddr = _vaddr;
    dma_desc->paddr = getaddr_ret.paddr;
    dma_desc->cached = 0;
    dma_desc->size_bits = 12;
    dma_desc->alloc_cookie = (void*)frame;
    dma_desc->cookie = NULL;
    /* Advance the virtual address marker */
    _vaddr += BIT(12);
    return 0;
}

static int
vmm_init(void)
{
    vka_object_t fault_ep_obj;
    vka_t* vka;
    simple_t* simple;
    vspace_t* vspace;
    int err;

    vka = &_vka;
    vspace = &_vspace;
    simple = &_simple;
    fault_ep_obj.cptr = 0;

    simple_default_init_bootinfo(simple, seL4_GetBootInfo());

    _allocator = bootstrap_use_current_simple(simple, ALLOCATOR_STATIC_POOL_SIZE,
                                              allocator_mem_pool);
    assert(_allocator);

    allocman_make_vka(vka, _allocator);

    err = sel4utils_bootstrap_vspace_with_bootinfo_leaky(vspace,
                                                         &_alloc_data,
                                                         seL4_CapInitThreadPD,
                                                         vka,
                                                         seL4_GetBootInfo());
    assert(!err);

    /* Initialise device support */
    err = sel4platsupport_new_io_mapper(*simple, *vspace, *vka,
                                        &_io_ops.io_mapper);
    assert(!err);

#ifdef ARCH_X86
    err = sel4platsupport_get_io_port_ops(&_io_ops.io_port_ops, simple);
    assert(!err);
#endif

    /* Setup debug port: printf() is only reliable after this */
    platsupport_serial_setup_simple(NULL, simple, vka);

    /* Initialise MUX subsystem */
#ifdef ARCH_ARM
    err = mux_sys_init(&_io_ops, &_io_ops.mux_sys);
    assert(!err);
#endif

    /* Initialise DMA */
    err = dma_dmaman_init(&_dma_morecore, NULL, &_io_ops.dma_manager);
    assert(!err);

    /* Allocate an endpoint for listening to events */
    err = vka_alloc_endpoint(vka, &fault_ep_obj);
    assert(!err);
    _fault_endpoint = fault_ep_obj.cptr;

    vka_object_t mutex_aep_obj;
    err = vka_alloc_notification(vka, &mutex_aep_obj);
    assert(!err);
    sync_mutex_init(&_mutex, mutex_aep_obj.cptr);

    /* Create an IRQ server */
    err = irq_server_new(vspace, vka, simple_get_cnode(simple), 254,
                         simple, /*seL4_CapNull*/fault_ep_obj.cptr,
                         0xCAFE, 256, &_irq_server);
    assert(!err);

    return 0;
}

usb_t *usb = NULL;

static void
usb_irq_handler(struct irq_data *irq_data)
{
    usb_t *usb_priv = (usb_host_t*)irq_data->token;
    usb_handle_irq(usb_priv);
    irq_data_ack_irq(irq_data);
}

static inline void udelay(uint32_t us)
{
	volatile uint32_t i;
	for (; us > 0; us--) {
		for (i = 0; i < 1000; i++);
	}
}
extern void set_flipper_effort(usb_dev_t udev, int8_t effort);
extern void clear_fault(usb_dev_t udev, uint16_t fault);
extern void set_status(usb_dev_t udev, uint8_t status);
extern void set_flipper_postion(usb_dev_t udev, int angle, int velocity);
extern uint16_t report_flipper_postion(usb_dev_t udev);
static void
usb_cdc_test(usb_dev_t udev)
{
	uint16_t angle = 0;
	struct acm_line_coding coding;
	coding.dwDTERate = 115200;
	coding.bCharFormat = ACM_STOP_1BIT;;
	coding.bParityType = ACM_PARITY_NONE;
	coding.bDataBits = 8;

	usb_cdc_bind(udev);

	acm_set_line_coding(udev, &coding);

	acm_set_ctrl_line_state(udev, ACM_CTRL_RTS | ACM_CTRL_DTR);

	clear_fault(udev, 0xFFFF);
	udelay(1000);

	set_status(udev, 4);
	udelay(1000);

	angle = report_flipper_postion(udev);
	printf("Flipper angle: %u\n", angle);

	set_flipper_effort(udev, 20);
	udelay(4000);

	angle = report_flipper_postion(udev);
	printf("Flipper angle: %u\n", angle);
}

static void
usb_serial_test(usb_dev_t udev)
{
	usb_pl2303_bind(udev);
}

static void
usb_test(void)
{
    int err;
    usb_dev_t usb_storage = NULL;
    seL4_Word badge;
    sel4utils_thread_t thread;

    while (1) {
        usb_storage = usb_get_device(usb, 3);
	if (usb_storage) {
            break;
	}
    }

//    for (int i = 1; i <= 9; i++) {
//	    usb_storage = usb_get_device(usb, i);
//	    if (usb_storage->prod_id == 0x0008) {
//		    printf("Found Flipper: %u\n", i);
//		    break;
//	    }
//    }

//    usb_storage_bind(usb_storage, &_mutex);
    usb_lsusb(usb, 1);
//    usb_cdc_test(usb_storage);
    usb_serial_test(usb_storage);

    seL4_DebugHalt();
}

int
main(void)
{
    int err;
    struct irq_data *irq_data;
    sel4utils_thread_t thread;
    const int *irq;

    err = vmm_init();
    assert(!err);

    usb = malloc(sizeof(usb_t));

    err = usb_init(USB_HOST_DEFAULT, &_io_ops, usb);
    assert(!err);

    irq = usb_host_irqs(&usb->hdev, NULL);
    irq_data = irq_server_register_irq(_irq_server, irq[0], usb_irq_handler, usb);
    assert(irq_data);
    

    /* Create test thread */
    err = sel4utils_configure_thread(&_simple, &_vka, &_vspace, &_vspace, seL4_CapNull, 253,
		                     simple_get_cnode(&_simple), seL4_NilData, &thread);
    assert(!err);

    err = sel4utils_start_thread(&thread, usb_test, NULL, NULL, 1);
    assert(!err);

    /* Loop forever, handling events */
    while (1) {
        seL4_MessageInfo_t tag;
        seL4_Word sender_badge;

        tag = seL4_Recv(_fault_endpoint, &sender_badge);
        if (sender_badge == 0) {
            seL4_Word label;
            label = seL4_MessageInfo_get_label(tag);
            if (label == 0xCAFE) {
                irq_server_handle_irq_ipc(_irq_server);
            } else {
                printf("Unknown label (%d) for IPC badge %d\n", label, sender_badge);
            }
        } else {
            seL4_DebugHalt();
            while (1);
        }
    }

    return 0;
}

