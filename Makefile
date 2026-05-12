CC = gcc
AS = nasm
LD = ld

CFLAGS = -m64 -ffreestanding -fno-builtin -nostdlib -mno-red-zone -mcmodel=large -Iinclude -mavx -fPIC
ASFLAGS = -f elf64
LDFLAGS = -n -T linker.ld
OBJS = src/boot.o src/interrupts.o src/syscalls.o src/idt.o src/gdt.o src/memory.o src/task.o src/keyboard.o src/syscall.o src/mouse.o src/font.o src/graphics.o src/lib.o src/tar.o src/elf.o src/kernel.o src/paging.o src/pci.o src/e1000.o src/net.o src/ide.o src/exfat.o src/ac97.o

all: build/meuos.iso build/disk.img

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

apps/browser.o: apps/browser.c
	mkdir -p apps
	$(CC) $(CFLAGS) -c $< -o $@

apps/explorer.o: apps/explorer.c
	mkdir -p apps
	$(CC) $(CFLAGS) -c $< -o $@

apps/player.o: apps/player.c
	mkdir -p apps
	$(CC) $(CFLAGS) -c $< -o $@

apps/gui.elf: apps/entry.o apps/gui.o lib/protos.o src/html.o src/css.o src/layout.o src/render.o apps/app.ld
	$(LD) -n -T apps/app.ld -o $@ apps/entry.o apps/gui.o lib/protos.o src/html.o src/css.o src/layout.o src/render.o

apps/calc.elf: apps/entry.o apps/calc.o lib/protos.o apps/calc.ld
	$(LD) -n -T apps/calc.ld -o $@ apps/entry.o apps/calc.o lib/protos.o

apps/snake.elf: apps/entry.o apps/snake.o lib/protos.o apps/snake.ld
	$(LD) -n -T apps/snake.ld -o $@ apps/entry.o apps/snake.o lib/protos.o

apps/browser.elf: apps/entry.o apps/browser.o lib/protos.o src/html.o src/css.o src/layout.o src/render.o apps/browser.ld
	$(LD) -n -T apps/browser.ld -o $@ apps/entry.o apps/browser.o lib/protos.o src/html.o src/css.o src/layout.o src/render.o

apps/explorer.elf: apps/entry.o apps/explorer.o lib/protos.o apps/explorer.ld
	$(LD) -n -T apps/explorer.ld -o $@ apps/entry.o apps/explorer.o lib/protos.o

apps/player.elf: apps/entry.o apps/player.o lib/protos.o apps/player.ld
	$(LD) -n -T apps/player.ld -o $@ apps/entry.o apps/player.o lib/protos.o

initrd.tar: apps/gui.elf apps/calc.elf apps/snake.elf apps/browser.elf apps/explorer.elf apps/player.elf
	tar -cvf $@ -C apps gui.elf calc.elf snake.elf browser.elf explorer.elf player.elf

build/meuos.iso: meuos.bin initrd.tar
	mkdir -p isodir/boot/grub
	mkdir -p build
	cp meuos.bin isodir/boot/
	cp initrd.tar isodir/boot/
	cp grub.cfg isodir/boot/grub/
	grub-mkrescue -o $@ isodir

build/disk.img:
	mkdir -p build
	dd if=/dev/zero of=$@ bs=1M count=256
	mkfs.exfat -n PROTOS_HD $@
	mkdir -p mnt
	MEULOOP=$$(sudo losetup -f --show $@); \
	sudo mount -t exfat $$MEULOOP mnt/; \
	sudo mkdir -p mnt/Sistema mnt/Jogos mnt/Documentos; \
	sudo bash -c 'echo "PROTOS CONFIG" > mnt/Sistema/config.txt'; \
	sudo bash -c 'echo "Bem vindo ao ProtOS" > mnt/Sistema/leia-me.txt'; \
	sudo bash -c 'echo "Este e o explorador de ficheiros" > mnt/Documentos/notas.txt'; \
	sudo bash -c 'echo "Snake Game" > mnt/Jogos/pontuacoes.txt'; \
	sudo cp musica.wav mnt/musica.wav; \
	sudo umount mnt/; \
	sudo losetup -d $$MEULOOP
	
run: build/meuos.iso build/disk.img
	qemu-system-x86_64 -cpu max -boot d -cdrom build/meuos.iso -hda build/disk.img -m 2G -vga std -device e1000 -net nic -net user -device AC97 -no-reboot -no-shutdown

build: build/meuos.iso build/disk.img
	@echo "Build concluido."

clean:
	-sudo umount mnt/ 2>/dev/null || true
	-sudo losetup -D 2>/dev/null || true
	sudo rm -rf src/*.o meuos.bin isodir build apps/*.o apps/*.elf initrd.tar lib/*.o mnt