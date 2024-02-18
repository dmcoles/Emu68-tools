#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>

#include "wifipi.h"
#include "sdio.h"
#include "brcm.h"
#include "brcm_sdio.h"
#include "brcm_chipcommon.h"

#define D(x) x

#define TIMEOUT_WAIT(check_func, tout) \
    do { ULONG cnt = (tout) / 2; if (cnt == 0) cnt = 1; while(cnt != 0) { if (check_func) break; \
    cnt = cnt - 1; delay_us(2, WiFiBase); }  } while(0)

void delay_us(ULONG us, struct WiFiBase *WiFiBase)
{
    (void)WiFiBase;
    ULONG timer = LE32(*(volatile ULONG*)0xf2003004);
    ULONG end = timer + us;

    if (end < timer) {
        while (end < LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
    }
    while (end > LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
}

// Set the clock dividers to generate a target value
ULONG get_clock_divider(ULONG base_clock, ULONG target_rate)
{
    ULONG targetted_divisor = 0;
    if(target_rate > base_clock)
    {
        targetted_divisor = 1;
    }
    else
    {
        targetted_divisor = base_clock / target_rate;
        ULONG mod = base_clock % target_rate;
        if(mod) {
            targetted_divisor--;
        }
    }

    // Decide on the clock mode to use

    // Currently only 10-bit divided clock mode is supported

    // HCI version 3 or greater supports 10-bit divided clock mode
    // This requires a power-of-two divider

    // Find the first bit set
    int divisor = -1;
    for(int first_bit = 31; first_bit >= 0; first_bit--)
    {
        ULONG bit_test = (1 << first_bit);
        if(targetted_divisor & bit_test)
        {
            divisor = first_bit;
            targetted_divisor &= ~bit_test;
            if(targetted_divisor)
            {
                // The divisor is not a power-of-two, increase it
                divisor++;
            }
            break;
        }
    }

    if(divisor == -1)
        divisor = 31;
    if(divisor >= 32)
        divisor = 31;

    if(divisor != 0)
        divisor = (1 << (divisor - 1));

    if(divisor >= 0x400)
        divisor = 0x3ff;

    ULONG freq_select = divisor & 0xff;
    ULONG upper_bits = (divisor >> 8) & 0x3;
    ULONG ret = (freq_select << 8) | (upper_bits << 6) | (0 << 5);

    return ret;
}

// Switch the clock rate whilst running
int switch_clock_rate(ULONG base_clock, ULONG target_rate, struct WiFiBase *WiFiBase)
{
    // Decide on an appropriate divider
    ULONG divider = get_clock_divider(base_clock, target_rate);

    // Wait for the command inhibit (CMD and DAT) bits to clear
    while(rd32(WiFiBase->w_SDIO, EMMC_STATUS) & 0x3)
        delay_us(1000, WiFiBase);

    // Set the SD clock off
    ULONG control1 = rd32(WiFiBase->w_SDIO, EMMC_CONTROL1);
    control1 &= ~(1 << 2);
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL1, control1);
    delay_us(2000, WiFiBase);

    // Write the new divider
    control1 &= ~0xffe0;		// Clear old setting + clock generator select
    control1 |= divider;
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL1, control1);
    delay_us(2000, WiFiBase);

    // Enable the SD clock
    control1 |= (1 << 2);
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL1, control1);
    delay_us(2000, WiFiBase);

    return 0;
}

