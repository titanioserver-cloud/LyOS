# LyOs

LyOs é um sistema operativo de 64 bits (x86_64) desenvolvido a partir do zero. O projeto evoluiu de um kernel monolítico básico para um ecossistema gráfico interativo, focado em performance pura via CPU e abstração de software de alto nível.

Este projeto demonstra a implementação prática de conceitos avançados de engenharia de sistemas, desde a gestão de memória física até a um toolkit de interface gráfica pseudo-orientado a objetos em C puro.

## 1. Arquitetura do Kernel e Hardware

O núcleo do LyOs interage de forma direta com o hardware virtualizado, sem dependências externas. O arranque é compatível com Multiboot2.

* **Gestão de Memória (Heap e Paginação)**:
  * **Paging (x86_64)**: Implementação de paginação de 4 níveis (PML4, PDPT, PD, PT) com isolamento de espaços de endereçamento (`create_address_space`).
  * **Alocador Dinâmico (`memory.c`)**: O sistema não usa a `libc`. Foi desenvolvido um alocador customizado (`kmalloc`/`kfree`) baseado em listas ligadas. O alocador garante alinhamento de 16 bytes por bloco, funde blocos livres automaticamente para evitar fragmentação e utiliza um `BLOCK_MAGIC` (0x12345678) para deteção de corrupção de memória.
* **Multitasking Preemptivo**: O kernel realiza troca de contexto (context switch) guardando o estado dos registos de cada tarefa de forma isolada, gerido através das interrupções do Timer (APIC/PIT).
* **Sistema de Ficheiros duplo**: O sistema suporta um ramdisk `TAR` (Initrd) carregado via módulos Multiboot2 no arranque (usado para binários ELF iniciais como a GUI), e possui um driver IDE completo com suporte a tabelas de partição e leitura/escrita em discos `exFAT`.
  * **Suporte Avançado exFAT**: Operações expansivas permitindo leitura, criação (`N`), eliminação de ficheiros e pastas (`M`) nativamente e em tempo real na raiz do disco.

## 2. Motor Gráfico e Otimização (`graphics.c`)

O LyOs não depende de aceleração de GPU. Toda a renderização 2D (Software Rendering) é calculada na CPU, com otimizações de nível de assembler.

* **Double Buffering e Dirty Rectangles**: O ecrã (1280x720) não é desenhado a cada frame. Qualquer alteração (movimento do rato, novas janelas) regista um retângulo no array `dirty_rects` (limite de 300 retângulos simultâneos). O algoritmo funde retângulos sobrepostos e copia apenas os pixéis estritamente necessários para o ecrã físico no próximo V-Sync.
* **Aceleração SIMD (AVX/SSE)**: Para garantir fluidez sem GPU, as operações pesadas de cópia de buffers ou preenchimento de cor sólida utilizam instruções vetoriais avançadas (`vmovups`, manipulando registos de 256 bits `ymm0` e `xmm0`) dentro de blocos de `__asm__ volatile`.
* **Alpha Blending Matemático**: O motor suporta mistura de cores RGBA em tempo real, calculando os desvios de bits diretamente na matriz do buffer para permitir janelas com transparência e bordas suavizadas.
* **Shared Buffers**: Suporte nativo para memória de vídeo partilhada. Aplicações pesadas podem pedir buffers isolados ao kernel e renderizar separadamente, cabendo à GUI fundir as imagens finais no ecrã.

## 3. Toolkit de User Space (`protos.c` / `protos.h`)

Para facilitar o desenvolvimento de aplicações em Ring 3 / User Space, o LyOs fornece uma biblioteca (`libprotos`) que atua como um Framework de UI.

