CC = gcc
AS = nasm
LD = ld

CFLAGS = -m64 -ffreestanding -fno-builtin -nostdlib -mno-red-zone -mcmodel=large -Iinclude -mavx -fPIC
ASFLAGS = -f elf64
LDFLAGS = -n -T linker.ld
OBJS = src/boot.o src/interrupts.o src/syscalls.o src/idt.o src/gdt.o src/memory.o src/task.o src/keyboard.o src/syscall.o src/mouse.o src/font.o src/graphics.o src/lib.o src/tar.o src/elf.o src/kernel.o src/paging.o src/pci.o src/e1000.o src/net.o src/ide.o src/exfat.o

all: meuos.iso disk.img

src/%.o: src/%.asm
	$(AS) $(ASFLAGS) $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

meuos.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

lib/protos.o: lib/protos.c
	mkdir -p lib
	$(CC) $(CFLAGS) -c $< -o $@

apps/entry.o: apps/entry.asm
	mkdir -p apps
	$(AS) $(ASFLAGS) $< -o $@

apps/gui.o: apps/gui.c
	mkdir -p apps
	$(CC) $(CFLAGS) -c $< -o $@

apps/calc.o: apps/calc.c
	mkdir -p apps
	$(CC) $(CFLAGS) -c $< -o $@

apps/snake.o: apps/snake.c
	mkdir -p apps
	$(CC) $(CFLAGS) -c $< -o $@

apps/gui.elf: apps/entry.o apps/gui.o lib/protos.o src/html.o src/css.o src/layout.o src/render.o apps/app.ld
	$(LD) -n -T apps/app.ld -o $@ apps/entry.o apps/gui.o lib/protos.o src/html.o src/css.o src/layout.o src/render.o

apps/calc.elf: apps/entry.o apps/calc.o lib/protos.o apps/calc.ld
	$(LD) -n -T apps/calc.ld -o $@ apps/entry.o apps/calc.o lib/protos.o

apps/snake.elf: apps/entry.o apps/snake.o lib/protos.o apps/snake.ld
	$(LD) -n -T apps/snake.ld -o $@ apps/entry.o apps/snake.o lib/protos.o

initrd.tar: apps/gui.elf apps/calc.elf apps/snake.elf
	tar -cvf $@ -C apps gui.elf calc.elf snake.elf

meuos.iso: meuos.bin initrd.tar
	mkdir -p isodir/boot/grub
	cp meuos.bin isodir/boot/
	cp initrd.tar isodir/boot/
	cp grub.cfg isodir/boot/grub/
	grub-mkrescue -o $@ isodir

disk.img:
	dd if=/dev/zero of=$@ bs=1M count=64
	mkfs.exfat -n PROTOS_HD $@
	mkdir -p mnt
	MEULOOP=$$(sudo losetup -f --show $@); \
	sudo mount -t exfat $$MEULOOP mnt/; \
	sudo mkdir -p mnt/Sistema mnt/Jogos; \
	sudo bash -c 'echo "PROTOS CONFIG" > mnt/Sistema/config.txt'; \
	sudo umount mnt/; \
	sudo losetup -d $$MEULOOP

run: meuos.iso disk.img
	qemu-system-x86_64 -cpu max -boot d -cdrom meuos.iso -hda disk.img -m 512M -vga std -device e1000 -net nic -net user -no-reboot -no-shutdown

build: meuos.iso disk.img
	@echo "Build concluido."

clean:
	-sudo umount mnt/ 2>/dev/null || true
	-sudo losetup -D 2>/dev/null || true
	sudo rm -rf src/*.o meuos.bin isodir meuos.iso apps/*.o apps/*.elf initrd.tar lib/*.o disk.img mnt