void cmd_int(ULONG cmd, ULONG arg, ULONG timeout, struct WiFiBase *WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    ULONG tout = 0;
    WiFiBase->w_LastCMDSuccess = 0;

    // Check Command Inhibit
    while(rd32(WiFiBase->w_SDIO, EMMC_STATUS) & 0x1)
        delay_us(10, WiFiBase);

    // Is the command with busy?
    if((cmd & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B)
    {
        // With busy

        // Is is an abort command?
        if((cmd & SD_CMD_TYPE_MASK) != SD_CMD_TYPE_ABORT)
        {
            // Not an abort command

            // Wait for the data line to be free
            while(rd32(WiFiBase->w_SDIO, EMMC_STATUS) & 0x2)
                delay_us(10, WiFiBase);
        }
    }

    ULONG blksizecnt = WiFiBase->w_BlockSize | (WiFiBase->w_BlocksToTransfer << 16);

    wr32(WiFiBase->w_SDIO, EMMC_BLKSIZECNT, blksizecnt);

    // Set argument 1 reg
    wr32(WiFiBase->w_SDIO, EMMC_ARG1, arg);

    // Set command reg
    wr32(WiFiBase->w_SDIO, EMMC_CMDTM, cmd);

    asm volatile("nop");
    //SDCardBase->sd_Delay(10, SDCardBase);

    // Wait for command complete interrupt
    TIMEOUT_WAIT((rd32(WiFiBase->w_SDIO, EMMC_INTERRUPT) & 0x8001), timeout);
    ULONG irpts = rd32(WiFiBase->w_SDIO, EMMC_INTERRUPT);

    // Clear command complete status
    wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, 0xffff0001);

    // Test for errors
    if((irpts & 0xffff0001) != 0x1)
    {
        D(bug("[WiFI] error occured whilst waiting for command complete interrupt (%08lx), status: %08lx\n", irpts, rd32(WiFiBase->w_SDIO, EMMC_STATUS)));

        WiFiBase->w_LastError = irpts & 0xffff0000;
        WiFiBase->w_LastInterrupt = irpts;
        return;
    }

    // SDCardBase->sd_Delay(10, SDCardBase);
    asm volatile("nop");

    // Get response data
    switch(cmd & SD_CMD_RSPNS_TYPE_MASK)
    {
        case SD_CMD_RSPNS_TYPE_48:
        case SD_CMD_RSPNS_TYPE_48B:
            WiFiBase->w_Res0 = rd32(WiFiBase->w_SDIO, EMMC_RESP0);
            break;

        case SD_CMD_RSPNS_TYPE_136:
            WiFiBase->w_Res0 = rd32(WiFiBase->w_SDIO, EMMC_RESP0);
            WiFiBase->w_Res1 = rd32(WiFiBase->w_SDIO, EMMC_RESP1);
            WiFiBase->w_Res2 = rd32(WiFiBase->w_SDIO, EMMC_RESP2);
            WiFiBase->w_Res3 = rd32(WiFiBase->w_SDIO, EMMC_RESP3);
            break;
    }

    // If with data, wait for the appropriate interrupt
    if(cmd & SD_CMD_ISDATA)
    {
        ULONG wr_irpt;
        int is_write = 0;
        if(cmd & SD_CMD_DAT_DIR_CH)
            wr_irpt = (1 << 5);     // read
        else
        {
            is_write = 1;
            wr_irpt = (1 << 4);     // write
        }

        int cur_block = 0;
        ULONG *cur_buf_addr = (ULONG *)WiFiBase->w_Buffer;
        while(cur_block < WiFiBase->w_BlocksToTransfer)
        {
            tout = timeout / 100;
            TIMEOUT_WAIT((rd32(WiFiBase->w_SDIO, EMMC_INTERRUPT) & (wr_irpt | 0x8000)), timeout);
            irpts = rd32(WiFiBase->w_SDIO, EMMC_INTERRUPT);
            wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, 0xffff0000 | wr_irpt);

            if((irpts & (0xffff0000 | wr_irpt)) != wr_irpt)
            {
                D(bug("[WiFi] error occured whilst waiting for data ready interrupt (%08lx)\n", irpts));

                WiFiBase->w_LastError = irpts & 0xffff0000;
                WiFiBase->w_LastInterrupt = irpts;
                return;
            }

            // Transfer the block
            UWORD cur_byte_no = 0;
            while(cur_byte_no < WiFiBase->w_BlockSize)
            {
                if(is_write)
                {
                    ULONG data = *(ULONG*)cur_buf_addr;
                    wr32be(WiFiBase->w_SDIO, EMMC_DATA, data);
                }
                else
                {
                    ULONG data = rd32be(WiFiBase->w_SDIO, EMMC_DATA);
                    *(ULONG*)cur_buf_addr = data;
                }
                cur_byte_no += 4;
                cur_buf_addr++;
            }

            cur_block++;
        }
    }
    // Wait for transfer complete (set if read/write transfer or with busy)
    if((((cmd & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B) ||
       (cmd & SD_CMD_ISDATA)))
    {
        // First check command inhibit (DAT) is not already 0
        if((rd32(WiFiBase->w_SDIO, EMMC_STATUS) & 0x2) == 0)
            wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, 0xffff0002);
        else
        {
            TIMEOUT_WAIT((rd32(WiFiBase->w_SDIO, EMMC_INTERRUPT) & 0x8002), timeout);
            irpts = rd32(WiFiBase->w_SDIO, EMMC_INTERRUPT);
            wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, 0xffff0002);

            // Handle the case where both data timeout and transfer complete
            //  are set - transfer complete overrides data timeout: HCSS 2.2.17
            if(((irpts & 0xffff0002) != 0x2) && ((irpts & 0xffff0002) != 0x100002))
            {
                D(bug("[WiFi] error occured whilst waiting for transfer complete interrupt (%08lx)\n", irpts));
                WiFiBase->w_LastError = irpts & 0xffff0000;
                WiFiBase->w_LastInterrupt = irpts;
                return;
            }
            wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, 0xffff0002);
        }
    }
    WiFiBase->w_LastCMDSuccess = 1;
}

// Reset the CMD line
static int reset_cmd(struct WiFiBase *WiFiBase)
{
    int tout = 1000000;
    ULONG control1 = rd32(WiFiBase->w_SDIO, EMMC_CONTROL1);
    control1 |= SD_RESET_CMD;
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL1, control1);
    while (tout && (rd32(WiFiBase->w_SDIO, EMMC_CONTROL1) & SD_RESET_CMD) != 0) {
        delay_us(1, WiFiBase);
        tout--;
    }
    if((rd32(WiFiBase->w_SDIO, EMMC_CONTROL1) & SD_RESET_CMD) != 0)
    {
        return -1;
    }
    return 0;
}

// Reset the CMD line
static int reset_dat(struct WiFiBase *WiFiBase)
{
    int tout = 1000000;
    ULONG control1 = rd32(WiFiBase->w_SDIO, EMMC_CONTROL1);
    control1 |= SD_RESET_DAT;
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL1, control1);
    while (tout && (rd32(WiFiBase->w_SDIO, EMMC_CONTROL1) & SD_RESET_DAT) != 0) {
        delay_us(1, WiFiBase);
        tout--;
    }
    if((rd32(WiFiBase->w_SDIO, EMMC_CONTROL1) & SD_RESET_DAT) != 0)
    {
        return -1;
    }
    return 0;
}

static void handle_card_interrupt(struct WiFiBase *WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    // Handle a card interrupt

    // Get the card status
    if(WiFiBase->w_CardRCA)
    {
        cmd_int(SEND_STATUS, WiFiBase->w_CardRCA << 16, 500000, WiFiBase);
        if(FAIL(WiFiBase))
        {
        }
        else
        {
        }
    }
    else
    {
    }
}