* **Interface de Syscalls**: A comunicação com o kernel é feita de forma segura através da interrupção de software `int $0x80`, suportando até 6 parâmetros via registos (`D`, `S`, `d`, `c`, `r8`, `r9`). As syscalls gerem processos, alocação, hardware e eventos de rato/teclado.
* **Sistema Pseudo-OOP (Orientação a Objetos)**: O C puro foi estruturado para agir como um sistema moderno.
  * Estruturas como `UIButton` e `UIWindow` encapsulam coordenadas, estado e apontadores de função (`.Draw`, `.IsClicked`, `.CloseClicked`).
  * Paleta de cores do sistema centralizada na struct estática `ColorPalette Color`.
  * Isto permite instanciar elementos visuais complexos com apenas uma linha de código nas aplicações finais (ex: `Button_Create`).

## 4. Stack de Rede e Comunicação (`pci.c` / Network)

* **PCI Bus Mastering**: O sistema enumera o bus PCI na inicialização, lê os espaços de configuração (Header Type 0) e ativa Bus Mastering de forma nativa para garantir acesso DMA (Direct Memory Access) das placas.
* **Drivers de Rede**: Inicialização direta do hardware Intel E1000 Gigabit.
* **Protocolos e Internet**: Implementação artesanal de construção e leitura de pacotes. O kernel suporta pacotes ARP, ping, pedidos DHCP para configuração automática de IP, resolução de domínios (DNS) e um stack TCP funcional para sessões de navegação.

## 5. Subsistema de Áudio e Multimédia (`ac97.c`)

* **Driver Intel AC'97**: Inicialização e leitura de hardware de som com suporte a transferências DMA usando *Buffer Descriptor Lists* (BDL). Isto permite que as amostras sonoras sejam processadas contínua e diretamente da memória sem travar a CPU.
* **Reprodução WAV**: Capacidade de aceder diretamente ao sistema de ficheiros exFAT para ler ficheiros `.wav` (PCM), ignorando os metadados dinamicamente para reproduzir áudio com fidelidade.
* **Em Desenvolvimento**: O sistema de áudio está atualmente em fase de melhorias contínuas para alargar o suporte a buffers circulares de maior dimensão e diferentes taxas de amostragem.

## 6. Ecossistema de Aplicações (GUI)

O executável principal (`gui.elf`) gere o ambiente de desktop, o sistema de janelas flutuantes e lança binários secundários via system calls (`exec`).

* **Launcher e Sidebar**: Gestão de instâncias de janelas e barra de tarefas interativa.
* **PROTOS Web Browser**: Aplicação complexa que utiliza um socket TCP nativo para ligar a um proxy externo (`10.0.2.2:8080`). Processa respostas HTTP brutas e transforma as tags de texto (`<h1>`, `<button>`, `<a>`, `<input>`) numa árvore visual desenhada pelo motor gráfico do OS em tempo real.
* **Explorador de Ficheiros (`explorer.elf`)**: App atualizada com suporte nativo aos *Shared Buffers*. Lê os clusters do disco rígido dinamicamente e fornece feedback da interface numa janela independente.
* **Media Player (`player.elf`)**: Interface de utilizador desenhada para acionar e comunicar com o driver de som do kernel (AC'97), reproduzindo faixas da raiz do sistema.
* **System Monitor**: Lê em tempo real a alocação do heap (`get_used_memory`), o disco montado (`get_disk_label`) e as instâncias ativas do scheduler.
* **Terminal e Calculadora**: Demonstrações nativas de execução de subprocessos, IPC (Inter-Process Communication) enviando mensagens entre PIDs, e percorrimento do disco FAT.

## Como Compilar e Executar

**Pré-requisitos**:
* `GCC` (Cross-compiler para `x86_64-elf`)
* `QEMU` (Requer suporte para e1000, dispositivo AC97 e placa gráfica pci)
* Módulos de python (`python3 proxy.py` opcional, para navegação web)

**Passos para Build**:
```bash
# Compilar o Kernel, a biblioteca protos e os binários em User Space
make all

# (Opcional) Iniciar o proxy num terminal secundário para habilitar o web browser
python3 proxy.py

# Iniciar o QEMU com os binários montados e o hardware extra
make run