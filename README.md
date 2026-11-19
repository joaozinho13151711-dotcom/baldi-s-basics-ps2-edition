# Baldi's Basics PS2 - Deluxe Edition (Pixel Art + 3 Personagens + 4 Itens)

Versao completa e aprimorada para PlayStation 2 (`ps2sdk` / `gsKit` / `audsrv`) com sprites pixel-art embutidos no codigo:
- **Sprites Pixel-Art 16x24 e 12x12 (`src/sprites.h`):**
  - **Baldi:** Rosto careca com fio de cabelo, olhos, boca, camisa verde, calca azul e regua amarela (aparece tambem na tela titulo, no Pad YCTP e no Game Over)
  - **Playtime:** Cabelo escuro, vestido vermelho e desafio de 5 pulos de corda
  - **Diretor da Escola (Principal):** Sueter cinza e gravata; se pegar voce correndo no corredor, aplica detencao
  - **Itens com icones pixel-art no mapa e no HUD:** Caderno, BSODA, Tesoura sem ponta e Barra energetica Zesty
- **Paredes texturizadas 16x16 (`src/textures.h`):** Tijolos brancos com rodape azul, salas amarelas com lousa verde rabiscada de giz e portas azuis com visor de vidro
- **Audio seguro para PS2 real (`src/audio.c`):** Buffer estatico alinhado em 64 bytes para rodar sem vazamento de memoria no hardware real

## Como gerar BALDI.ELF e BALDI_PS2.ISO
1. **GitHub Actions:** Suba os arquivos de dentro de `BaldiPS2_Deluxe_Edition/` para um repositorio no GitHub e execute **Actions -> Build PS2 ELF and ISO -> Run workflow**, depois baixe **BALDI-PS2-DELUXE**.
2. **Docker / Linux / WSL2:**
```bash
docker run --rm -v "$PWD":/src -w /src ps2dev/ps2dev make
```