static void handle_interrupts(struct WiFiBase *WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    ULONG irpts = rd32(WiFiBase->w_SDIO, EMMC_INTERRUPT);
    ULONG reset_mask = 0;

    if(irpts & SD_COMMAND_COMPLETE)
    {
        D(bug("[WiFi] spurious command complete interrupt\n"));
        reset_mask |= SD_COMMAND_COMPLETE;
    }

    if(irpts & SD_TRANSFER_COMPLETE)
    {
        D(bug("[WiFi] spurious transfer complete interrupt\n"));
        reset_mask |= SD_TRANSFER_COMPLETE;
    }

    if(irpts & SD_BLOCK_GAP_EVENT)
    {
        D(bug("[WiFi] spurious block gap event interrupt\n"));
        reset_mask |= SD_BLOCK_GAP_EVENT;
    }

    if(irpts & SD_DMA_INTERRUPT)
    {
        D(bug("[WiFi] spurious DMA interrupt\n"));
        reset_mask |= SD_DMA_INTERRUPT;
    }

    if(irpts & SD_BUFFER_WRITE_READY)
    {
        D(bug("[WiFi] spurious buffer write ready interrupt\n"));
        reset_mask |= SD_BUFFER_WRITE_READY;
        reset_dat(WiFiBase);
    }

    if(irpts & SD_BUFFER_READ_READY)
    {
        D(bug("[WiFi] spurious buffer read ready interrupt\n"));
        reset_mask |= SD_BUFFER_READ_READY;
        reset_dat(WiFiBase);
    }

    if(irpts & SD_CARD_INSERTION)
    {
        D(bug("[WiFi] card insertion detected\n"));
        reset_mask |= SD_CARD_INSERTION;
    }

    if(irpts & SD_CARD_REMOVAL)
    {
        D(bug("[WiFi] card removal detected\n"));
        reset_mask |= SD_CARD_REMOVAL;
        //SDCardBase->sd_CardRemoval = 1;
    }

    if(irpts & SD_CARD_INTERRUPT)
    {
        handle_card_interrupt(WiFiBase);
        reset_mask |= SD_CARD_INTERRUPT;
    }

    if(irpts & 0x8000)
    {
        reset_mask |= 0xffff0000;
    }

    wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, reset_mask);
}

void cmd(ULONG command, ULONG arg, ULONG timeout, struct WiFiBase *WiFiBase)
{
    // First, handle any pending interrupts
    handle_interrupts(WiFiBase);

    // Stop the command issue if it was the card remove interrupt that was
    //  handled
    if(0) //WiFIBase->w_CardRemoval)
    {
        WiFiBase->w_LastCMDSuccess = 0;
        return;
    }

    // Now run the appropriate commands by calling sd_issue_command_int()
    if(command & IS_APP_CMD)
    {
        command &= 0x7fffffff;

        WiFiBase->w_LastCMD = APP_CMD;

        ULONG rca = 0;
        if(WiFiBase->w_CardRCA)
            rca = WiFiBase->w_CardRCA << 16;

        cmd_int(APP_CMD, rca, timeout, WiFiBase);
        if(WiFiBase->w_LastCMDSuccess)
        {
            WiFiBase->w_LastCMD = command | IS_APP_CMD;
            cmd_int(command, arg, timeout, WiFiBase);
        }
    }
    else
    {
        WiFiBase->w_LastCMD = command;
        cmd_int(command, arg, timeout, WiFiBase);
    }
}

UBYTE sdio_read_byte(UBYTE function, ULONG address, struct WiFiBase *WiFiBase)
{
    cmd(IO_RW_DIRECT, ((address & 0x1ffff) << 9) | ((function & 7) << 28), 500000, WiFiBase);
    return WiFiBase->w_Res0;
}

void sdio_write_byte(UBYTE function, ULONG address, UBYTE value, struct WiFiBase *WiFiBase)
{
    cmd(IO_RW_DIRECT, value | 0x80000000 | ((address & 0x1ffff) << 9) | ((function & 7) << 28), 500000, WiFiBase);
}

void sdio_write_bytes(UBYTE function, ULONG address, void *data, ULONG length, struct WiFiBase *WiFiBase)
{
    WiFiBase->w_Buffer = data;
    WiFiBase->w_BlockSize = length;
    WiFiBase->w_BlocksToTransfer = 1;
    cmd(IO_RW_EXTENDED | SD_DATA_WRITE, 0x80000000 | ((address & 0x1ffff) << 9) | ((function & 7) << 28) | (length & 0x1ff) | (1 << 26), 500000, WiFiBase);
}

void sdio_read_bytes(UBYTE function, ULONG address, void *data, ULONG length, struct WiFiBase *WiFiBase)
{
    WiFiBase->w_Buffer = data;
    WiFiBase->w_BlockSize = length;
    WiFiBase->w_BlocksToTransfer = 1;
    cmd(IO_RW_EXTENDED | SD_DATA_READ, ((address & 0x1ffff) << 9) | ((function & 7) << 28) | (length & 0x1ff) | (1 << 26), 500000, WiFiBase);
}

UBYTE sdio_write_and_read_byte(UBYTE function, ULONG address, UBYTE value, struct WiFiBase *WiFiBase)
{
    cmd(IO_RW_DIRECT, value | 0x88000000 |  ((address & 0x1ffff) << 9) | ((function & 7) << 28), 500000, WiFiBase);
    return WiFiBase->w_Res0;
}

void sdio_backplane_window(ULONG addr, struct WiFiBase *WiFiBase)
{
    /* Align address properly */
    addr = addr & ~SBSDIO_SB_OFT_ADDR_MASK;

    if (addr != WiFiBase->w_LastBackplaneWindow) {
        WiFiBase->w_LastBackplaneWindow = addr;
        addr >>= 8;

        sdio_write_byte(SD_FUNC_BAK, SBSDIO_FUNC1_SBADDRLOW, addr, WiFiBase);
        sdio_write_byte(SD_FUNC_BAK, SBSDIO_FUNC1_SBADDRMID, addr >> 8, WiFiBase);
        sdio_write_byte(SD_FUNC_BAK, SBSDIO_FUNC1_SBADDRHIGH, addr >> 16, WiFiBase);
    }
}

