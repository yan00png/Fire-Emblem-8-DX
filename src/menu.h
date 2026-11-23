#ifndef MENU_H
#define MENU_H
/* Menu do port (Start+Select): Resume / Video / Controls / Debug GS. O jogo fica parado enquanto o menu esta aberto. */
void MenuRun(void);
void MenuVideoLoad(void);   /* aplica video e controles salvos (save/*.cfg); chamar depois do Ps2Save_Init e do BeInit */
#endif
