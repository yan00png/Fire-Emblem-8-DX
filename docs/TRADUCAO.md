# Idioma / tradução pt-BR

O jogo tem a opção **LANGUAGE** (ENGLISH / PORTUGUES) no menu do port (Start+Select → Video).

* Os textos em português ficam **fora** do repositório e do ELF: `<pasta da ROM>/save/fe8_ptbr.txt`, lido no boot.
  (O texto traduzido deriva do texto da Nintendo, por isso não é versionado aqui.)
* Mensagem sem tradução no arquivo cai no inglês original — dá para traduzir aos poucos.
* A fonte original já tem á é í ó ú â ê ô à ç; o port gera ã õ Ã Õ no boot.

## Formato
Uma mensagem por linha: `ID=texto` (ID decimal do `gMsgTable`; `#` = comentário).
`{XX}` = byte em hexadecimal (códigos de controle: `{01}` quebra de linha, `{1F}` espera, `{80}{xx}` rosto/comandos...).
Mantenha os mesmos códigos `{..}` do original (exceto `{01}`/`{1F}` de formatação, que podem mudar de lugar).
Limite: 0x554 bytes por mensagem.

## Obter os textos originais para traduzir
1. Crie `save/dump_msgs.flag` com 1 byte (ex.: `printf 1 > save/dump_msgs.flag`).
2. Inicie o jogo; após ~3 s é gravado `save/msgs_en.txt` (mesmo formato, texto em inglês) e o flag é esvaziado.
3. Copie as linhas que quiser para `fe8_ptbr.txt` e traduza.