ULONG sdio_backplane_addr(ULONG addr, struct WiFiBase *WiFiBase)
{
    sdio_backplane_window(addr, WiFiBase);
    return addr & SBSDIO_SB_OFT_ADDR_MASK;
}

void sdio_bak_write32(ULONG address, ULONG data, struct WiFiBase *WiFiBase)
{
    data = LE32(data);
    address = sdio_backplane_addr(address, WiFiBase);

    sdio_write_bytes(SD_FUNC_BAK, address | SBSDIO_SB_ACCESS_2_4B_FLAG, &data, 4, WiFiBase);
}

void sdio_bak_read32(ULONG address, ULONG *data, struct WiFiBase *WiFiBase)
{
    ULONG temp;
    address = sdio_backplane_addr(address, WiFiBase);
    sdio_read_bytes(SD_FUNC_BAK, address | SBSDIO_SB_ACCESS_2_4B_FLAG, &temp, 4, WiFiBase);
    *data = LE32(temp);
}

int sdio_buscoreprep(struct WiFiBase *WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    UBYTE clkval, clkset, tout;
    D(bug("[WiFi] sdio_buscoreprep\n"));

    /* Try forcing SDIO core to do ALPAvail request only */
    clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ;
    sdio_write_byte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, clkset, WiFiBase);

    /* If register supported, wait for ALPAvail and then force ALP */
	/* This may take up to 15 milliseconds */
	clkval = sdio_read_byte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, WiFiBase);
    if ((clkval & ~SBSDIO_AVBITS) != clkset) {
		D(bug("ChipClkCSR access: wrote 0x%02lx read 0x%02lx\n",
			  clkset, clkval));
		return 0;
	}

    tout = 15;
    do {
        clkval = sdio_read_byte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, WiFiBase);
        delay_us(1000, WiFiBase);
        if (--tout == 0)
        {
            
            break;
        }
    } while (!SBSDIO_ALPAV(clkval));

	if (!SBSDIO_ALPAV(clkval)) {
		D(bug("[WiFi] timed out while waiting for ALP ready\n"));
		return 0;
	}

    clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP;
	sdio_write_byte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, clkset, WiFiBase);
	delay_us(65, WiFiBase);

	/* Also, disable the extra SDIO pull-ups */
	sdio_write_byte(SD_FUNC_BAK, SBSDIO_FUNC1_SDIOPULLUP, 0, WiFiBase);

	return 1;
}

int sdio_init(struct WiFiBase *WiFiBase)
{
    ULONG tout;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    D(bug("[WiFi] SDIO init\n"));

    ULONG ver = rd32(WiFiBase->w_SDIO, EMMC_SLOTISR_VER);
    ULONG vendor = ver >> 24;
    ULONG sdversion = (ver >> 16) & 0xff;
    ULONG slot_status = ver & 0xff;
    
    D(bug("[WiFi]   vendor %lx, sdversion %lx, slot_status %lx\n", vendor, sdversion, slot_status));

    ULONG control1 = rd32(WiFiBase->w_SDIO, EMMC_CONTROL1);
    control1 |= (1 << 24);
    // Disable clock
    control1 &= ~(1 << 2);
    control1 &= ~(1 << 0);
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL1, control1);
    TIMEOUT_WAIT((rd32(WiFiBase->w_SDIO, EMMC_CONTROL1) & (0x7 << 24)) == 0, 1000000);
    if((rd32(WiFiBase->w_SDIO, EMMC_CONTROL1) & (7 << 24)) != 0)
    {
        D(bug("[WiFi]   controller did not reset properly\n"));
        return -1;
    }

    D(bug("[WiFi]   control0: %08lx, control1: %08lx, control2: %08lx\n", 
            rd32(WiFiBase->w_SDIO, EMMC_CONTROL0),
            rd32(WiFiBase->w_SDIO, EMMC_CONTROL1),
            rd32(WiFiBase->w_SDIO, EMMC_CONTROL2)));
#if 0
    // Read the capabilities registers - NOTE - these are not existing in case of Arasan SDHC from RasPi!!!
    SDCardBase->sd_Capabilities0 = rd32(SDCardBase->sd_SDHC, EMMC_CAPABILITIES_0);
    SDCardBase->sd_Capabilities1 = rd32(SDCardBase->sd_SDHC, EMMC_CAPABILITIES_1);

    {
        ULONG args[] = { SDCardBase->sd_Capabilities0, SDCardBase->sd_Capabilities1};
        RawDoFmt("[brcm-sdhc] Cap0: %08lx, Cap1: %08lx\n", args, (APTR)putch, NULL);
    }
