#include <fileio.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <fileXio_rpc.h>
#include "ps2_common.h"
#include "ps2_log.h"
#include "ps2_platform.h"

// Módulos IOP embutidos (bin2c a partir de $(PS2SDK)/iop/irx/*.irx; ver Makefile)
#define IRX(name) extern unsigned char name[]; extern unsigned int size_##name
IRX(iomanX_irx); IRX(fileXio_irx); IRX(usbd_irx); IRX(bdm_irx); IRX(bdmfs_fatfs_irx); IRX(usbmass_bd_irx);

static int g_iop_reset = 0;
int Ps2Platform_IopWasReset(void) { return g_iop_reset; }

int Ps2Platform_LoadIrx(const char *name, void *buf, unsigned int size) {
  int ret = -99; int id = SifExecModuleBuffer(buf, size, 0, NULL, &ret);
  Ps2Log("irx %-12s id=%d ret=%d", name, id, ret);
  return id;
}

// Reinicia o IOP: descarta os drivers que o aplicativo que abriu o ELF (uLaunchELF, OPL, FMCB...) deixou carregados,
// para que o jogo use SÓ os módulos embutidos.
static void reset_iop(void) {
  SifInitRpc(0);
  while (!SifIopReset("", 0)) {}
  while (!SifIopSync()) {}
  SifInitRpc(0);
}

void Ps2Platform_Init(const char *a0) {
  // Não reiniciar quando o ELF vem de CD/DVD/ISO (o OPL e o próprio disco dependem dos drivers atuais do IOP)
  // nem de host: (PCSX2/ps2link). Nos demais casos (pendrive, memory card, HDD) o IOP é reiniciado.
  int from_cd   = strncmp(a0, "cdrom", 5) == 0;
  int from_host = strncmp(a0, "host", 4) == 0;
#ifdef FORCE_IOP_RESET
  g_iop_reset = 1;
#else
  g_iop_reset = (!from_cd && !from_host);
#endif
  if (g_iop_reset) reset_iop();
  SifInitRpc(0);
  sbv_patch_enable_lmb();
  Ps2Log("lancado de: %s", a0[0] ? a0 : "(sem argv)");
  Ps2Log("IOP reiniciado: %s", g_iop_reset ? "sim" : "nao");

  if (from_cd) {   // modo disco: a ROM vem do cdrom0: via fio da BIOS/OPL; nada de drivers USB (derrubariam o CD virtual)
    fioInit();
    Ps2Log("modo disco: sem drivers USB");
    return;
  }
  // Armazenamento: iomanX + fileXio + USB (usbd) + BDM + FAT32/exFAT (bdmfs_fatfs) + usbmass_bd
  Ps2Platform_LoadIrx("iomanX", iomanX_irx, size_iomanX_irx);
  Ps2Platform_LoadIrx("fileXio", fileXio_irx, size_fileXio_irx);
  fileXioInit();
  Ps2Platform_LoadIrx("usbd", usbd_irx, size_usbd_irx);
  Ps2Platform_LoadIrx("bdm", bdm_irx, size_bdm_irx);
  Ps2Platform_LoadIrx("bdmfs_fatfs", bdmfs_fatfs_irx, size_bdmfs_fatfs_irx);
  Ps2Platform_LoadIrx("usbmass_bd", usbmass_bd_irx, size_usbmass_bd_irx);
  // Controle (libpad) e áudio (libsd/audsrv) entram aqui quando o jogo precisar.
}
