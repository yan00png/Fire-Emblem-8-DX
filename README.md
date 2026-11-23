# Fire Emblem 8 DX (PlayStation 2)

Port **nativo** (sem emulação de CPU/PPU) de *Fire Emblem: The Sacred Stones* para PlayStation 2. Usa a decompilação
[FireEmblemUniverse/fireemblem8u](https://github.com/FireEmblemUniverse/fireemblem8u) compilada para o EE (MIPS R5900) e uma camada de plataforma própria
que simula o hardware do GBA (VRAM, paleta, OAM, registradores, DMA, HBlank) e desenha com **gsKit**.

> **Este repositório não contém a ROM nem nenhum dado extraído dela** (gráficos, sons, mapas, textos). Você precisa da sua própria
> ROM *Fire Emblem: The Sacred Stones* (USA, SHA-1 `c25b145e37456171ada4b0d440bf88a19f4d509f`). Projeto de fã, sem relação com a Nintendo
> ou a Intelligent Systems. "Fire Emblem" é marca da Nintendo.

## Como funciona
- O ELF de lançamento **não leva assets**: a ferramenta `tools/romstrip.py` remove do ELF os trechos idênticos à ROM e, no boot, o jogo os restaura
  a partir da `fe8.gba` do usuário (validada por SHA-1). O som (m4a) também é lido da ROM no boot.
- Tela de carregamento própria (desenhada por código), com progresso da verificação da ROM, dados do jogo e áudio.
- Menu (Start+Select): vídeo (160p, 480i, 480p, 1080i, aspecto, filtro), controles, debug. Configuração salva em `save/`.
- Save do jogo em `save/fe8.sav`, ao lado do ELF (pendrive). Em mídia somente leitura o jogo roda sem salvar.
- Tela de crash própria (exceção + vigia de travamento) para depurar no PS2 real: tire uma foto e envie.

## Como construir
Requisitos: [ps2dev](https://github.com/ps2dev/ps2dev) (ps2sdk + gsKit, GCC do EE), python3, git, gcc/g++ e libpng-dev (ferramentas do host da decomp) e a sua ROM.
```sh
port/setup_decomp.sh ~/fe8u            # baixa a decomp no commit fixado (port/DECOMP_COMMIT) e aplica port/fe8-ps2.patch
cp sua_rom.gba ~/fe8u/baserom.gba
port/build_all.sh ~/fe8u ~/fe8build    # ajustes -> assets -> objetos C -> port -> dados -> banim -> link  => fe8_ps2.elf
```
Para jogar: coloque `fe8.gba` (a mesma ROM) na pasta do ELF (ou na raiz do pendrive: `mass:/fe8.gba`) e abra o `fe8_ps2.elf`.
Para um ELF pequeno, sem assets: `port/make_release.sh ~/fe8build ~/fe8u/baserom.gba fe8_release.elf`.
Recompilar só alguns arquivos da decomp: `port/rebuild_one.sh ~/fe8u ~/fe8build src/arquivo.c`.

## Organização
- `src/` renderizador GS (`gfx_cache`, `render`, `backend_gs`), `menu.c` (menu e tela de carregamento) e `main_ps2.c`.
- `src/platform/` IOP/USB, ROM, log, controle, save, áudio (saída audsrv + PSG), crash, e a memória/BIOS/DMA/LCD do GBA simulados.
- `port/` scripts de build; `fe8-ps2.patch` (ajustes mínimos na decomp), `apply_port_patches.py` (liga o código do GBA ao hardware simulado e corrige
  diferenças GBA/EE: acessos fora do vetor, `char` com sinal etc.), `src/m4a_engine.c` (motor de som m4a em C) e `src/gba_arm_port.c` (substitui o assembly ARM).
- `tools/romstrip.py` remove da ELF os trechos iguais à ROM.
- `tests/` testes no host (BIOS/DMA/LCD e comparação do caminho GS com um renderizador de referência).

## Notas de port (GBA -> EE)
- `u32`/`s32` = `unsigned int`/`int`; `char` é **unsigned** (`-funsigned-char`), como no agbcc; `-DNONMATCHING=1` usa as versões em C das funções em `asm()` ARM.
- Leituras/escritas em endereços que no GBA são inofensivos (BIOS, espelhos da EWRAM, bus aberto) dão TLB miss no EE: o tratador de exceção devolve 0 / descarta a escrita.
- O PCSX2 não gera esses TLB miss; teste no PS2 real quando possível.

## Estado
Jogável no PS2 real até onde foi testado (vídeo, menus, batalhas, eventos, save). Áudio ainda em validação no hardware.

## Créditos
decompilação **FireEmblemUniverse/fireemblem8u**, **ps2dev/ps2sdk/gsKit** e módulos IOP do ps2sdk (embutidos no ELF a partir do seu ps2sdk no build).

## Licença
MIT (veja `LICENSE`) para o código deste repositório. A decomp e a ROM têm seus próprios termos.