#endif
    TIMEOUT_WAIT(rd32(WiFiBase->w_SDIO, EMMC_STATUS) & (1 << 16), 500000);
    ULONG status_reg = rd32(WiFiBase->w_SDIO, EMMC_STATUS);
    if((status_reg & (1 << 16)) == 0)
    {
        D(bug("[WiFi]   no SDIO connected?\n"));
        return -1;
    }

    D(bug("[WiFi]   status: %08lx\n", status_reg));

    // Clear control2
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL2, 0);

    control1 = rd32(WiFiBase->w_SDIO, EMMC_CONTROL1);
    control1 |= 1;      // enable clock

    // Set to identification frequency (400 kHz)
    uint32_t f_id = get_clock_divider(WiFiBase->w_SDIOClock, 400000);

    control1 |= f_id;

    control1 |= (7 << 16);		// data timeout = TMCLK * 2^10
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL1, control1);
    TIMEOUT_WAIT((rd32(WiFiBase->w_SDIO, EMMC_CONTROL1) & 0x2), 1000000);
    if((rd32(WiFiBase->w_SDIO, EMMC_CONTROL1) & 0x2) == 0)
    {
        D(bug("[WiFI]   controller's clock did not stabilise within 1 second\n"));
        return -1;
    }

    D(bug("[WiFi]   control0: %08lx, control1: %08lx\n",
        rd32(WiFiBase->w_SDIO, EMMC_CONTROL0),
        rd32(WiFiBase->w_SDIO, EMMC_CONTROL1)));

    // Enable the SD clock
    delay_us(2000, WiFiBase);
    control1 = rd32(WiFiBase->w_SDIO, EMMC_CONTROL1);
    control1 |= 4;
    wr32(WiFiBase->w_SDIO, EMMC_CONTROL1, control1);
    delay_us(2000, WiFiBase);

    // Mask off sending interrupts to the ARM
    wr32(WiFiBase->w_SDIO, EMMC_IRPT_EN, 0);
    // Reset interrupts
    wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, 0xffffffff);
    
    // Have all interrupts sent to the INTERRUPT register
    uint32_t irpt_mask = 0xffffffff & (~SD_CARD_INTERRUPT);
#ifdef SD_CARD_INTERRUPTS
    irpt_mask |= SD_CARD_INTERRUPT;
