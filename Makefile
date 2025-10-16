PROJECT_NAME = blink
PROJECT_DIR = .
PROJECT_BUILD_DIR = $(PROJECT_DIR)/build
PROJECT_SRC_DIR = $(PROJECT_DIR)/src
##############################################################################
# Include required HAL libs below                                            #
##############################################################################
SRCS += $(wildcard $(PROJECT_SRC_DIR)/*.c)
SRCS += $(wildcard $(PROJECT_SRC_DIR)/*.cpp)
SRCS += $(RUNTIME)
SRCS += $(SHARED_DIR)/libs/uart_lib.c
#SRCS += $(SHARED_DIR)/libs/xprintf.c
SRCS += $(HAL_DIR)/peripherals/Source/mik32_hal_gpio.c
#SRCS += $(HAL_DIR)/peripherals/Source/mik32_hal_pcc.c
SRCS += $(HAL_DIR)/peripherals/Source/mik32_hal_adc.c
	
LIBS += -lc 

##############################################################################
#
TOOLCHAIN_DIR = /opt/mik32
CROSS ?= /opt/xpack-riscv-none-elf-gcc-14.2.0-2/bin/riscv-none-elf-
MIK32_UPLOADER_DIR = /opt/mik32/mik32-uploader
CC = $(CROSS)gcc
LD = $(CROSS)ld
STRIP = $(CROSS)strip
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump

MARCH = rv32imc_zicsr
MABI = ilp32

#
SHARED_DIR = $(TOOLCHAIN_DIR)/mik32v2-shared
HAL_DIR = $(TOOLCHAIN_DIR)/mik32-hal
#LDSCRIPT = $(SHARED_DIR)/ldscripts/eeprom.ld
#LDSCRIPT = $(SHARED_DIR)/ldscripts/spifi.ld
LDSCRIPT = ./src/eeprom_spifi.ld
RUNTIME = $(SHARED_DIR)/runtime/crt0.S

INC += -I $(SHARED_DIR)/include
INC += -I $(SHARED_DIR)/periphery
INC += -I $(SHARED_DIR)/runtime
INC += -I $(SHARED_DIR)/libs
INC += -I $(HAL_DIR)/core/Include
INC += -I $(HAL_DIR)/peripherals/Include
INC += -I $(HAL_DIR)/utilities/Include


OBJDIR = $(PROJECT_BUILD_DIR)

CFLAGS +=  -Os -MD -fstrict-volatile-bitfields -fno-strict-aliasing -march=$(MARCH) -mabi=$(MABI) -fno-common -fno-builtin-printf -DBUILD_NUMBER=$(BUILD_NUMBER)+1  -fno-common -flto 

LDFLAGS +=  -nostdlib -lgcc -mcmodel=medlow -nostartfiles -ffreestanding -Wl,-Bstatic,-T,$(LDSCRIPT),-Map,$(OBJDIR)/$(PROJECT_NAME).map,--print-memory-usage -march=$(MARCH) -mabi=$(MABI)


OBJS := $(SRCS)
OBJS := $(OBJS:.c=.o)
OBJS := $(OBJS:.cpp=.o)
OBJS := $(OBJS:.S=.o)
OBJS := $(OBJS:..=miaou)
OBJS := $(addprefix $(OBJDIR)/,$(OBJS))

#all: $(OBJDIR)/$(PROJECT_NAME).elf $(OBJDIR)/$(PROJECT_NAME).hex $(OBJDIR)/$(PROJECT_NAME).bin $(OBJDIR)/$(PROJECT_NAME).asm
all: $(OBJDIR)/$(PROJECT_NAME).elf $(OBJDIR)/$(PROJECT_NAME).hex $(OBJDIR)/$(PROJECT_NAME).asm

inc_build_num:
	@expr $(BUILD_NUMBER) + 1 > build_number

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.elf: $(OBJS) | $(OBJDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBSINC) $(LIBS)

%.hex: %.elf
	$(OBJCOPY) -O ihex $^ $@

#%.bin: %.elf
#	$(OBJCOPY) -O binary $^ $@

%.asm: %.elf
	$(OBJDUMP) -S -d $^ > $@

$(OBJDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS)  $(INC) -o $@ $^
	$(CC) -S $(CFLAGS)  $(INC) -o $@.disasm $^

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS)  $(INC) -o $@ $^

$(OBJDIR)/%.o: %.S
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $^ -D__ASSEMBLY__=1


clean:
	rm -rf $(OBJDIR)



upload: $(OBJDIR)/$(PROJECT_NAME).hex
	python $(MIK32_UPLOADER_DIR)/mik32_upload.py --run-openocd --openocd-exec=`which openocd` --openocd-scripts $(MIK32_UPLOADER_DIR)/openocd-scripts --openocd-interface interface/ftdi/mikron-link.cfg $(OBJDIR)/$(PROJECT_NAME).hex 


term:
	sudo minicom -D /dev/ttyU*0 -b 115200