#endif
    wr32(WiFiBase->w_SDIO, EMMC_IRPT_MASK, irpt_mask);

    delay_us(2000, WiFiBase);

    D(bug("[WiFi] Clock enabled, control0: %08lx, control1: %08lx\n",
        rd32(WiFiBase->w_SDIO, EMMC_CONTROL0),
        rd32(WiFiBase->w_SDIO, EMMC_CONTROL1)));

    // Send CMD0 to the card (reset to idle state)
    cmd(GO_IDLE_STATE, 0, 500000, WiFiBase);
    if(FAIL(WiFiBase))
    {
        D(bug("[WiFi] SDIO: no CMD0 response\n"));
        return -1;
    }

    // Send CMD8 to the card
    // Voltage supplied = 0x1 = 2.7-3.6V (standard)
    // Check pattern = 10101010b (as per PLSS 4.3.13) = 0xAA

    cmd(SEND_IF_COND, 0x1aa, 500000, WiFiBase);

    int v2_later = 0;
    if(TIMEOUT(WiFiBase))
        v2_later = 0;
    else if(CMD_TIMEOUT(WiFiBase))
    {
        if(reset_cmd(WiFiBase) == -1)
            return -1;
        wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
        v2_later = 0;
    }
    else if(FAIL(WiFiBase))
    {
        D(bug("[WiFi] failure sending CMD8 (%08lx)\n", WiFiBase->w_LastInterrupt));
        return -1;
    }
    else
    {
        if(((WiFiBase->w_Res0) & 0xfff) != 0x1aa)
        {
            D(bug("[WiFi] unusable card\n"));
            D(bug("[WiFi] CMD8 response %08lx\n", WiFiBase->w_Res0));
            return -1;
        }
        else
            v2_later = 1;
    }

    // Here we are supposed to check the response to CMD5 (HCSS 3.6)
    // It only returns if the card is a SDIO card
    cmd(IO_SET_OP_COND, 0, 10000, WiFiBase);
    if(!TIMEOUT(WiFiBase))
    {
        if(CMD_TIMEOUT(WiFiBase))
        {
            if(reset_cmd(WiFiBase) == -1)
                return -1;
            wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
            D(bug("[WiFi] Not a SDIO card - aborting\n"));
            return -1;
        }
        else
        {
            D(bug("[WiFi] SDIO card detected - CMD5 response %08lx\n", WiFiBase->w_Res0));
        }
    }

    D(bug("[WiFi] Set host voltage to 3.3V\n"));
    cmd(IO_SET_OP_COND, 0x00200000, 10000, WiFiBase);
    if(!TIMEOUT(WiFiBase))
    {
        if(CMD_TIMEOUT(WiFiBase))
        {
            if(reset_cmd(WiFiBase) == -1)
                return -1;
            wr32(WiFiBase->w_SDIO, EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
        }
    }
    D(bug("[WiFi] CMD5 response %08lx\n", WiFiBase->w_Res0));

    /* The card is SDIO. Increase speed to standard 25MHz and obtain CID as well as RCA */#
    D(bug("[WiFi] Switching clock to 25MHz\n"));
    switch_clock_rate(WiFiBase->w_SDIOClock, SD_CLOCK_NORMAL, WiFiBase);

    delay_us(10000, WiFiBase);

    // Send CMD3 to enter the data state
    cmd(SEND_RELATIVE_ADDR, 0, 500000, WiFiBase);
    if(FAIL(WiFiBase))
    {
        D(bug("[WiFi] error sending SEND_RELATIVE_ADDR\n"));
        return -1;
    }

    WiFiBase->w_CardRCA = WiFiBase->w_Res0 >> 16;

    D(bug("[WiFi] SEND_RELATIVE_ADDR returns %08lx\n", WiFiBase->w_Res0));

    cmd(SELECT_CARD, WiFiBase->w_CardRCA << 16, 500000, WiFiBase);

    D(bug("[WiFi] Card selected, return value %08lx\n", WiFiBase->w_Res0));

    D(bug("[WiFi] Selecting 4bit mode\n"));
    UBYTE cccr7 = sdio_read_byte(SD_FUNC_CIA, BUS_BI_CTRL_REG, WiFiBase);
    if (SUCCESS(WiFiBase))
    {
        cccr7 |= 0x80;      // Disable card detect pullup
        cccr7 &= ~0x07;     // Clear width bits
        cccr7 |= 0x02;      // Select 4 bit interface
        sdio_write_byte(SD_FUNC_CIA, BUS_BI_CTRL_REG, cccr7, WiFiBase);

        /* Set 4bit width in CONTROL0 register */
        wr32(WiFiBase->w_SDIO, EMMC_CONTROL0, 2 | rd32(WiFiBase->w_SDIO, EMMC_CONTROL0));
    }
#if 0
    /* Check if card supports high speed mode */
    UBYTE cccr13 = sdio_read_byte(SD_FUNC_CIA, BUS_SPEED_CTRL_REG, WiFiBase);
    if (SUCCESS(WiFiBase))
    {
        if (cccr13 & 1) {
            D(bug("[WiFi] SDIO device reports 50MHz support, enabling it.\n"));
            cccr13 = (cccr13 & ~0xe) | 2;
            sdio_write_byte(SD_FUNC_CIA, BUS_SPEED_CTRL_REG, cccr13, WiFiBase);
            switch_clock_rate(WiFiBase->w_SDIOClock, SD_CLOCK_HIGH, WiFiBase);
            delay_us(10000, WiFiBase);
        }
    }
#endif
#if 0
    D(bug("[WiFi] Dumping CCCR contents:"));
    for (int i=0; i < 0x17; i++)
    {
        if ((i & 0xf) == 0)
            bug("\n         ");
        UBYTE v = sdio_read_byte(SD_FUNC_CIA, i, WiFiBase);
        D(bug("%02lx ", v));
    }
    bug("\n");
#endif

    /* Set blocksizes for function 1 and 2 to 64 and 512 bytes respectively */
    sdio_write_byte(SD_FUNC_CIA, SDIO_FBR_ADDR(1, 0x10), 0x40, WiFiBase);    // Function 1 - backplane
    sdio_write_byte(SD_FUNC_CIA, SDIO_FBR_ADDR(1, 0x11), 0x00, WiFiBase);

    sdio_write_byte(SD_FUNC_CIA, SDIO_FBR_ADDR(2, 0x10), 0x00, WiFiBase);    // Function 2 - radio
    sdio_write_byte(SD_FUNC_CIA, SDIO_FBR_ADDR(2, 0x11), 0x02, WiFiBase);

    /* Enable backplane function */
    D(bug("[WiFi] Enabling function 1 (backplane)\n"));
    sdio_write_byte(SD_FUNC_CIA, BUS_IOEN_REG, 1 << SD_FUNC_BAK, WiFiBase);
    do {
        D(bug("[WiFi] Waiting...\n"));
    } while(0 == (sdio_read_byte(SD_FUNC_CIA, BUS_IORDY_REG, WiFiBase) & (1 << SD_FUNC_BAK)));
    D(bug("[WiFi] Backplane is up\n"));

    union
    {
        UBYTE id[4];
        ULONG id32;
    } u;

    /* Force PLL off until brcmf_chip_attach() */
    sdio_write_byte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ, WiFiBase);
    UBYTE tmp = sdio_read_byte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, WiFiBase);
    if (((tmp & ~SBSDIO_AVBITS) != (SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ))) {
        D(bug("[WiFi] Chip CLK CSR access error, wrote 0x%02lx, read 0x%02lx\n", SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ, tmp));
    }

    sdio_buscoreprep(WiFiBase);
    sdio_read_bytes(SD_FUNC_BAK, CORE_CC_REG(SI_ENUM_BASE_DEFAULT, chipid), u.id, 4, WiFiBase);
    u.id32 = LE32(u.id32);

    WiFiBase->w_ChipID = u.id32 & CID_ID_MASK;
    WiFiBase->w_ChipREV = (u.id32 & CID_REV_MASK) >> CID_REV_SHIFT;

    D(bug("[WiFi] Chip ID: %04lx rev %ld\n", WiFiBase->w_ChipID, WiFiBase->w_ChipREV));

    LoadFirmware(WiFiBase, WiFiBase->w_ChipID, WiFiBase->w_ChipREV);

    ULONG soci_type = (u.id32 & CID_TYPE_MASK) >> CID_TYPE_SHIFT;

    D(bug("[WiFi] SOCI type: %s\n", (ULONG)(soci_type == SOCI_SB ? "SB" : "AI")));

    if (soci_type == SOCI_AI)
    {
        
    }

    /* Magic setup after ZeroWi project */
    D(bug("[WiFi] ZeroWi magic...\n"));


    // [18.004850] Disable pullups
    sdio_write_byte(SD_FUNC_BAK, BAK_PULLUP_REG, 0, WiFiBase);
    // Get chip ID again, and config base addr [18.005201]
    sdio_read_bytes(SD_FUNC_BAK, SB_32BIT_WIN, u.id, 4, WiFiBase);
    ULONG config_base_addr;
    sdio_read_bytes(SD_FUNC_BAK, SB_32BIT_WIN+0xfc, &config_base_addr, 4, WiFiBase);

    D(bug("[WiFi] Chip ID (again): %02lx-%02lx-%02lx-%02lx\n", u.id[0], u.id[1], u.id[2], u.id[3]));
    D(bug("[WiFi] Config base address: %08lx\n", LE32(config_base_addr)));

    // Reset cores [18.030305]
    sdio_bak_write32(ARM_IOCTRL_REG, 0x03, WiFiBase);
    sdio_bak_write32(MAC_IOCTRL_REG, 0x07, WiFiBase);
    sdio_bak_write32(MAC_RESETCTRL_REG, 0x00, WiFiBase);
    sdio_bak_write32(MAC_IOCTRL_REG, 0x05, WiFiBase);
    // [18.032572]
    sdio_bak_write32(SRAM_IOCTRL_REG, 0x03, WiFiBase);
    sdio_bak_write32(SRAM_RESETCTRL_REG, 0x00, WiFiBase);
    sdio_bak_write32(SRAM_IOCTRL_REG, 0x01, WiFiBase);

    ULONG tmp32 = 0xdeadbeef;
    sdio_bak_read32(SRAM_IOCTRL_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SRAM IOCTRL REG=%08x\n", tmp32));

    // [18.034039]
    sdio_bak_write32(SRAM_BANKX_IDX_REG, 0x03, WiFiBase);
    sdio_bak_write32(SRAM_BANKX_PDA_REG, 0x00, WiFiBase);

    sdio_bak_read32(SRAM_IOCTRL_REG, &tmp32, WiFiBase);
    if (tmp32 != 1)
        D(bug("[WiFi] SRAM IOCTRL REG=%08x but should be 0x00000001\n", tmp32));
    else
        D(bug("[WiFi] SRAM IOCTRL REG=%08x\n", tmp32));

    sdio_bak_read32(SRAM_RESETCTRL_REG, &tmp32, WiFiBase);
    if (tmp32 != 0)
        D(bug("[WiFi] SRAM RESETCTRL REG=%08x but should be 0x00000000\n", tmp32));
    else
        D(bug("[WiFi] SRAM RESETCTRL REG=%08x\n", tmp32));

    // [18.035416]
    sdio_bak_read32(SRAM_BASE_ADDR, &tmp32, WiFiBase);
    D(bug("[WiFi] SRAM BASE ADDR=%08x\n", tmp32));
    sdio_bak_write32(SRAM_BANKX_IDX_REG, 0, WiFiBase);
    sdio_bak_read32(SRAM_UNKNOWN_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SRAM UNKNOWN REG=%08x for IDX 0\n", tmp32));
    sdio_bak_write32(SRAM_BANKX_IDX_REG, 1, WiFiBase);
    sdio_bak_read32(SRAM_UNKNOWN_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SRAM UNKNOWN REG=%08x for IDX 1\n", tmp32));
    sdio_bak_write32(SRAM_BANKX_IDX_REG, 2, WiFiBase);
    sdio_bak_read32(SRAM_UNKNOWN_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SRAM UNKNOWN REG=%08x for IDX 2\n", tmp32));
    sdio_bak_write32(SRAM_BANKX_IDX_REG, 3, WiFiBase);
    
    // [18.037502]
    if ((sdio_read_byte(SD_FUNC_CIA, 0x00f1, WiFiBase) & 0xff) != 1)
    {
        D(bug("[WiFi] Expected 0x00f1 reg of CIA to be 1\n"));
    }
    sdio_write_byte(SD_FUNC_CIA, 0x00f1, 3, WiFiBase);

    UBYTE tmp8_4[4];

    sdio_read_bytes(SD_FUNC_BAK, 0x8600, &tmp8_4, 4, WiFiBase);  // !!sdio_cmd53_read(SD_FUNC_BAK, 0x8600, u32d.bytes, 4);
    tmp8_4[1] |= 0x40;                                           // !!u32d.bytes[1] |= 0x40;
    sdio_write_bytes(SD_FUNC_BAK, 0x8600, &tmp8_4, 4, WiFiBase); // !!sdio_cmd53_write(SD_FUNC_BAK, 0x8600, u32d.bytes, 4);

    // [18.052762]
    sdio_write_byte(SD_FUNC_CIA, BUS_IOEN_REG, 1 << SD_FUNC_BAK, WiFiBase);
    sdio_write_byte(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, 0, WiFiBase);
    delay_us(45000, WiFiBase);
    sdio_write_byte(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, 8, WiFiBase);

    if ((sdio_read_byte(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, WiFiBase) & 0xff) != 0x48)
    {
        D(bug("[WiFi] Expected BAK_CHIP_CLOCK_CSR_REG reg of BAK to be 0x48\n"));
    }

    /* Now it's time to upload the firmware */
    D(bug("[WiFi] Loading firmware...\n"));
    ULONG remaining = (ULONG)WiFiBase->w_FirmwareSize;
    UBYTE *sdio_bin = WiFiBase->w_FirmwareBase;
    for (ULONG addr = 0; addr < (ULONG)WiFiBase->w_FirmwareSize; )
    {
        ULONG sz = remaining > 64 ? 64 : remaining;

        ULONG a = sdio_backplane_addr(addr, WiFiBase);

        sdio_write_bytes(SD_FUNC_BAK, SB_32BIT_WIN + a, &sdio_bin[addr], sz, WiFiBase);

        addr += sz;
        remaining -= sz;
    }

    sdio_backplane_addr(0x58000, WiFiBase);
    sdio_read_bytes(SD_FUNC_BAK, 0xee80, &tmp32, 4, WiFiBase);
    
    D(bug("[WiFi] Read from 0xee80 backplane 0x58000 returned %08lx\n", tmp32));

    D(bug("[WiFi] Loading nvram...\n"));
    ULONG a = sdio_backplane_addr(0x078000, WiFiBase);
    const UBYTE *config_data = (const UBYTE *)WiFiBase->w_ConfigBase;
    const ULONG config_len = WiFiBase->w_ConfigSize;

    remaining = config_len;
    for (ULONG addr = 0; addr < config_len; )
    {
        ULONG sz = remaining > 64 ? 64 : remaining;

        sdio_write_bytes(SD_FUNC_BAK, 0xfd54 + a, (void*)&config_data[addr], sz, WiFiBase);

        addr += sz;
        remaining -= sz;
    }

    UBYTE tmp8[64];
    sdio_read_bytes(SD_FUNC_BAK, 0xffd4, tmp8, 44, WiFiBase);

    D(bug("[WiFi] 44-byte read after config upload:"));
    for (int i=0; i < 44; i++)
    {
        if (i % 32 == 0)
        {
            D(bug("\n[WiFi]   "));
        }
        D(bug(" %02lx", tmp8[i]));
    }
    D(bug("\n"));

    sdio_bak_read32(SRAM_IOCTRL_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SRAM_IOCTRL_REG=%08lx\n", tmp32));
    sdio_bak_read32(SRAM_RESETCTRL_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SRAM_RESETCTRL_REG=%08lx\n", tmp32));
    sdio_bak_write32(SB_INT_STATUS_REG, 0xffffffff, WiFiBase);

    sdio_bak_write32(ARM_IOCTRL_REG, 0x03, WiFiBase);
    sdio_bak_write32(ARM_RESETCTRL_REG, 0x00, WiFiBase);
    sdio_bak_write32(ARM_IOCTRL_REG, 0x01, WiFiBase);

    sdio_bak_read32(ARM_IOCTRL_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] ARM_IOCTRL_REG=%08lx\n", tmp32));

    sdio_backplane_window(BAK_BASE_ADDR, WiFiBase);
    sdio_write_byte(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, 0, WiFiBase);
    delay_us(100000, WiFiBase);
    sdio_write_byte(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, 0x10, WiFiBase);
    delay_us(100000, WiFiBase);

    tmp = sdio_read_byte(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, WiFiBase);
    if (tmp != 0xd0)
    {
        D(bug("[WiFi] Got BAK_CHIP_CLOCK_CSR_REG of %02lx but expected 0xd0\n", tmp));
    }

    sdio_write_byte(SD_FUNC_BAK, BAK_CHIP_CLOCK_CSR_REG, 0xd2, WiFiBase);
    sdio_bak_write32(SB_TO_SB_MBOX_DATA_REG, 0x40000, WiFiBase);

    D(bug("[WiFi] Enabling function 2 (radio)\n"));
    sdio_write_byte(SD_FUNC_CIA, BUS_IOEN_REG, (1 << SD_FUNC_BAK) | (1 << SD_FUNC_RAD), WiFiBase);
    int timeout=20;
    do {
        D(bug("[WiFi] Waiting...\n"));
        delay_us(300000, WiFiBase);
        if (timeout-- == 0)
            break;
    } while(0 == (sdio_read_byte(SD_FUNC_CIA, BUS_IORDY_REG, WiFiBase) & (1 << SD_FUNC_RAD)));
    D(bug("[WiFi] Radio function is up\n"));

    sdio_bak_write32(SB_INT_HOST_MASK_REG, 0x200000f0, WiFiBase);
    sdio_bak_read32(SR_CONTROL1, &tmp32, WiFiBase);
    D(bug("[WiFi] SR_CONTROL1 = %08lx\n", tmp32));

    sdio_backplane_window(BAK_BASE_ADDR, WiFiBase);

    sdio_write_byte(SD_FUNC_BAK, BAK_WAKEUP_REG, 2, WiFiBase);
    sdio_write_byte(SD_FUNC_BUS, BUS_BRCM_CARDCAP, 6, WiFiBase);
    sdio_write_byte(SD_FUNC_BUS, BUS_INTEN_REG, 0x07, WiFiBase);
    tmp = sdio_read_byte(SD_FUNC_BUS, BUS_INTPEND_REG, WiFiBase);
    D(bug("[WiFi] BUS_INTPEND_REG = %02lx\n", tmp));

    sdio_bak_read32(SB_INT_STATUS_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SB_INT_STATUS_REG = %08lx\n", tmp32));
    sdio_bak_write32(SB_INT_STATUS_REG, 0x200000c0, WiFiBase);

    sdio_bak_read32(SB_TO_HOST_MBOX_DATA_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SB_TO_HOST_MBOX_DATA_REG = %08lx\n", tmp32));

    sdio_bak_write32(SB_TO_SB_MBOX_REG, 0x02, WiFiBase);
    sdio_bak_read32(SR_CONTROL1, &tmp32, WiFiBase);
    D(bug("[WiFi] SR_CONTROL1 = %08lx\n", tmp32));

    sdio_bak_read32(0x68000 | 0x7ffc, &tmp32, WiFiBase);
    D(bug("[WiFi] Value at 0x6fffc = %08lx\n", tmp32));
    sdio_backplane_window(0x38000, WiFiBase);

    sdio_read_bytes(SD_FUNC_BAK, 0x70d4, tmp8, 64, WiFiBase);

    D(bug("[WiFi] 64-byte read after init:"));
    for (int i=0; i < 44; i++)
    {
        if (i % 32 == 0)
        {
            D(bug("\n[WiFi]   "));
        }
        D(bug(" %02lx", tmp8[i]));
    }
    D(bug("\n"));

    sdio_bak_read32(SB_INT_STATUS_REG, &tmp32, WiFiBase);
    D(bug("[WiFi] SB_INT_STATUS_REG = %08lx\n", tmp32));

    sdio_bak_write32(SB_INT_STATUS_REG, 0x80, WiFiBase);
    sdio_read_bytes(SD_FUNC_RAD, SB_32BIT_WIN, tmp8, 64, WiFiBase);

    D(bug("[WiFi] 64-byte read from RAD function:"));
    for (int i=0; i < 44; i++)
    {
        if (i % 32 == 0)
        {
            D(bug("\n[WiFi]   "));
        }
        D(bug(" %02lx", tmp8[i]));
    }
    D(bug("\n"));

    return 0;
}
